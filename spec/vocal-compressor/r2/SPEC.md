# Vocal compressor — behavioural specification (r2)

Status: APPROVED r2 (2026-10-29, supersedes r1 after independent verification of a first implementation). Frozen — changes require a new revision. See "Changes from r1" at the end.

Derived only from black-box measurement of the licensed reference processor in a VST3 host (user-generated test
signals, host-visible parameter metadata) and its public user documentation. No code, disassembly or vendor assets
were used or are included. Product naming, parameter IDs, GUI and branding are the implementer's own choice.

## 1. Scope

A three-control dynamics processor for vocals: a downward expander ("gate"), a compressor with automatic makeup
whose static curve saturates near full scale, and an output gain. Stereo (2 in / 2 out, linked detection) is the
primary variant; a mono (1 in / 1 out) variant is identical with one channel. Linear-phase/zero-colouration: with all
controls neutral the output is the input multiplied by a constant 0.99 (§4.5).

## 2. Definitions

- `fs`: sample rate. `x[n]`: input frame; `a[n] = max(|x_L[n]|, |x_R[n]|)` (stereo; mono: `|x[n]|`).
- dB values are 20·log10 of linear amplitude. "Peak level" of a sine = its amplitude.
- `k = 10^(−Compression/20)` (Compression ≤ 0 dB, so k ≥ 1).

## 3. Parameters

Host-normalised value `r ∈ [0, 1]`.

| Parameter | Mapping | Range | Default | Display |
|---|---|---|---|---|
| Compression | `−36 + 36·r` dB | −36 … 0 dB | 0.0 dB | 0.1 dB |
| Gate | `r = 0`: off (shown "−Inf"); otherwise `−80 + 80·r` dB | off, −78 … 0 dB in practice (any r > 0 maps linearly) | off | 0.1 dB |
| Gain | `−36 + 36·r` dB | −36 … 0 dB | 0.0 dB | 0.1 dB |
| Bypass | host bypass | | off | |

All three are continuous; values may be quantised to the display resolution.

## 4. Signal processing

### 4.1 Look-ahead and latency
The processor works on a look-ahead window of `H` samples and reports a latency of `H` samples so that hosts time-align
its output: `H = 64` for fs ≤ 50 kHz (44.1/48 kHz), `H = 128` for 88.2/96 kHz. (176.4/192 kHz: use 256 — not measured.)
All statements below are about the latency-compensated (time-aligned) output.

### 4.2 Gate gain (downward expander, 1:2, hard knee)
Gate level detector (instant attack via look-ahead; release whose rate depends on the distance to the current level),
in dB (`L_A[n] = 20·log10(A[n])`):
```
A[n]   = max(a[n], a[n+1], …, a[n+H])                     // forward max over the look-ahead window
D      = L_d[n−1] − L_A[n]                                // dB above the current (held) input level
if D <= 0:  L_d[n] = L_A[n]                               // instant attack
else:       L_d[n] = L_A[n] + max(0, D − S(D)/fs)         // release; reaches the input level in finite time
S(D)   = 21.57 + 44.19·(1 − exp(−D/8.03))   dB/s                 // ≈ 66 dB/s far above the level, ≈ 22 dB/s just above it
gg[n]  = min(1, 10^((L_d[n] − Gate)/20))                   // gate off: gg = 1
```
So below the threshold the gain falls 1 dB per dB of detected level below the threshold (expansion ratio 1:2); there is
no soft knee in the measurement resolution (2 dB steps). The gate opens instantly (it anticipates level rises through the
look-ahead) and closes as the detector releases toward the current input level along the curve above. The closing shape
depends only on the distance to the current level (verified for drops of 30–50 dB at thresholds 0/−20/−40 dB: all
curves collapse onto one within 1 ms), not on the absolute level. The closed form reproduces the measured curve within
4 ms / 0.27 dB at 44.1, 48 and 96 kHz (§9.6.1 is the normative table).

