# Implementation notes: Craft Dynamics (spec r1)

This is the builder side of a clean-room implementation of [`spec/dynamics/r1/SPEC.md`](../../spec/dynamics/r1/SPEC.md)
(approved 2026-10-29). I used only that spec, this repository (conventions and the stereo-imager / flanger code),
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
  CurveTables.h          GENERATED: Appendix A (unlimited), expansion output-limit law, Appendix B
  StaticCurves.h         §5/§6 static laws: f_R(x) with interpolation in r, output limit, PeakRef offset,
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
3. Compressor detector: per-channel power pre-smoother (0.15 ms), max-linked, `ℓ = 10·log10(2p)`, dB-domain
   attack/release smoother; with PDR > 0 the dual detector `max(e_fast(Attack, 20 ms), e_slow(0.5·PDR, Release))`.
   Gain = `f_R(e − T)` (+ expansion output limit) + PeakRef offset + Makeup, then a 1 ms one-pole on the dB gain
   (see "Envelope conformance").
4. Gate detector: same pre-smoother; gate mode uses `(min(Attack, 2 ms), Release)` and freezes decay during Hold;
   open when `e > Open + 1.85`, close when `e < Close + 1.85` and Hold expired; the linear gain `g` follows the state
   with one-poles (Attack towards 1, Release towards 0). Expander mode: `g = 10^(h(e − Open)/20)`. Module gain
   `G = g + F·(1 − g)` (algebraically `F + (1 − F)·g`, written so that `g = 1` is exactly 1).
5. Audio path: the input is delayed by exactly 340 samples when LookAhead is on (the detectors above saw the
   undelayed signal). Wideband/Sidechain modules multiply the full band; a Split module applies its gain to the
   active band only: `y = AP(x) + (g − 1)·A(x)` (= `g·A + P`).
6. Output gain; monitor (Audio / Sidechain detection signal at unity / Passive `2·P`); key modes silence the keying
   channel.

