# Implementation notes — Craft doubler (spec r1)

Clean-room implementation (builder side) of [`spec/doubler/r1/SPEC.md`](../../spec/doubler/r1/SPEC.md)
(SHA-256 `8ed06647…f2caa`, approved 2026-10-29). Only the spec, `docs/process.md`, this repository, public
documentation/literature and this implementation were used. No original product, analyst workspace, vendor
asset, manual or installation was accessed, and no vendor plug-in was loaded.

Products: **Craft Doubler 4** (`CDb4`, CLAP id `com.craftaudio.doubler4`, 4 voices) and **Craft Doubler 2**
(`CDb2`, `com.craftaudio.doubler2`, 2 voices); manufacturer **Craft Audio** (`Crft`); version 0.1.0.
Formats: VST3, CLAP, Standalone. Platforms: Windows x64 (MSVC), Linux x64 (GCC). Bus layout: stereo out, stereo
or mono in (mono is fed to both input channels, spec §1). Licence: AGPL-3.0 like the rest of the repository
(JUCE), see [THIRD_PARTY.md](../../THIRD_PARTY.md).

## Architecture

All paths are relative to `plugins/doubler/`.

```
CMakeLists.txt        targets (doubler_dsp, CraftDoubler4, CraftDoubler2, doubler_snapshot, tests)
source/dsp/           host-independent, header-only DSP core (no JUCE)
  Parameters.h          §3 mappings r -> value, quantisation, inverses; Settings; defaults of both variants
  Smoothing.h           one-pole smoother (snaps to target), linear ramp
  Lfo.h                 §7 triangle LFO
  Shelf.h               §8 RBJ shelves (with the spec's corner-frequency law), §9 feedback high-pass
  Voice.h               §5/§6/§7/§9 one voice: delay line, splicing pitch shifter, LFO, feedback loop
  DoublerEngine.h       §4 complete stereo engine: direct, routing, voices, pan, EQ, master, bypass
source/plugin/        JUCE AudioProcessor shared by both products (voice count chosen per target)
source/ui/            editor, look-and-feel, controls, displays, output meters (all vector-drawn)
tests/                Catch2 tests of the DSP core (spec §11), registered with CTest as doubler/*
tools/Snapshot.cpp    optional dev tool rendering the editors to PNG (-DCRAFT_BUILD_SNAPSHOT=ON)
```

`DoublerProcessor` takes the number of voices at construction; `createPluginFilter()` picks it from the compile
definition `DOUBLER_VOICES` set per target (`doubler_add_variant`). The 2-voice product is the same engine with
voices 3–4 absent (spec §1); a test checks it is bit-identical to the 4-voice engine with voices 3–4 off.

The engine processes in double precision per sample. All constants that the spec gives as "design-rate times"
are converted to `round(time × fs)` samples (decision D2) in `RangeConfig::make`.

## How each spec section is implemented

**§3 Parameters.** One mapping struct per law (`param::Linear<lo,hi,steps>`, `Frequency` = `F(r)`, `Rate` =
`0.1·4000^r`, `Routing` with thresholds 0.3/0.8). The JUCE parameters use a `NormalisableRange` built from these
mappings, so the host's normalised value is the spec's `r` and every reported value is quantised. Defaults are
given as physical values (`defaultSettings(numVoices)`, both variants). Switches are `AudioParameterBool`
(threshold 0.5 — Range: off = 80 Hz). Modulation Reset is a bool parameter used as a trigger; the engine reacts to
its rising edge. A host bypass parameter is added (5 ms crossfade to dry).

**§4 Signal path.** `DoublerEngine::processSample`: direct = smoothed 2×2 rotation `g_d [[cos θ, −sin θ],
[sin θ, cos θ]]` (zero when off); voices read their routed source (A for voices 1/3, B for 2/4), voice output ×
`10^(G/20)` × constant-power pan `(cos φ, sin φ)`, `φ = Pan + 45°`; the voice sum goes through low shelf then high
shelf; master gain on everything. Latency reported to the host: 0.

**§5 Voice delay / inherent latency.** `Lr` = round(320/44100·fs) (Range 80) or round(1088/44100·fs) (Range 20).
`Dn = max(Delay, Lr)` (Align No) or `Delay + Lr` (Align Yes, and the direct is delayed by exactly `Lr` samples).
Reads are linearly interpolated. With Detune = 0, Depth = 0 and Octave off the voice is an exact delay of `Dn`
(the shifter is bypassed; the tests check this bit-exactly).