### 4.3 Compressor gain
Static (memoryless) target, computed from the UN-gated input level:
```
t[n] = 1 / (1 + (k − 1) · a[n])                 // normalised target gain (1 at silence, 1/k at full scale)
h[n] = min(t[n], t[n+1], …, t[n+H])             // forward min over the look-ahead window
```
`h` then passes a program-dependent release stage (§4.4) producing `q[n] ≤ 1` with instant attack
(`q[n] = h[n]` whenever `h[n]` is lower than the release-stage state).

### 4.4 Release (program dependent)
The release behaviour is specified normatively by the measured tables in §9.3/§9.4 (tolerances in §8). Observed
properties the implementation must exhibit:
1. After a short transient (≤ 20 ms above the sustained level) the gain returns to the sustained-level value within a
   few milliseconds (essentially only the look-ahead smoothing of §4.5 limits it).
2. After sustained heavy compression the release is slow and S-shaped (initial plateau that grows with the amount of
   compression), completing in roughly 1–1.5 s (§9.3).
3. Intermediate burst lengths give intermediate behaviour (§9.4): the "slow" component charges with a time constant of
   the order of 100–300 ms.
4. Steady low-frequency tones: the gain ripple within one cycle follows §9.8 (reference values; it is largest at weak
   compression, e.g. 0.69 dB at Compression −6 / 50 Hz, and ≤ 0.11 dB at Compression −36, because stronger
   compression engages the slow, held component).
5. Release times are fixed in milliseconds (identical at 44.1/48/96 kHz within 0.1 dB).
6. The release after a loud passage depends on the level the signal drops to, not only on the passage's length: after a
   50 ms 0 dBFS passage, returning to −40 dBFS releases almost entirely within a few ms, but returning to −20 dBFS keeps
   ≈ 90 % of the reduction at 2 ms and releases over ≈ 100–200 ms (Compression −36). Double bursts and burst trains
   build up the slow component more than a single burst of the same length. §9.7 tabulates these cases normatively.
Informative (non-normative) structure that reproduces these properties approximately: `q = min(fast, slow)` where
`fast = h` (no extra release) and `slow` is a gain derived from a slowly charging/discharging envelope of the held level
(`1/(1 + (k−1)·e_slow)`), with the slow envelope's release shaped in two stages (≈ 20–65 ms in the dB domain,
growing with |Compression|, followed by ≈ 0.45 s in the linear-gain domain). Implementers are free to choose any
algorithm meeting §9.3/§9.4 within §8.

### 4.5 Smoothing, combination and output
```
s[n]  = q[n] · gg[n]
s1[n] = mean(s[n−37 … n])        // boxcar, 38 samples   (88.2/96 kHz: 75 samples)
s2[n] = mean(s1[n−26 … n])       // boxcar, 27 samples   (88.2/96 kHz: 54 samples)
y[n]  = x[n] · k · s2[n] · 0.99 · 10^(Gain/20)      // same gain on both channels (linked)
```
(Boxcars are causal moving averages; with the forward windows above, a level rise is anticipated by ≈ 65 samples at
48 kHz.) The constant 0.99 (−0.087 dB) is part of the reference behaviour at every setting.

Consequences (all measured): neutral settings (Compression 0, Gate off, Gain 0) → `y = 0.99·x` exactly, no limiting
even above 0 dBFS; Gain is a plain output gain; the compressor's static curve `y = 0.99·k·x/(1+(k−1)|x|)` approaches
≈ 0 dBFS output for large inputs at strong compression (this is the "ceiling" behaviour); detection is linked and
frequency-independent (50 Hz–12 kHz within 0.1 dB); output is independent of input level scaling only through the
curves above (no further nonlinearity: residual distortion on steady sines equals that of the gain modulation).

## 5. Signal flow summary
input → [look-ahead level analysis] → gate gain × compressor gain (with program-dependent release) → two boxcar
smoothers → × k (makeup) × 0.99 × output Gain → output. Mono variant: same with one channel.

## 6. Dynamic parameter changes
Not measured. Parameter changes must be click-free (implementer choice, e.g. ≈ 20 ms smoothing of Compression, Gate
threshold and Gain in the dB domain).

## 7. Behaviour not specified / out of scope
Metering ballistics (input energy, total gain reduction with peak hold, output peak meters), GUI skins, presets.
176.4/192 kHz look-ahead lengths (extrapolated in §4.1).

