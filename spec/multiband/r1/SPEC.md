# Four-band multiband dynamics processor — behavioural specification (candidate r1)

Status: APPROVED r1 (2026-10-29, review decisions §7). Frozen — changes require a new revision. Derived only from black-box measurement of a licensed reference processor (host renders of
user-generated stimuli, host-visible parameter metadata) and its public/licensed documentation. No code,
disassembly or vendor assets. Product naming, parameter IDs, GUI and branding are the implementer's own.

## 1. Scope
Stereo in / stereo out (a mono variant applies the same processing to one channel). A complementary
4-band crossover splits the signal; each band has an independent level-dependent gain ("dynamic EQ"
style: one Range control instead of a ratio); the bands are summed and an output gain applied.
Latency: 0 samples (the crossover is minimum-phase; impulse peaks are delayed only by group delay).
The whole processor is linear time-invariant whenever every band's Range is 0 dB.

## 2. Parameters (host-normalised value r ∈ [0,1])
| Parameter | Mapping | Range | Default | Display |
|---|---|---|---|---|
| Output Gain | −18 + 36 r dB | −18 … +18 dB | 0.0 dB | 0.1 dB |
| Low / Mid / High Crossover | 16 · (21357/16)^r Hz (log) | 16 Hz … 21.4 kHz | 92 / 4000 / 11071 Hz | 1 Hz |
| Crossover Q | 0.10 + 0.65 r | 0.10 … 0.75 | 0.60 | 0.01 |
| Knee | r = 0 shows "Soft", r = 1 "Hard", else value 0.75 r (display) | Soft … Hard | 0.50 (r 0.667) | 0.01 |
| Release mode | Auto / Manual (toggle) | | Auto | |
| Behavior | Electro / Opto (toggle) | | Electro | |
| Band n Gain | −18 + 36 r dB | −18 … +18 dB | 0.0 dB | 0.1 dB |
| Band n Range | −24 + 42 r dB | −24 … +18 dB | −8.0 dB | 0.1 dB |
| Band n Threshold | −80 + 80 r dB | −80 … 0 dB | −27.0 / −19.2 / −30.1 / −35.3 dB | 0.1 dB |
| Band n Attack | 0.5 · 1000^r ms (log) | 0.5 … 500 ms | 34.99 / 16.03 / 4.99 / 2.00 ms | 0.01 ms |
| Band n Release | 5 · 1000^r ms (log) | 5 … 5000 ms | 40.27 / 25.00 / 9.98 / 5.00 ms | 0.01 ms |
| Band n Solo, Band n Bypass | toggles | | off | |
| Bypass (host) | | | off | |

Crossover frequencies are expected in ascending order (Low < Mid < High); behaviour for crossed settings is
implementation-defined but must stay stable and click-free.

## 3. Crossover (exact)
Each crossover k (frequency f_k, common Q) uses one second-order section, evaluated as a digital filter obtained by
the bilinear transform pre-warped at f_k (i.e. analog prototype in s normalised to f_k, s → j·tan(πf/fs)/tan(πf_k/fs)):

```
w0(Q): the unique w0 > 0 with |w0² / (w0² − 1 + j·w0/Q)| = 1/√2   (each section is −3.01 dB at f_k)
D_k(s) = s² + s·w0/Q + w0²
LP_k  = (w0² / D_k)²                                   (−6.02 dB at f_k)
AP_k  = (s² − s·w0/Q + w0²) / D_k                       (2nd-order allpass)
HP_k  = AP_k − LP_k
```
Bands: `B1 = LP1·AP2·AP3`, `B2 = HP1·LP2·AP3`, `B3 = HP1·HP2·LP3`, `B4 = HP1·HP2·HP3`.
Hence `B1+B2+B3+B4 = AP1·AP2·AP3` (unit magnitude, minimum-phase allpass).

Measured agreement of this model with the reference (48 kHz, Q 0.1/0.3/0.6/0.75, two crossover sets): phase error
≤ 0.03°, complex error of every band ≤ −51 dB re unity (typically −63 … −140 dB), summed response −66 … −119 dB.

