# Implementation notes: Craft Opto Leveler (spec r1)

Clean-room implementation built only from the approved behavioural specification
[`spec/opto-leveler/r1/SPEC.md`](../../spec/opto-leveler/r1/SPEC.md) and its normative tables
[`TABLES.md`](../../spec/opto-leveler/r1/TABLES.md) / [`TABLES_extra.md`](../../spec/opto-leveler/r1/TABLES_extra.md),
public DSP literature and the existing Craft plug-ins in this repository. No vendor code, binaries, presets, manuals or
artwork were used.

| | |
|---|---|
| Product | **Craft Opto Leveler** (Craft Audio, `Crft`), plug-in code `COpt`, CLAP id `com.craftaudio.opto-leveler` |
| Formats | VST3, CLAP, Standalone; Windows x64 and Linux x64 |
| Layouts | stereo (one linked detector, D6) and mono (D3), input = output |
| Latency | 0 samples |

## Architecture

```
source/dsp/            host-independent DSP core (header only, target opto_leveler_dsp)
  SpecTables.h         GENERATED at configure time from the spec Markdown (cmake/EmbedSpecTables.cmake)
  Laws.h               T0 tapers, T1 static curves, T2 gain law, Auto Makeup offsets, text helpers
  Weighting.h          Hi Freq sidechain weighting (6 peaking sections, gains interpolated in Hi Freq)
  WeightingTable.h     constants written by tools/fit_weighting.py
  Detector.h           quadrature envelope, energy-separation amplitude, compressed-domain averaging, ripple
  OptoCell.h           the spec 4.3 three-component cell model, implemented literally
  OutputStage.h        tube colour (+ emphasis), T8 response, analog noise / hum, parameter ramps
  LevelerEngine.h      the complete chain (spec 3)
source/plugin/         JUCE AudioProcessor, parameter layout, Auto Makeup coupling
source/ui/             editor, look and feel, knobs / segmented switches, needle meter (all drawn in code)
tests/                 Catch2 tests against the spec tables (SpecMeasure.h reproduces the measurement procedures)
tools/                 Snapshot.cpp (editor PNGs), Calibrate.cpp (prints table errors for a tuning) and the
                       Python design scripts fit_colour.py, fit_response.py, fit_weighting.py, design_hilbert.py
```

Per sample (`LevelerEngine::process`):

1. **Sidechain** (per channel, fed from the input): Hi Freq weighting → envelope → averaging; the louder channel's
   level is used for both channels (D6, max of the channel levels).
2. **Static curve**: level (dBFS, sine-peak calibrated) → GR via T1C/T1L, interpolated linearly in level and in Peak
   Reduction (rows are pre-blended whenever Peak Reduction or the Compress/Limit blend ramps).
3. **Cell**: the 4.3 model with `a_t = 1 − 10^(−GR/20)`; `g = 1 − Σ c_i`.
4. **Waveform following** (spec 6, LF distortion): `g ← g · exp(−D · r)` where `r` is the low-passed instantaneous
   waveform level relative to the envelope and `D = 0.0255 · slope(T1) · (1/g − 1)^0.737`.
5. **Audio**: `x · g · makeup(T2)` → colour → response (T8) → Mix with the dry input → Trim → + analog noise →
   output protection (smooth saturation above +30 dBFS only, see assumption 12).

### Level detector (spec 6, T1, T3, T6)

The detector has to satisfy three tables at once: T1 (exact static calibration to sine peak), T3 (attacks at 1–10 ms
that match the 4.3 model driven by an *ideal level step* — the literal model already uses 0.95 of the 1 dB budget) and
T6 (white noise of equal RMS reduced *less* than a sine, increasingly so at high levels). A plain rectifier with a
smoothing filter cannot do this: short smoothing makes the fast-attack/slow-release cell follow noise peaks (noise
then gets 2–3 dB *more* reduction), long smoothing delays burst onsets. The detector is therefore built from:

* **Quadrature envelope** `e = √(I² + Q²)` from a 90° all-pass pair (2 × 6 second-order sections in z⁻², designed by
  `tools/design_hilbert.py`, ±0.77° over 10 Hz…20 kHz at 44.1–192 kHz). For a steady sine `e` is exactly the peak
  level (no ripple), for Gaussian noise its mean is σ√(π/2) — the same sine/noise ratio as a rectified-average
  detector calibrated to sine peak, as the spec describes.
* **Averaging in a compressed domain**: one-pole (1.5 ms) average of `F(e)` with `F(e) = k ln(1 + e/k)` (k = 0.2,
  linear at low level, logarithmic above) that additionally saturates above e = 0.2 (`satNepers` = 0.5), converted
  back with `F⁻¹`. A steady sine is unaffected (constant `e`); for noise, averaging a concave function of the fluctuating
  envelope reads lower, more so at high levels — this reproduces the level dependence of T6.
