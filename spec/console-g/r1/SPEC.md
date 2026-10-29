# G-type console channel strip — behavioural specification r1

Status: APPROVED r1 (2026-10-29), frozen. Review decisions in §12. Tables in `tables/` are normative. Functional description only; derived from black-box
measurement of a licensed reference plug-in and its public manual. No vendor/product/brand names are used.
Machine-readable tables live next to this file in `tables/` and are part of the specification.

## 1. Overview and signal flow

Stereo (2→2) and mono (1→1) variants. Per channel, linked dynamics. Fixed processing order:

```
in → polarity → input trim → [baseline transfer curve] →
   default ("dynamics pre-EQ"):   dynamics → filters(HP,LP) → EQ(LF,LMF,HMF,HF) → output gain → out
   "dynamics post-EQ" (Ch Out on): filters → EQ → dynamics → output gain → out
   "filters split" on:             filters are placed before the dynamics (filters → dynamics → EQ)
   "filters to side-chain" on:     filters act only on the dynamics detector signal, not on the audio
```

- Polarity: multiply by −1.
- Input trim: −18…+18 dB, applied before everything gain-dependent (it moves the signal relative to the dynamics
  thresholds: +6.3 dB trim shifts the compressor curve by exactly 6.3 dB).
- Output gain: −24…+12 dB (taper in §2), applied last.
- Level alignment: 0 dB on the threshold scales corresponds to −18 dBFS (sine RMS → see §5).
- Latency: 0 samples (all paths, all settings).
- EQ bypass / dynamics bypass: remove the respective section; the baseline curve (§1.1) remains.

### 1.1 Baseline transfer curve (always active)
Every setting, including all sections bypassed and "analog" off, applies the memoryless curve

  y = x + 0.00316·x³   (x in full-scale units, per sample, per channel)

(measured coefficient 0.00316 ±0.00005 at −30…0 dBFS sine; residual higher harmonics ≤ −100 dBc).
At −30 dBFS the deviation from a bit-exact path is ≤ 1.4·10⁻⁷. Harmonic test vectors (1 kHz sine): H3 =
−98 dBc at −18 dBFS, −74 dBc at −6 dBFS, −62 dBc at 0 dBFS; H2 ≤ −94 dBc.

### 1.2 Analog emulation switch
"Analog" on adds, per channel, uncorrelated white noise at −110.8 dBFS RMS (flat PSD ≈ −154.5 dB/Hz at 48 kHz,
L/R correlation < 0.01), present also on digital silence. The static/frequency behaviour of all sections is otherwise
unchanged (responses at −30 dBFS identical to off within 0.01 dB). Small additional low-level 5th-harmonic content
(≈ −122 dBc at −30 dBFS) is optional.

## 2. Parameters (host-visible), ranges, tapers

Each continuous parameter maps the normalised host value r ∈ [0,1] to the displayed value by the 41-point
piecewise-linear tables in `tables/param_tapers.csv` (r = 0, 0.025, …, 1; reproduce them exactly, interpolate
linearly between points; displayed values are rounded as shown there). The audio behaviour is a function of the
displayed value as given in §3–§7.

