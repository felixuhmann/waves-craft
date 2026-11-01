# Independent verification — colour delay, spec r3 build

Performed by the verifier (analyst side), per step 5 of [docs/process.md](../process.md). Only observed differences
are reported; no reference-side implementation detail is passed to the builder.

## Method
REAPER 7.83 (x64, Windows), unattended offline renders, 32-bit float. Build under test: `feat/color-delay` @ b6b290d,
Release VST3. Same stimuli and settings as the reference measurements: effects programme at 44.1 / 48 / 96 kHz,
impulses at 100 / 120 / 137 BPM, a full-scale square at Feedback 100 %, and the quiet-sweep core grid.

## Results against spec r3

| Area | Result | Numbers |
|---|---|---|
| Effects fed after the compressor | **Pass** | Doubler and distortion Send 10/25 % gains follow the delay-path gain reduction on the staircase (0 to −17.5 dB) |
| Doubler gain | **Pass** | +13.2 dB below threshold (spec +13.0 ± 1); end-to-end +9.4 / +1.3 / −4.3 dB at −20 / −8 / 0 dBFS (reference +9.4 / +1.2 / −4.5); quiet sweep +12.5 dB (reference +12.85) |
| Distortion gain vs level, Send 10 / 25 / 50 / 100 % | **Pass** | Within 0.2 dB of the reference at every step |
| Distortion small-signal, Send 25 / 50 / 100 % | **Pass** | Within 0.5 dB, 100 Hz–4 kHz. Outside the normative band, 8 kHz at Send 100 % is +9.3 vs +11.3 dB |
| Distortion H3 | **Pass** | Within 1.3 dB at −24 / −12 / 0 dBFS, Send 50 and 100 % |
| Distortion gate, Send 10 % | **Pass** | Silent at −32 dBFS, open at −28 dBFS (−10.7 dB, equal to the reference) |
| Per-channel routing, Width 100 % | **Pass** | No cross-feed; L/R 0.00 dB; tone correlation 0.57 (reference 0.60) |
| Sample-rate agreement | **Pass** | Distortion and doubler level tables agree to ≤ 0.1 dB across 44.1 / 48 / 96 kHz; Send 50 % at 8 kHz −19.0 / −18.2 / −18.6 dB |
| Overload (§7.5) | **Pass** | Bounded: wet output ≤ +1.9 dBFS. Differs from the reference's 0 dBFS internal clip by review decision |
| Regression: host sync | **Pass** | 24/24 exact |
| Regression: reverb | **Pass** | E −1.2 … +0.5 dB at the three rates (reference −0.7 … +0.8 dB); octave profile as in the r2 check |
| Regression: core path | **Pass** | Quiet-sweep grid vs spec model median −146.7 dB |

## Remaining differences (within r3)
- A steady 1 kHz tone through the reverb at Size 50 % reads about 17 dB lower than the reference. A single tone samples
  one point of a dense modal response and is not an r3 metric; band energy matches. Not investigated further.
- Reference delay-path clipping at 0 dBFS is deliberately absent.

## Not verified
GUI, automation/parameter smoothing in a host, delay-time change behaviour, link controls in a host, phaser beyond the
1 Hz rate check of the r1 round, mono-to-stereo layout, CLAP hosting, the Linux build (CI), rates above 96 kHz.
