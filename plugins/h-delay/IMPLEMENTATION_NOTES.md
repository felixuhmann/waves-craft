# Implementation notes — Craft Hybrid Delay (spec r3)

Clean-room implementation (builder side) of [`spec/h-delay/r3/SPEC.md`](../../spec/h-delay/r3/SPEC.md)
(SHA-256 `4c366d87…61f2ba`, approved 2026-10-29; supersedes r2 and r1), updated after the independent
verifications of the r1 and r2 builds ([`docs/verification/h-delay-r1.md`](../../docs/verification/h-delay-r1.md),
[`docs/verification/h-delay-r2.md`](../../docs/verification/h-delay-r2.md)). Only the spec, `docs/process.md`, this repository, public
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
  Saturation.h           §4.1 envelope-controlled loop saturation
  Modulation.h           triangle LFO, modulation smoother (one-pole with tau = D, 18 Hz low-pass)
  LoFi.h                 fixed 8th-order low-pass defined in Hz (§7)
  Analog.h               the four Analog colour modes (§9.2) and their noise generator (§9.3)
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
loop:           w_c = HiPass(LoPass(LoFi(S_c(v_c + noise_c))))   S = envelope saturation (§4.1); line_c.push(w_c)
output:         y_c = G * (cos(m pi/2) * x_c + sin(m pi/2) * (+-1) * Analog_c(wet_c))   (§4.5 output stage)
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
Filters: the host value is `r`; `f = 19 · 1058^r` over the whole range, HiPass Off only at r = 0, LoPass Off
only at r = 1 (r2). Typed values / the Link ring never land on Off by accident (clamped to r ≥ 1e-4 / ≤ 1 − 1e-4). Display formats as §2 ("375 ms", "60", "0.0 dB", "0.10 Hz", "1.15k Hz", "0 %", "437Hz",
"1.24kHz", "Off"). An extra host Bypass parameter crossfades (10 ms) to the dry input.

**§3 Delay time.** `t = beats · 60 / tempo`; 1/4 = one beat, T × 2/3, D × 1.5, 1 bar = 4 beats, 2 bars = 8.
If `t > 3500 ms`, the longest note *in the list* whose duration fits is used (`effectiveNote`). Samples:
`round(t_ms · fs / 1000)` (review decision 11.2, no truncation). Host sync reads the host tempo from the play
head every block (120 BPM until a host reports one). The wet path is full-band at every rate (review decision
11.5; tested at 88.2/96 kHz up to 0.45·fs).

**§4 Core.** Exactly the equations above; one delay line per channel holds `w`, read at D (wet) and D + 1
(feedback) before the new sample is written. Integer delays are plain buffer reads, so echoes land on exact
samples with nothing in between (review decision 11.1). Feedback above 100 % self-oscillates, bounded by S.

