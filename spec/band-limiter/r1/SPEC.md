# Five-band peak-limiting maximiser (linear-phase and low-latency engines) — behavioural specification (r1)

Status: APPROVED r1 (2026-11-01), frozen; review decisions in §14. Derived only from black-box measurement of a licensed reference (audio in/out and host-visible
parameter metadata) and its public user documentation. Functional facts only; no code, no vendor assets.
Tables in `tables/*.csv` (LF line endings) are normative where stated; their SHA-256 values are listed in `SHA256`.

## 1. Scope and variants
One engine, four products (stereo; mono = same processing on one channel, not measured):

| Variant | Crossover engine | Interface |
|---|---|---|
| **LP-Multi** | linear-phase | full per-band controls (§3) |
| **LP-Simple** | linear-phase | global controls + Profile selector (§9) |
| **LL-Multi** | low-latency, phase-compensated | full per-band controls, no Release-behaviour selector |
| **LL-Simple** | low-latency, phase-compensated | global controls + Profile selector |

Signal flow (all variants): input → 5-band split (§4) → per-band Gain → peak-limiting mixer (§5–§7: one limiter
that sets a gain per band so that the *sum* of the bands never exceeds the threshold) → sum → make-up and
Out Ceiling (§5) → requantiser with dither / noise shaping (§10).

## 2. Latency
- LP variants: constant pure delay, reported to the host: **3528 samples** (= 80 ms at 44.1 kHz) as measured at
  44.1/48/96 kHz. Implementations may report the latency of their own linear-phase design if it is ≥ the
  look-ahead they need; the reported value must equal the actual delay exactly (host delay compensation).
- LL variants: reported latency **64 samples** (look-ahead). Actual delay of an impulse at neutral settings is the
  reported value plus the frequency-dependent group delay of the common allpass (§4.2).
- Neutral settings (Threshold 0, Ceiling 0, all band gains 0, no over, Quantize 24 / Dither None / Shaping None):
  LP output = input delayed, exact to 24-bit rounding (measured bit-exact on the test impulses).

## 3. Parameters (host-normalised value r ∈ [0,1])
| Parameter | Mapping | Range | Default |
|---|---|---|---|
| Threshold | −30 + 30 r dB | −30…0 dB | 0 |
| Out Ceiling | −30 + 30 r dB | −30…0 dB | 0 |
| Band n Gain (n = 1…5) | −12 + 24 r dB | ±12 dB | 0 |
| Band n Priority | −12 + 24 r dB | ±12 | 0 |
| Band n Release | 0.01 · 250000^r ms | 0.01…2500 ms | 598.66 / 503.04 / 402.20 / 298.46 / 200.52 ms (bands 1…5) |
| Band n Solo | toggle | | off |
| Low / LowMid / HighMid / High crossover | 16 · 1334.8^r Hz, **clamped** to 40–350 / 150–3000 / 1022–4757 / 4000–16000 Hz | | 80 / 320 / 1278 / 5113 Hz |
| Separation | 100 r % | 0…100 % | 100 % |
| Release behaviour (LP-Multi only) | 5 steps: Adaptive (default), Aggressive, Warm, Scaled, Manual at r = 0, ¼, ½, ¾, 1 | | Adaptive |
| Release multiplier (Simple only) | 0.1 · 100^r | ×0.1…×10 | ×1.00 |
| Profile (Simple only) | 9 steps (§9) | | Profile 1 |
| Quantize | 24 / 22 / 20 / 18 / 16 bit at r = 0, ¼, ½, ¾, 1 | | 24 |
| Dither | None / Type 1 / Type 2 (r ≈ 0 / 0.5 / 1) | | Type 1 |
| Shaping | None / Moderate / Normal / Ultra (r = 0, ⅓, ⅔, 1) | | Normal |

The LL-Multi variant has no Release-behaviour selector; it behaves as Adaptive.
Linked Threshold/Ceiling movement and Gain/Priority linking are UI conveniences (§13).

## 4. Band split
Five bands b = 1…5 separated by the four crossover frequencies f₁ < f₂ < f₃ < f₄. With all band gains at 0 dB and no
limiting, the bands sum to the input exactly (LP: pure delay; LL: common allpass, flat magnitude).

### 4.1 Linear-phase engine (LP)
Zero-phase (after the constant delay) bands defined from four low-pass shapes Lₖ(f), k = 1…4:
`band₁ = L₁, band_b = L_b − L_{b−1} (b = 2…4), band₅ = 1 − L₄` — magnitudes real and non-negative, summing to 1.
Lₖ(f) is a function of the octave offset o = log₂(f / fₖ) only (normative table `tables/xo_linear_lowpass.csv`,
columns = measured crossover frequencies):
- For crossovers 40 Hz … 3 kHz use the shape of the 320/1000/1278 Hz columns (they agree within 0.006);
  values: L(−2) ≈ 0.96, L(−1) ≈ 0.853, L(−0.5) ≈ 0.723, L(0) ≈ 0.513, L(+0.5) ≈ 0.242, L(+1) ≈ 0.034, L(+1.5) ≈ 0.
