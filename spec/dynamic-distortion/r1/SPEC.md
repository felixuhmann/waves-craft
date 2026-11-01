# Dynamic distortion — behavioural specification (r1)

Status: APPROVED r1 (review gate applied; see §11). Frozen — changes require a new revision.

Derived only from black-box measurement of a licensed reference processor in a VST3 host with user-generated test signals, host-visible parameter metadata and the reference's public user guide. No code, disassembly or vendor assets used or included. Product naming, parameter IDs, GUI and branding are the implementer's own.

## 1. Scope
Stereo-in/stereo-out (plus a mono variant: same processing on one channel). The two channels are processed **independently** (no linking, no crosstalk: measured ≤ −290 dB). Latency 0 samples. Behaviour is sample-rate independent (44.1/48/96 kHz agree within 0.11 dB on every static target); all times are in ms and all frequencies in Hz.

## 2. Definitions
`dBFS` = 20·log10(|amplitude|) for a full-scale-1.0 float signal; levels of sine tones are quoted as **RMS** dBFS (a sine of peak 1.0 is −3.01 dBFS RMS) unless marked `dBpk`.

## 3. Parameters
Host-normalised value r ∈ [0,1]. Display resolution in brackets.

| Parameter | Mapping | Range | Default |
|---|---|---|---|
| Input | −18 + 36·r dB [0.1 dB] | −18…+18 dB | 0 dB |
| Direct Level | 100·r % [0.1 %] | 0…100 % | 100 % |
| Drive | 100·r % [0.1 %] | 0…100 % | 50 % |
| Distortion Level | 100·r % [0.1 %] | 0…100 % | 62 % |
| Attack | 0.1·5000^r ms [0.1 ms] | 0.1…500 ms | 1.0 ms |
| Release | 5·1000^r ms [0.01 ms] | 5…5000 ms | 31.8 ms |
| Bass | −50 + 100·r [0.5] | −50…+50 | −50 |
| Mid | −50 + 100·r [0.5] | −50…+50 | 0 |
| Mid Freq | 150·(100/3)^r Hz [1 Hz] | 150 Hz…5 kHz | 1252 Hz |
| Treble | −50 + 100·r [0.5] | −50…+50 | −15 |
| Low-pass (LP) | 500·42^r Hz [1 Hz] | 500 Hz…21 kHz | 13015 Hz |
| Output | −18 + 36·r dB [0.1 dB] | −18…+18 dB | 0 dB |

## 4. Signal flow (per channel)
```
x ─► ×Input ─┬───────────────────────────────────────────────► ×Direct(D%) ──┐
             ├─► envelope detector (Attack, Release) ─► expansion gain E (dB)  │
             └─► tone EQ (Bass, Mid@MidFreq, Treble) ─► ×Drive(d) ─► ×E ─► CLIP ─► HP ─► LP ─► ×DistLevel(L%) ─► + ─► ×Output
```
- The detector sees the signal **after Input gain and before the tone EQ** (EQ settings do not move the expansion onset; Input gain moves it 1:1).
- The tone EQ, Drive and expansion act **before** the clipper; HP and LP act **after** it (verified: LP 1 kHz still clips a 2 kHz tone; Bass −50 prevents a 50 Hz tone from clipping).
- Output = Output-gain · (Direct path + Distortion path). Direct path is the input after Input gain only (no EQ, no filtering; bit-exact apart from the level law).

### 4.1 Level laws (normative, dB, linear interpolation in dB between rows; 0 % = −∞)

Direct Level (gain of the direct path):

| % | 5 | 10 | 20 | 25 | 30 | 40 | 50 | 60 | 70 | 75 | 80 | 90 | 100
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| -60.00 | -50.00 | -24.00 | -22.00 | -20.00 | -16.00 | -12.00 | -9.60 | -7.20 | -6.00 | -4.80 | -2.40 | +0.00 |

Distortion Level (gain applied to the distortion path; 100 % = 0 dB):

