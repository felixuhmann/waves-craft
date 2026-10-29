# FET-style limiting amplifier — behavioural specification (r1)

Status: APPROVED r1 (2026-10-29), frozen. Review decisions in §10. Tables in `tables/` are normative where stated. Functional description only, derived from black-box measurement of a licensed reference
plug-in (two modelled hardware revisions) in a host with user-generated test signals, plus public documentation.
No code, disassembly or vendor assets used or included. Names, artwork and branding are the implementer's own.
Machine-readable tables in `tables/` (next to this file) are part of the specification and are normative where
stated.

## 1. Scope and variants

A stereo (one linked detector for both channels) and a mono variant of an analog-modelled FET-style
limiting amplifier with two selectable **Revisions**:

- **Revision A** — "late, low-noise" voicing (default).
- **Revision B** — "early" voicing (different gain staging, knee, timing, top-end roll-off and noise).

Latency 0 samples. Stereo variant: one gain applied to both channels, derived from a detector fed by both
channels (linked; §6.4). No crosstalk of audio between channels (measured ≤ −300 dB).

## 2. Controls (host parameters)

Host-normalised value r ∈ [0,1]. Display values below are what the parameter shows; the audio effect of
Input/Output is given by §4 tables (the scales are **not** literal dB gains).

| Control | Display mapping from r | Range | Default |
|---|---|---|---|
| Input | r=0: −∞; r ≤ 0.25: −60 + 96 r dB; r ≥ 0.25: −36 + 48 (r − 0.25) dB; 0.1 dB display steps | −∞ … 0 | −30 (r = 0.375) |
| Output | same mapping as Input | −∞ … 0 | −18 (r = 0.625) |
| Attack | 1 + 6 r (2 decimals); **1 = slowest, 7 = fastest** | 1 … 7 | 3.00 |
| Release | 1 + 6 r (2 decimals); **1 = slowest, 7 = fastest** | 1 … 7 | 4.00 |
| Ratio | r < 0.1125 → 20; < 0.3625 → 12; < 0.6125 → 8; < 0.8625 → 4; else All | 20/12/8/4/All | 4 |
| Analog | r < 0.2375 → 50 Hz; < 0.7375 → 60 Hz; else Off | 50 Hz/60 Hz/Off | Off |
| Revision | r < 0.5 → B; else A | A/B | A |
| Meter | GR / In / Out (display only) | | GR |
| Comp Off | off/on (r ≥ 0.5 = compression bypassed, colouration kept) | | off |
| Mix | 100 r % (0.1 % steps) | 0 … 100 % | 100 % |
| Trim | −18 + 36 r dB (0.1 dB) | −18 … +18 dB | 0 dB |
| Auto Makeup | off/on (r ≥ 0.5) | | off |

## 3. Signal flow

```
x ─► [Input gain Gin(rev, Input)] ─► [gain cell: GR(t)] ─► [colouration: harmonics, response, noise] ─►
     [Output gain Gout(rev, Output)] ─► wet
y = Trim · ( Mix·wet + (1 − Mix)·x )          (Mix linear amplitude crossfade, in phase; Mix 0 % → y = Trim·x
                                                exactly, bit-exact for Trim 0 dB)
```

- **Trim**: exact dB gain after Mix (measured error ≤ 0.01 dB).
- **Comp Off**: GR ≡ 0 dB, everything else unchanged (gain staging and colouration remain; at high internal
  level the colouration stage saturates strongly — §7.2 table rows `compoff_*`).

## 4. Gain staging (normative tables)

- `tables/gain_input.csv`: small-signal gain (1 kHz, −60 dBFS input, Output display 0, no GR) vs Input display,
  per revision. Linear interpolation in display dB between rows; −∞ display = silence. Note the strongly non-linear
  law and the saturation above about −6 display.
- `tables/gain_output.csv`: additional gain of the Output control relative to Output display 0, per revision.
- The All ratio adds +1.2 dB (rev A) / +0.6 dB (rev B) small-signal gain, included in its static GR table as a
  positive value at low level.

Tolerance: ±0.2 dB at table rows; interpolation between rows is the normative law.

## 5. Static compression law (normative table)