Examples (48 kHz, crossovers 92/4000/11071 Hz, Q 0.6): |B1| at 92 Hz −6.02 dB, at 1 kHz −76.7 dB; |B2| at 1 kHz
−0.29 dB, at 4 kHz −6.02 dB; |B3| at 6 kHz −3.09 dB; |B4| at 15 kHz −1.51 dB; sum magnitude 0.000 dB at all frequencies.
Q 0.1: |B1| at 1 kHz −42.2 dB. Q 0.75: |B1| at 20 Hz +0.08 dB (slight peaking).

Solo: output = sum of the soloed bands (with their dynamics/gain); Bypass of a band: that band at unity gain with no
dynamics (its Gain is ignored). Output Gain multiplies the sum. Host bypass is bit-exact.

## 4. Static gain curve (per band, exact in structure, tabulated in shape)
Each band applies a real gain `G_band = Gain_dB + Range · f(x; Knee, Range)` (dB) to its band signal, where
`x = L_band − Threshold` and `L_band` is the band signal's level expressed as the peak level of an equivalent
steady sine (dBFS). The detector works on the band's own (crossover-filtered) signal; bands do not interact.
`f` rises monotonically from 0 (well below threshold) towards 1 (well above): Gain applies at low levels, Gain+Range at
high levels. f(+6.4 dB) = 0.5 for every Range and Knee, i.e. half the Range is reached 6.4 dB above Threshold
(sine-peak terms). Static behaviour is memoryless (no hysteresis, ≤ 0.002 difference rising vs falling) and does not
depend on Release mode or Behavior.

Normative table of f(x) at Range −12 dB for the Knee settings (x in dB; between rows interpolate linearly in Knee
value, Soft = 0.00, Hard = 0.75):

| x | −36 | −32 | −28 | −24 | −20 | −16 | −12 | −8 | −4 | 0 | +4 | +8 | +12 | +16 | +20 | +24 | +28 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Soft | .009 | .013 | .021 | .033 | .051 | .078 | .118 | .174 | .247 | .339 | .444 | .555 | .660 | .752 | .826 | .882 | .921 |
| 0.15 | .007 | .010 | .017 | .026 | .042 | .066 | .102 | .157 | .232 | .328 | .441 | .558 | .668 | .762 | .835 | .889 | .927 |
| 0.30 | .004 | .007 | .011 | .018 | .031 | .051 | .085 | .138 | .216 | .318 | .437 | .561 | .675 | .770 | .843 | .895 | .932 |
| 0.50 | .000 | .001 | .003 | .006 | .014 | .030 | .060 | .113 | .195 | .304 | .433 | .564 | .683 | .779 | .851 | .902 | .937 |
| 0.71 | .000 | .000 | .000 | .000 | .000 | .005 | .033 | .087 | .173 | .291 | .429 | .567 | .690 | .788 | .859 | .908 | .941 |
| Hard | .000 | .000 | .000 | .000 | .000 | .001 | .028 | .082 | .169 | .289 | .428 | .568 | .691 | .789 | .860 | .909 | .941 |

Range dependence (Knee 0.50) — add `f_R(x) − f_{−12}(x)` from this table to the Knee table above:

| x | −36 | −32 | −28 | −24 | −20 | −16 | −12 | −8 | −4 | 0 | +4 | +8 | +12 | +16 | +20 | +24 | +28 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| R −24 | .001 | .002 | .005 | .011 | .023 | .046 | .086 | .146 | .229 | .331 | .443 | .555 | .660 | .750 | .823 | .879 | .919 |
| R −12 | .000 | .001 | .003 | .006 | .014 | .030 | .060 | .113 | .195 | .304 | .433 | .564 | .683 | .779 | .851 | .902 | .937 |
| R −8 | .000 | .001 | .002 | .005 | .012 | .026 | .054 | .105 | .185 | .297 | .430 | .567 | .689 | .787 | .858 | .908 | .941 |
| R −3 | .000 | .001 | .002 | .004 | .010 | .023 | .048 | .096 | .176 | .289 | .427 | .569 | .696 | .794 | .865 | .913 | .945 |
| R +3 | .000 | .001 | .002 | .004 | .009 | .020 | .043 | .089 | .167 | .283 | .425 | .572 | .701 | .800 | .871 | .917 | .948 |
| R +6 | .000 | .001 | .001 | .003 | .008 | .019 | .042 | .087 | .165 | .280 | .424 | .572 | .702 | .802 | .872 | .919 | .949 |
| R +12 | .000 | .001 | .001 | .003 | .007 | .018 | .039 | .084 | .161 | .278 | .423 | .572 | .703 | .803 | .873 | .920 | .949 |
| R +18 | .000 | .000 | .001 | .003 | .007 | .017 | .039 | .083 | .162 | .279 | .424 | .572 | .701 | .801 | .872 | .919 | .949 |

