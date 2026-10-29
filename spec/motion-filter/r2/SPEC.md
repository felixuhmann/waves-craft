# Modulated multimode filter — behavioural specification (r2)

Status: APPROVED r2 (2026-10-29), supersedes r1 — see "Changes from r1" at the end. Review decisions (standing policy: match the reference, do not reproduce
clear defects): (1) cutoff equals the knob at any sample rate (reference over-reads up to +13 % near 5 kHz at
48 kHz); (2) comb delay equals its displayed ms; (3) free delay time equals its displayed ms (reference doubles
it); (4) free-running sequencer starts on step 1; (5) no HF ripple at high sample rates. Frozen.

Derived from black-box measurement of a licensed reference processor in a VST3 host (REAPER, 48 kHz unless
stated; spot checks at 44.1/96 kHz) with user-generated stimuli, host-visible parameter metadata and public
documentation. No code, disassembly or vendor assets. Naming, IDs, GUI and branding are the implementer's own.

The reference is an analog-style, partly nonlinear and time-varying processor. This spec is therefore
**behavioural with tolerances** (§12), not sample-exact. Linear/small-signal parts are specified precisely.

## 1. Scope and variants
Stereo in / stereo out (primary). Optional variants: mono, mono-in → stereo-out (same engine; right channel
fed from the mono input). Latency: report ≤ 64 samples to the host (any internal oversampling must be
latency-compensated).

## 2. Signal flow (per channel)
```
in ──► Input gain ──┬──────────────────────────────────────────────────────────────► dry ──┐
                    └► [Delay (Route=Pre)] ► Drive/saturator ► VCF ► Crush ► wet chain ──► Mix ► Delay-mix ► Output gain ► out
                         ▲ (Route=Feedback: filter output feeds the delay line, see §8)
```
- **Mix** (§7) blends the dry signal with the processed (filter) signal. The **delay** (§8) has its own dry/wet.
- Modulation (§6) offsets filter Freq/Delay/Gain, Resonance/Feedback and delay Time.

## 3. Parameters (host-normalised r ∈ [0,1] → value)
| Parameter | Mapping | Range | Default |
|---|---|---|---|
| Filter type | 6 choices | LP, HP, BP, BR, Comb, Amp | LP |
| Slope | toggle | 12 / 24 dB/oct | 12 |
| Freq (LP/HP/BP/BR) | 30·600^r Hz | 30 Hz – 18 kHz | 18 kHz |
| Comb delay | 11 + 17 r ms | 11 – 28 ms | 28 ms |
| Amp gain | −80 + 80 r dB | −80 – 0 dB (−80 = mute) | 0 dB |
| Resonance / Comb feedback | 100 r % | 0 – 100 % | 0 % |
| Spread | −50 + 100 r % | ±50 % | 0 |
| Smooth | 100 r % | 0 – 100 % | 10 % |
| Drive | 100 r % | 0 – 100 % | 0 |
| Crush | 100 r | 0 – 100 | 0 |
| Input, Output | −40 + 50 r dB | −40 … +10 dB | 0 dB |
| Mix (filter) | −100 + 200 r | −100 (dry) … +100 (wet) | +100 |
| LFO shape | 5 choices | sine, triangle, saw, pulse, sample-and-hold | sine |
| LFO rate (free) | 0.1·1000^r Hz | 0.1 – 100 Hz | 4 Hz |
| LFO sync / rate | toggle / 12 choices | 4/1, 2/1, 1/1, 1/2, 3/8, 1/4, 3/16, 1/6, 1/8, 1/12, 1/16, 1/32 | on, 1/12 |
| Sequencer steps 1–16 | −100 + 200 r % | ±100 % | 0 |
| Sequencer last step | 1–16 | | 16 |
| Sequencer rate (free) | 0.1·5000^r steps/s | 0.1 – 500 | 4 |
| Sequencer sync / rate | toggle / choices | 1/1, 1/2, 3/8, 1/4, 3/16, 1/6, 1/8, 1/12, 1/16, 1/32 (step length) | on, 1/32 |
| Swing | 75 r % | 0 – 75 % | 0 |
| Delay time (free) | 50·20^r ms | 50 – 1000 ms | 415 ms |
| Delay sync / rate | toggle / choices | 1/32, 1/16, 1/12, 1/8, 1/6, 3/16, 1/4, 3/8, 1/2, 1/1 | on, 1/8 |
| Delay right ratio | 3 choices | ×1, ×1.5, ×2 | ×1 |
| Delay feedback | 100 r % | 0 – 100 % | 30 % |
| Delay mix | −100 + 200 r | −100 (dry) … +100 (delay only) | −100 |
| Delay mode | toggle | digital / analog | digital |
| Delay route | toggle | pre-filter / feedback-through-filter | pre |
| Follower source | toggle | input / sidechain | input |
| Keyboard tracking | toggle | off / on (MIDI) | off |
| Modulation amounts | −100 + 200 r % each | ±100 % | 0 |
Nine modulation amounts: {LFO, Sequencer, Follower} × {Freq/Delay/Gain, Res/Feedback, Delay time}.
Sync-rate choice lists map r uniformly onto the list (nearest).