Define the internal level **x = L + Gin(rev, Input)**, where L is the input level as **sine peak in dBFS**.
Steady-state gain reduction GR(x) ≤ 0 dB depends on revision and ratio only (Release changes it by ≤ 0.3 dB;
Attack not at all), and is given in `tables/static_gr.csv` (x in 1 dB steps; rows marked `E` are settled values
with 4 s steps and take precedence over rows marked `A` at the same x; interpolate linearly).

Properties (informative, follow from the table): threshold rises with ratio (rev A: GR = −1 dB at x ≈ +0.5 / +2.5 /
+3.5 / +5.5 dB for 4 / 8 / 12 / 20); very soft knee; slope above threshold ≈ −0.8 … −0.95 dB/dB for all ratios
(ratio mainly shifts the threshold); maximum reduction ≈ 31.9 dB (all ratios saturate there); revision B knees
earlier and harder at 4:1.

Steady-state output level: `out = L + Gin + GR(x) + Gout(rev, Output)` (+ Mix/Trim as §3).

Tolerance (steady state, 1 kHz sine): ±1.0 dB for ratios 4/8/12/20; ±2.5 dB for All (its curve shifts by up to
2 dB with the Input setting at the same x).

## 6. Dynamics

### 6.1 Detector and gain cell
Peak-responding, feedback-style behaviour: on a level step the gain reduction overshoots the final value
slightly and settles. Times are in milliseconds and independent of sample rate (§9).

### 6.2 Attack (normative)
Attack t63 = time for GR (dB) to reach 63 % of its final value after a +30 dB step on a 4 kHz carrier
(Input −12, ratio 4, ≈ 21 dB GR): `tables/timing.csv` rows `att_*`:
Attack 1 → 1.55 ms, 3 → 0.73 ms, 5 → 0.31 ms, 7 → 0.17 ms (interpolate geometrically in between).
Higher ratios attack faster at the same setting (20:1 at Attack 3 → 0.21 ms; All → 0.21 ms, All at 7 → 0.08 ms).
Tolerance ±25 %.

### 6.3 Release (normative tables)
Program-dependent. `tables/timing.csv` gives t63/t90 of the recovery (in dB) after bursts of 5, 50, 500 and
2000 ms (+30 dB, ≈ 21 dB GR) per Release setting, ratio and revision; `tables/release_trajectories.csv` gives the
sampled recovery GR(t) after each burst. Key values (rev A, ratio 4, long holds): Release 7 → t63 0.54 s;
6 → 0.93 s; 5 → 1.75 s; 4 → 2.2 s; 3 → 3.2 s; 2 → ≈ 3.8 s; 1 → ≈ 4 s. After a 5 ms burst recovery is much faster
(Release 7: t63 94 ms; 6: 300 ms; 4: 1.37 s). All mode releases quickly and almost linearly in dB after a short
delay (t63 ≈ 70 ms at Release 7, ≈ 0.48 s at Release 4).
Shape (informative): short bursts recover roughly exponentially in dB (t90 ≈ 2–2.7 × t63); long holds recover
closer to linearly in dB (t90 ≈ 1.6 × t63).
Tolerance: t63/t90 ±25 %; trajectory points ±1.5 dB (rows marked `approx`: ±3 dB — the measurement gap was
shorter than full recovery for Release 1–2).

### 6.4 Stereo link
Stereo variant: one detector for both channels; a loud signal on one channel reduces the other channel's gain
by the same amount (measured: R gain drops while L bursts, by the full GR). Use max(|L|,|R|) detection.

## 7. Colouration (normative tables)

### 7.1 Frequency response (small signal, no GR)
`tables/freq_response.csv` (re 1 kHz): rev A essentially flat (−0.4 dB at 50 Hz, −1.2 dB at 20 kHz); rev B
rolls off the top (−0.7 dB 10 kHz, −2.5 dB 15 kHz, −7.8 dB 20 kHz) at 48 kHz. Tolerance ±0.3 dB below 15 kHz,
±1 dB above.