**Discrete settings** (bypasses, modes, EQ modes, filter type, monitor, key, lookahead) are handled by two engine
"cores": on a change the active core (all filter, detector and delay-line state) is copied, the copy gets the new
configuration and the output crossfades linearly over 10 ms (§8: ≤ 20 ms). After the fade only one core runs, so
the neutral path stays bit-exact. **Continuous settings** (threshold, ratio position, makeup, PeakRef amount, floor,
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
| §4 detector | `Core::tick` detector part; constants `preSmootherMs`, `pdrFastReleaseMs`, `gateDetectorMaxAttackMs`. |
| §5 static curve | `curves::compressorCurveDb` (bilinear: linear in x between table rows, linear in r between the 14 columns plus an exact-zero column at r = 0.218), `curves::compressorGainDb` (+ expansion output limit), `curves::peakRefOffsetDb`. |
| §6 gate/expander | gate state machine + hold in `Core::tick`; `curves::expanderGainDb` (Appendix B), `curves::applyFloor`, `floorvalue::linear`. |
| §7 filter module | `filterdesign::design` (ω0 from the half-power condition for LP/HP, prewarped bilinear transform at Freq, AP with the sign that puts it in phase with H²), `FilterChannel`. |
| §8 smoothing | one-pole smoothers + 10 ms core crossfade (see Engine). |
| §12 UI | `source/ui/*`. |

### Static curve tables

`tools/gen_tables.py` turns Appendix A into the engine table:

- Rows up to x = +41.28 dB were measured with T = −40 dB and rows from +43.28 dB with T = −80 dB (from the
  x_det relation, the sine peak never exceeds 0 dBFS). In the T = −80 rows the expansion columns (0.50 … 0.93:1)
  first plateau and then fall: that fall is the §5 output limit. The pairs (unlimited output, limited output) give a
  single, consistent law L(y): identity up to 0 dBFS, then 0.63 dB at +1, 1.35 at +3, 2.29 at +9, 2.83 dBFS ceiling.
- Limited T = −40 entries are mapped back through L⁻¹ where that is well conditioned; the rest of the gap is filled
  by a monotone cubic through those points and the PeakRef anchors of §10.4 (0.50:1 at T = −20/−60/−80).
- The engine applies L to the estimated output peak `e − 1.277 dB + f` for expansion positions only, before makeup.
  Replaying the measurement through this model reproduces every printed Appendix A entry within 0.15 dB.
- Beyond the table the curves are held constant, except the 20.62:1 and 50:1 columns, which continue with slope −1.

## Tests

`ctest -R dynamics/` (or `dynamics_tests`), Catch2, registered with CTest via `catch_discover_tests`.

| File | Covers |
|---|---|
| `StaticCurveTests.cpp` | Every Appendix A point (71 rows × 14 ratio positions) through the full engine with a steady 1 kHz sine at 48 kHz, every 4th row also at 44.1/96 kHz, against the raw printed values (±0.1 dB; ±0.3 cancellation notch; ±0.5 expansion near the limit); §10.1 table; threshold-shift invariance; exact 0 dB at 1.00:1; displayed ratios; maximum reductions; makeup; §10.3 output limit; §10.4 PeakRef offsets (curve rule and engine). |
| `DetectorTests.cpp` | detector reading x_det = peak + 1.277 dB at 44.1/48/96 kHz; waveform dependence (100 Hz, 10 kHz, noise); max-linking (one channel / one channel 10 dB lower); all §10.5 compressor envelopes incl. PDR (t10/t63/t90 printed, per-1-ms error against a reference envelope through the spec points); PDR behaviour. |
| `GateTests.cpp` | §10.2 open/close levels on a 0.25 dB stair (incl. Attack 0.1 ms and GateClose −43); floor law (−27.42 / +12.04 dB, −inf, N floors in gate mode); Appendix B through the engine; expander positive floor and negative-polarity notch; §10.5 gate envelopes (open t63, close t10/t63/t90, linear-gain RMS ≤ 0.06, Hold). |
| `FilterTests.cpp` | §7 shapes (half power / centre / notch at Freq, closeness to the analog prototype), §10.6 sidechain and passive magnitudes, split identity A + P = AP, \|P\| = 1 − \|A\|, A in phase with AP (all four types), engine monitors, split flat at 0.00 dB, split compression on the active band only. |
| `RoutingTests.cpp` | bit-exact neutral (all variants, float and double), lookahead exactly 340 samples at 44.1–192 kHz and gain leading the delayed audio, §10.7 key modes incl. sidechain monitor, variant identity, mono = one channel of the stereo engine, click-free toggles/parameter jumps, settling within 20 ms. |
| `ParameterTests.cpp` | reference mappings and defaults, display strings and parsing, floor encoding. |

### Envelope conformance (§9, §10.5)

The engine follows the §4 model literally plus one addition: a 1 ms symmetric one-pole on the compressor gain (dB).
It moves the 1 ms-attack cases onto the measured points (t10/t63/t90 = 1.5/3.5/6.5 ms, same as the reference) and
does not change static gains. Remaining differences of the §4 model against §10.5 (my build, 48 kHz):

| Setting | attack t10/t63/t90 (ref) | release t10/t63/t90 (ref) |
|---|---|---|
| Att 1, Rel 50 | 1.5/3.5/6.5 (1.5/3.5/6.5) | 4.5/26.5/57.5 (2.5/25.5/74.5) |
| Att 10, Rel 50 | 5.5/18.5/42.5 (3.5/16.5/32.5) | 4.5/26.5/58.5 (4.5/28.5/80.5) |
| Att 100, Rel 50 | 44.5/176.5/>1000 (23.5/164.5/312.5) | 4.5/26.5/59.5 (8.5/33.5/82.5) |
| Att 1, Rel 10 | 1.5/3.5/6.5 (1.5/2.5/5.5) | 2.5/7.5/13.5 (1.5/6.5/16.5) |
| Att 1, Rel 200 | 1.5/3.5/6.5 (1.5/3.5/6.5) | 13.5/101.5/223.5 (7.5/95.5/290.5) |
| Att 1, Rel 1000 | 1.5/3.5/6.5 (1.5/3.5/6.5) | 60.5/500.5/1109.5 (30.5/417.5/1144.5) |
| PDR 22, 3 ms burst | — | 7.5/16.5/165.5 (1.5/15.5/769.5) |
| PDR 102, 30 ms burst | — | 3.5/12.5/311.5 (1.5/11.5/521.5) |

The reference releases start a little faster and end slower than a single dB-domain one-pole can; t90 values of long
PDR tails sit where the curve is almost flat, so the dB error there is small. Against a reference envelope
interpolated through the spec points the per-1-ms error is 0.4–1.5 dB RMS (99th percentile 1.0–3.1 dB); the tests
assert the documented bounds (RMS ≤ 1.6 dB, P99 ≤ 4.5 dB) and print the values. Gate envelopes meet §9
(linear-gain RMS 0.044 / 0.048 ≤ 0.06); the gate closes ~10–14 ms later than the reference at Release 100 ms.

## Questions / assumptions

1. **Expansion output limit.** Modelled as a static law on the estimated output peak (`e − 1.277 + f`) applied to
   expansion positions only, before Makeup (the spec does not say whether Makeup is inside the limit). The unlimited
   curves above the limit are reconstructed (see "Static curve tables").
2. **PeakRef for cancellation ratios.** §10.4 gives only −5.00:1. Positions up to −15.90:1 (r ≤ 0.9) use the curve
   rule `−f_R(x_ref)`; between r = 0.9 and 1.0 the offset blends linearly to the −5.00:1 table (interpolated in T,
   held below −80 dB, extrapolated linearly above −10 dB and clamped at ≥ 0). Cap +40 dB everywhere.
3. **Ratio display / host mapping.** The host value is the curve position r (0.001 resolution, default 0.218 =
   1.00:1). The displayed ratio interpolates log-ratio between the spec's known positions; 0.8 < r ≤ 0.9 shows
   −50:1 … −15.90:1 and 0.9 … 1 shows −15.90:1 … −5.00:1 (log-interpolated magnitude). The curve between the
   14 tabulated positions is linear in r, with an exact 0 dB column at 0.218 (|r − 0.218| ≤ 0.0005 is exactly 0 dB).
4. **Floor parameter encoding.** One host parameter covers "−10 N … −100 N", "−inf" and "−100 … +12 dB"
   (r ∈ [0, 0.15] / (0.15, 0.2) / [0.2, 1]). Negative-polarity floors act as −inf in gate mode (§6).
5. **GateClose −inf** means "no hysteresis" (close at GateOpen); a GateClose above GateOpen is clamped to GateOpen.
   GateOpen −inf keeps the gate/expander fully open (exactly unity).
6. **Expander table ends.** Below the Appendix B range the expander continues 1:2 (slope 1); above it the last
   value (−0.02 dB) is held.
7. **Sidechain monitor** shows the detection signal after the lookahead delay (aligned with the reported latency);
   "filtered" when any present module's EQ mode is not Wideband. In L→R / R→L it shows the key (keying channel's
   detection signal) on the output channel.
8. **Variant extras.** Output Gain and the host bypass exist in every product; LookAhead/Key/Monitor/input gains only
   where the spec lists them. External sidechain inputs are not provided (D5).
9. **Noise vs sine (§4 consequences).** The §4 model gives equal-RMS Gaussian white noise ~0.4 dB *more* reduction
   than a 1 kHz sine (uniform white noise ~0.2 dB less), not the quoted ~0.7 dB less; 100 Hz (+0.7 dB more) and
   10 kHz (~1.0 dB less) match. I kept the specified model; the test checks only a bounded difference.
10. **Compressor gain smoothing** of 1 ms (see above) is my addition within §8's latitude.
11. **Both modules in Split** (D4): the active band gets `g_c·g_g`, the passive band 1.
12. **PDR = 0 → >0** switches the detector structure without a crossfade; e_slow tracks e_fast while PDR is 0 so
    the change is continuous.

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

See the table below for the latest local results; CI builds and tests Windows and Linux.

| Item | Result |
|---|---|
| Unit tests | STATUS_TESTS |
| Windows x64 Release (MSVC, VS 2022) | STATUS_WINDOWS |
| Linux x64 (GCC 13, tests only locally) | STATUS_LINUX |