**§4.1 Saturation.** `S(v) = v / (1 + 0.72·e + 0.28·|v|)` with a peak envelope `e` of the input (instant
attack). Deviation from the recommended form (allowed: "any implementation that meets the tables"): the envelope
holds its peak for 1.1 ms before releasing, and the release τ is 8 ms instead of 9.7 ms. Reason: with the plain
9.7 ms release the 1 ms row of §4.4 is 0.32–0.37 dB off (tolerance 0.3 dB); the hold supplies the short-gap
compression while the shorter release keeps the 5 ms row and the tone tables in tolerance. Isolated impulses are
still saturated exactly like `v/(1+|v|)`, so the §4/§5 examples are unchanged. Each line (and each Ping Pong
line) has its own envelope. Checked in `tools/fit/sat_check_r2.py` and the tests (§4.2 ±0.15 dB, §4.3 fundamental
±0.2 dB / H3 ±2.5 dB (±4 at 0.95) / H2 ≤ −150 dBc, §4.4 ±0.3 dB at 44.1 and 48 kHz).
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
reference are not reproduced (spec: not required). r2: the 48 kHz table is the target at every rate, including
44.1 kHz (−27.07 dB at 8 kHz there; the reference's −30.9 dB is noted in the spec but not required).

**§8 Modulation.** One triangle LFO shared by both channels (assumption 12.2), starting at its centre.
Nominal offset `D · Depth/100 · tri / 2`, then a smoother whose law the spec leaves open (12.1):
1. a one-pole low-pass with time constant equal to the current (glided) delay time, τ = 1.0 · D;
2. an 18 Hz, Q 1.0 second-order low-pass.
The r1 slope limiter attenuated large slow excursions (verification: D 50 ms / Depth 50 % gave ±8.7 ms instead of
±12.4 ms). The r2 table cannot be met by any slope limit (a 25 ms/s slope must pass at D = 50 ms, while a 20 ms/s
slope is attenuated to 0.73 at D = 200 ms), but it follows a time constant proportional to D: long delays saturate
at `A/(π·τ·f) = Depth/100 · D/2 / (π·D·f)` ≈ 12–13 ms. All 12 rows within ±15 % at 44.1/48/96 kHz (worst ≈ 13 %:
50 Hz and 20 Hz at D = 10 ms; 100 ms row −9 %); fit: `tools/fit/mod_fit_r2.py`. Fractional reads use a
16-tap Kaiser-windowed sinc (β = 7, 512 tabulated phases with linear interpolation between phases).

**§9 Delay-time changes.** The read delay follows the new integer target with a one-pole, τ = 0.2 s, snapping
exactly when within 1e-6 samples → pitch glide; click-free (tested). The optional output dip is not
implemented.

**§9.2 Analog.** Off = §4 exactly. Modes 1–4 are fixed chains applied **once to the wet signal read from the
delay line** (r3 §9.2 confirms this placement; noise stays in the loop): gain → [low shelf] → [high shelf] →
[peak] → [band dead-zone] → memoryless terms ([crossover `u − xo·tanh(u/xt)`], [asymmetry `c2·u²/√(u²+d²)`],
cubic `k3·u³`, `c3·u|u|`; in mode 1 they act only on a band below 178 Hz and the difference is added back) →
1 Hz DC blocker. Constants fitted offline (`tools/fit/analog_fit_r3.py`, after the §4.1 saturation) to the
response row, the stationary 100 Hz table (a = 0.03/0.1/0.3/0.95) **and the r3 §9.2.1 click vectors**.

r3 changes: the r2 chains combined instantaneous expansion with a slow (20 ms) envelope compressor. Steady tones
were right, but a click passed the expansion before the compressor reacted (mode 1 echo 1 +3.6 dB, mode 4 +1.8 dB
in the verification). The slow compressor is no longer used (the stage remains in the code, set to 0); all level
shaping is memoryless or linear, so clicks and tones see the same curve. Mode 1 needed its extra harmonics to be
frequency-dependent: on bass tones they are strong (H3 −35.5 dBc at a = 0.1) while clicks must stay within −0.2…−0.3
dB of Off. The terms are therefore applied to a low-passed band (transformer-like low-frequency distortion). The DC
blocker moved from 5 Hz to 1 Hz: at 5 Hz it cost 0.46 dB at 15 Hz / 0.26 dB at 20 Hz in every mode, which looked like
a HiPass corner 2–3 % too high near r = 0. The HiPass itself was already exact (tested against the formula at
r = 0.01 / 0.05).

Per mode: 1: low shelf −3.1 dB at 97 Hz, low-band even and odd harmonics; 2: −0.57 dB, a −0.38 dB shelf from
570 Hz, weak level-dependent H2, slight compression; 3: a ~93 Hz band through an asymmetric soft dead zone
("starved"), mid/high tilt; 4: −0.41 dB, mild compression, plus a small low-level expansion below ≈ −66 dBFS that
supplies the low-level 3rd harmonic (−36 dBc at a = 0.03). Worst fitted error / tolerance over all three tables:
0.26 / 0.77 / 0.68 / 0.73. No time modulation (timing tested). Mode changes crossfade over 20 ms.

**§9.3 Noise.** Injected into the loop signal before the saturation on every pass (accumulates with feedback:
rise = loop power sum `1/(1−g²)`, and per-pass growth at 100 %). Three random noise bands: 10 Hz (Q 0.7, 60 % of
the power), 46.6 Hz and 91.8 Hz (Q 15, 25 % / 15 %); left/right independent. The injection level of each mode is
calibrated so that the output (after the mode's wet chain; mode 4's low-level expansion included) is −102.7 / −85.5 / −106.0 / −108.7 dBFS at Feedback 0.
Off is digital silence. Tested at Feedback 0/25/50/75/100 % (±3 dB, ±6 dB at 100 %).

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
  rounding to the nearest sample at 44.1/48/88.2/96/192 kHz (incl. triplets: 16000 not 15999), mappings incl. the
  r2 filter end points (19.7 Hz at r = 0.005, 19.4 kHz at r = 0.995, Off only at 0 / 1), display formats, tap.
- Core: the §4 example (exact recursion and the printed values; silence between echoes), echo timing at
  44.1/48/96 kHz in ms and note modes, Host/BPM tempo, Dual, Feedback 200 % bounded, Ping Pong examples (both
  start sides), mono sum input, Mix 0 bit-exact (float), Output gain, equal-power Mix, zero latency, Phase,
  Mono→Stereo, glide τ and click-free, Mix/Output ramp ≤ 5 ms, stability with everything on.
- Saturation (r2): §4.2 compression vs frequency at −30/−12 dBFS (±0.15 dB), §4.3 100 Hz fundamental (±0.2 dB),
  H3 (±2.5 / ±4 dB) and H2 (≤ −150 dBc), §4.4 per-pass ratios at 1/2/3/5/10/20 ms, 44.1 and 48 kHz (±0.3 dB).
- Filters: HiPass/LoPass magnitude vs an independent bilinear-prototype formula at 44.1/48/96 kHz, 15 Hz–20 kHz,
  ±0.1 dB; the 14.2 kHz → 11.44 kHz corner example; LoFi table (48 kHz table at all rates) and through the engine.
- Modulation: LFO shape and rate (±0.2 %), the 12-row r2 table at 44.1/48/96 kHz (±15 %) incl. the verifier's
  D 50 ms/Depth 50 % and D 1024 ms/Depth 10 % cases, echo position under modulation, Depth 0 exact, interpolator.
- Clicks (r3 §9.2.1): echo-peak ratios vs Off for all six rows (±0.5 dB) and never louder than Off.
- HiPass corner at r = 0.01 / 0.05 (44.1/48/96 kHz, 15–100 Hz, ±0.1 dB) and flat low end of the mode chains.
- Analog: response row (±1 dB; mode 3 ±3 dB at 50–500 Hz), 100 Hz table at four levels (fundamental ±0.5 dB, mode 3
  at 0.03 ±1.5 dB; H2/H3 ±4 dB; "—" as ≤ −150 dBc for Off and ≤ −100 dBc for mode 4, whose loop noise sets the
  floor), noise vs feedback, noise spectrum, no timing modulation; wet path full-band at 88.2/96 kHz (to 0.45·fs).

40 test cases, ~480 000 assertions. Offline fit / check scripts (Python, SciPy; only the spec's numbers):
`tools/fit/` — `sat_check_r2.py` (§4.1–4.4), `mod_fit_r2.py` (§8), `analog_fit_r3.py` (§9.2 + §9.2.1 clicks; uses `analog_fit_r2.py`), `eval_r2.py`,
`lofi_fit*.py` (§7); the r1 scripts are kept for provenance.

## Questions / assumptions (for the review gate)

r2/r3-specific:

1. **Analog placement**: chains once on the wet output, noise inside the loop. r3 §9.2 confirms this (it resolved
   the r2 conflict).
2. **Saturation form**: recommended form plus a 1.1 ms hold and 8 ms release (see §4.1) because the plain form
   misses the 1 ms row of §4.4 by up to 0.37 dB.
3. **Off-row H3 at a = 0.95**: −28.1 dBc vs −24.7 (inside the ±4 dB tolerance; the recommended form gives −28.4).
4. **Mode 4 "—" H2**: tested as ≤ −100 dBc rather than −110 because the mode's loop noise (−109 dBFS) is part of the
   output; the chain itself is odd-symmetric (no H2).
5. **Modulation law** (12.1): τ = D one-pole + 18 Hz low-pass; r3 §8 now describes it the same way. Both channels
   share the LFO phase (12.2).
6. **Noise spectrum** (12.4): random bands, not deterministic tones, so they accumulate like noise in the loop.
7. **Mode 4 low-level expansion**: the only memoryless way found to give −36 dBc H3 at a = 0.03 while staying at
   Off level for clicks and loud tones is a slight gain rise (up to ×2.8) for signals below ≈ −66 dBFS. It is
   inaudible in practice, but it is a modelling choice; a level-dependent (envelope) form would also fit if
   preferred.
8. **Mode 1 band split** (178 Hz low-pass band for the distortion terms) is inferred from the click-vs-tone
   targets, not measured directly.

Carried over from r1:

9. **Delay ms resolution**: quantised to 1 ms (display shows integers). Delay R defaults to the Delay defaults.
10. **"1 bar" = 4 beats** (4/4) regardless of the host time signature.
11. **Glide dip** (§9.1) not implemented (optional); glide is click-free.
12. **Toggles inside the loop** (Ping Pong, LoFi, Analog mode, filters on/off, Phase) crossfade over 20 ms; the
    Ping Pong start side switches instantly (only affects new input). Depth changes are smoothed (20 ms).
13. **Tap**: interval of the last two taps between 40 ms and 4 s; ms mode sets Delay (L), Host mode the nearest
    note value (log-distance) at the host tempo, BPM mode sets BPM = 60000 / interval (one tap = one beat).
14. **Tail length** reported as 30 s (feedback can sustain forever).
15. **Mono (1 → 1) variant** not shipped; Mono→Stereo is the same plug-in with a mono input bus.

## Public references and provenance

- Spec §14: Laakso et al., "Splitting the unit delay" (IEEE SPM 1996) — windowed-sinc fractional delay;
  Dattorro, "Effect Design Part 2" (JAES 1997) — modulated delay lines; Zölzer (ed.), *DAFX* 2nd ed. — delay
  effects, shelving/peaking filters, nonlinear processing; R. Bristow-Johnson, "Cookbook formulae for audio EQ
  biquad filter coefficients" — all biquads.
- Kaiser window / zeroth-order Bessel series and the bilinear transform with pre-warping, inverse-Chebyshev
  prototypes (standard DSP textbook material; the offline fits used SciPy's public `scipy.signal.cheby2` and
  `scipy.optimize.least_squares`).
- Giannoulis, Massberg & Reiss, "Digital Dynamic Range Compressor Design", JAES 60(6), 2012 — peak envelope
  detector (attack / hold / release) of the §4.1 saturation.
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

## Robustness (after the r3 verification)

The r3 verification saw REAPER exit mid-batch twice (exit code 1) while rendering this build. The same exits were
later traced, in another verifier's runs, to an access violation in a third-party DLL loaded by the host process.
Investigation here:
- **Plug-in level:** `tools/hoststress` (shared stress host, see CONTRIBUTING.md) against `Craft Hybrid Delay.vst3`.
  Runs covered 44.1/48/96 kHz, stereo / mono-in, float / double, max block 32–4096 with random blocks, all 21
  host parameters automated from the audio thread (extremes included), and message-thread parameter gestures.
  They also exercised state save/restore during processing, re-prepare / release, and editor open/close.
  - Release, 20 min: 792.7 M samples, 1072 prepares, 5331 state restores, 1.8 M parameter moves, 356 editor opens.
    0 failures (no NaN/Inf, nothing above +40 dBFS), no crash.
  - Debug CRT (checked iterators) + MSVC AddressSanitizer, 15 min: 118.4 M samples, 325 prepares, 1644 state
    restores, 101 editor opens. 0 failures, no ASan report.
- **DSP level:** new `tests/StressTests.cpp` runs 20 s per configuration at 44.1/48/96/192 kHz, stereo and
  mono→stereo, random blocks of 1–4096 samples. It randomises every setting (extreme feedback, host tempo
  1–1000 BPM, all note values and Analog modes) and re-prepares mid-stream. It passes, and the whole suite also
  passes under GCC `-fsanitize=address,undefined` with `_GLIBCXX_ASSERTIONS`.
- **Code review:** parameters are read through APVTS atomics, and state restore goes through the APVTS. The engine
  is per-sample, with no block-size-dependent buffers, so blocks larger than the prepared size are safe. Delay
  reads are clamped to the buffer (max delay × 1.5 modulation + interpolator taps). Denormals are handled by
  ScopedNoDenormals plus an envelope flush, and all members are initialised.
- **One real-time fix (not a crash):** `readSettings()` looked parameters up by string on every audio block, and
  those lookups allocate. The parameter pointers are now resolved once in the constructor.

No crash cause was found in the plug-in.

## Status

- Spec r3 implemented: r2 (verified: every r2 table passed) plus the §9.2.1 click vectors and the low-end fix.
- Windows x64 Release build (VST3, CLAP, Standalone; MSVC 19.44, Visual Studio 17 2022 generator) without
  warnings; installed to `%LOCALAPPDATA%\Programs\Common\VST3`, `C:\Program Files\Common Files\VST3` and
  `%LOCALAPPDATA%\Programs\Common\CLAP`.
- Tests: 41 h-delay test cases (all repository tests: 65/65) pass with MSVC and with GCC 13 (Linux, DSP only).
- Linux plug-in build not run locally (JUCE system packages missing); CI builds it.
- `ci/package.py` finds the product without changes (`craft-hybrid-delay-<version>-<platform>.zip`).
- r3 not yet verified. Not done: host-level validation (pluginval / DAW session).