### 4.1 Hard-knee Range rows (normative)
| x | −36 | −32 | −28 | −24 | −20 | −16 | −12 | −8 | −4 | 0 | +4 | +8 | +12 | +16 | +20 | +24 | +28 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Hard, R −24 | .000 | .000 | .000 | .000 | .003 | .023 | .063 | .126 | .213 | .321 | .440 | .558 | .665 | .757 | .829 | .884 | .923 |
| Hard, R −12 | .000 | .000 | .000 | .000 | .000 | .001 | .028 | .082 | .169 | .289 | .428 | .568 | .691 | .789 | .860 | .909 | .941 |
| Hard, R +18 | .000 | .000 | .000 | .000 | .000 | .000 | .000 | .000 | .050 | .214 | .406 | .584 | .724 | .824 | .890 | .932 | .958 |

Tolerance for the static curve: ±0.3 dB for |Range| ≤ 12, ±0.4 dB above.

Beyond x = +28 continue smoothly towards 1 (measured: f(+32) ≈ 0.95 at Range −12, Knee 0.5). Below x = −36 f → 0.
Spot checks for the combined rule: Range −24 / Hard: f(−8) = 0.126, f(0) = 0.321, f(+12) = 0.665;
Range +18 / Hard: f(−4) = 0.050, f(0) = 0.214, f(+12) = 0.724 (tolerance ±0.02 in f).
Range 0 ⇒ no dynamics (band is a fixed gain). Thresholds shift the curve exactly (x = L − T).
Static measurements were taken with fast time constants (Attack 0.5 ms, Release 5 ms) and agree with slow settings
(Attack 5–50 ms, Release 50 ms: −10.17 dB at −10 dBFS, Range −12, Threshold −30 in both cases).

## 5. Dynamics (attack / release / behaviour / Auto)
Normative behaviour is given by envelope tables (band 2, 1 kHz tone, Range −12, Threshold −30, Knee 0.50; input
steps −50 → −10 dBFS at t = 0 for attack and −10 → −50 dBFS for release after 1 s at −10). Values are the band
gain in dB. Tolerance: ±1.0 dB or ±25 % of the change from the start value, whichever is larger.

Attack (Manual/Electro, Release 50 ms):

| t after step | 1 ms | 2 ms | 5 ms | 10 ms | 20 ms | 50 ms | 100 ms | 200 ms | 500 ms |
|---|---|---|---|---|---|---|---|---|---|
| Attack 0.5 ms | −9.7 | −10.3 | −10.1 | −10.2 | −10.2 | −10.2 | −10.2 | −10.2 | −10.2 |
| Attack 5 ms | −2.7 | −3.1 | −5.5 | −8.2 | −9.9 | −10.2 | −10.2 | −10.2 | −10.2 |
| Attack 1 ms | −7.5 | −8.9 | −10.1 | −10.2 | −10.2 | −10.2 | −10.2 | −10.2 | −10.2 |
| Attack 20 ms | −1.3 | −1.1 | −1.7 | −3.0 | −5.2 | −8.8 | −10.1 | −10.2 | −10.2 |
| Attack 50 ms | −1.0 | −0.6 | −0.8 | −1.4 | −2.4 | −5.1 | −8.0 | −9.8 | −10.2 |
| Attack 200 ms | −0.9 | −0.4 | −0.3 | −0.5 | −0.8 | −1.6 | −2.9 | −5.1 | −8.8 |
| Attack 5 ms, Knee Hard | −2.5 | −3.0 | −5.4 | −8.2 | −10.0 | −10.3 | −10.3 | −10.3 | −10.3 |
| Attack 5 ms, Knee Soft | −3.0 | −3.4 | −5.6 | −8.1 | −9.6 | −9.9 | −9.9 | −9.9 | −9.9 |
| Attack 5 ms, Range −24 | −3.7 | −4.6 | −8.4 | −13.8 | −18.6 | −19.6 | −19.7 | −19.7 | −19.7 |
| Attack 5 ms, Range +12 | +3.5 | +5.5 | +8.2 | +9.7 | +10.3 | +10.4 | +10.4 | +10.4 | +10.4 |

