# Implementation notes: Craft Mastering Limiter (spec r1)

This is the builder side of a clean-room implementation of
[`spec/mastering-limiter/r1/SPEC.md`](../../spec/mastering-limiter/r1/SPEC.md) (approved 2026-10-29). I used
only that spec, this repository (conventions, the stereo-imager / flanger code structure), public DSP literature
and my own work. I did not access any original product, analyst workspace, vendor asset or vendor installation.
The not-yet-merged `peak-limiter` branch was not used (nothing is shared with it).

| Target | Product | Plugin code | CLAP id | I/O (spec §1) |
|---|---|---|---|---|
| `CraftMasteringLimiter` | Craft Mastering Limiter | `CMlm` | `com.craftaudio.mastering-limiter` | Stereo 2→2 (linked) or Mono 1→1 via the bus layout |

- **Manufacturer:** Craft Audio (`Crft`). **Version:** 0.1.0. **Formats:** VST3, CLAP, Standalone.
- **Platforms:** Windows x64 (MSVC) and Linux x64 (GCC). **Licence:** AGPL-3.0 (as the repository).
- **Latency** (reported with `setLatencySamples`, §3): 64 samples at 44.1/48 kHz, 128 at 88.2/96 kHz,
  256 at 176.4/192 kHz.

## Architecture

All paths are relative to `plugins/mastering-limiter/`.

```
CMakeLists.txt           mastering_limiter_dsp (INTERFACE), CraftMasteringLimiter plugin, optional snapshot tool
source/dsp/              host-independent, header-only DSP core (no JUCE)
  Parameters.h             §2 mappings, 0.1 dB / 0.01 ms resolution, display strings, Settings
  GainComputer.h           §3-§5: latency config, sliding-minimum hold, fixed / adaptive release, two boxcars,
                           ceiling enforcement
  Requantiser.h            §6-§7: word length, Type 1 / Type 2 dither, error-feedback noise shaper, PRNG
  NoiseShapingCoefficients.h  generated NTF tables (6 sample rates x 3 modes x 40 taps)
  MasteringEngine.h        §1 signal flow, L-sample delay of audio and make-up, 20 ms Threshold/Ceiling ramps
                           (§8), bypass crossfade, meter data
source/plugin/           JUCE AudioProcessor + parameter layout
source/ui/               editor, look and feel, value boxes, segmented selectors, meters (all vector graphics)
tests/                   Catch2 tests (CTest prefix "mastering-limiter/")
tools/design_noise_shapers.py   offline NTF design (numpy/scipy), writes NoiseShapingCoefficients.h
tools/Snapshot.cpp       renders the editor to PNG (CRAFT_BUILD_SNAPSHOT)
```

Per sample t the engine computes the linked sample peak, the gain computer returns `g[n]` and `r[n]` for the
aligned sample `n = t − L`, the audio and the make-up factor (taken at the same input time as the threshold that
produced `r`) come out of L-sample delay lines, `v = x·g·makeup` goes through the per-channel requantiser.
Everything runs in double precision (D4); float and double host buffers are supported.

## Spec mapping

