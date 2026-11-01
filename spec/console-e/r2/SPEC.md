# E-type console channel strip — behavioural specification r2

Status: APPROVED r2 (2026-10-30, supersedes r1; verification of the first build; r1 review gate: review gate: standing policy — match the reference, do not reproduce defects, original visuals, own naming; decisions §10). Frozen. Derived from black-box measurement of a licensed reference plug-in and from its
user documentation (paraphrased). Contains no vendor, product or brand names. Measured target tables are in
`APPENDIX.md` (same folder), which is part of this specification.

## 1. Scope and variants

A channel strip consisting of input trim, polarity switch, a 4-band equaliser with high-pass and low-pass filters,
a compressor/limiter, an expander/gate, routing switches, an "analogue" switch and an output fader.

| Variant | Channels |
|---|---|
| Stereo (primary) | 2 in / 2 out, one linked dynamics detector |
| Mono | 1 in / 1 out, same processing |

Latency: 0 samples. All curves are defined in Hz. The Appendix A tables (48 kHz) are normative at every sample rate.
On the reference, 44.1 and 96 kHz agree with 48 kHz within ±0.1 dB below 8 kHz for filters, LF and mid bands; the HF
band at its highest frequency settings differs by up to 1.5 dB between 48 and 96 kHz (see tolerance §12).

## 2. Parameters

Host-normalised value r ∈ [0,1]. **Every continuous control is piecewise linear between the 21 points r = k/20**
given below (normative; the displayed value at those points is the value shown, values in between are linear in r).
Release controls act in seconds (the reference labels them "ms"; the label is not reproduced). Display resolution:
0.1 dB for levels, 1 Hz / 0.01 kHz for frequencies, 0.01 for Q and release, 0.1 for ratio.

