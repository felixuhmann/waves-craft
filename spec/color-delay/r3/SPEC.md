# Multi-effect delay — behavioural specification (r3)

Status: APPROVED r3 (2026-10-29; supersedes r2 — see "Changes from r2" at the end). Review decisions (standing user policy — match the reference, do not reproduce
defects, original visuals, own naming; ambiguities resolved by the analyst):
1. Effects are fed post wet gain (measured) although the vendor manual states otherwise — measured behaviour is normative.
2. The reference host-side quirk where parameter changes apply asynchronously is a vendor implementation artefact
   and is not part of this spec.
3. Character effects (reverb, distortion, doubler, phaser) are specified by measured targets, not bit-exact.
4. Mono-to-stereo variant: mono input feeds both channels (assumption).
Frozen — changes require a new revision.

Derived only from (a) black-box measurement of the licensed reference processor in a VST3 host using
user-generated test signals (sweeps, impulses, tone staircases/steps/bursts, impulse trains), (b) host-visible
parameter metadata, and (c) the reference's public/licensed user documentation (paraphrased). No code,
disassembly or vendor assets were used or are included. Naming, parameter IDs, GUI and branding are the
implementer's own.

Accuracy classes used below:
- **[exact]** deterministic linear behaviour; reference matched by the analyst's model to ≤ −90 dB re peak.
- **[model]** nonlinear/time-variant behaviour specified by a fitted model; tolerances in §10.
- **[target]** character effects specified by measured targets; the algorithm is the implementer's design.

## 1. Scope and variants
Stereo delay with a compressed wet path and four parallel colour effects.
- **Stereo** variant: 2 in / 2 out.
- **Mono-to-stereo** variant: 1 in / 2 out; the mono input feeds both channels' inputs identically (assumption,
  not separately measured).

Latency: 0 samples for the dry/delay path. The minimum delay-line delay is 1 sample (setting 0 ms = 1 sample).

## 2. Parameters
Host-normalised value `r ∈ [0,1]`; quantise to the stated resolution.

| Parameter | Mapping | Range | Default | Resolution |
|---|---|---|---|---|
| Input | −18 + 36 r dB | −18 … +18 dB | 0 dB | 0.1 dB |
| Output | −18 + 36 r dB | −18 … +18 dB | 0 dB | 0.1 dB |
| Mode | toggle | ms / Host-sync | Host | — |
| Delay L ms, Delay R ms | 2000 r ms, rounded to 4 ms | 0 … 2000 ms | 500 | 4 ms |
| Delay L note, Delay R note | 19 values evenly spaced over r: 1/64T, 1/64, 1/32T, 1/64D, 1/32, 1/16T, 1/32D, 1/16, 1/8T, 1/16D, 1/8, 1/4T, 1/8D, 1/4, 1/2T, 1/4D, 1/2, 1/2D, Whole | | 1/4 | — |
| Feedback L, Feedback R | 100 r % | 0 … 100 % | 29.9 % | 0.1 % |
| Delay Link | toggle | off / on | on | — |
| Dry/Wet | 100 r % | 0 … 100 % | 100 % | 0.1 % |
| HP | 16·(21357/16)^r Hz; r = 0 (16 Hz) = **off** | 16 … 21357 Hz | off | 1 Hz |
| LP | 16·(21357/16)^r Hz; r = 1 (21357 Hz) = **off** | 16 … 21357 Hz | off | 1 Hz |
| HP/LP Link | toggle | off / on | off | — |
| Verb On, Dist On, Doubler On, Phaser On | toggles | | all off | — |
| Verb Size | 1 + 99 r % | 1 … 100 % | 54.9 % | 0.1 % |
| Verb Level, Dist Level, Doubler Level, Phaser Level | 100 r % | 0 … 100 % | 50 % | 0.1 % |
| Dist Send | 100 r % | 0 … 100 % | 82.2 % | 0.1 % |
| Dist Width | 100 r % | 0 … 100 % | 0 % | 0.1 % |
| Doubler Detune | −100 + 200 r cents | −100 … +100 | +8 | 1 cent |
| Phaser Rate | 20·exp(8.28·(r − 1)) Hz | ≈0.005 … 20 Hz | 0.34 Hz | 0.01 Hz |
| Monitor | 3-way: L / L+R / R (r < 0.25 → L, r > 0.75 → R) | | L+R | — |