| Parameter | Range / values | Default |
|---|---|---|
| Comp threshold | +10 … −20 dB | 0 dB |
| Comp ratio | 1 … ∞ (shown "+Inf") | 1 (= compressor off) |
| Comp fast attack | off/on | off |
| Comp release | 0.1 … 4.0 s (see D1) | 0.4 s |
| Exp/Gate threshold | −30 … +10 dB | −30 dB |
| Exp/Gate range | 0 … 40 dB | 0 dB (= expander off) |
| Exp/Gate mode | Expander / Gate | Expander |
| Exp/Gate fast attack | off/on | off |
| Exp/Gate release | 0.1 … 4.0 s | 0.2 s |
| Dynamics bypass, Ch Out (dynamics post-EQ), Filters to side-chain, Filter split | switches | off |
| EQ bypass | switch | off |
| HP freq | Off, 16 … 350 Hz (display) | Off (r = 0) |
| LP freq | Off, 20.8 … 3.0 kHz (display; decreasing with r) | Off (r = 0) |
| LF gain / HF gain | −17 … +17 dB (display) | 0 dB |
| LF freq | 30 … 450 Hz | 60 Hz |
| HF freq | 1.5 … 16 kHz | 8 kHz |
| LMF / HMF gain | −20 … +20 dB | 0 dB |
| LMF / HMF Q | 3.5 … 0.1 (r = 0 → 3.5) | LMF 2.5, HMF 1.5 |
| LMF freq | 0.2 … 2.5 kHz | 0.5 kHz |
| HMF freq | 0.6 … 7.0 kHz | 3.5 kHz |
| LMF ÷3 / HMF ×3 | switches | off |
| Input trim | −18 … +18 dB | 0 dB |
| Output gain | −24 … +12 dB | 0 dB |
| Analog, Polarity (+/−), Meter source (In/Out, display only) | switches | off / + / Out |

## 3. Filters (HP, LP)

### 3.1 High-pass (18 dB/oct)
3rd-order high-pass. The digital filter is the bilinear transform (no frequency prewarping) at the actual sample rate
of a fixed analog prototype H_HP(s) = (B3 s³ + B2 s² + B1 s + B0)/(s³ + A2 s² + A1 s + A0) (s in rad/s), with
prototype coefficients per display value in `tables/hp.csv` (24 display values; all zeros at s = 0; for display
values between table rows interpolate the complex pole pair as (ln ω, ln ζ) and the real pole as ln ω, linearly in
log₁₀ of the display value). Note the display value is a label, not the −3 dB point:

| display | −3 dB point (48 kHz) |
|---|---|
| 16 Hz | 15.9 Hz |
| 20 Hz | 17.9 Hz |
| 70 Hz | 32.0 Hz |
| 120 Hz | 103 Hz |
| 200 Hz | 147 Hz |
| 350 Hz | 415 Hz |

Asymptotic slope 18 dB/octave; pole pair Q ≈ 0.87 with an additional real pole. The same prototype is used at every
sample rate (verified 44.1/48/96 kHz: ≤ 0.2 dB).

### 3.2 Low-pass (12 dB/oct)
Low-pass whose magnitude is defined by the target table `tables/lp_target.csv` (magnitude in dB at 1/6-octave
frequencies 20 Hz…20 kHz, per host value r = k/40, measured at 96 kHz where the response is closest to the analog
intent; interpolate linearly in r and in log f) — any 2nd-order (or higher)
digital design that meets the table within tolerance (§10) is acceptable; a magnitude-matched biquad design
(e.g. Vicanek, "Matched Second Order Digital Filters", 2016) is recommended. −3 dB points (96 kHz): display 20.8 k →
38.7 kHz, 12.0 k → 24.4 kHz, 10.0 k → 16.7 kHz, 8.0 k → 9.3 kHz, 6.0 k → 6.9 kHz, 5.0 k → 5.7 kHz, 4.0 k → 4.1 kHz.

## 4. Equaliser bands

All four bands are second-order sections in series. LF, HF and LMF are bilinear transforms (no prewarp) of fixed
analog prototypes; HMF is magnitude-matched to an analog peaking target. All responses are stated at −30 dBFS input.