| Control | 0.00 | 0.05 | 0.10 | 0.15 | 0.20 | 0.25 | 0.30 | 0.35 | 0.40 | 0.45 | 0.50 | 0.55 | 0.60 | 0.65 | 0.70 | 0.75 | 0.80 | 0.85 | 0.90 | 0.95 | 1.00 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Input trim (dB) | -18.0 | -16.2 | -14.4 | -12.6 | -10.8 | -9.0 | -7.2 | -5.4 | -3.6 | -1.8 | 0.0 | 1.8 | 3.6 | 5.4 | 7.2 | 9.0 | 10.8 | 12.6 | 14.4 | 16.2 | 18.0 |
| Output fader (dB) | -24.0 | -22.2 | -20.4 | -18.6 | -16.8 | -15.0 | -13.2 | -11.4 | -9.6 | -7.8 | -6.0 | -4.2 | -2.4 | -0.6 | 1.2 | 3.0 | 4.8 | 6.6 | 8.4 | 10.2 | 12.0 |
| Comp threshold (dB) | 10.0 | 8.5 | 7.0 | 5.5 | 4.0 | 2.5 | 1.0 | -0.5 | -2.0 | -3.5 | -5.0 | -6.5 | -8.0 | -9.5 | -11.0 | -12.5 | -14.0 | -15.5 | -17.0 | -18.5 | -20.0 |
| Comp ratio (:1) | 1.0 | 1.3 | 1.5 | 1.8 | 2.0 | 2.3 | 2.7 | 3.0 | 3.7 | 4.3 | 5.0 | 6.0 | 7.0 | 8.0 | 12.0 | 16.0 | 20.0 | 40.0 | 60.0 | 80.0 | ∞ |
| Comp release (s) | 0.10 | 0.11 | 0.13 | 0.14 | 0.15 | 0.17 | 0.18 | 0.20 | 0.27 | 0.33 | 0.40 | 0.60 | 0.80 | 1.00 | 1.33 | 1.67 | 2.00 | 2.50 | 3.00 | 3.50 | 4.00 |
| Exp threshold (dB) | -30.0 | -28.7 | -27.5 | -26.2 | -25.0 | -22.8 | -20.7 | -18.5 | -16.3 | -14.2 | -12.0 | -9.2 | -6.3 | -3.5 | -0.7 | 2.2 | 5.0 | 6.3 | 7.5 | 8.7 | 10.0 |
| Exp range (dB) | 0.0 | 1.3 | 2.5 | 3.8 | 5.0 | 6.7 | 8.3 | 10.0 | 13.3 | 16.7 | 20.0 | 23.3 | 26.7 | 30.0 | 31.7 | 33.3 | 35.0 | 36.3 | 37.5 | 38.7 | 40.0 |
| Exp release (s) | 0.10 | 0.11 | 0.13 | 0.14 | 0.15 | 0.17 | 0.18 | 0.20 | 0.27 | 0.33 | 0.40 | 0.60 | 0.80 | 1.00 | 1.33 | 1.67 | 2.00 | 2.50 | 3.00 | 3.50 | 4.00 |
| HP frequency (Hz, label) | Off | 17 | 18 | 19 | 20 | 37 | 53 | 70 | 87 | 103 | 120 | 147 | 173 | 200 | 233 | 267 | 300 | 313 | 325 | 337 | 350 |
| LP frequency (kHz, label) | Off | 19.5 | 17.0 | 14.5 | 12.0 | 10.7 | 9.3 | 8.0 | 7.0 | 6.0 | 5.0 | 4.7 | 4.3 | 4.0 | 3.8 | 3.7 | 3.5 | 3.4 | 3.3 | 3.1 | 3.0 |
| EQ gains LF/LMF/HMF/HF (dB, label) | -15.0 | -13.5 | -12.0 | -10.5 | -9.0 | -7.5 | -6.0 | -4.5 | -3.0 | -1.5 | 0.0 | 1.5 | 3.0 | 4.5 | 6.0 | 7.5 | 9.0 | 10.5 | 12.0 | 13.5 | 15.0 |
| LF frequency (Hz, label) | 30 | 35 | 40 | 45 | 50 | 67 | 83 | 100 | 133 | 167 | 200 | 233 | 267 | 300 | 321 | 343 | 364 | 386 | 407 | 429 | 450 |
| LMF frequency (kHz, label) | 0.20 | 0.23 | 0.25 | 0.28 | 0.30 | 0.47 | 0.63 | 0.80 | 0.87 | 0.93 | 1.00 | 1.17 | 1.33 | 1.50 | 1.67 | 1.83 | 2.00 | 2.13 | 2.25 | 2.37 | 2.50 |
| HMF frequency (kHz, label) | 0.60 | 0.65 | 0.70 | 0.75 | 0.80 | 1.03 | 1.27 | 1.50 | 2.00 | 2.50 | 3.00 | 3.50 | 4.00 | 4.50 | 5.00 | 5.50 | 6.00 | 6.25 | 6.50 | 6.75 | 7.00 |
| HF frequency (kHz, label) | 1.50 | 1.63 | 1.75 | 1.88 | 2.00 | 3.00 | 4.00 | 5.00 | 6.00 | 7.00 | 8.00 | 8.67 | 9.33 | 10.00 | 11.33 | 12.67 | 14.00 | 14.50 | 15.00 | 15.50 | 16.00 |
| LMF Q / HMF Q | 3.50 | 3.25 | 3.00 | 2.75 | 2.50 | 2.25 | 2.00 | 1.88 | 1.75 | 1.63 | 1.50 | 1.37 | 1.25 | 1.13 | 1.00 | 0.88 | 0.75 | 0.62 | 0.50 | 0.30 | 0.10 |

Ratio: values between the 21 points are linear in r (∞ at r = 1). HP: r = 0 is Off. LP: r = 0 is Off.
Defaults (r): input trim 0.5 (0 dB); output fader 0.667 (0.0 dB); comp threshold 0.333 (0.0 dB); ratio 0 (1:1);
comp release 0.5 (0.40 s); exp threshold 0 (−30 dB); range 0; exp release 0.35 (0.20 s); filters Off; EQ gains 0.5;
LF 0.23 (60 Hz); LMF 0.26 (0.50 kHz); HMF 0.55 (3.50 kHz); HF 0.5 (8.00 kHz); LMF Q 0.2 (2.50); HMF Q 0.5 (1.50).
With the taper above these defaults display exactly the stated values (no adjustment of r is needed or allowed).

Switches (all default off/normal): analogue on/off; compressor fast attack; expander fast attack; expander/gate mode
(default expander); EQ bypass; EQ to dynamics sidechain; dynamics bypass; dynamics after EQ ("channel out");
filters split (filters before dynamics); polarity invert; meter input/output (display only).

