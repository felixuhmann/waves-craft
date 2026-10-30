# Independent verification — vocal compressor (spec r1 → r2)

Performed 2026-10-29 by the verifier (analyst side), per step 5 of [docs/process.md](../process.md). Only observed
differences are reported here; no reference-side implementation detail is passed to the builder.

## Method
- Host: REAPER 7.83 (x64, Windows), unattended offline render, 32-bit float; 44.1, 48 and 96 kHz.
- Build under test: `feat/vocal-compressor` @ bf82e85 (Windows Release VST3).
- Stimuli (user-generated): 1 kHz staircases (2 dB steps), level steps, gate drops, single-sample clicks, tones at
  50 Hz–12 kHz, linked L/R, noise, 0 dBFS bursts of 2 ms–1 s inside a −40 dBFS tone, low-frequency tones, plus new
  cases: gate drops at higher levels (0→−50/−30, −10→−60, −20→−70 dBFS) and extra release patterns (steps to −20/−30/−60
  dBFS after 50 ms–2 s, double bursts, a burst train). Same settings (host-normalised values) on build and reference.

## Results against spec r1
| Area | Result | Numbers |
|---|---|---|
| Neutral (×0.99, no limiting) | pass | max \|y − 0.99·x\| = 3e-8 |
| Latency / time alignment | pass | build output time-aligned with the reference (neutral and Gain configs ≤ −150 dB re peak sample-wise) |
| Static curve (§9.1), Compression −3…−36, Gain −6…−36, combined | pass | worst 0.003 dB (tolerance 0.05) |
| Look-ahead attack ramp (§9.2) at 44.1/48/96 kHz | pass | build = reference = §4.3/§4.5 chain within 0.001 dB at every measurable sample |
| Gate static (§9.5), thresholds −78…0 | pass | worst 0.005 dB |
| Gate + compressor combined (§9.5) | pass | −2.06 / +7.82 dB vs −2.1 / +7.8 |
| Release after sustained levels (§9.3) | pass | worst 0.69 of tolerance |
| Release after bursts (§9.4), 48 kHz | pass | worst 0.80 of tolerance |
| Release at 44.1/96 kHz vs reference | pass (by r1's rule) | burst-table points within 0.4–3.7 dB of the reference |
| Gate closing (§9.6) | pass (r1 ±1.5 dB) | worst 1.2 dB from the table |
| 50 Hz ripple ≤ 0.2 dB (§4.4.4) | **fail** | build 0.36 dB (Compression −18) and 1.36 dB (−36) at 50 Hz |

## Builder questions (adjudicated)
- **A2 (gate closing shape):** the rounding near the floor is real, not a −120 dBFS measurement artefact. The reference
  closes along one curve that depends only on the distance to the new level (identical within 1 ms for 30–50 dB drops at
  thresholds 0/−20/−40 dB and output levels down to −60 dBFS). The constant 63 dB/s law of r1 was wrong (slope ≈ 66 dB/s far
  from the level, ≈ 22 dB/s just above it). r2 §4.2 gives the law; implemented literally it meets every §9.6/§9.6.1
  point within 0.41 dB.
- **A3 (attack points n = −1, 0 at Compression −36):** r1's values were wrong (measured near zero crossings). Correct
  values (chain = reference within 0.001 dB where measurable): Compression −36: n = −1 0.65 dB, n = 0 0.25 dB;
  Compression −18: 0.30 / 0.18 dB. r2 §9.2 restates them.

## Spec r2 (approved, `spec/vocal-compressor/r2/SPEC.md`)
See its "Changes from r1": gate release law S(D) and new §9.6.1 table (tolerance ±0.75 dB), ripple tolerance relative to
measured values (§9.8; the reference itself shows 0.69 dB at Compression −6 / 50 Hz), new normative release table §9.7
(21 cases), corrected §9.2 points.

## Differences of the build against r2 (to fix)
1. **Release on patterns outside the r1 tables (§9.7):** 9 of 21 cases out of tolerance. The build releases
   almost immediately after short loud passages regardless of the level returned to; the reference holds the reduction
   when the drop is moderate. Worst: Compression −36, 50 ms at 0 dBFS then −20 dBFS: remaining reduction at 2 ms 0.98 dB
   (reference 16.95), at 20 ms 0.51 (9.38), at 50 ms 0.17 (5.48). Double bursts / burst trains: build 1.3–2.7 dB at 2 ms
   (reference 7.8). Step −6→−30 after 300 ms: 15.55 vs 20.16 dB at 2 ms (Compression −36).
2. **Gate closing (§4.2/§9.6/§9.6.1, ±0.75 dB):** build follows the r1 constant-rate law with a soft corner; up to
   1.6 dB from the reference (e.g. −25.2 vs −26.43 dB at 400 ms, −48.3 vs −47.51 at 800 ms). Implement r2's S(D).
3. **Low-frequency ripple (§9.8):** 6 of 9 points above reference + 0.15 dB: Compression −18 at 100/200 Hz
   (0.36/0.18 vs 0.04/0.00), Compression −36 at 50/100/200 Hz (1.36/1.17/0.46 vs 0.11/0.03/0.00). At strong compression
   the reference holds the gain over a cycle; the build lets it follow the waveform.

## Not verified
Linux and CLAP builds; parameter automation/smoothing in a host; 88.2/176.4/192 kHz; mono layout; GUI.