## 8. Tolerances for acceptance
- Static curve (steady 1 kHz sines −90…0 dBFS, any Compression/Gain, gate off): ±0.05 dB vs §4.3/§4.5 (§9.1).
- Neutral: |y − 0.99·x| ≤ 1e-6 (float32 rounding).
- Look-ahead attack ramp (§9.2): ±0.25 dB per tabulated point; latency reported = H samples.
- Gate static (§9.5): ±0.2 dB; gate closing trajectories (§9.6, §9.6.1): ±0.75 dB per point.
- Release tables (§9.3, §9.4, §9.7): ±1.5 dB or ±25 % of the tabulated remaining gain reduction, whichever is larger, per point.
- Steady-tone gain ripple (§9.8): ≤ tabulated value + 0.15 dB. Linking: both channels receive identical gain.

## 9. Test vectors (48 kHz unless stated)

### 9.1 Static gain (exact): `G(L) = 20·log10(0.99 · k / (1 + (k−1)·10^(L/20)))` dB for a steady sine of peak level L dBFS
| Compression | L = −90 | −40 | −30 | −20 | −12 | −4 | 0 |
|---|---|---|---|---|---|---|---|
| 0 | −0.09 | −0.09 | −0.09 | −0.09 | −0.09 | −0.09 | −0.09 |
| −6 | +5.91 | +5.83 | +5.64 | +5.09 | +3.97 | +1.68 | −0.09 |
| −18 | +17.91 | +17.33 | +16.19 | +13.33 | +9.14 | +3.30 | −0.09 |
| −36 | +35.90 | +31.72 | +26.48 | +18.75 | +11.51 | +3.83 | −0.09 |
Add Gain (dB) to every value. Single isolated samples obey the same law (e.g. Compression −36: a lone sample of
amplitude 0.01 / 0.1 / 0.5 / 1 / 2 after silence is output as 0.385 / 0.866 / 0.975 / 0.990 / 0.998).

### 9.2 Look-ahead attack ramp
Gain in dB (0.99 factor excluded) around a 1 kHz step from −40 dBFS to 0 dBFS, n = samples relative to the first
loud sample:
| Compression | −70 | −64 | −60 | −56 | −48 | −40 | −32 | −24 | −16 | −8 | −4 | −1 | 0 | +8 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| −36 | 31.80 | 31.80 | 31.73 | 31.52 | 30.65 | 28.98 | 26.15 | 21.88 | 15.92 | 7.81 | 3.22 | 0.65 | 0.25 | 0.00 |
| −18 | 17.42 | 17.42 | 17.37 | 17.21 | 16.51 | 15.16 | 12.93 | 9.80 | 6.03 | 2.35 | 0.95 | 0.30 | 0.18 | 0.00 |
(Values are 20·log10(k·s2[n]) from the §4.3 hold + §4.5 boxcars, which the reference matches within 0.001 dB at every
sample where the input is not near zero. At n = 0 the 1 kHz step starts at phase 0 (x = 0), so a sample-wise gain cannot
be measured there; the tabulated value is the gain applied by the chain. r1's values at n = −1 and n = 0 were measured
sample-wise near zero crossings and were wrong. The same ramp shape in samples holds at 44.1 kHz; at 96 kHz lengths
double.)