**Labels vs. behaviour.** The frequency labels of the filters and EQ bands are nominal. The actual response for each
control position is defined by the tables in Appendix A (derived band centre frequencies in A.17). Implementations
must reproduce the measured responses, not the labels. Appendix A rows are keyed by the displayed value of the r2
taper (e.g. "Q 0.50" is r = 0.90, "LMF 0.50 kHz" is r = 0.26).

## 3. Signal flow

```
in → input trim → polarity → fixed saturation (§9) → [split: HP/LP] → A → B → output fader → [analogue noise] → out
     default:  A = dynamics, B = EQ (with HP/LP unless split)
     channel out: A = EQ, B = dynamics
```
- **EQ to sidechain:** the EQ (and HP/LP unless split) is removed from the audio path and filters the dynamics
  detector input instead.
- **EQ bypass:** EQ section (and HP/LP) passes audio unchanged. **Dynamics bypass:** compressor and expander pass audio
  unchanged. With all controls neutral the output equals the input within 1.4·10⁻⁷ (float rounding) plus the §9
  saturation (negligible below −20 dBFS).
- Polarity inverts the signal (exact).
- Input trim and output fader are exact gains with their displayed dB values (measured ±0.002 dB).

Gain staging: there is no output limiting or clipping anywhere in the chain; with trim, EQ boosts, make-up and fader
at maximum the output exceeds 0 dBFS by tens of dB (the reference likewise, unbounded). Output must stay finite.

## 4. High-pass and low-pass filters

- **High-pass:** third-order Butterworth (one real pole and one pole pair with Q = 1.0, all at the corner
  frequency f_HP), 18 dB/oct. f_HP per control position (fit error ≤ 0.5 dB; interpolate log-linearly in r):

| r | 0.1 | 0.2 | 0.3 | 0.4 | 0.5 | 0.6 | 0.7 | 0.8 | 0.9 | 1.0 |
|---|---|---|---|---|---|---|---|---|---|---|
| f_HP (Hz) | 16.9 | 17.8 | 28.2 | 56.0 | 102.9 | 132.1 | 174.6 | 230.9 | 322.9 | 414.8 |
- **Low-pass:** two-pole low-pass, 12 dB/oct, analogue-matched (no bilinear frequency warping). Analogue pole
  frequency f_LP and Q per control position (48 kHz fits; above ≈ 12 kHz the reference's digital poles show raised Q
  as a result of its matched design — the normative target is the measured magnitude, Appendix A.2):

| r | 0.4 | 0.5 | 0.6 | 0.7 | 0.8 | 0.9 | 1.0 |
|---|---|---|---|---|---|---|---|
| f_LP (Hz) | 12533 | 8047 | 6056 | 4824 | 4177 | 3837 | 3342 |
| Q | 0.69 | 0.64 | 0.63 | 0.64 | 0.65 | 0.66 | 0.67 |

  The f_LP/Q pairs above are **informative only**: a single two-pole section with these values does not reproduce
  the measured magnitude above ≈ 8 kHz (it undershoots the roll-off by several dB at 12.5 kHz). **Appendix A.2 is the
  normative target** at every sample rate (with the §12 tolerances); additional poles may be used to meet it.
  Loss at 20 kHz (48 kHz reference): r = 0.1 → −0.6 dB, 0.2 → −2.0 dB, 0.3 → −7.7 dB (Appendix A.2).
  Above 0.25·fs the reference rolls off more steeply at 44.1/48 kHz than at 96 kHz; the target is the 96 kHz
  (analogue-like) behaviour with the tolerance of §12.
- r = 0 (Off) bypasses each filter exactly.

## 5. Equaliser bands

All four bands are second-order sections whose response is analogue-like (no warping toward Nyquist):

| Band | Shapes | Character |
|---|---|---|
| LF | shelf (default) or bell | shelf: second-order low shelf, maximum gain +16.0 / −16.7 dB at control ±15; slight overshoot (<0.1 dB) on the flat side. Bell: bandwidth narrows with boost |
| LMF, HMF | bell | "proportional-Q": the effective bandwidth narrows as |gain| increases; Q control sets the base width; peak gain exceeds the control value (≈ +18.4 dB at +15 with Q 3.5; ≈ +15.3 dB at Q 0.1) |
| HF | shelf (default) or bell | shelf: second-order high shelf, ≈ +15.7 / −15.7 dB maximum; bell as mids with frequency-derived width |

