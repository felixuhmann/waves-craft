# Optical leveling amplifier — behavioural specification (candidate r1)

Status: APPROVED r1 (2026-10-29), frozen. Review decisions in §10. Derived only from black-box measurement of a licensed reference plug-in (user-generated test
signals, host-visible parameter metadata, 44.1/48/96 kHz) and public documentation. No code, disassembly or vendor
assets were used or are included. Names, artwork and branding are the implementer's own.
Normative tables are in `TABLES.md` (T1–T8) and `TABLES_extra.md` (T0, T7b) next to this file; they are part of the
specification.

## 1. Scope and variants
A stereo leveling amplifier modelled on a vintage optical (electro-luminescent panel + photo-resistor) gain cell with
tube make-up stages. Variants: **Stereo** (two audio paths, one shared detector — §6) and **Mono** (one path; assumed
identical to one channel of Stereo, not measured).

## 2. Parameters

| Parameter | Range | Default | Notes |
|---|---|---|---|
| Gain | 0–100 (display units, 0.01 resolution) | 40.00 | make-up; law T2; 32.28 = unity; host taper T0 |
| Peak Reduction | 0–100 (display units) | 46.00 | sets the static curve (T1); host taper T0 |
| Mode | Compress / Limit | Compress | |
| Hi Freq | 0–100 (0.1 steps) | 50 | sidechain weighting; 100 = flat (§5) |
| Analog | Off / 50 Hz / 60 Hz | Off (host default) | noise + mains hum (§8) |
| Mix | 0–100 % | 100 % | linear wet/dry crossfade (§7) |
| Trim | −18…+18 dB (0.1 dB) | 0 dB | exact output gain |
| Auto Makeup | Off / On | Off | UI coupling Peak Reduction → Gain (§9) |
| VU display | In / GR / Out | GR | metering only, no audio effect |

Gain and Peak Reduction labels are not dB: they are knob-scale units with the hardware's non-linear scaling.
Implementations must reproduce the display↔normalised mapping of T0 so host automation behaves alike.

## 3. Signal path
```
in ─┬───────────────────────────────────────────────────────────┐ (dry, linear, delay-free)
    └─► gain cell g(t) ─► make-up stage (Gain, T2) ─► tube colour (T7/T7b) ─► response (T8) ─► Mix ─► Trim ─► + Analog noise ─► out
         ▲
 sidechain: in ─► Hi Freq weighting (§5) ─► level detector (§6) ─► static curve (T1) ─► cell dynamics (§4)
```
- Latency 0 samples. Behaviour is sample-rate independent (static ≤ 0.03 dB, trajectories ≤ 0.02 dB between
  44.1/48/96 kHz); all times are in seconds.
- With Peak Reduction 0 the cell is transparent; at unity Gain (32.28) the path gain is −0.05 … +0.06 dB
  (level-dependent, T7) and the response follows T8.
- The detector is fed from the plug-in input (feed-forward), not from the output.

## 4. Gain cell: static curve and dynamics

### 4.1 Static curve (normative: T1C, T1L)
Steady-state gain reduction GR (dB) for a steady 1 kHz sine of peak level L (dBFS), Hi Freq 100, is given by T1C
(Compress) and T1L (Limit) for Peak Reduction rows 20…100. Interpolate linearly in L between columns and linearly in
Peak Reduction display units between rows; below the lowest tabulated non-zero row GR = 0. Characteristics (informative):
- Threshold (GR = 1 dB) moves from about −6.5 dBFS (PR 40) to −49 dBFS (PR 100); PR ≤ 30 gives no reduction below 0 dBFS.
- Compress: soft knee over ≈ 6 dB, then ≈ 0.763 dB of GR per dB (≈ 4.2 : 1), max ≈ 34 dB.
- Limit: same threshold, ≈ 0.915 dB/dB (≈ 12 : 1), GR saturating near 31 dB.

