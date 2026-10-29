# Independent verification — vocal compressor, build against spec r2 (→ r3)

Performed 2026-10-29 by the verifier (analyst side), per step 5 of [docs/process.md](../process.md). Differences only.

## Method
REAPER 7.83 (x64, Windows), unattended offline render, 32-bit float. Build under test: `feat/vocal-compressor` @ 11857fb
(Windows Release VST3). Same stimuli as the r1 verification plus: 4 s low-frequency tones for the ripple settling
question, and 37 s of synthetic speech-like material (voiced/unvoiced syllables, phrase levels −30…−6 dBFS).

## Results against spec r2
| Area | Result | Numbers |
|---|---|---|
| §9.2 attack ramp, 44.1/48/96 kHz | pass | build vs reference ≤ 0.0006 dB at every measurable sample |
| §9.3 release after sustained levels, 44.1/48/96 kHz | pass | worst 0.58 of tolerance |
| §9.4 release after bursts, 48/44.1/96 kHz | pass | worst 0.88 / 0.92 / 0.89 of tolerance (Compression −18, 100 ms, t = 2 ms) |
| §9.7 release patterns | **1 point fails** | Compression −24, `step_0_-20_50`, t = 10 ms: 1.72 dB vs 3.32 (1.06 of tolerance); all other 251 points pass |
| §9.6 / §9.6.1 gate closing | pass | worst 0.41 dB (tolerance 0.75) |
| §9.8 ripple (r2 conditions: 1.5 s tone, window 1.0–1.5 s) | **1 point fails** | Compression −36 / 50 Hz: 0.276 dB vs limit 0.26 (reference 0.11); other 8 points pass |

## A9 (builder question): tone length of the §9.8 measurement
The reference values were measured with a 1.5 s, −6 dBFS tone after 1.5 s of silence, over its last 0.5 s (1.0–1.5 s
after onset). r2 did not state this. The reference's own ripple keeps settling for 1–3 s, so the result depends on the
window. Spec r3 now states the stimulus and tabulates ripple for windows 0.5/1.0/1.5/2.0/3.5 s after onset (0.5 s window
informative). Reference vs build (windows 1.0 / 1.5 / 2.0 / 3.5 s):

| Compression | f (Hz) | reference | build |
|---|---|---|---|
| -6 | 50 | 0.69 / 0.58 / 0.56 / 0.55 | 0.07 / 0.05 / 0.05 / 0.05 |
| -6 | 100 | 0.24 / 0.19 / 0.17 / 0.17 | 0.02 / 0.00 / 0.00 / 0.00 |
| -6 | 200 | 0.02 / 0.01 / 0.01 / 0.01 | 0.01 / 0.00 / 0.00 / 0.00 |
| -18 | 50 | 0.20 / 0.13 / 0.12 / 0.12 | 0.21 / 0.10 / 0.10 / 0.10 |
| -18 | 100 | 0.04 / 0.02 / 0.02 / 0.02 | 0.12 / 0.01 / 0.00 / 0.00 |
| -18 | 200 | 0.00 / 0.00 / 0.00 / 0.00 | 0.06 / 0.00 / 0.00 / 0.00 |
| -36 | 50 | 0.11 / 0.06 / 0.05 / 0.05 | 0.28 / 0.11 / 0.07 / 0.07 |
| -36 | 100 | 0.03 / 0.01 / 0.01 / 0.01 | 0.02 / 0.02 / 0.00 / 0.00 |
| -36 | 200 | 0.00 / 0.00 / 0.00 / 0.00 | 0.00 / 0.00 / 0.00 / 0.00 |

The build settles more slowly than the reference at 50 Hz with Compression −36 (0.28 dB at 1.0 s; within limits from
1.5 s on) and is otherwise at or below the reference. Its 0.5 s-window values are higher than the reference's at
Compression −18 (50 Hz: 1.98 vs 0.98 dB; 100 Hz: 1.03 vs 0.21 dB). That window is informative only.

## Program-like material (gross behaviour)
20 ms RMS envelopes of the output, build vs reference, where the reference output is above −60 dBFS:
| Settings | median \|Δ\| | 95th pct | max | overall level |
|---|---|---|---|---|
| C-6 | 0.02 dB | 0.34 dB | 0.69 dB | -0.05 dB |
| C-18 | 0.08 dB | 0.88 dB | 1.88 dB | +0.04 dB |
| C-36 | 0.74 dB | 3.12 dB | 5.14 dB | +0.85 dB |
| C-18, G-50 | 0.08 dB | 0.88 dB | 1.88 dB | +0.04 dB |
| C-36, G-50 | 0.74 dB | 3.12 dB | 5.14 dB | +0.85 dB |

At Compression −6/−18 the build tracks the reference closely. At Compression −36 it is 0.85 dB louder overall, with
envelope deviations up to 5 dB on speech. The release tables do not fully constrain the release law for speech-like
material; the build stays within every table tolerance. The gate (−50 dB) adds no difference.

## Remaining differences (to fix)
1. §9.7: Compression −24, `step_0_-20_50`, t = 10 ms: remaining reduction 1.72 dB vs 3.32 (tolerance ±1.5). It also
   reads 7.27 → 4.87 → 3.32 dB at 2/5/10 ms in the reference, against the build's faster early release.
2. §9.8 (r3): Compression −36, 50 Hz, window 1.0–1.5 s: 0.276 dB vs limit 0.258 (reference 0.108). The slow component
   settles later than in the reference.
3. Informative only (no table): at Compression −36 on speech-like material the build is about 0.85 dB louder than the
   reference. This is consistent with item 1, a slightly faster release of moderate level drops.

## Not verified
Linux and CLAP builds; automation in a host; 88.2/176.4/192 kHz; mono layout; GUI.
