# Implementation notes — Craft Hybrid Delay (spec r1)

Clean-room implementation (builder side) of [`spec/h-delay/r1/SPEC.md`](../../spec/h-delay/r1/SPEC.md)
(SHA-256 `2ceea267…6922ba`, approved 2026-10-29). Only the spec, `docs/process.md`, this repository, public
documentation and this implementation were used. No original product, analyst workspace, vendor asset, preset,
manual or installation was accessed, and no vendor plug-in was loaded.

Product: **Craft Hybrid Delay** (`CHdl`, CLAP id `com.craftaudio.h-delay`), manufacturer **Craft Audio** (`Crft`),
version 0.1.0. Formats: VST3, CLAP, Standalone. Platforms: Windows x64 (MSVC), Linux x64 (GCC).
One binary serves the spec's **Stereo** variant (2 → 2) and **Mono→Stereo** (1 → 2: the host picks the bus
layout). The separate Mono (1 → 1) variant is not shipped (spec: "should"; unmeasured).

Licence: AGPL-3.0 like the rest of the repository (JUCE under AGPL); see [THIRD_PARTY.md](../../THIRD_PARTY.md).

## Architecture

Paths relative to `plugins/h-delay/`.

```
CMakeLists.txt         targets (added automatically by the root CMakeLists.txt)
source/dsp/            host-independent, header-only DSP core (no JUCE), double precision
  Parameters.h           §2 mappings (Delay taper, note table, BPM, Feedback, Rate, filters, ...), display formats
  DelayTime.h            §3 note durations, 3500 ms fallback, rounding to samples, tap quantisation
  Biquad.h               RBJ cookbook sections (spec §6 corner w0 = 2 atan(pi f / fs)), pre-warped analog sections
  DelayLine.h            circular buffer, 16-tap Kaiser-windowed-sinc fractional read (exact for integer delays)
  Modulation.h           triangle LFO, modulation smoother (slope compression + leak + 20 Hz low-pass)
  LoFi.h                 fixed 8th-order low-pass defined in Hz (§7)
  Analog.h               the four Analog colour modes (§9) and their wet-path noise
  Smoothing.h            one-pole, linear and fixed-time ramps
  HybridDelayEngine.h    complete per-sample processor (§3-§9)
source/plugin/         JUCE AudioProcessor + parameter layout
source/ui/             editor, look-and-feel, widgets (knobs, tap pad, display, meter, link ring) - all vector-drawn
tests/                 Catch2 tests of the DSP core (registered with CTest, prefix "h-delay/")
tools/Snapshot.cpp     optional dev tool rendering the editor to PNG (-DCRAFT_BUILD_SNAPSHOT=ON)
```

### Signal flow per sample (`HybridDelayEngine::processSample`)

```
per channel c:  Dg_c  = glide(D_c)                    one-pole, tau 0.2 s (§9), D_c integer samples (§3)
                m_c   = smooth(Dg_c * depth * tri / 2) modulation offset (§8)
                d_c   = Dg_c + m_c
                wet_c = line_c.read(d_c)              first echo at exactly D
                fb_c  = line_c.read(d_c + 1)          loop length D + 1 (§4)
normal:         v_c = x_c + g * fb_c
ping pong:      v_L = injL * xm + g^2 * fb_R,  v_R = injR * xm + fb_L,  xm = (L + R)/sqrt 2   (§5)
loop:           w_c = HiPass(LoPass(LoFi(sat(Analog(v_c)))))   sat(v) = v / (1 + |v|);  line_c.push(w_c)
output:         y_c = G * (cos(m pi/2) * x_c + sin(m pi/2) * (+-1) * (wet_c + noise_c))   (§4 output stage)
```

## Spec mapping

**§1 Variants / latency.** `isBusesLayoutSupported`: stereo out with stereo or mono in. In Mono→Stereo the input
feeds both lines (Ping Pong uses it as `xm` directly) and Phase R follows Phase L. Latency 0. The dry path is
bit-transparent: at Mix 0 the output stage multiplies by exactly 1 and 0 (tested with float buffers, including
an active Analog mode whose noise is part of the wet signal).

**§2 Parameters.** Host-normalised values are the spec's `r`. Delay (ms) uses `1 + 1000 r` below r = 0.5 and a
monotone cubic (Fritsch–Carlson PCHIP, first slope 1000 ms so the taper is C1) through the spec's ten upper
points, quantised to 1 ms. Separate parameters for Delay ms / Delay note / Delay R ms / Delay R note (choice of
the 20 note names). BPM `30 + 270 r`, Feedback `200 r`, Mix, Output `−18 + 36 r` dB, Rate `0.1 · 60000^r`,
Depth, Analog (Off, 1…4, default 2), Sync (BPM / Host / ms, default Host), Ping Pong start (Left / Right).
Filters: the host value is `r`; `f = 19 · 1058^r`, HiPass Off below r = 0.05, LoPass Off above r = 0.95 (see
assumption 2). Display formats as §2 ("375 ms", "60", "0.0 dB", "0.10 Hz", "1.15k Hz", "0 %", "437Hz",
"1.24kHz", "Off"). An extra host Bypass parameter crossfades (10 ms) to the dry input.