- The High crossover becomes steeper as its frequency rises: use the 5113 Hz and 12000 Hz columns, interpolated
  linearly in log₂(f₄) between them (and the standard shape at f₄ ≤ 3 kHz). The shape is defined in Hz, not relative
  to Nyquist: band responses at the default crossovers agree across 44.1/48/96 kHz within about 1 dB.
- Tolerance: |L_impl − L_table| ≤ 0.02 for every table point.

### 4.2 Low-latency engine (LL)
All five bands share one common allpass phase A(f): `band_b(f) = A(f) · M_b(f)` with real magnitudes M_b ≥ 0 and
ΣM_b = 1 (the bands are in phase with each other at every frequency). Normative magnitudes at the default crossovers
and the measured group delay of A at 48 kHz: `tables/xo_lowlatency_bands.csv`. For other crossover settings the
magnitudes scale with fₖ in octave terms. Tolerance: magnitudes ±0.03 absolute; A's group delay is informative
(any allpass that keeps the bands in phase and the sum flat within ±0.05 dB is acceptable; the reference's is
≈ 372 samples at 20 Hz, 315 at 80 Hz, 96 at 320 Hz, 24 at 1.28 kHz, 1.3 at 10 kHz).

### 4.3 Solo
Solo of one or more bands outputs only the soloed bands (after their gain and the limiter).

## 5. Static gain, make-up, ceiling
- Below the limiting point the output is `y = x · 10^((Gain_b − T + C)/20)` per band, summed (T = Threshold,
  C = Out Ceiling, both ≤ 0): make-up equals −T.
- The output sample peak never exceeds the ceiling: |y| ≤ 10^(C/20) for every sample (measured on clicks, tone
  steps, bursts, two-tone and noise programme). No inter-sample (true-peak) processing: an fs/4 sine with sample
  peaks −3 dBFS and T = −6 dB comes out with sample peaks exactly at 0 dBFS.
- Static staircases (single tone, T −12, C 0): output = input + 12 dB up to the limiting point, then held at
  sample-peak 0 dBFS. Tone amplitude above the limiting point may deviate by up to +1.3 dB / −1.3 dB from the
  sample-peak-limited value at low frequencies (band-gain ripple at the tone frequency, informative).

## 6. Peak-limiting mixer (distribution of attenuation across bands)
Normative behaviour = the measured cases in `tables/distribution_two_tone.csv` and `tables/distribution_multitone.csv`
(steady-state output level of each tone, and output peak), tolerance **±1.0 dB per tone** and output peak per §5.
Facts the implementation must reproduce qualitatively and that the tables quantify:
1. One limiter for all bands: the attenuation of each band is chosen so the sum of all bands stays at or below the
   threshold; with equal priorities, bands carrying equal overs get similar attenuation (two equal tones in bands 2
   and 4: 12.2 / 11.5 dB at T −12).
2. **Priority** shifts attenuation away from a band towards the others, with a saturating effect (band-2 priority
   −12…+12 moves the 100 Hz tone from −7.7 to −3.6 dBFS and the 2 kHz tone from −4.3 to −8.4 dBFS).
3. **Separation** mixes the band detectors: at 100 % each band's detector sees only its band; at 0 % every detector
   sees all bands (near-equal attenuation, wideband-like). Intermediate values over-attenuate (output peak 0.7–1.0 dB
   below the ceiling at 25–75 %) — reproduce as measured.
4. **Band gain** is applied before the limiter: in limiting, extra gain on a band raises its level less than 1:1.
5. Distribution depends on the over amount and on each band's release (bands with slower release keep more
   attenuation on periodic material): see the T −6 / −12 / −18 rows.
An informative structure that reproduces these facts is given in Appendix A.

## 7. Dynamics
- Attack: look-ahead; no output sample exceeds the ceiling (§5). Gain reduction may start before the over by at most
  the look-ahead.
- Release: per band, from the band's Release value and the Release behaviour, with program-adaptive shortening after
  short overs (Adaptive/Aggressive/Warm/Scaled) and no adaptation in Manual. Normative: `tables/release_steps.csv`
  (tone step −24 → 0 → −24 dBFS, T −12; gain-reduction depth during the over and t63/t90/t99 of the recovery) and
  `tables/release_bursts.csv` (recovery after 20 ms vs 200 ms bursts). Tolerance: times ±25 % or ±20 ms (whichever
  is larger); trajectory points ±1.5 dB.
- Facts: Manual recovery does not depend on burst length; Adaptive recovers much faster after a short (20 ms) burst
  than after a long one; Warm and Scaled are slower than Adaptive in band 2 and slightly faster in band 5; a band's
  Release value scales its recovery (10 / 100 / 1000 ms → t63 81 / 157 / 467 ms in Manual).
- Release multiplier (Simple variants) multiplies all band Release values of the selected profile.

