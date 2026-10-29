# Hybrid-style compressor — behavioural specification (candidate r1)

Status: APPROVED r1 (2026-10-29), frozen. Review decisions in §15. Derived only from black-box measurement of the licensed reference in a
VST3 host with user-generated test signals, host-visible parameter metadata and the reference's licensed manual
(paraphrased). No vendor code, assets or names. Product name, parameter IDs and visuals are the implementer's own.

## 1. Scope
Stereo-linked feed-forward compressor with automatic makeup, transient "punch", tempo-synced release, parallel mix,
output trim, four analog colour modes and an output peak stage. Variants: **Stereo** (2→2, one linked detector) and
**Mono** (1→1, same engine on one channel; not measured — assumed identical).

## 2. Parameters (host-normalised value r ∈ [0,1])
| Parameter | Mapping | Range | Default | Display |
|---|---|---|---|---|
| Threshold | T = −48·r dB | 0 … −48 dB | −18 dB | 0.1 dB |
| Ratio | table R(r) below (monotone; interpolate log R linearly in r between the 41 points) | 1 … 50 | 3.00 | 2 decimals |
| Mix | m = 100·r % | 0 … 100 | 100 | 0.1 |
| Attack | A = 0.5·200^r ms | 0.5 … 100 ms | 7.00 ms | 3 significant digits |
| Punch | P = 30·r (unit: ms of sidechain delay, §7.3) | 0 … 30 | 1.0 | 0.1 |
| Release | Rel = 3·1000^r ms | 3 … 3000 ms | 100 ms | 3 significant digits |
| Release sync | 3 states: BPM (r < 0.25), Host (0.25 ≤ r < 0.75), ms (r ≥ 0.75) | | ms | |
| Release note (sync modes) | note table below | 1/64T … 2 bars | 1/32D | |
| Analog | Off (r < 0.125), Mode 1 (< 0.375), Mode 2 (< 0.625), Mode 3 (< 0.875), Mode 4 | | Mode 2 | |
| Output | O = −18 + 36·r dB | ±18 dB | 0.0 dB | 0.1 dB |
| Limiter | Off (r < 0.5) = "clipper", On = "limiter" (§11) | | Off | |
| Meter select | In / GR / Out (display only, no audio effect) | | GR | |
| Manual tempo (BPM sync mode) | 30 … 300 BPM or Off (Off → host tempo is used) | | Off | |
| Bypass | host bypass | | off | |

Ratio table R(r) at r = k/40, k = 0…40:
1.00 | 1.12 | 1.24 | 1.39 | 1.55 | 1.72 | 1.92 | 2.13 | 2.37 | 2.63 | 2.92 | 3.24 | 3.59 | 3.97 | 4.40 | 4.87 | 5.38 | 5.95 | 6.57 | 7.25 | 8.00 | 8.82 | 9.72 | 10.70 | 11.77 | 12.95 | 14.23 | 15.63 | 17.15 | 18.81 | 20.63 | 22.60 | 24.74 | 27.08 | 29.61 | 32.36 | 35.35 | 38.59 | 42.09 | 45.89 | 50.00

Release note table (sync modes) at r = k/40, k = 0…40 (value used for r ∈ [k/40, (k+1)/40)):
1/64T | 1/64T | 1/64 | 1/64 | 1/64D | 1/64D | 1/32T | 1/32T | 1/32 | 1/32 | 1/32D | 1/32D | 1/16T | 1/16T | 1/16 | 1/16 | 1/16D | 1/16D | 1/8T | 1/8T | 1/8 | 1/8 | 1/8 | 1/8D | 1/8D | 1/4T | 1/4T | 1/4 | 1/4 | 1/4D | 1/4D | 1/2T | 1/2T | 1/2 | 1/2 | 1/2D | 1/2D | 1B | 1B | 2B | 2B
Note durations: 4 beats per bar; "T" = triplet (×2/3), "D" = dotted (×3/2); "1B"/"2B" = 1 / 2 bars. Release time in
sync modes = the note duration at the tempo (e.g. 1/8 at 120 BPM = 250 ms) and then behaves exactly as that ms value.

