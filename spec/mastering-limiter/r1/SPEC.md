# Look-ahead maximiser with requantisation — behavioural specification (r1)

Status: APPROVED r1 (2026-10-29; review decisions D1–D5 in §12 under the standing policy: match the reference, do not reproduce clear defects, original visuals, own naming). Frozen — changes require a new revision.

Derived only from black-box measurement of the licensed reference processor in a VST3 host (user-generated
stimuli: DC carrier + clicks/bursts, DC steps, sine level staircases, inter-sample-peak sines, synthetic
noise program, digital silence, low-level sines, slow LSB ramps, DC offsets), host-visible parameter
metadata and the publicly shipped user guide. No code, disassembly or vendor assets were used or are included.

Reference-model accuracy (48 kHz, limiter with requantisation bypassed, see §9): ≤ −105 dB re peak on 34/38
deterministic configurations, ≤ −88 dB on all; ≤ −106 dB at 44.1 and 96 kHz. Remaining residuals are
float32 rounding inside the reference (§4 note).

Product naming, parameter IDs, GUI and branding are the implementer's own.

## 1. Scope
Wideband brick-wall peak limiter with look-ahead, automatic gain make-up, an adaptive-release mode, and a final
requantisation stage with selectable word length, dither and noise shaping. Variants sharing one algorithm:
**Stereo** (2 in / 2 out, linked detection) and **Mono** (1 in / 1 out). An external-sidechain variant exists
on the reference but is out of scope for r1.

Signal flow: input → limiter gain (§4/§5) × make-up → requantiser (§6/§7) → output.

## 2. Parameters
Host-normalised value `r ∈ [0,1]`.

| Parameter | Mapping | Range | Default | Resolution / display |
|---|---|---|---|---|
| Threshold | `−30 + 30 r` dB | −30 … 0 dB | 0.0 dB | 0.1 dB, "−12.0 dB" |
| Out Ceiling | `−30 + 30 r` dB | −30 … 0 dB | 0.0 dB | 0.1 dB |
| Release | `0.01 · 10^(5r)` ms | 0.01 … 1000 ms | 1.00 ms (r = 0.4) | value = 2-decimal display, "1.00 ms" |
| Adaptive Release | toggle (r ≥ 0.5 on) | off/on | **on** | — |
| Word Length | 5 steps: 24 / 22 / 20 / 18 / 16 bit (r = 0, 0.25, 0.5, 0.75, 1; nearest step) | | 24 bit | "24 Bits" |
| Dither | 3 steps: None / Type 1 / Type 2 (r = 0, 0.5, 1) | | Type 1 | |
| Noise Shaping | 4 steps: None / Moderate / Normal / Ultra (r = 0, 1/3, 2/3, 1) | | Normal | |
| Bypass | host bypass | | off | |

Notes: with Adaptive Release on, the Release control has no audible effect (measured: outputs bit-identical
for Release 3.16 / 31.6 / 316 ms). The UI should show Release as inactive in that mode.

## 3. Latency
Reported and actual latency `L`; all sample positions below are in aligned time (output sample n corresponds to
input sample n).

| Sample rate | L |
|---|---|
| 44.1 / 48 kHz | 64 samples |
| 88.2 / 96 kHz | 128 samples |
| 176.4 / 192 kHz | 256 samples (by extrapolation) |

## 4. Limiter gain, fixed release (Adaptive Release off)
For each aligned sample n (stereo: one common gain):

1. Peak `p[n] = max(|x_L[n]|, |x_R[n]|)` (mono `|x[n]|`). Sample peak only — no inter-sample (true-peak)
   detection: an fs/4 sine with sample peaks 0.707 passes unchanged at Threshold 0 although its true peak is 1.0.
