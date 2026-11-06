# Implementation notes — Craft Punch Compressor (spec r2)

Clean-room implementation (builder side) of [`spec/punch-compressor/r2/SPEC.md`](../../spec/punch-compressor/r2/SPEC.md)
(approved 2026-11-01, supersedes r1; verification of the r1 build: `docs/verification/punch-compressor-r1.md`). Only the spec, `docs/process.md`, the repository's own code,
public documentation / textbook DSP and this implementation were used. No original product, analyst workspace,
vendor asset or vendor installation was accessed.

Product: **Craft Punch Compressor** (`CPcm`, CLAP id `com.craftaudio.punch-compressor`, bundle id `audio.craft.CPcm`);
manufacturer **Craft Audio** (`Crft`); version 0.1.0. Formats: VST3, CLAP, Standalone. Platforms: Windows x64 (MSVC),
Linux x64 (GCC). One plug-in serves both spec variants: **stereo** (2 → 2, one linked detector) and **mono** (1 → 1)
through the host's bus layout (decision D4).

## Architecture

All paths are relative to `plugins/punch-compressor/`.

```
CMakeLists.txt       targets of this plugin (added automatically by the root CMakeLists.txt)
source/dsp/          host-independent, header-only DSP core (no JUCE)
  Parameters.h         §2 maps r -> value (quantised to the display resolution), inverse maps, note table, Settings
  StaticCurve.h        §4 correction table, combination rule, gain reduction; §5 makeup k(R), s(Rel)
  Primitives.h         look-ahead H per rate, delay lines, sliding max/min, boxcar, smoother, RBJ biquads, noise
  GainSmoother.h       §7 attack (one-pole, linear gain) and the program-dependent release (two stages)
  Analog.h             §10 analog modes 1-4 (fitted constants)
  OutputStage.h        §11 look-ahead peak stage (ceiling 1.0)
  PunchEngine.h        complete per-sample engine (§3 signal flow), parameter smoothing, bypass, meters
source/plugin/       JUCE AudioProcessor + parameter layout (host tempo from the play head, latency reporting)
source/ui/           editor, look-and-feel, knobs, value boxes, segmented selectors, needle meter (vector-drawn)
tests/               Catch2 tests of the DSP core (CTest prefix "punch-compressor/")
tools/Snapshot.cpp   optional dev tool rendering the editor to PNG (-DCRAFT_BUILD_SNAPSHOT=ON)
tools/fit/           offline fitting scripts used to derive the release-law and analog-mode constants
```

### Signal flow (per sample, real time `t`; aligned index `n = t − H`)

1. Linked detector input `a = max(|L[t]|, |R[t]|)`; sliding maximum over the newest H+1 samples gives
   `p[n] = max_{k=0..H} a[n+k]` (§3.1); `D = 20 log10 p`.
2. Punch: `D` passes a variable delay of `round(P · fs / 1000)` samples (§7.3).
3. Static curve `GR(D; T, R)` with the smoothed threshold and `u = 1 − 1/R` (§4) → target gain `10^(−GR/20)`.
4. `GainSmoother` (attack / release, §7) → gain-reduction gain `g`.
5. Makeup `Mk(T, R, Rel)` (§5, smoothed in dB): `gc = g · 10^(Mk/20)`.
6. Mix with the H-delayed dry sample: `y1 = x + m (x·gc − x)` (exact when `gc = 1`) (§8).
7. Output gain (smoothed in dB) (§9).
8. Analog mode (+ its noise) (§10).
9. Output peak stage, gain from a predicted copy of the chain (§11), final clamp to ±1.0 (D2).

Latency: H = 64 (44.1/48 kHz), 128 (88.2/96 kHz), 256 (176.4/192 kHz, extrapolated), reported via
`setLatencySamples` in `prepareToPlay` (and 64 before the first prepare). Tail = H / fs.

## How each spec section is implemented

