# Independent verification — de-esser (build of spec r1)

Performed 2026-10-29 by the verifier (analyst side), per step 5 of [docs/process.md](../process.md). Only observed
differences are reported; no reference-side implementation detail is passed on. "The reference" = the licensed
processor the spec was derived from.

## Method
- Host: REAPER 7.83 x64 (Windows), unattended offline renders, 32-bit float; 48 kHz, plus 44.1 and 96 kHz subsets.
- Build under test: `feat/de-esser` @ e3778eb, Release VST3 (`Craft De-Esser`, `Craft De-Esser Mono`).
- Stimuli (user-generated): −30 dBFS sweeps (L, then R), 10 kHz sine level staircases (L=R and L only), high-passed
  noise staircase, the spec's burst sequence, pilot-tone + probe-sweep pairs (split shelf), and a new 35 s burst set
  (bursts 1 ms … 1 s at −10/−20/−30 dBFS, double bursts at 20–300 ms spacing, a 10-burst train).
- Identical parameter values in build and reference; build and reference rendered in the same harness.
- Gain trajectories are extracted **exactly** as sample-wise `y/x` (Wideband, zero latency). The r1 spec's envelope
  vectors had been extracted with an analytic-signal (Hilbert) estimator, which is biased within ~10 ms of level steps.

## Results against spec r1

| Area | Configs | Result |
|---|---:|---|
| Sidechain filter / monitor output vs §4/§6 model | 27 (HP/BP × 2–16 kHz, 44.1/48/96 kHz) | pass — worst -145.4 dB re peak (limit −100) |
| Sidechain / monitor vs reference | same | worst -104.9 dB re peak |
| No-reduction path (Threshold 0, Split and Wideband, Freq 2/5.5/16 kHz) | 6 | pass — bit-identical to input at 44.1/48/96 kHz |
| Static curve, 10 kHz sine L=R (Thr −12/−24/−36/−60/0) | 5 × 13 levels | pass — max 0.11 dB from reference (±0.3) |
| Static curve, L only (Thr −36) | 13 levels | pass — max 0.06 dB |
| Noise vs sine (Thr −36) | 13 levels | pass — max 0.21 dB |
| Split shelf at maximum attenuation (Freq 3/5.5/11 kHz) | 3 × 8 freqs | pass — max 0.008 dB / 0.03° from reference (±0.1 dB / ±1°) |
| Mono variant vs stereo with L = R | 1 | pass — -229 dB re peak |
| Envelope vectors §9.4 (as written in r1) | 6 | pass within r1's tolerances (the builder fitted to them) |

## Finding: the r1 release targets did not describe the reference

With exact gain extraction, the reference recovers much faster after short bursts than r1's vectors and single
release constant implied, with a slow tail whose share grows with how long the reduction was held. Examples (gain in
dB, Threshold −40, 10 kHz, offsets in samples at 48 kHz after the burst ends):

| After | +96 build / ref | +192 build / ref | +480 build / ref | +2400 build / ref |
|---|---|---|---|---|
| −10 dBFS for 1 ms | -17.1 / -14.0 | -15.7 / -9.2 | -10.1 / -3.9 | -0.5 / -0.7 |
| −10 dBFS for 5 ms | -24.2 / -17.3 | -20.5 / -10.7 | -11.2 / -6.8 | -0.5 / -1.5 |
| −10 dBFS for 20 ms | -24.1 / -21.2 | -19.5 / -16.9 | -9.9 / -10.8 | -0.4 / -2.0 |
| −10 dBFS for 1000 ms | -23.8 / -21.9 | -19.4 / -17.3 | -11.7 / -10.9 | -1.9 / -2.0 |
| −20 dBFS for 5 ms | -15.8 / -11.9 | -13.6 / -8.2 | -7.8 / -5.5 | -0.4 / -1.2 |

A two-component release (fast part, plus a slow part that charges while reduction is held) reproduces the reference
to 0.08–0.10 dB RMS on the new burst set at 44.1/48/96 kHz. It is specified in **spec r2**
([`spec/de-esser/r2/SPEC.md`](../../spec/de-esser/r2/SPEC.md), supersedes r1: release law, re-derived §9.4 vectors, new §9.6 release table, updated tolerances).

## Results against spec r2 (release only; everything else is unchanged from r1 and passes as above)

| Check | Result |
|---|---|
| §9.4 vectors, max deviation (limit ±1.0 dB) | A: 0.65, B: 2.07, C: 0.28, C′: 0.49, D: 8.37, E: 0.36 |
| §9.6 trajectory points within tolerance | 106 / 166 (fails: 60) |
| Gain error RMS on the §9.6 stimulus (limit 0.3 dB) | 0.57 dB |
| Same at 44.1 / 96 kHz (vs the r2 model) | 0.57 / 0.57 dB RMS |

Largest deviations (build − reference, dB): after a 2 ms burst +192: −9.2; after a 5 ms burst +192: −9.9; two 5 ms bursts
20 ms apart, +192: −7.4; after sustained reduction +96/+192: −2.0; after 20–50 ms bursts +960: +2.3 (build recovers
too slowly at first after short bursts, and slightly too fast in the tail after long ones).

**Verdict:** conforms to r1 except for the release behaviour, which r1 itself mis-described; **does not yet conform to r2**
(release law). Everything else needs no change.

## Not verified
- Linux and CLAP builds (CI only); automation / parameter smoothing and mode crossfades in a host; GUI and meters;
  88.2/176.4/192 kHz; Band-pass sidechain dynamics beyond the filter response; Mono variant beyond the L=R equivalence.