Delay Link on: the right channel uses the left channel's delay time (ms or note) and feedback. Turning link off keeps
the right channel at the values it had while linked (the manual: previous right settings are not restored).
HP/LP Link on: moving one filter moves the other inversely so that `f_HP · f_LP` stays constant (manual example:
LP 8 kHz, HP 150 Hz → HP 600 Hz moves LP to ≈2 kHz); the controls stop when they meet.

## 3. Signal path (per channel unless stated)
1. **Input gain** `xi = x · 10^(Input/20)`. [exact]
2. **Dry path**: `xi`.
3. **Wet path**:
   a. **Feedback delay** [exact]: `w[n] = xi[n − D] + g · w[n − D]`, `g = Feedback/100` (exactly linear,
      0 … 1.0; 100 % repeats forever without loss). Each channel has its own D and g; there is **no** cross-feed.
      `D = max(1, round(delay_ms · fs / 1000))` in ms mode. In Host mode `D = max(1, floor(N · fs))` where
      `N = beats · 60 / BPM` is the exact (rational) note duration, limited to 2.0 s, evaluated so that results that
      are mathematically integers are not lost to rounding (e.g. compute `floor(beats_num · 60 · fs / (beats_den · BPM))`
      in integer/rational arithmetic, or add ≤1e-9 before flooring). Examples @48 kHz: 1/4 at 120 BPM = 24000;
      1/16T at 120 BPM = 4000; 1/16T at 100 BPM = 4800; 1/8D at 100 BPM = 21600; 1/16T at 137 BPM = 3503.
      (The reference occasionally lands one sample off this rule, e.g. 1/16T at 120 BPM = 3999; that irregularity is
      not reproduced.) Delay lines are integer-sample (no interpolation).
   b. **HP then LP filter** (outside the feedback loop; each applied once) [exact] — see §4. Off positions bypass.
   c. **Compressor**, stereo-linked, see §5. [model]
   d. **Wet gain** from the Dry/Wet law (§6).
4. **Colour effects** (§7) take the signal of step 3d as their input — i.e. **after the compressor's gain reduction**
   and after the wet gain — run in parallel, and their outputs are added to the mix (each scaled by its level; an effect
   that is off contributes nothing). [target] Measured: the output level of every effect follows the compressor's gain
   reduction exactly (e.g. doubler effect gain re plugin input: +13.0 dB below threshold, +9.25 dB at a −20 dBFS
   1 kHz tone where the compressor reduces by 3.75 dB, −4.5 dB at 0 dBFS where it reduces by 17.6 dB). Feeding the
   effects from before the compressor is non-conformant.
5. **Mix**: `y = dry_gain · xi + wet_out + Σ effect outputs`.
6. **Output gain** `10^(Output/20)`. [exact]
7. **Monitor**: L → right output silent; R → left output silent; L+R → both. [exact]

## 4. HP / LP filters [exact]
Third-order complementary pair with a fixed shape parameter `a = 0.132316` (±2e−6), designed in the analog domain
(normalised to the cutoff `fc`) and discretised with the bilinear transform prewarped at `fc` at the actual sample
rate `fs`:

```
D(s)  = (s + 1)(s² + (1 + a)s + 1)
LP(s) = (1 − a s²) / D(s)
HP(s) = (s³ − a s) / D(s)
s → (1 − z⁻¹) / (k (1 + z⁻¹)),   k = tan(π fc / fs)
```
Properties: |LP| = |HP| = −3.01 dB at fc; LP + HP is an allpass. Sample-rate independent (verified 44.1/48/96 kHz).

## 5. Compressor [model]
Feed-forward, stereo-linked, on the wet path after the filters, before the wet gain.
- Detector: `lvl = max(|L|, |R|)`; envelope `e` with instant attack and one-pole release τ = 1.06 ms (linear amplitude).
- Static curve on `X = 20·log10(e)` (dBFS): soft knee, threshold T = −23.23 dB, ratio R = 6.196, knee width W = 23.22 dB:
  `GR = 0` for `2(X−T) < −W`; `GR = (X−T)(1 − 1/R)` for `2(X−T) > W`; otherwise `GR = (1 − 1/R)(X − T + W/2)² / (2W)`.
- Gain smoothing on GR (dB): one-pole, attack τ = 8.17 ms (when GR rises), release τ = 439 ms (when GR falls).
- Output gain `G = (1 − p)·10^(−GR/20) + p` with parallel (uncompressed) fraction p = 0.0292, applied equally to both channels.
- Measured static gain at 1 kHz (sine peak level → gain): −33 dBFS: −0.10 dB; −27: −1.07; −21: −3.25; −15: −6.78;
  −9: −11.16; −3: −15.58; 0: −17.56; +6: −20.83; +12: −23.07 (via Input gain). These measured values are
  normative (±0.3 dB). The model above gives −21.18 / −24.19 dB at +6 / +12 dBFS; above ≈ +4 dBFS the curve must
  ease so that the measured values are met (any smooth easing that leaves levels ≤ +2 dBFS unchanged is acceptable).
