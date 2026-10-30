# Implementation notes: Craft Dynamics (spec r3)

This is the builder side of a clean-room implementation of [`spec/dynamics/r3/SPEC.md`](../../spec/dynamics/r3/SPEC.md)
(approved 2026-10-29; r3 supersedes r2 and r1 after the independent verifications
[`dynamics-r1.md`](../../docs/verification/dynamics-r1.md) and [`dynamics-r2.md`](../../docs/verification/dynamics-r2.md)). I used only that spec, this repository (conventions and the stereo-imager / flanger code),
public DSP literature and my own work. I did not access any original product, analyst workspace, vendor asset or
vendor installation.

**Products** (one shared engine and processor; the variant is chosen per CMake target):

| Target | Product | Plugin code | CLAP id | Spec §1 variant | Modules |
|---|---|---|---|---|---|
| `CraftDynamics` | Craft Dynamics | `CDyn` | `com.craftaudio.dynamics` | Full (primary) | compressor/expander, gate/expander, filter module, key, monitor, lookahead, input gains |
| `CraftCompressor` | Craft Compressor | `CCmp` | `com.craftaudio.compressor` | Compressor | compressor/expander only, wideband |
| `CraftGate` | Craft Gate | `CGat` | `com.craftaudio.gate` | Gate | gate/expander only, wideband |
| `CraftCompressorSC` | Craft Compressor SC | `CCsc` | `com.craftaudio.compressor-sc` | Compressor + filter | compressor/expander, filter module, key, monitor, lookahead, input gains |

- **Manufacturer:** Craft Audio (`Crft`). **Version:** 0.1.0.
- **Formats:** VST3, CLAP and Standalone. **Platforms:** Windows x64 (MSVC) and Linux x64 (GCC).
- **I/O:** stereo 2→2 (primary); mono 1→1 is accepted by every product and runs the same engine on one channel (D7).
- **Latency:** 340 samples with LookAhead on (reported to the host, updated when the switch changes), otherwise 0.
- **Licence:** AGPL-3.0, the same as the repository.

## Architecture

All paths below are relative to `plugins/dynamics/`.

```
CMakeLists.txt         dynamics_dsp (INTERFACE), four plugin targets (dynamics_add_variant), optional snapshot tool
source/dsp/            host-independent, header-only DSP core (no JUCE)
  CurveTables.h          GENERATED: Appendix A (r3, unclipped) and Appendix B
  StaticCurves.h         §5/§6 static laws: f_R(x) with interpolation in r, PeakRef offset,
                         expander h(x), floor law
  Filters.h              §7 filter module: prewarped bilinear design of H, the in-phase allpass, biquads,
                         per-channel paths (sidechain H^3, active H^2, allpass)
  DynamicsEngine.h       §3/§4/§6/§8 engine: variants, settings, detectors, gate state machine, routing,
                         lookahead, smoothing, crossfades, meters
  Parameters.h           §2 mappings r -> value, quantisation, ratio display, floor encoding, display/parse text
  Smoothing.h            one-pole smoother (snaps to target) and linear crossfade ramp
source/plugin/         JUCE AudioProcessor + parameter layout (shared by the four targets)
source/ui/             original vector GUI: transfer graph, filter graph, value boxes, segmented switches, meters
tests/                 Catch2 tests (dynamics_tests, CTest prefix "dynamics/"); SpecTables.h = raw spec tables
tools/gen_tables.py    regenerates CurveTables.h / SpecTables.h from the spec (numpy + scipy)
tools/Snapshot.cpp     renders the editors to PNG (CRAFT_BUILD_SNAPSHOT=ON; development aid)
```

### Engine

`DynamicsEngine` processes in place (`float` or `double`) in double precision. Per sample:

1. Input gains (smoothed linear gains; exactly 1.0 at 0 dB).
2. Detection signals: wideband or the sidechain filter `H³` (three cascaded sections), per module according to its
   EQ mode. Key mode selects which channels drive the detectors.