2. Required gain `r[n] = min(1, T / p[n])`, `T = 10^(Threshold/20)`.
3. Look-ahead hold over the following L samples (current sample excluded): `h[n] = min(r[n+1] … r[n+L])`.
4. Release (`q[−1] = 1`): `q[n] = h[n]` if `h[n] < q[n−1]`, else `q[n] = h[n] − (h[n] − q[n−1])·c`,
   `c = exp(−ln10 / (Release_s·fs))` — the linear distance to the current hold target shrinks ×10 per
   Release time.
5. Smoothing: two causal boxcars in cascade, `s = MA_B(MA_A(q))`, `(A,B) = (38,27)` at 44.1/48 kHz,
   `(75,54)` at 88.2/96 kHz, `(150,107)` at 176.4/192 kHz (`A + B − 1 = L`).
6. Ceiling enforcement: `g[n] = min(s[n], r[n])` — the reference never exceeds the ceiling on any sample
   (measured; matches this rule to ≤ −135 dB).
7. Limited signal: `v[n] = x[n] · g[n] · 10^((Ceiling − Threshold)/20)`.

Below threshold the gain is the constant `Ceiling − Threshold` dB. With Threshold = Ceiling = 0, no overs and
requantisation bypassed (§6: None/None at a word length whose grid contains the input) the output is bit-exact.

Note (precision): the reference keeps the release state in single precision; with long releases its recovery
differs from a double-precision implementation by up to ~2e-5 (≈ −92 dB). Either precision is acceptable (§10).

## 5. Adaptive Release (default on)
Attack, look-ahead hold, smoothing and ceiling enforcement are unchanged; only step 4 changes. Behaviour:
recovery after an overload depends on the overload's **duration** (not its depth, not the Release control):
short overloads recover within ~1–2 ms to a small residual that then decays slowly; overloads ≥ ~0.5 s recover
along a single slow curve (linear-gain deficit ×10 per 500 ms: t63 ≈ 217 ms, t90 ≈ 498 ms, t99 ≈ 998 ms).

Normative measured recovery of the linear-gain deficit after a DC step overload ends (Threshold −12 dB,
times from the end of the overload; identical at 44.1, 48, 96 kHz):

| Overload | Depth over threshold | t63 | t90 | t99 |
|---|---|---|---|---|
| 5 ms | 12 dB | 0.96 ms | 1.35 ms | 377 ms |
| 20 ms | 12 dB | 1.10 ms | 126 ms | 626 ms |
| 100 ms | 12 dB | 111 ms | 392 ms | 892 ms |
| 500 ms | 12 dB | 217 ms | 498 ms | 998 ms |
| 2000 ms | 12 dB | 219 ms | 501 ms | 1000 ms |
| 500 ms | 6 dB | 216 ms | 498 ms | 998 ms |
| 500 ms | 24 dB | 222 ms | 498 ms | 998 ms |

Recommended realisation (meets the table within the §10 tolerance; the exact law is free): two followers of
the hold target `h` in linear gain, output `q = min(fast, slow)`:
- fast: step-4 recursion with Release 0.65 ms;
- slow: towards lower targets a one-pole with time constant ≈ 99 ms (`slow ← h − (h − slow)·e^(−1/(0.099·fs))`),
  towards higher targets the step-4 recursion with Release ≈ 500–530 ms.

## 6. Requantisation
Applied to every output sample after §4/§5 (also when no limiting occurs), per channel:

- Word length b ∈ {24, 22, 20, 18, 16}; LSB `Δ = 2^(1−b)` (full scale ±1).
- Without dither and shaping: `y = Δ · round(v/Δ)` (round half away from zero or to even — ties are measure-zero).
  Measured exact for all word lengths, including 24 bit.
- Digital silence with Dither None stays exactly zero for every shaping mode.
- Output limited to the representable range (clip at the largest code) — not measured, implementer rule.

## 7. Dither and noise shaping
Error-feedback quantiser: `u = v − Σ e-history filtered`, `y = Δ·round((u + d)/Δ)`, `e = y − u`, so that the
total output error `y − v` equals the noise-transfer filter (NTF) applied to the white error `e`
(dither + rounding). NTF is monic (`h[0] = 1`), stable, minimum phase; its magnitude is specified in §7.3.