## 3. Signal flow and latency
Per sample, stereo-linked (one gain for both channels):
1. **Detector**: p[n] = max over k = 0…H of max(|L[n+k]|, |R[n+k]|) (forward sample-peak hold over H+1 samples),
   level D = 20·log10 p (dBFS). H = 64 samples at 44.1/48 kHz (see §12 for other rates). The audio path is delayed by
   H samples; the plug-in reports H samples of latency (host-compensated, so the measured output is time-aligned).
2. **Punch** delays the detector signal by P ms (P = Punch value) relative to the audio (§7.3).
3. **Static curve** (§4) gives the target gain reduction GR(D) ≥ 0 dB.
4. **Gain smoothing** (§7): attack on linear gain, release per §7.2.
5. **Makeup** (§5) is added: compressed signal c = x·10^((−GR_smoothed + Mk)/20).
6. **Mix** (§8): y1 = (1 − m/100)·x + (m/100)·c (linear, sample-aligned).
7. **Output** gain: y2 = y1·10^(O/20) (exact).
8. **Analog mode** (§10) acts on y2 (after Output — its level dependence follows the post-Output level).
9. **Output peak stage** (§11): ceiling 0 dBFS.
**Neutral identity**: Ratio 1, Analog Off, Output 0, Mix any, Punch any → output bit-identical to the input for all
signals below 0 dBFS (measured max |y − x| = 5e-17).

## 4. Static curve
d = D − T (dB above threshold). GR(d) = 0 for d ≤ 0 (except the low-threshold knee rows below), and for d > 0:
**GR(d) = d·(1 − 1/R) + Δ(T, d)**, with the correction Δ (dB, measured with 1 kHz sines, Attack 7 ms, Release 100 ms,
Ratio 4; Δ is ratio-independent within ±0.05 dB for R ≥ 2 at T = −24, see the R rows) — normative, linear
interpolation in d and in T between rows; clamp GR ≥ 0:

| T \ d | -2 | -1 | 0 | 1 | 2 | 3 | 6 | 9 | 12 | 18 | 24 |
|---|---|---|---|---|---|---|---|---|---|---|---|
| -6 | -0.00 | +0.00 | +0.00 | -0.32 | -0.42 | -0.46 | -0.52 | — | — | — | — |
| -12 | — | — | +0.00 | — | — | -0.46 | -0.52 | -0.54 | -0.55 | — | — |
| -24 | +0.00 | +0.00 | +0.01 | -0.26 | -0.35 | -0.41 | -0.48 | -0.52 | -0.54 | -0.56 | -0.57 |
| -36 | — | — | +0.16 | — | — | -0.20 | -0.33 | -0.41 | -0.46 | -0.52 | -0.55 |
| -48 | +0.09 | +0.51 | +1.03 | +0.85 | +0.69 | +0.56 | +0.23 | +0.00 | -0.17 | -0.37 | -0.48 |
| −24, R=1.25 | -0.00 | -0.00 | +0.00 | -0.14 | -0.22 | -0.29 | -0.39 | -0.44 | -0.47 | -0.51 | -0.53 |
| −24, R=1.5 | -0.00 | +0.00 | +0.00 | -0.19 | -0.29 | -0.35 | -0.44 | -0.49 | -0.51 | -0.54 | -0.56 |
| −24, R=2 | +0.00 | -0.00 | +0.01 | -0.23 | -0.33 | -0.39 | -0.47 | -0.51 | -0.53 | -0.56 | -0.57 |
| −24, R=50 | -0.00 | +0.00 | +0.02 | -0.27 | -0.36 | -0.41 | -0.48 | -0.52 | -0.54 | -0.56 | -0.57 |

