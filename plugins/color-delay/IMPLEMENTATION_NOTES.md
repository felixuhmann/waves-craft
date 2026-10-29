# Implementation notes — Craft Colour Delay (spec r1)

Clean-room implementation (builder side) of [`spec/color-delay/r1/SPEC.md`](../../spec/color-delay/r1/SPEC.md)
(SHA-256 `a07b0579…02eb55`, approved 2026-10-29). Only the spec, `docs/process.md`, public literature and this
implementation were used. No original product, analyst workspace, vendor asset or vendor installation was accessed.

Product: **Craft Colour Delay** (`CCdl`, CLAP id `com.craftaudio.color-delay`); manufacturer **Craft Audio**
(`Crft`); version 0.1.0. Formats: VST3, CLAP, Standalone. Platforms: Windows x64 (MSVC), Linux x64 (GCC).
Bus layouts: stereo → stereo, and mono → stereo (spec §1 "Mono-to-stereo variant", see assumption 1).

## Architecture

All paths are relative to `plugins/color-delay/`.

```
CMakeLists.txt        targets of this plug-in (added automatically by the root CMakeLists.txt)
source/dsp/           host-independent, header-only DSP core (no JUCE)
  Parameters.h          §2 mappings r -> value + quantisation, note table / host-sync lengths, §6 Dry/Wet law, §7 level laws
  Smoothing.h           one-pole smoother (snaps exactly to its target), linear ramp
  FeedbackDelay.h       §3a integer-sample feedback delay, crossfaded delay-time changes
  Filters.h             first-order / biquad sections, §4 third-order HP/LP pair (+ analog prototype for tests)
  Compressor.h          §5 stereo-linked compressor model
  Reverb.h              §7.1 16-line FDN reverb
  Distortion.h          §7.2 voiced soft-clip distortion with gate and width
  Doubler.h             §7.3 synchronised dual-tap pitch shifter
  Phaser.h              §7.4 four-notch phaser
  ColourDelayEngine.h   §3 complete signal path (per-sample, double precision), Settings struct
source/plugin/        JUCE AudioProcessor, parameter layout, Delay Link / HP-LP Link logic
source/ui/            editor, look-and-feel, knobs / LED buttons / segmented switches / meters (all vector-drawn)
tests/                Catch2 unit tests of the DSP core (registered with CTest, prefix "color-delay/")
tools/Snapshot.cpp    optional dev tool rendering the editor to PNG (-DCRAFT_BUILD_SNAPSHOT=ON)
```

## How each spec section is implemented

**§1 Scope.** One product; `isBusesLayoutSupported` accepts stereo→stereo and mono→stereo. With a mono input the
processor copies the input to both channels before the engine (identical channel inputs). Latency 0 (no look-ahead;
the dry path is sample-aligned; the minimum delay is 1 sample).

**§2 Parameters.** Every host parameter's normalised value is the spec's `r`. `NormalisableRange` lambdas apply the
mapping **and** the quantisation in `convertFrom0to1` (Input/Output 0.1 dB, Delay 4 ms, percentages 0.1 %, HP/LP
1 Hz, Size 0.1 %, Detune 1 cent, Rate 0.01 Hz), so hosts and the editor only see step values. Toggles are
`AudioParameterBool`; Mode (`ms`/`Host`), note values (19) and Monitor (`L`/`L+R`/`R`) are `AudioParameterChoice`,
whose index is `round(r·(n−1))` — this gives exactly the spec's thresholds (Monitor: r < 0.25 → L, r > 0.75 → R).
Defaults as in the table (Mode Host, 500 ms, 1/4, 29.9 %, Link on, 100 % wet, filters off, effects off, Size 54.9 %,
Levels 50 %, Send 82.2 %, Width 0 %, Detune +8, Rate 0.34 Hz, Monitor L+R). A host Bypass parameter is added.
*Delay Link*: the engine uses the left time/feedback for both channels while linked; additionally the processor mirrors
the left parameters into the right ones (message thread, `AsyncUpdater`), so the linked right controls display the left
values and unlinking keeps the right channel at the values it had while linked. Moving a right control while linked
moves the left one. *HP/LP Link*: on engaging, the product `f_HP·f_LP` is captured (and stored with the state); moving
one filter sets the other to `product / f`, and both stop at `sqrt(product)` where they meet.

