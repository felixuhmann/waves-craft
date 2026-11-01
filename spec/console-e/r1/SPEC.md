# E-type console channel strip — behavioural specification r1

Status: APPROVED r1 (2026-10-29, review gate: standing policy — match the reference, do not reproduce defects, original visuals, own naming; decisions §10). Frozen. Derived from black-box measurement of a licensed reference plug-in and from its
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

Host-normalised value r ∈ [0,1]. Every continuous control uses a **piecewise-linear taper with knots at
r = 0, 0.2, 0.5, 0.8, 1** (values between knots are linear in r). Display resolution in brackets.

| Control | r=0 | r=0.2 | r=0.5 | r=0.8 | r=1 | Default | Display |
|---|---|---|---|---|---|---|---|
| Input trim (dB) | −18 | −10.8 | 0 | +10.8 | +18 | 0 | 0.1 dB |
| Output fader (dB) | −24 | −16.8 | −6 | +4.8 | +12 | 0 (r=0.667) | 0.1 dB |
| Comp threshold (dB) | +10 | +4 | −5 | −14 | −20 | 0 (r≈0.333) | 0.1 dB |
| Comp ratio | 1 | 2.0 | 5.0 | 20 | ∞ | 1 | 0.1 (see note) |
| Comp release (s) | 0.1 | 0.15 | 0.4 | 2.0 | 4.0 | 0.4 | 0.01 |
| Exp threshold (dB) | −30 | −25 | −12 | +5 | +10 | −30 | 0.1 dB |
| Exp range (dB) | 0 | 5 | 20 | 33.3 | 40 | 0 | 0.1 dB |
| Exp release (s) | 0.1 | 0.15 | 0.4 | 2.0 | 4.0 | 0.2 (r=0.35) | 0.01 |
| HP frequency (Hz, label) | Off | 20 | 120 | 300 | 350 | Off | 1 Hz |
| LP frequency (kHz, label) | Off | 12.0 | 5.0 | 3.5 | 3.0 | Off | 0.1 kHz |
| LF gain / LMF gain / HMF gain / HF gain (dB, label) | −15 | −9 | 0 | +9 | +15 | 0 | 0.1 dB |
| LF frequency (Hz, label) | 30 | 50 | 200 | 300 | 450 | 60 (r=0.23) | 1 Hz |
| LMF frequency (kHz, label) | 0.20 | 0.30 | 1.00 | 2.00 | 2.50 | 0.50 (r=0.26) | 0.01 kHz |
| HMF frequency (kHz, label) | 0.60 | 0.80 | 3.00 | 6.00 | 7.00 | 3.50 (r=0.55) | 0.01 kHz |
| HF frequency (kHz, label) | 1.5 | 2.0 | 8.0 | 14.0 | 16.0 | 8.0 | 0.01 kHz |
| LMF Q / HMF Q | 3.5 | 2.5 | 1.5 | 0.5 | 0.1 | 2.5 / 1.5 | 0.01 |

Ratio control: the taper is a lookup of discrete-feeling steps; the 21 values at r = k/20 are
1.0, 1.3, 1.5, 1.8, 2.0, 2.3, 2.7, 3.0, 3.7, 4.3, 5.0, 6.0, 7.0, 8.0, 12, 16, 20, 40, 60, 80, ∞ (linear in between).
HP: r ∈ (0, 0.2] covers labels 17–20 Hz, r = 0 is Off. LP: r = 0 is Off.

Switches (all default off/normal): analogue on/off; compressor fast attack; expander fast attack; expander/gate mode
(default expander); EQ bypass; EQ to dynamics sidechain; dynamics bypass; dynamics after EQ ("channel out");
filters split (filters before dynamics); polarity invert; meter input/output (display only).

**Labels vs. behaviour.** The frequency labels of the filters and EQ bands are nominal. The actual response for each
control position is defined by the tables in Appendix A (derived band centre frequencies in A.17). Implementations
must reproduce the measured responses, not the labels.

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

  r = 0.1–0.3 put the corner at 19.6–22.3 kHz (≤ 2.7 dB loss at 20 kHz, Appendix A.2).
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

### 7.1 Static expander points (threshold −20, range 40; gain dB vs input dBFS)
−45: −16.9, −42: −13.4, −39: −9.9, −36: −6.3, −33: −2.6, −30 and above: 0.

## 8. Routing (verified behaviour)

With ratio 4, threshold −10 and LF +12 dB at 100 Hz (−6 dBFS tones), gain at 50 Hz: default (dynamics → EQ)
+3.2 dB; channel out (EQ → dynamics) −4.9 dB; EQ in sidechain −17.6 dB (no audible boost, more reduction).
Split with HP at r=1: the filter acts before the dynamics (50 Hz: −52.2 dB vs −64.2 dB unsplit).

## 9. Fixed saturation and "analogue" switch

- A fixed, very mild odd-order expansion acts on the signal after the input trim in all modes:
  y ≈ x + 0.0032·x³ (x full-scale = 1): at 0 dBFS the gain is +0.021 dB, H3 −62 dBc, H2 ≈ −94 dBc;
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
1. Neutral settings, any input ≤ −20 dBFS: output = input within −100 dB re peak.
2. Every Appendix A row at 48 kHz: magnitude within tolerance §12.
3. Compressor: staircase of 1 kHz tones (−51…+3 dBFS, 3 dB steps) for each Appendix B ratio at thresholds +10, 0, −10,
   −20: steady-state gain within ±0.5 dB of M + GR.
4. Attack/release and expander/gate timing tables within ±25 %.
5. Polarity: exact inversion. Output fader / input trim: exact dB.
6. Analogue On, silent input: −110.7 ± 1.5 dBFS rms noise per channel.

## 12. Tolerances
- EQ/filter magnitude: ±0.5 dB from 20 Hz to 0.25·fs; ±1.5 dB from 0.25·fs to 20 kHz (±3 dB for the low-pass).
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
