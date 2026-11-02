# Independent verification: Craft FET Limiter against spec r1 (and r2)

Build: `feat/fet-limiter` @ d142a1a, Release VST3. Host: REAPER, offline, 32-bit float. The build and the reference got identical
stimuli and normalised parameter values. Coverage is 40 static, 21 dynamics, 12 colour and 8 fast-burst settings at 48 kHz, plus
4 extra settings at 48 kHz and 2 static settings each at 44.1 and 96 kHz. Only differences are reported.

**Verdict: not ready to merge.** Static gain staging passes; fast dynamics and harmonics do not. Spec r2 corrects r1 where r1 was wrong.

## Results
| Area | Result | Numbers (build vs reference) |
|---|---|---|
| Static curve, ratios 4/8/12/20, rev A | Pass | Max 0.29 dB (limit 1.0); 44.1/96 kHz ratio 4: 0.17 dB |
| Static curve, All, rev A | Marginal | 2.55–2.63 dB on the 0.4 s staircase, also at 44.1/96 kHz (r1 limit 2.5, r2 limit 2.7) |
| Static curve, rev B | **Fail at ratio 8** | 2.34 dB at Input −12, 0 to −2 dBFS: reduction exceeds the −31.9 dB maximum |
| Output gain, trim, mix | Pass | Max 0.22 dB; Mix 0 % identical |
| Frequency response, rev A and B | Pass | Max 0.28 dB, 20 Hz–20 kHz |
| Small-signal gain, All +1.2 / +0.6 dB | Pass | 38.29 vs 38.28 dB; 41.18 vs 41.24 dB |
| Analog noise | Pass | Rev A −70.9 vs −71.8 dBFS; rev B −76.1 dBFS |
| Trajectories, ratios 4/8/20 rev A and rev B, holds ≥ 50 ms, Release ≥ 3 | Mostly pass | Most within ±1.5 dB; up to 2.1 dB at Release 7 |
| Trajectories, All, Attack 7 / Release 7 | **Fail** | 2.8–4.3 dB, holds ≥ 50 ms |
| Harmonics (r1 §7.2) | **Fail** | 96 of 216 H2/H3 cells out of tolerance |
| Fast bursts (r2 table, first burst excluded) | **Fail** | 174 of 264 checks out of tolerance |

## Adjudication of the builder's questions
1. **Fast-burst misses.** Real, and larger than the 55 reported. The reference holds about 7 / 29 / 41 ms at (7,7) / (5,5) / (3,4) in
   All before recovering; the build holds 0.2 ms there at −10 dBFS, and 14–97 ms at −22 dBFS, where the reference holds 4–18 ms.
   Part of r1 was wrong: its All 5 ms rows were the first burst of a stream (−27.2 dB; all later bursts −23.8 dB, for any idle of
   0.5–12 s). The build reads −29.5 dB. Spec r2 replaces those rows with `fast_bursts.csv`.
2. **`E` vs staircase rows in All.** The builder's reading is right: `E` rows take precedence. The gap is a slow creep in the reference, now in r2 item 4.
3. **Release 1–2 absolute levels.** The builder is right: those rows were contaminated by unfinished recovery. Compare by shape (r2 item 6).
4. **All gain.** Rev B +0.6 dB is correct. **Comp Off removing the All gain is wrong:** the reference keeps it (+1.20 dB, rev A).
5. **Ratio 12 interpolation.** Acceptable except at the fastest setting: Attack 7 / Release 7 after a 5 ms burst reads −24.6 vs −21.4 dB.
6. **Comp Off low-level distortion.** The frequency dependence is real: rev A −30 dBFS, 100 Hz H2 is −42.6 vs −53.7 dBc. Those rows must be asserted.
7. **Soft ceiling above +30 dBFS.** Allowed, but the Comp Off level is wrong: with a 0 dBFS input the build outputs +36.0 / +34.9 dBFS where the
   reference saturates at +31.0 / +31.7 dBFS.

## Issues for the builder (against r2)
1. Implement the hold and meet `tables/fast_bursts.csv`, including All at Input −24 (−17 vs −12 dB) and rev B All (−36 vs −26 dB).
2. All at Attack 5 / Release 5: recovery t63 is about 2.5× too slow (706 vs 283 ms).
3. All at Attack 7 / Release 7: remove the deep first-burst fit and meet the creep values in r2 item 4 (build −26.1 dB at 2 s vs −24.4 dB).
4. Rev B ratio 8: cap reduction per `static_gr.csv` at high level.
5. Harmonics: meet every `harmonics.csv` row. Compressed H3 is much too low (1 kHz −18 dBFS: −81.6 vs −64.5 dBc; rev B about −100 vs −62 dBc), and 100 Hz H2 is about 10 dB high in rev A.
6. Comp Off: keep the All gain and match the output ceiling (r2 items 7, 8).

## Not verified
Linux and CLAP builds, automation in a host, Auto Makeup in the editor, GUI, mono, 88.2 kHz and above, and dynamics at 44.1/96 kHz (static only).
r2 is table-normative: its new tables are reference measurements, so there was no law to check by literal implementation.
