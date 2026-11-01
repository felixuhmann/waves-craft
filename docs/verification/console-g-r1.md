# Independent verification: Craft Console G against spec r1

Build `feat/console-g` @ 66cea1d, REAPER offline renders, identical settings and stimuli on build and reference, 44.1/48/96 kHz. Differences only. Worst values shown.

## Passes
| Area | Result |
|---|---|
| Latency, polarity, neutral | 0 samples; −99.9 dB re peak vs reference |
| Analog noise | −110.87 vs −110.73 dBFS |
| HP, all display values | ≤ 0.08 dB, so ignoring the small numerator terms is fine |
| LF/HF shelves on grid | ≤ 0.002 dB |
| LMF | ≤ 0.42 dB |
| HMF without ×3 | ≤ 0.35 dB to 15 kHz (one setting 0.71 dB at 14.5 kHz, see issues) |
| 96 kHz HMF, LP | ≤ 0.16 dB |
| Compressor static, default timing | ≤ 0.05 dB on rising staircases, 1.32 dB worst on falling |
| Expander/gate static | ≤ 0.55 dB, gate hysteresis within 0.33 dB |
| Link, noise vs sine | within 0.2 dB (noise vs sine 0.4 dB at T 0) |
| No latch into silence | pass |

## Issues against spec r2
1. Baseline curve must follow the output gain; the build applies it before.
2. LP at 44.1/48 kHz: 31 of 40 positions exceed 0.5 dB (2.84 dB worst) against the reference; use the per-rate tables.
3. HMF ×3: use a factor of 2.88, not 3 (3.5 k: 9730 vs 9393 Hz; up to 1.62 dB).
4. Compressor timing varies with threshold and ratio; it must not. T 0/R 2 attack t63 24 vs 44 ms, release t63 325 vs 126 ms; R 1.5 release 272 vs 127 ms.
5. Fast-attack H3 at −6 dBFS: −73 vs −52 dBc (±6 dB). Fast-attack static: up to 1.39 dB at R ∞ against the 1.0 dB near-knee limit.
6. Expander/gate timing: closing starts about 25 ms late, opening is about twice as slow, and long-release staircases differ by up to 4.24 dB.
7. LF 450 Hz +11.9 dB: 1.07 dB off-grid error; +6.0 dB 0.30 dB. Use the extra rows.
8. HMF −20 dB, Q 1.5 at 48 kHz: 0.71 dB at 14.5 kHz against 0.5 dB.

## Adjudicated questions
- §9.1 LP vectors vs table: both true at their own rate; the response depends on sample rate. r2 has per-rate tables.
- HMF ×3: applying it to the effective centre is right; the factor was wrong.
- Shelf rows: on-grid exact; extra rows added for the only off-grid failure found.
- Expander: no hysteresis difference; the state after silence is open at stream start in both.
- Gate floor below range 20: not measured, interpolation stays an assumption.
- Output safety stage: accepted (D9). The reference has no protection and exceeds +100 dBFS at extreme settings. The stage must be transparent below +24 dBFS.

## Not verified
Linux and CLAP builds, mono layout, automation in a host, GUI, rates above 96 kHz, §5 H3 at settings other than T −10/R 4, ±12 dB off-grid shelves beyond the 450 Hz row.
