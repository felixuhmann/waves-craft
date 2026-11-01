# Optical compressor/limiter — behavioural specification r1

Status: APPROVED r1 (2026-11-01), frozen. Tables in `tables/` are normative. Functional description only, derived
from black-box measurement of a licensed reference with user-generated signals and public documentation. No code,
disassembly or vendor assets. Product naming, IDs, and GUI artwork are the implementer's own.

## 1. Scope
Solid-state style optical compressor/limiter, stereo (one shared detector) and mono. Latency 0 samples.
Behaviour is defined in milliseconds and dB at all rates: 44.1 and 96 kHz measured static within 0.15 dB of 48 kHz and
release times within 8 % (one point at 44.1 kHz). Tables were measured at 48 kHz and apply at all rates.

## 2. Parameters (host value r in 0..1, all linear)
| Control | Mapping | Default | Resolution |
|---|---|---|---|
| Gain | 10 r | 4.08 (unity) | 0.01 |
| Peak Reduction (PR) | 10 r | 0 | 0.01 |
| Comp/Limit | toggle | Comp | |
| Hi Freq | 100 r (100 = flat) | 100 | 0.1 |
| Analog | Off / 50 Hz / 60 Hz | Off | |
| Mix | 100 r % | 100 % | 0.1 |
| Trim | −18 + 36 r dB | 0 dB | 0.1 |
| Auto Makeup | toggle, see §8 | Off | |
| VU display | In / GR / Out, display only | GR | |
Also a host bypass (click-free).

## 3. Signal path
`input → gain cell (GR from detector) → fixed colouration (§6) → Gain → Mix with dry → Trim → output`.
- Gain law: `g_dB = 5.0·(Gain − 4.08) + 0.015`. Exact at all measured settings (0, 2, 6, 8, 10): −20.385, −10.385, +9.615, +19.615, +29.615 dB.
- No output clipping at any measured level (+29.66 dB gain with 0 dBFS input, measured unclipped).
- Mix: linear in-phase crossfade `y = m·wet + (1−m)·dry`. At Mix 0 output equals input exactly. Measured within 0.003 dB.
- Trim: exact dB gain after Mix.

## 4. Static compression
`tables/static_gr.csv`: steady 1 kHz sine, both channels, Hi Freq 100, Gain unity; `gain_dB` is the steady reduction
at each level, at PR 0, 2, 3, 4, 5, 6, 7, 8, 10 for modes C and L. Sine peak level, dBFS. Interpolate linearly in level,
then linearly in dB between adjacent PR rows. Examples: PR 8 Comp at −24/−12/0 dBFS gives −7.64/−16.65/−25.56 dB.
PR 8 Limit gives −8.20/−19.05/−30.70 dB. PR 10 Limit saturates near −32 dB at 0 dBFS.
Comp is about 4:1 above a soft knee, Limit about 16:1 at moderate reduction. PR 0 and 2 give no reduction.

## 5. Detector
- Stereo link: one detector; a tone on one channel gets the same reduction as the same tone on both. Detector input
  behaves as max(|L|, |R|). The other channel's signal is scaled by the same gain, and no crosstalk is added.
- Frequency dependence: `tables/hifreq_gain.csv` (PR 6, Comp) is normative for 100 Hz to 8 kHz and Hi Freq 0, 25, 50,
  75, 100. Lower Hi Freq raises sensitivity above about 1 kHz: at 8 kHz, 0 dBFS, Hi Freq 0 gives −24.46 dB, Hi Freq 100 −17.86 dB.
  Below 300 Hz Hi Freq has no effect. For other PR values apply the equivalent level offset W(f, Hi Freq) in the detector.
  W (dB, informative fit) for Hi Freq 0/25/50/75/100: 100 Hz −0.85 all; 300 Hz −0.7 all; 1 kHz +0.65/+0.2/+0.05/+0.05/0;
  3 kHz +4.65/+2.3/+1.3/+0.9/+0.85; 8 kHz +7.95/+5.25/+1.8/+0.4/+0.1.