### 9.3 Release after sustained levels
Remaining gain reduction (dB, relative to the final steady gain at the lower level) after a 1 kHz tone held 1.5 s at the
upper level drops to the lower level; t in ms after the drop.
| Compression | step (dBFS) | 2 | 5 | 10 | 20 | 50 | 100 | 200 | 400 | 700 | 1000 | 1500 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| −6 | 0→−40 | 5.82 | 5.81 | 5.78 | 5.69 | 5.25 | 4.52 | 3.44 | 2.09 | 1.04 | 0.51 | 0.13 |
| −6 | −10→−40 | 2.19 | 2.17 | 2.15 | 2.10 | 1.95 | 1.74 | 1.38 | 0.87 | 0.42 | 0.19 | 0.01 |
| −6 | −20→−30 | 0.46 | 0.46 | 0.45 | 0.44 | 0.41 | 0.36 | 0.28 | 0.16 | 0.05 | −0.01 | −0.06 |
| −18 | 0→−40 | 17.33 | 17.32 | 17.30 | 17.22 | 16.55 | 14.06 | 8.68 | 4.31 | 2.00 | 0.99 | 0.29 |
| −18 | −10→−40 | 9.42 | 9.41 | 9.39 | 9.32 | 8.82 | 7.51 | 5.32 | 3.03 | 1.47 | 0.73 | 0.20 |
| −18 | −20→−30 | 2.77 | 2.76 | 2.74 | 2.68 | 2.49 | 2.21 | 1.73 | 1.08 | 0.53 | 0.25 | 0.03 |
| −36 | 0→−40 | 31.71 | 31.69 | 31.61 | 31.33 | 29.41 | 24.02 | 14.34 | 5.97 | 2.46 | 1.18 | 0.35 |
| −36 | −10→−40 | 22.01 | 22.00 | 21.97 | 21.87 | 21.14 | 18.57 | 12.23 | 5.41 | 2.29 | 1.11 | 0.33 |
| −36 | −20→−30 | 7.63 | 7.63 | 7.63 | 7.61 | 7.50 | 7.13 | 5.97 | 3.65 | 1.66 | 0.79 | 0.22 |

