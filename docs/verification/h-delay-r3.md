# Independent verification — hybrid delay against spec r3 (build 51f2e79)

Performed 2026-10-29 by the verifier (analyst side), step 5 of [docs/process.md](../process.md). Only observed
differences are reported. Follows [h-delay-r2.md](h-delay-r2.md).

## Method
REAPER 7.83 (x64, Windows), unattended offline renders, 32-bit float, 48 kHz. Build `feat/h-delay` @ 51f2e79
(Release VST3). Same stimuli and raw parameter values as the r2 pass; reference renders reused where settings are
identical, new reference renders for the HiPass and low-level checks. About 140 configurations.

## Results against spec r3

| Area | Result | Numbers |
|---|---|---|
| §9.2.1 clicks, all four modes (mode/Off echo ratios) | Pass | worst error −0.33 dB (mode 2, click 0.3/Feedback 80 %, echo 6); no mode above Off (mode 4 −0.02…−0.15 dB) |
| §9.2 mode responses | Pass | mode 1 within 0.3 dB, mode 3 sweep row within ±3 dB, mode 4 within 0.5 dB; mode 2 reads −0.9 dB above 2 kHz vs table −1.4…−1.7 (within ±1 dB, but 0.5–0.8 dB less tilt than the previous build) |
| §9.2 distortion table (100 Hz, a 0.03–0.95) | Pass | fundamentals within 0.4 dB; harmonics within ±4 dB |
| §9.3 noise vs feedback | Pass | worst −0.9 dB (mode 2, 100 %) |
| HiPass near r = 0, Analog Off and mode 2 | Pass vs spec, −0.29 dB vs reference | the build follows `19·1058^r` exactly (r = 0.01 → 20.36 Hz) in both Analog settings; the reference uses the displayed integer frequency (20 Hz): −0.29 dB at 15 Hz, −0.16 dB at 20 Hz. r = 0.05: within 0.03 dB |
| Core regression (delays, feedback, Ping Pong, filters, Mix/Output/Phase) | Pass | build vs §4 model ≤ −140 dB; filters within 0.01 dB of the reference |
| §4.2 / §4.3 / §4.4 | Pass | unchanged from the r2 pass (worst −0.09 dB / H3 within limits / +0.23 dB) |
| §8 modulation table | Pass | unchanged (worst −12 %) |
| LoFi, glide, note values | Pass | stopband ≤ −52 dB; τ 0.19 s; note values exact |

The earlier r2-pass HiPass renders were made with Analog Off.

## Differences outside the r3 tables (candidate r4 items)
- HiPass/LoPass: reference quantises the frequency to the displayed resolution (see above).
- Analog Off, a = 0.3: 3rd harmonic at 400 Hz / 1 kHz is −39.0 / −38.9 dBc vs reference −42.5 / −49.6 dBc (r3 only
  specifies 100 Hz).
- Mode 1, a = 0.3: 2nd harmonic −50.6 dBc at 400 Hz and −82.4 dBc at 1 kHz vs reference −43.2 / −56.2 dBc — the
  reference's mode-1 distortion does not stop at 178 Hz, it falls off gradually.
- Mode 4 at very low level: 100 Hz at −60 dBFS reads +5.8 dB / H3 −21.7 dBc vs reference −6.4 dB / H3 −9.6 dBc; at
  −80 dBFS +8.5 dB vs +3.5 dB (100 Hz) and +8.5 vs +7.8 dB (1 kHz). The low-level gain rise matches at 1 kHz but not the
  reference's strong low-level distortion around −60 dBFS.

## Observation
Two render batches containing the build ended with REAPER exiting unexpectedly mid-batch (exit code 1), once during
a 48 kHz grid; re-running the remaining configurations succeeded. The reference batches did not show this. A
plugin-validator run (instantiate/process/teardown stress) on the build is recommended.

## Not verified
GUI, Tap, BPM-sync mode, Dual mode, Mono→Stereo routing, automation other than delay time, CLAP hosting, Linux build,
44.1/96 kHz for the r3 changes, wet path above 20 kHz.