**§3 Signal path** (`ColourDelayEngine::processSample`): input gain → per-channel feedback delay
`w[n] = xi[n−D] + g·w[n−D]` (no cross-feed, `g` up to exactly 1.0) → HP → LP → stereo-linked compressor → wet gain →
the four colour effects in parallel on that post-wet-gain signal → `y = dry·xi + wet + Σ effects` → output gain →
Monitor. `D = max(1, round(ms·fs/1000))` in ms mode and `max(1, floor(note_seconds·fs))` (≤ 2 s) in Host mode, using
the host tempo from the play head (120 BPM if the host provides none). Internal arithmetic is double precision.

**§4 Filters.** The analog pair is factored as `LP = 1/(s+1) · (1−a s²)/(s²+(1+a)s+1)` and
`HP = s/(s+1) · (s²−a)/(s²+(1+a)s+1)` with `a = 0.132316`, and each section is discretised with the bilinear
transform prewarped at `fc` at the actual rate (`k = tan(π fc/fs)`). Off positions (HP 16 Hz, LP 21357 Hz) bypass the
filter; switching in/out crossfades over ~20 ms; frequency changes are smoothed in the log domain (coefficients
recomputed per sample while moving). `fc` is limited to 0.49 fs for low sample rates.

**§5 Compressor.** Exactly the spec model: detector `max(|L|,|R|)`, instant attack / 1.06 ms one-pole release, soft-knee
static curve (T −23.23 dB, R 6.196, W 23.22 dB), GR smoothing 8.17 ms attack / 439 ms release in dB, output gain
`(1−p)·10^(−GR/20) + p` with p = 0.0292, the same gain on both channels. One addition (assumption 4): a high-level taper
that reduces the GR slope by 0.25 above about +4 dBFS detector level (soft knee 4 dB), so that the measured static gains
at +6 / +12 dBFS (via Input gain) are met; it is exactly zero below +2 dBFS.

**§6 Dry/Wet law.** The 21-point wet table; linear in dB between grid points, linear in amplitude between 0 and 5 %;
dry(m) = wet(100 − m). Gains smoothed (~20 ms one-pole, snapping exactly to the target).

**§7 Colour effects** (each fed the post-wet-gain stereo signal; on/off crossfaded over ~20 ms, state cleared when
fully off; Level law `(L/100)^3` for the reverb and `^3.5` for the others):

- *Reverb* (`Reverb.h`): pre-delay `13 + 0.11·Size` ms (fractional, smoothed) → four series Schroeder all-pass
  diffusers per channel (g = 0.62) → 16-line FDN (14–42 ms lines, normalised Hadamard matrix followed by a signed cyclic
  shift, which is orthogonal but not involutory). Each line has an absorption filter (gain × first-order low shelf at
  300 Hz × high shelves at 1.8 and 5.5 kHz) fitted per line by least squares + Gauss-Newton so that its attenuation
  equals `−60 dB·length/(RT60(f)·fs)` at the seven octave centres of the RT60 table (interpolated linearly over Size).
  Inputs/outputs use mutually orthogonal (scrambled Hadamard) vectors, so the two outputs are decorrelated; a small
  ipsilateral direct share gives the onset at the pre-delay. The output gain follows an energy model of the network,
  corrected by a per-sample-rate calibration (the network's impulse response is measured once per rate at the five Size
  rows during `prepare` and cached; ~0.1 s) so the IR energy follows −0.7 dB (Size 1 %) … +0.8 dB (100 %).
