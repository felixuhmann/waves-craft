# Flanger — behavioural specification (candidate r1)

Status: APPROVED r1 (2026-10-29; review decisions in §11 per standing user policy). Frozen — changes require a new revision.

Derived only from black-box measurement of the licensed reference processor in a VST3 host with user-generated
test signals (impulses, sweeps, tones, noise), host-visible parameter metadata, and public documentation.
No code, disassembly or vendor assets were used or are included. Product naming, parameter IDs, GUI and
branding are the implementer's own.

Accuracy of this description versus the reference (analyst validation, broadband noise, 44.1/48/96 kHz):
static (unmodulated) settings exact to float precision; modulated settings within −44 … −74 dB re output peak
(residual consistent with the reference's own LFO numerics, ≈0.05 sample trajectory jitter).

## 1. Scope and variants

A modulated-delay effect (flanger / chorus / phaser-style) per channel:

| Variant | I/O | Behaviour |
|---|---|---|
| **Stereo** | 2 in / 2 out | two independent channel processors; the right channel's LFO is phase-offset by Stereo (§5.3) |
| **Mono** | 1 in / 1 out | one channel processor (the "left" one) |
| **Mono→Stereo** | 1 in / 2 out | the mono input feeds both channel processors (see §9 for measured details) |

Linear and time-invariant for fixed settings when the LFO is static (Rate 0, Depth 0 or Stop). No level
dependence (checked over 20 dB). Reported latency: 0 samples.

## 2. Parameters

Displayed value → internal value. Host-normalised mapping `r ∈ [0,1]` given where measured; implementers may use
any monotonic mapping whose displayed values and internal quantisation match.

| Parameter | Range / mapping | Default | Internal resolution |
|---|---|---|---|
| Delay | 0.1 … 50 ms, `0.1·500^r` ms | 2.2 ms | 0.1 ms |
| Depth | 0 … 100 %, `100 r` | 100 % | 1 % |
| Rate | 0 … 20 Hz, `0.005·4000^r` Hz | 0.20 Hz | rounded to 0.005 Hz; values < 0.01 Hz → 0 (LFO static) |
| Mix | 0 … 100 % | 50 % | 1 % |
| Feedback | 0 … 100 % | 50 % | 1 % |
| Output Gain | −12 … +12 dB, linear in r | 0 dB | 0.1 dB |
| Stereo | 0 … 180°, `180 r` | 0° | 1° (then see §5.3) |
| Filter Freq | 100 Hz … 20 kHz, `100·200^r` Hz | 1.0 kHz | display 0.1 k (<10 k) / 1 k (≥10 k) |
| Filter Type | Low-pass / High-pass | High-pass | — |
| Filter On | off/on | off | — |
| Waveform | Sine / Triangle | Sine | — |
| Wet Polarity (Inv Mix) | +/− | + | — |
| Feedback Polarity (Inv Feedback) | +/− | + | — |
| Tape | off/on | off | — |
| Stop | off/on | off | — |
| Link (Rate↔Depth) | off/on | off | — |
| Tempo Sync | off/on | off | — |

Display formats: Delay "2.2 ms"; Depth/Mix/Feedback "50 %"; Rate "0.20 Hz" (two decimals below 10 Hz, one above);
Gain "0.0 dB"; Stereo "90"; Filter "1.0k" / "12k".

## 3. Signal flow (per channel processor)

```
x ──×G_in──┬──────────────────────[dry delay]──────────── ×g_dry ──┐
           │                                                       (+)── y
           └─(+)──► delay line ──read at d(n)──► w(n) ──[FIR]── ×s_wet·g_wet ──┘
             ▲                                │
             └────── × (±F_loop) ◄────────────┘   (feedback taken from w, BEFORE the filter)
```

- `G_in = 10^(Gain/20)` (applied at the input; the processor is linear so position is immaterial).
- Delay line: `v(n) = x_in(n) + p_fb·F_loop·w(n)`, `w(n)` = read of `v` at fractional delay `d(n)` samples with
  **2-point linear interpolation**: with `k = floor(d)`, `a = d − k`: `w(n) = (1−a)·v(n−k) + a·v(n−k−1)`.
  `d(n) ≥ 1` always (see §4), so the loop has no zero-delay path.
- `F = Feedback/100`; `F_loop = min(F, 0.999)`; `p_fb = −1` if Feedback Polarity inverted else +1.
- Filter (§6) acts on the wet output only — it is **not** inside the feedback loop (every echo is filtered exactly
  once). Filter off: FIR = identity (no added delay).
- Wet gain compensation: `g_wet = 1 − F^(20·Delay_ms)/2` (Delay_ms = quantised Delay value in ms).
  Examples: F 0.95 → 0.5928 (0.2 ms), 0.6683 (0.4 ms), 0.8208 (1 ms), 0.9970 (5 ms); F 1.0 → 0.5.
- `s_wet = −1` if Wet Polarity inverted else +1.
- Mix `m = Mix/100`: `g_dry = sqrt(1 − m)`, `g_mix_wet = sqrt(m)`; `y = g_dry·dry + g_mix_wet·s_wet·g_wet·FIR(w)`.

## 4. Delay trajectory

Let `D = floor(Delay_ms · fs / 1000)` (integer samples at the actual sample rate `fs`; e.g. 2.2 ms → 105 @48 k,
97 @44.1 k, 211 @96 k). `L(n)` is the LFO value in [−1, 1] (§5); `depth = Depth/100`.

- **Normal (Tape off):** `d(n) = D + 0.95·depth·D·L(n)`; dry path undelayed.
  (Depth 100 % sweeps 0.05·D … 1.95·D; no upper limit was observed — 50 ms at 100 % reaches 97.5 ms.)
- **Tape on:** `c = floor(depth·D)`; `d(n) = max(1, c + (c − 1)·L(n))`; dry path delayed by `max(1, c)` samples.
  So the wet sweeps from 1 sample up to `2c − 1`, passing through the dry signal's position ("through zero").
  Depth 0 % with Tape: wet and dry both at 1 sample.

## 5. LFO

### 5.1 Waveforms
- Sine: `L = sin θ`.
- Triangle: `L = tri(θ)`, piecewise linear with the same phase convention as the sine
  (0 → +1 at θ = π/2, −1 at 3π/2, 0 at 0/π, linear in between) — i.e. a linear delay sweep.

### 5.2 Phase and rate
- `θ(n) = 2π·Rate·n/fs + φ_ch` with `n` counted from the start of playback/render (θ = 0 at the first processed
  sample; see §9 for the measured reference to song position).
- Rate 0 (or Stop on): the LFO holds its current phase (at start: θ = φ_ch).
- Stop: freezes θ at its current value; on release the LFO continues from the frozen phase.

### 5.3 Stereo offset
Right-channel phase offset `φ_R = floor(Stereo·1024/360) · 2π/1024` (Stereo in integer degrees; e.g. 30° → 29.88°,
45° → 45°, 90° → 90°); left channel `φ_L = 0`. A positive offset makes the right LFO lead the left.

### 5.4 Tempo sync
See §9.

### 5.5 Link
Review decision (§11): Link is a control-coupling convenience: while on, changing Rate scales Depth inversely
(and vice versa) so that `Rate × Depth` stays constant (constant peak pitch deviation), clamped to the ranges.
The DSP always uses the current Rate and Depth values.

## 6. Filter

9-tap linear-phase FIR (4-sample group delay on the wet signal only), window `w = Kaiser(9, β = 4.0)`
(peak 1), cut-off `fc` = Filter Freq, `n = −4 … 4`:

```
h_raw[n] = (2·fc/fs) · sinc(2·fc·n/fs) · w[n]          (sinc(x) = sin(πx)/(πx))
Low-pass:  h = h_raw / Σ h_raw                          (unity DC gain)
High-pass: h = δ[n] − h_raw                             (NOT renormalised; DC gain = 1 − Σ h_raw)
```

Measured taps match this to ≤ 2·10⁻³ (deviation attributed to the reference's parameter quantisation).
Consequence (reproduce): the high-pass is gentle at low settings (e.g. 1 kHz → DC gain −1.96 dB).

## 7. Parameter smoothing
The reference applies no smoothing to Delay/Depth/Rate changes (documented zipper noise). Review decision (§11):
implement click-free smoothing of all continuous parameters (time constant 10–50 ms; Delay changes as a smooth
glide of the read position), and crossfade toggles (≤ 20 ms). Static behaviour is unaffected.

## 8. Tolerances for acceptance
- Static settings (Depth 0, Rate 0 or Stop): impulse response matches §3–§6 to ≤ −100 dB re peak.
- Modulated settings: output versus the §3–§6 model on noise ≤ −60 dB re peak over 2 s from start; LFO
  trajectory within ±0.1 sample of `d(n)`.
- Latency 0; no level dependence; all sample rates 44.1–192 kHz.

## 9. Measured details (tempo sync, phase reference, Mono→Stereo)
- **Phase reference:** the LFO restarts at θ = 0 at the start of every playback/render, independent of the song
  position (a render starting at 3.125 s produced the same trajectory as one starting at 0). Same in Tempo Sync.
- **Tempo Sync on:** Rate is interpreted as LFO cycles per beat: `f_LFO = Rate × BPM/60` (Rate 1.00 at 120 BPM →
  2 Hz), phase reference as above. The Rate display unit changes to "x/Beat" (implementer may offer beat-division
  presets in the UI).
- **Mono→Stereo:** only input channel 1 is used; it feeds both channel processors (outputs 1 and 2); output 2's LFO
  is offset by Stereo exactly as in §5.3 (Stereo 90 → output 2 leads by 90°). Input channel 2 is ignored.

## 10. Test vectors

Impulse responses (48 kHz, Depth 0, Filter off unless stated), taps as (sample index, value):

| Setting | Output |
|---|---|
| Mix 50, Fb 50, Delay 2.2 | (0, 0.70711), (105, 0.70711), (210, 0.35355), (315, 0.17678), … ×0.5 |
| Mix 25, Fb 0 | (0, 0.86603), (105, 0.5) |
| Mix 75, Fb 0 | (0, 0.5), (105, 0.86603) |
| Mix 100, Fb 0, Delay 0.1 / 0.5 / 1.0 / 50 | single tap at 4 / 24 / 48 / 2400 |
| Mix 100, Fb 75, Delay 1.0 | (48, 0.998414), then ×0.75 per 48 samples |
| Mix 100, Fb 100, Delay 1.0 | (48, 0.5), then ×0.999 per 48 samples |
| Mix 100, Fb 95, Delay 0.4 | (19, 0.66829), then ×0.95 per 19 samples |
| Mix 50, Wet Polarity − | (0, 0.70711), (105, −0.70711) |
| Mix 100, Fb 50, Feedback Polarity − | (48, 1.0), (96, −0.5), (144, 0.25), … |
| Gain −12 / +12, Mix 0 | (0, 0.25119) / (0, 3.98107) |
| Tape, Mix 50, Depth 0 | (1, 1.41421) |
| Tape, Mix 100, Depth 50, Rate 0 | (52, 1.0) |
| LP 1 kHz, Mix 100, Fb 0, Delay 1 | taps 48…56: 0.0174425 0.0655388 0.1292758 0.1845239 0.2064381 (symmetric) |
| HP 1 kHz, same | taps 48…56: −0.0035202 −0.0132267 −0.0260898 −0.0372397 0.9582871 (symmetric) |
| HP 10 kHz, same | taps 48…56: 0.0061158 0.0245142 −0.0505566 −0.2764258 0.5850583 (symmetric) |
| Mix 100, Fb 50, LP 1 kHz | each echo (every 48 samples) = the LP kernel × 0.5^k, centred at 52 + 48k |

LFO trajectories (Mix 100, Fb 0, Delay 2.2 ms, 48 kHz; d in samples at t seconds):

| Setting | d(t) |
|---|---|
| Sine, Rate 1, Depth 100 | 105 + 99.75·sin(2πt): t=0.75 → 5.25, t=0.25 → 204.75 |
| Sine, Rate 1, Depth 50 | 105 + 49.875·sin(2πt) |
| Triangle, Rate 1, Depth 100 | 105 + 99.75·tri(2πt) |
| Rate 1, Stereo 90 | right = 105 + 99.75·sin(2πt + π/2) |
| Tape, Rate 1, Depth 100 | 105 + 104·sin(2πt) (wet), dry at 105 |
| Delay 10, Depth 30, Rate 1 | 480 + 136.8·sin(2πt) |

## 11. Review decisions (standing user policy: match the reference, do not reproduce defects)
1. Parameter smoothing added (reference zipper noise = defect, §7).
2. Link implemented as described in §5.5 (reference ignored host-set Rate while Link was on = defect).
3. History-dependent internal Delay value (reference) not reproduced: internal Delay always equals the displayed,
   0.1 ms-quantised value.
4. No internal clipping (reference warns of internal clipping at high levels; not reproduced).
5. Reproduced on purpose (deterministic, harmless): integer-sample base delay, 0.95 depth factor, ms-based wet
   gain compensation, 1024-step stereo offset quantisation, non-renormalised high-pass, filter outside the loop.

## 12. User interface (functional layout — visuals must be original)
All artwork, colours, fonts, logos and names are the implementer's own; built from scratch.
- Four sections left→right:
  1. **Mix / Feedback / Filter**: Mix value box with a wet-polarity toggle; Feedback value box with a
     feedback-polarity toggle; Filter type toggle (LP/HP icon) + frequency value box + on/off toggle.
  2. **Delay / Rate / Depth**: Delay value box with a Tape toggle; Rate value box (with Tempo-Sync toggle) and
     Depth value box joined by a Link toggle.
  3. **LFO**: Waveform selector (Sine/Triangle) with a small waveform picture; a modulation indicator that moves
     with the current LFO position (left/right channel), with a Stop toggle; Stereo value box with a small
     L/R phase indicator.
  4. **Output**: Gain fader (−12 … +12 dB) with value box, stereo output peak meters with clip indicators and
     numeric peak-hold boxes (click to reset).
- Value boxes: drag vertically to change, click to type, double-click/Alt-click to reset to default.

## 13. Public references usable by the implementer
- J. Dattorro, "Effect Design, Part 2: Delay-Line Modulation and Chorus", J. Audio Eng. Soc. 45(10), 1997.
- U. Zölzer (ed.), "DAFX: Digital Audio Effects", delay-based effects chapter.
- T. Laakso et al., "Splitting the Unit Delay", IEEE Signal Processing Magazine, 1996 (fractional delay).
- J. F. Kaiser window / windowed-sinc FIR design (any DSP textbook, e.g. Oppenheim & Schafer).