**§6 Pitch shifter** (`Voice.h`). A read tap moves through the voice delay line; its delay changes by `1 − ρ`
per sample, `ρ = 2^(c/1200)·(0.5 if Octave)`. Window `[Dn − Wh, Dn + Wh]`:
- ρ > 1: when the tap has come within half a fade length of `Dn − Wh`, a new tap is started at `+J`
  (longer delay); ρ < 1: mirrored at `Dn + Wh`, new tap at `−J`.
- `J ∈ [Jmin, Jmax]` maximises the normalised cross-correlation between the delay-line signal around the old
  read position and around the candidate position (window `Jmin` long: 5.6 ms / 23.9 ms). Search: coarse on a
  decimated grid (step ≈ fs/24 kHz), then full resolution around the best lag, then parabolic interpolation to a
  fractional jump. Silence → mid-range jump.
- The old and new taps are mixed with complementary linear gains; the fade progresses with the tap's travel
  (1.7 ms of delay travel, design rate), centred on the window edge, so the louder tap switches exactly at the
  edge and the fading tap overshoots by ≤ 0.85 ms — inside the `Lr − Wh` margin (0.89 ms / 1.07 ms), so every
  read stays causal even at the smallest `Dn`.
- Consequences (measured by the tests, 48 kHz): mean delay `Dn ∓ 2.0…2.2 ms` for ±50 cents (spec 2.2 ± 0.7),
  louder-tap segments inside `[Dn − Wh, Dn + Wh]`, jumps inside `[Jmin, Jmax]`.

Leaving the exact-delay path is seamless (the reset shifter is one tap at `Dn`); returning to it ramps from the
shifter output to the exact tap in 5 ms, then the shifter is reset.

**§7 Modulation.** `TriangleLfo` (0 → +1 at ¼ → −1 at ¾ → 0), multiplied by the signed depth and added to the
detune in cents; frequency = Rate Hz (decision D3). All LFOs start at phase 0 when the engine is reset (plug-in
start) and restart together on a Modulation Reset rising edge.

**§8 EQ.** `designShelf`: `f_a` from the spec's law (÷1.5414 / ×1.5694, `10^(±G/80)`), `f0 = (fs/π)·atan(π f_a/fs)`,
RBJ cookbook low/high shelf with Q = 0.5, TDF-II biquads on the voice sum only. Gain and log-frequency are
smoothed (τ 2 ms; coefficients recomputed every 16 samples while moving); on/off crossfades in 5 ms.

**§9 Feedback.** `x[n] = s[n] + fb · HP(y[n−1])` is written into the voice delay line, so the loop period is
`Dn + 1` sample and the pitch shifter sits inside the loop; feedback is taken before gain/pan/EQ/master. HP =
`(1 − z⁻¹)/((1 + k) + (k − 1)z⁻¹)`, `k = tan(π F/fs)` (F clamped to 0.49 fs). 100 % is allowed (test: bounded).

**§10 Decisions.** D1: Align with zero detune uses `Delay + Lr` like every other path. D2: all design-rate
constants scale with fs. D3: LFO at Rate Hz. D4: Rate shown in Hz.

**§11.5 Dynamics.** Gains, pans and on/off are smoothed on the final linear coefficients (one-pole, τ 1 ms:
residual < 0.7 % after 5 ms; smoothers snap to their target so settled paths are exact). Detune/depth/octave are
smoothed in cents (τ 1 ms). A Delay or Range change cross-fades linearly (5 ms) from the old to the new nominal
delay (tap offsets are kept relative to `Dn`); a change arriving during a cross-fade starts when it ends (≤ 10 ms).
Align changes cross-fade the direct delay the same way.

**§12 GUI** (`source/ui`, original artwork drawn in code, Craft family palette). Header with an original
two-wave logo mark. Top row: *Stage* (semicircle; dot per voice + "D" for the direct; angle = pan, radius = gain;
drag; double-click toggles), *Delay / Detune* plane (x 0–100 ms, y ±100 cents; horizontal bar length ∝ |depth|;
a small tick shows the live modulated pitch; the region below `Lr` where delays are clamped is hatched when Align
is off; "-8va" badge for octave), *Voice EQ* curve (log 16 Hz–21 kHz, −30…+15 dB view; L/H markers; drag;
double-click toggles the shelf). Middle strip: Direct on/gain/pan, Align No/Yes, FB HP, Range 80/20, Modulation
Reset (momentary), EQ L and EQ H (on, gain knob, freq box). Voice rows (colour-coded): number/on, gain knob, pan
knob, delay, feedback, octave (down arrow), detune, depth, rate. Right: master fader, stereo meters (+6…−48 dBFS),
peak counters and clip indicator (click resets). Bottom: input routing selectors (L / L+R / L−R and R / R+L / R−L).

