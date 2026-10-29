# Independent verification — spec doubler r1 (Craft Doubler 4 / Craft Doubler 2)

Performed 2026-10-29 by the verifier (analyst side), per step 5 of [docs/process.md](../process.md).
Only observed differences are reported here; no reference-side implementation detail is passed to the builder.

## Method
- Host: REAPER 7.83 (x64, Windows), unattended offline render, 32-bit float, at 48 kHz (full grid) and
  44.1 / 96 kHz (subset).
- Build under test: `feat/doubler` @ ecf61b3, Release VST3 from the local MSVC build.
- Stimulus (user-generated): impulse on L, impulse on R, 6 s white noise on L, 4 s sines at 1 kHz / 100 Hz / 5 kHz on L.
- 49 configurations for Craft Doubler 4: direct path, voice pan/delay/Range/Align, input routing, feedback with two
  high-pass settings, all seven §11.2 EQ rows, detune ±1/±10/±50/±100 cents, octave, octave + 50, Range 20 Hz and
  modulation. 8 configurations for Craft Doubler 2, and 13 per extra sample rate. Controls were set by their
  **displayed values**.
- The same grid was rendered through the reference for the statistical comparisons.

## Results
### Deterministic paths (§11.1, §11.2) — build vs. spec r1 model
| Area | Configs | Worst | Notes |
|---|---:|---:|---|
| Direct gain/pan/Align, voice pan, routing A/B, integer voice delays, Range 80/20, Align (Dn = Delay + Lr) | 26 | −146 dB re peak | bit-exact where no arithmetic is involved |
| Feedback 60 % (high-pass 16 Hz and 578 Hz) | 2 | −147 dB | |
| EQ table rows (§11.2) | 7 | −146 dB | |
| Fractional voice delay (9.4 ms) | 1 per rate | −89.9 dB (48 kHz), −89.5 dB (44.1 kHz), −81.3 dB (96 kHz) | see "Differences" 1 |
| Craft Doubler 2 subset | 5 | −148 dB (fractional delay −89.9 dB) | identical to the 4-voice results |
| 44.1 / 96 kHz subset (Align, Range 20, EQ, feedback) | 9 per rate | −145 dB | inherent latency and window scale with the sample rate (decision D2) |

Review decisions confirmed:
- **D1:** no Align offset at detune 0. The reference shows the offset; the build follows the spec.
- **D2:** latency and window scale with fs.
- **D3:** modulation period is 1/Rate. Measured 1.000 s at Rate 1.0 at all three rates.

### Pitch shifter (§11.3) — build, with the reference in brackets (48 kHz)
| Check | Limit | Build | [Reference] |
|---|---|---|---|
| Pitch error, ±1…±100 cents (1 kHz, 100 Hz, 5 kHz) | ≤ 0.5 cents | ≤ 0.02 cents | [≤ 0.43] |
| Octave / octave + 50 | −1200 / −1150 ± 6 cents | −1200.0 / −1150.0 | [−1204.0 / −1153.8] |
| Sine envelope 1 % percentile, 1 kHz / 100 Hz | ≥ 0.97 | ≥ 0.990 | [≥ 0.983] |
| Sine envelope 1 % percentile, 5 kHz, +50 cents | ≥ 0.90 | 0.946 | [0.946] |
| White-noise level | −1.8 ± 0.6 dB | −1.76 … −2.08 dB | [−1.80 … −1.88] |
| ⅓-octave band deviation ±10…±100 cents, vs. broadband gain | ≤ 3 dB | ≤ 2.31 dB | [≤ 2.46] |
| same, vs. mean of band gains | ≤ 3 dB | ≤ 2.09 dB | [≤ 2.10] |
| Delay range between splices (Dn = 20 ms, window ±6.37 ms, ±0.5 ms) | 13.1 … 26.9 ms | 13.69 … 26.40 ms | [14.04 … 25.98] |
| Splice jump size | 4.6 … 12.1 ms | 5.65 … 11.02 ms | [5.25 … 10.27] |
| Mean delay ±50 cents | Dn ∓ 2.2 ± 0.7 ms | 17.97 / 21.91 ms | [17.88 / 21.92] |
| 44.1 / 96 kHz: pitch, envelope, noise level | as above | pass (5 kHz envelope 0.936 / 0.986) | |

### Modulation (§11.4)
- **Depth +100, Rate 1.0:** a triangle between −98 and +98 cents (with 20 ms analysis smoothing), period 1.000 s.
- **Depth −100:** mirrors the Depth +100 trajectory. The largest difference is 5.6 cents, at splice points.
- **Detune +20:** offsets the trajectory by +19.97 cents on average.

## Differences found
1. **Fractional voice delay precision.** At delays that aren't a whole number of samples, the interpolation weights
   are off by about 2·10⁻⁵ (impulse taps 0.80002 / 0.19998 instead of 0.8 / 0.2).
   - The error is constant over time, with no drift: −90 dB re peak at 44.1/48 kHz and −82 dB at 96 kHz.
   - It is inaudible, but it misses the −135 dB deterministic-path accuracy the spec's model achieves.
   - It is consistent with computing the delay in single precision. Minor.
2. **Crossfade overlap time.** At small detune the build mixes old and new taps for slightly longer than the
   reference. At −10 cents, 19 % of windows correlate below 0.8 with a single tap, against 16 % for the reference.
   All statistical limits pass.
3. **Spec wording, pitch shifter** (raised by the builder).
   - The reference's noise loss (−1.8 dB) matches what linear interpolation alone produces, and its taps are mixed
     only around splices. "Continuous two-tap overlap" in §6 is therefore misleading.
   - The builder's choice (linear reads plus a short crossfade at the window edge) matches the reference statistics.
     A wording fix is proposed for a future revision.
   - Band deviation passes under both definitions. The proposed revision measures it against broadband gain,
     consistent with the noise-level check.

No other non-conformances found.

## Not verified
- Mono→stereo bus layout and CLAP hosting.
- Parameter-change dynamics (§11.5) and the delay-change crossfade.
- Modulation Reset and LFO start phase in a host.
- Delay window and jumps at Range 20 Hz; only pitch and mean-delay statistics were checked there.
- Feedback with detune.
- Linux build (left to CI) and a GUI review.

One REAPER session exited early (rc=1) during the first 48 kHz run. Re-rendering the remaining 24 configurations
completed normally, so this could not be reproduced as a plugin crash.