### 7.1 Dither types
- **Type 1**: TPDF dither of ±1 LSB peak (sum of two independent uniform ±½ LSB values), independent per
  channel, added always — including during digital silence. Without shaping: total error 0.50 LSB rms, white
  (flat within ±0.2 dB), values {−1, 0, +1} LSB with P(±1) = 12.5 % each.
- **Type 2**: lower-level dither. Without shaping: total error 0.41 LSB rms (RPDF ±½ LSB equivalent).
  With shaping: same NTF shape as Type 1 but the shaped error PSD is 4.8 dB (±1 dB) lower than Type 1's at every frequency and sample rate.
  On digital silence the output is exactly zero (all shaping modes).
- **None**: no dither (rounding only; shaped rounding error if shaping is on).

### 7.2 Noise-shaping mode totals (16-bit, 48 kHz)
| Mode | Type 1 total error (LSB rms) | Type 2 total error (LSB rms, −60 dBFS sine) |
|---|---|---|
| None | 0.50 | 0.41 |
| Moderate | 1.01 | 0.64 |
| Normal | 1.98 | 1.19 |
| Ultra | 3.92 | 2.27 |

### 7.3 NTF magnitude, dB (= PSD of Type-1 total error relative to white TPDF error), per sample rate
Values are the same for every word length (verified 16/20/24 bit). The shaping curve is designed per sample
rate (it is not a frequency-scaled copy).

48 kHz:

| f (kHz) | 0.1 | 0.5 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 | 13 | 14 | 15 | 16 | 17 | 18 | 19 | 20 | 21 | 22 | 23 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Moderate | +0.3 | −1.7 | −5.4 | −10.1 | −12.1 | −12.6 | −12.0 | −10.7 | −8.9 | −7.5 | −6.8 | −6.1 | −4.8 | −2.2 | +1.7 | +6.4 | +9.1 | +9.8 | +9.8 | +9.8 | +9.8 | +9.7 | +9.8 | +9.9 | +9.8 |
| Normal | −2.5 | −5.5 | −9.7 | −13.9 | −16.2 | −17.0 | −16.8 | −15.7 | −14.0 | −11.9 | −10.0 | −8.5 | −6.9 | −4.8 | −1.5 | +3.5 | +10.3 | +15.0 | +16.4 | +16.5 | +16.5 | +16.4 | +16.4 | +16.5 | +16.4 |
| Ultra | −5.5 | −8.6 | −12.1 | −16.3 | −18.8 | −19.6 | −19.5 | −18.6 | −17.0 | −15.1 | −13.2 | −11.4 | −9.7 | −7.6 | −4.6 | −0.2 | +6.4 | +16.1 | +21.5 | +23.0 | +23.1 | +22.9 | +22.8 | +23.0 | +23.0 |

44.1 kHz:

| f (kHz) | 0.1 | 0.5 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 | 13 | 14 | 15 | 16 | 17 | 18 | 19 | 20 | 21 | 22 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Moderate | +2.0 | −0.9 | −5.1 | −9.6 | −11.5 | −11.9 | −11.0 | −9.4 | −7.7 | −6.7 | −6.2 | −5.4 | −3.8 | −1.1 | +2.8 | +7.1 | +9.9 | +10.9 | +10.7 | +10.5 | +10.6 | +10.7 | +10.7 | +10.6 |
| Normal | −1.0 | −4.3 | −7.8 | −12.4 | −14.3 | −14.8 | −14.1 | −12.6 | −10.9 | −9.7 | −9.0 | −8.2 | −6.8 | −4.3 | −0.6 | +4.6 | +10.9 | +16.0 | +17.7 | +17.7 | +17.6 | +17.8 | +17.7 | +17.6 |
| Ultra | −2.2 | −5.8 | −9.7 | −14.3 | −16.5 | −16.8 | −16.1 | −14.7 | −13.0 | −11.8 | −11.2 | −10.5 | −9.1 | −6.7 | −3.1 | +2.0 | +8.8 | +17.3 | +22.8 | +24.3 | +24.2 | +24.3 | +24.4 | +24.3 |