Combination rule (normative): with Δ₄(T, d) = the Ratio-4 rows above (interpolated linearly in T, clamped to
−6 … −48) and Δ₂₄(d, R) = the T = −24 row for ratio R (R ≥ 2: the R = 4 row; R < 2: interpolate between the
R = 1.25 / 1.5 / 2 rows linearly in 1 − 1/R):
**Δ(T, d, R) = Δ₂₄(d, R) + [Δ₄(T, d) − Δ₄(−24, d)] · (1 − 1/R) / 0.75**.
For d > 24 use the d = 24 value; for d < −2 Δ = 0. The positive values at T = −36/−48 near d = 0 (a small soft knee
starting ≈ 2 dB below threshold) are part of the behaviour and scale with 1 − 1/R.

## 5. Automatic makeup
Mk(T, R, Rel) = k(R) · max(0, −T − 9) · s(Rel)  [dB], applied to the compressed path only (before Mix).
k(R) (interpolate linearly in 1 − 1/R; k(1) = 0):
| R | 1.25 | 1.5 | 2 | 3 | 4 | 8 | 20 | 50 |
|---|---|---|---|---|---|---|---|---|
| k | 0.0108 | 0.1056 | 0.2166 | 0.3191 | 0.3670 | 0.4351 | 0.4736 | 0.4886 |

s(Rel) (Rel = effective release time in ms incl. sync modes; interpolate linearly in log10 Rel; clamp outside):
| Rel (ms) | 3 | 10 | 30 | 100 | 300 | 1000 | 3000 |
|---|---|---|---|---|---|---|---|
| s | 0.2476 | 0.6596 | 0.8730 | 0.9628 | 0.9902 | 1.0000 | 1.0029 |

Examples: T −24, R 4, Rel 100 → +5.30 dB; T −48, R 50, Rel 1000 → +19.05 dB; T ≥ −9 → 0 dB.

## 6. Detector properties (consequences of §3.1; normative test vectors)
- Steady 1 kHz sine: no gain ripple (H3 < −120 dBc at R 4, T −24, any Release).
- Equal-RMS white noise vs 1 kHz sine (both −15 dBFS RMS, R 4, T −24) — extra reduction for noise, dB:
  A0.5: -4.51, A2: -4.11, R4_T-24: -3.62, A30: -3.02, A100: -2.55, Rel3: -2.72, Rel30: -3.23, Rel300: -3.94, Rel1000: -4.32 (config names: A = Attack ms, Rel = Release ms,
  "R4_T-24" = Attack 7 / Release 100).
- Low-frequency ripple (−6 dBFS sine, R 4, T −24, Attack 7, Mix 100, Analog Off) H3 re fundamental, dBc:
  Release 3 ms: 50 Hz −23.8, 100 Hz −32.2, 200 Hz −44.3, 500 Hz < −120;
  Release 30 ms: −28.5 / −37.6 / −50.0 / < −120; Release 100 ms: −30.9 / −42.2 / −55.1 / < −120;
  Release 300 ms: −32.2 / −46.6 / −61.5 / < −120. (H2 < −85 dBc throughout.)

## 7. Dynamics
### 7.1 Attack
Gain decreases with a one-pole on **linear** gain, time constant = Attack (63 % in A ms).
### 7.2 Release
Normative: the trajectory tables in §7.4. Informative law that meets them: when the target gain is above the current
gain, the gain (in dB) approaches the target with a one-pole of time constant 0.7·Release.
### 7.3 Punch
The detector signal is delayed by P ms (P = Punch value, 0…30) relative to the audio: gain reduction (attack) and
recovery (release) both start P ms later than with Punch 0 (measured: P 5/10/20/30 → onset delays 5/10/20/30 ms;
release start delayed by the same amount). The default Punch 1.0 therefore delays by 1 ms.
### 7.4 Normative trajectories
Stimulus: 1 kHz sine at −40 dBFS (0.6 s) → −6 dBFS (0.6 s) → −40 dBFS (1.4 s); T −24, R 4, Mix 100, Analog Off,
Output 0. Values = per-millisecond RMS gain (dB, output re input) at the stated time after the step (attack table:
after the step up; release table: after the step down). Defaults Attack 7 ms, Release 100 ms, Punch 0 unless named.

