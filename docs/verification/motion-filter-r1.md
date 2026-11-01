# Independent verification — motion filter (spec r1, with r2 corrections)

Performed 2026-10-29 by the verifier (analyst side), per step 5 of [docs/process.md](../process.md).
Only observed differences are reported; no reference-side implementation detail is passed to the builder.

## Method
- Host: REAPER 7.83 x64 (Windows), unattended offline renders, 32-bit float; 48 kHz unless stated, spot checks at
  44.1 and 96 kHz; project tempo 120 BPM.
- Build under test: `feat/motion-filter` @ 640ae17, Release VST3 "Craft Motion Filter".
- The build and the reference were driven with identical stimuli (exponential sweeps L-only/R-only at −30/−12/−3
  dBFS; a −12 dBFS 1 kHz tone; a −20 dBFS 200 Hz probe tone for cutoff tracking; level-stepped 1 kHz bursts for the
  follower) and identical settings. Build controls were set by their displayed values (continuous knobs) or by
  choice index (selectors/toggles/amount sliders).
- ≈ 270 build renders and 23 new reference renders (resonance 95/100 and band-pass gain series).

## Results against spec r1
| Area | Result | Numbers |
|---|---|---|
| LP/HP/BR 12/24 dB, Res 0–90, 100 Hz–5 kHz | pass | magnitude ≤ 0.4 dB from the §5 model; fitted cutoff within 1 % of the knob (incl. 5 kHz, review decision 1); Q within 3 % |
| BP magnitude | pass vs r1 formula, **fails vs reference** | build peak gain 0 dB at every Res; reference peak gain = Q (+2.9 dB at Res 50, +6.4 dB at 90). r1 was self-contradictory → corrected in r2 |
| Res 95/100 | **fails vs reference** (r1 numbers wrong) | build Q 4.2 (−30 dBFS) → 2.6 (−3 dBFS); reference Q 2.2 (Res 95) / 2.3–2.6 (Res 100), level-independent → r2 |
| Fixed chain (§4) 48 kHz | pass | −3.3 dB @20 Hz, +0.30 dB @1 kHz, −0.8/−1.8/−2.9/−5.8 dB @15/18/20/22 kHz |
| Fixed chain 44.1 kHz | marginal | 22 kHz −10.3 dB vs −4.8 dB target (deviation −5.5 dB, tolerance −5 dB); ≤ 20 kHz pass |
| Fixed chain 96 kHz | pass | flat +0.30 dB to 40 kHz (review decision 5) |
| Latency | pass | 37 samples reported to and compensated by the host; output aligned (group delay 0.3 samples, same as reference) |
| Comb | pass | delay = displayed value (within the 0.1 ms display step; review decision 2); feedback ratio 0.499/0.643/0.774 vs 0.500/0.645/0.776 |
| Amp, Mix, Input/Output, Spread | pass | gains within 0.3 dB of reference; −80 dB mutes; spread cutoffs within 1.5 % |
| Drive small-signal gain | pass | +1.85/+3.68/+7.32 dB at 25/50/100 % (spec +1.9/+3.7/+7.2) |
| Drive compression at −3 dBFS | **fail at 100 %** | 0.26/0.52/1.82 dB at 25/50/100 % (wide-open filter) vs 0.4/0.8/2.7 ±0.5 dB; with LP 1 kHz 0.58/0.95/2.39 dB (pass) |
| Crush | **fails vs reference** (r1 law wrong) | build follows r1 (70 Hz at 50, near-silent at 75–100); reference hold length 1 + 64·(Crush/100)² samples at 48 kHz, level preserved → r2 |
| Delay | pass | free time = displayed (within 1 ms display step; review decision 3); 1/8 sync = 250.0 ms; ×1.5/×2 right; feedback ratio 0.495 @50 %, sustain @100 %; dry/wet table within 0.003 after normalisation |
| LFO | pass | rates exact (0.1–16 Hz, free and synced); depth ±0.39/±1.33 oct at 25/50 % (reference identical within 0.01 oct); sine/triangle shape residual ≤ 0.07 oct, saw 0.22, pulse 0.34 (edge timing) |
| Sequencer | pass | step timing exact; Last-step, sync, swing comparable; starts on step 1 (one-step offset to the reference = review decision 4); residual ≤ 0.03 oct |
| Smooth | pass | residual vs reference 0.10–0.18 oct on pulse LFO with Smooth 25/50/100 % |
| Seq → Amp offsets | pass | within 0.3 dB of reference for v = 12.5…100 %; negative offsets mute in both |
| LFO → Amp swing | informative | 25 %: 11.2 dB p-p (r1 formula) vs 14.7 dB reference — r1/r2 law is approximate |
| Follower | **fails vs reference** (r1 law wrong) | per-channel ✓, but offsets ≈ 3.6× too large (e.g. −50 %, −17 dBFS: −4.3 oct vs −0.83 oct) → r2 table |

## Spec revision
[r2](../../spec/motion-filter/r2/SPEC.md) corrects §5 (BP gain = Q; Q 2.12 → 2.40 from Res 90 to 100, level-
independent), §6.3 (follower target table), §9 (crush hold-length law). Remaining build work is listed below.

## Issues for the builder
1. BP: implement s/D (peak gain = Q) per r2 §5.
2. Res > 90: linear Q 2.12 → 2.40; remove level-dependent resonance limiting (r2 §5).
3. Crush: hold length N = 1 + 64·(Crush/100)² samples at 48 kHz, level preserved (r2 §9).
4. Follower: meet the r2 §6.3 table (≈ 6.4·A oct at ±50 %, saturating ≈ +1.8 oct).
5. Drive 100 %: compression at −3 dBFS 1.8 dB vs 2.7 ± 0.5 dB (wide-open filter).
6. 44.1 kHz output roll-off at 22 kHz −10.3 dB vs −4.8 dB (tolerance −5 dB) — marginal.

## Not verified
Mono variants, sidechain follower source, keyboard tracking, LFO S&H statistics, swing law details, analog-delay
glide, modulation of Res/comb delay/delay time, follower → Res/Amp, host automation smoothing, CLAP, Linux, GUI.
Note: several REAPER runs ended early with an access violation inside a vendor component loaded by the host (not
the build); affected configurations were re-rendered.
