# Independent verification — Craft Dynamics / Compressor / Gate / Compressor SC (spec r1 build)

Verifier side, step 5 of [docs/process.md](../process.md). Differences only; no reference implementation details.
Build: `feat/dynamics` @ dbf4fa3 (Release VST3, Windows). Host: REAPER 7.83, offline 32-bit float renders, controls set
by displayed value (raw position for Ratio). Reference renders with identical settings and stimuli (1 kHz / 100 Hz /
10 kHz / noise stairs, 1 kHz burst sequences, L/R sweeps) at 44.1 / 48 / 96 kHz, about 250 configurations.

## Summary
The static behaviour, filters, routing, lookahead, linking, sample-rate independence and variants conform. The
dynamic behaviour does not fully match the reference; most of that traces to spec r1 itself, which has been revised:
**spec r2** (`spec/dynamics/r2/SPEC.md`, supersedes r1). The build must be updated to r2 (issues below).

| Area | Result | Numbers |
|---|---|---|
| Static compressor curves (all 14 ratio positions, T −80/−60/−40/−20) | pass | ≤ 0.07 dB (0.50:1 expansion: 0.28 dB, within ±0.5) |
| Makeup, input/output gains, bypass | pass | ≤ 0.01 dB; neutral bit-exact |
| Lookahead / latency, key modes L→R / R→L, stereo link | pass | wideband paths ≤ −104 dB re peak; link ≤ 0.01 dB |
| Sample rates 44.1 / 48 / 96 kHz | pass | burst envelopes identical across rates (Att 10: RMS 0.27 dB at all three) |
| Filter module magnitudes (LP/BP/HP/BR, sidechain |H|³, active, passive, split flat) | pass | ≤ 0.1 dB above −40 dB |
| Gate open/close thresholds | pass | 0.25 dB lower than the reference (±0.3) |
| Gate floor law | pass | ≤ 0.08 dB |
| Expander static (Appendix B), positive floors | pass | ≤ 0.05 dB |
| Variants (compressor-only, gate-only, compressor + filter) | pass | same as Full |
| Band-pass split / passive / active **polarity** | **fail** | build output inverted (180°) vs reference; band-reject correct |
| Sidechain monitor with only the gate filtered | **fail** | reference monitor wideband; build shows the gate's band-pass |
| Detector type (noise vs sine) | **fail** | equal-RMS noise: build −0.33 dB vs reference +0.86 dB (≈1.2 dB); 100 Hz/10 kHz within 0.25 dB |
| Compressor envelopes vs r2 §10.5.1 | **fail** | 41 of 460 cells outside tolerance: attack lags 2.6–4.7 dB in the first 3 ms after a step (all Att 1 rows); Rel 10 release 2.5–4 dB too fast at +2…+5 ms |
| Expander envelope | **fail** | starts closing at 67.5 ms vs 10.5 ms (t10); linear-gain P99 0.28 |
| Gate envelope, Release 1000 ms | **fail** | linear-gain RMS 0.104 (limit 0.06); other gate vectors 0.008–0.021 |
| PeakRef near full scale | **fail** | expansion ratios: output held at −14 … −21 dBFS where the reference reaches 0 dBFS (13–21 dB); compression ratios ≈ +1 dB too high at 0 dBFS |
| Negative ("N") expander floor | minor | 0.75 dB at −60 dBFS near the cancellation notch |
| Ratio display at exactly r = 0.8 | minor | shows "−50.00:1" (behaviour is 50:1) |

## Adjudication of the builder's flagged items
1. **§10.5 release envelopes:** confirmed. r1's single-stage law cannot meet its own tolerance (a literal r1
   implementation already misses the reference by up to 0.97 dB RMS / 2.59 dB P99). r2 specifies a revised law
   (rectified detector, detector release factor, PDR fast release, small slow memory) and makes sampled reference
   trajectories normative, with tolerances a literal implementation of r2's text demonstrably meets (460/460 cells).
2. **Noise vs sine claim:** the claim was right, the r1 model was wrong. The reference behaves like a rectified-average
   detector (white noise gets ≈0.8 dB less reduction); r2 §4 specifies it.
3. **Gate closes 10–14 ms late at Release 100 ms:** within r1's gate tolerance (linear-gain RMS 0.015–0.021); no change.
4. **Reconstructed expansion/output limit:** the static curves are fine, but the limit must act on the final output
   after the PeakRef offset and Makeup (r2 §10.9 table); applying it before the offset causes the PeakRef failures.

## Required changes (implement spec r2)
- §4 rectified-average detector with K_CAL; §4–§5 compressor envelope law; meet §10.5.1. Remove the extra 1 ms
  compressor gain smoother (it causes most of the early-attack lag).
- §6 expander dynamics (fast detector release, dB gain smoothing); meet §10.5.3. Gate envelope at Release 1000 ms.
- §7 allpass sign: band-pass split/passive/active must have the same polarity as band-reject.
- §3.7 sidechain monitor follows the compressor's EQ mode only.
- §5/§10.9 output limit on the final output; PeakRef near-full-scale table.
- §2 r = 0.8 displays "50.00:1". Negative-floor notch region to ±0.05 dB.

## Not verified
Linux and CLAP builds, automation and parameter smoothing in a host, GUI, external sidechain (not specified), 88.2 kHz
and above, both modules in Split at once, mono variants. Compressor Release 200 ms at 44.1/96 kHz (harness unit issue;
other envelopes confirm sample-rate independence).
