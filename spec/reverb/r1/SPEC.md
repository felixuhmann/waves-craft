# Algorithmic reverb — behavioural specification r1

Status: APPROVED r1 (2026-11-01), frozen. Tables in `tables/` are normative and part of this specification.
Derived only from black-box measurement of a licensed reference (impulse renders at 48 kHz, subsets at 44.1/96 kHz),
host-visible metadata and public documentation. No code, disassembly or vendor assets. A reverb need not be
sample-identical: late-tail targets are statistical; level laws and filters are exact where stated.

## 1. Scope
Stereo in/out; a mono-in/stereo-out layout feeds the input to both channels (assumed, not measured). Zero latency.
At fixed settings the processor is LINEAR and TIME-INVARIANT (reference: repeated impulses identical to ≤ −129 dB,
level scaling ≤ −127 dB; no modulation). The build must also be: both ≤ −100 dB.

## 2. Parameters (r = host-normalised 0..1)
| Parameter | Mapping | Default |
|---|---|---|
| Predelay | −160 + 320 r ms | 0 |
| Time | 0.1·200^r s (0.1..20) | 2.40 s |
| Size | 1 + 99 r | 100 |
| Diffusion | 100 r | 0 |
| Decay | 0.04 + 3.46 r; r = 1 shows "Linear" | Linear |
| Early level, Tail level | −40 + 40 r dB; −40 = Off (silent) | −2, 0 dB |
| Wet/Dry | 100 r % | 100 |
| Output gain | −24 + 24 r dB | 0 |
| Damping low/high ratio | 0.10 + 1.90 r | 1.00 / 0.60 |
| Damping low freq; EQ low freq | 16·100^r Hz | 492; 156 Hz |
| Damping high freq; EQ high freq | 1000·21^r Hz | 2008; 5723 Hz |
| EQ low/high gain | −24 + 36 r dB | 0 / −10.7 dB |
| Decorrelation | 0..6 | 0 |
| Type | Hall 1, Hall 2, Room, Chamber, Church, Plate 1, Plate 2, Reverse, Gated, Non-Linear, Echo, Reso (r = k/11) | Hall 1 |
| Bypass | toggle; bit-exact | off |

## 3. Signal path
input → EQ (two shelves, §6) → { early generator (§4) × Early level ; tail engine (§5) × Tail level }.
wet = early + tail (exactly additive: reference residual −153 dB; build ≤ −100 dB).
out = (sqrt(1−m)·dry + sqrt(m)·wet) · Output gain, with m = Wet/Dry/100 (equal power; exact ±0.01 dB).
At m = 0 and Output 0 dB the output is bit-exact dry. No output limiter or clip was found at the tested levels
(open question 3).

Predelay p: for p ≥ 0 the tail is delayed by p ms; early and dry are unmoved. For p < 0 the dry path is delayed by |p| ms,
early is unmoved, and the tail moves 15.1 ms EARLIER regardless of |p| (measured at −20, −50, −160).
Defect NOT reproduced (D1): the reference's tail delay wraps modulo 8192 samples at large positive p
(+160 lands at 75.96 ms); the build delays the tail exactly p ms over 0..160.

Base settings for all tables unless the config says otherwise: Predelay 0, Time 2.40, Size 100, Diffusion 50, Decay Linear,
Early 0 dB, Tail 0 dB, Wet 100, Output 0, damping ratios 1.00/1.00 at 500/4000 Hz, EQ 0/0 dB at 200/4000 Hz, Decorrelation 0.
"Early only" = Tail Off; "Tail only" = Early Off. Impulse response = output for a unit impulse on one input.

## 4. Early reflections (per type)
- `er_envelope.csv`: per type and path (LL, LR, RL, RR): onset ms, total energy dB and energy per time bin. NORMATIVE:
  bins ≥ −60 dB within ±2 dB, total ±1 dB, onset ±1 ms.
- `er_taps.csv`: the 16 largest samples per type and path (time, signed gain). NORMATIVE for the first 8 taps by time
  of LL and LR: ±0.5 ms, ±2 dB, same sign. Later ones informative.
- Size scales early spacing: at Hall 1, see configs er_size* in §9 (informative: Size 25/50 energy −7.1/−7.0 dB L+R sum vs −7.8).
- Decorrelation changes L/R detail only: energy unchanged ±0.1 dB; `decorrelation.csv` gives energy and cross-correlation.
- Assumption: early reflections do not depend on Time, Decay or damping (not measured; §3 additivity holds).