- Cut and boost are approximately reciprocal (a cut of the same control value mirrors the boost within ~1 dB).
- Wide mid-band settings (Q ≤ 0.5) at large gain show a small residual high-frequency shelf (≤ +0.7 dB at +15 dB).
- The measured responses in Appendix A (A.3–A.16, plus derived peak parameters A.17) are normative.

## 6. Compressor / limiter

- **Detector:** stereo-linked (one gain for both channels; a −40 dBFS channel receives the same reduction as a
  −10 dBFS channel). Level-sensing with a mild high-frequency emphasis: for a −6 dBFS sine (ratio 4, threshold −10)
  reduction rises from 9.6 dB at 50 Hz to 10.3 dB at 1 kHz and 11.7 dB at 10 kHz (table §6.4).
- **Level reference:** threshold control values are dB re an internal reference where −18 dBFS = 0 dB.
- **Static curve:** very soft knee. Gain = M(T,R) + GR_R(u), u = L − 0.864·T with L the steady 1 kHz sine level in
  dBFS and T the threshold control; GR_R tabulated in Appendix B.1 (curves for R = 1.5, 2, 3, 4, 8, 12, 20, ∞;
  interpolate linearly in 1 − 1/R between tabulated ratios; R = 1 → no reduction). The table reproduces all measured
  thresholds within 0.2 dB (0.5 dB at ∞).
- **Automatic make-up gain** M(T,R): Appendix B.2; zero at threshold +10; linear in T between +10 and −10;
  interpolate in T and in 1 − 1/R.
- **Attack:** slow ("auto", default): 10/63/90 % of the gain change after a +30 dB step in ≈ 6 / 46 / 90 ms;
  fast: ≈ 1 / 2 / 5 ms.
- **Release** (recovery after a −30 dB step from ≈ 13 dB reduction):

| Release control | slow attack t63 / t90 | fast attack t63 / t90 |
|---|---|---|
| 0.1 s | 87 / 217 ms | 31 / 73 ms |
| 0.4 s | 127 / 339 ms | 66 / 278 ms |
| 1.0 s | 334 / 1371 ms | 298 / 1404 ms |
| 4.0 s | 1659 / 3431 ms | 1662 / 3433 ms |

### 6.4 Frequency dependence of reduction (ratio 4, threshold −10, −6 dBFS sine, dB gain incl. make-up)
| 50 Hz | 100 | 200 | 500 | 1 k | 2 k | 5 k | 10 k |
|---|---|---|---|---|---|---|---|
| −9.57 | −9.55 | −9.66 | −9.84 | −10.33 | −10.84 | −11.50 | −11.67 |

### 6.5 Dependence of steady-state reduction on attack mode and release (normative)
With a steady 1 kHz sine the reduction is not independent of the timing controls: relative to slow attack / release
0.4 s (the condition of Appendix B) the gain changes by ΔG below (ratio 4, threshold −10; level in dBFS; dB, negative =
more reduction). For other ratios and thresholds scale with the reduction: ΔG ≈ −k·|GR| where GR is the Appendix B
reduction and k is the last column (informative rule, ±0.4 dB of the table at ratio 4; the table is normative).

| Attack | Release | −24 | −18 | −12 | −6 | 0 | k |
|---|---|---|---|---|---|---|---|
| slow | 0.1 s | +0.30 | +0.42 | +0.41 | +0.50 | +0.37 | -0.037 |
| slow | 0.4 s | +0.00 | +0.00 | +0.00 | +0.00 | +0.00 | 0.000 |
| slow | 1.0 s | -0.21 | -0.34 | -0.53 | -0.47 | -0.58 | 0.053 |
| slow | 4.0 s | -0.54 | -0.86 | -1.16 | -1.29 | -1.58 | 0.116 |
| fast | 0.1 s | -0.89 | -1.20 | -1.61 | -1.68 | -1.95 | 0.151 |
| fast | 0.4 s | -1.03 | -1.47 | -1.89 | -2.02 | -2.34 | 0.178 |
| fast | 1.0 s | -1.13 | -1.62 | -2.08 | -2.31 | -2.64 | 0.199 |
| fast | 4.0 s | -1.22 | -1.77 | -2.30 | -2.57 | -2.95 | 0.221 |