96 kHz (noise moved above the audio band):

| f (kHz) | 0.1 | 1 | 2 | 4 | 6 | 8 | 10 | 12 | 14 | 16 | 18 | 20 | 22 | 24 | 26 | 28 | 30 | 32 | 34 | 36 | 38 | 40 | 42 | 44 | 46 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Moderate | −22.6 | −22.6 | −22.2 | −21.5 | −20.1 | −18.1 | −15.2 | −11.2 | −5.8 | +2.1 | +8.2 | +7.4 | +7.2 | +8.2 | +8.4 | +7.9 | +7.8 | +8.2 | +8.4 | +8.2 | +7.9 | +8.0 | +8.3 | +8.4 | +8.2 |
| Normal | −27.4 | −27.6 | −27.4 | −26.7 | −25.8 | −24.0 | −22.0 | −19.3 | −15.8 | −11.4 | −6.0 | +1.3 | +10.3 | +15.1 | +14.9 | +14.0 | +13.8 | +14.5 | +15.2 | +15.3 | +15.0 | +15.3 | +15.5 | +14.9 | +14.0 |
| Ultra | −31.0 | −30.6 | −30.5 | −30.0 | −28.9 | −27.4 | −25.3 | −22.7 | −19.4 | −15.2 | −9.5 | −1.6 | +13.9 | +16.5 | +12.1 | +14.0 | +17.7 | +19.6 | +19.6 | +18.6 | +17.6 | +17.8 | +18.9 | +19.4 | +19.2 |

Other rates: 88.2 kHz uses the 96 kHz curves at the same frequencies; 176.4/192 kHz: implementer's design that
keeps the NTF ≤ the 96 kHz curve below 20 kHz (open question 3).

## 8. Parameter changes
Not measured on the reference. Threshold/Ceiling changes must be click-free (smooth the static gain ≈ 20 ms);
Release/Adaptive changes take effect immediately; Word Length/Dither/Shaping switch at a block boundary
(the discontinuity is at LSB level).

## 9. Test vectors (48 kHz unless stated; Dither None, Shaping None, Word Length 24 unless stated)
- Neutral (Threshold = Ceiling = 0): impulse 0.25 passes bit-exact; latency L reported, output aligned.
- Static: 1 kHz sine staircase −40…+12 dBFS, Threshold −12, Ceiling 0: output = input·4 (12 dB make-up) below
  −12 dBFS input; every output sample ≤ 1.0.
- Threshold −12, Ceiling −3: below-threshold gain +9 dB; max |output| = 10^(−3/20).
- Click (one sample of 1.0 on a 0.01 DC carrier, Threshold −12, Release 10 ms): gain begins to fall exactly 64
  samples before the click, reaches 0.2512 one sample before it; S-shaped (two boxcars 38, 27).
- Two clicks 2.0 then 1.0 separated by 64 samples: the recovery after the first targets the second's required
  gain (§4 step 4).
- fs/4 sine, sample peaks 1.06 (amplitude 1.5, 45° phase), Threshold 0: output sample peaks exactly 1.0.
- Fixed-release recovery after a 0.5 s DC overload (12 dB over): t63 / t90 / t99 = 0.75/1.02/1.21 ms (0.01 ms),
  1.15/1.71/2.71 (1 ms), 4.98/10.62/20.62 (10 ms), 44.1/100.6/200.6 (100 ms), 435/1000/2001 ms (1000 ms).