## 5. Tail
Targets per `tail_envelope.csv` (per type/path) and `tail_params.csv` (per parameter variation, Hall 1):
onset ±1 ms, total ±1 dB, time bins ≥ −60 dB ±2 dB, octave T30 ±10 %, EDT ±20 %.
`band_balance.csv`: octave energy density relative to 1 kHz, for early and tail, each type: ±3 dB.
Type does not change T30: every type's tail decays at Time (±5 %); types differ in onset and build-up envelope (Reverse and Gated
ramp up over about 160 ms; see bins).
- Time: T30 ≈ Time for Time ≥ 1 s (±5 %). Below about 0.75 s the late slope stops following Time and stays near 0.72 s, while
  total energy falls: −8.8 / −10.6 / −12.4 / −17.7 dB at 0.5 / 0.29 / 0.2 / 0.1 s (tail_params rows).
- Size: onset ≈ 26.2 + 0.6045·Size ms (Hall 1 ±0.3 ms); T30 and energy unchanged.
- Diffusion: total LL energy −5.5 / −8.0 / −8.9 dB at 0 / 50 / 100; T30 unchanged; low Diffusion is sparse at onset, high is dense.
- Decay below Linear shortens the tail: T30 0.82 / 0.84 / 1.05 / 1.63 s and energy −34.8 / −26.8 / −17.2 / −11.1 dB at 0.21 / 0.55 / 1.59 / 2.81
  for Time 2.40 (rows tl_decay*, tl_decay1.59_t1 for Time 1.09).
- Damping: frequency-dependent RT multiplier. Below the low knee RT → low ratio × Time; above the high knee RT → high ratio × Time,
  with a gradual transition of about 2 octaves (rows tl_d*). Ratio < 1 shortens decay; the reference's manual wording "slower" does not
  match measurement (D2). Tail level is tested at Time/damping per rows.
- Tail stereo: LL and RR energies may differ per type as tabulated; late inter-channel correlation (max |xcorr| within ±1 ms, after 80 ms)
  ≤ 0.1 at Time ≥ 1 s.

## 6. EQ (shelves on the input to both early and tail)
Magnitudes in `eq_response.csv` are normative ±0.5 dB (ratio to base over 1/6-octave bands). Formula, in bilinear-warped frequency at the true sample rate (replace f and fc by tan(πf/fs) and tan(πfc/fs) throughout), G = 10^(g/20), knee fc:
- Low shelf, g ≥ −12 dB: |H|² = (f² + G fc²)/(f² + fc²/G) (half the dB gain at fc).
- High shelf: the same with f → fc²/f.
- Below −12 dB the shelf becomes a first-order cut with its knee still ≈ −6 dB: low |H|² = (f² + G²w²)/(f² + w²), w = √3·fc; high: mirrored.
  At −24 dB it is a pure first-order high-/low-pass (G treated as 0).
Boost and cut are mirror images for |g| ≤ 12.

## 7. Sample rate
All times are in ms/s and hold at any rate: at 44.1 and 96 kHz the reference's tail T30, onsets and envelope bins match 48 kHz within 0.1 dB in bins
and 0.5 % in T30. Extra energy above 20 kHz at 96 kHz is not specified.

## 8. Parameter changes
Not measured. Click-free changes within 50 ms are required; changing Type or Size may crossfade (D3).

## 9. Test configs (ids in the tables)
er_<Type>, tl_<Type> early-only/tail-only per type; tl_timeT; er_/tl_sizeZ; tl_diffD; tl_decayD (Time 2.4); tl_dhiR[_fF], tl_dloR[_fF] damping ratio R (knee F, else 4000/500);
er_eqlG / er_eqhG [_fF] EQ gain G (knee F, else 200/4000).

## 10. User interface (functional layout; visuals must be original)
Top: Type menu, Decorrelation selector, view tabs (All, Damping, Reverb, EQ). Display: left damping curve with 2 handles (frequency, ratio); centre time view with early-reflection
stems and the tail envelope shape; right EQ curve with 2 handles (frequency, gain). Bottom: faders Predelay, Time, Size, Diffusion, Decay; Early, Tail, Wet/Dry; Output with L/R peak meter and resettable peak readout.
Negative Predelay colours its handle as a warning (dry is being delayed). All artwork, colours, fonts and names are the implementer's own.

## 11. Decisions
D1 tail delay wrap not reproduced. D2 ratio semantics follow measurement. D3 parameter smoothing implementer's choice, click-free. D4 mono-in layout assumed.
D5 statistical tail targets: sample identity is not required, but linearity and time-invariance are.

## 12. Open questions
1. Early spacing vs Size per type (only Hall 1 measured). 2. Tail structure (informative: Jot FDN with per-band absorption fits §5). 3. Output clip at extreme levels (not measured: test inputs ≤ −6 dBFS).
4. Decay law between tested values (interpolate in table). 5. Time > 4 s (assumed T30 = Time). 6. Interaction of Size and Type on tail onset (Hall 1 law only; other types at Size 100 per table).

## 13. Public references
Schroeder 1962; Moorer 1979; Jot & Chaigne 1991 (FDN); Dattorro 1997; ISO 3382-1.
