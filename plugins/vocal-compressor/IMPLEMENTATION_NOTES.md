# Implementation notes — Craft Vocal Compressor (spec r2)

Clean-room implementation (builder side) of [`spec/vocal-compressor/r2/SPEC.md`](../../spec/vocal-compressor/r2/SPEC.md)
(SHA-256 `c7e16167…5b0938ed`, approved 2026-10-29; supersedes r1 after the verification
[`docs/verification/vocal-compressor-r1.md`](../../docs/verification/vocal-compressor-r1.md)). Only the spec, `docs/process.md`, the repository's own code,
public documentation (JUCE / CLAP docs, textbook dynamics-processing material) and this implementation were used.
No original product, analyst workspace, vendor asset or vendor installation was accessed.

Product: **Craft Vocal Compressor** (`CVoc`, CLAP id `com.craftaudio.vocal-compressor`, bundle id `audio.craft.CVoc`);
manufacturer **Craft Audio** (`Crft`); version 0.1.0. Formats: VST3, CLAP, Standalone. Platforms: Windows x64
(MSVC), Linux x64 (GCC). One plug-in serves both variants: the host chooses **stereo** (2 in / 2 out, linked
detection) or **mono** (1 in / 1 out) through the bus layout.

## Architecture

All paths are relative to `plugins/vocal-compressor/`.

```
CMakeLists.txt     targets of this plugin (added automatically by the root CMakeLists.txt)
source/dsp/        host-independent, header-only DSP core (no JUCE)
  Parameters.h       §3 mappings r -> dB with 0.1 dB quantisation, gate "off" (D5), display / parsing, Settings
  Primitives.h       LookaheadConfig (§4.1/§4.5/D7 lengths per rate), SlidingMax, Boxcar, DelayLine, Smoother
  Gate.h             §4.2 gate detector (dB domain, instant attack, distance-dependent release S(D)), 1:2 gain
  ReleaseStage.h     §4.3/§4.4 compressor gain: fast path + program-dependent release (constants in ReleaseLaw)
  VocalCompressorEngine.h  complete per-sample processor: detection, gate, compressor, boxcars, makeup, output
source/plugin/     JUCE AudioProcessor + parameter layout
source/ui/         editor, look-and-feel, fader strips with meters, value boxes (all vector-drawn)
tests/             Catch2 unit tests of the DSP core (registered with CTest, prefix "vocal-compressor/")
tools/Snapshot.cpp optional dev tool rendering the editor to PNG (-DCRAFT_BUILD_SNAPSHOT=ON)
```

### Signal flow (per sample; newest input is `t`, aligned sample `n = t − H`)

1. Smoothed Compression, Gate threshold and Gain in dB (one-pole, τ = 5 ms, ≈ 23 ms to 99 %; snaps to the exact
   target, so settled values are bit-exact). `k = 10^(−Compression/20)`.
2. Linked level `a = max_c |x_c[t]|`; `A[n] = max(a[n … n+H])` — a monotonic-deque sliding maximum over the last
   `H + 1` inputs (`SlidingMax`, O(1) amortised).
3. Gate (§4.2): detector `L_d` in dB released at `S(D)` toward `20·log10 A[n]`, `gg = min(1, 10^((L_d − Gate)/20))`
   (gate off: `gg = 1`).
4. Compressor (§4.3/§4.4): `q[n] = ReleaseStage::process(A[n], k − 1)` — the fast path is
   `h[n] = 1/(1 + (k−1)·A[n]) = min(t[n … n+H])` (t is monotonic in a, so the forward min of t equals t of the
   forward max), combined with the program-dependent envelopes (below). Detection uses the ungated level (D4).
5. `s = q · gg`, two causal boxcars (38 + 27 samples at ≤ 50 kHz, 75 + 54 at ≤ 100 kHz, 150 + 108 above; `Boxcar`
   keeps a running sum that is re-summed exactly once per buffer cycle, so a window of ones is exactly 1).
6. `y[n] = x[n] · k · s2[n] · 0.99 · 10^(Gain/20)`, the same gain on both channels; `x[n]` comes out of an
   `H`-sample delay line, so the reported latency `H` time-aligns the output.

Internal arithmetic is double precision (64-bit host buffers are supported too). Bypass (host parameter or
`processBlockBypassed`) cross-fades in 10 ms to the latency-compensated dry signal.

## How each spec section is implemented

**§1 Scope / variants.** `isBusesLayoutSupported` accepts mono→mono and stereo→stereo; stereo detection uses
`max(|L|, |R|)` and one common gain (tested: both channels get identical gain). No limiter, no further
nonlinearity (D1).

**§3 Parameters.** Host-normalised value = spec `r`. The JUCE `NormalisableRange` lambdas apply the mapping and the
0.1 dB quantisation, so hosts and GUI only see step values. Compression and Gain: `−36 + 36 r` dB, default 0.
Gate: `r = 0` is **off** (shown "-Inf", D5; internally the bottom value −80 dB stands for off), any `r > 0` is
`−80 + 80 r` dB (the smallest step above off is −79.9 dB), default off. Typed text accepts numbers, "-inf"/"off".
Bypass is the host bypass parameter (`getBypassParameter`).

