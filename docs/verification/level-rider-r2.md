# Verification — Craft Level Rider against spec r2

Verifier, 2026-11-01. Build `feat/level-rider` @ 0d385d9 (installed). Host: REAPER offline, 32-bit float; same stimuli and
settings as the reference. Differences only. A second noise seed (same schedules) on both plugins separates real
misses from noise sensitivity.

## Verdict: do not land yet — short fix list below
The r2 work fixed T4 and most of T2, but it made speech-like material worse, which is the plugin's main use.

## Results (48 kHz unless stated)
| Check | Result |
|---|---|
| T4 idle (±1.0 dB) | **Pass**, worst 0.68 dB (was 6.47 dB) |
| T2 (±0.6 dB) | 7 cells miss (was 11): −3→−33 at ±3 −1.49 vs −0.6; Target −12 +3.34 vs +2.1; Target −33 −11.84 vs −9.9; 4 Fast cells 0.65–0.8 dB |
| T3 | 14 cells miss (see fixes) |
| T7 | 12 of 168 cells miss; 4 are Idle↔Ride timing cells |
| Speech-like vs reference (rms; was) | Slow ±12 **2.05** (0.89), Fast ±12 **1.74** (1.12), default ±6 1.59 (2.34); mean error −1.4…−1.8 dB (seed 2) |
| 44.1 / 96 kHz | Fast −21→−33 vs reference: 0.69 dB at 44.1, 5.8 dB at 48, 2.45 dB at 96. Slow −33→−39: 4.39 / 6.15 / 5.13 dB |

T1, T5, T6, Live/Mono, sidechain, automation and the GUI were not re-checked in this pass.

## Noise sensitivity
Reference between seeds: T2 ≤ 0.44 dB, speech 0.09–0.18 dB rms. Fix items 1, 3, 4 and Fast −21→−27 miss on both seeds, so they are real.
T7 and Slow −21→−33 were checked on one seed only. The build's own between-seed spread is up to 1.94 dB (Fast −15→−21) against 0.32 dB for the reference.

## Questions settled
- **Real misses:** the T3 rows below are real; T7 −9→−27 was seen on one seed. The T2 −3→−33 misses at ±3 and Target −12/−33 are real but small; see exceptions.
- **96 kHz:** not a 96 kHz problem. The large misses also occur at 48 kHz, and the rate dependence is in the build, not the reference (reference vs 48 kHz table ≤ 1.35 dB). It is worth fixing via one rate-independent constant set (item 2).
- **Speech:** not acceptable; it regressed in two of three configurations.
- **T4 stimulus:** the builder's assumption (12 s bursts, 6 s gaps) was wrong. The actual stimulus (6 s bursts, 10 s gaps) is now stated in r3. The build passes T4 on it.

## Spec r3 (with this report)
`spec/level-rider/r3/`: normative speech target T8 with fixtures (rms ≤ 1.0 dB, |mean| ≤ 0.75 dB; the §6 structure meets it at 0.72–0.84 dB), T4 stimulus stated, ±1 s allowance on T7 transition cells, priority of T8 over T3/T7 cells (D8).

## Fix list
1. Meet T8 in all three configurations (build reads about 1.4–1.8 dB low on speech). Start by re-checking the r2 program-dependent release, which introduced this.
2. Use one rate-independent set of time constants (the per-rate refit is the likely cause of the rate spread), and make the release decision less sensitive to noise.
3. Fast −21→−33 rises 1–4 s early (+3.47 vs −2.3 at 3 s). Fast −39→−21 attacks too fast at 0.03 s (−2.79 vs +0.3).
4. Slow −33→−39 rises too late (1.16 vs 5.0 at 3 s; 4.28 vs 10.4 at 4 s). Fast −33→−39 misses by 1.6–3.2 dB.
5. Slow T7 −9→−27 rises 3 s early (−0.16 vs −6.23 at 6 s). Fast −21→−27 at 1–1.5 s misses by about 1.7 dB; Slow −21→−33 at 3 s by 2.0 dB.

## Acceptable as documented exceptions
Remaining T2 cells within 0.8 dB, T2 −3→−33 at ±3 and Target −12/−33 (up to 1.9 dB; the only 30 dB drop), and any T3/T7 cell that conflicts with T8 (D8), each listed in `IMPLEMENTATION_NOTES.md`.