- Frequency dependence at −6 dBFS (gain): 30 Hz −10.3 … −12.0 (depends on the analysis window), 60 Hz −12.8 … −13.1,
  120 Hz −13.3, 250 Hz – 4 kHz −13.4, 8 kHz −12.9 … −13.2, 16 kHz −12.6 … −12.8 dB. The §5 model gives −13.1 dB at 30 Hz;
  tolerance at 30–60 Hz is ±2.5 dB, elsewhere ±0.5 dB.
- Known deviation: for isolated single-sample transients the reference reduces the peak by 0.2–0.9 dB
  (0.5 FS impulse → ×0.908), the model by less; see §10.

## 6. Dry/Wet law [exact]
Wet gain (dB) at Dry/Wet = 0, 5, …, 100 %: −∞, −26.2, −20.0, −17.0, −14.0, −12.2, −10.5, −9.2, −8.0, −7.0, −6.0,
−5.2, −4.4, −3.8, −3.1, −2.5, −1.9, −1.4, −0.9, −0.4, 0.0. Dry gain(m) = wet gain(100 − m). Between grid points
interpolate linearly in dB (between 0 and 5 %, linearly in amplitude). Verified at every 5 % point (≤ −140 dB re peak).

## 7. Colour effects [target]
All four take the same post-wet-gain stereo signal (§3.4) and are therefore silent when Dry/Wet = 0 % (measured;
this contradicts the vendor manual's statement — the measured behaviour is normative).
Channel routing (measured with one-sided input): **distortion and doubler process each channel independently** —
a left-only input produces output only on the left channel and vice versa (no cross-channel mixing, no mono sum).
The reverb is a stereo reverb: a one-sided input produces decorrelated output on both channels. Levels below are the effect
output relative to the effect input, at Level = 100 %. Level law (all effects): output gain falls steeply with Level —
measured −17.5 dB (reverb) / −21 dB (distortion, doubler) at 50 % and −37 dB (reverb) at 25 %; use
`gain = (Level/100)^3` for the reverb and `(Level/100)^3.5` for the other three.

### 7.1 Reverb
Stereo, decorrelated outputs (L/R correlation of the tail ≈ 0 for one-sided input). Pre-delay ≈ 13 + 0.11·Size ms.
Impulse-response energy for a left-only input impulse, `E = Σ(hL² + hR²)` (sum over both output channels), ≈ 0 dB
(−0.7 dB at Size 1 %, +0.5 dB at 50 %, +0.8 dB at 100 %); the right-channel energy is 1.6–1.7 dB below the left
(informative). Octave-band energy of E relative to the 1 kHz band (Size 1 / 50 / 100 %):
63 Hz −1.0/+0.1/−0.1, 125 Hz +0.5/+0.8/+1.0, 250 Hz −0.7/−0.6/+0.3, 500 Hz −0.2/+0.2/+0.4, 2 kHz −2.2/−1.8/−1.3,
4 kHz −4.3/−3.5/−3.3, 8 kHz −6.5/−5.4/−5.1, 16 kHz −9.3/−7.8/−7.2 dB. RT60 (s) by octave band:

| Size | 125 | 250 | 500 | 1k | 2k | 4k | 8k Hz |
|---|---|---|---|---|---|---|---|
| 1 % | 0.93 | 0.89 | 0.84 | 0.83 | 0.72 | 0.61 | 0.48 |
| 25 % | 1.56 | 1.53 | 1.46 | 1.36 | 1.27 | 1.10 | 0.87 |
| 50 % | 3.05 | 2.99 | 2.90 | 2.74 | 2.44 | 2.05 | 1.61 |
| 75 % | 3.80 | 3.86 | 3.55 | 3.33 | 3.06 | 2.60 | 2.02 |
| 100 % | 4.92 | 4.65 | 4.50 | 4.20 | 3.84 | 3.25 | 2.53 |

Interpolate between sizes. Algorithm: implementer's choice (e.g. a feedback-delay-network reverb).

### 7.2 Distortion
Symmetric soft saturation (odd harmonics only; 2nd harmonic < −95 dB re fundamental), preceded by a band-pass
voicing and followed by Level, **per channel** (see §7). Send acts as drive. All levels below are per channel, measured
end-to-end re the **plugin input** (Delay 100 ms, Feedback 0, Dry/Wet 100 %, effect output isolated by subtraction);
they therefore include the compressor's gain reduction at levels above ≈ −34 dBFS (§5) — the distortion stage's own
transfer is the table value minus the compressor gain at that level. At Send ≤ 25 % the distortion stage itself is
linear in level (the end-to-end decline is the compressor). Identical for one-sided and centred (L = R) input. Small-signal response (−40 dBFS sweep):
Send 50 %: 50 Hz −7.9, 100 Hz +12.1, 200 Hz +14.7, 500 Hz +13.9, 1 kHz +11.7, 2 kHz +6.4, 4 kHz −2.7, 8 kHz −17.8,
16 kHz −59.6 dB; Send 75 %: 100 Hz +23.2, 1 kHz +24.4, 4 kHz +10.8 dB; Send 100 %: 100 Hz +26.1, 1 kHz +28.7,
2 kHz +30.8, 4 kHz +24.3, 8 kHz +11.3 dB; Send 25 %: 1 kHz −1.7 dB (output present at −40 dBFS input).
1 kHz gain vs input level (Send 100 %): −40: +28.7, −32: +21.3, −24: +13.5, −16: +5.5, −8: −2.5, 0 dBFS: −10.5 dB;
(Send 50 %): −40: +11.7, −28: +10.9, −24: +8.7, −16: +2.3, −8: −4.9, 0: −12.3 dB; (Send 25 %): −40: −1.8, −32: −2.0,
−24: −3.7, −16: −7.9, −8: −13.7, 0: −19.3 dB; (Send 10 %): gated ≤ −32, −28: −10.7, −24: −11.8, −20: −13.6, −16: −16.0,
−12: −18.8, −8: −21.8, −4: −24.7, 0: −27.4 dB. Send 25 % small-signal response: 100 Hz −5.7, 1 kHz −1.7, 2 kHz −10.8,
4 kHz −23.7, 8 kHz −40.6 dB. Latency ≈ 6.8 ms (beyond the delay).
Saturation at 1 kHz (Send 100 %): 3rd harmonic −9 dB re fundamental at −40 dBFS input rising to −5 dB at 0 dBFS;
(Send 50 %) negligible below −28 dBFS, −20 dB at −24 dBFS, −11 dB at 0 dBFS. A gate silences the distortion output
when its (post-drive) input is low: at Send 10 % the output is silent for inputs ≤ −32 dBFS, at Send 0 % for
≤ −28 dBFS (per-channel input level). Width: 0 % = both channels use identical processing (identical inputs give
identical outputs); 100 % = the two channels' processing differs so that identical L/R inputs give partially
decorrelated outputs (1 kHz tone: correlation ≈ 0.6) at equal level (±0.5 dB). Width never mixes channels.
Informative (not normative): at Width 100 % the reference's per-channel voicing also changes (Send 50 %, small signal:
100 Hz +18.1, 1 kHz +17.4, 2 kHz −0.2, 4 kHz −2.6, 8 kHz −19.1 dB) and its tone gain varies irregularly with level.
Exact voicing is the implementer's design within §10 tolerances.

