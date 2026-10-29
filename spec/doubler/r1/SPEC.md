# Pitch-shift doubler — behavioural specification (candidate r1)

Status: APPROVED r1 (2026-10-29; review decisions D1–D4 in §10 applied under the standing user policy: match the reference, do not reproduce clear defects, original visuals, own naming). Frozen — changes require a new revision.

Derived only from (a) black-box measurement of the licensed reference processor in a VST3 host with
user-generated test signals (sweeps, impulses, click trains, sines, noise), (b) host-visible parameter
metadata and (c) public documentation. No code, disassembly or vendor assets were used or are included.
Product naming, parameter IDs, GUI and branding are the implementer's own choice.

## 1. Scope

A stereo-in / stereo-out "doubler": the input is copied to N additional **voices** (N = 4 for the main
variant, N = 2 for a reduced variant), each with its own gain, pan, delay, feedback, octave switch, fixed
detune and pitch modulation; plus a **direct** (dry) path, a voices-only shelving EQ and a master gain.
The 2-voice variant is bit-identical to the 4-voice variant with voices 3–4 off (verified), so implement one
engine parameterised by N. Mono→stereo: feed the mono input to both input channels. (Mono→mono variants are
out of scope.)

## 2. Notation

`fs` = sample rate. **Design-rate times**: some internal constants are defined below as times derived at
44.1 kHz; implement them as `round(time × fs)` samples (see §10 decision D2).
`F(r) = 15.625 · 2^(125·r/12)` Hz (frequency taper, 15.6 Hz … 21357 Hz).

## 3. Parameters

Host-normalised `r ∈ [0,1]`; values quantised to the stated resolution.

| Parameter | Mapping | Range | Default (4-voice) | Resolution |
|---|---|---|---|---|
| Master Gain | `−24 + 24r` dB | −24 … 0 dB | 0 dB | 0.1 dB |
| Feedback High-Pass | `F(r)` | 16 … 21357 Hz | 16 Hz (r = 0.003) | 1 Hz |
| Modulation Reset | trigger | — | — | rising edge resets all LFOs |
| Range | switch | 80 Hz / 20 Hz | 80 Hz (r < 0.5) | — |
| Align Direct | switch | No / Yes | No | — |
| Direct On | switch | | On | — |
| Direct Gain | `−48 + 48r` dB | −48 … 0 dB | 0 dB | 0.1 dB |
| Direct Pan | `−45 + 90r` ° | −45 … +45° | 0° | 0.1° |
| EQ Low On / EQ High On | switch | | On / On | — |
| EQ Low / High Gain | `−88 + 100r` dB | −88 … +12 dB | 0.0 dB (r 0.88) / +4.1 dB (r 0.921) | 0.1 dB |
| EQ Low / High Freq | `F(r)` | 16 … 21357 Hz | 101 Hz (r 0.258) / 1306 Hz (r 0.613) | 1 Hz |
| Voice n On | switch | | V1 on, V2 off, V3 on, V4 off | — |
| Voice n Gain | `−48 + 48r` dB | −48 … 0 dB | −6, −12, −6, −12 dB | 0.1 dB |
| Voice n Pan | `−45 + 90r` ° | −45 … +45° | −45, +45, +45, −45° | 0.1° |
| Voice n Delay | `100r` ms | 0 … 100 ms | 9.4, 16.0, 23.7, 21.0 ms | 0.1 ms |
| Voice n Feedback | `100r` % | 0 … 100 % | 0 % | 1 % |
| Voice n Octave | switch | off / −1 octave | off | — |
| Voice n Detune | `−100 + 200r` cents | ±100 cents | +6, +10, −6, −10 cents | 1 cent |
| Voice n Mod Depth | `−200 + 400r` cents | ±200 cents | 0 | 1 cent |
| Voice n Mod Rate | `0.1 · 4000^r` | 0.1 … 400 | 1.0 (r 0.278) | 0.1 |
| Input Routing A (voices 1, 3) | 3-way | L / (L+R)/2 / (L−R)/2 | L | r < 0.3 / < 0.8 / ≥ 0.8 |
| Input Routing B (voices 2, 4) | 3-way | R / (R+L)/2 / (R−L)/2 | R | same thresholds |

2-voice variant defaults: gains −6/−6 dB, pans −45/+45°, delays 9.4/23.7 ms, detunes +6/−6 cents, both on.

## 4. Signal path

Input stereo `(L, R)`. Output `(Lo, Ro) = Master · (Direct + EQ(Σ voices))`.