Release after a sustained (1 s) overload (Attack 5 ms):

| t after step | 20 ms | 50 ms | 100 ms | 200 ms | 500 ms | 1 s | 2 s |
|---|---|---|---|---|---|---|---|
| Rel 5 ms, Electro | −7.0 | −3.5 | −1.4 | −0.4 | −0.2 | −0.2 | −0.2 |
| Rel 20 ms, Electro | −8.5 | −4.9 | −1.9 | −0.4 | −0.2 | −0.2 | −0.2 |
| Rel 50 ms, Electro | −9.3 | −6.7 | −3.3 | −0.7 | −0.2 | −0.2 | −0.2 |
| Rel 200 ms, Electro | −9.9 | −8.9 | −6.8 | −3.7 | −0.2 | −0.2 | −0.2 |
| Rel 500 ms, Electro | −10.1 | −9.6 | −8.5 | −6.5 | −2.7 | −0.2 | −0.2 |
| Rel 1000 ms, Electro | −10.1 | −9.9 | −9.3 | −8.0 | −5.1 | −2.2 | −0.2 |
| Rel 5000 ms, Electro | −10.1 | −10.1 | −10.0 | −9.7 | −8.7 | −7.4 | −5.5 |
| Rel 20 ms, Opto | −6.2 | −3.2 | −1.3 | −0.3 | −0.2 | −0.2 | −0.2 |
| Rel 50 ms, Opto | −6.3 | −3.2 | −1.3 | −0.3 | −0.2 | −0.2 | −0.2 |
| Rel 200 ms, Opto | −6.7 | −3.7 | −1.8 | −0.7 | −0.2 | −0.2 | −0.2 |
| Rel 500 ms, Opto | −7.4 | −4.6 | −2.6 | −1.5 | −0.8 | −0.4 | −0.2 |
| Rel 1000 ms, Opto | −7.9 | −5.4 | −3.6 | −2.5 | −1.7 | −1.1 | −0.5 |
| Rel 50 ms, Electro, Knee Hard | −9.4 | −6.6 | −3.2 | −0.5 | 0.0 | 0.0 | 0.0 |
| Rel 50 ms, Electro, Knee Soft | −9.1 | −6.7 | −3.6 | −1.1 | −0.6 | −0.6 | −0.6 |
| Rel 50 ms, Electro, Range −24 | −17.0 | −10.7 | −4.9 | −1.2 | −0.5 | −0.5 | −0.5 |
| Rel 50 ms, Electro, Range +12 | +10.1 | +8.9 | +5.9 | +1.5 | +0.1 | +0.1 | +0.1 |

Behaviour: Electro releases slowly while the gain change is large and faster as it approaches the static value;
Opto does the opposite (fast at large gain change, slowing near the static value). After a sustained overload Auto is
identical to Manual. After a short overload Auto releases faster (program-dependent release):

Release after a 30 ms burst (−10 dBFS) starting from −50 dBFS (values at t after the burst ends):

| t | 20 ms | 50 ms | 100 ms | 200 ms | 500 ms |
|---|---|---|---|---|---|
| Rel 50, Electro, Manual | −1.2 | −0.8 | −0.4 | −0.2 | −0.2 |
| Rel 50, Electro, Auto | −1.1 | −0.7 | −0.4 | −0.2 | −0.2 |
| Rel 500, Electro, Manual | −4.0 | −2.5 | −1.9 | −1.0 | −0.2 |
| Rel 500, Electro, Auto | −1.8 | −1.3 | −0.9 | −0.3 | −0.2 |
| Rel 500, Opto, Manual | −4.8 | −3.4 | −2.7 | −2.2 | −1.3 |
| Rel 500, Opto, Auto | −2.1 | −1.5 | −1.4 | −1.3 | −0.8 |
| Rel 200, Electro, Manual | −2.8 | −1.5 | −0.9 | −0.3 | −0.2 |
| Rel 200, Electro, Auto | −1.4 | −1.1 | −0.5 | −0.2 | −0.2 |
| Rel 1000, Electro, Manual | −4.5 | −3.1 | −2.4 | −1.9 | −0.2 |
| Rel 1000, Electro, Auto | −1.9 | −1.4 | −1.2 | −0.5 | −0.2 |
| Rel 200, Opto, Manual | −4.6 | −3.1 | −2.2 | −1.7 | −0.5 |
| Rel 200, Opto, Auto | −2.0 | −1.4 | −1.3 | −0.9 | −0.3 |
| Rel 1000, Opto, Manual | −4.9 | −3.6 | −2.9 | −2.5 | −1.9 |
| Rel 1000, Opto, Auto | −2.2 | −1.7 | −1.6 | −1.5 | −1.1 |
| Rel 20, Electro/Opto, Manual vs Auto | difference ≤ 0.1 dB at Release 20 ms |  |  |  |  |