### 9.4 Release after bursts (program dependence)
A 0 dBFS 1 kHz burst of length B inside a −40 dBFS 1 kHz tone; remaining gain reduction (dB re the steady −40 dBFS gain)
at t ms after the burst ends.
| Compression | B (ms) | 2 | 5 | 10 | 20 | 50 | 100 | 200 | 400 | 700 | 1000 | 1500 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| −6 | 2 | 0.00 | 0.00 | 0.00 | 0.00 | 0.00 | 0.00 | 0.00 | 0.00 | 0.00 | 0.00 | 0.00 |
| −6 | 5 | 0.01 | 0.01 | 0.01 | 0.00 | 0.00 | 0.00 | 0.00 | 0.00 | 0.00 | 0.00 | 0.00 |
| −6 | 10 | 0.09 | 0.09 | 0.09 | 0.08 | 0.07 | 0.06 | 0.03 | 0.00 | 0.00 | 0.00 | 0.00 |
| −6 | 20 | 0.26 | 0.25 | 0.25 | 0.24 | 0.22 | 0.19 | 0.14 | 0.07 | 0.00 | 0.00 | 0.00 |
| −6 | 50 | 0.73 | 0.73 | 0.72 | 0.70 | 0.65 | 0.58 | 0.46 | 0.27 | 0.11 | 0.02 | 0.00 |
| −6 | 100 | 1.46 | 1.45 | 1.43 | 1.40 | 1.30 | 1.16 | 0.92 | 0.58 | 0.27 | 0.11 | 0.00 |
| −6 | 200 | 2.87 | 2.67 | 2.62 | 2.56 | 2.38 | 2.11 | 1.67 | 1.05 | 0.52 | 0.24 | 0.03 |
| −6 | 500 | 5.56 | 5.30 | 5.02 | 4.72 | 4.26 | 3.71 | 2.86 | 1.77 | 0.88 | 0.43 | 0.10 |
| −6 | 1000 | 5.81 | 5.78 | 5.72 | 5.59 | 5.12 | 4.42 | 3.37 | 2.05 | 1.02 | 0.50 | 0.13 |
| −18 | 2 | 0.00 | 0.00 | 0.00 | 0.00 | 0.00 | 0.00 | 0.00 | 0.00 | 0.00 | 0.00 | 0.00 |
| −18 | 5 | 0.02 | 0.02 | 0.02 | 0.02 | 0.01 | 0.00 | 0.00 | 0.00 | 0.00 | 0.00 | 0.00 |
| −18 | 10 | 0.17 | 0.17 | 0.17 | 0.16 | 0.15 | 0.13 | 0.09 | 0.03 | 0.00 | 0.00 | 0.00 |
| −18 | 20 | 0.47 | 0.47 | 0.46 | 0.45 | 0.42 | 0.37 | 0.28 | 0.16 | 0.05 | 0.00 | 0.00 |
| −18 | 50 | 1.36 | 1.34 | 1.33 | 1.30 | 1.21 | 1.08 | 0.85 | 0.53 | 0.25 | 0.09 | 0.00 |
| −18 | 100 | 3.44 | 3.02 | 2.82 | 2.69 | 2.50 | 2.20 | 1.74 | 1.09 | 0.54 | 0.25 | 0.04 |
| −18 | 200 | 9.12 | 7.76 | 6.84 | 6.01 | 5.07 | 4.32 | 3.29 | 2.00 | 0.99 | 0.49 | 0.12 |
| −18 | 500 | 17.15 | 16.87 | 16.41 | 15.51 | 13.02 | 9.94 | 6.51 | 3.54 | 1.69 | 0.84 | 0.24 |
| −18 | 1000 | 17.32 | 17.30 | 17.26 | 17.12 | 16.27 | 13.55 | 8.38 | 4.22 | 1.96 | 0.97 | 0.29 |
| −36 | 2 | 0.39 | 0.06 | 0.00 | 0.00 | 0.00 | 0.00 | 0.00 | 0.00 | 0.00 | 0.00 | 0.00 |
| −36 | 5 | 0.53 | 0.14 | 0.00 | 0.00 | 0.00 | 0.00 | 0.00 | 0.00 | 0.00 | 0.00 | 0.00 |
| −36 | 10 | 0.70 | 0.25 | 0.03 | 0.00 | 0.00 | 0.00 | 0.00 | 0.00 | 0.00 | 0.00 | 0.00 |
| −36 | 20 | 1.09 | 0.56 | 0.23 | 0.04 | 0.00 | 0.00 | 0.00 | 0.00 | 0.00 | 0.00 | 0.00 |
| −36 | 50 | 3.28 | 2.35 | 1.75 | 1.28 | 0.93 | 0.80 | 0.63 | 0.39 | 0.17 | 0.05 | 0.00 |
| −36 | 100 | 7.84 | 6.00 | 4.82 | 3.82 | 2.80 | 2.27 | 1.75 | 1.08 | 0.53 | 0.24 | 0.03 |
| −36 | 200 | 21.35 | 16.62 | 13.40 | 10.61 | 7.56 | 5.64 | 3.93 | 2.29 | 1.10 | 0.54 | 0.14 |
| −36 | 500 | 31.24 | 30.52 | 29.39 | 27.31 | 22.30 | 16.49 | 9.93 | 4.59 | 2.03 | 0.98 | 0.29 |
| −36 | 1000 | 31.68 | 31.61 | 31.45 | 31.01 | 28.65 | 23.03 | 13.73 | 5.79 | 2.41 | 1.16 | 0.35 |
At 44.1 and 96 kHz the same table holds within 0.2 dB.

### 9.5 Gate static (Compression 0, steady 1 kHz sines; gain re open)
Gate threshold T: gain = 0 dB for L ≥ T; gain = (L − T) dB for L < T. Examples: T = −40: L = −46 → −6.0, L = −50 →
−10.0, L = −70 → −30.0, L = −90 → −50.0. T = −78: L = −84 → −6.0. With Compression −18 and Gate −40 the gains multiply:
L = −50 → +7.8 dB, L = −60 → −2.1 dB (compressor gain computed from the ungated input level).

### 9.6 Gate closing
Gain (dB re open) after a 1 kHz tone drops from −20 to −70 dBFS (Compression 0); t ms after the drop:
| Gate | 100 | 200 | 300 | 350 | 400 | 500 | 600 | 700 | 800 | 900 | 1000 |
|---|---|---|---|---|---|---|---|---|---|---|---|
| −20 | −6.75 | −13.38 | −19.96 | −23.22 | −26.43 | −32.68 | −38.49 | −43.55 | −47.52 | −50.01 | −50.01 |
| −40 | 0.00 | 0.00 | 0.00 | −3.22 | −6.43 | −12.68 | −18.49 | −23.55 | −27.52 | −30.01 | −30.01 |
| −60 | 0.00 | 0.00 | 0.00 | 0.00 | 0.00 | 0.00 | 0.00 | −3.55 | −7.52 | −10.01 | −10.01 |
Re-opening when the tone returns to −20 dBFS is complete at the first loud sample (look-ahead).