**Direct**: if on, `g_d = 10^(DirectGain/20)`, `θ = DirectPan`:
`Ld = g_d (cos θ·L − sin θ·R)`, `Rd = g_d (sin θ·L + cos θ·R)` (a stereo rotation; −45° folds the image left).
If Align Direct = Yes, the direct is delayed by the inherent latency `Lr` (§5).

**Voice n** (n = 1…N): source `s` from its routing (voices 1, 3 use routing A; 2, 4 routing B).
Voice core (§5–§7) produces mono `y`. Then `y·10^(Gain/20)` is panned with constant power:
`φ = Pan + 45°` → left `cos φ`, right `sin φ` (−45° = left only, 0° = −3.01 dB each side).

**EQ** (applies to the voice sum only, never the direct; identical on both channels): low shelf then high shelf,
each bypassed when off (see §8).

**Master**: `10^(MasterGain/20)` on everything.

No reported plugin latency (0 samples) in any mode.

## 5. Voice delay and inherent latency

Inherent latency `Lr` (design-rate time, §2): **Range 80 Hz: 7.256 ms (= 320 samples at 44.1 kHz)**;
**Range 20 Hz: 24.671 ms (= 1088 samples at 44.1 kHz)**.

Nominal voice delay `Dn` (relative to the undelayed input):
- Align Direct = No: `Dn = max(Delay, Lr)` (delays below `Lr` are clamped to `Lr`).
- Align Direct = Yes: `Dn = Delay + Lr` (and the direct is delayed by `Lr`, so the relative delay equals the
  Delay setting, reaching 0 ms).

Voice delays are fractional (linear interpolation between samples).

With Detune = 0, Depth = 0 and Octave off, the voice core is an exact delay of `Dn` (no pitch-shifter colouration).

## 6. Pitch shifter

Pitch ratio `ρ = 2^(c/1200) · (0.5 if Octave)`, where `c = Detune + LFO(t)` cents (§7). Duration is preserved
(it is a pitch shifter, not a modulated delay).

Required functional structure (measured behaviour of the reference):
- Reads from the voice delay line with read taps moving at rate `ρ` (relative delay changes by `1 − ρ` per sample),
  fractional (linear-interpolated) reads.
- The effective delay stays inside a window around `Dn`: for `ρ > 1` the delay decreases until it reaches
  `Dn − Wh`, for `ρ < 1` it increases until `Dn + Wh`; then a splice to a new read position is made, jumping back by
  an amount `J` chosen within `[Jmin, Jmax]` to **maximise the similarity (normalised cross-correlation) of the
  signal at the old and new read positions**, so periodic material splices phase-coherently.
  Range 80 Hz: `Wh = 6.37 ms`, `J ∈ [5.6, 11.1] ms`; Range 20 Hz: `Wh = 23.6 ms`, `J ∈ [23.9, 35.0] ms`
  (design-rate times).
- Old and new taps are mixed with complementary **linear** gains (w, 1 − w) during the transition; the
  reference behaves as a continuous two-tap overlap (uncorrelated material loses 1.76 dB on average, see §11).
- Mean delay for ±50 cents at Range 80 Hz: `Dn ∓ 2.2 ms` (± 0.7 ms) (design-rate time).

Exact splice-search details are implementation-defined; acceptance is statistical (§11.3).

## 7. Pitch modulation (per voice)

Triangle LFO in cents, amplitude ±|Depth| around the nominal Detune. At start / reset the LFO is at 0
(nominal detune) and first moves in the direction of the Depth sign (+ = upward). LFO frequency = **Rate Hz**
(review decision D3). Each voice's LFO runs freely from plugin start; Modulation Reset (rising edge) restarts all
voices' LFOs at phase 0 simultaneously.

## 8. EQ (voices only)

RBJ-cookbook shelves (low shelf, high shelf), `Q = 0.5`, gain `G` = EQ Gain dB, centre frequency
`f_a = F_set · 10^(+G/80) / 1.5414` (low) and `f_a = F_set · 1.5694 · 10^(−G/80)` (high), mapped to the digital
design frequency `f0 = (fs/π)·atan(π f_a / fs)`, then the standard cookbook coefficients with `w0 = 2π f0 / fs`.
Gains down to −88 dB are valid (deep cuts make the shelf a cut filter). Sample-rate independent (verified 44.1/48/96 kHz).

## 9. Feedback (per voice)

