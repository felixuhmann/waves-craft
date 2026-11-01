# Independent verification — spec peak-limiter r1 (Craft Peak Limiter)

Performed 2026-10-29 by the verifier (analyst side), per step 5 of [docs/process.md](../process.md).
Only observed differences are reported; no reference-side implementation detail is passed to the builder.

## Method
- Host: REAPER 7.83 x64 (Windows), unattended offline render, 32-bit float, plugin delay compensation on.
- Build under test: `feat/peak-limiter` @ 5daa9c7, Release VST3 from the builder's local MSVC build.
- Stimuli (user-generated): DC carrier with clicks/bursts of varying width and height, click pairs, Gaussian noise
  bursts, sine bursts (50 Hz–5 kHz), DC overload steps (5 ms–2 s, 6/12/24 dB over), 1 kHz sine level staircase,
  fs/4 sine (inter-sample peaks), synthetic program material.
- 98 configurations at 48 kHz plus 5 each at 44.1 and 96 kHz; controls set by known raw values (the build's
  parameter mappings equal spec §2, confirmed by a display-taper probe).
- Comparison (a): the spec §4 model with decision D1, after latency alignment, excluding the first L samples.
  Comparison (b): the reference processor rendered with identical stimuli and settings.

## Results
| Area | Result |
|---|---|
| Latency / PDC | Pass — reported latency equals actual latency (64 samples at 44.1/48 kHz, 128 at 96 kHz); neutral impulse output bit-identical to input after host compensation |
| Static curve, attack, fixed release, click pairs, linking, noise/sine bursts, program (Auto Release off) | Pass — build vs spec model worst −147.5 dB re peak (48 kHz, 75 configs), −171.6 dB (44.1 kHz), −151.0 dB (96 kHz); limit −100 dB |
| Ceiling guarantee | Pass — output never exceeds the ceiling (max overshoot 0 in all configs, incl. Auto Release) |
| Neutral / inter-sample test | Pass — bit-exact at Threshold = Ceiling = 0 with no overs; fs/4 sine with 0.707 sample peaks passes unchanged |
| Auto Release table (§5, 15 rows × t63/t90/t99) | Pass — all within ±20 %; worst +15.7 % (t63, Release 1.00 ms / 500 ms row) |
| Auto Release recovery | Monotonic in all measured cases |

### Differences versus the reference
- **By design (D1):** the reference exceeds the ceiling on the peak/last over-threshold sample of an overload
  (relative +2e-6 … +2.9e-3, larger for faster Release); the build clamps those samples to the ceiling. These are
  the only samples that differ in fixed-release mode (1–5 samples per stimulus; worst −50.8 dB re peak at
  Release 0.01 ms, −95.8 dB at 10 ms).
- **Reference rounding:** at Threshold −24 dB the reference shows a ≈ 3.6e-5 relative gain offset on a sine
  staircase (−88.8 dB re peak); the build follows the specified exact gain.
- **Auto Release:** the law is not specified (spec open question 1); the build's recovery differs from the
  reference's in shape (sample-wise −47 … −8 dB re peak on overload steps and program) while meeting the table.
  Measured with the same method on both: after long overloads the build recovers 5–10 % faster than the
  reference; after 5–20 ms overloads it is up to 12 % slower at t63/t99 for Release 31.62 ms.
- **D2–D4:** stream-start behaviour, default Release (1.00 ms) and single Threshold/Ceiling controls — as specified.

## Not verified
Linux build (CI only), CLAP hosting, 88.2/176.4/192 kHz, mono layout in a host, Threshold/Ceiling automation
smoothing in a host, host bypass crossfade, GUI.