### 4.1 LF shelf, HF shelf
Prototype H(s) = (B2 s² + B1 s + B0)/(s² + A1 s + A0) (s in rad/s), digitised by the bilinear transform without
prewarping at the actual sample rate. Prototype coefficients at the measured grid points (display gain × display
frequency) are in `tables/lf_shelf.csv`, `tables/hf_shelf.csv` (exact at the grid points: fit error ≤ 0.003 dB).
At 0 dB gain the band is exactly flat. Between grid points, interpolate in the representation
q = (ln B2, ln ω_z, ln ζ_z, ln ω_p, ln ζ_p) with ω_z = √(B0/B2), ζ_z = B1/(2·B2·ω_z), ω_p = √A0, ζ_p = A1/(2ω_p)
(continuous across real/complex root changes): monotone-cubic (PCHIP) along display gain (including the flat point
0 dB, q_flat = (0, ln ω_p, ln ζ_p, ln ω_p, ln ζ_p) of the nearest non-zero gain), linear along log₁₀(frequency)
between frequency rows. Gain grid: 16 display gains at LF 50/100/300 Hz and HF 3/8/16 kHz, 6 gains at the other
frequency rows. Measured interpolation error vs off-grid reference settings: ≤ 0.05 dB on dense rows,
≤ 1.1 dB elsewhere (§10).
Notes: displayed gain is not the realised gain (e.g. LF at 100 Hz: display −17/−8.5/−4.2/+4.3/+8.5/+17 dB →
DC gain −19.2/−5.4/−2.3/+2.8/+8.7/+18.5 dB); the shelves show counter-overshoot on the far side (LF +15 @ 60 Hz:
−0.59 dB at 2 kHz). Verified rule: identical prototype at every rate (44.1/48/96 kHz ≤ 0.01 dB).

### 4.2 LMF bell
Analog peaking bell H(s) = (s² + s·ω0·A/Q_e + ω0²)/(s² + s·ω0/(A·Q_e) + ω0²), A = 10^(G_e/40), digitised by the
bilinear transform without prewarping (this reproduces the reference at all rates; the exact reference prototype
differs from this bell form by ≤ 0.26 dB, ≤ 0.5 dB at ±20 dB / Q 0.1). Effective parameters (ω0 = 2π·f0_eff,
Q_e, G_e) per measured point in `tables/lmf_bell.csv` (gain × Q at 500 Hz; frequency series at Q 1.5, ±12 dB;
÷3 cases). Interpolation: G_e and Q_e by PCHIP along display gain (G_e = 0 at 0 dB) and along the host value of Q;
f0_eff scales with display frequency per the frequency series (PCHIP in log f), with the gain-dependent factor
f0_eff(g, 500)/f0_eff(±12, 500). ÷3: use display frequency / 3 in the frequency mapping (500 ÷ 3 → 119 Hz effective
at +12 dB). Characteristic values: Q 0.1/0.5/1.0/1.5/2.5/3.5 → Q_e ≈ 0.19–0.22/0.23–0.26/0.30–0.34/0.42–0.54/
0.50–0.57/0.53–0.78 (rising with |gain|); display ±4/±6/±10/±12/±20 dB → G_e ≈ +5.7 −3.0 / +7.6 −5.0 / +12.7 −8.8 /
+15.3 −10.3 / +21.3 −21.0 dB; centre 500 Hz display → 460 (|g| 6) … 527 Hz (|g| 20); 2.5 kHz → 1.81 kHz.

### 4.3 HMF bell
Magnitude target = analog peaking bell |(s² + s·w0·A/Q_e + w0²)/(s² + s·w0/(A·Q_e) + w0²)|, A = 10^(G_e/40), with
effective parameters per grid point in `tables/hmf_bell_target.csv` (fit error ≤ 0.26 dB, 20 Hz…20 kHz).
Effective parameters (examples): Q 0.1 → Q_e 0.20–0.22, Q 1.5 → 0.44–0.53, Q 3.5 → 0.57–0.75 (rising with |gain|);
display gain ±3/±6/±12/±20 dB → G_e ≈ ±2.2/±5.3/±10.5…+14.5/±21.3 dB (boost slightly larger than cut); centre at
display 3.5 kHz → 3.05 kHz (|G| ≤ 6) … 3.65 kHz (|G| = 20); display frequency mapping 0.6 k → 646 Hz,
1.5 k → 1.50 kHz, 2.5 k → 2.45 kHz, 3.5 k → 3.24 kHz, 5 k → 4.21 kHz (at −12 dB). ×3 multiplies the display frequency
by 3 (3.5 k ×3 → effective ≈ 10.7 kHz). Digitise with a magnitude-matched design (as §3.2); the reference keeps the
response nearly sample-rate independent up to 20 kHz (44.1 vs 96 kHz ≤ 0.4 dB).

