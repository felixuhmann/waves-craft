# Five-band dynamic harmonic enhancer — behavioural specification (approved r1)

Status: APPROVED r1 (2026-10-29), frozen. Review decisions in §7. Tables in `tables/` are normative.

Derived only from black-box measurement of the licensed reference processor in a VST3 host (REAPER, offline renders,
user-generated stimuli), host-visible parameter metadata and the reference's user documentation. No code,
disassembly or vendor assets were used. Tables in `tables/` (next to this file) are normative.
Variants: **Stereo** (primary, measured) and **Mono** (one channel, no width controls; assumed identical per channel).

## 1. Parameters (host-normalised value r ∈ [0,1])

| Parameter | Mapping | Range / display | Default |
|---|---|---|---|
| Input gain | −18 + 36·r dB | −18…+18 dB, 0.1 dB steps (display "0.0dB") | 0 dB |
| Output gain | −18 + 36·r dB | −18…+18 dB | 0 dB |
| Direct gain | r = 0: Off (mute); else −48 + 48·r dB | Off, −46.8…0 dB | 0 dB |
| Band A…E gain | r = 0: Off (no processed signal); else 48·r dB | Off, 1.2…48 dB | Off |
| Crossover 1…4 | log-like taper, `tables/crossover_taper.csv` (16 Hz at r=0, 578 Hz at r=0.5, 21357 Hz at r=1) | 16…21357 Hz (each limited by its neighbours) | 90 / 500 / 2594 / 9310 Hz |
| Band A…E width (Stereo) | w = r + 2r² | 0…3, 0.01 steps | 1.00 |
| Punch | 5·r | 0…5, 0.1 steps | 3.0 |
| Band A…E Solo, Band A…E Bypass | toggles | Off/On | Off |

Crossovers keep their order: a crossover cannot be set below its lower or above its upper neighbour.

## 2. Signal flow (exact in structure)

```
x_in = x · 10^(InputGain/20)
direct  = Σ_bands  W_b( Band_b(x_in) )                          · DirectGain
process = Σ_bands  W_b( BandGain_b · P_b( Band_b(x_in) ) )      (band b contributes only if its Gain is not Off
                                                                 and its Bypass is off)
y = (direct + process) · 10^(OutputGain/20)
```

- `Band_b` are the five crossover bands (§3). `W_b` is the per-band width matrix (§4), applied to both parts.
- `P_b` is the per-band processing (§5): a level-dependent, time-varying gain plus mild harmonic generation.
- The two paths add exactly: output = direct-only output + processed-only output (measured residual ≤ −150 dB re peak).
- Band Bypass removes that band's processed contribution exactly (the direct part is unchanged, bit-identical).
- Solo: only the soloed band(s) are heard, both their direct and processed parts (several solos add).
- With all band gains Off and Direct 0 dB, the output is the crossover allpass of the input (§3): unit magnitude,
  non-zero phase. Direct Off with all gains Off is silence.
- Latency: 0 samples (nothing reported to the host; the allpass phase is part of the response).
- Output is not clipped (floating point).

## 3. Crossover (table-normative)

Five bands from four crossover frequencies, designed at the true sample rate (band levels at 44.1, 48 and 96 kHz
agree within 0.1 dB above −60 dB; the group delay in ms is identical).

- **Complementarity (exact):** the sum of the five bands is an allpass (|Σ| = 0.000 dB at all frequencies).
- **Band magnitudes (normative):** `tables/xover_bands_default.csv` (crossovers 88/494/2576/9285 Hz) and
  `tables/xover_bands_set2.csv` (197/990/3978/11977 Hz), 61 log-spaced points 20 Hz–20 kHz, at 48 kHz.
  Tolerance: ±0.5 dB where the band is above −20 dB, ±1.5 dB down to −40 dB, ±4 dB down to −60 dB; below −60 dB only
  "≤ −55 dB" is required.