## 6. Colouration
- Low-level response `tables/freq_response.csv` (independent of mode and PR): −0.28 dB at 30 Hz, 0 at 1 kHz, +0.47 dB at 10 kHz, +1.42 dB at 20 kHz, ±0.2 dB. Above 18 kHz at 96 kHz the response may continue to rise (+1.93 dB at 20 kHz measured there); ±0.6 dB allowed.
- Harmonics `tables/harmonics.csv` (steady tones; gain and H2–H5). Odd-dominated. Without reduction H3 at 0 dBFS is −55 dBc at 100 Hz, −82 at 1 kHz, −76 at 5 kHz. Under reduction H3 rises strongly at low frequency, e.g. PR 8 Limit at 0 dBFS, 100 Hz: −28 dBc; 1 kHz: −47 dBc. This is gain-cell following of the waveform and follows from fast detector behaviour.

## 7. Dynamics
Tables are normative: `attack.csv` (reduction at t ms after a burst starts), `release.csv` (reduction at t ms after it ends),
`release_times.csv` (end reduction, t63, t90). Stimulus: 1 kHz probe at −40 dBFS continuous, with a burst of the stated level and
length replacing it after at least 2 s; reduction is short-time gain relative to before the burst (4 ms analysis window).
Settings C5, C8, L8; bursts 5/50/500/3000 ms at −6 dBFS, 500 ms at −18 and 0 dBFS.
Character: attack about 5 ms to half of the final reduction at higher settings; release is two-stage, with about 63 % recovered in 0.1–0.18 s, then a tail
of several seconds. Tail grows with burst length and depth: after PR 8 Comp, −6 dBFS, GR at +1 s is −0.24 dB after 5 ms and −1.75 dB after 3 s. GR −0.09 dB remains at +15 s.
Informative starting model (fit 1.5 dB RMS, not normative): fast state tracks target with attack τ 18 ms and release τ 75 ms;
slow state charges toward the fast state with τ 15.5 s and relaxes toward target with τ 2.5 s; reduction is the deeper of the two.

## 8. Analog and Auto Makeup
- Analog Off: no noise (silence stays below −170 dBFS). Analog 50/60: `tables/noise.csv`, noise −87.7 dBFS rms at unity Gain, uncorrelated between channels; hum line −95.6 dBFS at mains frequency, second harmonic about −105 dBFS, third about −105 dBFS at 50 Hz. Gain 10: noise −82.7 dBFS, hum unchanged.
- Auto Makeup: host-automated PR does not move Gain and audio is unchanged. Editor-only linkage: when enabled, moving PR in the editor moves Gain by `(G(PR_old) − G(PR_new))/5` units, where G is the Comp reduction at −18 dBFS from §4. Gain changes do not move PR. Host-automated Gain is unaffected.

## 9. Review decisions
- D1: measured behaviour reproduced, tables override the fitted model.
- D2: parameter changes are click-free within 20 ms; the reference's late parameter application is not reproduced.
- D3: Auto Makeup editor rule above is our definition; the reference's editor curve was not measurable via host automation.
- D4: Mono assumed the same on one channel, not measured.
- D5: defaults are neutral (Gain 4.08, PR 0, Hi Freq 100, Analog Off), so a fresh instance is unity. The reference's first-load values differ.

## 10. Tolerances
Static at table points ±0.3 dB; between PR rows ±1.0 dB; Hi Freq table ±0.5 dB (±1.5 dB at 8 kHz Hi Freq 0). Gain, Trim, Mix ±0.02 dB. Attack and release table points ±1.5 dB, ±3 dB within 5 ms of a level change; t63, t90 ±25 %. Frequency response ±0.2 dB. Harmonics ±6 dB where above −90 dBc. Noise and hum ±3 dB. Latency exact. Output finite; never latches silent.

## 11. UI (functional; artwork must be original, do not imitate any hardware or vendor look)
Two large knobs left and right (Gain; Peak Reduction, 0–10) flanking one level meter with In/GR/Out selector. Comp/Limit switch and Auto Makeup button beside Peak Reduction. Lower strip: Analog Off/50/60, Hi Freq knob marked Flat at 100, Mix knob, Trim knob.

## 12. Open questions
Exact gain-cell and release law; release for program between measured cases; PR rows 1 and 9 (interpolated); Hi Freq at other PR values (W model); Mono; rates above 96 kHz.

## 13. Public references
Giannoulis, Massberg, Reiss, "Digital Dynamic Range Compressor Design", JAES 60(6), 2012. Zölzer (ed.), "DAFX". Published descriptions of electro-luminescent/photo-resistor gain cells.