### 4.4 EQ test vectors (48 kHz, magnitude dB at the listed frequencies)
See §9.1.

## 5. Compressor

- Stereo-linked: one detector drives the same gain on both channels (a steady signal on R receives the reduction
  caused by L).
- Detector: peak-sensitive (white noise receives 0.7…1.4 dB more reduction than a sine of equal RMS, see §9.3).
- Static behaviour (1 kHz sine, default timing): `tables/comp_static.csv` gives output gain (dB, including make-up)
  vs input level −54…0 dBFS for thresholds +10/+5/0/−5/−10/−15/−20 dB × ratios 1.5/2/3/4/6/10/20/∞. Interpolate
  linearly in threshold and in 1−1/ratio. Key characteristics:
  - Wide soft knee (≈ 3–4 dB knee width in the detector domain) and an effective slope above threshold of
    ≈ 0.65·(1−1/R)… 0.97 (R=∞) of the nominal; e.g. T −10 dB, R 4: −0.9 dB at −22 dBFS, −5.3 at −14, −10.4 at −6
    (incl. make-up +3.0).
  - Threshold reference: T dB ↔ T − 18 dBFS (sine RMS).
  - Automatic make-up gain (applied at all levels) depends on threshold and ratio: e.g. R 2: +0.9/+1.9/+4.3 dB,
    R 4: +1.5/+3.0/+6.1 dB, R ∞: +2.4/+4.8/+8.7 dB at T = 0/−10/−20 dB. Full table: first column of
    `tables/comp_static.csv` (input −54 dBFS = below threshold for all T).
- Timing (1 kHz bursts −40 → −4 → −40 dBFS, T −10 dB, R 4):

| setting | attack 50/63/90 % | release 50/63/90 % |
|---|---|---|
| auto attack, release 0.4 s | 31/45/85 ms | 87/126/334 ms |
| auto attack, release 0.1 s | 34/48/94 ms | 61/87/220 ms |
| auto attack, release 1.2 s | 34/47/93 ms | 201/345/1197 ms |
| fast attack, release 0.4 s | 1.5/2.5/4.5 ms | 43/66/268 ms |
| fast attack, release 0.1 s | 1.5/2.5/4.5 ms | 22/30/72 ms |

  "Auto" attack is program dependent and slower for larger reductions (fitted one-pole attack constant ≈ 33 ms at
  T 0 dB / R 2 rising to ≈ 190 ms at T −20 dB / R ∞); release is likewise program dependent (shorter for deeper
  reduction). The static sine gain depends slightly on timing (detector ripple): fast attack gives ≈ 2.3 dB more
  reduction at −2 dBFS than auto (T −10, R 4), release 4.0 s ≈ 1.5 dB more than 0.4 s.
- Gain-modulation distortion (1 kHz, T −10, R 4, 48 kHz): H3 −86/−73/−70/−69 dBc at −30/−18/−6/0 dBFS (auto attack),
  −72/−57/−52/−51 dBc (fast attack). This arises from detector ripple; implementations reproduce it within ±6 dB.
- An informative model that reproduces the static tables within ≈ 0.3 dB RMS (per-setting parameters):
  peak detector env ← env + α(|x|−env) (α from attack/release time constants), level L = 20·log10(env) + d,
  x = L − (T − 18), reduction = −s·W·ln(1 + e^{x/W}); see `tables/comp_model_fit.csv`.

## 6. Expander / gate