## 4. Fixed processing chain
- **Wet chain**: 2nd-order high-pass ≈ 21 Hz (Butterworth; −3.6 dB at 20 Hz, −0.15 dB at 50 Hz) and +0.35 dB
  broadband gain.
- **Output HF response (both dry and wet)** at 44.1/48 kHz: −0.3 dB @10 kHz, −1.2 @15 kHz, −2.1 @18 kHz,
  −3.2 @20 kHz, −4.8 @22 kHz (consequence of internal resampling). Reproduce within §12 tolerance; at higher
  sample rates the response must be flat ±0.5 dB to 20 kHz (review decision: the reference's ±1.5 dB HF ripple
  at 96 kHz is not reproduced).
- **Baseline saturation**: even at Drive 0 the wet path compresses ≈0.2 dB for a −3 dBFS input.

## 5. Filter (VCF)
Per channel, a 2nd-order resonant section; Slope 24 = two identical sections in cascade (same fc, Q).
Continuous-time prototypes, with s = jω/ωc:
- LP: 1/D, HP: s²/D, **BP: s/D** (peak gain = Q at fc, i.e. +3.0 dB at Res 50, +6.5 dB at Res 90; slope 24 =
  squared), BR: (1+s²)/D, where D = 1 + s/Q + s².
- **Cutoff**: fc = Freq knob (plus modulation, §6, and spread). Review decision: the reference over-reads by
  up to +13 % near 5 kHz at 48 kHz (sample-rate-dependent warping; ≈ exact at 96 kHz) — do NOT reproduce;
  use a correctly prewarped/ZDF design so fc equals the knob at any rate. Clamp fc to [30 Hz, 18.6 kHz].
- **Resonance → Q**: 1/Q = 1 − 0.00586·Res for Res ≤ 90 (Q ≈ 1.0 at 0, 1.41 at 50, 2.12 at 90); above 90 Q
  rises linearly to **Q = 2.40 at Res 100** (measured: 2.2 at 95; 2.30–2.57 at 100 depending on type/cutoff).
  Q is **level-independent** (measured constant within ±0.05 from −30 to −3 dBFS input at Res 90–100); no
  level-dependent resonance limiting. No self-oscillation without input is required.
- **Spread**: left cutoff ×2^(+0.0144·Spread%), right ×2^(−0.0144·Spread%) (±0.72 oct at ±50 %).
- **Comb**: wet = feedback comb y[n] = x[n−D] + g·y[n−D] (no direct term). D = displayed delay (review
  decision: the reference's actual delay is 6.2 + 1.402·(display−11) ms, i.e. 6.2–30 ms for a displayed
  11–28 ms; implement the displayed value). g = 0.50 + 0.0029·Feedback% (0.50 at 0 %, 0.65 at 50 %,
  0.79 at 95 %).