| Spec | Implementation |
|---|---|
| §2 parameters | `ParameterLayout.cpp`; Threshold/Ceiling `−30 + 30 r` snapped to 0.1 dB ("-12.0 dB"); Release `0.01·10^(5r)` ms snapped to 0.01 ms ("1.00 ms", r = 0.4 → 1.00); Adaptive bool (default on); Word Length / Dither / Shaping as `AudioParameterChoice` (index k ↔ r = k/(n−1), exactly the spec steps); host bypass parameter. Defaults: 0 dB / 0 dB / 1.00 ms / Adaptive on / 24 bit / Type 1 / Normal (D5). |
| §3 latency | `latencyConfigFor()`: L = 64/128/256 with (A,B) = (38,27)/(75,54)/(150,107); non-standard rates use the nearest family. Reported to the host in `prepareToPlay`. |
| §4.1–4.2 | sample peak `max(|xL|,|xR|)`, `r = min(1, T/p)`; no true-peak detection. |
| §4.3 | `SlidingMin` (monotonic deque) over the last L required gains = `min(r[n+1] … r[n+L])` in aligned time. |
| §4.4 | `q = h` if `h < q`, else `h − (h − q)·c`, `c = exp(−ln10/(Release·fs))`. |
| §4.5 | `Boxcar` A then B (causal running sums). When the whole window is 1.0 the output is exactly 1.0 (count of non-unity samples), so a non-limiting stream is bit-exact. |
| §4.6–4.7 | `g = min(s, r[n])`; `v = x·g·10^((C−T)/20)`. |
| §5 adaptive | recommended realisation: fast follower (step 4, 0.65 ms) and slow follower (one-pole τ = 99 ms downwards, step 4 with 500 ms upwards), `q = min(fast, slow)`. Release control ignored (D2). |
| §6 | `y = Δ·nearbyint(v/Δ)` (scaling by 2^(b−1) is exact; ties to even), Δ = 2^(1−b); output clipped to ±1.0. Digital silence (v == 0 exactly) with Dither None or Type 2 gives exactly 0 and pushes a zero error into the shaper history. |
| §7 error feedback | `u = v + Δ·Σ_{k≥1} h_k e[n−k]`, `y = Δ·round(u/Δ + d)`, `e = y/Δ − u/Δ` ⇒ `y − v = H(z)·e` with the monic minimum-phase FIR NTF H. |
| §7.1 Type 1 | TPDF ±1 LSB (sum of two independent uniform ±½), per channel independent PRNG streams, always added. |
| §7.1 Type 2 | no shaping: RPDF ±½ LSB (0.41 LSB rms). With shaping: TPDF ±a LSB on a moving signal (a = 0.20 / 0.18 / 0.15 for Moderate / Normal / Ultra), RPDF ±½ LSB on a static non-zero input (range < 2 LSB over the last 32–64 samples) to break limit cycles (D3), nothing on exact digital silence (output 0). |
| §7.3 NTF | 40-tap monic minimum-phase FIR per rate (44.1, 48, 88.2, 96, 176.4, 192 kHz) and mode, fitted offline (see below). Other rates use the nearest design. |
| §8 | Threshold and Ceiling: linear ramps in dB over 20 ms that land exactly on the target; Release / Adaptive immediately; Word Length / Dither / Shaping at the block boundary (changing the shaping clears the error history). |
| §12 D1–D5 | D1 `min(s, r)`; D2 Release greyed out with Adaptive on; D3 see Type 2; D4 double precision; D5 defaults. |
| §14 UI | see below. |

### Noise-shaping filter design (§7.3)

`tools/design_noise_shapers.py` interpolates each table (dB, linear in frequency), removes the mean so the target
satisfies the Gerzon–Craven condition (a monic minimum-phase filter has zero mean log-magnitude), takes the
minimum-phase cepstral solution as a start and refines the 39 free taps by weighted least squares on the dB
magnitude (table points weighted strongly, a dense grid weakly). It verifies the tolerance at every table
point and that all zeros are inside the unit circle (max |zero| 0.91–0.97). Fit at the table points:
≤ 0.47 dB at 44.1/48 kHz, ≤ 0.22 dB at 96 kHz. Resulting Type 1 totals at 48 kHz: 1.00 / 1.94 / 3.79 LSB rms
(spec 1.01 / 1.98 / 3.92).

- **88.2 kHz:** the 96 kHz curves cut at 44.1 kHz have a negative mean log-magnitude (the dropped 44–48 kHz band
  is +14…+19 dB), which no monic minimum-phase filter can match. The missing mean is distributed in proportion to
  the tolerance (weight 1 up to 20 kHz, 2.5 above); worst deviation ≈ 0.7 dB in band, ≈ 1.8 dB above 20 kHz.
- **176.4/192 kHz** (open question 3): target = 96 kHz curve − 1 dB up to 20 kHz, a transition to a plateau
  above 26 kHz whose level satisfies the zero-mean condition. The fitted response stays below the 96 kHz curve
  up to 20 kHz (tested with 0.5 dB estimation slack).