Shares the gain element with the compressor (gains add in dB); same linked detector. Threshold reference
T dB ↔ T − 18 dBFS. Static data (1 kHz sine, up and down staircases): `tables/exp_static.csv`.
- Expander: below threshold, downward expansion of slope ≈ 1.2 dB per dB (≈ 1:2.2, e.g. T −20 dB, range 40:
  −2.8/−7.6/−12.4/−17.0 dB at 4/8/12/16 dB below −38 dBFS), limited to a floor that depends non-linearly on the
  range control: range 5 → −0.8 dB, range 10 → −7.7 dB, range 40 → at least −35 dB (slope-limited in all measured
  cases). Intermediate ranges: interpolate the floor in dB linearly between measured points (range 20/30 rows of
  the table when present). No hysteresis.
- Gate: binary open/attenuated with a floor of −25.4 dB (range 20) and −52.7 dB (range 40) (other ranges: linear
  interpolation of the floor in dB vs range). Opens on rising level ≈ 2 dB (T −20) to 4 dB (T −30) above the
  threshold reference, closes on falling level ≈ 3 dB below it (hysteresis ≈ 5–7 dB).
- Timing: release 0.1…4 s (release 4.0 s visibly slows recovery on the staircase); fast attack switch as compressor.
- Combined compressor + expander (T −10/R 4 with T −20/range 20) sums both gains (§9.2 row "cg").

## 7. Routing tests (§1)
- Default: dynamics pre-EQ (LF +11.9 dB @ 450 Hz shelf adds +5.9 dB at 1 kHz after the compressor: compressor
  curve unchanged, output +5.9 dB).
- Ch Out on: EQ before dynamics (the same shelf moves the compressor curve by 5.9 dB).
- Filters to side-chain (HP 350): audio unfiltered; detector filtered (1 kHz tone: compressor curve within 0.2 dB).
- Split (HP 350): filters before dynamics.

## 8. Sample rate
LF, HF, LMF, HP: fixed analog prototypes, bilinear transform at the actual rate (exact behaviour of the reference).
HMF, LP: magnitude target as specified (rate independent). Dynamics time constants in seconds; baseline curve and
analog noise level rate independent.

## 9. Test vectors