- *Distortion* (`Distortion.h`): mono sum `L+R` (assumption 6) → drive `P(Send)` → gate on the post-drive peak envelope
  (threshold −30 dBFS, 2 dB hysteresis, 1 ms / 20 ms) → band-pass voicing (2× HP 96.8 Hz, 2× LP 4.3 kHz, peak 704 Hz
  +1.5 dB, LP 8.7 kHz; fitted to the small-signal table) → symmetric saturator (linear up to |x| = 0.4, rational soft limit
  above; odd harmonics only) → presence peak 3 kHz +3 dB (raises H3 re the fundamental) → make-up `Q(Send)` → 6.5 ms
  delay (total ≈ 6.8 ms with the voicing group delay) → width network `L = cos t·d + sin t·u`, `R = cos t·d − sin t·u`,
  `t = Width·π/4`, `u` = all-pass-decorrelated `d` (0 % identical, 100 % uncorrelated with equal level).
  `P` (dB) over Send 0/10/25/50/100 % = −3/1/8.5/22/56, `Q` = −10.3 (≤ 50 %) → −25.3 (100 %); fitted offline to the
  §7.2 drive, harmonic and gate data.
- *Doubler* (`Doubler.h`): dual-tap delay-line pitch shifter, delays within 3–10 ms, sin² crossfades, 4-point Lagrange
  interpolation, +9.2 dB output gain. Each tap's delay moves at `1 − 2^(cents/1200)` samples per sample, so every grain
  carries exactly the shifted pitch. When a grain ends, the tap restarts at the delay (within a 4.3 ms search span) whose
  lag to the other tap maximises the normalised input autocorrelation (synchronised splicing), which removes the comb
  cancellation / splice phase jumps of a free-running shifter. Same shift on both channels.
- *Phaser* (`Phaser.h`): four second-order all-pass sections in series (Q 2, centres `f0·{1, 1.45, 2.1, 3.05}`) with
  0.25 feedback, output `0.5·(x + allpass)` → four notches; sine LFO at exactly Phaser Rate sweeps `f0` exponentially
  2.3–4.3 kHz, so the notches cover ≈ 2–15 kHz.

**§8 Parameter changes.** Delay-time changes crossfade linearly (30 ms) between the old and new read positions (a
further change waits for the running crossfade). All other controls use ~20 ms one-pole smoothing that snaps exactly to
the target (so settled settings are bit-exact to the static equations).

**§12 GUI.** Original vector-drawn editor (940×610), dark graphite with an amber delay section and one colour per
effect; own two-ring mark and "Colour Delay" badge with four colour dots. Upper delay panel: Feedback L (small knob),
Link (LED button), badge, Mode switch (ms/Host), Feedback R; large Delay L/R knobs (show ms in ms mode, the note value in
Host mode) with note drop-downs beneath (enabled in Host mode); between them Dry/Wet, HP, Link, LP. Lower row: Reverb,
Distortion, Doubler, Phaser modules, each with an on/off LED button, a Level knob and Size / Send + Width / Detune /
Rate. Left edge: input meter above Input; right edge: output meter above Output and the L/ST/R Monitor switch. Small
knobs show their value while hovered/dragged; double-click or Alt-click resets; linked right controls are dimmed and
mirror the left values; tooltips explain the toggles. The header shows the host tempo in Host mode.

## Tests (`tests/`, Catch2, 32 test cases / 2305 assertions, ~16 s with `ctest -j`)

- §2 mappings, quantisation round trips, note table and host-sync lengths (1/4 → 24000, 1/8D → 18000, 1/16T → 3999,
  Whole → 96000 at 120 BPM / 48 kHz; 2 s limit), ms lengths, Delay Link through the engine.
- §6 Dry/Wet law at all 21 grid points, interpolation; §7 level laws (−17.5 / −37 / −21 dB within ±1.5 dB), also
  through the engine (effects post wet gain: Dry/Wet 25 % lowers them by the wet law).
- §11 vectors: impulse 0.001 FS, 100 ms, 50 % → 0.001, 0.0005, 0.00025 at 100/200/300 ms, nothing else, R silent;
  Dry/Wet 25 % → 0.74989 dry / 0.24547 wet; HP/LP 1 kHz values (±0.02 dB); Input +6 dB → +5.99 dB ± 0.03;
  1 kHz static gains at −30/−12/0 dBFS; gain recovery after −6 → −40 dBFS.
- §10 [exact]: the engine against an independent straight-line model (single third-order polynomial bilinear filter in
  long double, plain delay recursion) on noise, random settings, 44.1/48/96 kHz: ≤ −90 dB re peak. Filters: −3.01 dB at
  fc, LP + HP all-pass and agreement with the analog prototype at 44.1/48/96 kHz.