- **Amp**: gain = displayed dB (−80 dB ⇒ silence).
- Measured small-signal fits (reference at 48 kHz, after removing §4 chain; analog prototype fc/Q):

| Type | Knob Hz | Res | Slope | fit fc | fit Q |
|---|---|---|---|---|---|
| BP | 100 | 0 | 12 | 99 | 0.98 |
| BP | 100 | 0 | 24 | 100 | 0.96 |
| BP | 100 | 50 | 12 | 99 | 1.42 |
| BP | 100 | 50 | 24 | 100 | 1.39 |
| BP | 100 | 90 | 12 | 99 | 2.21 |
| BP | 100 | 90 | 24 | 100 | 2.14 |
| BP | 1000 | 0 | 12 | 1019 | 0.95 |
| BP | 1000 | 0 | 24 | 1019 | 0.95 |
| BP | 1000 | 50 | 12 | 1013 | 1.37 |
| BP | 1000 | 50 | 24 | 1013 | 1.37 |
| BP | 1000 | 90 | 12 | 1006 | 2.07 |
| BP | 1000 | 90 | 24 | 1007 | 2.08 |
| BP | 5000 | 0 | 12 | 5602 | 0.90 |
| BP | 5000 | 0 | 24 | 5601 | 0.90 |
| BP | 5000 | 50 | 12 | 5401 | 1.32 |
| BP | 5000 | 50 | 24 | 5400 | 1.32 |
| BP | 5000 | 90 | 12 | 5259 | 2.06 |
| BP | 5000 | 90 | 24 | 5258 | 2.06 |
| BR | 100 | 0 | 12 | 100 | 1.02 |
| BR | 100 | 0 | 24 | 100 | 1.01 |
| BR | 100 | 50 | 12 | 100 | 1.46 |
| BR | 100 | 50 | 24 | 100 | 1.44 |
| BR | 100 | 90 | 12 | 100 | 2.26 |
| BR | 100 | 90 | 24 | 100 | 2.21 |
| BR | 1000 | 0 | 12 | 1000 | 0.97 |
| BR | 1000 | 0 | 24 | 998 | 0.97 |
| BR | 1000 | 50 | 12 | 1000 | 1.39 |
| BR | 1000 | 50 | 24 | 999 | 1.39 |
| BR | 1000 | 90 | 12 | 1000 | 2.13 |
| BR | 1000 | 90 | 24 | 1000 | 2.13 |
| BR | 5000 | 0 | 12 | 5020 | 0.88 |
| BR | 5000 | 0 | 24 | 4996 | 0.88 |
| BR | 5000 | 50 | 24 | 5003 | 1.30 |
| BR | 5000 | 90 | 24 | 5008 | 2.04 |
| HP | 100 | 0 | 12 | 101 | 1.03 |
| HP | 100 | 0 | 24 | 101 | 1.02 |
| HP | 100 | 50 | 12 | 101 | 1.50 |
| HP | 100 | 50 | 24 | 101 | 1.46 |
| HP | 100 | 90 | 12 | 101 | 2.32 |
| HP | 100 | 90 | 24 | 101 | 2.24 |
| HP | 1000 | 0 | 12 | 1017 | 0.98 |
| HP | 1000 | 0 | 24 | 1018 | 0.98 |
| HP | 1000 | 50 | 12 | 1011 | 1.41 |
| HP | 1000 | 50 | 24 | 1012 | 1.41 |
| HP | 1000 | 90 | 12 | 1006 | 2.17 |
| HP | 1000 | 90 | 24 | 1007 | 2.16 |
| HP | 5000 | 0 | 12 | 5534 | 0.91 |
| HP | 5000 | 0 | 24 | 5532 | 0.91 |
| HP | 5000 | 50 | 12 | 5357 | 1.35 |
| HP | 5000 | 50 | 24 | 5356 | 1.35 |
| HP | 5000 | 90 | 12 | 5231 | 2.10 |
| HP | 5000 | 90 | 24 | 5231 | 2.10 |
| LP | 100 | 0 | 12 | 98 | 0.88 |
| LP | 100 | 50 | 12 | 98 | 1.27 |
| LP | 100 | 90 | 12 | 98 | 1.92 |
| LP | 1000 | 0 | 12 | 1022 | 0.92 |
| LP | 1000 | 0 | 24 | 1021 | 0.96 |
| LP | 1000 | 50 | 12 | 1015 | 1.32 |
| LP | 1000 | 50 | 24 | 1014 | 1.38 |
| LP | 1000 | 90 | 12 | 1011 | 2.00 |
| LP | 1000 | 90 | 24 | 1010 | 2.10 |
| LP | 5000 | 0 | 12 | 5674 | 0.88 |
| LP | 5000 | 0 | 24 | 5675 | 0.88 |
| LP | 5000 | 50 | 12 | 5447 | 1.30 |
| LP | 5000 | 50 | 24 | 5448 | 1.30 |
| LP | 5000 | 90 | 12 | 5288 | 2.02 |
| LP | 5000 | 90 | 24 | 5288 | 2.02 |

  (The 5 kHz rows show the reference's warping over-read that this spec does not require.)

## 6. Modulation
All modulation offsets are summed per destination, slewed by **Smooth** (one-pole, τ = Smooth % × 1 ms;
0 % = no slew), then applied.
- **Depth taper**: amount a ∈ [−1,1] acts through T_k(|a|)·sign(a) with T_k(x) = (e^(k·x) − 1)/(e^k − 1):
  k = 3.04 for sequencer and follower amounts, k = 3.57 for LFO amounts.
- **Freq destination** (octaves added to cutoff): sequencer 5.17·v·T(a) (v = step value ∈ [−1,1]);
  LFO 9.23·l(t)·T(a) (l ∈ [−1,1] waveform); follower see §6.3.
  Measured: seq a=100 %: v=0.1 → +0.51 oct, 0.25 → +1.28, 0.5 → +2.62; v=100 %: a=10 % → +0.09,
  25 → +0.30, 40 → +0.62, 50 → +0.93, 60 → +1.36, 75 → +2.31 oct. LFO a=25 % → ±0.39 oct, 50 % → ±1.33 oct.
- **Res destination**: sequencer adds 100·v·T(a) percentage points (clamped 0–100).
- **Amp destination** (gain): linear gain g = max(0, g_knob + 0.0647·(e^(1.502·m) − 1)), m = the sequencer-scale
  offset v·T(a) (LFO: m = 1.43·l·T(a)); any negative m large enough mutes. (Approximate; measured at knob −40 dB.)
- **Comb delay, delay time** destinations: same normalised scaling as Freq (offset in units of the knob's
  log range, full range = 1.0) — not measured; implementer's choice within that rule.
### 6.1 LFO
Waveforms (phase 0 at transport start / plugin start): sine sin(2πφ); triangle starting at 0 rising; saw
falling from +1 to −1; pulse +1 for the first half cycle, −1 for the second (50 % duty); S&H a new uniform
random value in [−1,1] each cycle. Free rate exact (±0.1 %); synced rates are note lengths at host tempo
(e.g. at 120 BPM: 1/2 = 1.000 s, 1/6 = 0.333 s, 1/12 = 0.167 s per cycle). Same phase on both channels.
### 6.2 Sequencer
Steps 1..Last loop; free rate = steps per second; synced = one step per note length. Starts at step 1 on
transport/plugin start (review decision: the reference starts its free-running sequence on step 2 — not
reproduced). Swing delays every second step by Swing % of a step (implementer's choice of exact law;
not measured). Output is a stepped signal (then Smooth).
### 6.3 Envelope follower
Per channel (no cross-channel linking), from the input (or sidechain bus when Source = sidechain). Fast peak
follower (attack ≤ 5 ms, release ≤ 25 ms). Freq-destination offset vs the follower's steady-state peak amplitude A
(full scale = 1; steady 1 kHz sine bursts; HP type, Smooth 0) — normative targets:

| Amount | A = 0.01 (−40 dBFS) | A = 0.072 (−22.8) | A = 0.135 (−17.4) | A = 0.26 (−11.7) | A = 0.51 (−5.8) |
|---|---|---|---|---|---|
| +50 % | +0.06 oct | +0.43 | +0.82 | +1.60 | +1.79 |
| −50 % | −0.06 | −0.42 | −0.83 | −1.66 | ≤ −4 (to the 30 Hz clamp region) |
| +100 % | +0.60 | +1.80 | +1.80 | +1.81 | +1.82 |

i.e. ≈ 6.4·A octaves at ±50 % for small A, saturating near +1.8 oct for positive offsets. Tolerance ±0.2 oct or
±25 % (whichever larger). With Smooth > 0 the offset is the smoothed (lower) value.

### 6.4 Keyboard tracking
Not measurable in the test host. Implement: MIDI note n offsets Freq by (n − 60)/12 octaves (Comb: delay
×2^(−(n−60)/12); Amp: none). Implementer's choice; document.

## 7. Mixer
Input and Output are plain gains (dB). Filter Mix m ∈ [−1,1]: dry = (1 − m)/2, wet = (1 + m)/2 (linear).

## 8. Delay
- Time: free = displayed ms (review decision: the reference's free-mode delay is 2× the displayed value; synced
  values are exact — implement displayed). Right channel time × {1, 1.5, 2}.
- Feedback: each repeat = previous × Feedback % (exactly linear; 100 % sustains indefinitely — keep that but
  guard against blow-up with a soft limiter).
- Delay mix (x = (m+1)/2): dry/delay-tap gains (first tap, feedback 0):

| m | −100 | −75 | −50 | −25 | 0 | +25 | +50 | +75 | +100 |
|---|---|---|---|---|---|---|---|---|---|
| dry | 1.000 | 0.971 | 0.888 | 0.761 | 0.603 | 0.432 | 0.264 | 0.116 | 0.000 |
| delay | 0.000 | 0.193 | 0.368 | 0.509 | 0.603 | 0.646 | 0.639 | 0.586 | 0.500 |
- Route pre: delay (with its dry/wet) before the filter. Route feedback: filter (and saturator) inside the
  delay feedback loop (each repeat filtered again).
- Analog mode: identical to digital for a constant time; on time changes analog glides (pitch-bend, one-pole
  ≈ 100 ms on the read position) while digital switches with a short (≈ 20 ms) crossfade. Not measured.
## 9. Drive and Crush
- Drive: input boost into a soft saturator. Small-signal gain +0.073 dB per Drive % (+1.9 dB at 25 %, +3.7 at
  50 %, +7.2 at 100 %). Compression at −3 dBFS input: 0.4 / 0.8 / 2.7 dB at 25 / 50 / 100 %. Odd-dominant,
  low-order harmonics.
- Crush: sample-and-hold of the filter output with hold length N = 1 + 64·(Crush/100)² samples at 48 kHz, i.e.
  hold time T_h = N / 48 000 s independent of the host rate (Crush 25 → 5 samples ≈ 9.6 kHz, 50 → 17 ≈ 2.8 kHz,
  75 → 37 ≈ 1.3 kHz, 100 → 65 ≈ 740 Hz). Output level is preserved (held sample values). 0 = off.
## 10. Smoothing of parameter changes
User parameter changes must be click-free (≥ 5 ms ramps). Not further specified.
## 11. Not specified
Exact saturator curve, LFO S&H random sequence, swing law, analog-delay glide law, follower ballistics
beyond §6.3, mono variants' internal routing, MIDI learn.
## 12. Tolerances
- Small-signal magnitude (−30 dBFS) where |H| > −20 dB: ±1.0 dB for 12 dB LP/HP/BR, ±1.5 dB for BP and
  24 dB types, cutoff ±3 %, Q ±10 % (vs. §5 with the decisions applied).
- §4 chain: ±0.5 dB below 10 kHz; −5 … +0.5 dB above 15 kHz.
- Modulation offsets: ±0.15 oct or ±8 % (whichever larger); rates/times ±0.5 %; Smooth τ ±20 %.
- Delay times ±1 sample; feedback ratios ±0.01; delay-mix gains ±0.03.
- Drive gains/compression ±0.5 dB. Crush hold length ±1 sample (at 48 kHz).
## 13. UI (functional layout — visuals must be original)
Nine grouped sections: filter-type selector (6 types); global (Spread, Smooth, Drive, Crush knobs); filter
controls (Freq/Delay/Gain knob and Res/Feedback knob, each surrounded by three small bipolar modulation-amount
sliders labelled LFO/SEQ/FOLLOW, plus Slope 12/24 and KBD toggles); mixer (Mix, Input, Output); LFO (rate knob,
sync toggle, five shape buttons); sequencer (16 bipolar step sliders, Last-step selector, Swing, Rate, Sync,
Drag/Draw edit-mode toggle); delay (Time knob with three modulation sliders, Right ratio selector, Sync, Feedback,
Dry/Wet, Analog/Digital, Route Pre/Fbk); follower source toggle (Input/SC); a response display drawing the
current filter curve that can be dragged (x = Freq/Delay, y = Res/Feedback). All drawing original
(vector graphics), own colours, fonts and names.
## 14. Reference-model accuracy (analyst side, for information)
Small-signal 2nd-order fits: median max-error 0.65 dB, worst 3.4 dB (24 dB band-pass skirts) over 67 configs.
Sequencer freq-offset law matches 10 measured points within 0.03 oct (0.25 oct at 90 % depth near the clamp).
Delay taps, feedback ratios and right-channel ratios match exactly (±1 sample, ±0.002).
Carry-over check: settled double-render identical to single render; L/R-input responses consistent.

## 15. Public references
Zavalishin, "The Art of VA Filter Design" (ZDF/TPT SVF); Zölzer (ed.), "DAFX"; standard comb/delay, LFO,
step-sequencer and envelope-follower designs; sample-rate reduction ("decimation") effects.

## Changes from r1
| § | r1 | r2 (verified against the reference, 2026-10-29) |
|---|---|---|
| 5 | BP = (s/Q)/D (peak 0 dB) with text "peak gain = Q" (contradictory) | BP = s/D: peak gain = Q (reference: +2.9 dB at Res 50, +6.4 dB at Res 90, +7.6 dB at Res 100) |
| 5 | Q rises steeply to 4.6 at Res 100; level-dependent limiting (≈2.5 at −3 dBFS) | Q rises linearly 2.12 → 2.40 from Res 90 to 100; level-independent |
| 6.3 | follower offset = 23.4·A·T(a)/T(0.5) oct | measured target table; ≈ 6.4·A oct at ±50 %, saturating ≈ +1.8 oct; per channel |
| 9 | Crush f_h = 48 000·2^(−0.19·Crush) Hz | hold length N = 1 + 64·(Crush/100)² samples at 48 kHz; level preserved |
| 12 | Crush hold rate ±10 % | hold length ±1 sample |
Known remaining approximation (unchanged from r1, informative): the Amp-destination LFO law (§6) under-predicts the
reference's swing at small amounts (25 %: formula ≈ 11 dB peak-to-peak, reference ≈ 15 dB); treat as approximate.