### 4.2 Dynamics (normative: T3)
GR trajectories for bursts of 20 ms…5 s at two depths and three settings are tabulated in T3 (attack: time after
onset; release: time after the burst ends, programme continuing at −50 dBFS). Properties:
- Attack is fast and drive-dependent: deeper targets are reached faster (≈ 50 % of target GR in 5 ms at a 12.6 dB target,
  ≈ 34 % at a 5 dB target); the last ≈ 5–10 % creeps in over 0.1–1 s.
- Release is two-stage and programme-dependent: a fast part (≈ 0.25 s) followed by a slow part (seconds) whose share
  grows with how long the reduction was held (after a 20 ms burst the 1 s residual is ≈ 0.5 dB, after ≥ 2 s ≈ 1.45 dB
  for a 12.6 dB burst). Full recovery takes ≈ 10–20 s.
- In the linear attenuation domain a = 1 − g the release shape is independent of depth.

### 4.3 Reference cell model (informative; meets T3 within §11 tolerances)
State: N = 3 components c_i ≥ 0 (attenuation fractions), total attenuation a = Σ c_i, cell gain g = 1 − a.
Target attenuation a_t = 1 − 10^(−GR_static/20), GR_static from §4.1 for the current detector level.
Per sample (sample period Δ):
```
for each i:
    target_i = w_i · a_t
    if target_i > c_i:  tau = τa_i · (0.6 / a_t)^β      # charge (attack), faster for deeper targets
    else:               tau = τr_i                        # discharge (release)
    c_i += (target_i − c_i) · (1 − exp(−Δ / tau))
```
Constants: w = (0.7805, 0.1381, 0.0814); τa = (4.90 ms, 23.1 ms, 110 ms); τr = (0.264 s, 2.75 s, 5.38 s); β = 1.26.
Implemented literally this reproduces the 600 T3 points with RMS 0.28 dB, max 0.94 dB. Any implementation meeting T3
within tolerance is conforming.

## 5. Sidechain weighting — Hi Freq (normative: T4, T5)
Hi Freq shapes the detector's frequency sensitivity; 100 = flat (equal reduction at all frequencies, within T4's
100 row), lower settings reduce sensitivity mainly in the 100 Hz–2 kHz region (by ≈ 9–10 dB at 200 Hz–1 kHz for
Hi Freq 0) while 50 Hz and ≥ 5 kHz are reduced less. T4 gives steady-state GR per frequency (−10 dBFS sine, PR 50) for
Hi Freq 0/25/50/75/100; T5 gives 1 kHz static GR vs level for Hi Freq 0/25/50/75/100 at PR 60. Interpolate linearly in
Hi Freq. The weighting affects the detector only; the audio path is unaffected.

## 6. Level detector and stereo linking
- Detector type (normative: T6): equal-RMS white noise receives less reduction than a steady sine (by ≈ 2 dB at −10 dBFS
  RMS in Compress, ≈ 4 dB in Limit) — i.e. an averaging (rectified-average-like) detector, calibrated so that a steady sine
  of peak L yields T1. The detector's fast response lets gain follow low-frequency waveforms, which produces the
  low-frequency distortion under compression in T7 (e.g. 100 Hz, PR 50, −10 dBFS: H3 ≈ −41 dBc).
- Stereo: one detector for both channels; both channels always receive identical gain (a −50 dBFS channel next to a
  −10 dBFS channel gets the same GR as the loud one). Combination rule: detector level = max of the two channels'
  levels (assumption, see D6).

## 7. Output stage, mix, trim
- Make-up gain (Gain display → dB) per T2, piecewise linear in display units between the tabulated points
  (−51.4 dB at 0, 0 dB at 32.28, +38.7 dB at 100). Slight level dependence (≤ 0.2 dB) is allowed.
- Tube colour: harmonics per T7 (vs level and frequency, PR 0 and 50) and T7b (vs Gain at −10 dBFS): predominantly
  2nd harmonic (≈ −53 dBc at −10 dBFS, unity Gain), 3rd ≈ 7 dB lower; harmonic level falls ≈ 1 dB per dB of output level
  reduction. Colour acts after the gain cell (compression lowers its drive).