- §5/§10 compressor: static gains at 1 kHz (−33 … +12 dBFS) within ±0.3 dB; frequency dependence 120 Hz – 4 kHz within
  ±0.3–0.5 dB, 8/16 kHz and 60 Hz within ±1 dB; envelope vs an independent implementation of the §5 model on a level
  staircase ≤ 0.5 dB RMS; stereo linking; 0.5 FS impulse within 1 dB of ×0.908.
- §7/§10 reverb at 44.1/48/96 kHz: RT60 per octave band (Schroeder T20 on one-octave band-filtered IRs) within ±15 %
  (worst case about ±10 % at 48 kHz), pre-delay ±3 ms, energy ±1.5 dB per output (mean ±0.75 dB), |ρ| < 0.2.
- Distortion: small-signal response at Send 50 % within ±2 dB (100 Hz – 4 kHz), Send 25 % at 1 kHz, Send 100 % +14 …
  +24 dB, H3 profile within ±4 dB at the four measured points, negligible below −28 dBFS, H2 < −95 dB, gate thresholds
  (Send 10 %: silent at −32, open at −28 dBFS; Send 0 %: silent at −28, open at −24), width 0 / 100 %, latency 6.8 ± 1 ms.
- Doubler: pitch within ±0.5 cent (median of 20 ms frequency estimates; 440 Hz / 1 kHz / 2.5 kHz × ±1…±100 cents),
  level 9.2 ± 1 dB (tone) / ± 1.5 dB (band-limited noise), latency 3–10 ms.
- Phaser: ≥ 4 notches at four LFO positions, notch range within ≈ 1–18 kHz (measured 1.9–14.6 kHz); LFO rate within ±5 %
  (0.34, 2, 7 Hz) from the output envelope.
- Monitor, effects silent at Dry/Wet 0 %, effects off contribute nothing (bit-identical), click-free delay change,
  Feedback 100 % lossless, host bypass bit-exact once faded.

## Questions / assumptions (for the review gate)

1. **Mono-to-stereo** is offered as an additional bus layout of the same product (mono input copied to both channels),
   not as a separate product; hosts pick it on mono tracks.
2. **Triplet rounding**: the measured 1/16T = 3999 samples (not 4000) is reproduced by computing triplets with the
   7-digit factor 0.6666666 and flooring; straight and dotted values are exact. Consequence: every triplet at a tempo
   where the exact length is an integer comes out one sample shorter.
3. **Phaser Rate resolution**: 0.01 Hz steps; below 0.01 Hz (r ≈ 0 … 0.08) the unrounded value is kept so the bottom of
   the range (≈ 0.005 Hz) stays reachable.
4. **Compressor at high level**: the §5 model gives −21.18 / −24.19 dB at +6 / +12 dBFS vs the measured −20.83 /
   −23.07 dB; a GR taper above +4 dBFS (0.25 slope reduction, 4 dB knee) brings both within ±0.3 dB and leaves
   everything ≤ +2 dBFS untouched. Other known deviations of the (normative) model vs. the observations, not corrected:
   gain recovery at 500 ms −4.45 dB vs −5.35 dB observed (100 ms and 1.2 s points match); at −6 dBFS the model gives
   −13.1 dB at 30 Hz vs −10.3 dB measured (spec §5 says the model reproduces this via detector ripple — it does not with
   the stated time constants; 60 Hz −13.2 vs −12.8); single-sample transients (spec §13.1) and in-loop interaction with
   feedback (§13.2) are not modelled.
5. **Effect input coupling / levels**: effect levels are relative to the post-wet-gain signal; the phaser's output level
   is `0.5·(x + allpass)` at Level 100 % (not characterised).
6. **Distortion mono sum**: "Width 0 % = identical L/R distortion output" implies a mono path; the input is `L + R`, so a
   one-sided input (as used in the spec measurements) is processed at its own level; centred stereo material drives the
   distortion 6 dB harder. Width decorrelation via an all-pass chain is my choice.