Attack (ms after step up: 0, 1, 2, 3, 5, 8, 12, 20, 30, 50, 80, 150):
| A0.5 | -4.10 | -7.08 | -7.56 | -7.63 | -7.64 | -7.64 | -7.64 | -7.64 | -7.64 | -7.64 | -7.64 | -7.64 |
| A2 | +1.91 | -0.74 | -2.85 | -4.44 | -6.32 | -7.33 | -7.59 | -7.64 | -7.64 | -7.64 | -7.64 | -7.64 |
| R4_T-24 | +4.25 | +3.34 | +2.47 | +1.64 | +0.09 | -1.91 | -3.96 | -6.29 | -7.29 | -7.62 | -7.64 | -7.64 |
| A30 | +5.05 | +4.83 | +4.61 | +4.39 | +3.96 | +3.33 | +2.51 | +0.99 | -0.72 | -3.42 | -5.84 | -7.45 |
| A100 | +5.22 | +5.16 | +5.09 | +5.02 | +4.89 | +4.69 | +4.43 | +3.92 | +3.28 | +2.08 | +0.41 | -2.74 |
| P5 | +5.30 | +5.30 | +5.30 | +5.30 | +4.25 | +1.64 | -1.29 | -5.07 | -6.95 | -7.60 | -7.64 | -7.64 |
| P10 | +5.30 | +5.30 | +5.30 | +5.30 | +5.30 | +5.30 | +2.47 | -3.02 | -6.29 | -7.55 | -7.64 | -7.64 |
| P20 | +5.30 | +5.30 | +5.30 | +5.30 | +5.30 | +5.30 | +5.30 | +4.25 | -3.02 | -7.29 | -7.63 | -7.64 |
| P30 | +5.30 | +5.30 | +5.30 | +5.30 | +5.30 | +5.30 | +5.30 | +5.30 | +4.25 | -6.29 | -7.62 | -7.64 |

Release (ms after step down: 0, 2, 5, 10, 20, 50, 100, 200, 400, 800, 1300):
| Rel3 | -10.38 | -4.76 | -1.41 | +0.49 | +1.27 | +1.36 | +1.36 | +1.36 | +1.36 | +1.36 | +1.36 |
| Rel30 | -7.98 | -6.92 | -5.56 | -3.77 | -1.24 | +3.09 | +4.81 | +4.81 | +4.81 | +4.81 | +4.81 |
| R4_T-24 | -7.58 | -7.16 | -6.66 | -5.92 | -4.64 | -1.84 | +1.40 | +5.30 | +5.30 | +5.30 | +5.30 |
| Rel300 | -7.45 | -7.21 | -6.96 | -6.68 | -6.17 | -4.82 | -3.05 | -0.46 | +4.21 | +5.45 | +5.45 |
| Rel1000 | -7.42 | -7.35 | -7.30 | -7.21 | -7.04 | -6.54 | -5.79 | -4.50 | -2.49 | +0.37 | +3.69 |
| Rel3000 | -7.42 | -7.41 | -7.39 | -7.36 | -7.30 | -7.12 | -6.84 | -6.31 | -5.36 | -3.78 | -2.21 |
| Host_1-8 | -7.46 | -7.21 | -6.94 | -6.61 | -6.00 | -4.45 | -2.47 | +0.36 | +5.44 | +5.44 | +5.44 |
| Host_1-4 | -7.43 | -7.22 | -7.01 | -6.82 | -6.50 | -5.61 | -4.36 | -2.39 | +0.44 | +5.48 | +5.48 |
("A0.5" = Attack 0.5 ms; "Rel30" = Release 30 ms; "R4_T-24" = defaults; "P10" = Punch 10; "Host_1-8" = sync Host,
1/8 note at 120 BPM.)