### 6.6 Attack timing vs. release (10/63/90 % of a +30 dB step, ratio 4, threshold −10)
| Release | slow attack | fast attack |
|---|---|---|
| 0.1 s | 6.5 / 46 / 89 ms | 1.5 / 3.0 / 5.5 ms |
| 0.4 s | 6.0 / 45 / 88 ms | 1.5 / 3.0 / 6.0 ms |
| 1.0 s | 6.5 / 45.5 / 94 ms | 1.5 / 3.0 / 7.0 ms |
| 4.0 s | 7.5 / 60 / 123.5 ms | 1.5 / 5.0 / 11 ms |

## 7. Expander / gate

- Linked detector as §6. Threshold in the same dB reference.
- **Expander:** below threshold the gain falls ≈ 1.23 dB per dB (≈ 1 : 2.2), down to a floor set by Range.
  Measured floors: range 5 → 0.8 dB, 10 → 7.7, 15 → 14.5, 20 → 21.4 dB. Above 20 dB the floor was not reached
  within the −60 dBFS test range; specification choice: floor = 21.4 + 1.38·(range − 20) dB (continuing the
  15–20 dB slope).
- **Gate:** hard switching between unity and the floor at threshold (opens when the input rises above threshold).
  Gate floor: range 20 → 25.4 dB, 40 → 52.7 dB (interpolate linearly in dB for other ranges; 0 → 0 dB).
- **Opening** (both modes): within ≈ 4 ms (fast attack: < 1 ms).
- **Closing** after the signal falls 35 dB below threshold: 

| Release | expander t10 / t63 / t90 | gate t10 / t63 / t90 |
|---|---|---|
| 0.1 s | 24 / 68 / 96 ms | 36 / 156 / 332 ms |
| 0.4 s (fast attack) | 58 / 166 / 236 ms | 86 / 390 / 830 ms |
| 1.0 s | 128 / 364 / 520 ms | 188 / 856 / 1826 ms |
| 4.0 s | 470 / 1344 / 1922 ms | 626 / 2260 / 3700 ms |

### 7.1 Static expander / gate gain (normative; steady 1 kHz sine, dB gain vs input level)
Corrected in r2: the r1 list was offset by +9 dB in input level. Gain 0 dB at all levels not listed above the
table's range. The expansion slope is ≈ 1.17–1.23 dB/dB; the expander begins ≈ 2–6 dB below the threshold's internal
level (threshold control − 18 dBFS), the gate switches at ≈ that internal level (thr −20 → opens between −39 and −36
dBFS; thr −10 → between −33 and −30 dBFS). Values follow the table; interpolate linearly in level, threshold and range.