- Response at PR 0 per T8 (−0.27 dB at 20 Hz, −0.69 dB at 10–15 kHz).
- Mix: out = m · wet + (1 − m) · dry, dry = unprocessed input (no colour, no delay), m = Mix/100.
- Trim: exact gain (measured ±0.05 dB incl. colour).

## 8. Analog noise
Off: no added signal (output noise floor < −170 dBFS). 50 Hz / 60 Hz: add, to both channels, uncorrelated white noise of
density −135 dBFS/Hz (≈ −92 dBFS over 20 kHz) plus mains hum with harmonics at k·f_m (f_m = 50 or 60 Hz) of peak level
(dBFS) k=1: −121 / −130, k=2: −98 / −99, k=3: −117 / −121, k=4: −106 / −106, k=5: −135 / −128, k=6: −121 / −127
(50 Hz / 60 Hz). Total ≈ −88.8 dBFS RMS. Tolerance ±2 dB per component.

## 9. Auto Makeup (informative)
When On, moving Peak Reduction moves the Gain control so that output level stays roughly constant as reduction
increases (measured: Gain rises ≈ 3.7 / 6.9 / 9.6 dB when Peak Reduction goes 40 → 50 / 60 / 70). Moving Gain does not
move Peak Reduction. The coupling offset is re-established when Auto Makeup is switched on. See D2.

## 10. Review decisions (standing policy: match the reference; don't reproduce clear defects)
- **D1** Static curve, programme-dependent release, drive-dependent attack, sidechain weighting, tube colour and analog
  noise are reproduced as measured.
- **D2** In the reference, after Auto Makeup has moved Gain, switching Auto Makeup off can leave a hidden make-up offset
  that is not reflected in the Gain display (display 32.28 measured +8.7 dB). Defect — not reproduced: the audio make-up
  always equals T2 at the displayed Gain value.
- **D3** Mono variant assumed identical to one Stereo channel.
- **D4** Parameter changes are smoothed click-free within 20 ms (reference smoothing not measured).
- **D5** Hi Freq behaves as measured (T4/T5), regardless of its label wording.
- **D6** Stereo link combination = max of channel levels (only "identical GR on both channels" was measured).

## 11. Tolerances
- Static (T1, T5): ±0.5 dB per point.
- Dynamics (T3): ±1.0 dB per point, RMS ≤ 0.4 dB over all T3 points.
- Sidechain weighting (T4) and detector type (T6): ±1.0 dB per point.
- Gain law (T2): ±0.1 dB; Trim/Mix: ±0.05 dB.
- Harmonics (T7/T7b): ±3 dB for components above −90 dBc; response (T8): ±0.2 dB.
- Analog noise: ±2 dB per component; Off must add nothing above −140 dBFS.
- Latency 0; sample-rate independence as §3.

## 12. User interface (functional layout — visuals must be original)
Do not imitate the reference's or the original hardware's look (no brushed-metal panel, no vintage logo, no copied
knob or meter artwork, no person names). Functional elements:
- Two large knobs, **Gain** (left) and **Peak Reduction** (right), scaled 0–100 with value readouts.
- A level meter in the centre with a selector **In / GR / Out** (VU-style ballistics: ≈ 300 ms integration).
- **Compress / Limit** switch; **Auto Makeup** switch.
- A lower strip with **Analog** (Off / 50 Hz / 60 Hz), **Hi Freq** (0–100, 100 marked "flat"), **Mix** (0–100 %) and
  **Trim** (−18…+18 dB).
- Double-click / Alt-click resets a control to its default; values editable by typing.

## 13. Open questions
1. Exact detector law (rectified-average vs other) and its time constants — T6/T7 are normative.
2. Stereo-link combination rule (D6).
3. Behaviour for repeated bursts closer than the measured 20 s gaps (memory accumulation) beyond the §4.3 model.
4. Exact tube non-linearity law (tables only).
5. Mono variant; rates above 96 kHz.