| Spec | Implementation |
|---|---|
| §2 parameters | `Parameters.h` (`param::Threshold … Tempo`), `ParameterLayout.cpp`. Host value = spec r; `convertFrom0to1` applies the spec map and quantises to the display resolution (0.1 dB, 2-decimal ratio, 3 significant digits for Attack / Release, 0.1 for Mix / Punch). Ratio: 41-point table, log R linear in r. Release Sync / Analog / Meter are JUCE choice parameters whose index rounding equals the spec thresholds (0.25/0.75, 0.125/0.375/…). Release note: index `floor(40 r)` (spec: value for r ∈ [k/40, (k+1)/40)). Limiter: bool ("Clipper" / "Limiter"). |
| §2 sync | `Settings::effectiveReleaseMs()`: ms mode → Release; Host → note at the host tempo; BPM → note at the manual tempo, or the host tempo when the manual tempo is Off. Note duration = beats · 60000 / BPM (T ×2/3, D ×3/2, 1B = 4 beats). The effective value drives the release law **and** the makeup s(Rel). |
| §3 detector / latency | `SlidingMax` (monotonic deque, O(1)) over H+1 samples; audio `DelayLine` of H; `lookaheadForSampleRate`. |
| §3 neutral identity | Ratio 1 ⇒ u = 0 ⇒ GR ≡ 0 and Mk ≡ 0 ⇒ `gc` is exactly 1.0; mix written as `x + m (x·gc − x)`; Output 0 ⇒ factor exactly 1.0; Analog Off returns its input; the peak stage's gain is exactly 1.0 below 0 dBFS (constant boxcar windows return their value exactly). Smoothers snap to their exact targets. Tested bit-exact for double and float at 44.1/48/96 kHz, any Mix / Punch / Threshold, mono and stereo. |
| §4 static curve | `StaticCurve.h`: `delta4At` (Ratio-4 rows, linear in d and T, T clamped to −6…−48), `delta24At` (R ≥ 2: the R-4 row; R < 2: rows 1.25/1.5/2 linear in u), `delta` (normative combination rule; 0 below d = −2, the d = 24 value above), `gainReduction`: d ≤ 0 → max(0, Δ) (knee), d > 0 → d·u + Δ, clamped ≥ 0. |
| §5 makeup | `makeupK` (linear in u, k(1) = 0), `makeupS` (linear in log10 Rel, clamped), `makeupDb`. Examples reproduced (+5.30 / +19.05 / 0 dB). |
| §6 detector vectors | Consequences of §3.1 + the release law; its sustained fast release was fitted to them (below). |
| §7.1 attack | One-pole on linear gain, τ = Attack. |
| §7.2 / §7.4 / §7.5 release | Three-stage program-dependent law (next section). |
| §7.3 punch | Side-chain (detector level) delay of P ms: onset and release start both move by P (tested ±0.5 ms). |
| §8 mix / §9 output | Linear, sample-aligned crossfade; exact dB gain. Mix smoothed linearly, Output and makeup in dB (τ 3.5 ms). |
| §10 analog | `Analog.h` (section "Analog fit" below). |
| §11 output stage | `OutputStage.h` + prediction in `PunchEngine` (section "Output peak stage"). |
| §12 sample rates | All times in ms; H and the output-stage ramp lengths scale with the rate; filters are pre-warped RBJ biquads. |
| §14 UI / D5 | See "GUI"; all continuous parameters are smoothed (click-free, settled within 20 ms). |

## Release law (spec r2 §7.2 / §7.4 / §7.5 / §6, open question 1)

The r1 build's two-stage fit met the §7.4 tables but failed after short overloads. §7.5 shows: a burst up to about
10 ms recovers in about a millisecond at any Release; after longer bursts a medium part recovers in about 15 ms;
the slow part charges in about 100 ms regardless of Release and then recovers on its own, depending only on its
current depth and on Release. `GainSmoother` (all on the linear gain-reduction gain; applied gain = `min(gA, gM)`):

* **gA**: attack = one-pole, τ = Attack (normative). Release = one-pole, τ = `min(1.11, 0.2 Rel)` ms for short
  overloads, growing with a programme state `p` (rises with τ 40 ms during reduction, falls with τ 5 ms otherwise) to
  `min(11.5 (Rel/100)^0.75, 0.224 Rel)` as `p³`: this gives the §6 LF ripple and noise-vs-sine values under sustained
  programme without slowing the recovery after a burst.