### 9.6.1 Gate closing at higher levels (normative; shape depends only on the distance to the new level)
Gain (dB re the gain before the drop; Compression 0) after a 1 kHz tone drops from `hi` to `lo` dBFS; t ms after the drop.
| Gate | hi → lo | 50 | 100 | 200 | 300 | 400 | 500 | 600 | 700 | 750 | 800 | 850 | 900 | 1000 | 1200 | 1500 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 0 | 0 → −50 | −3.41 | −6.74 | −13.37 | −19.95 | −26.43 | −32.67 | −38.49 | −43.55 | −45.68 | −47.51 | −49.02 | −50.00 | −50.00 | −50.00 | −50.00 |
| 0 | 0 → −30 | −3.33 | −6.55 | −12.79 | −18.59 | −23.63 | −27.57 | −30.00 | −30.00 | −30.00 | −30.00 | −30.00 | −30.00 | −30.00 | −30.00 | −30.00 |
| −20 | 0 → −50 | −0.00 | −0.00 | −0.00 | −0.00 | −6.43 | −12.68 | −18.49 | −23.55 | −25.69 | −27.51 | −29.02 | −30.01 | −30.01 | −30.01 | −30.01 |
| −20 | −10 → −60 | 0.00 | 0.00 | −3.38 | −9.96 | −16.43 | −22.68 | −28.49 | −33.55 | −35.69 | −37.51 | −39.02 | −40.00 | −40.00 | −40.00 | −40.00 |
| −40 | −10 → −60 | 0.00 | 0.00 | 0.00 | 0.00 | 0.00 | −2.68 | −8.49 | −13.55 | −15.69 | −17.51 | −19.02 | −20.01 | −20.01 | −20.01 | −20.01 |

Detector release (time for the detector to fall from D dB above the current level to the level itself):
| D (dB) | 0.1 | 0.25 | 0.5 | 1 | 2 | 3 | 4 | 5 | 6 | 8 | 10 | 12 | 15 | 20 | 25 | 30 | 35 | 40 | 45 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| time (ms) | 4.5 | 11.0 | 21.3 | 40.6 | 74.8 | 104.8 | 131.9 | 156.7 | 179.9 | 222.6 | 261.9 | 298.8 | 351.1 | 433.4 | 512.2 | 589.3 | 665.4 | 740.9 | 816.1 |