| Setting \ input (dBFS) | -57 | -54 | -51 | -48 | -45 | -42 | -39 | -36 | -33 | -30 | -27 | -24 | -21 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Exp, thr −30, range 40 | -11.0 | -7.6 | -4.1 | -0.5 | +0.0 | +0.0 | +0.0 | +0.0 | +0.0 | +0.0 | +0.0 | +0.0 | +0.0 |
| Exp, thr −20, range 40 | -20.3 | -16.9 | -13.4 | -9.9 | -6.3 | -2.6 | +0.0 | +0.0 | +0.0 | +0.0 | +0.0 | +0.0 | +0.0 |
| Exp, thr −10, range 40 | -30.2 | -26.8 | -23.3 | -19.8 | -16.1 | -12.5 | -8.7 | -4.9 | -1.1 | +0.0 | +0.0 | +0.0 | +0.0 |
| Exp, thr 0, range 40 | -40.5 | -37.1 | -33.6 | -30.1 | -26.5 | -22.8 | -19.0 | -15.2 | -11.4 | -7.6 | -4.0 | -0.4 | +0.0 |
| Exp, thr −20, range 10 | -7.5 | -7.6 | -7.7 | -7.7 | -5.8 | -2.1 | +0.0 | +0.0 | +0.0 | +0.0 | +0.0 | +0.0 | +0.0 |
| Exp, thr −20, range 20 | -20.1 | -16.7 | -13.2 | -9.6 | -6.0 | -2.3 | +0.0 | +0.0 | +0.0 | +0.0 | +0.0 | +0.0 | +0.0 |
| Exp, thr −10, range 5 | -0.7 | -0.8 | -0.8 | -0.8 | -0.8 | -0.8 | -0.8 | -0.8 | -0.3 | +0.0 | +0.0 | +0.0 | +0.0 |
| Exp, thr −10, range 15 | -14.3 | -14.5 | -14.5 | -14.5 | -14.5 | -12.0 | -8.3 | -4.5 | -0.7 | +0.0 | +0.0 | +0.0 | +0.0 |
| Exp, thr −10, range 30 | -30.1 | -26.7 | -23.2 | -19.6 | -16.0 | -12.3 | -8.6 | -4.8 | -1.0 | +0.0 | +0.0 | +0.0 | +0.0 |
| Gate, thr −20, range 40 | -52.2 | -52.7 | -52.7 | -52.7 | -52.7 | -52.7 | -52.7 | +0.0 | +0.0 | +0.0 | +0.0 | +0.0 | +0.0 |
| Gate, thr −10, range 40 | -52.2 | -52.7 | -52.7 | -52.7 | -52.7 | -52.7 | -52.7 | -52.7 | -52.7 | +0.0 | +0.0 | +0.0 | +0.0 |
| Gate, thr −10, range 20 | -25.0 | -25.4 | -25.4 | -25.4 | -25.4 | -25.4 | -25.4 | -25.4 | -25.4 | +0.0 | +0.0 | +0.0 | +0.0 |

## 8. Routing (verified behaviour)

With ratio 4, threshold −10 and LF +12 dB at 100 Hz (−6 dBFS tones), gain at 50 Hz: default (dynamics → EQ)
+3.2 dB; channel out (EQ → dynamics) −4.9 dB; EQ in sidechain −17.6 dB (no audible boost, more reduction).
Split with HP at r=1: the filter acts before the dynamics (50 Hz: −52.2 dB vs −64.2 dB unsplit).

## 9. Fixed saturation and "analogue" switch

- A fixed, very mild odd-order expansion acts on the signal after the input trim in all modes:
  y ≈ x + 0.0032·x³ (x full-scale = 1): at 0 dBFS the gain is +0.021 dB, H3 −62 dBc (H2 ≈ −94 dBc is present in the reference but is informative only and
  need not be reproduced);
  H3 rises 12 dB per 6 dB of level. Input trim shifts it, the output fader does not.
- **Analogue On** adds a constant noise floor of −110.7 dBFS rms (per channel, broadband) and very low inter-channel
  leakage (≈ −69 dB); it does not change frequency response or distortion.

## 10. Decisions (review gate)

- **D1** Frequency labels are nominal; behaviour follows the measured responses (labels kept for display).
- **D2** Filters and EQ follow the analogue-like shape up to Nyquist at every sample rate; the reference's extra
  roll-off of the low-pass near Nyquist at 44.1/48 kHz is not required (tolerance covers either).
- **D3** Input trim and output fader are exact dB gains (as measured).
- **D4** The "analogue" noise is reproduced (it is the documented purpose of the switch); the fixed saturation of §9
  is reproduced.
- **D5** Parameter changes must be click-free (≤ 20 ms smoothing); the reference's asynchronous application of
  parameter changes is not reproduced.
- **D6** Single linked detector for the stereo variant (as measured).

## 11. Test vectors
1. Neutral settings, any input ≤ −30 dBFS: output = input within −100 dB re peak (at −20 dBFS the §9 term alone
   is ≈ −90 dB, so −88 dB applies there).
2. Every Appendix A row at 48 kHz: magnitude within tolerance §12.
3. Compressor: staircase of 1 kHz tones (−51…+3 dBFS, 3 dB steps) for each Appendix B ratio at thresholds +10, 0, −10,
   −20: steady-state gain within ±0.5 dB of M + GR.
4. Attack/release and expander/gate timing tables (§6, §6.6, §7) within ±25 %; §6.5 and §7.1 static tables ±0.5 dB.
5. Polarity: exact inversion. Output fader / input trim: exact dB.
6. Analogue On, silent input: −110.7 ± 1.5 dBFS rms noise per channel.