Loop: the voice core's output is fed back to its own input through a gain `fb = Feedback/100` and a first-order
high-pass (bilinear, prewarped at `F_set` = Feedback High-Pass): `H(z) = (1 − z⁻¹)/((1 + k) + (k − 1)z⁻¹)`,
`k = tan(π F_set / fs)`. Loop period = `Dn + 1 sample`. The pitch shifter is inside the loop (echoes keep shifting).
Feedback is taken before voice gain, pan, EQ and master (they scale all echoes equally).
100 % feedback is allowed (echoes decay only by the high-pass; at 16 Hz ≈ 0.2 % per pass).

## 10. Review decisions

- **D1 — Align with zero detune.** The reference's detune-0 path with Align = Yes uses `Delay + 495 samples`
  (Range 80) / `+ 1978 samples` (Range 20) instead of `Delay + Lr`, contradicting its own documentation and its
  detuned path. Not reproduced: always `Dn = Delay + Lr` (§5).
- **D2 — Sample-rate dependence.** The reference's inherent latencies and splice window are fixed sample counts
  (320 / 1088 samples at every rate), i.e. correct only at 44.1 kHz. Implement them as the 44.1 kHz times scaled
  to the actual rate (as with the earlier stereo-imager decision). At 48 kHz this makes the inherent latency
  7.26 ms instead of the reference's 6.67 ms.
- **D3 — Modulation rate.** The reference's triangle runs at **Rate/2 Hz**, although its documentation states one
  cycle per 1/Rate s. Treated as a defect: LFO frequency = Rate Hz.
- **D4** — Display label for Mod Rate: "Hz".

## 11. Test vectors and tolerances

### 11.1 Deterministic paths (any fs; reference matches the model to ≤ −135 dB re peak)
| Setting (others: voice 1 only, gain 0 dB, Detune 0, Depth 0, EQ off, direct off) | Expected |
|---|---|
| Direct only, pan 0 | identity |
| Direct pan −22.5° | `Lo = 0.9239 L + 0.3827 R`, `Ro = −0.3827 L + 0.9239 R` |
| Direct pan −45° | `Lo = 0.7071(L + R)`, `Ro = 0.7071(R − L)` |
| Direct gain −12 dB / Master −12 dB | ×0.2512 |
| Direct, Align Yes, Range 80 / 20 | delay 7.256 / 24.671 ms (design-rate times) |
| Voice 1, pan 0, delay 20 ms | `Lo = Ro = 0.7071 · L(t − 20 ms)` |
| Voice 1, pan −45° / +45° / −22.5° | L only / R only / (0.9239, 0.3827) |
| Voice 1 delay 0 / 5 / 10 / 50 ms, Range 80, Align No | 7.256 / 7.256 / 10 / 50 ms |
| same, Range 20 | 24.671 / 24.671 / 24.671 / 50 ms |
| Voice 1 delay 0 / 50 ms, Align Yes, Range 80 | 7.256 / 57.256 ms |
| Routing A = (L+R)/2 | voice source `0.5(L + R)`; (L−R)/2 → `0.5(L − R)`; Routing B (R−L)/2 → `0.5(R − L)` |
| Voice 2 / 4 default routing | source R; Voice 3 → source L |
| Feedback 60 %, HP 16 Hz, delay 20 ms, pan −45° (impulse) | echoes at 20 ms + k·(20 ms + 1 sample), peaks 1.0, 0.5994, 0.3591, 0.2153 |
| Feedback 60 %, HP 578 Hz | 2nd/1st echo spectrum ratio = 0.6 · H_HP(f), first-order, |err| ≤ 0.05 dB |

### 11.2 EQ magnitude (dB, 48 kHz; tolerance ±0.15 dB, ±0.5 dB for high shelf above 4 kHz)
| Shelf | Freq set | Gain | 20 Hz | 100 Hz | 250 Hz | 1 kHz | 4 kHz | 10 kHz | 20 kHz |
|---|---|---|---|---|---|---|---|---|---|
| low | 281 | +12 | +11.92 | +10.34 | +6.16 | +0.79 | +0.05 | +0.01 | 0.00 |
| low | 281 | −12 | −11.70 | −7.44 | −2.61 | −0.21 | −0.01 | 0.00 | 0.00 |
| low | 281 | −88 | −38.44 | −12.70 | −3.69 | −0.28 | −0.02 | 0.00 | 0.00 |
| high | 1306 | +4.1 | 0.00 | +0.01 | +0.08 | +0.96 | +3.42 | +4.00 | +4.10 |
| high | 1189 | +12 | 0.00 | +0.07 | +0.44 | +4.44 | +10.80 | +11.84 | +11.99 |
| high | 1189 | −24 | 0.00 | −0.02 | −0.14 | −2.04 | −12.98 | −21.28 | −23.86 |
| high | 5040 | +12 | 0.00 | 0.00 | +0.03 | +0.40 | +4.24 | +9.68 | +11.88 |