7. **Distortion level vs Send**: the fitted drive rises by 34 dB from Send 50 % to 100 % while the make-up falls by 15 dB,
   so small signals gain +19 dB (spec: +14 … +24) and loud signals get quieter/denser at high Send. Results: H3 at the
   four measured points −8.4 (spec −9), −7.0 (−5), −20.1 (−20), −7.7 (−11) dB; small-signal response at Send 50 % within
   0.4 dB of the table from 50 Hz to 8 kHz (16 kHz: −67 vs −59.6 dB, outside the toleranced band).
8. **Gate**: detects the broadband post-drive level (before the voicing), threshold −30 dBFS with 2 dB hysteresis,
   1 ms attack / 20 ms release (spec gives only thresholds).
9. **Doubler latency** is interpreted as the 3–10 ms range of the sweeping taps (6.5 ms at Detune 0). Pitch accuracy is
   per grain; the level is calibrated for tones (sin² crossfade of in-phase taps); noise comes out ≈ 1 dB lower.
10. **Reverb**: IR energy is defined per output for a one-sided impulse (mean of L and R hits the target, each within
    ±1.5 dB); pre-delay is the onset at −40 dB re peak; Size changes glide (pre-delay smoothed, absorption redesigned
    every 64 samples).
11. **Delay Link editing**: while linked, moving a right control moves the left one (both always equal); the reference
    behaviour for this case is not specified.
12. **HP/LP Link** product is captured when the link is switched on and saved with the plug-in state; with a filter at its
    off position the other filter is computed from 16 Hz / 21357 Hz respectively.
13. **Bypass**: host bypass parameter, 20 ms crossfade to the unprocessed input (delay keeps running).
14. **Meters**: input meter shows the level after the Input gain; peak meters −48 … +6 dBFS, ~36 dB/s fall, 1.5 s hold.
15. **Tail length** reported as 10 s (Feedback 100 % repeats indefinitely).
16. Fixed editor size 940×610; not resizable.

## Public references and provenance

- J.-M. Jot, A. Chaigne, "Digital delay networks for designing artificial reverberators", AES 90th Conv., 1991; J. Stautner,
  M. Puckette, "Designing multi-channel reverberators", CMJ 6(1), 1982 (FDN with orthogonal feedback matrix and
  per-line absorption); M. R. Schroeder, "Natural sounding artificial reverberation", JAES 10(3), 1962 (all-pass
  diffusers); J. Dattorro, "Effect Design, Part 1/2", JAES 45(9)/(10), 1997 (diffusion, delay modulation, pitch shifting).
- U. Zölzer (ed.), *DAFX*, 2nd ed., 2011 — delay-line pitch shifting with crossfades and synchronised splicing,
  phasers with all-pass chains, soft-clipping distortion, shelving filters.
- D. Giannoulis, M. Massberg, J. D. Reiss, JAES 60(6), 2012 — soft-knee curve and smoothing (only to read §5).
- R. Bristow-Johnson, "Audio EQ Cookbook" — biquad low/high-pass, peaking and all-pass designs, bilinear prewarping.
- M. R. Schroeder, "New method of measuring reverberation time", JASA 37, 1965 — backward integration (tests).
- JUCE 8 public API documentation, clap-juce-extensions README, Catch2 v3 documentation.

## Building

From the repository root (all plug-ins):

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure -R color-delay
```

Artefacts: `build/plugins/color-delay/CraftColourDelay_artefacts/Release/{VST3,CLAP,Standalone}`. `ci/package.py` finds
them generically (product zip `craft-colour-delay-<version>-<platform>.zip`). Editor snapshot:
`-DCRAFT_BUILD_SNAPSHOT=ON`, then run `color_delay_snapshot <dir>`.

## Status

Windows x64 Release build (MSVC 2022, VST3 + CLAP + Standalone, no warnings in this plug-in) and all 32 unit tests
pass (also on Linux / GCC 13 for the DSP core); `ci/package.py` produces `craft-colour-delay-<version>-windows-x64.zip`. The Linux plug-in targets were not built locally; they use the same portable CMake as the other
Craft plug-ins and are covered by CI.