| % | 5 | 10 | 20 | 25 | 30 | 40 | 50 | 60 | 70 | 75 | 80 | 90 | 100
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| -55.00 | -40.00 | -30.00 | -28.00 | -26.00 | -22.00 | -18.00 | -14.40 | -10.80 | -9.00 | -7.20 | -3.60 | +0.00 |

Between 50 % and 100 % the laws are exactly linear in dB: Direct 0.24 dB/%, Distortion 0.36 dB/%. Tolerance ±0.05 dB.

### 4.2 Drive (normative)
Small-signal gain of the distortion path at 1 kHz (Input 0 dB, EQ neutral, Distortion Level 100 %, signal well below the expansion knee), dB, linear interpolation in dB between rows. Tolerance ±0.1 dB.

| Drive % | 0 | 5 | 10 | 15 | 20 | 25 | 30 | 35 | 40 | 45 | 50 | 55 | 60 | 65 | 70 | 75 | 80 | 85 | 90 | 95 | 100
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| -4.70 | +1.30 | +7.30 | +10.30 | +13.30 | +16.80 | +20.30 | +22.80 | +25.30 | +26.60 | +27.80 | +29.10 | +30.30 | +31.80 | +33.30 | +33.61 | +33.80 | +34.10 | +34.30 | +34.80 | +35.30 |

### 4.3 Expansion (normative static curve)
Steady 1 kHz sine, Drive 0 %, Attack 1 ms, Release 31.8 ms: increase of the distortion-path gain relative to its small-signal value, vs input sine level (RMS dBFS, after Input gain). Below −40 dBFS the gain is constant. Above −14 dBFS the reference's expansion cannot be observed (the clipper dominates); implementations shall continue the curve monotonically with slope ≤ 0.4 dB/dB and cap it at +8 dB. Tolerance ±0.4 dB.

| Level dBFS | -40 | -38 | -36 | -34 | -32 | -30 | -28 | -26 | -24 | -22 | -20 | -18 | -16 | -14 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| ΔG dB | +0.00 | +0.09 | +0.26 | +0.52 | +0.88 | +1.36 | +1.96 | +2.65 | +3.41 | +4.21 | +4.99 | +5.74 | +6.42 | +7.05 |

The detector is a per-channel full-wave **peak** follower in the linear amplitude domain (it follows the waveform at low frequencies with fast Attack, which produces most of the low-frequency distortion; see §6.2). Attack and Release are 'time-to-settle' style controls: the informative model in §8 uses one-pole time constants τ_att = Attack/3.67 and τ_rel = Release/1.99.

### 4.4 Clipper
Memoryless symmetric hard limit of the distortion-path signal at ±c with c ≈ 0.30–0.32 (output of a fully clipped tone at Distortion Level 100 %: −10.1 ± 0.3 dBFS RMS). Fully clipped tones show odd harmonics close to a square wave (h3 ≈ −9.5 dBc, h5 ≈ −14 dBc).

### 4.5 Tone EQ (pre-clipper; normative responses)
Small-signal magnitude of the distortion path relative to EQ neutral (Drive 0 %, −50 dBFS sweep), dB. Each band at a negative value is a cut filter whose depth is linear in the value (max 18 dB at −50); a positive value is the exact mirror (inverse) of the cut at the same |value|, combined with a broadband gain of −6·v/50 dB (Bass, Treble) or −9·v/50 dB (Mid) — so the boost peak is +12 dB (Bass/Treble) / +9 dB (Mid) at +50. Bass: low shelf, half depth at ≈100 Hz. Mid: bell at Mid Freq. Treble: wide bell centred ≈9 kHz. Tolerance ±0.5 dB (±1 dB above 15 kHz).

