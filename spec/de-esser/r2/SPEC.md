# De-esser — behavioural specification (approved r2)

Status: APPROVED r2 (2026-10-29), supersedes r1. Frozen — changes require a new revision. Review decisions in §10
(D1–D3); changes from r1 in §13 (release law and envelope test vectors re-derived after independent verification).

Derived only from black-box measurement of the licensed reference processor in a VST3 host (user-generated
sweeps, tones, noise, bursts), host-visible parameter metadata and the licensed user documentation. No code,
disassembly or vendor assets were used or are included. Product naming, parameter IDs and GUI are the
implementer's own.

## 1. Scope

A sibilance processor: a filtered sidechain drives a fast peak compressor whose gain reduction is applied either
to the whole signal (Wideband) or only to the upper part of the spectrum (Split).

Variants: **Stereo** (2 in / 2 out, linked: one gain for both channels) — primary; **Mono** (1 in / 1 out) — see §10.

Latency 0 samples. With no gain reduction the output is identical to the input in both audio modes (measured
bit-exact). Gain reduction is the only non-linearity; the processor adds no colouration otherwise.

## 2. Definitions

`fs` = sample rate. `L`, `R` = input channels. Sidechain signal `d = SC((L + R)/2)` where `SC` is the sidechain
filter (§4). All levels in dBFS (full scale = 1.0).

## 3. Parameters

Host-normalised value `r ∈ [0, 1]`.

| Parameter | Mapping | Range | Default | Display |
|---|---|---|---|---|
| Threshold | `−80 + 80·r` dB | −80 … 0 dB | 0 dB (r=1) | "−43.6 dB", 0.1 dB resolution |
| Freq | `2000 · 8^r` Hz | 2000 … 16000 Hz | 5506 Hz (r=0.4870) | integer Hz |
| SideChain filter | r < 0.5 → BandPass, else HighPass | | HighPass | |
| Audio | r < 0.5 → Split, else Wideband | | Split | |
| Monitor | r < 0.5 → Output, else SideChain | | Output | |

There are no user ratio / attack / release controls; those are fixed (§5).

## 4. Sidechain filter (exact)

All sections are standard "Audio EQ Cookbook" (RBJ) biquads designed at the actual `fs` (bilinear, prewarped at f0):

- **HighPass mode**: `SC = HP2(f0 = 0.84·Freq, Q = 1/√2)` applied **three times in cascade** (6th order, 36 dB/oct).
- **BandPass mode**: the same three high-pass sections followed by one `LP2(f0 = 1.457107·Freq, Q = 1/√2)`.
  (If `1.457107·Freq ≥ fs/2`, clamp f0 just below `fs/2`.)

The filter applies only to the sidechain, never to the audio path. Reference measurement vs this definition:
≤ −107 dB re. peak at 44.1 / 48 / 96 kHz for all tested Freq values.

## 5. Detector, gain computer, gain smoothing

Per sample, from `d` (§2):

1. **Peak envelope** `e`: if `|d| > e`: `e += (|d| − e)·a_det` else `e *= r_det`.
   Attack time constant ≈ 11 µs (practically instantaneous), release time constant ≈ 0.38 ms.
2. **Static curve** (hard knee): `level = 20·log10(e)`; `GR = clamp((level − (Threshold − 0.08 dB)) · (1 − 1/7), 0, 25.57 dB)`.
   Ratio 7:1 (measured slope 0.8565 dB/dB), maximum attenuation 25.57 dB (= linear 1/19).
3. **Gain smoothing (two-component release)** on the gain *deficit* `dt = 1 − 10^(−GR/20)` (0 = no reduction):
   - Fast part `df`: one-pole toward `dt` — attack (when `dt > df`) τ_a ≈ 0.47 ms, release (when `dt < df`)
     τ_f ≈ 11.6 ms.
   - Slow part `ds`: while `df > ds` it charges toward `df` with τ_c ≈ 7.2 ms; otherwise, while `dt < ds`, it
     releases toward `dt` with τ_s ≈ 32.4 ms.
   - Applied gain `g = 1 − max(df, ds)`.
   (one-pole coefficient `c = 1 − exp(−1/(τ·fs))`.)
   Effect: after short bursts (≲ 5 ms) most of the reduction recovers with the fast constant; the longer the
   reduction has been held, the more of it recovers with the slow constant (program-dependent release). This form
   matches the reference within 0.06–0.11 dB RMS (99th percentile ≤ 0.26 dB) on bursts of 1 ms … 1 s at −10/−20/−30
   dBFS, double bursts and burst trains, at 44.1 / 48 / 96 kHz.

