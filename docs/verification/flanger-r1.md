# Independent verification — flanger spec r1 (Craft Flanger / Mono / Mono-Stereo)

Performed 2026-10-29 by the verifier (analyst side), per step 5 of [docs/process.md](../process.md).
Only observed differences are reported here; no reference-side implementation detail is passed to the builder.

## Method
- Host: REAPER 7.83 (x64, Windows), unattended, offline render, 32-bit float WAV.
- Build under test: `feat/flanger` @ 4740cb1, Release VST3 from the builder's local MSVC build
  (`Craft Flanger`, `Craft Flanger Mono`, `Craft Flanger Mono-Stereo`).
- Stimulus: user-generated independent white noise per channel (σ = 0.1), starting at the first sample of the render
  (the spec's LFO phase reference is the render start).
- Controls were set by their **displayed values** (midpoint of the normalised range that shows the target text),
  independent of the build's internal parameter mapping.
- (a) Build vs. the approved spec r1 model: 2 s of output per configuration.
- (b) Build vs. the reference: the reference's own raw-exact validation renders were re-run through the build with the
  same stimulus and identical settings; 2 s of signal compared.

## Results

### (a) Build vs. spec r1 model (error re output peak; limits from spec §8)
| Sample rate | Static configs (limit −100 dB) | Modulated configs (limit −60 dB) |
|---|---|---|
| 48 kHz | 20/20 pass, worst −145.4 dB | 13/13 pass, worst −146.8 dB |
| 44.1 kHz | 6/6 pass, worst −145.9 dB | 5/5 pass, worst −148.4 dB |
| 96 kHz | 6/6 pass, worst −146.2 dB | 5/5 pass, worst −148.9 dB |

Coverage at 48 kHz: mix law, feedback 0–100 % (including the 0.999 loop cap), both polarity toggles, output gain,
delay 0.1–50 ms, tape mode (static and modulated), low-/high-pass filter at 1 k / 2 k / 5 k / 10 kHz including
filter-with-feedback, Stop with stereo offset, sine and triangle LFO at 0.37–13.5 Hz, stereo offsets 30/90/180°,
tempo sync at 120 BPM, a 50 ms deep sweep, and the Mono and Mono-Stereo variants. 44.1/96 kHz: representative subset.

### (b) Build vs. reference (2 s of noise, raw-exact settings, 7 configurations incl. tape, triangle, filters, polarity)
| Sample rate | Range | Same comparison for the spec model vs. reference |
|---|---|---|
| 44.1 kHz | −49.0 … −73.1 dB | identical to the build's figures (build vs. model ≤ −147 dB) |
| 48 kHz | −47.4 … −82.0 dB | identical |
| 96 kHz | −41.2 … −65.4 dB | identical |

The remaining build-vs-reference difference equals the spec model's own difference from the reference to 0.1 dB in every
case, i.e. it is the reference's LFO trajectory jitter that spec r1 states (≈ 0.05 sample) and does not ask to
reproduce. On static settings the two agree to float precision.

### Differences by design (spec r1 review decisions)
Not exercised against the reference because the spec deliberately deviates: parameter smoothing (§7), Link coupling
(§5.5), internal Delay always equal to the displayed value (§11.3), no internal clipping (§11.4).

## Observations (not acceptance-critical)
- Display formats omit units: Delay "2.2", Depth/Mix/Feedback "50", Output Gain "0.0" (spec §2 lists "2.2 ms", "50 %",
  "0.0 dB"). Rate and Filter Freq formats match ("0.20 Hz", "1.0k").
- As in the spec, Rate is stored in 0.005 Hz steps but displayed with 0.01 Hz resolution below 10 Hz, so two adjacent
  internal values can show the same text (e.g. 0.365 and 0.370 both read "0.37 Hz").
- All three variants load in REAPER with the expected I/O (2/2, 1/1, 1/2) and spec defaults.

## Not verified
Linux builds and CLAP hosting (CI only); automation/smoothing behaviour in a host; Link and Stop toggled during
playback; LFO restart on a second playback; tempo sync at tempos other than 120 BPM; 88.2/176.4/192 kHz host renders;
GUI review.
