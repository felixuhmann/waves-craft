# De-esser — behavioural specification (approved r1)

Status: APPROVED r1 (2026-10-29). Frozen — changes require a new revision. Review decisions in §10 (D1–D3).

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
3. **Gain smoothing** on the linear target gain `gt = 10^(−GR/20)`: one-pole, `g += (gt − g)·c`, with
   `c` for decreasing gain (attack) from τ ≈ 0.47 ms and for increasing gain (release) from τ ≈ 28.8 ms.
   (one-pole coefficient `c = 1 − exp(−1/(τ·fs))`, `r = exp(−1/(τ·fs))`.)

All time constants are fixed **in seconds** (verified at 44.1 / 48 / 96 kHz).

Program-dependent release (observed, approximate): after very short loud bursts (≤ 10 ms) the reference recovers
faster than the single release constant above predicts (§9.4 vector D); after sustained material it follows it.
Implementations should reproduce this qualitatively (e.g. a release that depends on how long the gain reduction
has been held) within the §8 tolerances; the exact mechanism is not specified.

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
- Dynamic envelope (§9.4): attack vectors within ±1.0 dB; release after sustained reduction within ±1.0 dB;
  release after bursts ≤ 10 ms within ±4 dB (approximate program dependence).
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

### 9.4 Envelope — 10 kHz sine in L and R, Wideband, Freq 2000 HP; gain (dB) at sample offset after a level change

| Vector | Threshold | Change | +5 | +10 | +20 | +48 | +96 | +192 | +480 | +960 | +2400 | +4800 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| A attack | −40 | −60 → −10 dBFS (from 0 dB GR) | −1.71 | −3.48 | −6.69 | −14.99 | −23.16 | −25.39 | −25.49 | −25.46 | −25.42 | −25.41 |
| B release (sustained) | −40 | −10 → −60 after 1 s | −25.40 | −25.41 | −25.42 | −25.25 | −23.55 | −19.41 | −12.29 | −7.23 | −2.33 | −0.55 |
| C attack small | −40 | −60 → −30 | −0.96 | −1.95 | −3.54 | −6.39 | −7.81 | −8.14 | −8.27 | −8.28 | −8.28 | −8.28 |
| C′ release small | −40 | −30 → −60 after 1 s | −8.26 | −8.23 | −8.15 | −7.92 | −7.54 | −6.84 | −5.23 | −3.49 | −1.20 | −0.23 |
| D release after 5 ms burst | −40 | −10 for 5 ms → −60 | −28.6 (transient) | −34.8 (transient) | −34.5 | −24.0 | −23.9 | −24.0 | −10.35 | −5.83 | −1.91 | −0.49 |
| E attack, Thr −25 | −25 | −60 → −10 | −1.29 | −2.62 | −4.85 | −9.35 | −11.97 | −12.50 | −12.64 | −12.64 | −12.64 | −12.63 |

(Vector D values in the first 100 samples are dominated by the measurement method around the burst end; use
offsets ≥ 192 for acceptance.)

### 9.5 Split shelf at maximum attenuation (g = 1/19): response of a low-level probe, magnitude dB / phase deg

| Freq | k | 1 k | 2 k | 3 k | 5.506 k | 8 k | 11 k | 16 k | 20 k |
|---|---|---|---|---|---|---|---|---|---|
| 3000 | 0.06290 | −3.18/−43 | −7.26/−58 | −10.30/−63 | −15.26/−63 | −18.40/−58 | −21.04/−50 | −23.89/−33 | −25.15/−17 |
| 5506 | 0.11916 | −1.14/−27 | −3.45/−45 | −5.75/−54 | −10.30/−63 | −13.61/−64 | −16.81/−61 | −21.27/−49 | −24.22/−29 |
| 11000 | 0.27732 | −0.24/−13 | −0.88/−24 | −1.80/−33 | −4.52/−50 | −7.22/−58 | −10.30/−63 | −15.58/−63 | −20.84/−50 |

## 10. Decisions (review gate) and open questions

Decisions (standing policy: match the reference; do not reproduce clear defects):
- D1 Reproduced: fixed 7:1 ratio, 25.57 dB maximum attenuation, the −0.08 dB threshold offset, the √10 split corner,
  the −3 dB mono monitor level, program-dependent release (approximately).
- D2 Not reproduced: asynchronous application of parameter changes. Instead: Threshold/Freq changes must be
  click-free (smooth over ≤ 20 ms); mode switches (Audio, SideChain, Monitor) crossfade over ≤ 20 ms.
- D3 Mono variant (not measured): identical processing with `d = SC(x)` (so the same signal level as the stereo
  L=R case triggers the same reduction) and `y = …(x)`.

Open questions:
1. Exact mechanism of the program-dependent release (only targets specified).
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