3. Compressor detector (r2 §4): per-channel rectified average `m ← a·m + (1 − a)·|s|` (RECT_TAU 0.127 ms),
   max-linked, `ℓ = 20·log10(m) + K_CAL` (3.32 dB), dB-domain smoother with (Attack, DET_REL·Release); with PDR > 0
   `max(e_fast(Attack, PDR_FAST_REL), e_slow(0.5·PDR, DET_REL·Release))`. Gain (r2 §5): target reduction
   `t = −f_R(e − T)`, slow memory `s` (jumps up to t, decays with SLOW_REL·Release), `R = (1 − SLOW_W)·t + SLOW_W·s`,
   `G = −R + Makeup + PeakRef offset`.
4. Gate detector: same rectifier; gate mode uses `(min(Attack, 2 ms), Release)` and freezes decay during Hold;
   open when `e > Open + 1.85`, close when `e < Close + 1.85` and Hold expired; the linear gain `g` follows the state
   with one-poles (Attack towards 1, Release towards 0). Expander mode (r2 §6): detector `(Attack,
   EXP_DET_REL·Release)`, target `u = h(e + EXP_CAL − Open)` (EXP_CAL = 1.11 dB, r3 §4.3), applied gain smoothed
   in dB with `EXP_GAIN_ATT·Attack` / `EXP_GAIN_REL·Release`. Module gain `G = g + F·(1 − g)` (algebraically
   `F + (1 − F)·g`, written so that `g = 1` is exactly 1).
5. Audio path: the input is delayed by exactly 340 samples when LookAhead is on (the detectors above saw the
   undelayed signal). Wideband/Sidechain modules multiply the full band; a Split module applies its gain to the
   active band only: `y = AP(x) + (g − 1)·A(x)` (= `g·A + P`).