### 9.7 Release after other level patterns (normative)
Remaining gain reduction (dB re the final steady gain at the lower level) at t ms after the last loud sample. Cases
(1 kHz tones; levels in dBFS): `step_hi_lo_ms` = 0.5 s at lo, then `ms` at hi, then lo; `dbl_50` = −40 tone with two
0 dBFS bursts of 50 ms, 100 ms start-to-start; `train_30` = −40 tone with five 0 dBFS bursts of 30 ms every 200 ms.
| Compression | case | 2 | 5 | 10 | 20 | 50 | 100 | 200 | 400 | 700 | 1000 | 1500 | 2000 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| −12 | step_-6_-30_300 | 5.95 | 5.28 | 4.77 | 4.32 | 3.84 | 3.36 | 2.62 | 1.64 | 0.84 | 0.44 | 0.14 | 0.03 |
| −12 | step_0_-20_50 | 1.27 | 0.97 | 0.85 | 0.81 | 0.76 | 0.68 | 0.55 | 0.36 | 0.19 | 0.10 | 0.03 | 0.01 |
| −12 | step_0_-60_1500 | 11.91 | 11.90 | 11.88 | 11.81 | 11.22 | 9.43 | 6.39 | 3.53 | 1.70 | 0.86 | 0.26 | 0.05 |
| −12 | step_-3_-40_150 | 3.67 | 3.28 | 3.09 | 2.97 | 2.76 | 2.44 | 1.94 | 1.24 | 0.64 | 0.33 | 0.10 | 0.02 |
| −12 | step_-10_-20_2000 | 3.47 | 3.47 | 3.45 | 3.40 | 3.18 | 2.81 | 2.20 | 1.39 | 0.72 | 0.37 | 0.12 | 0.02 |
| −12 | dbl_50 | 2.41 | 2.31 | 2.28 | 2.23 | 2.08 | 1.86 | 1.49 | 0.96 | 0.50 | 0.26 | 0.08 | 0.02 |
| −12 | train_30 | 2.06 | 2.02 | 2.00 | 1.95 | 1.83 | 1.63 | 1.31 | 0.85 | 0.45 | 0.23 | 0.07 | 0.01 |
| −24 | step_-6_-30_300 | 14.38 | 13.45 | 12.29 | 10.74 | 8.32 | 6.40 | 4.49 | 2.60 | 1.27 | 0.64 | 0.20 | 0.04 |
| −24 | step_0_-20_50 | 7.27 | 4.87 | 3.32 | 2.02 | 0.75 | 0.21 | 0.02 | 0.00 | 0.00 | 0.00 | 0.00 | 0.00 |
| −24 | step_0_-60_1500 | 23.79 | 23.77 | 23.74 | 23.59 | 22.41 | 18.20 | 10.27 | 4.80 | 2.19 | 1.09 | 0.33 | 0.07 |
| −24 | step_-3_-40_150 | 7.15 | 6.04 | 5.29 | 4.66 | 4.01 | 3.48 | 2.70 | 1.68 | 0.86 | 0.45 | 0.14 | 0.03 |
| −24 | step_-10_-20_2000 | 7.15 | 7.15 | 7.14 | 7.13 | 7.01 | 6.62 | 5.43 | 3.16 | 1.46 | 0.72 | 0.22 | 0.04 |
| −24 | dbl_50 | 4.12 | 3.51 | 3.15 | 2.90 | 2.66 | 2.35 | 1.86 | 1.19 | 0.62 | 0.32 | 0.10 | 0.02 |
| −24 | train_30 | 3.41 | 2.98 | 2.72 | 2.56 | 2.36 | 2.10 | 1.67 | 1.07 | 0.56 | 0.29 | 0.09 | 0.02 |
| −36 | step_-6_-30_300 | 20.16 | 19.47 | 18.45 | 16.76 | 13.36 | 10.09 | 6.65 | 3.54 | 1.58 | 0.77 | 0.23 | 0.05 |
| −36 | step_0_-20_50 | 16.95 | 14.78 | 12.32 | 9.38 | 5.48 | 3.09 | 1.41 | 0.46 | 0.11 | 0.03 | 0.00 | 0.00 |
| −36 | step_0_-60_1500 | 35.38 | 35.34 | 35.23 | 34.76 | 31.52 | 23.29 | 12.05 | 5.23 | 2.33 | 1.15 | 0.35 | 0.07 |
| −36 | step_-3_-40_150 | 13.81 | 10.52 | 8.49 | 6.76 | 4.92 | 3.82 | 2.83 | 1.73 | 0.87 | 0.45 | 0.14 | 0.03 |
| −36 | step_-10_-20_2000 | 9.02 | 9.02 | 9.02 | 9.00 | 8.91 | 8.61 | 7.64 | 5.33 | 2.82 | 1.45 | 0.45 | 0.09 |
| −36 | dbl_50 | 7.83 | 6.01 | 4.84 | 3.84 | 2.84 | 2.31 | 1.79 | 1.13 | 0.59 | 0.30 | 0.09 | 0.02 |
| −36 | train_30 | 7.77 | 5.96 | 4.80 | 3.81 | 2.82 | 2.29 | 1.78 | 1.13 | 0.58 | 0.30 | 0.09 | 0.02 |

### 9.8 Steady-tone gain ripple (reference)
Peak-to-peak gain variation (dB) over the samples of the last 0.5 s of a −6 dBFS sine where |x| exceeds half its amplitude.
| Compression | 50 Hz | 100 Hz | 200 Hz |
|---|---|---|---|
| −6 | 0.69 | 0.24 | 0.02 |
| −18 | 0.20 | 0.04 | 0.00 |
| −36 | 0.11 | 0.03 | 0.00 |

