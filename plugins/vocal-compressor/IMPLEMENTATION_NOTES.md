# Implementation notes — Craft Vocal Compressor (spec r1)

Clean-room implementation (builder side) of [`spec/vocal-compressor/r1/SPEC.md`](../../spec/vocal-compressor/r1/SPEC.md)
(SHA-256 `1cb05815…a21a4a16`, approved 2026-10-29). Only the spec, `docs/process.md`, the repository's own code,
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
  Gate.h             §4.2 gate detector (instant attack, 63 dB/s release) and 1:2 expander gain
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
3. Gate (§4.2): `d[n] = max(A[n], decay(d[n−1]))`, `gg = min(1, d/T)` (gate off: `gg = 1`).
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

**§4.2 Gate.** `GateDetector`: `d = max(A, decay(d))`, release `c_g = 10^(−63/(20 fs))` per sample, gain
`min(1, d/T)` → 1:2 expansion, hard knee, instant (look-ahead) opening. One addition (assumption A2): within the last
5 dB above the current level `A` the decay rate is scaled by the remaining distance (an exponential approach in dB),
which rounds the corner where a closing gate meets its final value. With constant `A` the detector equals `A`
exactly, so the static curve (§9.5) is unchanged. While the gate is off the detector keeps running (without the
soft corner) so switching it on is seamless; the threshold glides from −140 dB when switched on and back when off,
and `gg` is exactly 1 once "off" has settled.

**§4.3 Compressor target.** `t = 1/(1 + (k−1)·a)`; forward min over the look-ahead window = `t(A)`. Makeup `k` is
applied after the boxcars, so neutral settings give exactly `y = 0.99·x` (k = 1, all gains exactly 1).

**§4.4 Release.** The spec makes the tables normative (D3). `ReleaseStage` follows the informative structure
(fast path + slower envelopes of the held level, gain `1/(1 + (k−1)·e)`), generalised and fitted to all 36 rows
of §9.3/§9.4:

- All envelopes act on the "compression ratio" `z = 1 + (k−1)·level` (normalised gain `1/z`). Each is a one-pole
  that moves in a warped domain `u = (z^λ − 1)/λ` (λ = 1 amplitude, λ = 0 dB, λ = −1 linear gain) with its own time
  constant and warp for rising (attack) and falling (release) targets.
- **Fast path** `z_h = 1 + (k−1)·A[n]` per sample: instant attack and release, so short transients recover as soon
  as the look-ahead smoothing allows (§4.4 item 1).
- **Medium branch**: a "charge" envelope follows `z_h` (attack 102 ms, ≈ dB domain, instant release), a "medium"
  envelope follows the charge envelope (attack 104 ms, release 26.7 ms in the amplitude domain). The two-pole
  charge is S-shaped in time, so bursts of 2–20 ms leave (almost) nothing, while 50–200 ms bursts at strong
  compression leave a reduction that decays within tens of ms (table 9.4, first columns).
- **Slow branch**: `slow1` follows the level held over 12 ms (attack 192 ms, release 52.7 ms with warp −1.98, i.e.
  slow while the reduction is large — the plateau that grows with |Compression|), `slow2` follows `slow1` (attack
  149 ms, release 420 ms with warp −0.64, close to the "≈ 0.45 s in the linear-gain domain" tail of §4.4).
- `q = 1/max(z_h, z_medium, z_slow2)`.
- The 12 ms hold is longer than half a 50 Hz period minus the look-ahead window, so a steady 50 Hz tone gives a
  constant slow target, which converges to exactly the peak (static) gain (§4.4 item 4; tested ≤ 0.2 dB ripple).
- The envelopes are updated at a ≈ 6 kHz control rate (every `round(fs/6000)` samples, each step sees the maximum
  level of its block); all their time constants are ≥ 26 ms. The fast path stays per sample. Time constants are in
  milliseconds, so the behaviour is the same at 44.1/48/96 kHz (§4.4 item 5; tested within 0.1 dB / 0.2 dB).
- Envelopes are stored as levels `(z − 1)/(k − 1)`, so a change of Compression rescales them consistently; with
  Compression 0 they track the level.