### User interface (§14)

Original vector artwork in the Craft palette (dark panels, amber/teal accents, as the other Craft plugins).

- Left **Input / Threshold**: L/R input peak bars on a 0 … −30 dB scale; the amber marker beside the bars is the
  Threshold (drag it or click on the scale; Shift = fine; double-click resets); value box below ("-12.0").
- Centre **Atten**: gain-reduction bar growing down from 0 dB (0 … −30 dB) with peak-hold line, "Atten" peak
  readout (click resets), make-up gain text, and the **LINK** toggle (moves both markers together, keeping
  their offset; stored in the plug-in state, not automatable).
- Right **Output / Ceiling**: L/R output bars with the Ceiling marker and value box, L/R peak-hold readouts in
  dBFS (click resets; red at ≥ 0 dBFS).
- Bottom: **Release** value box (dimmed and inactive while Adaptive is on), **ADAPTIVE** toggle, **Word Length**
  (24/22/20/18/16), **Dither** (None/Type 1/Type 2) and **Noise Shaping** (None/Moderate/Normal/Ultra) segmented
  selectors. Every value box: drag, click to type, double-click / Alt-click to reset.
- Mono layout: one bar per meter and one peak readout.

## Tests

`ctest -R mastering-limiter/` (Catch2, host-independent DSP core): 35 test cases (≈ 4.1 million assertions), all passing on Windows (MSVC 2022, Release) and Linux (GCC 13).