## 8. Mix
y1 = (1 − m)·x + m·c, m = Mix/100, sample-aligned (both paths delayed by H). Linear crossfade, verified to 0.001.

## 9. Output gain
Exact dB gain O (±0.002 dB).

## 10. Analog modes (after Output gain; independent of the compressor)
Off = identity. Modes add colouration, harmonics and noise (measured with Ratio 1, Output 0, Mix 100):
Small-signal magnitude re Off (−30 dBFS sweep), dB, at 20 / 50 / 100 / 200 / 500 / 1k / 2k / 5k / 10k / 20k Hz:
| Mode 1 | -0.11 | -0.17 | -0.21 | -0.24 | -0.26 | -0.26 | -0.27 | -0.27 | -0.28 | -0.32 |
| Mode 2 | +0.01 | +0.01 | -0.05 | -0.26 | -0.63 | -0.76 | -0.78 | -0.68 | -0.42 | -0.08 |
| Mode 3 | +0.02 | +0.17 | +0.30 | -0.28 | -2.78 | -4.27 | -4.65 | -3.62 | -1.79 | -0.37 |
| Mode 4 | -0.36 | -0.34 | -0.29 | -0.34 | -0.34 | -0.31 | -0.18 | -0.08 | -0.07 | -0.22 |

Steady tones — fundamental gain re input (dB) / H2 (dBc) / H3 (dBc):
| Mode | 100 Hz a=0.03 | 100 Hz a=0.1 | 100 Hz a=0.3 | 100 Hz a=0.95 | 1 kHz a=0.03 | 1 kHz a=0.3 | 1 kHz a=0.95 |
|---|---|---|---|---|---|---|---|
| 1 | -0.21 / -82.9 / -87.7 | -0.21 / -74.2 / -77.6 | -0.2 / -65.2 / -68.8 | -0.17 / -56.6 / -60.8 | -0.26 / -81.2 / -84.6 | -0.24 / -62.3 / -65.9 | -0.2 / -54.0 / -58.3 |
| 2 | -0.05 / -97.8 / -94.1 | -0.05 / -94.5 / -83.4 | -0.05 / -89.3 / -74.2 | -0.05 / -79.1 / -64.7 | -0.76 / -92.0 / -75.5 | -0.7 / -70.8 / -56.4 | -0.57 / -61.8 / -47.8 |
| 3 | 0.31 / -61.3 / -62.3 | 0.28 / -51.4 / -52.9 | 0.22 / -43.6 / -46.0 | 0.13 / -37.8 / -42.4 | -4.3 / -33.1 / -34.7 | -1.64 / -23.0 / -28.4 | -0.49 / -21.9 / -32.2 |
| 4 | -0.33 / -109.3 / -36.8 | -0.1 / -125.2 / -47.2 | -0.03 / -132.8 / -56.7 | -0.01 / -141.0 / -66.7 | -0.31 / -116.7 / -36.6 | -0.03 / -147.0 / -56.6 | -0.0 / -147.9 / -66.6 |

Noise floor with silent input (dBFS RMS, full band): Mode 1 -101.6, Mode 2 -82.7,
Mode 3 -100.9, Mode 4 -108.7; Off: digital silence. Noise is uncorrelated between channels.
Clicks (single sample of amplitude a, peak of the output): a = 0.3 → Mode 1 0.2906, 2 0.2882,
3 0.2547, 4 0.299; a = 0.9 → 0.8743, 0.8679, 0.7982, 0.899
(no mode raises a transient above its input). Mode 3 is strongly level-dependent: its response follows the
post-Output level (e.g. Ratio 1: input −50 dBFS gives −5.1 dB with Output −12 and −4.75 dB with Output +12).