### 9.1 EQ / filter magnitude (48 kHz, −30 dBFS, dB)
| setting | 30 | 60 | 100 | 200 | 500 | 1k | 2k | 3.5k | 5k | 8k | 12k | 16k | 20k |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| LF +15.0 dB @ 60 Hz | +16.32 | +14.43 | +11.85 | +7.30 | +1.95 | -0.07 | -0.59 | -0.47 | -0.31 | -0.13 | -0.05 | -0.02 | -0.00 |
| LF -15.0 dB @ 60 Hz | -12.98 | -11.62 | -9.57 | -5.65 | -1.19 | +0.25 | +0.51 | +0.35 | +0.21 | +0.09 | +0.03 | +0.01 | +0.00 |
| LF +11.9 dB @ 200 Hz | +13.82 | +13.62 | +13.17 | +11.53 | +6.70 | +2.23 | -0.66 | -1.00 | -0.71 | -0.32 | -0.12 | -0.04 | -0.01 |
| LF -11.9 dB @ 450 Hz | -8.62 | -8.56 | -8.43 | -7.87 | -5.43 | -2.41 | -0.15 | +0.51 | +0.52 | +0.32 | +0.14 | +0.05 | +0.01 |
| HF +15.0 dB @ 8 kHz | -0.01 | -0.03 | -0.09 | -0.33 | -1.48 | -1.18 | +4.03 | +8.85 | +11.62 | +14.66 | +16.54 | +17.40 | +17.78 |
| HF -15.0 dB @ 8 kHz | +0.01 | +0.03 | +0.08 | +0.31 | +1.32 | +0.74 | -4.09 | -8.18 | -10.20 | -12.00 | -12.86 | -13.18 | -13.32 |
| HF +11.9 dB @ 1.5 kHz | -0.21 | -0.74 | -1.55 | -0.80 | +6.67 | +11.26 | +13.69 | +14.47 | +14.69 | +14.82 | +14.86 | +14.88 | +14.88 |
| HF -11.9 dB @ 16 kHz | +0.00 | +0.01 | +0.02 | +0.06 | +0.36 | +0.98 | +0.20 | -2.99 | -4.84 | -6.39 | -7.06 | -7.31 | -7.40 |
| LMF +12 dB, Q 1.0, 0.5 kHz | +0.93 | +2.92 | +5.64 | +10.62 | +15.27 | +11.74 | +6.61 | +3.34 | +1.93 | +0.82 | +0.36 | +0.19 | +0.12 |
| LMF -12 dB, Q 3.5, 0.5 kHz | -0.11 | -0.43 | -1.15 | -3.84 | -10.22 | -4.87 | -1.51 | -0.52 | -0.26 | -0.10 | -0.04 | -0.02 | -0.01 |
| LMF +20 dB, Q 1.5, 0.5 kHz | +0.61 | +2.06 | +4.40 | +9.76 | +21.11 | +13.32 | +6.57 | +3.14 | +1.77 | +0.74 | +0.32 | +0.17 | +0.11 |
| LMF +12 dB, Q 1.5, 2.5 kHz | +0.03 | +0.12 | +0.31 | +1.15 | +4.82 | +10.45 | +14.42 | +9.61 | +6.51 | +3.42 | +1.69 | +0.96 | +0.63 |
| LMF +12 dB, Q 1.5, 0.5 kHz ÷3 | +4.30 | +9.72 | +14.68 | +11.39 | +3.97 | +1.32 | +0.36 | +0.12 | +0.06 | +0.02 | +0.01 | +0.00 | +0.00 |
| HMF +12 dB, Q 1.0, 3.5 kHz | +0.02 | +0.08 | +0.21 | +0.80 | +3.53 | +7.77 | +12.73 | +14.48 | +13.13 | +9.71 | +6.49 | +4.42 | +3.23 |
| HMF -12 dB, Q 0.5, 3.5 kHz | -0.02 | -0.08 | -0.22 | -0.82 | -3.46 | -7.03 | -10.09 | -10.83 | -10.28 | -8.39 | -6.01 | -4.23 | -3.12 |
| HMF +12 dB, Q 1.5, 7 kHz | +0.00 | +0.01 | +0.04 | +0.14 | +0.83 | +2.69 | +6.72 | +11.38 | +13.72 | +12.00 | +8.13 | +5.54 | +4.02 |
| HMF +12 dB, Q 0.1, 3.5 kHz | +0.05 | +0.21 | +0.55 | +1.87 | +6.22 | +10.67 | +14.10 | +14.90 | +14.28 | +12.17 | +9.41 | +7.17 | +5.65 |
| HMF +12 dB, Q 1.5, 0.6 kHz ×3 | +0.03 | +0.11 | +0.29 | +1.07 | +4.56 | +10.00 | +14.32 | +9.88 | +6.75 | +3.58 | +1.79 | +1.01 | +0.67 |
| HP 70 (display) | -3.89 | -0.10 | -0.00 | -0.00 | +0.00 | +0.00 | +0.00 | +0.00 | +0.00 | +0.00 | +0.00 | +0.00 | +0.00 |
| HP 120 | -32.11 | -14.26 | -3.41 | -0.08 | -0.00 | +0.00 | +0.00 | +0.00 | +0.00 | +0.00 | +0.00 | +0.00 | +0.00 |
| HP 350 | -68.42 | -50.41 | -37.09 | -19.07 | -1.23 | -0.02 | -0.00 | +0.00 | +0.00 | +0.00 | +0.00 | +0.00 | +0.00 |
| LP 3.2 kHz | -0.00 | -0.00 | -0.00 | -0.00 | -0.03 | -0.14 | -0.79 | -3.49 | -7.55 | -15.46 | -24.06 | -30.81 | -34.15 |
| LP 8.0 kHz | +0.00 | -0.00 | -0.00 | -0.00 | -0.00 | -0.02 | -0.07 | -0.23 | -0.50 | -1.54 | -4.47 | -9.28 | -13.94 |
| LP 12.0 kHz | +0.00 | +0.00 | +0.00 | -0.00 | -0.00 | -0.00 | -0.00 | -0.01 | -0.02 | -0.07 | -0.21 | -0.62 | -1.98 |
| combination (see §9.1) | -1.30 | +0.36 | -2.10 | -5.49 | -5.54 | -2.29 | +0.75 | +1.48 | +0.64 | -0.93 | -2.06 | -2.91 | -4.45 |