**§4.1 Latency.** `LookaheadConfig::forSampleRate`: fs ≤ 50 kHz → H = 64; ≤ 100 kHz → 128; above → 256 (D7).
`setLatencySamples(H)` in `prepareToPlay` (and a 44.1 kHz value in the constructor for hosts that ask early).
Tested: an impulse comes out exactly H samples later at 44.1/48/96/192 kHz.

**§4.2 Gate.** `GateDetector` implements the r2 law literally, in dB: `L_A = 20·log10 A[n]` (digital silence
→ −300 dB), `D = L_d[n−1] − L_A`; `D ≤ 0` → `L_d = L_A` (instant attack, opening anticipated by the look-ahead);
otherwise `L_d = L_A + max(0, D − S(D)/fs)` with `S(D) = 21.57 + 44.19·(1 − e^(−D/8.03))` dB/s. Gain
`gg = min(1, 10^((L_d − Gate)/20))` → 1:2 expansion, hard knee. With constant `A` the detector equals `L_A`, so the
static curve (§9.5) is exact. The detector keeps running while the gate is off, so switching it on is seamless; the
threshold glides from −140 dB when switched on and back when off, and `gg` is exactly 1 once "off" has settled.
Tested: every §9.6 / §9.6.1 point within ±0.75 dB and the §9.6.1 detector release times at 44.1/48/96 kHz.

**§4.3 Compressor target.** `t = 1/(1 + (k−1)·a)`; forward min over the look-ahead window = `t(A)`. Makeup `k` is
applied after the boxcars, so neutral settings give exactly `y = 0.99·x` (k = 1, all gains exactly 1).

**§4.4 Release.** The spec makes the tables normative (D3; r2: §9.3, §9.4, §9.7 and the ripple table §9.8).
`ReleaseStage` follows the informative structure (fast path + slower envelopes, gain `1/(1 + (k−1)·e)`),
generalised and fitted to all of them:

- All envelopes act on the "compression ratio" `z = 1 + (k−1)·level` (normalised gain `1/z`). Each is a one-pole
  that moves in a warped domain `u = (z^λ − 1)/λ` (λ = 1 amplitude, λ = 0 dB, λ = −1 linear gain, …) with its own
  time constant and warp for rising (attack) and falling (release) targets.
- **Program-dependent release time** (new in r2, §4.4 item 6): `τ = releaseMs · exp(dest·x_target/10 + own·x_own/10)`
  with `x_target = 20·log10(target z)` — the reduction the signal now asks for — and `x_own` the envelope's own
  reduction. A release toward a still strongly compressed level is slow, toward a lightly compressed one fast.
  `τ` is floored at 0.04 ms so the law does not depend on the sample rate.
- **Fast path** `z_h = 1 + (k−1)·A[n]`, per sample, instant.
- **Transient envelope**, per sample, follows `z_h` (attack ≈ 5 ms). Its destination-dependent release holds the
  reduction after loud passages when the level only drops moderately (e.g. 0 → −20 dBFS at Compression −36,
  §9.7), lets go within a few ms when the new level is only lightly compressed (short bursts inside −40 dBFS,
  §9.4), and holds the gain across the cycles of strongly compressed low-frequency tones (§9.8).
- **Charge / medium**: `charge` follows the level held over 5 ms (attack 84 ms, very slow release — it keeps its
  charge through pauses); `medium` follows `min(z_h, z_charge)` (attack 84 ms, fast destination-dependent release).
  So the extra reduction after a loud passage grows with the time spent loud, also across double bursts and burst
  trains (§9.7 `dbl_50` / `train_30` ≈ a single 100 ms burst, as in the reference).
- **Slow branch**: `slow1` follows the held level, `slow2` follows `slow1` (release ≈ 330 ms near the
  linear-gain domain) — the S-shaped release with a plateau that grows with |Compression| (§9.3).
- `q = 1/max(z_h, z_transient, z_medium, z_slow2)`. Exact constants: `ReleaseLaw` in `ReleaseStage.h`.
- Charge / medium / slow envelopes are updated at a ≈ 12 kHz control rate (each step sees the maximum level of its
  block); fast path and transient run per sample. Envelopes are stored as levels `(z − 1)/(k − 1)`, so a change of
  Compression rescales them consistently; with Compression 0 they track the level.
- CPU: a few % of one core for a 48 kHz stereo stream (the development machine was heavily shared during
  measurement: 4 % neutral / 12–16 % compressing at load average ≈ 66 on 22 threads, i.e. well below that unloaded).

