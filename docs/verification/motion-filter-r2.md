# Independent verification — motion filter, spec r2 (final)

Performed 2026-10-29 by the verifier (analyst side), per step 5 of [docs/process.md](../process.md). Differences only.
Build under test: `feat/motion-filter` @ c2e6cb7 (drive re-check rendered against the installed c2e6cb7; filter,
crush, follower, chain and modulation renders against d3fccb6, which differs from c2e6cb7 only in the drive
saturator). Same host, stimuli and method as [motion-filter-r1.md](motion-filter-r1.md), plus steady 1 kHz / 9 kHz /
15 kHz sines for drive compression and aliasing at 48 and 44.1 kHz.

## Spec r2 items
| Area | Result | Numbers (build / reference) |
|---|---|---|
| BP peak gain = Q | pass | +3.03 / +2.93 dB (Res 50), +6.51 / +6.44 (Res 90), +7.60 / +7.60 (Res 100) at 1 kHz |
| Q from Res 90 to 100 | pass | Q 2.13 / 2.28 / 2.42 at Res 90 / 95 / 100 (r2: 2.12 / 2.26 / 2.40); level-independent −30 … −3 dBFS; all types within 0.2 dB of the r2 model (worst 0.41 dB, BP 5 kHz slope 24) |
| Crush | pass | hold length 5 / 17 / 37 / 65 samples at Crush 25 / 50 / 75 / 100 (r2: 5 / 17 / 37 / 65; reference identical); level preserved (+0.2 … +0.3 dB) |
| Follower | pass | +50 %: +1.74 / +1.38 / +0.78 / +0.42 / +0.28 oct vs +1.79 / +1.60 / +0.82 / +0.43 / +0.28 (−5.8 … −27.7 dBFS); −50 % and +100 % within 0.13 oct; Smooth 50 % within 0.2 oct |

## Build fixes
| Area | Result | Numbers |
|---|---|---|
| Output roll-off at 22 kHz | pass | −4.7 dB at 44.1 kHz and at 48 kHz (target −4.8); 96 kHz flat |
| Drive compression, steady 1 kHz sine at −3 dBFS peak | pass | 0.30 / 0.61 / 1.21 / 4.04 dB vs reference 0.31 / 0.61 / 1.17 / 4.18 dB at Drive 0 / 25 / 50 / 100 % |
| Drive compression, swept-sine method (as r1/r2 tables) | pass | 0.41 / 0.82 / 2.80 dB vs reference 0.42 / 0.80 / 2.71 dB at 25 / 50 / 100 % |
| Drive aliasing (Drive 100 %) | pass | worst alias product 30 … 40 dB below the reference's: 9 kHz tone at −3 dBFS −81 / −77 dBc (48 / 44.1 kHz) vs −52 / −42; 15 kHz at −6 dBFS −67 / −65 dBc vs −31 / −29 |

Drive level reference: the stimulus is correct (−3.0 dBFS peak, verified). The r1/r2 compression figures
(0.4 / 0.8 / 2.7 dB) were obtained with the swept-sine method, which reads ≈ 0.65× the compression of a steady sine
at the same peak level on both the reference and the build. The equivalent steady-sine targets are
0.3 / 0.6 / 1.2 / 4.2 dB; c2e6cb7 meets both. The earlier "middle" refit is superseded and should not be reverted.

## Regression (unchanged areas)
LP / HP / BR at Res 50 within 0.36 dB of the model, cutoff within 1 %; comb delay 958 samples (20.0 ms display) and
ratio 0.647 at 50 %; Amp −12 / Mix 0 gains within 0.1 dB of r1 results; LFO, synced LFO and sequencer cutoff swings
within 0.07 oct of the reference.

## Remaining differences (informative)
- Slope 24 at Res ≥ 90 with −3 dBFS input: the reference's resonant peak is 2.5 … 3 dB lower than at −30 dBFS
  (level-dependent limiting); the build stays level-independent as r2 specifies (r2 was derived from slope-12 data).
- LFO → Amp swing at small amounts remains approximate (see r2).

## Not verified
Mono variants, sidechain follower source, keyboard tracking, S&H statistics, swing details, analog-delay glide,
modulation of Res / comb delay / delay time, host automation smoothing, CLAP, Linux, GUI.
