# Independent verification: Craft Punch Compressor, build 8f378d4 against spec r1

Host renders at 48 kHz, plus 44.1 and 96 kHz subsets, about 200 settings. Build and reference got the same stimuli and settings. Differences only.

## Passes
| Area | Result |
|---|---|
| Neutral identity, latency | Bit-exact at 44.1 / 48 / 96 kHz; reported latency compensated |
| Static curves + makeup, Mix, Output | Staircases within 0.10 dB of the reference (worst T −48); R 1.25…50 and T −6…−48 grid agrees |
| §7.4 attack / punch | Worst 2.61 dB in the first 5 ms (limit 3); 0.61 dB from 8 ms |
| §7.4 release | Worst 0.87 dB (limit 1.5) |
| §6 LF ripple H3 | Within 3.2 dB (limit 4) |
| §10 response, fundamentals, noise totals, clicks | Response ≤ 0.25 dB, fundamentals ≤ 0.07 dB, totals exact |
| Rate behaviour | Differences at 44.1 / 96 kHz match those at 48 kHz |

## Fails against r1
- §6 noise vs sine: Release 1000 is +0.62 dB (limit ±0.5). The other eight are −0.31…−0.40 dB.
- §11: a slow output-stage release lowers levels well below the ceiling (see below).
- §10: Mode 3 leaves a DC transient after hot passages, mean up to 0.18 decaying over about 300 ms; the reference stays within 0.002. Gain reads about 20 dB high for up to 200 ms.

## Differences outside r1's tables (now normative in r2)
- Release does not generalise. After short overloads the reference recovers in milliseconds at any Release; the build takes tens to hundreds of ms. Of 1152 grid points, 19 % are off by more than 1.5 dB and 8 % by more than 3 dB (max 7.6 dB). Example: Release 100, 20 ms burst: +4.36 dB vs −2.72 dB at 2 ms. Programme material differs by up to about 5 dB (1.3 dB RMS).
- Output stage: the reference has no slow release. It is back at full gain within 2 ms; the build needs over 100 ms. With Output +12 the build dips to −2.1 dB where the reference holds +5.7 dB. Loud peaks reach 0.85 where the reference reaches 1.0.
- 100 Hz under compression: at amplitude 0.1, +3.62 dB vs +4.86 dB, H3 −42.3 vs −35.7 dBc.
- Analog noise: the build is white; the reference is strongly shaped (r2 §10).

## Adjudications
- Fitted two-stage release: meets the r1 tables but fails beyond them. Rework against r2 §7.5.
- Empty Δ cells, Δ → 0 below Ratio 1.25, 29…300 BPM map: accepted (r2 D6). Ratios between 1 and 1.25 and the tempo map were not measured.
- Limiter = Clipper: accepted. Final clamp: accepted (D2); the reference exceeds 1.0 (up to 1.14 in Mode 3), by design not reproduced.
- Exact release law: not identified; tables are normative. Trajectory window: [t, t + 1 ms). Harmonic analysis: Hann over whole seconds, from 0.5 s.

## Issues for the builder (against r2)
1. Release: meet §7.5 and keep §7.4, §6.
2. Output stage: remove the slow release and over-limiting, meet §11.
3. Mode 3: remove the post-hot DC transient; check all modes.
4. Noise vs sine at Release 1000 within ±0.5 dB; 100 Hz level vectors.
5. Shape analog noise per §10.

## Not verified
GUI, automation in a host, CLAP, Linux, mono layout, rates above 96 kHz, Ratios between 1 and 1.25, manual tempo. r2 §7.5 and the new vectors are reference measurements used directly as tables, so no law was checked literally.