### 7.3 Doubler
Pitch shifter, per channel: the output is the input shifted by exactly `Detune` cents (±0.3 cent), same sign on both
channels, **+13.0 dB re its input** (the post-compressor wet signal, §3.4) at Level 100 %, latency 3–10 ms beyond the
delay. End-to-end re plugin input this gives +12.9 dB for quiet signals (−40 dBFS sweep or tone) and +9.25 dB for a
−20 dBFS 1 kHz tone (compressor −3.75 dB). (r1/r2 quoted "+9.2 dB re its input"; that was the −20 dBFS end-to-end
value including the compressor.) Algorithm: implementer's choice
(e.g. dual-grain delay-line pitch shifter with crossfades).

### 7.4 Phaser
Multi-notch (≥ 4 notches) sweeping phaser on the effect input, LFO rate = Phaser Rate (Hz), notches sweeping across
roughly 2–15 kHz. Not characterised in detail (impulse-snapshot sampling aliased at higher rates); design is the
implementer's choice: e.g. 6–8 first-order allpass stages with feedback, LFO rate as displayed.

## 7.5 Overload behaviour
With sustained loud input at Feedback 100 % the reference hard-clips its wet/feedback path and its output at ±1.0
(0 dBFS), so the loop saturates at full scale. Review decision: this internal clipping of a floating-point signal path is
not reproduced. Implementations must keep the feedback loop and the output bounded for any input (e.g. a soft limit that
is transparent below +24 dBFS with a ceiling ≤ +30 dBFS); behaviour below +24 dBFS is governed by §3–§7.