All time constants are fixed **in seconds** (verified at 44.1 / 48 / 96 kHz).

Notes: the detector is a peak detector, so wideband noise receives more reduction than a sine of equal RMS
(about +3 dB more at the same RMS, §9.3). Stereo is linked via the (L+R)/2 sidechain: a signal in one channel only
needs +6 dB to cause the same reduction as the same signal in both channels.

## 6. Audio path

Let `g` be the smoothed gain from §5 (same for both channels).

- **Wideband**: `y = g · x` per channel.
- **Split**: complementary first-order band split with
  `k = tan(π·Freq/fs) / √10`,
  `LP1(z) = k(1 + z⁻¹) / ((1 + k) + (k − 1)z⁻¹)`, `HP1(z) = (1 − z⁻¹) / ((1 + k) + (k − 1)z⁻¹)` (so `LP1 + HP1 = 1`);
  `y = LP1(x) + g · HP1(x)` per channel. With `g = 1` the output must equal the input (≤ −140 dB; the reference is
  bit-exact — implement the identity case so it stays exact, e.g. by computing `x + (g − 1)·HP1(x)`).
  The split corner does not depend on the amount of gain reduction or on the SideChain filter mode
  (verified at 44.1 / 48 / 96 kHz and Freq 2–16 kHz).
- **Monitor = SideChain**: both outputs = `d / √2` (= `SC(L + R) / (2√2)`), the filtered sidechain **without**
  gain reduction (measured identical to ≤ 5e−8). Monitor is a listening aid; detection keeps running.

## 7. Behaviour not specified / out of scope

Meter ballistics (display only), preset handling, GUI resizing. The reference applies some parameter changes
asynchronously relative to audio — not reproduced (decision D2).

## 8. Tolerances for acceptance

- Sidechain filter / monitor output: ≤ −100 dB re. peak vs §4/§6 definitions.
- Below threshold / no reduction: output identical to input (≤ −140 dB), both modes.
- Static gain (steady tones/noise, §9.2–9.3): within ±0.3 dB.
- Split shelf at fixed gain (§9.5): within ±0.1 dB / ±1°.
- Dynamic envelope: §9.4 vectors within ±1.0 dB; §9.6 release trajectories within ±1.0 dB, except within the first
  480 samples (10 ms) after bursts of 5–50 ms, where ±3.0 dB applies. On the §9.6 stimulus as a whole the gain
  error (dB, sample-wise) must have RMS ≤ 0.3 dB.