Fit: least squares + Nelder–Mead on the production engine (the scripted §9.3/§9.4/§9.7 stimuli and the §9.8 tones,
errors normalised by the §8 tolerances), at 48 and 96 kHz jointly; the fitting scripts are not part of the product.
Worst point as a fraction of its tolerance: 0.99 (§9.4 Compression −36, B = 20 ms, t = 2 ms, 44.1 kHz), 0.97
(§9.7 Compression −24, `step_0_-20_50`); all §9.3/§9.4/§9.7 points and all §9.8 ripple limits pass at 44.1, 48 and
96 kHz in the unit tests.

**§4.5 Smoothing and output.** As in the signal flow; 0.99 is `VocalCompressorEngine::outputFactor` (D2).

**§6 Parameter changes.** One-pole smoothing in dB, τ = 5 ms (D6); tested: no gain step larger than 0.1 dB per
sample when all three parameters jump.

**§12 UI.** Original vector-drawn editor in the Craft house style (graphite panels; coral = compressor, violet =
gate, teal = signal/output). Left: **Gate** fader over an input energy meter (mean square, scale labelled 12 … 72 dB
below full scale, top = 0, "OFF" at the bottom), value box above ("-Inf" when off), input level readout below.
Centre: large **Compression** fader (0 … −36, marks every 6 dB) drawn inside the total gain-reduction meter
(compressor × gate, smoothed gain `s2`), peak-hold readout below (click to reset). Right: **Gain** fader (0 … −36)
over L/R output peak meters (one meter in mono), value above, peak readouts below (click to reset). Linking:
**Ctrl/Cmd-drag** on Compression or Gain moves the other parameter by the same number of dB (audition compression at
constant level). Double-click / Alt-click resets a control; clicking a value box lets you type a value, dragging it
adjusts (Shift = fine). Meters are display only. A footer shows the look-ahead in samples.

## Tests (Catch2, `ctest -R vocal-compressor/`)

| File | What |
|---|---|
| `PrimitiveTests.cpp` | sliding max vs brute force, boxcar exactness, lengths per sample rate |
| `ParameterTests.cpp` | mappings, quantisation, gate off (D5), display/parsing |
| `StaticTests.cpp` | latency H (impulse), neutral `|y − 0.99x| ≤ 1e-6` incl. +6 dBFS (D1), static curve §9.1 ±0.05 dB with/without Gain (gain and output peak), isolated samples §9.1, attack ramp §9.2 (r2 values, ±0.25 dB everywhere) at 44.1/48/96 kHz, linking, 50 Hz–12 kHz independence, smooth parameter changes |
| `GateTests.cpp` | §9.5 static ±0.2 dB, combined gains (+7.8 / −2.1 dB), gate off, §9.6 / §9.6.1 trajectories ±0.75 dB and §9.6.1 detector release times at 44.1/48/96 kHz, instant re-opening, 65.76 dB/s far above the level |
| `ReleaseTests.cpp` | all rows of §9.3, §9.4 and §9.7 within the §8 tolerance at 44.1/48/96 kHz, ms-based times, §9.8 ripple ≤ table + 0.15 dB at 44.1/48/96 kHz (2 s and 3 s tones), short transients |

Gains are measured exactly with a constant probe (1e-7) on the right channel, far below every test level.

## Questions / assumptions

- **A1 — Release law.** Not given in closed form (D3); implemented as described in §4.4 above and fitted to the
  r2 tables. Behaviour between and beyond the tabulated conditions (other levels, durations, programme material) is
  the model's interpolation.
- **A2 / A3 (r1 questions)** were answered by the verification and are resolved in r2: the gate uses the r2 S(D)
  law literally (no own corner rounding any more), and the §9.2 test uses the corrected r2 values with ±0.25 dB at
  every point.
- **A4 — 176.4/192 kHz.** H = 256, boxcars 150 + 108 (D7).
- **A5 — Parameter smoothing** (D6, not measured): τ = 5 ms one-pole in dB for Compression, Gate threshold and Gain.
  Switching the gate on/off glides the threshold from/to −140 dB.
- **A6 — Mono** variant = the same engine with one channel (open question 4).
- **A7 — Meters** (§7 unspecified): input energy = mean square of the linked input per GUI frame with a 30 dB/s
  fall; gain reduction = smallest smoothed gain `s2` (compressor × gate) per frame, 30 dB/s return, peak hold until
  clicked; output = sample peaks per channel, ≈ 27 dB/s fall, peak hold until clicked.
- **A8 — Gate value "off"** is represented by the bottom of the range (−80 dB ≙ r = 0); r > 0 never maps to off.
- **A9 — §9.8 tone length.** The spec measures "the last 0.5 s" of a steady −6 dBFS sine without giving its length.
  The slow components still charge during the first ≈ 1.5 s (as the reference does after a level rise, §9.4), so
  the gain still drifts in a 1–1.5 s tone; the tests use 2 s and 3 s tones (steady state). Question for the analyst:
  how long were the §9.8 tones?

## Status

Complete for r2: DSP core, plug-in (VST3, CLAP, Standalone; Windows x64 Release build verified, `craft_hoststress`
run against the VST3), editor, tests.