Suggested (non-binding) structure: detector on the band signal → level in dB smoothed with Attack as a one-pole time
constant on rising level (plus a short initial fast component) → static curve f → gain smoothing during release whose
rate depends on the current gain change per Behavior, scaled by the Release setting; Auto shortens the effective release
when the preceding overload was short. Time constants are in milliseconds and independent of sample rate.

## 6. Stereo, detector, sample rate, smoothing
- Stereo: every band's detector is linked across channels: both channels receive the same band gain, determined by the
  louder channel (measured: L −10 / R −40 dBFS → both channels −10.17 dB; L −40 / R −10 → both −10.17 dB at Range −12;
  same for Range +12 → +10.43 dB on both).
- Detector type: peak-responding (sine-peak-equivalent levels as in §4). Test vector (Range −12, Threshold −30, Knee 0.50;
  white noise whose band-2 component has the given RMS; identical for Attack 0.5/Release 5 and Attack 5/Release 50):
  band RMS −32.5 → −5.9 dB, −28.4 → −7.3 dB, −24.0 → −8.8 dB, −20.1 → −9.5 dB, −16.1 → −10.2 dB, −12.1 → −10.4 dB
  (tolerance ±0.75 dB). An RMS detector would give ≈1.5–2 dB less reduction.
- Sample rate: static curves, attack/release envelopes and the crossover model are sample-rate independent (verified
  44.1/48/96 kHz: dynamics within 0.05 dB, static within 0.05 dB). Time constants are in ms.
- Parameter smoothing: click-free, ≤ 20 ms (review decision D2).
- Neutral settings (all Range 0, all Gains 0): the output is the crossover allpass (unit magnitude, not bit-exact);
  host bypass is bit-exact.

## 7. Review decisions
D1. The crossover and static curve are reproduced as measured (structure exact, curve shape by table).
D2. Parameter changes must be smoothed (click-free, ≤ 20 ms); the reference's smoothing was not measured.
D3. Dynamics are specified by envelope tables (release law not reducible to a single closed form from the data).
D4. Crossed crossover frequencies: implementation-defined, stable.
D5. The §3 crossover model applies at every sample rate. The reference shows an additional smooth phase deviation
    (≤ 8.3°, magnitude still exactly unity) at 88.2/96 kHz for Q < 0.5; this is not reproduced.
D6. Static curve shape between measured (Knee, Range) points follows the interpolation rule of §4 (Knee table plus Range
    correction; between Knee 0.50 and Hard the Range correction blends linearly into the Hard-knee rows of §4.1).

## 8. UI (functional layout; ORIGINAL visuals)
- Large frequency-domain display (20 Hz–20 kHz, log) showing: the three crossover markers (draggable horizontally
  to move a crossover, vertically to change that band's Gain, with modifier key to change Range); for each band a
  shaded region between its Gain line and Gain+Range; and an animated line showing the instantaneous total gain
  (band gains as a curve through the crossover shape).
- Per band a column: input-level meter with a draggable Threshold marker, value boxes for Threshold, Gain, Range,
  Attack, Release, and Solo / Bypass buttons.
- Master section: Threshold, Gain, Range, Attack, Release offsets applied to all bands (relative, preserving
  differences); Knee; Release mode (Auto/Manual); Behavior (Electro/Opto); Crossover Q; Output gain fader with
  output meters.
- All artwork, colours, fonts and names are the implementer's own.

## 9. Open questions
1. Exact peak-detector law (noise test vector in §6 is normative).
2. Exact attack/release laws (tables normative).
3. Auto law (only short-vs-sustained contrast measured).
4. Mono variant not measured (assumed identical per channel).