- Gain values in §9.4/§9.6 are exact sample-wise gains `y/x` (Wideband, zero latency) — not envelope-detector
  estimates (r1's tables were biased by an analytic-signal estimator near level steps).
- Time constants scale with fs (seconds); latency 0.

## 9. Test vectors (48 kHz unless stated)

### 9.1 Sidechain/monitor magnitude (Monitor = SideChain, left-only input, either output), dB

| Freq, mode | 1 k | 2 k | 3 k | 4 k | 5 k | 5.506 k | 6 k | 8 k | 11 k | 16 k | 20 k |
|---|---|---|---|---|---|---|---|---|---|---|---|
| 2000 HP | −37.73 | −14.27 | −10.21 | −9.40 | −9.18 | −9.13 | −9.10 | −9.05 | −9.03 | −9.03 | −9.03 |
| 2000 BP | −37.79 | −15.12 | −13.49 | −16.13 | −19.41 | −21.03 | −22.56 | −28.13 | −35.33 | −47.14 | −60.48 |
| 5506 HP | −90.42 | −54.45 | −34.53 | −22.66 | −16.07 | −14.07 | −12.68 | −10.10 | −9.24 | −9.04 | −9.03 |
| 5506 BP | −90.42 | −54.47 | −34.59 | −22.85 | −16.55 | −14.78 | −13.69 | −13.08 | −17.20 | −28.12 | −41.39 |
| 11000 HP | −131.8 | −95.46 | −74.03 | −58.70 | −46.82 | −41.75 | −37.29 | −23.58 | −13.28 | −9.36 | −9.05 |
| 11000 BP | −131.8 | −95.46 | −74.03 | −58.71 | −46.83 | −41.76 | −37.31 | −23.63 | −13.55 | −12.33 | −22.51 |

### 9.2 Static curve — 10 kHz sine, same signal in L and R, peak level (dBFS) → steady gain (dB); Wideband, Freq 2000, HP

| Threshold | −48 | −42 | −36 | −30 | −24 | −18 | −12 | −6 | 0 |
|---|---|---|---|---|---|---|---|---|---|
| −12 | 0 | 0 | 0 | 0 | 0 | 0 | 0.00 | −4.98 | −10.12 |
| −24 | 0 | 0 | 0 | 0 | 0.00 | −4.93 | −10.08 | −15.22 | −20.36 |
| −36 | 0 | 0 | 0.00 | −4.85 | −9.98 | −15.12 | −20.26 | −25.41 | −25.57 |
| −60 | −9.98 | −15.12 | −20.26 | −25.40 | −25.57 | −25.57 | −25.57 | −25.57 | −25.57 |

Same sine in L only (R silent), Threshold −36: −24 → −4.79, −18 → −9.95, −12 → −15.10, −6 → −20.24, 0 → −25.39.

### 9.3 Noise vs sine (Threshold −36, Wideband, Freq 2000 HP)
High-passed (4 kHz, 8th order) white noise in L and R with the same RMS as a sine of the listed peak level:
−42 → −0.11, −36 → −3.01, −30 → −7.99, −24 → −13.18, −18 → −18.42, −12 → −23.44, −6 → −25.55 dB.

### 9.4 Envelope — 10 kHz sine in L and R, Wideband, Freq 2000 HP; exact gain (dB) at sample offset after a level change

Level change at offset 0; "after 1 s" = the level was held 1 s before the change. Vector D: −10 dBFS for 5 ms, then −60.

| Vector | Threshold | +5 | +10 | +20 | +48 | +96 | +192 | +480 | +960 | +2400 | +4800 |
|---|---|---|---|---|---|---|---|---|---|---|---|
| A attack −60→−10 | -40 | -1.34 | -3.02 | -6.61 | -14.47 | -22.69 | -25.13 | -25.38 | -25.40 | -25.40 | -25.40 |
| B release after 1 s at −10 | -40 | -25.39 | -25.36 | -25.26 | -24.81 | -21.88 | -17.31 | -10.94 | -6.45 | -1.99 | -0.38 |
| C attack −60→−30 | -40 | -0.73 | -1.71 | -3.51 | -6.27 | -7.76 | -8.12 | -8.27 | -8.28 | -8.28 | -8.28 |
| C′ release after 1 s at −30 | -40 | -8.27 | -8.24 | -8.13 | -7.91 | -7.52 | -6.82 | -5.21 | -3.48 | -1.20 | -0.24 |
| D release after 5 ms burst at −10 | -40 | -25.23 | -25.20 | -25.04 | -23.72 | -17.31 | -10.67 | -6.76 | -4.37 | -1.46 | -0.29 |
| E attack −60→−10, Thr −25 | -25 | -1.00 | -2.29 | -4.80 | -9.13 | -11.87 | -12.46 | -12.62 | -12.63 | -12.63 | -12.63 |

### 9.5 Split shelf at maximum attenuation (g = 1/19): response of a low-level probe, magnitude dB / phase deg

| Freq | k | 1 k | 2 k | 3 k | 5.506 k | 8 k | 11 k | 16 k | 20 k |
|---|---|---|---|---|---|---|---|---|---|
| 3000 | 0.06290 | −3.18/−43 | −7.26/−58 | −10.30/−63 | −15.26/−63 | −18.40/−58 | −21.04/−50 | −23.89/−33 | −25.15/−17 |
| 5506 | 0.11916 | −1.14/−27 | −3.45/−45 | −5.75/−54 | −10.30/−63 | −13.61/−64 | −16.81/−61 | −21.27/−49 | −24.22/−29 |
| 11000 | 0.27732 | −0.24/−13 | −0.88/−24 | −1.80/−33 | −4.52/−50 | −7.22/−58 | −10.30/−63 | −15.58/−63 | −20.84/−50 |

### 9.6 Release after bursts — 10 kHz sine in L and R, Wideband, Freq 2000 HP, Threshold −40; exact gain (dB) at sample offset after the burst ends (input then −60 dBFS)

| Burst | +48 | +96 | +192 | +480 | +960 | +2400 | +4800 | +9600 |
|---|---|---|---|---|---|---|---|---|
| −10 dBFS for 1 ms | -17.35 | -13.98 | -9.24 | -3.87 | -1.93 | -0.71 | -0.14 | -0.01 |
| −10 dBFS for 2 ms | -21.96 | -16.51 | -10.35 | -4.18 | -2.69 | -0.96 | -0.19 | -0.01 |
| −10 dBFS for 5 ms | -23.72 | -17.31 | -10.67 | -6.76 | -4.37 | -1.46 | -0.29 | -0.01 |
| −10 dBFS for 10 ms | -23.83 | -17.36 | -13.92 | -9.42 | -5.75 | -1.83 | -0.35 | -0.02 |
| −10 dBFS for 20 ms | -23.85 | -21.16 | -16.91 | -10.77 | -6.37 | -1.98 | -0.38 | -0.02 |
| −10 dBFS for 50 ms | -24.76 | -21.84 | -17.29 | -10.93 | -6.44 | -1.99 | -0.38 | -0.02 |
| −10 dBFS for 100 ms | -24.81 | -21.87 | -17.31 | -10.93 | -6.45 | -1.99 | -0.38 | -0.02 |
| −10 dBFS for 200 ms | -24.81 | -21.88 | -17.31 | -10.94 | -6.45 | -1.99 | -0.38 | -0.02 |
| −10 dBFS for 500 ms | -24.81 | -21.88 | -17.31 | -10.94 | -6.45 | -1.99 | -0.38 | -0.02 |
| −10 dBFS for 1000 ms | -24.81 | -21.88 | -17.31 | -10.94 | -6.45 | -1.99 | -0.38 | -0.02 |
| −20 dBFS for 5 ms | -15.19 | -11.95 | -8.22 | -5.50 | -3.65 | -1.25 | -0.25 | -0.01 |
| −20 dBFS for 20 ms | -15.78 | -14.50 | -12.49 | -8.69 | -5.39 | -1.74 | -0.34 | -0.01 |
| −20 dBFS for 200 ms | -16.24 | -14.89 | -12.77 | -8.84 | -5.47 | -1.75 | -0.34 | -0.01 |
| −30 dBFS for 5 ms | -7.29 | -6.27 | -4.74 | -3.36 | -2.33 | -0.84 | -0.17 | -0.01 |
| −30 dBFS for 20 ms | -7.67 | -7.30 | -6.63 | -5.08 | -3.40 | -1.18 | -0.23 | -0.01 |
| −30 dBFS for 200 ms | -7.91 | -7.52 | -6.82 | -5.21 | -3.48 | -1.20 | -0.24 | -0.01 |
| two 5 ms bursts at −10 dBFS, 20 ms apart — after the second | -23.73 | -17.32 | -11.57 | -8.19 | -5.14 | -1.67 | -0.32 | -0.01 |
| two 5 ms bursts at −10 dBFS, 50 ms apart — after the second | -23.72 | -17.31 | -10.67 | -7.28 | -4.66 | -1.54 | -0.30 | -0.01 |
| two 5 ms bursts at −10 dBFS, 100 ms apart — after the second | -23.72 | -17.31 | -10.67 | -6.86 | -4.43 | -1.48 | -0.29 | -0.01 |
| two 5 ms bursts at −10 dBFS, 300 ms apart — after the second | -23.72 | -17.31 | -10.67 | -6.76 | -4.37 | -1.46 | -0.29 | -0.01 |
| after the last of 10 × 5 ms bursts at −10 dBFS, every 60 ms | -23.72 | -17.31 | -10.67 | -7.22 | -4.63 | -1.53 | — | — |

(— = next burst follows.) Stimulus for the RMS criterion (§8): from −60 dBFS, bursts at −10 dBFS of 1, 2, 5, 10,
20, 50, 100, 200, 500, 1000 ms, then at −20 and −30 dBFS of 5, 20, 200 ms, then pairs of 5 ms bursts at −10 dBFS spaced
20 / 50 / 100 / 300 ms, then 10 × 5 ms bursts every 60 ms; each group followed by 1.5 s at −60 dBFS; 10 kHz sine, L = R.

## 10. Decisions (review gate) and open questions

Decisions (standing policy: match the reference; do not reproduce clear defects):
- D1 Reproduced: fixed 7:1 ratio, 25.57 dB maximum attenuation, the −0.08 dB threshold offset, the √10 split corner,
  the −3 dB mono monitor level, program-dependent release (approximately).
- D2 Not reproduced: asynchronous application of parameter changes. Instead: Threshold/Freq changes must be
  click-free (smooth over ≤ 20 ms); mode switches (Audio, SideChain, Monitor) crossfade over ≤ 20 ms.
- D3 Mono variant (not measured): identical processing with `d = SC(x)` (so the same signal level as the stereo
  L=R case triggers the same reduction) and `y = …(x)`.

Open questions:
1. Release law: §5 step 3 is a fitted functional form; the reference's internal mechanism may differ (residual ≤ 0.26 dB p99).
2. Threshold/Freq automation smoothing of the reference (not measured).
3. Freq quantisation (display shows integer Hz; behaviour between integers not measured).

## 11. Public references usable by the implementer
- R. Bristow-Johnson, "Cookbook formulae for audio EQ biquad filter coefficients".
- D. Giannoulis, M. Massberg, J. D. Reiss, "Digital Dynamic Range Compressor Design — A Tutorial and Analysis",
  J. Audio Eng. Soc. 60(6), 2012 (detector / gain computer / smoothing structures).
- U. Zölzer (ed.), "DAFX: Digital Audio Effects", dynamics processing chapter.

## 12. User interface (functional layout — visuals must be original)

Same controls, arrangement and display semantics as the reference so users find everything in the same place,
but all artwork, colours, fonts and names are the implementer's own (vector-drawn, no copied images/branding).

- **Left column**, three stacked boxes:
  - "Audio": one toggle button showing the current mode (Split / Wideband).
  - "Frequency": numeric value box (Hz, drag/type), below it "SideChain": a filter-mode selector with a small
    curve icon (high-pass shape / band-pass shape) and a drop-down arrow.
  - "Monitor": two stacked buttons "Audio" and "SideChain"; the active one lit; an indicator lights while
    monitoring the sidechain.
- **Centre**: "Threshold" — a vertical slider with a value flag next to its handle, aligned with a vertical
  **sidechain energy meter** (scale 0 … 80 dB below full scale, numeric peak readout under it); next to it the
  **attenuation meter** ("Atten", scale 0, −3, −6, −12, −18, Inf; the zone above −6 dB tinted differently) with a
  numeric peak-hold readout under it (click to reset).
- **Right**: stereo **output meters** (−30 … 0 dBFS) with numeric peak readouts underneath (click to reset).
- Header bar with the plugin's own name; preset/undo features optional.
- Double-click / Alt-click resets a control to default; values editable by typing.

## 13. Changes from r1
- §5 step 3: the single release constant (≈ 28.8 ms) plus a qualitative "program-dependent release" note is replaced
  by a fully specified two-component release (fast τ_f ≈ 11.6 ms, slow τ_s ≈ 32.4 ms, slow-part charging
  τ_c ≈ 7.2 ms). Reason: independent verification with exact sample-wise gain extraction showed the reference
  recovers much faster after short bursts than r1 implied (e.g. 4 ms after a 5 ms burst: −10.7 dB, not ≈ −24 dB).
- §9.4: vectors re-derived with exact sample-wise gain (r1 values were biased near level steps); B and D change most.
- §9.6 added: release trajectories after bursts of 1 ms … 1 s, three levels, double bursts and a burst train.
- §8: envelope tolerances updated (±1 dB; ±3 dB in the first 10 ms after 5–50 ms bursts; RMS ≤ 0.3 dB on §9.6).