## 12. Tolerances
- EQ/filter magnitude, at every sample rate (bands in absolute frequency): ±0.5 dB from 20 Hz to 12 kHz;
  ±1.5 dB from 12 kHz to 20 kHz (±3 dB for the low-pass). Above 20 kHz not normative.
  HF band with frequency control ≥ 8 kHz at sample rates other than 48 kHz: ±1.5 dB everywhere.
- Phase is not normative (minimum-phase sections expected).
- Static dynamics: ±0.5 dB; timing ±25 %.
- Saturation: H3 within ±3 dB of §9 at 0 and −6 dBFS.

## 13. User interface (functional layout; original visuals required)
Two side-by-side sections plus I/O:
- **Dynamics section:** compressor (ratio, threshold, release, fast-attack switch) with a gain-reduction meter;
  expander/gate (range, threshold, release, fast-attack switch, expander/gate switch) with its own activity meter;
  routing buttons: dynamics bypass, channel-out (post-EQ).
- **EQ section** from top: high-pass and low-pass knobs (fully counter-clockwise = off) with split button;
  HF (gain, frequency, bell switch); HMF (gain, frequency, Q); LMF (gain, frequency, Q); LF (gain, frequency, bell
  switch); EQ bypass and EQ-to-sidechain buttons.
- **I/O:** input trim, polarity button, analogue on/off, output fader, input/output meter selector and level meter.
- Knob values shown with the display resolutions of §2. Visual design (colours, knob caps, fonts, panel artwork) must be
  the implementer's own; do not imitate any console's appearance or colour coding.

## Changes from r1
| § | Change | Reason (verification against the reference) |
|---|---|---|
| 2 | Five-knot tapers replaced by the exact 21-point tapers; defaults now display exactly; Appendix A keyed to them | r1 knots wrong for frequency, Q, release, range, HP/LP labels (e.g. Q 0.50 is at r = 0.90, not 0.80) — same knob position gave a different band in the build (up to 6.5 dB response error) |
| 3 | Gain staging note: no output limiting | open question from the build's stress test |
| 4 | LP f/Q pairs informative; Appendix A.2 normative; 20 kHz losses corrected | r1 text contradicted A.2 (r = 0.3 loses 7.7 dB at 20 kHz) |
| 6.5 | New: steady-state reduction depends on attack mode and release | reference: up to −3.0 dB at fast/4 s vs slow/0.4 s; absent in r1 |
| 6.6 | New: attack timing vs release | fast attack slows to 1.5/5/11 ms at 4 s release |
| 7.1 | Static expander/gate table replaced (r1 list offset by +9 dB) | measured staircases |
| 9 | H2 informative | present at −94 dBc in the reference, inaudible |
| 11 | TV1 at ≤ −30 dBFS; TV4 extended | r1 TV1 contradicted §9 |
| 12 | Tolerance bands in absolute frequency at all rates | r1 wording made 96 kHz stricter than 48 kHz |

## 14. Open questions
1. Exact mechanism of the program-dependent (slow) attack and release; only tables are specified.
2. Compressor ratios between tabulated values and thresholds below −20: interpolation rule is a specification choice.
3. Expander range floors above 20 dB (expander mode) were not measured directly.
4. Mono variant assumed identical to one channel of the stereo variant (not measured).
5. Exact noise spectrum of the analogue switch (broadband assumed).
6. Dynamics time constants were measured at 48 kHz only (assumed defined in seconds).

## 15. Evidence summary (analyst side)
Measurement at 48 kHz (spot checks 44.1/96 kHz), REAPER offline renders, 1 kHz staircases, tone steps and exponential
sweeps; ≈ 400 configurations. Fit quality of the structural descriptions against the measurements: HP third-order
Butterworth ≤ 0.5 dB; LP two-pole ≤ 0.015 dB (digital fit); LF shelves ≤ 0.02 dB; bells 0.03–0.9 dB (analogue
second-order prototype; residual high-frequency skirts on wide mid settings and near Nyquist on the HF band — hence the
normative tables); compressor static tables reproduce all measured thresholds within 0.2 dB (0.5 dB at ∞:1);
dynamics timing from step responses (±10 % repeatability).