* **Transient acquisition**: when the envelope jumps 15 dB above the average (onset), or stays 8 dB below it for
  0.3 ms (end of a burst), the averager switches for 2 ms to a 0.02 ms time constant and tracks the
  *energy-separation amplitude* (DESA-2, Maragos/Kaiser/Quatieri), which is exact for a sinusoid from five samples.
  This removes the Hilbert transform's onset/offset transients (+15 % / −6 % for several ms) that otherwise cost
  ~0.3 dB on the T3 attack and >1 dB on the start of the release. Noise practically never triggers it (15 dB
  excursions of a 1.5 ms log-average are extremely rare).

### Cell (spec 4.3)

`OptoCell` is the reference model verbatim: three components, weights (0.7805, 0.1381, 0.0814), attack
τa·(0.6/a_t)^1.26 with τa = (4.90, 23.1, 110) ms, release τr = (0.264, 2.75, 5.38) s, per-sample exponential update.
The per-sample coefficients are exact exponentials, so trajectories are sample-rate independent.

### Waveform following (spec 6 / T7 under compression)

The literal cell cannot produce the measured −41 dBc H3 at 100 Hz (the cell smooths the detector ripple to about
−50 dBc). The extra gain modulation is driven by `r = ln √((s/e)² + 0.03²) − ln((0.03 + √1.0009)/2)` (exactly zero mean
for a sine, smooth so its sampled mean is rate independent), low-passed by a first-order shelf (corner 125 Hz, floor
0.03), delayed 16.3 µs, scaled by the local slope of the static curve and `(1/g − 1)^0.737`. The constants were found
with `tools/Calibrate.cpp` (`t7q`) against the compressed rows of T7.

### Output stage (T2, T7, T7b, T8, spec 7/8)

* **Make-up**: `T2(display) − R(1 kHz)` where R is the engine's own small-signal 1 kHz response, so the path gain
  at −60 dBFS / 1 kHz equals T2 exactly at every sample rate.
* **Colour** (`tools/fit_colour.py`): memoryless curve on the post-make-up signal
  `y = x + b·Xe(√(1+(x/Xe)²) − 1) + c·Xo·u|u|/(1+|u|) + d·Xd·v^2.5/(1+v^1.5)`; H2 tracks the output level 1 dB/dB and
  saturates at −46 dBc for very high Gain (T7b), H3/H5 rise 1 dB/dB (H5 17 dB under H3) and give the
  +0.05…+0.11 dB level-dependent gain of T7; the `v^2.5` term sets the H4 slope. Fit residual ≤ 2.4 dB over T7 (PR 0)
  and T7b. The curve sits between a +3.15 dB first-order pre-emphasis shelf (2.86 kHz) and its exact inverse, which
  raises the harmonics of high fundamentals by ~2.5 dB (T7 5 kHz rows) without changing the small-signal response.
  The even terms' DC is removed by the T8 high-pass.
* **Response** (`tools/fit_response.py`): −0.027 dB flat, 4.97 Hz first-order high-pass, first-order high shelf
  −0.589 dB at 4.0 kHz; error ≤ 0.16 dB against T8 at 44.1/48/96 kHz.
* **Mix / Trim**: `out = (m·wet + (1−m)·dry)·trim`, exact.
* **Analog**: Gaussian white noise (independent per channel, −135 dBFS/Hz, i.e. σ² = 10^−13.5 · fs/2) plus hum
  harmonics k·50/60 Hz at the tabulated peak levels (common to both channels), added after Trim.

### Parameters and smoothing

| Parameter | Host range / mapping | Default |
|---|---|---|
| Gain | 0–100 display, normalised ↔ display by T0 (piecewise linear, 11 points), 0.01 steps | 40.00 |
| Peak Reduction | 0–100 display, T0 taper, 0.01 steps | 46.00 |
| Mode | Compress / Limit | Compress |
| Hi Freq | 0–100, 0.1 steps, linear | 50 |
| Analog | Off / 50 Hz / 60 Hz | Off |
| Mix | 0–100 %, 0.1 steps | 100 % |
| Trim | −18…+18 dB, 0.1 steps | 0 dB |
| Auto Makeup | Off / On | Off |
| VU Display | In / GR / Out (metering only) | GR |

All audio parameters ramp linearly to their new value in 20 ms (D4): Peak Reduction and the Compress/Limit blend
(static curve re-blended per sample while ramping), make-up in dB, Hi Freq (weighting coefficients updated every
16 samples while ramping), Mix, Trim.