## 10. Review decisions (standing policy: match the reference, do not reproduce clear defects; ambiguity resolved by the analyst)
- D1 — No separate hard output limiter: the reference has none (neutral settings pass +6 dBFS samples scaled by 0.99);
  its "ceiling" behaviour is the saturating static curve plus output Gain. Reproduced as measured (not a defect: the
  documentation's "will not clip" holds for meaningful compression settings).
- D2 — The constant 0.99 output factor is reproduced (it is consistent, intentional headroom, not an error).
- D3 — Release is normative by tables (§9.3/§9.4) with tolerances, because no compact exact law could be identified;
  the informative structure in §4.4 is a starting point.
- D4 — Compressor detection uses the ungated input level (measured); the documentation's "compression after the gate"
  wording is satisfied because the gains multiply.
- D5 — Gate value r = 0 means "off" exactly (no reduction at any level); any r > 0 is a threshold −80 + 80·r dB.
- D6 — Parameter changes are smoothed click-free (not measured; implementer choice, §6).
- D7 — 176.4/192 kHz: H = 256, boxcars 150 and 108 samples (extrapolated by the doubling observed 48→96 kHz).
- D8 (r2) — The r1 claim "ripple ≤ 0.1 dB in the reference" held only at strong compression; §9.8 now tabulates the
  measured ripple and the tolerance is relative to it.

## 11. Open questions
1. Exact release law (specified by tables, §4.4).
2. 176.4/192 kHz look-ahead/boxcar lengths (extrapolated).
3. Parameter-change smoothing (not measured).
4. Mono variant assumed identical on one channel (not measured).

## 12. User interface (functional layout — visuals must be original)
Same controls, arrangement and display semantics as the reference so that a user finds everything in the same place, but
all artwork, colours, fonts, logos and names are the implementer's own (drawn in code; no copied images or branding).
- Left: a vertical **Gate** threshold fader overlaid on an **input energy meter** (scale 12 … 72 dB below full scale,
  top = 0), value box above (shows "−Inf"/off at the bottom position), input level readout below.
- Centre: a large vertical **Compression** fader (0 at top … −36 at bottom, scale marks every 6 dB) drawn inside a
  **total gain-reduction meter** (compressor + gate, peak-hold value below, click to reset).
- Right: a vertical **Gain** fader (0 at top … −36) overlaid on stereo **output peak meters** (L/R), value above, peak
  readouts below (click to reset).
- Linking gesture: Compression and Gain can be moved together (e.g. modifier-drag) to audition compression at constant
  level. Double-click (or Alt-click) resets a control to default; values can be typed.
- Meters are display only.

## Changes from r1
| Section | r1 | r2 | Reason |
|---|---|---|---|
| §4.2 gate detector | release at a constant 63 dB/s down to the current level (sharp corner) | dB-domain release with distance-dependent rate S(D) = 21.57 + 44.19·(1 − e^(−D/8.03)) dB/s | Verification: the measured closing curve is level-independent and rounds off over the last ≈ 10 dB; the constant-rate law missed §9.6 by up to 2.5 dB (at the floor) and 1.2 dB early |
| §4.4 item 4, §8, §9.8 | ripple ≤ 0.2 dB, "reference ≤ 0.1 dB" | per-setting table + 0.15 dB | reference ripple is 0.69 dB at Compression −6 / 50 Hz |
| §4.4 item 6, §9.7 | — | release depends on the level dropped to; new normative table of 21 cases | a first implementation fitted to §9.3/§9.4 deviated by up to 16 dB on other patterns |
| §8 gate tolerance | ±1.5 dB | ±0.75 dB | the law now reproduces the reference within 0.27 dB |
| §9.2 n = −1, 0 | 0.99/0.40 (−36), 0.39/0.21 (−18) | chain values (model = reference within 0.001 dB where measurable) | r1 values were measured near zero crossings (ill-conditioned) |
| §9.6.1 | — | high-level gate closing table + detector release time table | normative shape at levels well above the noise floor |