## 8. Parameter changes
Delay-time changes: not characterised; implement click-free (e.g. short crossfade between read positions).
Other controls: smooth with ≈ 20 ms one-pole smoothing (not characterised).

## 9. Behaviour not specified
GUI metering ballistics, presets, the vendor's preset names, mono-to-stereo variant internals beyond §1.

## 10. Tolerances for acceptance
- [exact] parts (delay/feedback/filters/mix/gains/monitor, compressor inactive i.e. signals ≤ −40 dBFS):
  ≤ −90 dB re peak against the §3–§6 equations.
- Compressor: static gain within ±0.3 dB of §5 values at 1 kHz; envelope (10 ms RMS) error vs the §5 model ≤ 0.5 dB RMS
  on level steps/bursts; transients: isolated impulse peaks may differ from the reference by ≤ 1 dB.
- Reverb: RT60 per band within ±15 %; pre-delay ±3 ms; energy E ±1.5 dB; octave-band energy profile ±2 dB;
  L/R tail correlation |ρ| < 0.2.
- Distortion: small-signal response within ±2 dB (100 Hz–4 kHz) at Send 50 % and ±3 dB at Send 75/100 %; gain-vs-level
  tables ±3 dB; odd-harmonic profile within ±4 dB; gate thresholds ±4 dB; one-sided input must not produce output on
  the other channel (≤ −90 dB re the driven channel); Width 100 % L/R level difference ≤ 1 dB.
- Doubler: pitch ±0.5 cent; level ±1 dB; latency ≤ 15 ms.
- Phaser: LFO rate ±5 %; ≥ 4 notches.
- Level laws: ±1.5 dB.

## 11. Test vectors (expected observations)
- Impulse 0.001 FS into L, Delay L 100 ms, Feedback 50 %, Dry/Wet 100 %: output L impulses at 100, 200, 300 ms of
  0.001 × 1, 0.5, 0.25 (exact); output R silent.
- Same with Dry/Wet 25 %: dry impulse 0.74989 × input at t=0, wet 0.24547 × input at 100 ms.
- Host mode @48 kHz: 120 BPM: 1/4 → 24000, 1/8D → 18000, 1/16T → 4000, 1/8T → 8000, 1/2T → 32000, 1/64T → 1000,
  Whole → 96000; 100 BPM: 1/16T → 4800, 1/8D → 21600, 1/64T → 1200; 137 BPM: 1/16T → 3503, 1/32T → 1751;
  140 BPM: 1/16T → 3428, 1/2T → 27428.
- Quiet sweep (−40 dBFS), HP 1 kHz: −3.01 dB at 1 kHz, −14.82 dB at 500 Hz, −29.32 dB at 200 Hz, −36.96 dB at 100 Hz.
- Quiet sweep, LP 1 kHz: −3.01 dB at 1 kHz, −14.88 dB at 2 kHz, −26.64 dB at 4 kHz, −38.48 dB at 10 kHz.
- Input +6 dB, sweep −40 dBFS: +5.99 dB ± 0.03 (compressor barely active).
- 1 kHz sine, delay 0 ms, Dry/Wet 100 %: gain at −30/−12/0 dBFS sine peak = −0.47 / −8.91 / −17.56 dB (steady state).
- Gain recovery after −6 → −40 dBFS step (reference observation): −13.42 dB → −10.78 dB after 100 ms, −5.35 dB after
  500 ms, ≈ 0 after 1.2 s. The §5 model gives −10.83 / −4.45 / −0.91 dB; tolerance ±1.0 dB at 100 and 500 ms and
  ≥ −1.0 dB at 1.2 s (the §5 model meets it).
- Distortion, Send 50 %, 1 kHz −40 dBFS tone on L only: left output +11.7 dB re input, right output silent; same tone on
  both channels: +11.7 dB on each (not +17.7).
- Doubler, Level 100 %, Detune +8, effect output isolated by subtraction, re plugin input: −40 dBFS one-sided sweep
  +12.9 ± 1 dB; 1 kHz tone on both channels at −40 / −20 / −8 / 0 dBFS: +13.0 / +9.3 / +1.2 / −4.5 dB (± 1 dB), at +8.0 cents.