**Auto Makeup** (spec 9, D2) is implemented in the processor as a UI coupling: while it is on, a Peak Reduction change
made on the message thread sets Gain to `T2⁻¹(offset + autoMakeup(PR))`, where `autoMakeup` interpolates the measured
offsets (0 / 3.7 / 6.9 / 9.6 dB at PR 40 / 50 / 60 / 70). The offset is captured when Auto Makeup is switched on and
whenever Gain is moved. The audio make-up is always T2 at the displayed Gain (the hidden-offset defect of D2 is not
reproduced).

### User interface (spec 12)

Original vector design in the shared Craft palette (dark slate panels, amber primary / teal secondary accents):
two large knobs **Gain** (left) and **Peak Reduction** (right) with 0–100 scales whose tick marks are placed with the
parameter's own T0 taper (so the scale is non-linear like the control), editable value boxes; a centre needle gauge
(shallow arc, −20…+3 VU, 0 VU = −18 dBFS sine RMS; GR mode shows dB of reduction on the same scale) with VU-style
ballistics (one-pole, 99 % in ≈ 300 ms) and a digital readout; **In / GR / Out** selector; **Compress / Limit**
segmented switch; **Auto Makeup** toggle; lower strip with **Analog** (Off / 50 Hz / 60 Hz), **Hi Freq** (scale with
"flat" at 100), **Mix** and **Trim** knobs. Double-click or Alt-click resets any control to its default; values can be
typed into the boxes. No brushed metal, vintage logos, hardware-style meter faces or names.

## Spec mapping

| Spec | Implementation | Test |
|---|---|---|
| §2 parameters, T0 tapers | `ParameterLayout.cpp`, `Laws.h` (`Taper`) | `TablesTests` "T0 host tapers" |
| §3 path, latency 0, feed-forward | `LevelerEngine.h` | `OutputTests` "zero latency", `DetectorTests` "feed-forward" |
| §3 sample-rate independence | exact exponentials, pre-warped filters, rate-jointly fitted weighting | `StaticTests` / `DynamicsTests` "sample-rate independent" |
| §4.1 T1C / T1L | `StaticCurve`, detector calibration | `StaticTests` T1C / T1L (all 288 points, ±0.5 dB) |
| §4.2–4.3 T3 | `OptoCell` (literal), detector transient acquisition | `DynamicsTests`: cell model with ideal steps; complete engine with real bursts (600 points each, ±1 dB, RMS ≤ 0.4) |
| §5 T4 / T5 | `Weighting`, `WeightingTable.h` | `StaticTests` T4 (±1 dB), T5 (±0.5 dB), interpolation |
| §6 T6, stereo link D6 | `Detector.h`, max of channel levels | `DetectorTests` T6 (±1 dB), stereo link, mono = one channel (D3) |
| §7 T2 | make-up | `OutputTests` T2 (±0.1 dB) |
| §7 T7 / T7b | `ColourStage`, waveform following | `OutputTests` T7, T7b |
| §7 T8 | `Response` | `OutputTests` T8 at 44.1/48/96 kHz (±0.2 dB) |
| §7 Mix / Trim | engine | `OutputTests` "Mix … Trim" (exact to 1e-9) |
| §8 analog noise | `AnalogNoise` | `OutputTests` analog (each hum line ±2 dB, density ±2 dB, Off < −170 dBFS) |
| §9 Auto Makeup | `PluginProcessor.cpp`, `autoMakeupDb` | `TablesTests` Auto Makeup offsets |
| D4 smoothing | `Ramp` (20 ms) | `OutputTests` smoothing |

## Tests

`ctest -R opto-leveler` (Catch2, 29 test cases, 3200+ assertions). Results at the time of writing (48 kHz):

| Check | Result | Tolerance |
|---|---|---|
| T1C / T1L static, 288 points | worst 0.03 dB | ±0.5 dB |
| T3, 4.3 model with ideal steps | RMS 0.281 dB, max 0.949 dB | RMS ≤ 0.4, ±1 dB |
| T3, complete engine with sine bursts | RMS 0.289 dB, max 0.98 dB | RMS ≤ 0.4, ±1 dB |
| T4 (incl. Limit row) | worst 0.84 dB | ±1 dB |
| T5 | worst 0.21 dB | ±0.5 dB |
| T6 noise | −0.53 / −0.37 / +0.23 / +0.46 dB | ±1 dB |
| T7 (±3 dB class) | worst 2.7 dB | ±3 dB |
| T8 at 44.1 / 48 / 96 kHz | worst 0.16 dB | ±0.2 dB |
| static / trajectory rate dependence (44.1, 96 vs 48 kHz) | 0.025 / 0.024 dB | 0.03 / (0.05, 0.1 attack) |
| analog noise | density −135.0 dB/Hz, hum lines within ±2 dB | ±2 dB |

The tests use `tests/SpecMeasure.h`, which re-creates the measurement procedures of the tables (steady sines and the
mean cell gain, bursts on a −50 dBFS bed sampled at `t` after onset/end, equal-RMS noise measured from the output RMS
relative to the PR 0 path, harmonic analysis by single-bin DFT over whole cycles).