### 11.3 Pitch shifter (statistical; voice 1 solo, panned left, delay 20 ms, Range 80 unless noted)
Reference values in brackets; acceptance limits after the arrow.
- Pitch of a 1 kHz sine (phase-slope over 3 s) for Detune ±1, ±10, ±50, ±100 cents: [±0.25 cents] → |err| ≤ 0.5 cents.
  Octave on: −1200 cents [−1204] → ±6 cents; Octave + 50: −1150 [−1154] → ±6 cents.
- Sine envelope (Hilbert), 1 % percentile: 1 kHz and 100 Hz at ±10…±100 cents [≥ 0.993] → ≥ 0.97;
  5 kHz at +50 cents [0.946] → ≥ 0.90.
- White noise: output RMS relative to input [−1.8 dB] → −1.8 ± 0.6 dB; 1/3-octave band deviation 100 Hz–16 kHz
  for ±10…±100 cents [≤ 2.5 dB] → ≤ 3 dB.
- Delay window (resampled cross-correlation trajectory on noise): all splice-free segments within `[Dn − Wh, Dn + Wh]`
  ± 0.5 ms; splice jumps within `[Jmin, Jmax]` ± 1 ms.
- Delay-parameter change: switches to the new delay within ≤ 10 ms with a click-free crossfade (reference: ≈ 5 ms).

### 11.4 Modulation
- Depth +100, Rate 1.0, Detune 0: triangle between −100 and +100 cents, period 1.0 s (decision D3), starting at 0
  rising. Depth −100: starting falling. Detune +20, Depth +100: between −80 and +120 cents.
- Reset: after a reset edge the voice is at its nominal detune and moves in the Depth direction.

### 11.5 Dynamics of parameters
Gain, pan, on/off and detune changes take effect within ≤ 5 ms without clicks (reference: essentially immediate,
residual < 1 % after 50 ms).

## 12. User interface (functional layout — visuals must be original)

Same controls and arrangement as the reference so users find everything in the same place; all artwork, colours,
fonts, names and logos must be the implementer's own, drawn in code.
- **Top row, three displays:** (1) a semicircular "stage" with one draggable dot per voice and one for the direct:
  angle = pan, radius = gain; (2) a delay/detune plane — x = delay 0…100 ms, y = detune ±100 cents — one draggable
  dot per voice (horizontal bar showing modulation depth); (3) the EQ curve (voices) with draggable L and H
  markers (double-click toggles that shelf).
- **Middle strip:** Direct on, gain knob, pan knob; Align (Yes/No); FB high-pass value; Range (80 Hz / 20 Hz);
  Modulation Reset button; EQ L: on, gain knob, freq box; EQ H: on, gain knob, freq box.
- **Voice rows** (one per voice, colour-coded): number/on button, gain knob, pan knob, delay box, feedback box,
  octave toggle (down arrow), detune box, depth box, rate box.
- **Right:** master output fader with stereo meters, peak counters (click to reset) and a clip indicator.
- **Bottom:** input routing selectors for voices 1/3 (L, L+R, L−R) and 2/4 (R, R+L, R−L).
- Value boxes: drag to change, double-click to type, Alt/double-click resets to default.

## 13. Open questions
1. Exact splice-search algorithm and crossfade length of the reference shifter (only statistics specified).
2. Mono→mono variant (not measured; out of scope).
3. EQ parameter smoothing (not measured; use ≤ 10 ms click-free smoothing).
4. Whether LFOs of different voices share phase at start (assumed: all start at phase 0 at plugin start).

## 14. Public references usable by the implementer
- U. Zölzer (ed.), "DAFX — Digital Audio Effects", chapters on delay-line pitch shifting and time-domain
  splicing (SOLA / WSOLA).
- W. Verhelst, M. Roelands, "An overlap-add technique based on waveform similarity (WSOLA)", ICASSP 1993.
- R. Bristow-Johnson, "Cookbook formulae for audio EQ biquad filter coefficients" (shelves, Q).
- J. Dattorro, "Effect Design, Part 2: Delay-Line Modulation and Chorus", J. Audio Eng. Soc. 45(10), 1997.