### 7.2 Harmonic distortion
`tables/harmonics.csv`: gain, H2, H3, H5 (dBc) for steady tones at 100 Hz / 1 kHz / 5 kHz at −30/−18/−6 dBFS for
representative settings (light/heavy compression, All, Comp Off, Input 0), both revisions; plus per-step H2..H5 in
the static staircases (analyst capture, summarised there). Typical: H2 −40 … −60 dBc, H3 −45 … −60 dBc with
compression; All mode raises H3 at 100 Hz (−45 dBc); Comp Off at high internal level saturates hard
(H2 ≈ −10 dBc, H3 ≈ −11 … −25 dBc). Even-order content is present (asymmetric curve).
Tolerance: H2/H3 ±4 dB where ≥ −70 dBc; below that ≤ −66 dBc.

### 7.3 Noise and hum (Analog)
Analog Off: no added noise (digital silence in → silence out). 50/60 Hz: added noise at the output,
independent of Input setting: rev A ≈ −71.8 dBFS rms (energy concentrated at very low frequencies);
rev B ≈ −76.1 dBFS rms with a mains line at 50 / 60 Hz ≈ −89 dBFS and its 3rd harmonic ≈ −95 dBFS
(`tables/noise.csv`). Tolerance ±2 dB on rms and line levels.

## 8. Auto Makeup
Measured (`docs` probe): with Auto Makeup on, Input changes made through the host parameter interface do **not**
move the Output parameter; the audio changes only slightly (steady small-signal level −0.8 dB at Input −12, no
change at Input −30). The manual describes an editor-level linkage (Output follows Input moves so that overall
level stays roughly constant; no movement below threshold; Output stops following once the compressor saturates;
Output moves never affect Input).
Requirement: implement Auto Makeup as an editor linkage per that description, applied to user gestures on Input
(the Output parameter is moved and recorded by the host like a normal edit), with the makeup amount derived from
the static law (§5) so that a −18 dBFS sine keeps its output level within ±2 dB while Input is moved. Host
automation of Input does not move Output. The −0.8 dB internal difference is not reproduced (review decision D6).

## 9. Sample rate
Gain staging, static law and times are independent of sample rate: static staircases at 44.1 and 96 kHz match 48 kHz
within 0.07 dB; attack/release times at 96 kHz match 48 kHz within 2 %. Response tables are given at 48 kHz; the
implementation must keep the analog-prototype response (not scale it with fs).

## 10. Review decisions (standing policy: match the reference, don't reproduce clear defects)
- D1: Attack/Release knobs keep the reference convention (1 slowest … 7 fastest) — it is the documented
  behaviour, not a defect. The manual's nominal times (attack 50 µs–1 ms, release 50 ms–1 s) do NOT match the
  measured behaviour; the measured tables are normative.
- D2: Input/Output non-linear scales are reproduced (they are part of the instrument's character); the displayed
  numbers are labels.
- D3: Parameter changes must be smoothed and click-free (≤ 20 ms); the reference applies some changes
  asynchronously (e.g. Auto Makeup moving Output late) — not reproduced as a timing artefact.
- D4: Analog noise/hum is reproduced as specified (§7.3) and is off by default.
- D5: Mono variant = the same processing on one channel (not separately measured).
- D6: Auto Makeup is an editor linkage only (§8); the reference's small unexplained internal level change is not
  reproduced.

## 11. Open questions
1. Exact detector/gain-cell law (feedback FET model) — dynamics are table-normative.
2. Release behaviour between the measured burst lengths and for complex programme.
3. Colouration at levels/frequencies between table rows; exact saturation curve of Comp Off.
4. Mono variant; Analog noise spectrum beyond the rms and line levels.

## 12. User interface (functional layout — visuals must be original)
One horizontal panel:
- Left: two large stepped-feel rotary controls **Input** and **Output** (scale labels −∞, 48, 36, 30, 24, 18, 12, 6,
  0) with value readouts.
- Middle: two smaller rotaries **Attack** and **Release** (1 … 7, labelled "slow ← → fast"), and a vertical column
  of five latching **Ratio** buttons (20, 12, 8, 4, All — exactly one active).
- Right: a moving-needle style **meter** (original drawing) with an over-LED, and a vertical button column
  **GR / In / Out** plus **Comp Off**.
- Bottom strip: **Auto Makeup** toggle, **Analog** selector (50 Hz / 60 Hz / Off), **Revision** selector (A / B —
  the two revisions may use two original colour themes), **Mix** and **Trim** small rotaries.
Double-click / Alt-click resets a control; values typed into readouts. No vendor/hardware names, logos, artwork or
fonts; do not imitate the hardware's faceplate.
