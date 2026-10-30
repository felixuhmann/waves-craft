# Parametric compander (compressor / gate / dynamic filter) — behavioural specification r3

Status: APPROVED r3 (2026-10-29; supersedes r2 after independent verification of the r2 build; see "Changes from r2" and "Changes from r1"). Frozen. Derived only from black-box measurement of the licensed reference in a VST3
host with user-generated stimuli, host-visible parameter metadata and the licensed user documentation (paraphrased).
No code, disassembly or vendor assets used or included. Names, IDs, GUI and branding are the implementer's own.

## 1. Products (one engine, four variants)
| Variant | Modules present |
|---|---|
| **Full** (primary) | compressor/expander, gate/expander, filter module (sidechain / split), key mode, monitor, lookahead, input gains |
| **Compressor** | compressor/expander only, wideband, no filter/key/lookahead/monitor/input gains |
| **Gate** | gate/expander only, wideband, no filter/key/lookahead/monitor/input gains |
| **Compressor + filter** | compressor/expander + filter module + key + monitor + lookahead + input gains (no gate) |
Each variant's audio for a given module setting is identical to the Full variant with the absent module bypassed
(measured bit-identical). Stereo (2→2) is required; mono (1→1) variants run the same engine on one channel.

## 2. Parameters
Normalised host value r ∈ [0,1]. Displayed values and ranges (implementers may choose their own host mapping as long
as displayed values and behaviour match):
| Parameter | Range / values | Default | Reference mapping (for information) |
|---|---|---|---|
| Comp Threshold T | −100 … 0 dB (0.1 dB) | 0 dB | −100 + 100 r |
| Ratio | 0.50:1 … 50:1, then "−50:1 … −5:1" (cancellation); the position r = 0.8 itself is 50:1 and must display "50.00:1" | 1.00:1 | positions r: 0→0.50, .1→0.65, .2→0.93, .3→1.45, .4→2.46, .5→4.58, .6→9.29, .7→20.62, .8→50.00, .9→−15.90, 1→−5.00 (neutral 1.00:1 at r = 0.218) |
| Comp Attack | 0.01 … 1000 ms | 2.00 ms | 0.01·10^(5r) |
| Comp Release | 1 … 10000 ms | 50 ms | 10^(4r) |
| PDR (program-dependent release) | 0 … 1000 ms (0 = off) | 0 | displayed 0,1,2,5,10,22,47,102,218,467,1000 at r = 0,.1,…,1 (≈ geometric) |
| Makeup | −40 … +40 dB | 0 | −40 + 80 r |
| Level reference | LowRef / PeakRef | LowRef | |
| Comp bypass, Gate bypass | off/on | off | |
| Gate mode | Gate / Expander | Gate | |
| GateOpen, GateClose | −∞, −90 … 0 dB (GateClose ≤ GateOpen) | −∞ | r=0 → −∞, else −100 + 100 r |
| Gate Attack | 0.01 … 1000 ms | 2.00 ms | as Comp Attack |
| Gate Release | 1 … 10000 ms | 30 ms | as Comp Release |
| Hold | 0.01 … 5000 ms | 0.01 ms | 0.01·500000^r |
| Floor | −∞ … +12 dB, and negative-polarity "−10 N … −100 N" (expander only) | −∞ | see §6 |
| Filter type | Low-pass, Band-pass, High-pass, Band-reject | Low-pass | |
| Filter Freq | 16 … 21357 Hz | 7179 Hz | 16·1335^r |
| Filter Q | 0.10 … 0.60 | 0.60 | 0.1 + 0.5 r |
| Comp EQ mode, Gate EQ mode | Wideband / Sidechain / Split | Wideband | |
| Monitor | Audio / Sidechain / Passive | Audio | |
| Key mode | Stereo / L→R / R→L | Stereo | |
| LookAhead | off/on | off | |
| Input gain L, R | −24 … 0 dB | 0 | |
| Output gain | −40 … +18 dB | 0 | −40 + 58 r |
| Link offsets (comp/gate attack, release, threshold) | UI convenience to move both modules' controls together | — | no audio effect of their own |
All controls are in absolute units independent of sample rate (times in ms).

## 3. Signal flow
1. Input gains (per channel).
2. Detection signal per module: wideband channel signals, or the sidechain-filtered signals (§7) if that module's EQ
   mode is Sidechain or Split. Key mode Stereo: both channels; L→R: only L drives the detectors; R→L: only R.
3. Detector (§4) → compressor gain g_c (§5); gate/expander gain g_g (§6). A bypassed module has gain 1.
4. Apply: Wideband/Sidechain module: gain multiplies the full-band signal. Split module: gain multiplies only the active
   band; output = g·A(x) + P(x) (§7). Both modules active: total gain g_c·g_g on the respective path (assumption when
   both are in Split: the active band receives g_c·g_g, the passive band 1).
5. Lookahead on: audio path delayed by exactly 340 samples (any sample rate); detectors see the undelayed signal; the
   plugin reports 340 samples latency. Off: zero latency.