Combination row: LF +6.0 dB @ 60 Hz, HF −6.0 dB @ 8 kHz, LMF −8 dB 0.30 kHz Q 0.75, HMF +5 dB 3.0 kHz Q 2.0,
HP 70, LP 12.0 k.

### 9.2 Dynamics static (1 kHz sine, output gain dB vs input dBFS)
| setting | -54 dBFS | -30 dBFS | -22 dBFS | -14 dBFS | -6 dBFS | 0 dBFS |
|---|---|---|---|---|---|---|
| comp T -20 dB, R 2.0 | +4.35 | +2.86 | +0.59 | -2.15 | -5.25 | -7.80 |
| comp T -20 dB, R 6.0 | +6.69 | +1.40 | -3.68 | -9.38 | -15.39 | -19.78 |
| comp T -20 dB, R +Inf | +8.74 | +0.05 | -7.06 | -14.53 | -22.46 | -28.43 |
| comp T -10 dB, R 1.5 | +1.10 | +1.10 | +0.85 | +0.21 | -0.76 | -1.66 |
| comp T -10 dB, R 4.0 | +2.98 | +2.29 | -0.90 | -5.25 | -10.36 | -14.17 |
| comp T -10 dB, R +Inf | +4.77 | +2.34 | -3.71 | -10.80 | -18.49 | -24.70 |
| comp T -5 dB, R 10.0 | +2.75 | +2.62 | -0.41 | -5.50 | -11.40 | -16.22 |
| comp T +0 dB, R 2.0 | +0.93 | +0.93 | +0.89 | -0.18 | -2.32 | -4.22 |
| comp T +0 dB, R +Inf | +2.38 | +2.38 | +0.35 | -5.47 | -12.72 | -18.69 |
| comp T +10 dB, R 4.0 | +0.00 | +0.00 | +0.00 | -0.32 | -3.12 | -6.24 |
| comp T −10, R 4, fast attack | +2.98 | +1.84 | -2.15 | -7.10 | -12.37 | -16.51 |
| comp T −10, R 4, release 4.0 s | +2.98 | +2.14 | -1.56 | -6.32 | -11.61 | -15.66 |
| expander T −20, range 40 | -17.04 | +0.00 | +0.00 | +0.00 | +0.01 | +0.02 |
| expander T 0, range 10 | -3.51 | -6.94 | +0.00 | +0.00 | +0.01 | +0.02 |
| gate T −20, range 40 | -33.90 | +0.00 | +0.00 | +0.00 | +0.01 | +0.02 |
| comp T −10/R 4 + expander T −20/range 20 | -10.05 | +2.29 | -0.90 | -5.25 | -10.36 | -14.17 |
| comp T −10/R 4, post-EQ, LF +11.9 @ 450 Hz | +8.90 | +5.97 | +1.82 | -2.94 | -8.26 | -12.30 |
| comp T −10/R 4, input trim +6.3 dB | +9.28 | +6.17 | +1.98 | -2.93 | -8.08 | -12.16 |