**§3 Delay time.** `t = beats · 60 / tempo`; 1/4 = one beat, T × 2/3, D × 1.5, 1 bar = 4 beats, 2 bars = 8.
If `t > 3500 ms`, the longest note *in the list* whose duration fits is used (`effectiveNote`). Samples:
`round(t_ms · fs / 1000)` (review decision 11.2, no truncation). Host sync reads the host tempo from the play
head every block (120 BPM until a host reports one).

**§4 Core.** Exactly the equations above; one delay line per channel holds `w`, read at D (wet) and D + 1
(feedback) before the new sample is written. Integer delays are plain buffer reads, so echoes land on exact
samples with nothing in between (review decision 11.1). Feedback above 100 % self-oscillates, bounded by sat.
Mix/Output: fixed 4 ms linear ramps (complete within 5 ms; exact target afterwards); Feedback: one-pole 10 ms.
At Mix 100 the cos/sin pair is exactly (0, 1).

**§5 Ping Pong.** As specified, with the asymmetric hop gains (L→R 1, R→L g², review decision 11.3) for both start
directions; in Dual mode each line keeps its own D. Toggling Ping Pong crossfades the loop inputs over 20 ms.

**§6 Filters.** RBJ high-pass (Q 0.80) / low-pass (Q 0.95) with `w0 = 2·atan(π f / fs)`, in the loop after
the saturation (and after LoFi), every pass. Frequencies glide in the log domain (τ 30 ms, coefficients
refreshed every 16 samples). Switching a filter on starts it at its "edge" (10 Hz for HiPass, 0.45 fs for
LoPass) and glides in; switching off glides to the edge and then removes it, so Off is exactly transparent.

**§7 LoFi.** Four analog second-order low-pass sections (two with transmission zeros at 9.27 kHz and
10.88 kHz), fitted offline to the table starting from an 8th-order inverse-Chebyshev prototype, and discretised
by the bilinear transform with each section's pole and zero frequency pre-warped. Result at 44.1/48/96 kHz:
pass band ≤ 4 kHz within ±0.05 dB, 5/6/7/8 kHz within ±0.35 dB of −1/−4.4/−12/−27 dB, 9 kHz ≤ −51.8 dB,
≥ 10 kHz ≤ −57.9 dB. In the loop after the saturation; toggling crossfades over 20 ms. The weak images of the
reference are not reproduced (spec: not required).

**§8 Modulation.** One triangle LFO shared by both channels (assumption 12.2), starting at its centre.
Nominal offset `D · Depth/100 · tri / 2`, then a smoother whose law the spec leaves open (12.1):
1. slope compression of the per-sample offset change, `dy = S · tanh(dx / S)` with S = 0.021 samples/sample
   (21 ms of delay change per second), plus a 5 s leak towards the nominal offset (prevents drift);
2. a 20 Hz, Q 1.2 second-order low-pass.
Fitted to the 11 tabulated points; at 44.1/48/96 kHz all are within the ±15 % tolerance (worst ≈ 12–14 %,
at the 200 ms and 10 Hz points), and the
"amplitude saturates at 12–13 ms for long delays" behaviour follows from the slope limit. Fractional reads use a
16-tap Kaiser-windowed sinc (β = 7, 512 tabulated phases with linear interpolation between phases).

**§9 Delay-time changes.** The read delay follows the new integer target with a one-pole, τ = 0.2 s, snapping
exactly when within 1e-6 samples → pitch glide; click-free (tested). The optional output dip is not
implemented.

**§9 Analog.** Off = §4 exactly. Modes 1–4 are fixed chains applied to the loop signal before the saturation
(placement open, 12.3): gain → [low shelf] → [high shelf] → [peak] → [band dead-zone] → [asymmetry] →
expansion `u + c3·u|u|` → slow envelope compressor `u / (1 + kc·env)` (20 ms) → 5 Hz DC blocker. Constants were
fitted offline against the response table (−30 dBFS, re Off), the 100 Hz distortion table and the mode 3 low-level
note. Mode 1: low shelf (−3.0 dB at 86 Hz) and a level-saturating even-harmonic term; mode 2: −0.8 dB plus a −0.9 dB
high shelf and a constant −51 dBc H2; mode 3: a ~105 Hz band passed through an asymmetric soft dead zone
("starved": strongly level-dependent loss and H2 at low level) plus a mid/high tilt; mode 4: −0.27 dB. No time
modulation (timing tested). Noise (wet path, outside the loop, silent input): a 10 Hz noise band plus 46.6 Hz and
91.8 Hz tones, total RMS −103 / −85.5 / −106 / −109 dBFS for modes 1–4; Off is digital silence. Mode changes
crossfade over 20 ms.