* **Gate**: accumulates reduction time weighted by depth (`(1 - t) / 0.775`, so deeper opens sooner), falls 3× as
  fast while the target is more than 0.17 dB above gA; open after `1.231 · hold(Rel)`, hold 34 … 7.8 ms for Rel 3 … 3000.
* **gM**: gate open: charges to gA (τ 3.6 ms). Otherwise relaxes to gS, τ = `min(12.5 + 4 Rel/3000, max(min(2.5, Rel/4), 0.135 Rel))`.
* **gS**: gate open: charges to gM (τ 22 ms up to 40 % of gM's reduction, then 142 ms). Otherwise recovers towards
  `(K t)^γ` in `z = g^γ` with γ = 1.1, K = 30 (nearly a straight ramp), τ = a(Rel)·Rel, a = 150 / 124.7 / 110.2 / 101.4 /
  93.3 / 100.7 / 99.7 at 3 / 10 / 30 / 100 / 300 / 1000 / 3000 ms (`tools/fit/relfit.py`, 1000 ms rows), and 3× faster
  above gain 0.72 (the last 3 dB, as in the table). Repeated peaks with the gate closed charge gS with τ 300 ms.

Constants: `tools/fit/opt.py` + `tune.cpp` (limited Nelder-Mead) and manual trims. Results at 48 kHz (limits in
brackets): §7.5 max error 1.50 dB (1.5), first 5 ms 2.48 dB (3), RMS per Release ≤ 0.52 dB; §7.4 release max
1.93 dB, within the first 5 ms (3), ≤ 1.29 otherwise; noise vs sine −0.35 … −0.48 dB (±0.5); LF ripple max 3.8 dB (4);
100 Hz levels −0.59 / +0.26 / −0.56 dB (1), H3 −1.5 / +3.9 / −2.4 dB (4). **Margins are thin** on §7.5, noise vs sine
and the 100 Hz, a = 0.3 H3: noise vs sine has a systematic negative offset that trades against that H3. The tests use
one Gaussian noise; another noise realisation can move noise vs sine by a few hundredths of a dB.

## Analog fit (spec §10)

Chain per mode and channel (`Analog.h`): gain → low shelf → peaking → high shelf → band split `b = BP(u)`,
`y = u − b + φ(b)` with the asymmetric soft dead zone `φ(b) = b − a± t± tanh(b / t±) + g3 b|b| + g2 b²` →
`y + b2 y² + b3 y|y|` → crossover `y − xo tanh(y / xk)` → minus a 1.5 ms moving average of the even-order part → 0.5 Hz DC blocker → + shaped noise. Unused stages are
skipped. Mode 1: shelves + 2nd/3rd-order terms (level-proportional H2/H3). Mode 2: broad −0.85 dB peaking dip and an
expanding band around 1.75 kHz (H3 at 1 kHz). Mode 3: an asymmetric dead zone on a broad band around 2.1 kHz: about
−5 dB at low level, opening towards full scale, strong H2/H3 – this also produces the post-Output level dependence.
**Hot-to-quiet (r2):** DC came from the even-order terms and lingered in the old 2 Hz blocker. The chain is now
evaluated at ±input to get its exact even part, whose moving average over 1.5 ms is subtracted: a finite tail, with
H2 of 1 kHz unchanged (null at 2 kHz). The blocker moved to 0.5 Hz (its own tail after a hot sine scales with its corner),
and Mode 3 uses a low-Q shelf instead of a ringing one. **Noise (r2):** white noise through six shelves stepping
between the §10 band densities, level set from the chain at 48 kHz so absolute density is rate independent; an
extra shelf above 30 kHz at high rates.
Mode 4: tiny crossover step (constant −67 dBFS H3) and a slight presence lift / top-octave notch.

Constants were fitted with `tools/fit/fitm.py` (scipy least squares on an emulation of the §10 measurements at
48 kHz; Mode 4 jointly at 44.1/48/96 kHz with a "no transient above input" constraint). Polynomial terms use the
signal clamped to ±2 (+6 dBFS) so hot signals (Output +18 dB) stay monotone. Engine results (tests): magnitude
within 0.25 dB of the table (worst: Mode 3 at 20 Hz), fundamentals within 0.11 dB, harmonics within 3.5 dB, clicks
within 0.012 of the table and never above the input, Mode 3 level points −4.97 / −4.79 dB (table −5.1 / −4.75),
noise floors exact (white, uncorrelated L/R). Across rates the 100 Hz / 1 kHz levels agree within 0.05 dB; above
10 kHz the bilinear warping makes 96 kHz differ by up to 0.4 dB (Mode 3, 20 kHz) – still inside ±0.5 dB.

## Output peak stage (spec §11, D2, D3)

The stage must look ahead although the analog mode sits between the compressor and the stage, and the total latency
must stay H. The engine runs a **prediction**: the newest input sample through the composite gain expected H samples from
now (`GainSmoother::predictGain`: the attack or fast release extrapolated towards the current target, so the stage does not
over-limit during an attack) and a second, noise-free analog stage. Its linked peak gives `q = min(1, 1/|y_pred|)`.
`OutputStage` delays q by H − 57, holds the minimum over 58 samples and smooths with moving averages of 45 and 14 samples
(×2 / ×4 at higher rates), fitted to the §11 ramp. There is **no slow release** (r2): the same ramp restores full gain about
1.2 ms after the last peak; at very low frequencies this acts on each half wave. A steady over-ceiling 1 kHz sine leaves at
peak 1.0, H3 < −150 dBc at 48 kHz. A final clamp to ±1.0 enforces D2. Limiter and Clipper are the same stage (D3).

## GUI (spec §14)

Original vector artwork in the house style (graphite panels, amber / teal accents, copper for gain reduction):
left panel with a needle meter (0 … −48 scale, input / GR / output selected by the IN / GR / OUT selector, peak
readout, click to reset) above the large Threshold knob; centre panel with Ratio, Attack, Release (ms knob, or a note
knob in the sync modes), Punch, Mix and Output; right panel with the Release Sync selector (BPM / HOST / MS), the
tempo readout (editable value in BPM mode, "HOST xxx.x" when Off; host tempo shown in Host mode), the Analog
selector (OFF, 1–4) and the Clipper / Limiter selector, plus the look-ahead note. Every value box: click to type,
drag to adjust, double-click or Alt-click to reset; knobs reset on Alt-click / double-click as well.

## Tests (`tests/`, Catch2)

37 test cases, all passing on Linux (GCC) and Windows (MSVC); dynamics, §7.5, level and noise tables at 44.1 / 48 / 96 kHz.
Harmonics: Hann over whole seconds from 0.5 s; trajectories: window [t, t + 1 ms) (r2).

* `R2Tests` – §7.5 table (all 96 rows, continuous render in table order), 100 Hz levels, noise bands, hot-to-quiet for all
  modes, §11 recovery and over-ceiling gains.
* `ParameterTests` – maps, ratio table (all 41 points, log interpolation, monotone), note table and floor mapping,
  sync / analog thresholds, note durations (1/8 @ 120 = 250 ms), effective release, display formats.
* `StaticCurveTests` – every tabulated Δ cell, low-ratio rows, R ≥ 2 rule (R 2 / R 50 rows within 0.05 dB),
  combination rule, clamps, knee, monotone curve, makeup tables and examples, steady 1 kHz sines through the engine
  against curve + makeup (±0.2 dB, 15 settings), no ripple (H3 < −120 dBc), 44.1 / 96 kHz within 0.03 dB.
* `DynamicsTests` – all attack / punch / release / host-sync rows of §7.4 (±1.5 dB, ±3 dB within 5 ms, RMS ≤ 0.8 dB),
  punch onset / release delays (±0.5 ms), noise vs sine (±0.5 dB), LF ripple (±4 dB, H2 < −85 dBc, 500 Hz
  < −120 dBc), trajectories at 44.1 / 96 kHz within 0.1 dB.
* `AnalogTests` – Off identity, magnitude table, steady-tone table, Mode 3 post-Output level, noise floors and L/R
  decorrelation, clicks (never above the input), rate independence.
* `EngineTests` – latency per rate, bit-exact neutral identity (double / float, mono / stereo), mix linearity,
  exact output gain, output-stage ramp / clean over-ceiling sine / never above 1.0 (all modes, both settings) /
  untouched below 0 dBFS, parameter smoothing (D5), robustness with random settings, denormals, +40 dB input,
  silence and impulses at 44.1 / 96 / 192 kHz.

## Questions / assumptions

* **A1 – empty Δ cells.** T −6, d 9…24: the next rows' values (−12 row for 9/12, −24 row for 18/24); T −12,
  d −2/−1: 0, d 1/2: linear in T between the −6 and −24 rows (−0.30 / −0.40), d 18/24: −24 row; T −36, d −2/−1:
  the T-interpolated knee shape scaled to the row's own d = 0 value (+0.014 / +0.078), d 1/2: linear in d between
  the row's d = 0 and d = 3 cells (+0.04 / −0.08).
* **A2 – ratios below 1.25.** Δ₂₄ is interpolated linearly in u from 0 at R = 1 to the R = 1.25 row, so Ratio 1 is
  neutral. At exactly R = 2 the normative rule selects the R-4 row (0.03 dB from the R-2 row).
* **A3 – release law.** Tables normative; the law is empirical (see "Release law").
* **A4 – tempo.** Without host tempo information 120 BPM is used. The manual tempo parameter (no map in the spec)
  is linear 29 … 300 BPM in 0.1 steps; values below 30 mean Off.
* **A5 – parameter quantisation.** Engine values are quantised to the display resolution (also the reference's
  displayed resolution); this changes values by < 0.05 dB / < 0.5 %.
* **A6 – output stage.** Prediction-based detection, linked channels, ramp fitted to §11, no slow release.
* **A7 – analog noise.** Shaped per the §10 bands; at 96 kHz the table is applied to absolute density. Harmonic cells
  below −125 dBFS are noise-limited (r2): tests check them only as an upper bound.
* **A8 – switching.** Analog-mode changes and bypass crossfade over 10 ms; Punch changes move the side-chain delay
  in one step (side chain only, smoothed by the gain smoother).
* **A9 – rates.** Rates below 80 kHz use H = 64, below 160 kHz H = 128, above H = 256 (spec: 176.4 / 192 extrapolated).
* **A10 – detector floor.** Silence gives D = −400 dB; absurd input (> 1e6 or NaN) is clamped in the detector.
* **A11 – Mode 3 at exactly 2 ms after a hot passage.** The window [2, 3) ms reads up to 0.91 dB above the settled gain
  (tested at ±1 dB), from 3 ms on within ±0.5 dB; the settled gain is +7.67 dB (spec +7.9). The mean rule holds at all modes.
* Question for the analyst: the exact release law.

## Public references and provenance

* R. Bristow-Johnson, "Cookbook formulae for audio EQ biquad filter coefficients" (shelves, peaking, band-pass).
* D. Giannoulis, M. Massberg, J. D. Reiss, "Digital Dynamic Range Compressor Design — A Tutorial and Analysis",
  JAES 60(6), 2012 (feed-forward topology, smoothing in the gain domain).
* U. Zölzer (ed.), "DAFX – Digital Audio Effects", 2nd ed., Wiley 2011, ch. 4 (dynamics, look-ahead limiting).
* D. Lemire, "Streaming maximum-minimum filter using no more than three comparisons per element" (monotonic
  deque sliding max / min).
* JUCE 8 and clap-juce-extensions documentation for the plug-in wrapper.
* House style and code structure follow the merged Craft plug-ins of this repository (no code from elsewhere).

## Building

From the repository root: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build &&
ctest --test-dir build -R punch-compressor/`. Artefacts: `build/plugins/punch-compressor/CraftPunchCompressor_artefacts/
Release/{VST3,CLAP,Standalone}`. `ci/package.py` finds the artefact directory automatically
(`craft-punch-compressor-<version>-<platform>.zip`).

## Status

* Windows x64 Release (MSVC): VST3, CLAP, Standalone, tests build; installed to the user and common VST3 folders and the user CLAP folder.
* Unit tests: 37 / 37 test cases pass on Windows (ctest) and Linux (GCC).
* `craft_hoststress --seconds 120 --editor`: 73.6 M samples, 94 prepares, 413 state restores, 179 k parameter moves, 30 editor opens, **0 failures**. Its mono-in / stereo-out layout is rejected by design; mono is covered by unit tests.
* Not re-verified since r2: GUI snapshots (no GUI change) and independent verification.
