# Independent verification — Craft Dynamics family, r2 build

Verifier side, step 5 of [docs/process.md](../process.md). Differences only; no reference implementation details.
Build: `feat/dynamics` @ 4195d72 (Release VST3, Windows). Same host, method and stimuli as
[dynamics-r1.md](dynamics-r1.md), plus a new full-scale onset test (silence → 0 / −20 dBFS 1 kHz and −6 dBFS 100 Hz).

## Summary
Every r2 acceptance table now passes. Two r2 text gaps and one reference behaviour that r1/r2 had mis-described were
found and are fixed in **spec r3** (`spec/dynamics/r3/SPEC.md`): the build needs one more update (items 1–3 below).

| Area (spec r2) | Result | Numbers |
|---|---|---|
| §10.5.1 compressor trajectories | pass | 460/460 cells; tight cell Att 0.1 ms, edge A +1 ms: +2.94 dB (±3.5) |
| §10.5 timing (t63/t90) | pass | 22/22 |
| §10.5.3 expander trajectory | pass | worst 0.142 (±0.15) |
| Gate envelopes | pass | 0.007–0.012 linear RMS; Release 1000 ms: 0.036 once the start-of-render state is excluded (see item 2) |
| §10.8 detector type | pass | noise +0.91/+0.71, 100 Hz −0.95/−0.71, 10 kHz +1.45/+1.13 dB (targets ±0.4) |
| §10.9 PeakRef near full scale | pass | worst 0.21 dB (±0.3) |
| Band-pass split / passive / active polarity | pass | phase difference 0.0° |
| Sidechain monitor rule | pass | wideband when only the gate is filtered |
| Ratio display at r = 0.8 | pass | "50.00:1" |
| Static regression (ratios, makeup, lookahead, expander statics, gate thresholds) | pass | ≤ 0.06 dB (1.00 … 50:1), cancellation 0.21 dB; gate thresholds one 0.25 dB step low (±0.3) |
| Negative-floor notch at −60 dBFS | pass (r3 rule) | 2.35 dB at the notch bottom only (−49.5 vs −51.9 dB), ≤ 0.36 dB at ±2 dB around it; the build's floor control resolves "−21 N" where the reference shows "−20.6 N" |

## Adjudication of the builder's additions beyond r2
- **(a) Expander calibration offset:** correct diagnosis. r2's expander text was missing a constant; a literal r2
  expander reads 1.11 dB low against the reference statics. r3 adds EXP_CAL = +1.11 dB and fixes Appendix B
  extrapolation below its first row (slope 1, x − 2.04 dB). A literal r3 expander matches the statics within 0.01 dB
  at GateOpen −40 and −20. Replace the per-setting calibration with the r3 constant.
- **(b) Output-limit onset safety (input peak held over 340 samples):** not needed. The reference has no soft output
  limiter at all: it hard-clips every output sample to ±1.0 (flat-topped waveform at exactly 1.0 with +20 dB Makeup or
  0.50:1 expansion at 0 dBFS). r1/r2's "≈ +2.8 dBFS expansion limit" and §10.9's flattening are that clip seen through
  RMS-based measurements. The build currently lets peaks reach +2.83 dBFS (1.385) on onsets and steady signals.
- **(c) +30 dBFS hard clamp:** redundant once the r3 ±1.0 clip is in place (no reference capture ever exceeds 0 dBFS).
  Remove it, or keep it only if it can never change the output.

## Required changes (implement spec r3)
1. §5.1 hard clip of the final output to ±1.0; Appendix A expansion columns replaced by unclipped curves (the build's
   own reconstruction should be replaced by the r3 table). Remove the soft +2.83 dBFS limiter and the onset hold.
2. §3 reset state at stream start / host reset: the build kept gate state between renders (gain 0.61 at the first
   measured block where the reference starts closed, Release 1000 ms).
3. §6 EXP_CAL and Appendix B extrapolation instead of the per-setting expander calibration.
Optional: give the negative ("N") floor 0.1 dB display resolution.

## Not verified
Linux and CLAP hosting, automation, GUI, 88.2 kHz and above, both modules in Split, mono variants, Output-gain
placement relative to the clip (D9 assumption). The reference's own PeakRef renders in the onset test showed its
parameter-order artefact (r1 D1) and were not used.