## Tests (`tests/`, Catch2)

Run with `ctest --test-dir build -R doubler` (or `doubler_tests` directly). 23 test cases:

| File | Spec | What |
|---|---|---|
| `ParameterTests.cpp` | §3, §1 | mappings, resolutions, quoted r → value pairs, defaults of both variants, 2-voice ≡ 4-voice with 3–4 off (bit-exact), zero latency |
| `PathTests.cpp` | §11.1 | direct identity / ±22.5° / −45° rotation / −12 dB gains / Align delay (Range 80/20); voice delay 20 ms, pans; delay clamping and Align table (0/5/10/50 ms, both ranges); routing A/B; feedback echo times/peaks (1, 0.5994, 0.3591, 0.2153); 2nd/1st echo = 0.6·H_HP(f) ≤ 0.05 dB; 100 % feedback bounded — at 44.1/48/96 kHz, tolerance 3e-7 (≈ −135 dB re peak) |
| `EqTests.cpp` | §11.2 | the full magnitude table (48 kHz, ±0.15 / ±0.5 dB), EQ measured through the engine at 44.1/48/96 kHz, EQ not on the direct, click-free EQ gain change |
| `ShifterTests.cpp` | §11.3 | pitch by phase slope (±1…±100 cents ≤ 0.5 cent, octave and octave+50 ≤ 6 cents; 44.1/48 kHz), Hilbert envelope 1 % percentile (1 kHz/100 Hz ≥ 0.97, 5 kHz +50 ≥ 0.90), white-noise level (−1.8 ± 0.6 dB) and 1/3-octave flatness (≤ 3 dB), delay window / jump sizes / mean delay (Range 80 and 20, octave), louder-tap trajectory = cross-correlation peak of the output, click-free delay change within 10 ms |
| `ModulationTests.cpp` | §11.4, §11.5 | triangle amplitude/period/start direction (depth ±), detune + depth range, output pitch follows the LFO, Modulation Reset (rising edge, all voices), gain/pan/on-off/direct/master/detune settle within 5 ms without steps, return to the exact path |

Measured values of the statistical tests (48 kHz, Range 80, delay 20 ms; references from the spec in brackets):
pitch error < 0.001 cent (≤ 0.5), octave −1200.0 / −1150.0 cents [−1204 / −1154]; envelope 1 % percentile
0.994–0.999 at 100 Hz and 1 kHz [≥ 0.993], 0.946 at 5 kHz +50 cents [0.946]; noise level −2.04…−2.14 dB [−1.8];
worst 1/3-octave deviation 1.7–2.6 dB [≤ 2.5]; mean delay ∓2.0…2.2 ms [∓2.2]; louder-tap window
[−6.38, +6.38] ms, jumps within [Jmin, Jmax].

## Questions / assumptions (for the review gate)

1. **Splice cross-fade length / "continuous two-tap overlap" (§6, §13.1).** The spec states both linear-interpolated
   reads and a continuous two-tap overlap that loses 1.76 dB on uncorrelated material. Linear interpolation alone
   already loses exactly 1.76 dB on white noise (mean of `1 − μ(1−μ)·2(1 − cos ω)` over μ and ω is 2/3), and a
   continuous linear overlap would add another 1.76 dB (total ≈ −3.5 dB, outside the −1.8 ± 0.6 dB acceptance
   limit). A continuous overlap would also need taps up to `J/2` beyond the window, which the reference's
   `Lr − Wh` margin (0.89 ms) cannot accommodate at the smallest delays. I therefore kept the linear reads (the
   5 kHz envelope of 0.946 then matches the reference value exactly) and used a short travel-based cross-fade
   (1.7 ms of delay travel) centred on the window edge. Result −2.1 dB, inside the limit. Please confirm.
