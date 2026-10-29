# Verification — Craft Level Rider / Level Rider Live against spec r1 (and r2)

Verifier (analyst side), 2026-10-29. Build: `feat/level-rider` (installed VST3). Reference: the measured reference
processor (stereo variant). Host: REAPER 7.83, offline render, 32-bit float, 48 kHz (44.1/96 kHz checks), same
stimuli and identical parameter settings on build and reference. Differences only; no reference internals.

## Summary
The build meets the static law, the gate and the detector weighting, but **fails the dynamics after large level
drops and after Idle** (T2 column "−3 → −33", several T3 rows, and T4). The cause is the build's assumption that
the level detector is frozen while Idle; the reference does not behave that way. Spec r2 (pushed with this report)
clarifies §7.2 and adds a normative table T7. The build must be updated to r2.

The reference renders are exactly repeatable (re-rendered `hold` stimulus bit-identical), so none of the misses
below can be attributed to reference run-to-run variability.

## Results against spec r1 (host renders)
| Table | Result |
|---|---|
| T1 static law (±0.25 dB) | **Pass** — all 5 configurations, worst 0.19 dB (build vs reference ≤ 0.19 dB) |
| T5 activity gate (state) | **Pass** — no non-adjacent state mismatch for Vocal Sensitivity −6/0/+6/+12 and Fast |
| T6 weighting (±1.0 dB) | **Pass** — worst 0.21 dB (8 kHz −3.61 vs −3.4) |
| T2 equilibria (±0.6 dB) | **Fail** — 11 cells: column 14 ("−3 → −33") in 6 of 7 configurations (e.g. ±12 −3.53 vs −4.6; ±12 Target −12 +4.27 vs +2.1; ±12 Fast −9.70 vs −6.9), plus 4 more ±12 Fast/±6 Fast cells at 0.7–0.8 dB |
| T3 trajectories (±1.5 dB / ±30 % time) | **Fail** — 27 cells: Slow −33→−39 (7 cells, up to 4.0 dB), Fast −33→−39 (8 cells, −9.4 vs −6.7 at 0.01–0.5 s), upward rides after 6 dB drops at 1–2 s (−15→−21, −21→−27, both modes, 2.0–2.6 dB too slow), Fast −9→−21 at 4 s, Fast −21→−33 |
| T4 idle (±1.0 dB) | **Fail** — burst −33 dBFS rows: Slow 1.83 dB (build reaches only +4.4/+4.9 dB where the reference reaches +6.7), Fast 6.47 dB (build −2.1 at 0.1 s vs +4.4); burst −21 Slow 0.75 dB (pass) |
| Sample rates | Build at 44.1/96 kHz differs from build at 48 kHz by up to 1.35 dB on T2 (the same large-drop cells) |

Mono/Live variants, Spill, the music sidechain, automation Write/Read and the GUI were not verified in the host.

## Large drops and speech-like material (build vs reference)
Drops of 6–36 dB after 12 s at the upper level (new table T7 in r2):
- Idle entry and re-acquisition times match the reference within about 1 s.
- On re-acquisition the build dips to the bottom of the range (−12 dB) and recovers late; the reference's dip is
  shallower (about −8, −5.7, −3.8 dB for 24, 30, 36 dB drops, Slow) because its detector restarts from the decayed
  recent-vocal level. Example (Slow, −3→−39): at 12 s build −12.0 vs −3.8, at 15 s −8.3 vs −1.4, at 19.5 s
  +7.3 vs +11.9 dB.
- Speech-like bursts with breath gaps: build vs reference rms 0.89 dB (Slow ±12), 1.12 dB (Fast ±12),
  2.34 dB (default ±6), p95 1.9 / 2.3 / 3.3 dB.

## Adjudication of the builder's questions
1. **27 out-of-tolerance cells**: genuine non-conformance (host renders show 38 incl. T4); not reference variability.
2. **Assumption "detector frozen while Idle"**: refuted — causes the T4 failure (stale loud level after a −9 dBFS
   burst drives the −33 dBFS burst downward in Fast) and the T2/T3/T7 large-drop misses.
3. **Analytic-power detector** and **weighting fit**: consistent with the reference (T1/T6 pass).
4. **Spill gate, sidechain re-seeding**: not measurable against the reference (not characterised); not verified.

## Robustness
One REAPER offline render of the `hold` stimulus stalled once (second pass of Attack Fast/±12, after the identical
first pass completed). Not reproduced: a full re-render and 4 extra back-to-back renders of that setting each completed
in about 3 s. Recorded only for awareness.

## Spec r2 (pushed with this report)
`spec/level-rider/r2/SPEC.md` (SHA-256 0decea38127c4b10eac672fa34074da812067b447927a8333882fc4f8d172d78):
§7.2 detector-after-Idle behaviour, new normative T7, D7. A model implementing the informative §7.2 rules halves the
error against the reference on large drops (4.4 → 2.0 dB rms) but does not meet every T3/T7 cell by itself — the
tables govern.

## Issues for the builder (against r2)
1. Do not freeze the detector while Idle. After a relative-threshold Idle, resume riding when the level is back
   within ≈18–21 dB of the decaying recent-vocal level, restarting the detector from that decayed level; after an
   absolute-threshold Idle (silence) restart from the current input. Meet T7, T2 column 14 and T4.
2. Upward rides after 6 dB drops are 2.0–2.6 dB too slow at 1–2 s (T3 −15→−21, −21→−27, both modes).
3. ±12 Fast/±6 Fast T2 cells at −15/−21/−39 dBFS (0.7–0.8 dB) and Fast −33→−39 initial value (−9.4 vs −6.7).