6. Output gain, then the r3 §5.1 hard clip of every output sample to ±1.0 (also for the monitor outputs); monitor (Audio / Sidechain = the compressor's detection signal at unity, filtered only when the
   compressor's EQ mode uses the filter / Passive `2·P`); key modes silence the keying channel.

**Discrete settings** (bypasses, modes, EQ modes, filter type, monitor, key, lookahead) are handled by two engine
"cores": on a change the active core (all filter, detector and delay-line state) is copied, the copy gets the new
configuration and the output crossfades linearly over 10 ms (§8: ≤ 20 ms). After the fade only one core runs, so
the neutral path stays bit-exact (for input within ±1.0, r3 §3).

**State at stream start / host reset (r3 §3):** detector levels at silence (−400 dB), compressor slow memory 0 dB,
gate closed (g = 0), expander gain at silence. `prepare()` and the processor's `reset()` override (host reset) both
restore it. GateOpen −inf means no gate at all and gives exactly unity from the first sample. **Continuous settings** (threshold, ratio position, makeup, PeakRef amount, floor,
input/output gains, filter frequency/Q) use one-pole smoothers (τ = 4 ms, settled well within 20 ms) that snap to
the exact target; filter coefficients are recomputed every 16 samples while Freq/Q move.

### Processor / GUI

`DynamicsProcessor` maps the host parameters to `dsp::Settings` each block, reports the latency (message thread,
on LookAhead changes), publishes meter values through atomics and implements the link toggles (message thread):
with a link on, a change of one control moves its partner by the same normalised amount (Comp Threshold ↔ Gate
Open, Comp Attack ↔ Gate Attack, Comp Release ↔ Gate Release). Links have no audio effect of their own (§2).
The host bypass parameter switches the engine to its neutral configuration (crossfaded; the lookahead delay is kept
so the reported latency stays valid).

The editor (§12) is original artwork drawn in code with the shared Craft palette (amber = compressor and active
band, teal = gate, lavender = passive band):

- transfer graph (input horizontal, output vertical, 0 … −100 dB) with the compressor and gate/expander curves, a
  dot per curve at the current detector level, and draggable triangles for Threshold, GateOpen (filled) and
  GateClose (outline) below the plot;
- compressor panel: Threshold, Ratio, Makeup, Attack, Release, PDR, LowRef/PeakRef, EQ mode, bypass, and gain-change
  (reduction downward) + detector-level meters with a click-to-reset peak box;
- gate panel: Gate/Expander, Open, Close, Floor, Attack, Release, Hold, EQ mode, bypass, meters;
- filter panel: response of the active (|H|²) and passive (1 − |H|²) bands with a crosshair (drag: horizontal =
  Freq, vertical = Q), type selector, Freq and Q boxes;
- global panel: input gains L/R, Key, Monitor, LookAhead, link toggles; output panel: output fader and IN/OUT peak
  meters with clip LEDs and click-to-reset peak value boxes.

Variants omit absent sections (Compressor/Gate: graph + module + output; Compressor SC: no gate panel/links).

## Spec mapping

| Spec | Implementation |
|---|---|
| §1 variants | `dsp::Variant` / `variantInfo`; absent modules are forced bypassed/wideband in `DynamicsEngine::setSettings`, so a variant is the Full engine with the module bypassed (bit-identical; tested). |
| §2 parameters | `dsp::param::*` (reference mappings r → value incl. Ratio positions, PDR table, Hold, Floor, Filter Freq/Q, gains); `ParameterLayout.cpp` registers only the parameters a variant has. Display strings in `formatDisplay`. |
| §3 signal flow | `DynamicsEngine::Core::tick` (steps 1–7 above). Both modules in Split: active band gets `g_c·g_g` (D4). |
| §3.5 lookahead | per-core 340-sample ring buffers for the audio and sidechain-monitor paths; `getLatencySamples()`. |
| §3 neutral | bypassed modules use the literal gain 1.0, gains at 0 dB are exactly 1.0 → bit-exact passthrough (tested in float and double, all variants, 44.1/48/96 kHz). The default settings (1.00:1, gate −inf) are bit-exact too. |
| §4 detector | `Core::tick` detector part; the §4.3 constants are `DynamicsEngine::rectTauMs`, `kCalDb`, `detRel`, `pdrFastReleaseMs`, `slowRel`, `slowWeight`, `expDetRel`, `expGainAtt`, `expGainRel`. |
| §5 static curve / dynamic gain | `curves::compressorCurveDb` (bilinear: linear in x between table rows, linear in r between the 14 columns plus an exact-zero column at r = 0.218), slow memory in `Core::tick`, `curves::peakRefOffsetDb`, `curves::staticGainDb` (GUI); output clip `DynamicsEngine::outputClip`. |
| §6 gate/expander | gate state machine + hold, expander dB gain smoothing in `Core::tick`; `curves::expanderGainDb` (Appendix B), `curves::applyFloor`, `floorvalue::linear`; `expCalDb`. |
| §7 filter module | `filterdesign::design` (ω0 from the half-power condition for LP/HP, prewarped bilinear transform at Freq, AP = D(−s)/D(s) for every type; band-pass uses A = −H² so that A stays in phase with AP), `FilterChannel`. |
| §8 smoothing | one-pole smoothers + 10 ms core crossfade (see Engine). |
| §12 UI | `source/ui/*`. |

### Static curve tables

`tools/gen_tables.py` writes Appendix A/B of r3 verbatim to `CurveTables.h` (engine) and `tests/SpecTables.h`. In r3
the expansion columns are the unclipped curves; the earlier "expansion output limit" was the reference's hard clip
seen through RMS measurements (r3 §5.1). Beyond the table the curves are held constant, except the 20.62:1 and 50:1
columns, which continue with slope −1. Below Appendix B's first row h(x) = x − 2.04 dB, above its last row −0.02 dB.

## Tests

`ctest -R dynamics/` (or `dynamics_tests`), Catch2, registered with CTest via `catch_discover_tests`.

| File | Covers |
|---|---|
| `StaticCurveTests.cpp` | Every Appendix A point (rows whose unclipped output would exceed 0 dBFS are left to the clip tests) (71 rows × 14 ratio positions) through the full engine with a steady 1 kHz sine at 48 kHz, every 4th row also at 44.1/96 kHz, against the raw printed values (±0.1 dB; ±0.3 cancellation notch); §10.1 table; threshold-shift invariance; exact 0 dB at 1.00:1; displayed ratios; maximum reductions; makeup; §10.3 clipped expansion levels; §10.4 PeakRef offsets (curve rule and engine). |
| `R2TableTests.cpp` | the r2 normative tables at 44.1, 48 and 96 kHz: all 460 cells of §10.5.1 (per-row tolerance; RMS/max error printed), §10.5 timing t63/t90 within 2 ms + 25 % (non-informative cells), §10.5.3 expander trajectory (±0.15), §10.8 detector type (noise / 100 Hz / 10 kHz, 50:1 and 4.58:1, ±0.4 dB), §10.9 PeakRef near full scale (27 configurations × 7 levels, ±0.3 dB). |
| `DetectorTests.cpp` | detector reading x_det = peak + 1.277 dB at 44.1/48/96 kHz; max-linking (one channel / one channel 10 dB lower); PDR behaviour. |
| `GateTests.cpp` | §10.2 open/close levels on a 0.25 dB stair (incl. Attack 0.1 ms and GateClose −43); floor law (−27.42 / +12.04 dB, −inf, N floors in gate mode); Appendix B through the engine; expander positive floor and negative-polarity notch; §10.5 gate envelopes (open t63, close t10/t63/t90, linear-gain RMS ≤ 0.06, Hold). |
| `FilterTests.cpp` | §7 shapes (half power / centre / notch at Freq, closeness to the analog prototype), §10.6 sidechain and passive magnitudes, split identity A + P = AP, \|P\| = 1 − \|A\|, A in phase with AP (all four types), AP(0) = +1 for every type and band-pass passive polarity (r2), engine monitors incl. "only the gate filtered → wideband monitor", split flat at 0.00 dB, split compression on the active band only. |
| `RoutingTests.cpp` | r3 §5.1 hard clip (sample peaks exactly ±1.0, also at onsets with extreme gains), r3 §3 state at start / after reset (gate starts closed; reset = fresh engine, bit-identical), bit-exact neutral (all variants, float and double), lookahead exactly 340 samples at 44.1–192 kHz and gain leading the delayed audio, §10.7 key modes incl. sidechain monitor, variant identity, mono = one channel of the stereo engine, click-free toggles/parameter jumps, settling within 20 ms. |
| `ParameterTests.cpp` | reference mappings and defaults, display strings and parsing, floor encoding. |

### Envelope conformance (§9, §10.5)

The engine implements the §4–§6 laws literally (constants of §4.3). Results against the normative tables, identical
at 44.1/48/96 kHz:

- §10.5.1: all 460 cells within their row tolerance; RMS error 1.04 dB, largest 5.5 dB (in a ±6 dB row). The tightest
  cell is Att 0.1 ms / edge A / +1 ms (−11.4 dB vs −14.88 ± 3.5).
- §10.5 timing: all non-informative t63/t90 within 2 ms + 25 %.
- §10.5.3 expander (with EXP_CAL): max error 0.148 linear gain (±0.15) - tight.
- §10.8: within ±0.4 dB. §10.9 (with the hard clip): all 189 points within ±0.3 dB.
- §10.4: the 0.50:1 offsets at T −60/−80 come out 0.08/0.12 dB from the table, because r3's unclipped column
  saturates at 23.96 dB; the test allows ±0.15 there (±0.1 elsewhere).

## Questions / assumptions

1. **Output clip placement (D9).** The ±1.0 clip acts on every output sample after Output gain, also on the
   Sidechain/Passive monitor outputs. There are no other safety measures. The r2 build's soft output limit, onset
   peak hold and +30 dBFS clamp are removed.
2. **PeakRef for cancellation ratios.** §10.4 gives only −5.00:1. Positions up to −15.90:1 (r ≤ 0.9) use the curve
   rule `−f_R(x_ref)`; between r = 0.9 and 1.0 the offset blends linearly to the −5.00:1 table (interpolated in T,
   held below −80 dB, extrapolated linearly above −10 dB and clamped at ≥ 0). Cap +40 dB everywhere.
3. **Ratio display / host mapping.** The host value is the curve position r (0.001 resolution, default 0.218 =
   1.00:1). The displayed ratio interpolates log-ratio between the spec's known positions; r = 0.8 shows 50.00:1
   (host floats are snapped to a 1e-6 grid first), 0.8 < r ≤ 0.9 shows −50:1 … −15.90:1 and 0.9 … 1 shows
   −15.90:1 … −5.00:1. The curve between the 14 tabulated positions is linear in r, with an exact 0 dB column at
   0.218 (|r − 0.218| ≤ 0.0005 is exactly 0 dB).
4. **Floor parameter encoding.** One host parameter covers "−10.0 N … −100.0 N" (0.1 dB steps), "−inf" and
   "−100 … +12 dB" (r ∈ [0, 0.15] / (0.15, 0.2) / [0.2, 1]). Negative-polarity floors act as −inf in gate mode.
5. **GateClose −inf** means "no hysteresis" (close at GateOpen); a GateClose above GateOpen is clamped to GateOpen.
   GateOpen −inf keeps the gate/expander fully open (exactly unity, from the first sample after a reset).
6. **Sidechain monitor** shows the compressor's detection signal after the lookahead delay (aligned with the
   reported latency). In L→R / R→L it shows the key (keying channel's detection signal) on the output channel.
7. **Variant extras.** Output Gain and the host bypass exist in every product; LookAhead/Key/Monitor/input gains only
   where the spec lists them. External sidechain inputs are not provided (D5).
8. **Both modules in Split** (D4): the active band gets `g_c·g_g`, the passive band 1.
9. **PDR = 0 → > 0** switches the detector structure without a crossfade; e_slow tracks e_fast while PDR is 0 so
   the change is continuous.
10. **§10.8 above 48 kHz.** The test's white noise is band-limited to 20 kHz at 96 kHz, so every rate gets the same
    audio-band signal.

## Public references

- D. Giannoulis, M. Massberg, J. D. Reiss, "Digital Dynamic Range Compressor Design — A Tutorial and Analysis",
  JAES 60(6), 2012 (level detection in the log domain, attack/release smoothing, gain computer structure).
- R. Bristow-Johnson, "Cookbook formulae for audio EQ biquad filter coefficients" (bilinear transform with
  prewarping; biquad forms).
- U. Zölzer (ed.), *DAFX: Digital Audio Effects*, 2nd ed., Wiley 2011, ch. 4 (dynamics processing, look-ahead).
- F. N. Fritsch, R. E. Carlson, "Monotone Piecewise Cubic Interpolation", SIAM J. Numer. Anal. 17(2), 1980 (table
  reconstruction, via SciPy's PCHIP).
- JUCE 8 and clap-juce-extensions documentation.

## Status

Latest local results (CI builds and tests Windows and Linux):

| Item | Result |
|---|---|
| Unit tests | 36 test cases / 8481 assertions, all passing (Windows MSVC Release and Linux GCC 13) |
| Windows x64 Release (MSVC, VS 2022) | all four products build (VST3, CLAP, Standalone); installed locally for host testing |
| Linux x64 | DSP tests built and passing locally (GCC 13); the plug-in targets are built by CI (no X11/ALSA dev packages locally) |

Not done / open: no listening test in a DAW yet; editor snapshots were checked with `tools/Snapshot.cpp` only.
`tools/hoststress` (shared stress host) was run against all four VST3 builds (see Status).
