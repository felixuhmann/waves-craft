# Independent verification — Craft Console E (spec console-e r1)

Performed 2026-10-30 by the verifier (analyst side), step 5 of [docs/process.md](../process.md). Differences only; no
reference implementation detail. Build under test: `feat/console-e` @ 29e09f9, Release VST3.

## Method
- Unattended host renders (32-bit float) of the build and the reference with **identical host-normalised control
  positions r** for every setting (same stimuli: −30 dBFS L/R log sweeps at 44.1/48/96 kHz; a 1 kHz dynamics stimulus
  with a −60…+3 dBFS staircase, steps, bursts, a link section, a gate section and a −6 dBFS frequency set).
- 184 + 44 EQ/filter settings at 48 kHz, 14 each at 44.1/96 kHz, 87 dynamics settings, 6 analogue/noise settings,
  4 maximum-gain settings. Comparison = build response / reference response (sweeps) or step gains and 10/63/90 %
  times (dynamics).

## Results against r1
| Area | Result |
|---|---|
| EQ/filters where both plugins display the same control value (48 kHz) | Pass — within ±0.5 dB to 12 kHz, ±1.5 dB above, except the low-pass row below |
| EQ/filters at other control positions | **36/184 outside tolerance (up to 6.5 dB)** — all caused by the r1 §2 tapers (same r gives a different band frequency/Q than the reference; e.g. LMF 0.44 vs 0.50 kHz, Q 0.30 vs 0.50, LF 125 vs 100 Hz). Spec error, corrected in r2 |
| Low-pass, 48 kHz | **0.57–0.69 dB too much loss at 10–12 kHz** (r = 0.4–0.6; limit 0.5); 1–3 dB more loss than A.2 at 16–20 kHz (inside ±3) |
| Low-pass, 44.1 kHz | r = 1.0: **3.5 dB** more loss at 12–20 kHz (limit 3); r = 0.5: 0.61 dB below 12 kHz |
| Low-pass, 96 kHz | r = 1.0: **1.24 dB** below 12 kHz (limit 0.5); r = 0.5: 2.6 dB at 12–20 kHz (inside 3) |
| HF bell at 16 kHz, 96 kHz | **1.65 dB** (limit 1.5); 44.1 kHz passes |
| High-pass (all rates), LF/LMF/HMF at matching values (all rates) | Pass |
| Neutral / polarity / trim / fader / latency | Pass — neutral −109.6 dB re peak at −30 dBFS; polarity exact; exact dB gains; 0 latency |
| Saturation | Pass — gain +0.021 dB and H3 −62.0 dBc at 0 dBFS (reference −62.1); H2 absent (reference −94 dBc, informative in r2) |
| Analogue switch | Pass — −110.7 dBFS rms per channel, uncorrelated, unchanged by output fader / input trim |
| Compressor static, slow attack, release 0.4 s, ratios 1.5–20 | Pass — ≤ 0.21 dB |
| Compressor static, ratio ∞ | thresholds +10/0/−20 pass (≤ 0.48 dB); **threshold −10: 0.76 dB** |
| Compressor static vs. attack mode / release | **up to 2.7 dB less reduction** than the reference with fast attack or long release (reference reduction grows with release and in fast mode; r2 §6.5) |
| Compressor timing (t63/t90) | Pass (±25 %) for all release settings; attack slow passes; **fast attack with 4 s release: t63 0.6×, t90 0.41× of the reference** (r2 §6.6) |
| Frequency dependence §6.4 | Pass — ≤ 0.2 dB |
| Expander/gate floors | Pass — range 5/15/20/30/40 and gate floors identical |
| Expander/gate switching level | **≈ 9–12 dB too high** (build expands/gates at levels where the reference is still at unity; gains differ by up to 52.7 dB on the staircase) — caused by the r1 §7.1 table, which was offset by +9 dB; corrected in r2 |
| Expander/gate release timing | Pass (t63 within ±25 %) where the switching level does not interfere |
| Routing (pre/post EQ, EQ to sidechain, split) | Pass except the LF-band frequency difference from the §2 tapers |

## Builder's questions — adjudication
1. "Q 0.5" rows: the data is right; Q 0.50 is at r = 0.90 (r1 §2 knots were wrong for most controls). r2 §2 gives the exact 21-point tapers.
2. Low-pass: r1 §4 f/Q pairs cannot reproduce A.2; A.2 is normative (r2 §4); "≤ 2.7 dB at 20 kHz" was wrong (r = 0.3 loses 7.7 dB). The build's extra pole overshoots A.2 (rows above).
3. Tolerance bands are now in absolute frequency at every sample rate (±0.5 dB to 12 kHz, ±1.5 dB 12–20 kHz, ±3 dB for the low-pass) — the build's interpretation.
4. Test vector 1 applies at ≤ −30 dBFS (−88 dB at −20 dBFS) — the build's interpretation.
5. H2 is present in the reference (−94 dBc at 0 dBFS) but informative only; omission accepted; polarity must stay exact.
6. The expander/gate switch point must follow the corrected r2 §7.1 table (r1 list offset by +9 dB); the "10.9 dB below threshold" derivation is invalid.
7. Default r values: with the r2 tapers the spec defaults display exactly 60 Hz / 0.50 kHz / 0.20 s etc.; the adjusted r values must be reverted.
8. Gain staging: the reference has no output limiting (with all gains at maximum it produces levels far above +40 dBFS); the stress-test flags above +40 dBFS are expected. Output must stay finite.

## Issues for the builder (implement spec r2)
1. §2: use the exact 21-point tapers for all continuous controls (and their display), revert the adjusted defaults.
2. §4 low-pass: meet Appendix A.2 at 44.1/48/96 kHz (currently 0.6–0.7 dB too much loss at 10–12 kHz, 3.5 dB at 12–20 kHz for r = 1.0 at 44.1 kHz, 1.24 dB below 12 kHz at 96 kHz).
3. HF bell at 16 kHz, 96 kHz: 1.65 dB → ≤ 1.5 dB.
4. §6.5: steady-state reduction must depend on attack mode and release as tabulated (missing; up to 2.7 dB).
5. §6.6: fast-attack timing must lengthen with release (4 s: ≈ 1.5/5/11 ms).
6. Ratio ∞, threshold −10: 0.76 dB → ≤ 0.5 dB.
7. §7.1: expander/gate static behaviour per the corrected table (switching level ≈ 9–12 dB too high now).

## Not verified
Linux and CLAP builds; automation/smoothing in a host; the GUI; mono variant; 88.2/176.4/192 kHz; dynamics timing at
44.1/96 kHz; the split-filter dynamics path beyond the high-pass case.