## 11. Output peak stage (ceiling 0 dBFS)
Both settings ("Limiter" On and Off) act as a look-ahead peak limiter with ceiling 1.0 (0 dBFS), sharing the H-sample
look-ahead: the gain starts falling ≈ 60 samples (48 kHz) before an over-ceiling peak and ramps smoothly (S-shaped,
−0.4 / −1.6 / −3.7 / −6.7 / −9.8 dB at 50 / 40 / 30 / 20 / 10 samples before a +12 dB step to −12 dB at the step);
a steady over-ceiling sine is output as a clean sine of peak 1.0 (H3 < −150 dBc). The two settings differ by at most
0.022 (linear) on a decaying noise burst driven 12 dB over the ceiling. Signals below 0 dBFS are untouched.

## 12. Sample rates
All times are in milliseconds and all static/makeup behaviour is sample-rate independent (verified at 44.1 / 48 /
96 kHz: static gains and makeup within 0.03 dB, attack/release/punch trajectories within 0.1 dB, analog-mode levels
within 0.05 dB). Look-ahead H: 64 samples at 44.1 / 48 kHz, 128 samples at 88.2 / 96 kHz (the gain-reduction onset
moves ≈ 45–66 samples early at 44.1/48 kHz and ≈ 85–145 samples at 96 kHz depending on Attack); at 176.4 / 192 kHz
use 256 (extrapolated, not measured). Report H as latency.

## 13. Tolerances
- Neutral identity (§3): bit-exact. Output gain: ±0.01 dB. Mix: ±0.01 dB.
- Static curve §4 + makeup §5 (steady 1 kHz sine, any T, R, Release): ±0.2 dB.
- Trajectories §7.4: each tabulated point ±1.5 dB, except within the first 5 ms after a level step ±3 dB; RMS over the
  per-ms trajectory 0…150 ms (attack) / 0…1300 ms (release) ≤ 0.8 dB.
- Punch onset/release delay: ±0.5 ms.
- Noise-vs-sine §6: ±0.5 dB. LF ripple H3 §6: ±4 dB.
- Analog §10: magnitude ±0.5 dB; fundamental ±0.5 dB; harmonics ±4 dB (mode 3 fundamental ±1 dB); noise ±3 dB.
- Output stage §11: never exceeds 1.0 (see D2); look-ahead ramp ±1 dB at the listed points.

## 14. User interface (functional layout — visuals must be original)
One panel: a large Threshold knob with a gain-reduction/level VU meter (0 … −48 scale) and an In / GR / Out meter
selector; knobs for Ratio, Attack, Release, Punch, Mix and Output; a Release sync selector (ms / Host / BPM) with the
release value shown as ms or as a note value, and a BPM readout (editable in BPM mode, shows host tempo in Host mode);
an Analog selector (Off, 1–4); a Limiter/Clipper toggle. Value boxes editable by typing; double-click / Alt-click resets.
All artwork, colours, fonts and names must be the implementer's own.

## 15. Review decisions
- D1: Reproduce all measured behaviour (tables normative), including the release-dependent makeup and the
  low-threshold knee.
- D2: The reference's output stage overshoots 1.0 by up to 2e-4 on transients; not reproduced — output never exceeds 1.0.
- D3: Limiter and Clipper are implemented as the same look-ahead peak limiter (they are measurably identical within
  0.022); the toggle is kept for compatibility.
- D4: Mono variant assumed identical on one channel (not measured).
- D5: Parameter changes must be smoothed click-free within 20 ms (the reference applies them asynchronously).

## 16. Open questions
1. Exact release law (tables normative; informative law §7.2).
2. The origin of the low-threshold knee (§4) and of the release dependence of the makeup (§5).
3. Exact output-stage smoothing law and the difference between Limiter and Clipper on extreme material.
4. Mono variant; rates above 96 kHz.