## 8. Low-latency engine dynamics
Static behaviour, ceiling and release as §5–§7 within the same tolerances; the LL rows of the tables apply where given.

## 9. Profiles (Simple variants)
Nine profiles, each a complete set of the LP-Multi controls (band priorities, releases, crossovers, Separation,
Release behaviour). Normative behaviour: `tables/profiles.csv` (two-tone output levels and 1 kHz step release per
profile), tolerances as §6/§7. Profile 1 equals the LP-Multi defaults exactly. Names are the implementer's own; the
measured character is:

| # | Character (informative) |
|---|---|
| 1 | Default (equals Multi defaults) |
| 2 | Slower adaptive release |
| 3 | Partly merged detectors (lower separation → deeper attenuation), slow release |
| 4 | Wideband-like, adaptive |
| 5 | Wideband-like, manual |
| 6 | Warmer balance (lows favoured) |
| 7 | Default balance, slower release |
| 8 | Highs favoured |
| 9 | Strongly lows favoured (for low-level loudness) |

The Profile parameter maps r to profile p = floor(8·r + 0.5) + 1 (p = 1…9; measured on a 1/32 grid).

## 10. Requantisation, dither, noise shaping
Identical to the single-band mastering limiter of this project (`spec/mastering-limiter/r2` §6–§7): measured
16-bit totals here — Type 1: None 0.50, Moderate 1.01, Normal 1.98, Ultra 3.93 LSB rms; Type 2: None 0.41,
Moderate 0.65, Normal 1.18, Ultra 2.28 LSB rms; None (rounding only): 0.29 / 0.58 / 1.15 / 2.26 LSB rms; Type 1
adds dither during digital silence, Type 2 outputs exactly zero on digital silence; word lengths 24/22/20/18/16;
rounding when Dither and Shaping are None. The quantiser cannot be bypassed (24 bit is the minimum processing);
the NTF tables of that spec apply unchanged. Tolerances as in that spec.

## 11. Open questions
1. The exact rule that steepens the High crossover (two measured settings only).
2. The exact distribution optimiser and adaptive-release law (table-normative).
3. Crossover behaviour when crossovers are set out of order (implementer's choice; must stay stable and sum flat).
4. Mono variants not measured.
5. Reported LP latency at rates other than 44.1 kHz (the host query returned 3528 samples at all rates).

## 12. Sample-rate behaviour
Measured at 44.1/48/96 kHz on the defaults, LP-Multi and LL-Multi: times are in ms and the same at each rate. 1 kHz step
release t63/t90/t99: Adaptive 231/409/495 ms (44.1), 233/415/499 (48), 226/407/492 (96); Manual 307/499/642, 309/503/647,
306/496/642. Ceiling holds at every rate. Two-tone rows may differ by up to ±0.75 dB between rates (T −12, 100 Hz / 2 kHz:
−5.79/−5.84 at 44.1, −6.15/−5.49 at 48, −5.41/−6.10 at 96), so apply the §6 tables at 48 kHz and ±1.5 dB elsewhere.

## 13. User interface (functional layout — visuals must be original)
Multi: Threshold and Out Ceiling faders with input/output meters and a link handle between them; attenuation meter
0…30 dB; a gain/frequency display showing the five bands' dynamic gain (live line, smoothed trail, peak-hold line) with
draggable band markers (horizontal = crossover, vertical = linked gain/priority) and four crossover markers; per band:
Solo, Gain, Priority (with a gain/priority link toggle), Release, band meter with the band's relative-threshold
marker; crossover value boxes and Separation; Release behaviour selector + master release offset; Quantize / Dither /
Shaping selectors with an "active" indicator. Simple: Threshold/Ceiling/attenuation as above, Profile selector,
Release multiplier, Quantize / Dither / Shaping. Original artwork, colours and names only.

## 14. Review decisions
- D1: Reproduce the measured behaviour including Separation's over-attenuation at intermediate values (it is
  documented as intended detector mixing, not a defect).
- D2: No true-peak limiting (the reference has none); exact sample-peak ceiling.
- D3: Crossover settings are clamped to the documented ranges (the reference clamps them).
- D4: Profile names are the implementer's own (the reference's preset names are not reused).
- D6: Mono variants are assumed to be the same processing on one channel (not measured).
- D5: The quantiser stage is shared with the mastering-limiter spec (identical measurements).

## Appendix A — informative structure
Per sample, with look-ahead N: band signals y_b = G_b·x_b; per-band detector d_b = Σ_j S_bj·peak_j over the look-ahead
window, where S = s·I + (1−s)/5·𝟙 (s = Separation); share weights w_b = 10^(κ·P_b/20) with κ ≈ 0.3–0.5 (saturating);
target band gains g_b chosen so that Σ_b g_b·d_b ≤ θ (θ = threshold), distributing the required reduction in
proportion to w_b⁻¹; smooth each g_b with the look-ahead attack smoother and a per-band release whose time constant
follows Release_b and the adaptive rule; finally a wideband look-ahead safety stage guarantees |y| ≤ ceiling.