| File | Covers |
|---|---|
| `LimiterTests.cpp` | latency/boxcar table at 6 rates; neutral impulse bit-exact (4 rates) and a no-over program bit-exact; static staircase (+12 dB below threshold, all samples ≤ 1.0); T −12/C −3 (+9 dB, peak = 10^(−3/20)); click on DC carrier (falls exactly 64 samples early, 0.2512 one sample before, exact two-boxcar S-curve); two clicks (recovery targets the second's requirement, equals the literal spec model); fs/4 sines (sample peaks 1.0607 → exactly 1.0; 0.707 passes bit-exact); fixed-release t63/t90/t99 for 0.01…1000 ms (48 and 96 kHz, ±0.6 % or 0.03 ms); engine vs a literal transcription of §4 on a stereo noise program at 44.1/48/96 kHz (≤ −100 dB; ≤ −85 dB at 500 ms); ceiling guarantee for all dither types, word lengths, both release modes; 20 ms parameter smoothing; bypass = delayed input; linked stereo detection. |
| `AdaptiveReleaseTests.cpp` | §5 table at 44.1/48/96 kHz (±25 %, 5 ms t99 ±35 %), monotonic recovery; Release ignored (bit-identical outputs for 3.16/31.6/316 ms); attack identical to fixed mode. |
| `RequantiserTests.cpp` | exact rounding for every word length (±4 LSB ramp); Type 1 on silence (values {−1,0,1}, P(±1) = 12.5 %, 0.50 rms, white ±0.2 dB); Type 1 on a signal (0.50 rms, channels independent); Type 2 unshaped 0.41 rms; silence stays exactly 0 for None and Type 2 in every shaping mode, rate and word length (also after loud material); no idle tones for constant input with Type 2 (spectral peak test); clipping. |
| `NoiseShapingTests.cpp` | measured NTF (Welch PSD of the total error, Type 1 on silence) at every §7.3 point for 44.1/48/88.2/96 kHz at 16 and 20 bit (±1.5 dB, ±2.5 dB above 20 kHz at 96k), integrated in-band (≤ 15 kHz) power ±1 dB; §7.2 totals (Type 1 on silence and −60 dBFS sine, Type 2 on −60 dBFS sine, ±10 % / ±0.05); Type 2 PSD 4.8 ± 1 dB below Type 1 at every table frequency (44.1/48/96 kHz); NTF independent of word length; designs monic + minimum phase (zero mean log-magnitude); 176.4/192 kHz below the 96 kHz curve up to 20 kHz. |
| `ParameterTests.cpp` | mappings, resolution, display strings, defaults, parsing, default output is dithered. |

Measured adaptive recovery at 48 kHz (spec in brackets): 5 ms 1.00 (0.96) / 1.45 (1.35) / 396 (377) ms;
20 ms 1.04 (1.10) / 144 (126) / 644 (626); 100 ms 121 (111) / 404 (392) / 904 (892); 500 ms 216 (217) /
499 (498) / 999 (998); 2000 ms 218 (219) / 501 (501) / 1001 (1000); 6 and 24 dB depth identical to 12 dB.

## Questions / assumptions

1. **Output clipping (§6, open question 4):** clipped at ±1.0 (float full scale), not at the largest two's
   complement code `1 − Δ`: the §9 vectors require output sample peaks of exactly 1.0 at Ceiling 0 dB.
2. **Type 2 dither (open question 2, D3):** mechanism as in the table above. Level/spectrum meet §7.1/§7.2
   (measured 4.0–4.9 dB below Type 1); on a static non-zero input it uses RPDF ±½ LSB, so its noise there is
   higher than 4.8 dB below Type 1 (no tolerance is specified for that case; D3 only requires no idle tones).
3. **Digital-silence rule:** "silence" is an exactly zero requantiser input sample; the output is exactly 0 for
   Dither None and Type 2 in every mode (D3, §6), including after loud material. Type 1 dithers always.
4. **Dither None + shaping on a constant non-zero input** can limit-cycle (classic error-feedback behaviour). The
   spec only addresses silence for None, so this is left as is (Type 2 is the non-tonal choice).
5. **Release values** are snapped to the 0.01 ms display resolution ("value = 2-decimal display").
6. **Non-standard sample rates:** latency/boxcars from the nearest family (boundaries 66 and 132 kHz), NTF from
   the nearest designed rate (log distance).
7. **88.2 kHz NTF / 176.4–192 kHz NTF:** see the design section.
8. **Parameter smoothing (§8):** linear dB ramps of 20 ms for Threshold and Ceiling (the make-up factor is delayed
   with the audio so the ceiling guarantee also holds during a ramp); host bypass crossfades over 10 ms to the
   L-delayed dry signal, keeping the latency constant.
9. **Mono:** offered through the bus layout of the same product (mono in/mono out); no separate mono product.
10. **Meter ballistics** (§11, not specified): instant attack, ≈ 27 dB/s decay, 1.5 s peak-hold line.

## Public references

- M. A. Gerzon, P. G. Craven, "Optimal Noise Shaping and Dither of Digital Signals", AES 87th Convention, 1989
  (zero mean log-NTF theorem for minimum-phase noise shapers).
- S. P. Lipshitz, J. Vanderkooy, R. A. Wannamaker, "Minimally Audible Noise Shaping", JAES 39(11), 1991;
  "Quantization and Dither: A Theoretical Survey", JAES 40(5), 1992 (RPDF/TPDF properties, error feedback).
- R. A. Wannamaker, "Psychoacoustically Optimal Noise Shaping", JAES 40(7/8), 1992.
- A. V. Oppenheim, R. W. Schafer, *Discrete-Time Signal Processing* (real-cepstrum minimum-phase construction).
- D. Lemire, "Streaming Maximum-Minimum Filter Using No More than Three Comparisons per Element", 2006
  (monotonic-deque sliding minimum).
- S. Vigna, xorshift128+ PRNG; splitmix64 seeding (public domain).
- JUCE 8 and clap-juce-extensions documentation.

## Status

- DSP core complete for every §9 test vector; all 35 unit tests pass (Windows MSVC Release via CTest, Linux GCC).
- VST3, CLAP and Standalone build on Windows x64 without warnings in this plug-in's sources; the editor was checked
  with the snapshot tool (stereo, mono, non-default settings).
- Not done: an independent verification report (`docs/verification/mastering-limiter-r1.md`) is the
  verifier's job; the Linux plug-in targets were not built locally (only the DSP tests), CI covers them.