| setting | 20 | 50 | 100 | 200 | 500 | 1000 | 2000 | 5000 | 9000 | 12000 | 15000 | 20000 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| bass-50 | -16.51 | -12.89 | -9.00 | -5.11 | -1.48 | -0.43 | -0.11 | -0.02 | -0.00 | -0.00 | -0.00 | +0.00 |
| bass-25 | -8.31 | -6.48 | -4.50 | -2.52 | -0.68 | -0.19 | -0.05 | -0.01 | -0.00 | -0.00 | +0.00 | +0.00 |
| bass25 | +5.31 | +3.47 | +1.50 | -0.48 | -2.32 | -2.81 | -2.95 | -2.99 | -3.00 | -3.00 | -3.00 | -3.00 |
| bass50 | +10.52 | +6.89 | +3.00 | -0.89 | -4.52 | -5.57 | -5.89 | -5.98 | -6.00 | -6.00 | -6.00 | -6.00 |
| mid-50_f150 | -1.09 | -4.93 | -12.42 | -14.36 | -4.22 | -1.34 | -0.37 | -0.06 | -0.02 | -0.01 | -0.00 | -0.00 |
| mid50_f150 | -7.91 | -4.07 | +3.42 | +5.36 | -4.78 | -7.65 | -8.63 | -8.94 | -8.98 | -8.99 | -8.99 | -9.00 |
| mid-50_f1252 | -0.05 | -0.31 | -1.12 | -3.42 | -9.92 | -16.95 | -14.48 | -6.03 | -2.62 | -1.57 | -1.01 | -0.59 |
| mid50_f1252 | -8.95 | -8.70 | -7.88 | -5.58 | +0.92 | +7.94 | +5.48 | -2.97 | -6.38 | -7.43 | -7.99 | -8.41 |
| mid-50_f5000 | -0.00 | -0.03 | -0.11 | -0.44 | -2.22 | -5.72 | -11.14 | -18.00 | -13.94 | -10.99 | -8.78 | -6.43 |
| mid50_f5000 | -8.99 | -8.97 | -8.89 | -8.56 | -6.78 | -3.28 | +2.14 | +9.00 | +4.95 | +1.99 | -0.22 | -2.57 |
| treble-50 | -0.02 | -0.10 | -0.40 | -1.40 | -5.21 | -9.76 | -14.16 | -17.47 | -18.00 | -17.85 | -17.43 | -16.16 |
| treble25 | -3.00 | -2.99 | -2.95 | -2.82 | -2.00 | -0.11 | +2.80 | +5.54 | +6.00 | +5.87 | +5.50 | +4.41 |
| treble50 | -5.98 | -5.90 | -5.60 | -4.60 | -0.79 | +3.76 | +8.16 | +11.47 | +12.00 | +11.85 | +11.43 | +10.16 |

### 4.6 Post-clipper filters (normative)
- **LP**: second-order low-pass made of two identical first-order sections, −6.02 dB at the set frequency (Q = 0.5), sample-rate independent up to 0.45·fs. Measured (relative): 

| setting | 20 | 50 | 100 | 200 | 500 | 1000 | 2000 | 5000 | 9000 | 12000 | 15000 | 20000 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| lp500 | -0.01 | -0.09 | -0.34 | -1.29 | -6.02 | -13.98 | -24.63 | -40.23 | -50.54 | -55.38 | -58.63 | -61.34 |
| lp2000 | -0.00 | -0.01 | -0.02 | -0.09 | -0.53 | -1.95 | -6.02 | -17.21 | -26.63 | -31.45 | -35.06 | -38.82 |
| lp8000 | +0.00 | +0.00 | -0.00 | -0.01 | -0.04 | -0.15 | -0.56 | -2.94 | -7.05 | -10.15 | -13.09 | -17.21 |

- **HP** (distortion path only): small-signal response relative to 1 kHz: 20 Hz -7.17 dB, 30 Hz -1.85 dB, 50 Hz -0.35 dB, 70 Hz -0.14 dB, 100 Hz -0.06 dB, 150 Hz -0.02 dB, 200 Hz -0.01 dB. Tolerance ±0.5 dB.

## 5. Small-signal behaviour
At Drive d and levels below −40 dBFS the distortion path is linear with gain = Drive table · EQ · HP · LP · Distortion law; the direct path is exactly the Direct law. Harmonics at −50 dBFS are ≤ −50 dBc.

