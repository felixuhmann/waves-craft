# Independent verification — multiband r1 (Craft Multiband)

Performed 2026-10-29 by the verifier (analyst side), per step 5 of [docs/process.md](../process.md).
Only observed differences are reported; no reference-side implementation detail is passed to the builder.

## Method
- Host: REAPER 7.83 x64 (Windows), unattended, offline render, 32-bit float; 48 kHz plus 44.1 / 96 kHz subsets.
- Build under test: `feat/multiband` @ 897ff20, Release VST3.
- Stimuli (own synthesis): L/R log sweeps, sine stairs −80…0 dBFS per band, step/burst sequences at 1 kHz and 40 Hz,
  equal-RMS noise stair, L/R link test, an extended overload sequence (5 / 30 / 100 / 300 ms and 1 s overloads at
  −20 / −10 / −4 dBFS) and a synthetic drum loop. Identical settings were rendered through the build and the reference.
- About 115 settings, including 10 dynamics settings and 3 four-band program settings that are not in the spec tables
  (used to test whether the build generalises beyond the tables).

## Results against spec r1
| Area | Result |
|---|---|
| Crossover (27 settings incl. Q 0.1–0.75, other crossovers, gains, band bypass, solo) | Pass. Build vs §3 model ≤ −130 dB re unity; host bypass bit-exact. Build vs reference within the spec's stated model agreement. |
| Crossover at 44.1 / 96 kHz | Pass vs model (≤ −133 dB). Differences from the reference at 96 kHz, Q < 0.5 are the by-design decision D5. |
| Stop-band examples §3 (Q 0.6 / 0.1, |B1| @ 1 kHz) | Build −76.43 / −41.65 dB = model exactly. The spec's −76.7 / −42.2 dB are reference measurements; the difference is within the model agreement. Spec corrected in r2. |
| Static curves (all Ranges, all Knees incl. Hard rows, thresholds, gain, Opto/Auto, band 3) | Pass. Max deviation from the reference 0.06–0.22 dB (limit ±0.3 / ±0.4 dB); hysteresis ≤ 0.07 dB. Same at 44.1 / 96 kHz. |
| Stereo link | Pass. Identical to the reference (−10.16 / +10.43 dB on both channels). |
| Noise detector vector §6 | Pass, but with little margin at slow settings: Attack 5 / Release 50 gives up to 0.69 dB less reduction than the vector (limit 0.75 dB); the reference is identical for both timings. |
| Attack table §5 (90 points) | Pass except Range +12 @ 1 ms (+4.8 vs +3.5 dB); the reference misses that point too (measurement window), so not counted. |
| Release table §5 (112 points) | Pass. Worst 0.91 of tolerance (Opto, Release 500 ms @ 100 ms). |
| 30 ms burst table §5 (70 points) | 1 marginal miss (1.01 of tolerance: Auto, Electro, Release 50 ms @ +20 ms: −3.3 vs −1.1 dB). Electro after a 30 ms burst releases consistently ~2 dB slower than the reference at +20 ms. |
| Sample-rate independence | Pass. 96 kHz vs 48 kHz build envelopes 0.006 dB RMS; 44.1 kHz 0.023 dB RMS (away from step edges). |
| Latency | 0 samples reported and observed. |

## Generalisation of the fitted dynamics law (cases not in the r1 tables)
| Case | Result |
|---|---|
| Sustained overloads (1 s at −20 / −10 / −4 dBFS), steady states, attack, Range ±6 / +9 / −18, Hard / Soft knee, Threshold −40 | Good: ≤ 0.2 dB RMS per segment. |
| Release after 100 ms and 300 ms overloads | **Does not generalise.** The build keeps 3–6 dB more reduction than the reference for 20–200 ms (e.g. Range −18 / Hard / Release 300 / Auto after 100 ms: −10.7 vs −4.4 dB at +20 ms; Release 2000 / Electro / Auto after 100 ms: −8.2 vs −3.8 dB). |
| Release after 5 ms overloads | Electro: up to 1.5 dB more reduction than the reference. Opto: up to 1.8 dB less. |
| Drum loop, 3 four-band settings | Level envelope (10 ms) 0.6–0.9 dB RMS, p95 1.2–1.9 dB, max 2.3 dB; build ≈0.6 dB more reduction on average (Electro). |

Against the new spec r2 §5.3 table (250 points) the build fails 47 points (worst 3.3 × tolerance, RMS deviation 1.27 dB);
the reference passes all (worst 0.05 × tolerance).

## Builder assumptions — adjudication
- Burst-table Attack 5 ms: correct (now stated in r2).
- Auto "forgets" after 20 × Release, minimum Auto release 5 %: not confirmed. Auto and Manual both release faster after
  short overloads than the build does (see generalisation); r2 §5.3 now constrains this.
- Hard-knee Range rows interpolated between −24 / −12 / +18: consistent with the reference wherever measured (static and
  Range −18 Hard dynamics).
- 96 kHz detector ripple at 20 Hz: not tested (band-2 and band-1/40 Hz tests at 48 kHz pass).

## Spec revision
Spec r2 (`spec/multiband/r2/SPEC.md`) supersedes r1. It corrects the stop-band examples, defines the tolerance start
value, states the table Attack, adds the §5.3 overload-duration table and a robustness requirement. See its
"Changes from r1".

## Issues for the builder (against r2)
1. Implement a release whose speed depends on the duration of the preceding overload in all modes, so r2 §5.3 passes
   (47 / 250 points currently fail), while keeping the r1 tables passing.
2. Electro release after a 30 ms burst: ~2 dB too slow at +20 ms (within tolerance but systematic). Should improve with item 1.
3. Noise vector at slow time constants: 0.69 dB of 0.75 dB used; make the detector give the same result at both timings.

## Observed but not reproducible
In one host session, the build's output went silent and stayed silent after a sequence of parameter changes. It started
at a Knee change from 0.30 to 0.71, while the render script was being modified externally. Fresh sessions and an exact
replay of the same sequence rendered normally. Not counted as a non-conformance; r2 §6 adds a robustness requirement as
a safeguard.

## Not verified
Linux and CLAP builds, Mono layout, automation and smoothing in a host, crossed crossovers, band 1 at 96 kHz / 20 Hz, GUI.