- Adaptive Release: the §5 table.
- Requantisation without dither: slow ramp ±4 LSB(b) → output equals `Δ·round(v/Δ)` exactly, for every b.
- Dither Type 1, no shaping, digital silence, 16 bit: output ∈ {−1, 0, +1}·Δ, 0.50 Δ rms, P(±Δ) = 12.5 % each.
- Dither Type 2, any shaping, digital silence: output exactly 0.
- Shaped spectra: §7.3 per mode and rate.

## 10. Tolerances
- Limiter, fixed release: output (with requantisation bypassed) vs §4 ≤ −100 dB re peak excluding the first L
  samples after stream start; with Release ≥ 300 ms or gain reduction > 24 dB ≤ −85 dB (precision note §4).
- Ceiling: output sample peaks never exceed Ceiling + requantisation error (≤ 1 LSB + dither peak).
- Adaptive Release: each t63/t90/t99 in §5 within ±25 % (t99 of the 5 ms row within ±35 %); recovery monotonic.
- Requantisation without dither: exact.
- Dither Type 1/2 totals (§7.1/§7.2) within ±0.05 LSB rms (no shaping) and ±10 % (shaped).
- NTF magnitude: each §7.3 point within ±1.5 dB (±2.5 dB above 20 kHz at 96 kHz); integrated in-band (≤ 15 kHz)
  noise power within ±1 dB of the table's integral.
- Latency exactly §3 and reported to the host.

## 11. Behaviour not specified / out of scope
External sidechain variant; meter ballistics; true-peak limiting (the reference has none); exact Adaptive
Release law (only §5's table); behaviour in the first L samples after stream start.

## 12. Review decisions (r1, standing policy: match the reference, do not reproduce clear defects)
- **D1** Ceiling enforcement `min(s, r)` reproduced (the reference already never exceeds the ceiling).
- **D2** Adaptive Release ignores the Release control — reproduced (it is the reference's documented intent:
  automatic release); the UI greys out Release while it is on.
- **D3** NOT reproduced: with Type 2 dither + Ultra shaping the reference emits a deterministic idle pattern
  (≈ 0.9 LSB rms tone-like signal at 18–24 kHz) on digital silence, and Type 2 with constant DC input produces
  deterministic limit-cycle patterns. These are noise-shaper limit cycles (defects). Required instead: digital
  silence → exactly zero for Type 2 in every shaping mode; no idle tones for constant input (Type 2 may add a
  minimal amount of dither when the input is non-zero to break limit cycles, within the §7 level tolerance).
- **D4** Precision: double-precision release state permitted (differs ≤ 2e-5 from the reference's float32).
- **D5** Default settings match the reference (Adaptive on, 24 bit, Type 1, Normal) — note this means the default
  output is dithered, not bit-exact.

## 13. Open questions
1. Exact Adaptive Release law (§5 table + recommended realisation only).
2. Type 2 dither mechanism (only level/spectrum and silence behaviour specified; it is signal-dependent in the
   reference).
3. Noise-shaping curves at 176.4/192 kHz (not measured).
4. Output clipping after requantisation at full scale (not measured; implementer rule in §6).
5. Mono and external-sidechain variants not measured.

## 14. User interface (functional layout — visuals must be original)
Same controls, arrangement and semantics as the reference; all artwork, colours, fonts, logos and names original,
drawn in code.
- Left: input level meters (L/R, dB scale 0 … −30) with a **Threshold** marker/slider beside them (drag the
  marker; value box "−12.0").
- Centre: **Gain-reduction** meter (vertical, 0 … −30 dB, "Atten" peak readout).
- Right: output meters (L/R) with an **Out Ceiling** marker/slider and value box; peak-hold readouts that reset
  on click.
- Below: **Release** value box/slider (ms, two decimals; shown inactive when Adaptive is on) and an
  **Adaptive Release** toggle; **Word Length** selector (24/22/20/18/16), **Dither** selector (None/Type 1/
  Type 2), **Shaping** selector (None/Moderate/Normal/Ultra).
- Optional link of Threshold and Ceiling markers (drag both together keeping the offset), double-click resets a
  control to its default.