## 6. Large-signal targets (normative, 48 kHz; same at 44.1/96 kHz)
### 6.1 Static tone tables
Direct 0 %, Distortion Level 100 %, EQ neutral, LP 21 kHz, Input/Output 0 dB. Cells: fundamental gain (dB, output fundamental / input) / third harmonic (dBc). Tolerances: fundamental ±2 dB at 200 Hz–2 kHz, ±3.5 dB at 50 Hz and for Drive 0 % at −10/0 dBFS (clipping knee, where the reference's knee is softer than a hard limit). h3: ±6 dB where the table value is > −25 dBc (±15 dB for Drive 0 % at −10/0 dBFS); ±18 dB in the onset band −40 < value ≤ −25 dBc (h3 changes steeply with level there); ≤ −40 dBc where the table value is ≤ −50 dBc.


**Drive 0 %, Attack 1 ms, Release 31.8 ms**

| f \ level | -50 | -40 | -30 | -20 | -10 | 0 |
|---|---|---|---|---|---|---|
| 50 Hz | -5.0 / -99 | -5.0 / -99 | -4.0 / -29 | -0.4 / -30 | -1.1 / -11 | -11.0 / -10 |
| 200 Hz | -4.7 / -99 | -4.7 / -99 | -3.6 / -43 | +0.2 / -62 | +1.3 / -17 | -8.6 / -14 |
| 500 Hz | -4.7 / -99 | -4.7 / -99 | -3.3 / -99 | +0.3 / -77 | +1.9 / -20 | -7.9 / -16 |
| 2000 Hz | -4.7 / -99 | -4.7 / -99 | -3.3 / -99 | +0.3 / -77 | +2.0 / -21 | -7.8 / -17 |

**Drive 0 %, Attack 100 ms, Release 31.8 ms**

| f \ level | -50 | -40 | -30 | -20 | -10 | 0 |
|---|---|---|---|---|---|---|
| 50 Hz | -5.0 / -99 | -5.0 / -99 | -5.0 / -56 | -4.3 / -45 | +0.4 / -44 | -7.1 / -16 |
| 200 Hz | -4.7 / -99 | -4.7 / -99 | -3.9 / -75 | -0.5 / -79 | +2.7 / -41 | -7.3 / -18 |
| 500 Hz | -4.7 / -99 | -4.7 / -99 | -3.5 / -99 | -0.3 / -79 | +2.8 / -42 | -7.3 / -18 |
| 2000 Hz | -4.7 / -99 | -4.7 / -99 | -3.5 / -99 | -0.3 / -79 | +2.8 / -43 | -7.3 / -18 |

**Drive 0 %, Attack 0.1 ms, Release 5 ms**

| f \ level | -50 | -40 | -30 | -20 | -10 | 0 |
|---|---|---|---|---|---|---|
| 50 Hz | -5.0 / -99 | -5.0 / -99 | -3.9 / -29 | -0.4 / -25 | -1.8 / -12 | -11.7 / -11 |
| 200 Hz | -4.7 / -99 | -4.7 / -99 | -3.5 / -41 | +0.2 / -50 | -1.6 / -12 | -11.5 / -11 |
| 500 Hz | -4.7 / -99 | -4.7 / -99 | -3.3 / -99 | +0.3 / -77 | -1.2 / -11 | -11.1 / -10 |
| 2000 Hz | -4.7 / -99 | -4.7 / -99 | -3.3 / -99 | +0.3 / -77 | +1.1 / -16 | -8.7 / -13 |

**Drive 50 %, Attack 1 ms, Release 31.8 ms**

| f \ level | -50 | -40 | -30 | -20 | -10 | 0 |
|---|---|---|---|---|---|---|
| 50 Hz | +27.5 / -86 | +27.4 / -38 | +19.2 / -8 | +9.1 / -7 | -0.9 / -7 | -10.9 / -7 |
| 200 Hz | +27.8 / -86 | +27.8 / -52 | +21.5 / -10 | +11.3 / -8 | +1.3 / -8 | -8.7 / -8 |
| 500 Hz | +27.8 / -86 | +27.8 / -52 | +22.2 / -11 | +11.9 / -9 | +1.9 / -9 | -8.1 / -9 |
| 2000 Hz | +27.8 / -86 | +27.9 / -52 | +22.3 / -11 | +12.0 / -9 | +2.0 / -9 | -8.0 / -9 |

**Drive 100 %, Attack 1 ms, Release 31.8 ms**

| f \ level | -50 | -40 | -30 | -20 | -10 | 0 |
|---|---|---|---|---|---|---|
| 50 Hz | +35.0 / -60 | +29.1 / -9 | +19.1 / -7 | +9.1 / -7 | -0.9 / -7 | -10.9 / -7 |
| 200 Hz | +35.3 / -60 | +31.5 / -13 | +21.4 / -8 | +11.3 / -8 | +1.3 / -8 | -8.7 / -8 |
| 500 Hz | +35.3 / -60 | +32.1 / -14 | +22.0 / -10 | +11.9 / -9 | +1.9 / -9 | -8.1 / -9 |
| 2000 Hz | +35.3 / -60 | +32.3 / -15 | +22.2 / -10 | +12.0 / -9 | +2.0 / -9 | -8.0 / -9 |

### 6.2 Envelope dynamics (normative)
1 kHz sine, Drive 0 %, level −50 dBFS for 1 s, then −20 dBFS for 1 s, then −50 dBFS. Values: distortion-path gain (dB, output RMS / input RMS over 1 ms windows) at the given time after the level change. Tolerance ±2.5 dB (the reference's +10 ms point after the drop includes window-straddling artefacts and is informative only).

| setting | ↑+1 | ↑+3 | ↑+10 | ↑+30 | ↑+100 | ↑+300 | ↑+990 | ↓+10 | ↓+30 | ↓+100 | ↓+300 | ↓+1000 | ↓+1990 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| A 0.1 / R 31.8 | +0.16 | +0.25 | +0.30 | +0.29 | +0.29 | +0.29 | +0.29 | -0.12 | -3.80 | -4.70 | -4.70 | -4.70 | -4.70 |
| A 1 / R 31.8 | -0.29 | +0.19 | +0.30 | +0.29 | +0.29 | +0.29 | +0.29 | -0.12 | -3.80 | -4.70 | -4.70 | -4.70 | -4.70 |
| A 10 / R 31.8 | -3.58 | -2.74 | -1.06 | +0.12 | +0.29 | +0.29 | +0.29 | -0.12 | -3.80 | -4.70 | -4.70 | -4.70 | -4.70 |
| A 100 / R 31.8 | -4.62 | -4.48 | -4.01 | -3.07 | -1.21 | +0.10 | +0.29 | -0.13 | -3.81 | -4.70 | -4.70 | -4.70 | -4.70 |
| A 500 / R 31.8 | -4.73 | -4.69 | -4.54 | -4.30 | -3.55 | -2.08 | -0.25 | -0.96 | -4.40 | -4.70 | -4.70 | -4.70 | -4.70 |
| A 1 / R 5 | -0.29 | +0.19 | +0.30 | +0.29 | +0.29 | +0.29 | +0.29 | -1.92 | -4.56 | -4.70 | -4.70 | -4.70 | -4.70 |
| A 1 / R 300 | -0.29 | +0.19 | +0.30 | +0.29 | +0.29 | +0.29 | +0.29 | +1.10 | -0.31 | -2.08 | -4.70 | -4.70 | -4.70 |
| A 1 / R 3000 | +0.85 | +0.86 | +0.91 | +0.90 | +0.88 | +0.83 | +0.67 | +1.64 | +0.68 | +0.46 | +0.01 | -2.85 | -4.70 |

(Release 3000: the 'before' state +0.9 dB reflects the preceding programme; tests must precede the step with ≥ 5 s at −50 dBFS or compare against the same preceding programme.)

### 6.3 Linking and channels
A tone on the left channel only produces exactly zero on the right channel; the right channel's distortion depends only on its own signal.

## 7. Aliasing
The reference applies the clipper without oversampling: a 15 kHz, −6 dBFS tone at Drive 50 % produces an alias at 3 kHz of −10.5 dBc. See review decision D1.

## 8. Informative model (non-normative)
One structure that meets §4–§6 within tolerance (verified by implementing this text literally): peak follower e[n] = |u|>e ? a·e+(1−a)|u| : r·e+(1−r)|u| with a = exp(−1/(τ_att·fs)), r = exp(−1/(τ_rel·fs)), τ_att = Attack/3.67, τ_rel = Release/1.99; expansion ΔG = soft-knee(20·log10 e − K) with K = -39.97 dBpk, slope 0.253 dB/dB, knee width 1.41 dB, cap 5.86 dB (or the §4.3 table directly); clip at c = 0.3039; HP second-order Butterworth ≈ 41 Hz is a coarser fit than §4.6's table — use the table. Achieved: static fundamentals RMS 1.0 dB (max 3.2 dB), audible h3 median 1.4 dB, envelope steps ≤ 2.5 dB.

## 9. Tolerances summary
Level/drive laws ±0.05–0.1 dB; EQ/LP ±0.5 dB (±1 dB > 15 kHz); HP ±0.5 dB; expansion ±0.4 dB; static tone tables per §6.1; envelope ±2.5 dB; latency 0; channels independent; sample-rate independence ±0.2 dB on §6.1 at 44.1/96 kHz.

## 10. Behaviour not specified
The reference's exact within-cycle waveform (clipped half-cycles sag by ≈30 % from start to end, i.e. a 90°-shifted fundamental component is present); its parameter smoothing (it applies changes asynchronously); Mono variant (assumed: one channel of the stereo processing); expansion above −14 dBFS (masked by the clipper).

## 11. Review decisions
- **D1 Aliasing:** not reproduced. The clipper shall be anti-aliased (oversampling ≥ 4× or an equivalent technique) so that the §7 test yields aliases ≤ −40 dBc; harmonic targets in §6 are unchanged. Latency introduced by anti-aliasing shall be reported to the host and kept ≤ 64 samples.
- **D2 Parameter changes:** smoothed click-free within ≤ 20 ms (the reference's asynchronous application is not reproduced).
- **D3 Waveform sag (§10):** not required; matching the §6 tables is the requirement.
- **D4 Naming:** no vendor, product or person names; the processor is a 'dynamic distortion'.

## 12. User interface (functional layout — visuals must be original)
Two sections. **Distortion section:** Drive, Attack, Release, Direct Level and Distortion Level controls with value readouts; Input and Output level controls with meters. **Tone section (pre-distortion):** Bass, Mid, Mid Freq, Treble and LP controls, with a small response curve showing the EQ + LP shape. Double-click or Alt-click resets a control; values can be typed. All artwork, colours, fonts and names are the implementer's own.

## 13. Open questions
1. Exact attack/release law (only the §6.2 trajectories are normative).
2. Clipper knee softness near the threshold at low Drive (§6.1 tolerance widened there).
3. Expansion above −14 dBFS.
4. Mono variant not measured.
5. Exact EQ filter prototypes (only responses are normative).

## 14. Public references
RBJ audio-EQ cookbook (shelf/bell filters); Giannoulis, Massberg, Reiss, 'Digital dynamic range compressor design — a tutorial and analysis', JAES 60(6), 2012 (envelope followers, soft knees); Parker, Zavalishin, Le Bivic, 'Reducing the aliasing of nonlinear waveshaping using continuous-time convolution', DAFx 2016; Pakarinen & Yeh, 'A review of digital techniques for modeling vacuum-tube guitar amplifiers', CMJ 33(2), 2009.