- CPU: ≈ 3 % of one core for a 48 kHz stereo stream on the development machine (x64, -O2).

Fit quality (worst point, as a fraction of the §8 tolerance "±1.5 dB or ±25 %, whichever is larger"):
§9.3 0.69, §9.4 0.76 at 48 kHz (0.79 at 44.1 kHz, 0.77 at 96 kHz). The worst point is Compression −36, B = 100 ms,
t = 10 ms (5.96 dB vs 4.82 dB). The fit used the scripted test signals of §9.3/§9.4 (1 kHz sines, burst inside a
−40 dBFS tone) and Nelder–Mead on the tolerance-normalised errors; the fitting script is not part of the product.

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
| `StaticTests.cpp` | latency H (impulse), neutral `|y − 0.99x| ≤ 1e-6` incl. +6 dBFS (D1), static curve §9.1 ±0.05 dB with/without Gain (gain and output peak), isolated samples §9.1, attack ramp §9.2 at 44.1/48/96 kHz, linking, 50 Hz–12 kHz independence, smooth parameter changes |
| `GateTests.cpp` | §9.5 static ±0.2 dB, combined gains (+7.8 / −2.1 dB), gate off, §9.6 trajectory ±1.5 dB, instant re-opening, 63 dB/s |
| `ReleaseTests.cpp` | all 36 rows of §9.3/§9.4 within the §8 tolerance, ms-based times at 44.1/96 kHz, 50 Hz ripple ≤ 0.2 dB, short transients |

Gains are measured exactly with a constant probe (1e-7) on the right channel, far below every test level.

## Questions / assumptions

- **A1 — Release law.** Not given in closed form (D3); implemented as described in §4.4 above and fitted to the
  tables. Behaviour between and beyond the tabulated conditions (other levels, other burst lengths, programme
  material) is the model's interpolation.
- **A2 — Gate closing corner (§9.6).** The literal §4.2 law (constant 63 dB/s down to the floor) reaches the floor
  at 793 ms in the §9.6 test and gives −50.0 dB at 800 ms where the table has −47.52 (−27.52, −7.52 for the other
  rows): 2.5 dB, outside the ±1.5 dB tolerance. The table shows a rounded corner (−43.55 / −47.52 / −50.01 at
  700/800/900 ms). The soft corner described above (last 5 dB) keeps every §9.6 point within 1.23 dB, keeps the
  63 dB/s slope and leaves the static curve untouched. Question for the analyst: is the corner real or a
  measurement artefact at the −120 dBFS output level?
- **A3 — Attack ramp at n = −1 / 0 (§9.2).** The literal §4.3 hold + §4.5 boxcars reproduce every tabulated
  point to ≤ 0.01 dB when the 1 kHz step starts at a zero crossing (phase-continuous sine), except n = −1
  (0.65 vs 0.99 dB) and n = 0 (0.25 vs 0.40 dB) for Compression −36. The input sample there is 0 (n = 0) or
  0.0013 (n = −1), so the tabulated output/input ratio is ill-conditioned; the test allows 0.4 dB at these two
  points, 0.25 dB elsewhere. Question for the analyst: how were those two values measured?
- **A4 — 176.4/192 kHz.** H = 256, boxcars 150 + 108 (D7).
- **A5 — Parameter smoothing** (D6, not measured): τ = 5 ms one-pole in dB for Compression, Gate threshold and Gain.
  Switching the gate on/off glides the threshold from/to −140 dB.
- **A6 — Mono** variant = the same engine with one channel (open question 4).
- **A7 — Meters** (§7 unspecified): input energy = mean square of the linked input per GUI frame with a 30 dB/s
  fall; gain reduction = smallest smoothed gain `s2` (compressor × gate) per frame, 30 dB/s return, peak hold until
  clicked; output = sample peaks per channel, ≈ 27 dB/s fall, peak hold until clicked.
- **A8 — Gate value "off"** is represented by the bottom of the range (−80 dB ≙ r = 0); r > 0 never maps to off.

## Status

Complete: DSP core, plug-in (VST3, CLAP, Standalone; Windows x64 Release build verified), editor, tests.