**§13 GUI.** Original vector-drawn design in the Craft Audio palette (graphite panels, amber/teal accents,
two-ring mark; no images or bundled fonts), 1000 × 440, fixed size. Left: square TAP pad (beat dots), LoFi toggle,
product label. Centre-left: large Delay knob (scale ends "1"…"3500" in ms mode, "1/64T"…"2 bars" in note modes;
the knob switches between the ms and note parameters with the sync mode). Centre: Phase L ("Ø L") / Ping Pong / Dual /
Phase R ("Ø R") toggles; dark display with the delay time(s) (one value, or L and R in Dual; note name plus ms and the
tempo in BPM/Host modes, "*" marks a 3500 ms fallback), the Ping Pong start selector (L/R) and drag-to-edit
values (L, R, and the BPM in BPM mode; double-click resets); BPM / Host / ms buttons below. Centre-right: large
Feedback knob (0…200). Right: Dry/Wet, Output and Analog (detent labels Off…4) knobs and a vertical output meter
(0…−48 dBFS, L/R bars, peak line and a peak/clip LED: amber above −6 dBFS, red on clip, click to reset).
Bottom: Modulation (Depth, Rate) and Filters (HiPass, Link ring, LoPass; the LoPass arc grows from its "Off" end). All knobs: double-click / Alt-click
reset, click the value to type, drag to change.

## Tests (`tests/`, Catch2)

- Parameters: Delay taper points/monotonicity/inverse, note table, fallback cases (60/120/30 BPM, exactly 3500 ms),
  rounding to the nearest sample at 44.1/48/88.2/96/192 kHz (incl. triplets: 16000 not 15999), mappings, display
  formats, tap quantisation.
- Core: the §4 example (exact recursion and the printed values; silence between echoes), echo timing at
  44.1/48/96 kHz in ms and note modes, Host/BPM tempo, Dual, Feedback 200 % bounded, Ping Pong examples (both
  start sides, exact model and the printed values), mono sum input, Mix 0 bit-exact (float), Output gain,
  equal-power Mix, zero latency, Phase, Mono→Stereo, sine fundamental vs the Off row (±0.3 dB), glide τ and
  click-free, Mix/Output ramp ≤ 5 ms, stability with everything on.
- Filters: HiPass/LoPass magnitude vs an independent bilinear-prototype formula at 44.1/48/96 kHz, 15 Hz–20 kHz,
  ±0.1 dB; the 14.2 kHz → 11.44 kHz corner example; LoFi table at three rates and through the engine.
- Modulation: LFO shape and rate (±0.2 %), the 11 smoothing points at 44.1/48/96 kHz (±15 %), D = 50 ms / Depth 50
  example, echo position under modulation, Depth 0 exact, interpolator accuracy.
- Analog: response table (±1 dB), 100 Hz fundamental (±0.5 dB) and H2/H3 (±4 dB) for modes 1–4, mode 3 at a = 0.03,
  noise level (±3 dB) and spectrum, Off silence, no timing modulation.