- **Shape scaling:** the band shapes scale with the crossover frequencies (the first-split low-pass, normalised to
  x = f/fc, is the same at fc = 88 Hz and 197 Hz within 0.1 dB above −40 dB): `tables/lp_prototype.csv`. At a crossover
  frequency the adjacent bands are each ≈ −7.0…−7.5 dB; far from the crossover the outer slopes approach 48 dB/octave,
  while each band's side towards an inner neighbour is gentler (e.g. the middle band falls ≈ 12 dB/oct below its
  lower crossover).
- **Direct-path phase (normative):** group delay of the summed bands at crossovers 88/494/2576/9285 Hz,
  `tables/direct_group_delay.csv` (≈ 317 samples @ 30 Hz, 116 @ 200 Hz, 27 @ 1 kHz, 4 @ 9.3 kHz at 48 kHz),
  tolerance ±10 % (±1 sample above 5 kHz).
- **Informative structure** (closest description found; not exact): cumulative low-passes `C_k` at each crossover
  (4th-order-like prototype of `lp_prototype.csv`), all mutually phase-aligned with allpass compensation, bands
  `B_1 = C_1`, `B_k = C_k − C_{k−1}`, `B_5 = AP − C_4`. The running sums of the measured bands reproduce the prototype
  within ≈ 0.6 dB at the crossovers. Any implementation meeting the tables and exact complementarity is acceptable.

## 4. Width (exact)

Per band, on that band's mid/side signals (M = (L+R)/2, S = (L−R)/2), for both the direct and the processed part:

```
w ≤ 1:  M' = M,      S' = w·S
w > 1:  g = sqrt((5 + 3/w²)/8);  M' = g·M,  S' = g·w·S
```
(w = 3 → M ×0.8165, S ×2.4495; w = 2 → M ×0.8478, S ×1.6956; measured to 4 decimals at 50 Hz–15 kHz, 44.1/48/96 kHz.)
Different widths per band act only inside that band's frequency range (e.g. band C width 0 leaves S ×0.18 at 1 kHz
and S ×0.99 at 50 Hz with default crossovers).

## 5. Band processing P_b (table-normative)

The processed part of a band is that band's signal multiplied by a level-dependent, time-varying gain and lightly
distorted, then scaled by the band gain. Channels share one gain per band (linked).

### 5.1 Gain scaling (exact)
- Band gain: every dB of band gain adds exactly 1 dB to the processed band (12/24/36/48 dB measured, ±0.05 dB).
- Input gain shifts the band level seen by `P_b` by exactly its value (the processed output follows §5.2 at the
  shifted level); Output gain is a plain output multiplier.

### 5.2 Settled level curve (normative)
After ≥ 2 s at a constant level, the processed band gain `Gp(L)` (output fundamental / input fundamental, dB, band gain
24 dB) depends on the input level L, Punch and the band: `tables/settled_curves.csv` (sines at band centres A 45 Hz,
B 200 Hz, C 1100 Hz, D 5000 Hz, E 14000 Hz; Punch 0/3/5; L = −70, −50, −40, −30, −20, −10, −3 dBFS; crossovers
88/494/2576/9285 Hz). Shape: constant within ≈ 0.5 dB from −20 dBFS up; rising as the level falls (upward compression,
e.g. band C Punch 3: −23.5 dB at −3 dBFS, −21.3 at −40, −18.0 at −50, −9.8 at −70); bounded at very low levels.
Punch acts mainly on the upper bands' level (band E at −10 dBFS: Punch 0 −45.5, 3 −33.7, 5 −32.3 dB; band C: −24.8,
−23.5, −23.9 dB) and on the low-level boost. Between tabulated levels interpolate linearly in dB; between bands use the
band whose crossover range contains the frequency. Tolerance ±1.0 dB (±2 dB at −70 dBFS).

### 5.3 Dynamics (normative trajectories)
- After a level **drop** the processed gain rises well above its new settled value and recovers over ≈ 1–2 s.
- After a level **rise** the gain reaches its settled value within ≈ 0.2 s.
- Normative trajectories: `tables/band_steps.csv` (each band at its centre frequency, band gain 24 dB, Punch 0/3/5,
  sequence −10 dBFS (2 s) → −40 dBFS (3 s) → −10 dBFS (2 s); gain sampled 5/20/50/100/200/500/1000/2000 ms after each
  change). Tolerance ±2 dB (±4 dB within 50 ms after a change; at 45 Hz the gain is evaluated over one 22 ms period).