## Questions / assumptions

1. **T3 measurement procedure.** The spec's figures for the 4.3 model (RMS 0.28, max 0.94) are reproduced exactly only
   with an ideal level step; real bursts need the transient acquisition described above (max 0.98 dB, close to the
   1 dB limit at C 50 / −10 dBFS / 10 ms, where the model itself is at 0.95).
2. **T6 detector law** (open question 1): implemented as the compressed-domain average described above, tuned to T6.
   T6 is assumed to be measured at 48 kHz; at other rates white noise has a different HF share in the sidechain.
3. **Hi Freq 100 row.** T4's Compress and Limit rows at Hi Freq 100 imply weightings that differ by up to 1.3 dB at
   10 kHz; the weighting fits their mean (errors +0.82 / −0.38 dB). Above 10 kHz (not measured) the 10 kHz value is
   continued.
4. **T7 compressed rows at −40 / −30 dBFS** show 1.2–2 dB of gain reduction where T1 has none (memory of the previous
   measurement, open question 3); for these rows only the colour (H2) is checked. The T7 *gain* column under
   compression (≥ −18 dBFS) is matched within 1.5 dB (it differs from T1/T4 by up to 0.95 dB itself at 100 Hz).
5. **H4 (100 Hz) and H5 (1 kHz) under compression** need odd-order gain modulation that the full-wave waveform
   following does not create; they are within ±6 dB (100 Hz H4 −4.5 dB, 1 kHz H5 up to +5.2 dB) instead of ±3 dB. All
   other T7 components above −90 dBc are within ±3 dB.
6. **Above 0 dBFS** the static curves continue with the slope of their last segment (up to +24 dBFS, GR ≤ 45 dB).
7. **Stereo link** = max of the channel levels (D6). The waveform-following ripple is taken from the louder channel.
8. **Auto Makeup** curve outside the measured 40…70 range: 0 dB below 40, extended to +12 dB at 80 and +15.5 dB at 100;
   coupling only for changes on the message thread (host automation of Peak Reduction is not coupled — the Gain
   automation recorded together with it plays back instead).
9. **Analog noise level convention**: −135 dBFS/Hz is taken as power relative to a full-scale (1.0) sample value,
   σ² = 10^−13.5·fs/2; total measured −90.7 dBFS RMS (re 1.0). The spec's "≈ −88.8 dBFS RMS" matches if its dBFS is
   referenced to a full-scale sine (+3 dB). Hum is common to both channels, noise independent; hum phases arbitrary
   (all harmonics start at zero phase).
10. **Meter**: 0 VU = −18 dBFS sine RMS (not specified); GR mode shows the mean cell gain reduction.
11. **Mono** layout: the same engine with one channel (D3), not measured.
12. **Output protection** (not in the spec): Gain +38.7 dB plus Trim +18 dB allow +56.7 dB of path gain, so a
    full-scale input at low Peak Reduction would leave the plug-in near +50 dBFS. Samples above +30 dBFS are
    saturated smoothly towards +39.2 dBFS (below the +40 dBFS runaway limit of `tools/hoststress`); nothing below
    +30 dBFS is affected, so none of the spec tables is.

## Public references

* JUCE 8 documentation (AudioProcessorValueTreeState, NormalisableRange, Slider, ParameterAttachment), CLAP and
  clap-juce-extensions documentation.
* P. Maragos, J. F. Kaiser, T. F. Quatieri, "Energy separation in signal modulations with application to speech
  analysis", IEEE Trans. Signal Processing 41(10), 1993 (DESA-2 amplitude estimate).
* Polyphase IIR all-pass pairs for 90° phase splitting (e.g. R. Lyons, *Understanding Digital Signal Processing*,
  Hilbert transformers; the classic allpass-pair quadrature networks); coefficients optimised here.
* R. Bristow-Johnson, "Cookbook formulae for audio EQ biquad filter coefficients" (peaking sections, pre-warping).
* D. Giannoulis, M. Massberg, J. D. Reiss, "Digital dynamic range compressor design — a tutorial and analysis",
  JAES 60(6), 2012 (detector / ballistics background).
* General descriptions of electro-optical (electro-luminescent panel + photo-resistor) leveling amplifiers:
  fast light-dependent attack, two-stage programme-dependent release of the photo-resistor (textbook level).
* IEC 60268-17 / ANSI C16.5 (VU meter ballistics: 99 % in 300 ms).

## Status

DSP core and all spec tests pass on Linux (GCC 13) and Windows (MSVC 2022); VST3, CLAP and Standalone build on
Windows x64; `tools/hoststress` (300 s, with editor) runs clean after the output protection was added (the first run
flagged legitimate +40…+52 dBFS outputs at maximum Gain + Trim with full-scale noise).