6. Output gain, then a hard clip of every output sample to [−1.0, +1.0] (0 dBFS) — always active (§5.1). Key modes L→R / R→L: only the keyed channel is output (the keying channel's output is silent).
7. Monitor: Audio = normal output. Sidechain = the compressor's detection signal per channel at unity: filtered
   (|H|³, §7) if the compressor's EQ mode is Sidechain or Split, else wideband — the gate's EQ mode does not change
   the monitor (measured with the compressor active and only the gate filtered: monitor wideband). Passive = 2 × P(x)
   (the passive band at +6.02 dB).
Neutral (both modules bypassed, gains 0 dB): bit-exact passthrough (for input within ±1.0).
State at stream start and on every host reset: all detector levels at silence (−∞), compressor slow memory 0 dB, gate
closed (g = 0), expander gain at its value for silence. The reference starts every render in this state.

## 4. Detector
All constants are listed in §4.3 and are normative. Per sample n, per channel c (after input gains / filter):
1. Rectifier: m_c ← a_r·m_c + (1 − a_r)·|s_c|, a_r = exp(−1/(RECT_TAU·fs)).
2. Link (Stereo key mode): m = max(m_L, m_R) (L→R / R→L: the keying channel only).
3. Level: ℓ = 20·log10(m) + K_CAL dB. K_CAL is the constant that makes a steady 1 kHz sine read
   e = (sine peak dB) + 1.277 dB through the compressor detector of step 4 at Attack 2 ms / Release 50 ms (the
   coordinate of Appendix A). With RECT_TAU as given, K_CAL = +3.32 dB (±0.01).
4. Level smoother (dB): e ← c·e + (1 − c)·ℓ with c = exp(−1/(τ·fs)), τ = τ_att if ℓ > e else τ_rel.
   - Compressor: τ_att = Comp Attack, τ_rel = DET_REL·Comp Release. With PDR > 0: e = max(e_fast, e_slow), e_fast with
     (Comp Attack, PDR_FAST_REL), e_slow with (0.5·PDR, DET_REL·Comp Release).
   - Expander: τ_att = Gate Attack, τ_rel = EXP_DET_REL·Gate Release.
   - Gate: τ_att = min(Gate Attack, 2 ms), τ_rel = Gate Release; during Hold (§6) e does not decrease.
Consequences (measured on the reference, must hold, §9): equal-RMS white noise gets ≈0.8 dB LESS gain reduction
than a 1 kHz sine (a rectified-average detector, not an RMS/power one); a 100 Hz sine ≈0.9 dB more and a 10 kHz
sine ≈1.2 dB less (table §10.8).

### 4.3 Constants
| Name | Value |
|---|---|
| RECT_TAU | 0.127 ms |
| DET_REL | 0.900 |
| PDR_FAST_REL | 41.5 ms |
| SLOW_REL | 2.907 |
| SLOW_W | 0.093 |
| EXP_DET_REL | 0.166 |
| EXP_GAIN_ATT | 0.384 |
| EXP_GAIN_REL | 0.842 |
| EXP_CAL | 1.11 dB |

## 5. Compressor / expander gain
x = e − T (dB). Static curve: f_R(x) from Appendix A for the selected Ratio (between positions interpolate linearly in r;
at exactly 1.00:1 the gain is exactly 0 dB). Properties: soft knee (≈ −2.2 dB at x ≈ +1.3 for strong
ratios), a slight positive bump (≤ +0.2 dB) about 12–18 dB below threshold, compression saturating 20–40 dB above
threshold for moderate ratios (max reduction: 1.15:1 → 3.6 dB, 1.45:1 → 9.3, 1.87:1 → 15.1, 2.46:1 → 20.9,
4.58:1 → 32.9, 9.29:1 → 46.4 dB), limiter-like slope −1 at 50:1, and cancellation (gain reduction exceeding the
limiter, then recovering to a fixed −10.5 … −30.4 dB) for negative ratios.
Dynamic gain (per sample): target reduction t = −f_R(e − T) (dB; negative for expansion ratios). A slow memory s
(dB): if t > s then s = t, else s ← c_s·s + (1 − c_s)·t with c_s = exp(−1/(SLOW_REL·Comp Release·fs)).
Reduction R = (1 − SLOW_W)·t + SLOW_W·s; compressor gain G_c(dB) = −R + Makeup (+ PeakRef offset). In steady state
s = t, so the static curve is unchanged.
Expansion ratios (< 1:1): upward gain per Appendix A (the expansion columns are the unclipped curve; they saturate at
the last listed value).
### 5.1 Output clip
There is no soft output limiter. The final output (after Makeup, PeakRef offset and Output gain) is hard-clipped to
±1.0 per sample. The apparent "+2.6 … +2.83 dBFS" levels of §10.3 and the flattening in §10.9 are this clip measured
by RMS ratio (a clipped sine has up to +3.01 dB more RMS than a sine of the same peak).
PeakRef: add a constant offset O(T, R) = −f_R(x_ref) with x_ref = 3.71 dB − T (detector coordinates; i.e. the curve
value for a sine whose peak is 2.43 dBFS), using the unlimited curve, capped at +40 dB. For cancellation ratios use
the table in §10.4.
Note: this law reproduces the reference envelopes to the §9 tolerances, but not exactly; in particular the steady
reduction during a sustained overload depends slightly on Attack/Release in the reference (table §10.5.2), which an
implementation may reproduce more closely by any means.

## 6. Gate / expander gain
Floor F (linear, signed): −∞ dB → 0; x dB → 10^(x/20); "−y N" → −10^(−y/20) (expander mode only; in gate mode
negative-polarity values act as −∞). Output gain of the module: G = F + (1 − F)·g, g ∈ [0,1].
- Gate: state opens when e > GateOpen + 1.85 dB, closes when e < GateClose + 1.85 dB and the hold timer has expired.
  On opening, start Hold (timer from the moment of opening; while it runs the gate cannot close and its detector
  does not decay). g follows the state with a one-pole in linear gain: τ = Gate Attack towards 1, Gate Release
  towards 0.
- Expander: target u = h(e + EXP_CAL − GateOpen) dB (h tabulated in Appendix B: 1:2 downward expansion below GateOpen, soft
  knee; below its first row h continues with slope 1, h(x) = x − 2.04 dB; above its last row h = −0.02 dB), with e from the expander detector of §4 (fast release EXP_DET_REL·Gate Release).
  The applied gain is smoothed in dB: v ← c·v + (1 − c)·u, τ = EXP_GAIN_ATT·Gate Attack if u > v else
  EXP_GAIN_REL·Gate Release; g = 10^(v/20). GateClose and Hold are ignored. Floor applies as above (positive floors give low-level
  boost up to +12 dB; negative-polarity floors produce a cancellation notch where g = |F|).

## 7. Filter module
Second-order section H(s) with the textbook Q equal to the Q control, discretised by the bilinear transform prewarped at
the Freq control (true sample rate):
- Low-pass: H = 1/((s/ω0)² + s/(Q·ω0) + 1), ω0 chosen so that |H(j·2πFreq)|² = 1/2.
- High-pass: H = (s/ω0)²/((s/ω0)² + s/(Q·ω0) + 1), ω0 chosen so that |H(j·2πFreq)|² = 1/2.
- Band-pass: H = (s/(Q·ω0))/((s/ω0)² + s/(Q·ω0) + 1), ω0 = 2πFreq.
- Band-reject: H = ((s/ω0)² + 1)/((s/ω0)² + s/(Q·ω0) + 1), ω0 = 2πFreq.
Allpass AP = D(−s)/D(s) for all four types (D = the common denominator (s/ω0)² + s/(Q·ω0) + 1, with that type's ω0):
AP(0) = +1 and the phase starts at 0° and falls to −360° (no sign inversion for any type; in particular the
band-pass split/passive output has the same polarity as the band-reject one).
Active band A = H(s)² (|A| = |H|², in phase with AP); passive band P = AP − A (|P| = 1 − |A|); A + P = AP exactly
(flat magnitude; phase of the allpass). Sidechain (detection) filter: magnitude |H|³ (phase not specified).
Measured: LP/BP model error ≤ 0.07 dB, HP ≤ 0.04 dB above −40 dB (≤ 0.8 dB deep in the stop band at 250 Hz).

## 8. Implementation latitude
- Parameter changes must be smoothed click-free (≤ 20 ms); toggles cross-fade (≤ 20 ms).
- Additional internal safety measures are allowed only if they never alter the output on the §10 vectors; the §5.1
  clip already bounds the output, so look-ahead onset holds or higher clamps are unnecessary.
- Any detector/gain implementation is acceptable if it meets §9 on the §10 vectors.

## 9. Tolerances
- Static gains (steady 1 kHz sine, Appendix A/B points, 44.1/48/96 kHz): ±0.1 dB for compression ratios, ±0.3 dB
  for cancellation ratios near their notch, ±0.5 dB expansion near the output limit.
- Gate open/close levels (steady sine peak, §10.2): ±0.3 dB. Floor law exact to ±0.05 dB, except within ±2 dB of
  input level around a negative-polarity floor's cancellation notch (ill-conditioned; ±3 dB there).
- Compressor envelopes: every point of the §10.5.1 trajectory table within the tolerance given in its row (±1.5 dB
  unless stated). §10.5 timing table: t63 and t90 within ±(2 ms + 25 %), except cells marked "i" (informative); t10
  values are informative.
- Gate envelopes: linear gain ≤ 0.06 RMS on the §10.5 gate vectors. Expander: every §10.5.3 point within ±0.15
  linear gain.
- Detector type (§10.8): noise / 100 Hz / 10 kHz offsets ±0.4 dB.
- Filters: §7 shapes ±0.1 dB where the response is above −40 dB.
- Lookahead: exactly 340 samples. No level-dependent artefacts in Wideband with both modules neutral.

## 10. Test vectors (reference measurements)
### 10.1 Static (sine 1 kHz, LowRef, Comp Attack 2 ms / Release 50 ms, threshold −40 dB) — gain (dB) vs sine peak level
| Ratio | −50 | −40 | −30 | −20 | −10 | 0 dBFS |
|---|---|---|---|---|---|---|
| 1.45:1 | −0.03 | −1.38 | −4.48 | −7.28 | −8.59 | −9.05 |
| 4.58:1 | −0.05 | −2.15 | −7.97 | −16.24 | −23.88 | −29.01 |
| 50:1 | −0.05 | −2.21 | −8.28 | −17.41 | −27.44 | −37.52 |
| −5.00:1 | −0.07 | −2.99 | −13.92 | −18.08 | −12.22 | −10.97 |
Threshold shift invariance: same curves translated with T (measured to 0.000 dB at −20/−40/−60/−80).
### 10.2 Gate (GateOpen −40, Gate Attack 2 ms, Release 30 ms, sine 1 kHz, 0.25 dB stair)
Opens between sine peak −39.25 and −39.0 dB; with Gate Attack 0.1 ms between −40.0 and −39.75; with GateClose −43:
closes between −42.25 and −42.5 dB (descending). Floor −27.42 dB gate closed → −27.42 dB; Floor +12.04 → +12.04 dB.
### 10.3 Expansion output limit (threshold −80, sine 1 kHz): Ratio 0.50:1 at input −10/−4/0 dBFS → output
+2.6/+2.8/+2.83 dBFS; 0.77:1 at 0 dBFS → +2.46; 0.93:1 at 0 dBFS → +1.43.
### 10.4 PeakRef offsets (dB, added to LowRef gain), threshold −10/−20/−60/−80
1.45:1 → 5.27/7.72/9.25/9.26; 4.58:1 → 9.85/18.25/32.61/32.94; 50:1 → 10.28/19.81/40.0/40.0;
0.50:1 → −11.21/−19.13/−24.04/−24.08; −5.00:1 → 5.77/8.60/10.44/10.46.
### 10.5 Envelope timing (1 kHz tone −40 → −10 dBFS step → −40, threshold −30, 50:1; dB gain change 10/63/90 % in ms)
| Setting | attack t10/t63/t90 | release t10/t63/t90 |
|---|---|---|
| Att 1, Rel 50 | 1.5/3.5/6.5 | 2.5/25.5/74.5 |
| Att 10, Rel 50 | 3.5/16.5/32.5 | 4.5/28.5/80.5 |
| Att 100, Rel 50 | 23.5/164.5/312.5 | 8.5/33.5/82.5 (i) |
| Att 1, Rel 10 | 1.5/2.5/5.5 | 1.5/6.5/16.5 |
| Att 1, Rel 200 | 1.5/3.5/6.5 | 7.5/95.5/290.5 |
| Att 1, Rel 1000 | 1.5/3.5/6.5 | 30.5/417.5/1144.5 |
| Att 1, Rel 1000, PDR 22 ms, after a 3 ms burst | — | 1.5/15.5/769.5 (i) |
| Att 1, Rel 1000, PDR 102 ms, after a 30 ms burst | — | 1.5/11.5/521.5 (i) |
Gate (open −30, sine step to −10 then back to −40), linear gain: Att 1/Rel 100: open t63 2.5 ms, close t10/t63/t90
93.5/182.5/313.5 ms; Rel 10: 9.5/18.5/31.5; Att 10: open t63 11.5 ms; Hold 97.6 ms after a 30 ms burst: close t10
162.5 ms after the burst ends.
#### 10.5.1 Compressor gain trajectories (normative)
Stimulus (both channels identical): 1 kHz sine, peak −40 dBFS from t = 0; at t = 0.5 s … the sequence −40 dBFS 1 s,
−10 dBFS 1 s (edge A at 1.5 s, edge R at 2.5 s), −40 dBFS 2 s; then −40 dBFS 1 s, −10 dBFS 30 ms (edge R30 at its end),
−40 dBFS 2 s; then −40 dBFS 1 s, −10 dBFS 3 ms (edge R3 at its end), −40 dBFS 2 s. Threshold −30 dB, Ratio 50:1, LowRef,
Makeup 0. Gain = in-phase gain Σx·y/Σx² over each 1 ms block (48 samples at 48 kHz); the value listed for offset k ms
is the block ending k ms after the edge. Values in dB.

| Setting | Edge | Tol (dB) | +1 | +2 | +3 | +5 | +10 | +20 | +50 | +100 | +200 | +500 | +1000 | +1900 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Att 0.1 / Rel 50 | A | ±3.5 | -14.88 | -19.34 | -19.54 | -19.59 | -19.59 | -19.59 | -19.59 | -19.59 | -19.59 | -19.59 | — | — |
| Att 0.1 / Rel 50 | R | ±1.5 | -18.87 | -17.63 | -16.56 | -14.81 | -11.72 | -8.11 | -3.54 | -1.16 | -0.27 | -0.27 | -0.27 | -0.27 |
| Att 0.1 / Rel 50 | R30 | ±1.5 | -18.87 | -17.63 | -16.56 | -14.81 | -11.72 | -8.11 | -3.54 | -1.16 | -0.27 | -0.27 | -0.27 | -0.27 |
| Att 0.1 / Rel 50 | R3 | ±1.5 | -18.85 | -17.61 | -16.55 | -14.80 | -11.71 | -8.11 | -3.54 | -1.16 | -0.27 | -0.27 | -0.27 | -0.27 |
| Att 1 / Rel 50 | A | ±1.5 | -3.19 | -9.10 | -12.65 | -15.96 | -17.90 | -18.29 | -18.32 | -18.32 | -18.32 | -18.32 | — | — |
| Att 1 / Rel 50 | R | ±1.5 | -17.85 | -16.75 | -15.79 | -14.18 | -11.30 | -7.85 | -3.41 | -1.08 | -0.14 | -0.14 | -0.14 | -0.14 |
| Att 1 / Rel 50 | R30 | ±1.5 | -17.85 | -16.75 | -15.79 | -14.18 | -11.30 | -7.85 | -3.41 | -1.08 | -0.14 | -0.14 | -0.14 | -0.14 |
| Att 1 / Rel 50 | R3 | ±1.5 | -13.76 | -13.12 | -12.53 | -11.49 | -9.46 | -6.79 | -3.05 | -0.98 | -0.14 | -0.14 | -0.14 | -0.14 |
| Att 10 / Rel 50 | A | ±1.5 | -0.18 | -0.88 | -1.63 | -3.13 | -6.55 | -11.58 | -15.72 | -16.13 | -16.16 | -16.16 | — | — |
| Att 10 / Rel 50 | R | ±1.5 | -16.12 | -15.82 | -15.23 | -13.80 | -11.00 | -7.63 | -3.28 | -1.01 | -0.07 | +0.04 | +0.04 | +0.04 |
| Att 10 / Rel 50 | R30 | ±1.5 | -14.30 | -14.09 | -13.63 | -12.47 | -10.11 | -7.13 | -3.11 | -0.96 | -0.07 | +0.04 | +0.04 | +0.04 |
| Att 10 / Rel 50 | R3 | ±2.0 | -2.34 | -2.74 | -2.78 | -2.67 | -2.37 | -1.87 | -0.93 | -0.29 | +0.01 | +0.04 | +0.04 | +0.04 |
| Att 100 / Rel 50 | A | ±2.0 | +0.05 | +0.01 | -0.04 | -0.17 | -0.53 | -1.27 | -3.43 | -6.66 | -11.48 | -15.68 | — | — |
| Att 100 / Rel 50 | R | ±4.0 | -15.89 | -15.85 | -15.76 | -15.40 | -13.59 | -9.26 | -3.57 | -0.99 | -0.05 | +0.06 | +0.06 | +0.06 |
| Att 100 / Rel 50 | R30 | ±2.5 | -2.08 | -2.14 | -2.21 | -2.31 | -2.44 | -2.13 | -1.02 | -0.29 | +0.02 | +0.06 | +0.06 | +0.06 |
| Att 100 / Rel 50 | R3 | ±1.5 | -0.10 | -0.16 | -0.21 | -0.30 | -0.40 | -0.38 | -0.17 | -0.02 | +0.05 | +0.06 | +0.06 | +0.06 |
| Att 1 / Rel 10 | A | ±1.5 | -3.16 | -9.07 | -12.63 | -15.81 | -17.36 | -17.51 | -17.51 | -17.51 | -17.51 | -17.51 | — | — |
| Att 1 / Rel 10 | R | ±1.5 | -15.59 | -12.18 | -9.94 | -7.06 | -3.54 | -1.11 | -0.10 | -0.10 | -0.10 | -0.10 | -0.10 | -0.10 |
| Att 1 / Rel 10 | R30 | ±1.5 | -15.59 | -12.18 | -9.94 | -7.06 | -3.54 | -1.11 | -0.10 | -0.10 | -0.10 | -0.10 | -0.10 | -0.10 |
| Att 1 / Rel 10 | R3 | ±1.5 | -12.76 | -10.35 | -8.61 | -6.26 | -3.21 | -1.02 | -0.10 | -0.10 | -0.10 | -0.10 | -0.10 | -0.10 |
| Att 1 / Rel 200 | A | ±1.5 | -3.21 | -9.10 | -12.66 | -15.99 | -18.03 | -18.55 | -18.62 | -18.62 | -18.62 | -18.62 | — | — |
| Att 1 / Rel 200 | R | ±1.5 | -18.50 | -18.18 | -17.88 | -17.30 | -16.04 | -14.02 | -10.14 | -6.67 | -3.40 | -0.63 | -0.16 | -0.16 |
| Att 1 / Rel 200 | R30 | ±1.5 | -18.48 | -18.17 | -17.86 | -17.29 | -16.03 | -14.01 | -10.14 | -6.67 | -3.40 | -0.63 | -0.16 | -0.16 |
| Att 1 / Rel 200 | R3 | ±1.5 | -13.97 | -13.80 | -13.63 | -13.31 | -12.56 | -11.28 | -8.53 | -5.79 | -3.02 | -0.57 | -0.16 | -0.16 |
| Att 1 / Rel 1000 | A | ±1.5 | -3.21 | -9.11 | -12.66 | -16.00 | -18.06 | -18.62 | -18.75 | -18.76 | -18.76 | -18.76 | — | — |
| Att 1 / Rel 1000 | R | ±1.5 | -18.73 | -18.66 | -18.60 | -18.47 | -18.15 | -17.56 | -16.02 | -14.00 | -11.18 | -6.66 | -3.39 | -1.21 |
| Att 1 / Rel 1000 | R30 | ±1.5 | -18.70 | -18.63 | -18.57 | -18.44 | -18.13 | -17.53 | -15.99 | -13.99 | -11.17 | -6.65 | -3.39 | -1.20 |
| Att 1 / Rel 1000 | R3 | ±1.5 | -14.08 | -14.04 | -14.01 | -13.94 | -13.76 | -13.43 | -12.53 | -11.25 | -9.29 | -5.77 | -3.01 | -1.08 |
| Att 1 / Rel 1000 / PDR 5 | A | ±1.5 | -3.21 | -9.11 | -12.65 | -15.89 | -17.82 | -18.49 | -18.75 | -18.76 | -18.76 | -18.76 | — | — |
| Att 1 / Rel 1000 / PDR 5 | R | ±1.5 | -18.73 | -18.66 | -18.60 | -18.47 | -18.15 | -17.56 | -16.02 | -14.00 | -11.18 | -6.66 | -3.39 | -1.21 |
| Att 1 / Rel 1000 / PDR 5 | R30 | ±1.5 | -18.63 | -18.55 | -18.48 | -18.35 | -18.04 | -17.45 | -15.93 | -13.94 | -11.14 | -6.64 | -3.38 | -1.20 |
| Att 1 / Rel 1000 / PDR 5 | R3 | ±2.0 | -13.32 | -11.93 | -11.06 | -10.06 | -9.18 | -8.85 | -8.40 | -7.73 | -6.62 | -4.35 | -2.36 | -0.86 |
| Att 1 / Rel 1000 / PDR 22 | A | ±1.5 | -3.21 | -9.11 | -12.65 | -15.84 | -17.49 | -17.90 | -18.39 | -18.69 | -18.76 | -18.76 | — | — |
| Att 1 / Rel 1000 / PDR 22 | R | ±1.5 | -18.73 | -18.66 | -18.60 | -18.47 | -18.15 | -17.56 | -16.02 | -14.00 | -11.18 | -6.66 | -3.39 | -1.21 |
| Att 1 / Rel 1000 / PDR 22 | R30 | ±3.0 | -17.48 | -16.15 | -15.16 | -13.82 | -12.15 | -11.09 | -10.37 | -9.44 | -7.95 | -5.09 | -2.70 | -0.98 |
| Att 1 / Rel 1000 / PDR 22 | R3 | ±4.5 | -12.98 | -10.97 | -9.62 | -7.91 | -5.92 | -4.63 | -3.99 | -3.75 | -3.31 | -2.31 | -1.32 | -0.49 |
| Att 1 / Rel 1000 / PDR 102 | A | ±1.5 | -3.21 | -9.11 | -12.65 | -15.82 | -17.36 | -17.57 | -17.75 | -18.02 | -18.37 | -18.72 | — | — |
| Att 1 / Rel 1000 / PDR 102 | R | ±1.5 | -18.73 | -18.66 | -18.60 | -18.47 | -18.15 | -17.56 | -16.01 | -14.00 | -11.18 | -6.66 | -3.39 | -1.21 |
| Att 1 / Rel 1000 / PDR 102 | R30 | ±6.0 | -16.11 | -13.35 | -11.55 | -9.31 | -6.66 | -4.77 | -3.52 | -3.22 | -2.85 | -2.01 | -1.15 | -0.43 |
| Att 1 / Rel 1000 / PDR 102 | R3 | ±5.0 | -12.84 | -10.65 | -9.16 | -7.24 | -4.90 | -3.17 | -1.89 | -1.51 | -1.33 | -0.96 | -0.56 | -0.21 |

#### 10.5.3 Expander gain trajectory (normative; linear gain, same stimulus; GateOpen −30 dB, Expander mode,
Gate Attack 1 ms, Release 100 ms, Floor −∞)

| Edge | +1 | +2 | +3 | +5 | +10 | +20 | +50 | +100 | +200 | +500 | +990 |
|---|---|---|---|---|---|---|---|---|---|---|---|
| A | 0.538 | 0.816 | 0.924 | 0.986 | 0.999 | 0.999 | 0.999 | 0.999 | 0.999 | 0.999 | 0.999 |
| R | 0.995 | 0.987 | 0.979 | 0.963 | 0.926 | 0.855 | 0.681 | 0.486 | 0.296 | 0.277 | 0.277 |
| R30 | 0.995 | 0.987 | 0.979 | 0.963 | 0.926 | 0.855 | 0.681 | 0.486 | 0.296 | 0.277 | 0.277 |
| R3 | 0.947 | 0.940 | 0.932 | 0.918 | 0.882 | 0.815 | 0.652 | 0.468 | 0.289 | 0.277 | 0.277 |

### 10.8 Detector type (static, Comp Attack 2 ms / Release 50 ms, threshold −40 dB, levels −30 … −4 dBFS)
Gain difference to a 1 kHz sine of the same RMS (positive = less reduction), mean over the range:

| Signal | 50:1 | 4.58:1 |
|---|---|---|
| white noise (equal RMS) | +0.86 | +0.66 |
| 100 Hz sine (equal peak) | −0.92 | −0.72 |
| 10 kHz sine (equal peak) | +1.19 | +0.92 |

### 10.9 PeakRef near full scale (sine 1 kHz, Attack 2 ms / Release 50 ms; gain = RMS ratio in dB vs input peak level; normative ±0.3 dB)
The flattening near full scale is the §5.1 output clip acting on the final output.

| Config (ratio position r, threshold T) | -12 dBFS | -10 dBFS | -8 dBFS | -6 dBFS | -4 dBFS | -2 dBFS | 0 dBFS |
|---|---|---|---|---|---|---|---|
| 0.50:1, T -10.0 dB | -11.42 | -10.71 | -9.64 | -8.20 | -6.45 | -4.48 | -2.40 |
| 0.50:1, T -20.0 dB | -12.40 | -10.32 | -8.26 | -6.28 | -4.47 | -2.84 | -1.42 |
| 0.50:1, T -60.0 dB | -0.20 | -0.15 | -0.11 | -0.08 | -0.05 | -0.03 | -0.01 |
| 0.50:1, T -80.0 dB | -0.02 | -0.01 | -0.01 | -0.01 | -0.01 | -0.00 | -0.00 |
| 1.45:1, T -10.0 dB | +4.32 | +3.89 | +3.38 | +2.79 | +2.15 | +1.48 | +0.53 |
| 1.45:1, T -20.0 dB | +3.92 | +3.24 | +2.57 | +1.95 | +1.38 | +0.88 | +0.32 |
| 1.45:1, T -60.0 dB | +0.07 | +0.05 | +0.04 | +0.03 | +0.02 | +0.01 | +0.00 |
| 1.45:1, T -80.0 dB | +0.01 | +0.01 | +0.00 | +0.00 | +0.00 | +0.00 | +0.00 |
| 4.58:1, T -10.0 dB | +8.39 | +7.70 | +6.85 | +5.84 | +4.46 | +2.79 | +1.00 |
| 4.58:1, T -20.0 dB | +11.73 | +10.22 | +8.49 | +6.67 | +4.81 | +2.93 | +1.05 |
| 4.58:1, T -60.0 dB | +1.43 | +1.09 | +0.81 | +0.58 | +0.39 | +0.24 | +0.10 |
| 4.58:1, T -80.0 dB | +0.16 | +0.12 | +0.09 | +0.06 | +0.04 | +0.03 | +0.01 |
| 50.00:1, T -10.0 dB | +8.79 | +8.08 | +7.20 | +6.13 | +4.59 | +2.86 | +1.05 |
| 50.00:1, T -20.0 dB | +12.66 | +10.86 | +9.00 | +7.08 | +5.12 | +3.15 | +1.17 |
| 50.00:1, T -60.0 dB | -5.55 | -7.55 | -9.56 | -11.56 | -13.56 | -15.56 | -17.56 |
| 50.00:1, T -80.0 dB | -25.57 | -27.57 | -29.57 | -31.57 | -33.57 | -35.57 | -37.57 |
| -5.00:1, T -10.0 dB | +3.77 | +2.78 | +1.51 | -0.11 | -2.15 | -4.74 | -8.15 |
| -5.00:1, T -20.0 dB | -1.91 | -5.32 | -10.26 | -19.81 | -22.86 | -13.27 | -9.48 |
| -5.00:1, T -60.0 dB | -0.22 | -0.18 | -0.15 | -0.12 | -0.10 | -0.08 | -0.07 |
| -5.00:1, T -80.0 dB | -0.02 | -0.02 | -0.01 | -0.01 | -0.01 | -0.01 | -0.01 |
| 4.58:1, T -30.0 dB | +10.85 | +9.16 | +7.49 | +5.88 | +4.26 | +2.57 | +0.85 |
| 4.58:1, T -50.0 dB | +3.64 | +2.85 | +2.17 | +1.58 | +1.09 | +0.68 | +0.26 |
| 4.58:1, T -70.0 dB | +0.49 | +0.37 | +0.27 | +0.19 | +0.13 | +0.08 | +0.03 |
| 4.58:1, T -100.0 dB | +0.02 | +0.01 | +0.01 | +0.01 | +0.00 | +0.00 | +0.00 |
| 0.65:1, T -40.0 dB | -1.36 | -1.01 | -0.74 | -0.52 | -0.34 | -0.21 | -0.10 |
| 1.00:1, T -40.0 dB | -0.00 | -0.00 | -0.00 | -0.00 | -0.00 | -0.00 | -0.00 |
| 13.69:1, T -40.0 dB | +12.88 | +10.90 | +8.92 | +6.95 | +4.99 | +3.04 | +1.10 |

### 10.6 Filter (sweep −30 dBFS, Freq 1 kHz, Q 0.6) — sidechain monitor magnitude; split/passive phase per §7 (band-pass and band-reject identical)
LP: 500 Hz −2.0, 1 kHz −9.0, 2 kHz −31.0, 3 kHz −49.8 dB. BP: 500 Hz −7.8, 1 kHz 0.0, 2 kHz −7.8 dB. HP mirror of LP.
Passive monitor (= 2·P) LP-type: 500 Hz −11.1, 1 kHz 0.0, 2 kHz +5.2, ≥5 kHz +6.0 dB. Split with no gain change: 0.00 dB
flat magnitude.
### 10.7 Key / routing
L→R: L output silent; R output = R with gain from L's level. R→L mirrored. Stereo: max-linked.

## 11. Review decisions (standing policy: match the reference, do not reproduce clear defects)
- D9 (r3) Output clip at 0 dBFS reproduced (the reference never outputs a sample beyond ±1.0); placement after Output
  gain is assumed (not separately measured).
- D8 (r2) Envelope law: no closed form reproducing every reference envelope exactly was found; §4–§5 give the law that
  meets the normative tables (verified by a literal implementation of this text against the reference, see "Changes
  from r1"); the tables, not the law, are the acceptance criterion. Cells marked (i) are informative.
- D1 PeakRef: the reference's offset depends on the order in which parameters were changed (same settings give
  different offsets); specified here as the deterministic function of §5.
- D2 Neutral bypass: the reference deviates by ≤ 3e-5; implementation must be bit-exact.
- D3 Parameter smoothing required (reference applies changes asynchronously).
- D4 Both modules in Split: combined active-band gain (assumption; not measured).
- D5 External sidechain inputs (the compressor-only variant exposes 4 inputs) are not specified/required.
- D6 Lookahead fixed 340 samples at all sample rates (as the reference).
- D7 Mono variants not measured; same engine on one channel.

## 12. User interface (functional layout; visuals must be original)
Same controls and arrangement concept as described: a large input/output transfer graph (input level horizontal,
output level vertical, 0 … −100 dB) showing the compressor curve (one colour) and the gate/expander curve (another
colour), with draggable markers for Threshold, GateOpen and GateClose along the bottom edge and a moving indicator
of the current detector level on each curve; compressor section controls (Threshold, Ratio, Makeup, Attack, Release,
PDR, LowRef/PeakRef, EQ mode, bypass) with a gain-change meter (reduction downward, gain increase upward) and a
detector-level meter; gate section controls (Gate/Expander, GateOpen, GateClose, Floor, Attack, Release, Hold, EQ
mode, bypass) with its meters; filter section with a frequency-response graph of the active (one colour) and passive
(another colour) bands, a crosshair to drag Freq (horizontal) and Q (vertical), and the type selector; global: input
gains L/R, output fader, Key mode, Monitor, LookAhead, link toggles for moving comp/gate controls together, peak
value boxes that reset on click. Variants omit absent sections. All artwork, colours, fonts and names original.

## 13. Open questions
1. Exact static-curve formula (tables given). 2. Exact envelope/PDR law (D8); the steady reduction's dependence on
Attack/Release and the PDR short-burst tails are only partly reproduced by §4–§5. 3b. The soft limiting near full scale
(§10.9) is specified by table only.
3. Both-modules-split combination (D4). 4. External sidechain (D5). 5. Mono variants (D7).

## Appendix A — compressor static curves f_R(x) (dB) vs detector-coordinate x = e − T
(measured with a steady 1 kHz sine; x_det = sine-peak level − T + 1.277 dB. Expansion columns (r ≤ 0.20) are the
unclipped curves from threshold −80 dB measurements; output levels above 0 dBFS are then clipped per §5.1.)

| x_det (dB) | r=0.00 (0.50:1) | r=0.10 (0.65:1) | r=0.15 (0.77:1) | r=0.20 (0.93:1) | r=0.25 (1.15:1) | r=0.30 (1.45:1) | r=0.35 (1.87:1) | r=0.40 (2.46:1) | r=0.50 (4.58:1) | r=0.60 (9.29:1) | r=0.70 (20.62:1) | r=0.80 (50.00:1) | r=0.90 (-15.90:1) | r=1.00 (-5.00:1) |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| -58.72 | -0.00 | -0.00 | -0.00 | -0.00 | +0.00 | +0.00 | +0.00 | +0.00 | +0.00 | +0.00 | +0.00 | +0.00 | +0.00 | +0.00 |
| -56.72 | -0.00 | -0.00 | -0.00 | -0.00 | +0.00 | +0.00 | +0.00 | +0.00 | +0.00 | +0.00 | +0.00 | +0.00 | +0.00 | +0.00 |
| -54.72 | -0.00 | -0.00 | -0.00 | -0.00 | +0.00 | +0.00 | +0.00 | +0.00 | +0.00 | +0.00 | +0.00 | +0.00 | +0.00 | +0.00 |
| -52.72 | -0.00 | -0.00 | -0.00 | -0.00 | +0.00 | +0.00 | +0.00 | +0.00 | +0.00 | +0.00 | +0.00 | +0.00 | +0.00 | +0.01 |
| -50.72 | -0.00 | -0.00 | -0.00 | -0.00 | +0.00 | +0.00 | +0.00 | +0.01 | +0.01 | +0.01 | +0.01 | +0.01 | +0.01 | +0.01 |
| -48.72 | -0.00 | -0.00 | -0.00 | -0.00 | +0.00 | +0.00 | +0.01 | +0.01 | +0.01 | +0.01 | +0.01 | +0.01 | +0.01 | +0.01 |
| -46.72 | -0.00 | -0.00 | -0.00 | -0.00 | +0.00 | +0.01 | +0.01 | +0.01 | +0.01 | +0.01 | +0.01 | +0.01 | +0.01 | +0.01 |
| -44.72 | -0.00 | -0.01 | -0.00 | -0.00 | +0.00 | +0.01 | +0.01 | +0.01 | +0.01 | +0.01 | +0.01 | +0.01 | +0.01 | +0.01 |
| -42.72 | -0.01 | -0.01 | -0.01 | -0.00 | +0.00 | +0.01 | +0.01 | +0.01 | +0.01 | +0.01 | +0.01 | +0.01 | +0.01 | +0.02 |
| -40.72 | -0.02 | -0.01 | -0.01 | -0.00 | +0.00 | +0.01 | +0.01 | +0.01 | +0.02 | +0.02 | +0.02 | +0.02 | +0.02 | +0.02 |
| -38.72 | -0.03 | -0.02 | -0.01 | -0.00 | +0.01 | +0.01 | +0.02 | +0.02 | +0.02 | +0.02 | +0.02 | +0.02 | +0.02 | +0.03 |
| -36.72 | -0.04 | -0.02 | -0.01 | -0.00 | +0.01 | +0.02 | +0.02 | +0.02 | +0.03 | +0.03 | +0.03 | +0.03 | +0.03 | +0.03 |
| -34.72 | -0.04 | -0.03 | -0.01 | -0.00 | +0.01 | +0.02 | +0.03 | +0.03 | +0.03 | +0.03 | +0.03 | +0.03 | +0.03 | +0.04 |
| -32.72 | -0.06 | -0.03 | -0.02 | -0.01 | +0.01 | +0.03 | +0.03 | +0.04 | +0.04 | +0.04 | +0.04 | +0.04 | +0.04 | +0.05 |
| -30.72 | -0.07 | -0.04 | -0.02 | -0.01 | +0.01 | +0.03 | +0.04 | +0.04 | +0.05 | +0.05 | +0.05 | +0.05 | +0.05 | +0.06 |
| -28.72 | -0.09 | -0.05 | -0.03 | -0.01 | +0.02 | +0.04 | +0.05 | +0.06 | +0.06 | +0.06 | +0.06 | +0.06 | +0.06 | +0.08 |
| -26.72 | -0.11 | -0.06 | -0.03 | -0.01 | +0.02 | +0.05 | +0.06 | +0.07 | +0.07 | +0.07 | +0.07 | +0.07 | +0.08 | +0.10 |
| -24.72 | -0.14 | -0.07 | -0.04 | -0.01 | +0.03 | +0.06 | +0.07 | +0.08 | +0.09 | +0.09 | +0.09 | +0.09 | +0.09 | +0.12 |
| -22.72 | -0.17 | -0.09 | -0.05 | -0.01 | +0.03 | +0.07 | +0.09 | +0.10 | +0.10 | +0.11 | +0.11 | +0.11 | +0.11 | +0.14 |
| -20.72 | -0.21 | -0.11 | -0.06 | -0.01 | +0.04 | +0.08 | +0.10 | +0.11 | +0.12 | +0.12 | +0.12 | +0.12 | +0.13 | +0.16 |
| -18.72 | -0.26 | -0.14 | -0.08 | -0.02 | +0.04 | +0.10 | +0.12 | +0.13 | +0.14 | +0.14 | +0.14 | +0.14 | +0.15 | +0.19 |
| -16.72 | -0.32 | -0.17 | -0.09 | -0.02 | +0.05 | +0.11 | +0.14 | +0.15 | +0.16 | +0.16 | +0.16 | +0.16 | +0.17 | +0.21 |
| -14.72 | -0.39 | -0.20 | -0.11 | -0.02 | +0.06 | +0.12 | +0.15 | +0.16 | +0.17 | +0.18 | +0.18 | +0.18 | +0.18 | +0.23 |
| -12.72 | -0.47 | -0.24 | -0.12 | -0.02 | +0.06 | +0.10 | +0.12 | +0.13 | +0.14 | +0.14 | +0.14 | +0.14 | +0.15 | +0.19 |
| -10.72 | -0.57 | -0.28 | -0.13 | -0.02 | +0.05 | +0.05 | +0.06 | +0.07 | +0.07 | +0.07 | +0.07 | +0.07 | +0.08 | +0.10 |
| -8.72 | -0.66 | -0.31 | -0.12 | -0.00 | +0.03 | -0.03 | -0.04 | -0.05 | -0.05 | -0.05 | -0.05 | -0.05 | -0.05 | -0.07 |
| -6.72 | -0.76 | -0.29 | -0.06 | +0.03 | -0.02 | -0.16 | -0.20 | -0.23 | -0.24 | -0.25 | -0.25 | -0.25 | -0.26 | -0.32 |
| -4.72 | -0.82 | -0.18 | +0.04 | +0.07 | -0.08 | -0.35 | -0.44 | -0.49 | -0.53 | -0.54 | -0.54 | -0.54 | -0.56 | -0.71 |
| -2.72 | -0.63 | +0.04 | +0.22 | +0.14 | -0.18 | -0.61 | -0.78 | -0.86 | -0.93 | -0.94 | -0.95 | -0.95 | -0.98 | -1.26 |
| -0.72 | -0.21 | +0.41 | +0.49 | +0.23 | -0.31 | -0.95 | -1.22 | -1.35 | -1.46 | -1.49 | -1.50 | -1.50 | -1.55 | -2.00 |
| +1.28 | +0.50 | +0.96 | +0.89 | +0.36 | -0.48 | -1.38 | -1.77 | -1.98 | -2.15 | -2.19 | -2.21 | -2.21 | -2.28 | -2.99 |
| +3.28 | +1.57 | +1.74 | +1.41 | +0.52 | -0.69 | -1.89 | -2.45 | -2.75 | -3.00 | -3.06 | -3.08 | -3.08 | -3.19 | -4.27 |
| +5.28 | +3.01 | +2.74 | +2.07 | +0.72 | -0.93 | -2.48 | -3.25 | -3.67 | -4.01 | -4.11 | -4.13 | -4.13 | -4.30 | -5.89 |
| +7.28 | +4.76 | +3.94 | +2.85 | +0.96 | -1.19 | -3.12 | -4.14 | -4.72 | -5.19 | -5.32 | -5.36 | -5.36 | -5.59 | -7.92 |
| +9.28 | +6.74 | +5.29 | +3.72 | +1.21 | -1.47 | -3.80 | -5.11 | -5.87 | -6.52 | -6.70 | -6.75 | -6.74 | -7.06 | -10.51 |
| +11.28 | +8.81 | +6.70 | +4.63 | +1.48 | -1.76 | -4.48 | -6.13 | -7.12 | -7.97 | -8.21 | -8.28 | -8.28 | -8.71 | -13.93 |
| +13.28 | +10.88 | +8.11 | +5.54 | +1.74 | -2.03 | -5.14 | -7.16 | -8.41 | -9.53 | -9.85 | -9.94 | -9.94 | -10.52 | -18.86 |
| +15.28 | +12.85 | +9.46 | +6.40 | +2.00 | -2.28 | -5.77 | -8.17 | -9.72 | -11.16 | -11.59 | -11.70 | -11.70 | -12.49 | -28.41 |
| +17.28 | +14.66 | +10.70 | +7.19 | +2.23 | -2.51 | -6.34 | -9.13 | -11.02 | -12.84 | -13.40 | -13.55 | -13.55 | -14.60 | -31.46 |
| +19.28 | +16.29 | +11.79 | +7.89 | +2.43 | -2.71 | -6.84 | -10.03 | -12.27 | -14.54 | -15.26 | -15.46 | -15.45 | -16.86 | -21.87 |
| +21.28 | +17.71 | +12.74 | +8.49 | +2.61 | -2.88 | -7.28 | -10.83 | -13.45 | -16.24 | -17.15 | -17.41 | -17.41 | -19.29 | -18.08 |
| +23.28 | +18.92 | +13.55 | +9.00 | +2.76 | -3.02 | -7.65 | -11.55 | -14.54 | -17.90 | -19.06 | -19.39 | -19.39 | -21.92 | -15.89 |
| +25.28 | +19.94 | +14.21 | +9.42 | +2.88 | -3.13 | -7.96 | -12.17 | -15.52 | -19.51 | -20.96 | -21.39 | -21.39 | -24.82 | -14.46 |
| +27.28 | +20.78 | +14.75 | +9.76 | +2.98 | -3.22 | -8.21 | -12.69 | -16.40 | -21.06 | -22.86 | -23.41 | -23.40 | -28.12 | -13.47 |
| +29.28 | +21.46 | +15.19 | +10.03 | +3.06 | -3.30 | -8.42 | -13.14 | -17.16 | -22.52 | -24.73 | -25.43 | -25.42 | -32.10 | -12.75 |
| +31.28 | +22.01 | +15.54 | +10.25 | +3.12 | -3.36 | -8.59 | -13.51 | -17.82 | -23.88 | -26.57 | -27.45 | -27.44 | -37.48 | -12.22 |
| +33.28 | +22.45 | +15.82 | +10.42 | +3.17 | -3.41 | -8.72 | -13.81 | -18.38 | -25.14 | -28.37 | -29.47 | -29.46 | -47.35 | -11.83 |
| +35.28 | +22.79 | +16.04 | +10.56 | +3.22 | -3.45 | -8.83 | -14.06 | -18.85 | -26.28 | -30.11 | -31.50 | -31.47 | -50.74 | -11.53 |
| +37.28 | +23.07 | +16.21 | +10.67 | +3.25 | -3.48 | -8.92 | -14.26 | -19.24 | -27.31 | -31.80 | -33.52 | -33.49 | -41.31 | -11.29 |
| +39.28 | +23.28 | +16.35 | +10.76 | +3.27 | -3.50 | -8.99 | -14.42 | -19.56 | -28.22 | -33.41 | -35.54 | -35.51 | -37.65 | -11.11 |
| +41.28 | +23.45 | +16.45 | +10.82 | +3.29 | -3.52 | -9.04 | -14.55 | -19.83 | -29.01 | -34.94 | -37.56 | -37.52 | -35.55 | -10.97 |
| +43.28 | +23.58 | +16.54 | +10.88 | +3.31 | -3.54 | -9.09 | -14.66 | -20.04 | -29.69 | -36.38 | -39.59 | -39.53 | -34.19 | -10.86 |
| +45.28 | +23.69 | +16.61 | +10.92 | +3.32 | -3.55 | -9.12 | -14.74 | -20.22 | -30.28 | -37.72 | -41.61 | -41.53 | -33.24 | -10.78 |
| +47.28 | +23.77 | +16.66 | +10.95 | +3.33 | -3.56 | -9.15 | -14.81 | -20.36 | -30.77 | -38.95 | -43.64 | -43.54 | -32.56 | -10.71 |
| +49.28 | +23.84 | +16.70 | +10.98 | +3.34 | -3.57 | -9.18 | -14.87 | -20.47 | -31.18 | -40.08 | -45.67 | -45.55 | -32.05 | -10.66 |
| +51.28 | +23.89 | +16.73 | +11.00 | +3.35 | -3.57 | -9.19 | -14.91 | -20.56 | -31.52 | -41.08 | -47.70 | -47.55 | -31.67 | -10.62 |
| +53.28 | +23.93 | +16.76 | +11.01 | +3.35 | -3.58 | -9.21 | -14.95 | -20.64 | -31.81 | -41.97 | -49.74 | -49.55 | -31.38 | -10.58 |
| +55.28 | +23.96 | +16.78 | +11.03 | +3.36 | -3.58 | -9.22 | -14.97 | -20.70 | -32.04 | -42.75 | -51.80 | -51.56 | -31.16 | -10.56 |
| +57.28 | +23.96 | +16.80 | +11.04 | +3.36 | -3.58 | -9.23 | -14.99 | -20.74 | -32.22 | -43.42 | -53.86 | -53.56 | -30.98 | -10.54 |
| +59.28 | +23.96 | +16.81 | +11.05 | +3.36 | -3.59 | -9.23 | -15.01 | -20.78 | -32.37 | -43.99 | -55.95 | -55.56 | -30.85 | -10.52 |
| +61.28 | +23.96 | +16.82 | +11.05 | +3.36 | -3.59 | -9.24 | -15.03 | -20.81 | -32.49 | -44.47 | -58.05 | -57.56 | -30.74 | -10.51 |
| +63.28 | +23.96 | +16.83 | +11.06 | +3.37 | -3.59 | -9.24 | -15.04 | -20.83 | -32.59 | -44.88 | -60.18 | -59.56 | -30.66 | -10.50 |
| +65.28 | +23.96 | +16.83 | +11.06 | +3.37 | -3.59 | -9.25 | -15.04 | -20.85 | -32.67 | -45.21 | -62.35 | -61.57 | -30.59 | -10.49 |
| +67.28 | +23.96 | +16.83 | +11.07 | +3.37 | -3.59 | -9.25 | -15.05 | -20.87 | -32.73 | -45.48 | -64.57 | -63.57 | -30.54 | -10.48 |
| +69.28 | +23.96 | +16.83 | +11.07 | +3.37 | -3.59 | -9.25 | -15.06 | -20.88 | -32.78 | -45.71 | -66.85 | -65.57 | -30.50 | -10.48 |
| +71.28 | +23.96 | +16.83 | +11.07 | +3.37 | -3.59 | -9.26 | -15.06 | -20.89 | -32.83 | -45.89 | -69.22 | -67.57 | -30.47 | -10.47 |
| +73.28 | +23.96 | +16.83 | +11.07 | +3.37 | -3.59 | -9.26 | -15.07 | -20.90 | -32.86 | -46.04 | -71.70 | -69.57 | -30.44 | -10.47 |
| +75.28 | +23.96 | +16.83 | +11.07 | +3.37 | -3.59 | -9.26 | -15.07 | -20.90 | -32.88 | -46.16 | -74.35 | -71.57 | -30.42 | -10.47 |
| +77.28 | +23.96 | +16.83 | +11.07 | +3.37 | -3.59 | -9.26 | -15.07 | -20.91 | -32.90 | -46.26 | -77.24 | -73.57 | -30.40 | -10.46 |
| +79.28 | +23.96 | +16.83 | +11.07 | +3.37 | -3.60 | -9.26 | -15.07 | -20.91 | -32.92 | -46.33 | -80.52 | -75.57 | -30.39 | -10.46 |
| +81.28 | +23.96 | +16.83 | +11.07 | +3.37 | -3.60 | -9.26 | -15.07 | -20.91 | -32.93 | -46.39 | -84.44 | -77.57 | -30.38 | -10.46 |

## Appendix B — expander unit gain h(x) (dB) vs x = e − GateOpen (detector coordinates)

| x_det = L − GateOpen (dB) | expander gain (dB) |
|---|---|
| -58.87 | -60.91 |
| -56.87 | -58.91 |
| -54.87 | -56.91 |
| -52.87 | -54.91 |
| -50.87 | -52.91 |
| -48.87 | -50.91 |
| -46.87 | -48.91 |
| -44.87 | -46.91 |
| -42.87 | -44.92 |
| -40.87 | -42.92 |
| -38.87 | -40.92 |
| -36.87 | -38.93 |
| -34.87 | -36.94 |
| -32.87 | -34.94 |
| -30.86 | -32.95 |
| -28.86 | -30.97 |
| -26.86 | -28.99 |
| -24.86 | -27.01 |
| -22.86 | -25.03 |
| -20.86 | -23.07 |
| -18.86 | -21.11 |
| -16.86 | -19.16 |
| -14.87 | -17.23 |
| -12.87 | -15.32 |
| -10.87 | -13.43 |
| -8.87 | -11.58 |
| -6.87 | -9.76 |
| -4.87 | -7.99 |
| -2.87 | -6.30 |
| -0.86 | -4.70 |
| +1.14 | -3.24 |
| +3.13 | -1.96 |
| +5.13 | -0.95 |
| +7.13 | -0.37 |
| +9.13 | -0.20 |
| +11.13 | -0.13 |
| +13.13 | -0.09 |
| +15.13 | -0.07 |
| +17.14 | -0.05 |
| +19.14 | -0.04 |
| +21.14 | -0.03 |
| +23.14 | -0.03 |
| +25.14 | -0.02 |
| +27.14 | -0.02 |
| +29.14 | -0.02 |
| +31.14 | -0.02 |
| +33.13 | -0.02 |
| +35.13 | -0.02 |
| +37.13 | -0.02 |
| +39.13 | -0.02 |
| +41.13 | -0.02 |

## Changes from r1
| § | Change | Why (verification of the r1 build against the reference) |
|---|---|---|
| 2 | r = 0.8 is 50:1 and displays "50.00:1" | build displayed "−50.00:1" at exactly r = 0.8 (behaviour already 50:1) |
| 3.7 | Sidechain monitor follows the compressor's EQ mode only | reference monitor stayed wideband with only the gate filtered; build showed the gate's filter |
| 4 | Rectified-average detector (|x| one-pole, RECT_TAU) with K_CAL instead of a power pre-smoother | r1's power detector gives white noise ≈0.4 dB MORE reduction than a sine; the reference gives ≈0.8 dB LESS (§10.8); the r1 text claimed the reference behaviour but its model contradicted it |
| 4–5 | Compressor detector release DET_REL·Release, PDR fast release PDR_FAST_REL, slow memory (SLOW_REL, SLOW_W) | r1's single-stage law misses the reference envelopes (releases start faster and end slower) |
| 6 | Expander: fast detector release + dB gain smoothing | reference expander starts closing ≈10 ms after the drop; r1 law ≈65 ms |
| 7 | Allpass sign fixed: AP = D(−s)/D(s) for every type | build's band-pass split/passive output was polarity-inverted vs the reference (r1 said "up to sign") |
| 9 | Envelope tolerances re-based on sampled tables (§10.5.1/§10.5.3), achievable by the §4–§5 law | r1's per-1-ms criterion referred to curves that were never tabulated |
| 10.5.1, 10.5.3, 10.8, 10.9 | New normative tables (reference measurements) | trajectories, expander, detector type, PeakRef near full scale (output limit acts on the final output) |
Literal check (fresh implementation of this text, constants parsed from §4.3, against the reference captures): every
§10.5.1 cell within its row tolerance, per-case RMS 0.18–0.62 dB, P99 ≤ 2.12 dB; §10.5.3 within ±0.14; §10.8 within
±0.35 dB; static Appendix A unchanged (≤ 0.05 dB at 1 kHz).

## Changes from r2
| § | Change | Why (verification of the r2 build against the reference) |
|---|---|---|
| 3, 5.1, 10.3, 10.9, App. A | Hard clip of the final output at ±1.0 replaces the "≈ +2.8 dBFS soft expansion limit"; expansion columns of Appendix A replaced by unclipped curves | the reference output waveform is flat-topped at exactly 1.0 (with makeup or expansion driving it over); the r2 tables had absorbed that clip via RMS measurement; the r2 build let peaks reach +2.83 dBFS |
| 3 | Defined state at stream start / host reset (gate closed, detectors silent) | build carried gate state across renders (gate at 0.61 where the reference starts closed) |
| 4.3, 6 | EXP_CAL = 1.11 dB added to the expander detector level; Appendix B extrapolation rule | a literal r2 expander read 1.11 dB low against the reference statics (Gate 2/30 ms); r2's "x − 0.91" asymptote contradicted the table |
| 8 | Safety measures must not alter the output on the vectors | reviewing the build's onset hold and +30 dBFS clamp |
| 9 | Floor-law tolerance relaxed within ±2 dB of a negative-floor notch | notch depth is ill-conditioned (reference vs build −51.9 / −49.5 dB with 0.4 dB floor display difference) |
Literal check (r2 checker extended with the §5.1 clip and the new columns, against the reference): PeakRef near full
scale (compression ratios) ≤ 0.03 dB; expansion statics (T −80 and −40, PeakRef 0.50:1) ≤ 0.06 dB; expander statics with EXP_CAL
and extrapolation ≤ 0.01 dB (GateOpen −40 and −20); trajectory/expander/detector-type results unchanged from r2.