- Informative: the behaviour is consistent with a gain driven by a slow level detector that releases in the linear
  amplitude domain with τ ≈ 0.5 s and attacks within ≈ 0.1–0.2 s, combined with a fast detector; a large slow/fast
  difference raises the gain. No single closed form reproduced all trajectories; the tables are the acceptance criterion.

### 5.4 Harmonics (normative)
Harmonic levels relative to the processed fundamental are independent of input level between −30 and −3 dBFS and
of band gain; `tables/harmonics.csv` gives H2…H5 (dBc) per band for in-band tones. Typical: band A at 50 Hz H2 −36,
H3 −42 dBc; band C 1 kHz H2 −60, H3 −66 dBc; band D/E 4 kHz H2 −71, H3 −76 dBc. Harmonics rise towards low
frequencies. Tolerance ±6 dB (only where the entry is above −90 dBc); entries for tones far outside a band are
informative.

## 6. Sample rate
Crossover (in Hz) and width are sample-rate independent. Processed-path timing and levels are specified in seconds /
dB as measured at 48 kHz; 96 kHz agrees within 0.5 dB.

## 7. Review decisions (standing policy: match the reference; do not reproduce defects; original visuals; own naming)
- **D1** At 44.1 kHz the reference's processed path is ≈ 4 dB hotter than at 48 and 96 kHz for the same signal
  (time constants apparently derived for the 48 kHz family). Not reproduced: the processed path must behave the same
  (in seconds and dB) at all sample rates.
- **D2** Parameter changes are smoothed click-free within 20 ms (the reference applies some changes asynchronously).
- **D3** Crossovers and band shapes are table-normative because no exact closed form was identified (§3).
- **D4** Dynamics and settled curves are table-normative (§5.2, §5.3).
- **D5** The Mono variant is the same processing on one channel without width controls (not measured).
- **D6** No output limiting or clipping (as the reference).

## 8. Tolerances (summary)
Exact items (§2 additivity/bypass, §4 width, §5.1 scaling): ≤ −100 dB re peak or ±0.05 dB. Tables: as stated in §3/§5.
Neutral settings (all gains Off, Direct 0 dB, widths 1): magnitude 0 ± 0.01 dB at every frequency.

## 9. User interface (functional layout — visuals must be original)
- Top: a spectrum-style display of the five band regions with draggable crossover handles; four numeric crossover
  boxes underneath (labels LO / LM / HM / HI).
- Left: Input gain knob (−18…+18 dB) with input meters (L/R) and peak readouts.
- Centre: a Direct fader (Off, −48…0 dB) followed by five band strips A–E, each with Solo and Bypass buttons, a vertical
  band-gain fader (Off, 0…48 dB) with value box and (Stereo) a width knob (0…3) with value box.
- Right: Punch value box (0…5); Output gain knob (−18…+18 dB) with output meters, peak readouts and a Trim readout.
- Linked-fader behaviour (moving all band faders together with an offset) is optional.
- No vendor names, logos, textures or colours of the reference.

## 10. Open questions
1. Exact crossover filter structure (tables given).
2. Exact dynamic law of the processed path (tables given).
3. Exact harmonic-generation mechanism (dBc tables given).
4. Behaviour of the processed path with multiple simultaneous tones / programme material (only single tones measured).
5. Mono variant; rates above 96 kHz.

## 11. Public references (for the implementer)
Linkwitz/Riley and allpass-complementary crossover literature; RBJ audio-EQ cookbook; Zölzer (ed.), "DAFX" (dynamics,
exciters/enhancers); Giannoulis, Massberg, Reiss, "Digital dynamic range compressor design", JAES 60(6), 2012; Parker,
Zavalishin, Le Bivic, "Reducing the aliasing of nonlinear waveshaping using continuous-time convolution", DAFx 2016.