- Distortion Send 10 %, 1 kHz on both channels: silent at −32 dBFS; −10.7 dB at −28 dBFS; −27.4 dB at 0 dBFS (± 3 dB).

## 12. User interface (functional layout — visuals must be original)
Same controls and arrangement as the reference, all artwork/fonts/colours/logos/names the implementer's own:
- **Upper "delay" panel**: top row, left to right: Feedback L (small knob), Link toggle, a central product badge (own
  design), Mode toggle (ms / Host), Feedback R (small knob). Middle: large Delay L knob (left) and large Delay R knob
  (right), each with a note-value drop-down beneath that is active in Host mode (in ms mode the knob shows ms).
  Between them a row of three small knobs: Dry/Wet, HP, LP, with an HP/LP Link button between HP and LP.
- **Lower "effects" row**: four modules side by side — Reverb, Distortion, Doubler, Phaser — each with an on/off
  button with indicator light, one main knob = Level, and secondary small controls arranged around it:
  Reverb: Size; Distortion: Send and Width; Doubler: Detune; Phaser: Rate.
- **Left edge**: vertical input meter above the Input knob. **Right edge**: vertical output meter above the Output knob,
  and the 3-way Monitor switch (L / ST / R).
- Value readouts on hover/drag; double-click (or Alt-click) resets a control to its default; drop-downs for note values.
- Linked Delay R / Feedback R controls show the left values while Link is on.

## 13. Open questions
1. Transient (single-sample) compressor behaviour (§5): an extra fast component exists in the reference; not modelled.
2. Possible partial in-loop interaction of the compressor with feedback (model envelope error 0.9 dB RMS with Feedback 50 %).
3. Phaser internals (§7.4), distortion width mechanism, reverb exact algorithm — targets only.
4. Delay-time change behaviour (pitch glide vs crossfade) — not measured.

## 14. Public references usable by the implementer
- U. Zölzer (ed.), "DAFX: Digital Audio Effects", 2nd ed., Wiley 2011 — delay, filters, dynamics, pitch shifting,
  distortion, phaser, reverb chapters.
- J. Dattorro, "Effect Design, Part 1: Reverberator and Other Filters", JAES 45(9), 1997; "Part 2: Delay-Line Modulation
  and Chorus", JAES 45(10), 1997.
- D. Giannoulis, M. Massberg, J. D. Reiss, "Digital Dynamic Range Compressor Design — A Tutorial and Analysis",
  JAES 60(6), 2012 (soft-knee static curve, smoothing topologies).
- R. Bristow-Johnson, "Audio EQ Cookbook" (bilinear transform with prewarping).

## Changes from r1
1. §3a Host-sync delay: exact floor of the rational note duration (r1's example "1/16T at 120 BPM = 3999" was a
   reference irregularity; integer-valued note lengths must not lose a sample). New examples at several tempos.
2. §5: the measured high-level static gains (+6/+12 dBFS) are normative; the r1 model overshoots them. LF frequency
   dependence given as ranges; the claim that detector ripple reproduces 30 Hz is withdrawn; tolerances added.
3. §7: channel routing made explicit — distortion and doubler are per-channel (r1 "Width 0 % = identical L/R output"
   was ambiguous and led to a mono-sum implementation); reverb energy definition made explicit; octave-band energy
   profile added.
4. §7.2: Send 75/100 % small-signal response and gain-vs-level tables added; Width behaviour clarified (no channel
   mixing, ≈0.6 correlation for identical inputs at 100 %, equal level).
5. §7.3: doubler level clarified as per channel for both tone and broadband input.
6. §10/§11: tolerances and test vectors updated accordingly (sync, recovery, distortion routing, doubler level).

## Changes from r2
1. §3.4: effects are fed **after the compressor's gain reduction** (and after the wet gain); stated explicitly with the
   measurement that proves it. Pre-compressor feeding is non-conformant.
2. §7.3: doubler gain corrected to +13.0 dB re its (post-compressor) input; r2's "+9.2 dB re its input, tone and broadband
   alike" was the −20 dBFS end-to-end value and was wrong for broadband/quiet input (+12.9 dB measured).
3. §7.2: distortion tables declared end-to-end (include compressor gain reduction); Send 10 % and 25 % gain-vs-level
   tables and Send 25 % small-signal response added; Width-100 % voicing change noted (informative).
4. §7.5 new: overload behaviour — reference's 0 dBFS internal hard clip not reproduced; loop/output must be bounded.
5. §11: doubler and distortion test vectors updated.