Values: output/input gain in dB (incl. make-up) on a rising 1 kHz staircase, 2 dB steps of 0.3 s, measured over the last 0.1 s of each step; the −54 dBFS column is the first step after 0.3 s of silence.

### 9.3 Detector / link
- White noise −20 dBFS RMS vs 1 kHz sine −20 dBFS RMS (T −10, R 4): gain −4.28 vs −3.62 dB (auto), −6.15 vs −5.20 dB
  (fast); T −20, R ∞: −13.18 vs −11.82 dB.
- Link: L bursts to −4 dBFS, R steady 2.5 kHz −40 dBFS: R receives the same −11.57 dB (T −10, R 4).

## 10. Tolerances
- LF/HF/HP: ≤ 0.1 dB vs the table prototypes at grid points; ≤ 1.0 dB at off-grid settings vs the §9.1 vectors
  (20 Hz…0.45·fs). LMF: ≤ 0.5 dB at table points, ≤ 1.0 dB off-grid.
- HMF, LP: ≤ 0.5 dB up to 15 kHz, ≤ 1.5 dB 15–20 kHz vs target; above 20 kHz unconstrained.
- Compressor/expander static tables: ≤ 0.5 dB (≤ 1.0 dB within ±3 dB of the knee); timing quantities ±25 %.
- Baseline curve coefficient ±2 %; analog noise level ±0.5 dB.
- Latency 0 samples; L/R identical processing.

## 11. User interface (functional layout; visuals must be original)
Single window, left-to-right: (1) dynamics column — compressor (ratio, threshold, release knobs; fast-attack switch;
gain-reduction meter), expander/gate (range, threshold, release; fast-attack and gate/expander switches);
dynamics routing switches (bypass, post-EQ, filters to side-chain, split); (2) filter section (HP, LP knobs; each
fully counter-clockwise = off); (3) EQ — HF (gain, freq), HMF (gain, freq, Q, ×3), LMF (gain, freq, Q, ÷3),
LF (gain, freq); EQ bypass; (4) master — input trim, output fader, analog switch, polarity switch, input/output
meter with selector (meter is display-only). Colour coding of knob groups and all artwork are the implementer's own
(no imitation of any console's appearance). Double-click resets a control to its default.

## 12. Review decisions (standing policy: match the reference; don't reproduce clear defects; original visuals; own naming)
- D1: Release controls are labelled in milliseconds by the reference but measure as seconds (0.1–4 s, as its manual
  states). Implement and label in seconds (defect not reproduced).
- D2: HP/LP/EQ frequency displays are labels that differ from the actual corner/centre frequencies; the spec follows
  the measured behaviour (tables) — the display labels may be kept as in §2 or the implementer may show the effective
  value; the audio must match the tables.
- D3: Baseline cubic curve and the analog noise are reproduced (they are the documented "non-flat bypass" and
  analog emulation).
- D4: HMF and LP use magnitude-matched digitisation (as the reference does); above 20 kHz no requirement.
- D5: Mono variant assumed identical per channel (not measured).
- D6: Parameter changes must be click-free (smoothing ≤ 20 ms); the reference's own smoothing was not measured.
- D7: In one long automated run the reference fell permanently silent (all-zero output, also for later neutral
  settings) after a sequence of expander settings; with the instance reset between settings it never happened.
  Treated as a defect: the implementation must never latch into silence; the expander floor is always finite.
- D8: Displayed EQ/filter values are labels of the reference's control positions, not exact physical frequencies
  or gains; the spec keeps the reference's display (§2) and defines the audio through the measured tables.

## 13. Open questions
1. Exact compressor/expander gain laws (tables given; parametric model informative only).
2. Program-dependent attack/release laws beyond the tabulated cases.
3. HMF/LP exact digitisation method.
4. Expander/gate hysteresis width (only the staircase evidence in the tables).
5. Mono variant and > 96 kHz behaviour.