34 test cases, ~480 000 assertions. The offline fit scripts that produced the LoFi / modulation / Analog
constants are in `tools/fit/` (Python, SciPy; they read only the spec's numbers).

## Questions / assumptions (for the review gate)

1. **Delay ms resolution**: quantised to 1 ms (display shows integers). Delay R defaults to the Delay defaults.
2. **Filter end points**: the spec gives `19 · 1058^r` with "Off (r = 0)" / "Off (r = 1)" but also the ranges
   ≈27 Hz…20.1 kHz (HiPass) and ≈19 Hz…14.2 kHz (LoPass), which correspond to r = 0.05 and r = 0.95. Implemented:
   HiPass Off for r < 0.05, LoPass Off for r > 0.95, formula elsewhere.
3. **"1 bar" = 4 beats** (4/4) regardless of the host time signature.
4. **Off-row harmonics**: the normative saturation `v/(1+|v|)` gives H3 of −36/−27/−20 dBc at a = 0.1/0.3/0.95,
   5–7 dB above the §9 Off row (−43/−34/−25) while its fundamental matches the row within 0.25 dB. The
   implementation keeps §4 exactly for Analog Off (the test checks the fundamental and bounds H3); for modes 1–4
   the fitted expansion/compressor pair brings H3 into the ±4 dB tolerance. Is the Off-row H3 a measurement
   artefact, or should Off also have the lower H3?
5. **Analog placement** (12.3): colouration/distortion inside the loop before the saturation (each repeat is
   coloured again); noise added to the wet output (does not build up with feedback; level independent of
   Feedback). The response/distortion tables were matched with Feedback 0.
6. **Mode 3 at 100 Hz**: the sweep table (−13.3 dB at −30 dBFS) and the tone note (−10 dB at a = 0.03) disagree by
   ~3 dB for nearly the same level; the fit splits the difference (sweep within ±1 dB was prioritised).
7. **Noise spectrum** (12.3): 60 % of the power in a 10 Hz band (Q 0.7), 25 % at 46.6 Hz, 15 % at 91.8 Hz;
   left/right noise independent.
8. **Modulation law** (12.1) as described above; both channels share the LFO phase (12.2). Depth changes are
   smoothed (20 ms); the LFO keeps running with Depth 0 (no exact-phase restart).
9. **Glide dip** (§9) not implemented (optional); glide is click-free.
10. **Toggles inside the loop** (Ping Pong, LoFi, Analog mode, filters on/off, Phase) crossfade over 20 ms; the
    Ping Pong start side switches instantly (only affects new input).
11. **Tap**: interval of the last two taps between 40 ms and 4 s; ms mode sets Delay (L), Host mode the nearest
    note value (log-distance) at the host tempo, BPM mode sets BPM = 60000 / interval (one tap = one beat).
12. **Tail length** reported as 30 s (feedback can sustain forever).
13. **Mono (1 → 1) variant** not shipped; Mono→Stereo is the same plug-in with a mono input bus.

## Public references and provenance

- Spec §14: Laakso et al., "Splitting the unit delay" (IEEE SPM 1996) — windowed-sinc fractional delay;
  Dattorro, "Effect Design Part 2" (JAES 1997) — modulated delay lines; Zölzer (ed.), *DAFX* 2nd ed. — delay
  effects, shelving/peaking filters, nonlinear processing; R. Bristow-Johnson, "Cookbook formulae for audio EQ
  biquad filter coefficients" — all biquads.
- Kaiser window / zeroth-order Bessel series and the bilinear transform with pre-warping, inverse-Chebyshev
  prototypes (standard DSP textbook material; the offline fits used SciPy's public `scipy.signal.cheby2` and
  `scipy.optimize.least_squares`).
- Fritsch & Carlson, "Monotone piecewise cubic interpolation", SIAM J. Numer. Anal. 17(2), 1980 (Delay taper).
- JUCE 8, clap-juce-extensions and Catch2 public documentation (as for the other Craft plug-ins).

## Building

From the repository root (builds every plug-in; `ctest` runs all tests, this plug-in's are prefixed `h-delay/`):

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Artefacts: `build/plugins/h-delay/CraftHybridDelay_artefacts/Release/{VST3,CLAP,Standalone}/`.
`-DCRAFT_BUILD_PLUGINS=OFF` builds only the DSP tests; `-DCRAFT_BUILD_SNAPSHOT=ON` adds `h_delay_snapshot`
(renders the editor to PNG). With the Visual Studio generator use `--config Release` / `ctest -C Release`.
Install: copy `Craft Hybrid Delay.vst3` to `%LOCALAPPDATA%\Programs\Common\VST3` (or
`C:\Program Files\Common Files\VST3`) and `Craft Hybrid Delay.clap` to `%LOCALAPPDATA%\Programs\Common\CLAP`;
on Linux to `~/.vst3` / `~/.clap`. `ci/package.py` picks the plug-in up automatically (`craft-hybrid-delay-*.zip`).

## Status

- Windows x64 Release build (VST3, CLAP, Standalone, MSVC 19.44, Visual Studio 17 2022 generator) verified
  locally without warnings; installed to `%LOCALAPPDATA%\Programs\Common\VST3`,
  `C:\Program Files\Common Files\VST3` and `%LOCALAPPDATA%\Programs\Common\CLAP`.
- Tests: 34 h-delay test cases (all repository tests: 58/58) pass with MSVC and with GCC 13 (Linux, DSP only).
- Linux plug-in build not run locally (JUCE system packages missing); the plug-in sources were syntax-checked
  with GCC against the JUCE headers; CI builds it.
- `ci/package.py` finds the product without changes (`craft-hybrid-delay-<version>-<platform>.zip`).
- Not yet done: host-level validation (pluginval / DAW session) and the independent verification against the
  reference (process step 5).
