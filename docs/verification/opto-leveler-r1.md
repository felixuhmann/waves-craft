# Independent verification — Craft Opto Leveler against spec r1

Build under test: `feat/opto-leveler` @ 559a70c, Release VST3. Host: REAPER 7.83, unattended offline renders, 32-bit float.
Build and reference received identical stimuli and settings. Differences only; no reference implementation detail.

## Results (48 kHz unless stated)
| Area | Tolerance | Result |
|---|---|---|
| Gain law, Trim, Mix (below +30 dBFS output) | ±0.1 / ±0.05 dB | Pass, ≤ 0.03 dB |
| Static curves, Compress and Limit (520 points) | ±0.5 dB | Pass, worst 0.31 dB |
| Dynamics, 48 kHz (600 points) | ±1.0 dB, RMS ≤ 0.4 | Pass, RMS 0.291, worst 0.99 |
| Dynamics, 44.1 / 96 kHz (200 points each) | same | Pass, RMS 0.296 / 0.297, worst 1.00 (at the limit; C 50, −10 dBFS, 100 ms burst, +10 ms) |
| Sidechain weighting (48 points) | ±1.0 dB | Pass, worst 0.88 |
| Hi Freq static curves | ±0.5 dB | Pass, ≤ 0.1 dB after removing the reference's hidden make-up offset (review decision D2) |
| Detector type | ±1.0 dB | Pass, worst 0.54 dB |
| Harmonics, uncompressed | ±3 dB | Pass, worst 2.7 dB |
| Response | ±0.2 dB | Pass, worst 0.16 dB |
| Stereo link | | Pass, ≤ 0.06 dB |
| Analog noise components | ±2 dB | Pass: density −135.0 dBFS/Hz (reference −136.0), hum lines as specified |
| Off noise floor | < −170 dBFS | Pass (digital silence) |
| Harmonics, compressed (Peak Reduction 50) | ±3 dB above −90 dBc | **Fail, 12 points, see below** |

## Non-conformances
Compressed harmonics, build vs reference (dBc):

| Tone | H3 | H4 | H5 |
|---|---|---|---|
| 100 Hz, −30 dBFS | −75.0 vs −64.5 | | −91.7 vs −74.1 |
| 100 Hz, −3 dBFS | | −91.0 vs −86.8 | |
| 100 Hz, 0 dBFS | | −88.6 vs −84.9 | |
| 1 kHz, −18 / −10 / −3 / 0 dBFS | | | up to 5.2 dB high (−66.6 vs −71.1 at 0 dBFS) |
| 5 kHz, −18 / −10 / −3 / 0 dBFS | | | up to 9.2 dB high (−74.2 vs −83.4 at −18 dBFS) |

The spec's −30 and −40 dBFS compressed rows carry gain reduction from the preceding measurement. Both plugins received the
same sequence, yet the 100 Hz, −30 dBFS row also shows 1.38 dB less reduction in the build. These levels are low and the
5 kHz H5 lies above the audio band, so audibility is expected to be small, but they exceed the stated tolerance.

## Differences outside the spec's tables
- 100 Hz burst onset: 3.3–3.6 dB less reduction in the first 5 ms, converging by 20 ms. 5 kHz bursts are within 1.4 dB.
- Sine sweep under compression (Peak Reduction 70): up to 1.9 dB less reduction near 10 kHz and 1.1 dB near 50 Hz.
- Full-scale white noise, Limit, Peak Reduction 100, Gain 100: 4.8 dB more reduction than the reference.

## Adjudications
- **Output limiting:** the reference has no output clip or limit; its float output reaches +56.9 dBFS at Gain 100, Trim +18.
  The build's saturation first changes the level at about +31.8 dBFS, reaching −2.3 dB at +38.8 dBFS. It never acts in normal
  use and no spec table reaches it, so it is accepted as a safety measure. Spec r2 is not required for it.
- **Detector design and waveform-following gain:** accepted; they are implementation choices and the static, dynamics and
  detector-type tables pass. The waveform-following gain is the likely source of the excess H5.
- **±6 dB test bound for compressed H4 / H5:** not confirmed; the spec says ±3 dB (see Non-conformances).
- **T6 at 48 kHz:** confirmed.
- **Analog noise total:** −90.7 vs −88.8 dBFS RMS, same measurement on both, so a reference-level explanation does not hold.
  Every specified component passes, and the total in the spec is approximate; accepted.
- **Auto Makeup outside 40–70:** informative only. With host automation the build applies no coupling and its make-up follows
  the displayed Gain, as D2 requires.
- **Host taper:** matches the spec's tabulated points; between them the build interpolates linearly, as the spec states.

## Not verified
Linux and CLAP builds, GUI, Auto Makeup coupling from the GUI, automation smoothing in a host, mono layout, rates above 96 kHz.
