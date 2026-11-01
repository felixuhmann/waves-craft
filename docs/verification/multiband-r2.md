# Independent verification — multiband r2 (Craft Multiband)

Performed by the verifier (analyst side), per step 5 of [docs/process.md](../process.md). Differences only.
Build: `feat/multiband` @ 72dca0f, Release VST3, REAPER 7.83, offline, 32-bit float, 48 kHz. Reference renders were reused
where stimulus and settings were identical. The earlier report is [multiband-r1.md](multiband-r1.md).

| Area | Result |
|---|---|
| §5.3 release after overloads (250 points) | Pass: 0 fails, worst 0.91 × tolerance, RMS deviation 0.36 dB (r1 build: 47 fails, 1.27 dB). |
| §5 attack table (90 points) | Pass. One point (Range +12 @ 1 ms) is outside tolerance for the build and the reference alike (measurement window). |
| §5 release table (112 points) | Pass under r2's start-value rule. Least margin: Opto, Release 200–500 ms @ 50–100 ms, 1.0–1.4 dB more reduction than the table. |
| §5.2 30 ms burst table (70 points) | Pass: worst 0.9 × tolerance; Electro at Release 1000 ms @ +20 ms is 1.2 dB short of the table. |
| Noise vector §6 | Pass at both timings: max deviation 0.54 dB (fast), 0.70 dB (slow), limit 0.75 dB. Build gives less reduction than the vector at low levels. |
| Stereo link | Pass (−10.16 / +10.43 dB on both channels). |
| Crossover | Pass: ≤ −130 dB vs the §3 model, latency 0, host bypass bit-exact. |
| Program material (drum loop, 3 settings) | Level envelope 0.5–0.8 dB RMS, max 2.0 dB (r1 build: 0.6–0.9 / 2.3 dB). |
| Non-table settings | Envelope 0.07–0.38 dB RMS overall (r1 build: up to 1.18 dB); see the short-burst note below. |

## Output clipping
The reference has no output clip stage: at Output or band Gain +18 dB it delivers peaks up to +19.1 dBFS, and a 0 dBFS sine at
neutral settings comes out at +1.1 dBFS (crossover allpass). The build matches: +19.1 dBFS / +1.1 dBFS, and within 0.05 dB at
Range +18. No spec change is needed. Very high output levels at extreme settings are the specified behaviour, so the build must
**not** clip at ±1.0, and host-stress level flags above +40 dBFS follow from gain stacking (up to +54 dB), not from a defect.

## Remaining differences (none violates the spec)
- Noise vector at slow settings uses 0.70 of 0.75 dB.
- 6–9 ms after a 5 ms overload ends, the build holds up to 10.9 dB more reduction than the reference (Range −18, Hard, Release 300, Auto; 7 dB at Range −12 / −18 elsewhere); converged within tolerance by the first §5.3 point at +20 ms. Not constrained by the spec; affects only the signal immediately after very short bursts.
- Drum-loop Electro settings: build about 0.3–0.6 dB more reduction on average.

## Not verified
Linux and CLAP builds, Mono layout, automation in a host, crossed crossovers, 44.1 / 96 kHz re-renders of this build (the builder's
tests cover them; the previous build matched), GUI.