2. **1/3-octave deviation definition (§11.3).** Measured as each band's gain minus the broadband (RMS) gain,
   Welch spectra, IEC bands 100 Hz–16 kHz. Relative to the *mean of the band gains in dB* instead, the worst
   case is ≈ 3.0 dB at −100 cents (the 16 kHz band, linear-interpolation droop) — please confirm which definition
   the verification uses.
3. **Splice search details** (implementation-defined): correlation window = `Jmin`, centred on the old read
   position and clipped to available samples; coarse/fine search with parabolic refinement; silence → mid jump.
   If a jump would leave the delay buffer the range is clamped.
4. **Static ↔ shifting transitions:** the exact-delay path applies only when Detune = 0, Depth = 0 and Octave off
   (targets). Leaving it is seamless; returning to it is a 5 ms crossfade.
5. **Delay-change handling:** 5 ms linear crossfade between the two nominal delays with the same shifter state;
   changes during a crossfade are queued (≤ 10 ms total), e.g. while dragging.
6. **Smoothing constants** (not measured): τ 1 ms for gains, pans, detune, depth and feedback amount; EQ
   τ 2 ms (§13.3, ≤ 10 ms); feedback HP frequency changes are applied per block without smoothing.
7. **LFO start phase** (§13.4): all voices at phase 0 at plug-in start (`prepareToPlay` resets).
8. **Mono:** mono→stereo is supported by the bus layout (mono input copied to both channels); no mono→mono variant.
9. **Feedback HP above Nyquist:** `F` is clamped to 0.49 fs (21357 Hz exceeds fs/2 at 44.1/48 kHz).
10. **Modulation Reset** is exposed as an automatable bool parameter ("Reset" while held); the GUI button is
    momentary.
11. **Value boxes** (§12 "double-click to type, Alt/double-click resets"): vertical drag changes (Shift = fine),
    double-click opens the text editor, Alt-click or Alt-double-click resets to the default. Knobs: drag,
    Alt-click resets.
12. **Tail length** reported as 2 s (feedback echoes); not specified.
13. **EQ display** clamps markers to −30…+15 dB; gains below −30 dB (down to −88) are set with the knob or box.
14. **Fixed editor size** (1106 × 610 for 4 voices, 1106 × 530 for 2 voices); not resizable.
15. **Safety:** a voice that produces a non-finite value is reset (cannot happen with valid input; protects the
    feedback loop).

## Public references and provenance

- W. Verhelst, M. Roelands, "An overlap-add technique based on waveform similarity (WSOLA) for high quality
  time-scale modification of speech", ICASSP 1993 — similarity-based splice point selection.
- U. Zölzer (ed.), *DAFX — Digital Audio Effects*, delay-line pitch shifting and time-domain splicing
  (SOLA/WSOLA), linear-interpolated fractional delays, bilinear first-order filters.
- J. Dattorro, "Effect Design, Part 2: Delay-Line Modulation and Chorus", JAES 45(10), 1997 — interpolated delay
  lines and modulation.
- R. Bristow-Johnson, "Cookbook formulae for audio EQ biquad filter coefficients" — shelving filters.
- JUCE 8 public API documentation, clap-juce-extensions README, Catch2 v3 documentation.

## Building

From the repository root (builds every plugin; this plugin's tests are prefixed `doubler/`):

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure -R doubler
```

Artefacts: `build/plugins/doubler/CraftDoubler4_artefacts/Release/{VST3,CLAP,Standalone}/` and
`CraftDoubler2_artefacts/...`. `-DCRAFT_BUILD_PLUGINS=OFF` builds only the DSP tests (fast, no JUCE);
`-DCRAFT_BUILD_SNAPSHOT=ON` adds `doubler_snapshot` (editor PNGs). `ci/package.py` picks both products up
automatically (`craft-doubler-4-*.zip`, `craft-doubler-2-*.zip`).

## Status

- Windows x64 Release build (MSVC 19.44, VST3 + CLAP + Standalone for both products) verified locally; installed
  to `%LOCALAPPDATA%\Programs\Common\VST3`, `C:\Program Files\Common Files\VST3` and
  `%LOCALAPPDATA%\Programs\Common\CLAP`. `ci/package.py` finds and zips both products.
- 23/23 test cases pass with MSVC (Windows) and GCC 13 (Linux, `-DCRAFT_BUILD_PLUGINS=OFF`).
- Linux plug-in targets not built locally (JUCE system packages not installed here); CI covers them.
- Not yet independently verified (no `docs/verification/doubler-r1.md`).
