# Implementation notes — Craft De-Esser (spec r1)

Clean-room implementation (builder side) of [`spec/de-esser/r1/SPEC.md`](../../spec/de-esser/r1/SPEC.md)
(SHA-256 `db412694…c4651`, approved 2026-10-29). Only the spec, this repository, public DSP references and this
implementation were used. No original product, analyst workspace, vendor asset or vendor installation was accessed.

Products: **Craft De-Esser** (stereo, `CDes`, CLAP id `com.craftaudio.de-esser`) and **Craft De-Esser Mono**
(`CDsM`, CLAP id `com.craftaudio.de-esser.mono`); manufacturer **Craft Audio** (`Crft`); version 0.1.0.
Formats: VST3, CLAP, Standalone. Platforms: Windows x64 (MSVC), Linux x64 (GCC). Licence: AGPL-3.0 like the
rest of the repository (JUCE), see [THIRD_PARTY.md](../../THIRD_PARTY.md).

## Architecture

All paths are relative to `plugins/de-esser/`.

```
CMakeLists.txt     targets of this plugin (added automatically by the root CMakeLists.txt)
source/dsp/        host-independent, header-only DSP core (no JUCE), double precision
  Parameters.h       §3 mappings r -> value, quantisation, Settings, mode enums
  Filters.h          §4 cookbook biquads + sidechain filter, §6 first-order split high band
  Dynamics.h         §5 peak detector, static curve, two-stage program-dependent gain smoothing
  Smoothing.h        D2 linear ramps (crossfades, threshold) and log-frequency glide
  DeEsserEngine.h    the complete per-sample processor (stereo linked or mono)
source/plugin/     JUCE AudioProcessor (channel count chosen per CMake target), parameter layout
source/ui/         editor, look-and-feel, controls and meters (all vector-drawn)
tests/             Catch2 tests of the DSP core against the spec test vectors (§8/§9)
tools/Snapshot.cpp optional dev tool rendering the editor to PNG (-DCRAFT_BUILD_SNAPSHOT=ON)
```

One `DeEsserProcessor` class takes the channel count at construction; `createPluginFilter()` uses the
compile definition `DE_ESSER_CHANNELS` set per CMake target (`de_esser_add_variant`). `ci/package.py` finds
both products automatically (it scans every `*_artefacts/Release` directory).

## How each spec section is implemented

**§1 Scope.** Stereo: 2 in / 2 out, one gain for both channels. Mono: 1 in / 1 out (D3). Latency 0
(`setLatencySamples(0)`, no look-ahead). The gain is the only non-linearity; with no reduction the output is
bit-identical to the input (see §6).

**§2 Sidechain signal.** `d = SC(0.5·(L + R))` (stereo), `d = SC(x)` (mono, D3). Internally double precision;
the plug-in also supports 64-bit host buffers.

**§3 Parameters.** The host-normalised value is the spec's `r`. Threshold `−80 + 80r` dB (0.1 dB steps, default
0 dB); Freq `2000·8^r` Hz quantised to integer Hz (default 5506 Hz at r = 0.4870). JUCE `NormalisableRange`
lambdas apply the mapping and quantisation, the processor re-derives the value from `r`. SideChain / Audio /
Monitor are two-entry choice parameters (`Band-Pass|High-Pass`, `Split|Wideband`, `Output|SideChain`), so
`r < 0.5` selects the first entry exactly as in §3; defaults High-Pass / Split / Output. Plus a host Bypass
(10 ms crossfade, fully bypassed output = input).

**§4 Sidechain filter.** `Filters.h`: RBJ cookbook HPF at `0.84·Freq`, `Q = 1/√2`, three identical sections in
cascade (transposed direct form II, double), followed in BandPass mode by one cookbook LPF at `1.457107·Freq`
clamped to `0.4999·fs`. Coefficients are designed at the actual sample rate. The high-pass cascade runs once;
HP and BP outputs are both available so the mode switch can crossfade. The filter feeds only the detector and
the monitor.

**§5 Detector / gain computer / smoothing.** `Dynamics.h`, per sample:
1. Peak envelope exactly as specified: `e += (|d|−e)·a_det` (τ 11 µs) if `|d| > e`, else `e *= r_det` (τ 0.38 ms).
2. Static curve: `GR = clamp((20·log10 e − (Threshold − 0.08)) · 6/7, 0, 20·log10 19)` → target `gt = 10^(−GR/20)`.
3. Smoothing, two stages in the linear-gain domain:
   - stage 1: attack `g1 += (gt−g1)·c(0.47 ms)` when `gt < g1`; release `g1 += (gt−g1)·c_rel` otherwise;
   - stage 2: `g = g1` when `g1 ≤ g` (attack passes unchanged), else `g += (g1−g)·c_on` (release onset);
   - program dependence: a hold-history state `h` charges towards 1 with τ 100 ms while `GR > 0` and forgets
     with τ 1 s otherwise; `c_rel = lerp(c(13 ms), c(31 ms), h)`, `c_on = lerp(c(10 ms), c(1.5 ms), h)`.
   All coefficients are `1 − exp(−1/(τ·fs))`, i.e. every time constant is in seconds. Both stages snap to the
   target within 1e-10, so the gain returns to exactly 1.0 after reduction.

   *Why not the single 28.8 ms release constant of §5.3:* implemented literally (one one-pole, τ 28.8 ms) the
   model reproduces vectors A, C, C′ and E but misses **B** (release after sustained maximum reduction) by up to
   3 dB (e.g. +192: −16.4 dB vs −19.41) and **D** at +192 (−16.3 vs −24.0 ± 4), outside the §8 tolerances. The
   spec leaves the program-dependent mechanism open (§5, open question 1) and asks for a release that depends on
   how long the reduction has been held; the structure above does that and was fitted (offline least-worst-case
   search over the §9.4 vectors) to B, C′ and D. After sustained reduction (h ≈ 1) the main release constant is
   31 ms, close to the spec's 28.8 ms; after a 5 ms burst (h ≈ 0.05) the release starts later but runs faster,
   which reproduces the shape of vector D (plateau to ~+192, then faster recovery than B).

**§6 Audio path.** `DeEsserEngine.h`. Split: `HP1` is the first-order high-pass of §6 with
`k = tan(π·Freq/fs)/√10`; the output is computed as `y = x + (g − 1)·HP1(x)` (= `LP1(x) + g·HP1(x)`), so `g = 1`
gives exactly `y = x`. Wideband: `y = g·x`. Monitor = SideChain: both outputs `d/√2`, no gain reduction applied,
detection keeps running.

**§7.** Meter ballistics are display-only (instant rise, ~26 dB/s fall); no presets; fixed-size GUI.

**D2 (decisions).** Threshold glides linearly in dB over 10 ms; Freq glides along a log axis over 10 ms
(filter / split coefficients are recomputed per sample only while gliding, and the exact target is used once
settled); Audio, SideChain and Monitor switches crossfade linearly over 10 ms (outputs of both states are
computed, so there is no filter restart). At the ends of a crossfade the pure path is used, which keeps the
no-reduction identity bit-exact.

**D3 Mono.** Same engine with one channel and `d = SC(x)`; tested to be sample-identical to the stereo
engine fed `L = R = x`.

## §12 User interface

Original vector-drawn editor in the Craft Audio palette (same look-and-feel family as the stereo imager), 600 × 394:

- Header with an original logo mark (a decaying "s" wave), product name and version.
- Left column, three panels: **Audio** (one toggle naming the current mode, Split / Wideband);
  **Frequency** (value box, drag / click-to-type / double-click or Alt-click resets) with **SideChain** below it
  (selector with a drawn high-pass / band-pass curve icon, the mode name and a drop-down arrow, opens a menu);
  **Monitor** (two stacked buttons Audio / SideChain, the active one lit, plus an indicator LED that lights while
  the sidechain is monitored).
- Centre: **Threshold** vertical slider with a value flag next to its handle, aligned pixel-exactly with the
  **sidechain energy meter** (0 … 80 dB below full scale; the threshold is also drawn as a line across the meter),
  numeric peak readout under it; the **attenuation meter** (0, −3, −6, −12, −18, Inf; the 0 … −6 dB zone tinted)
  with a peak-hold readout (click to reset). A threshold value box under the slider allows typing.
- Right: stereo **output meters** (−30 … 0 dBFS) with peak readouts underneath (click to reset). Mono shows one
  signal on both bars.

## Tests (`tests/`, Catch2, registered with CTest as `de-esser/…`)

| Test | Spec | Tolerance used |
|---|---|---|
| 9.1 sidechain / monitor magnitude, all 66 table points | §4, §6, 9.1 | ±0.0055 dB (±0.06 dB for the −131.8 entries; table rounding) |
| Monitor output vs independent long-double reference of §4/§6, 44.1/48/96 kHz, 4 Freq, HP/BP | §4, §8 | ≤ −100 dB re peak |
| Monitor level d/√2, GR not applied, mono = stereo L=R | §6, D3 | ±0.02 dB, ≤ 1e-15 |
| BP low-pass clamp at 44.1 kHz / 16 kHz | §4 | stable |
| 9.5 split shelf at g = 1/19 (3 Freq × 8 tones), k values | §6, 9.5 | ±0.1 dB, ±1° |
| LP1 + HP1 = 1 | §6 | 1e-15 |
| Static curve formula | §5.2 | exact |
| 9.2 static curve (36 points) and L-only line | 9.2 | ±0.3 dB |
| 9.3 noise vs sine (8th-order 4 kHz HP noise) | 9.3 | ±0.3 dB |
| 9.4 envelopes A, B, C, C′, E | 9.4 | ±1 dB |
| 9.4 D (offsets ≥ 192) | 9.4 | ±4 dB |
| Burst releases faster than sustained (≥ +480) | §5 | qualitative |
| A, B, C′ and static points at 44.1 and 96 kHz (offsets scaled in time) | §5, §8 | ±1 dB / ±0.3 dB |
| Bit-exact identity without reduction: Split/Wideband × HP/BP × stereo/mono × float/double, with Freq and mode changes while running | §1, §6, §8 | exact |
| Identity returns exactly after reduction released | §6 | exact |
| Latency 0 (output = g·x sample by sample) | §1 | exact |
| Split leaves the low band untouched at full reduction | §6 | |
| D2: no discontinuity on Monitor/Audio/SideChain/Threshold/Freq change; settles within 20 ms | D2 | |
| Parameter mappings and defaults | §3 | |

Model values at 48 kHz (gain in dB, same offsets as §9.4):

| Vector | +5 | +10 | +20 | +48 | +96 | +192 | +480 | +960 | +2400 | +4800 |
|---|---|---|---|---|---|---|---|---|---|---|
| A | −1.78 | −3.55 | −6.99 | −15.36 | −22.88 | −24.95 | −25.34 | −25.38 | −25.39 | −25.41 |
| B | −25.44 | −25.44 | −25.43 | −25.28 | −23.79 | −19.33 | −11.67 | −6.61 | −1.88 | −0.29 |
| C | −0.91 | −1.91 | −3.54 | −6.20 | −7.48 | −7.99 | −8.21 | −8.23 | −8.25 | −8.27 |
| C′ | −8.30 | −8.30 | −8.29 | −8.21 | −7.96 | −7.30 | −5.51 | −3.59 | −1.14 | −0.18 |
| D | (−25.11) | (−25.11) | (−25.11) | (−25.03) | (−24.18) | −20.56 | −11.34 | −4.75 | −0.48 | −0.01 |
| E | −1.28 | −2.61 | −4.93 | −9.23 | −11.51 | −12.23 | −12.49 | −12.52 | −12.54 | −12.56 |

Static curve (9.2) model values: Threshold −36 → −4.87 / −10.01 / −15.16 / −20.30 / −25.44 / −25.58 dB at
−30 … 0 dBFS (spec −4.85 / −9.98 / −15.12 / −20.26 / −25.41 / −25.57). The "gain" in these tests is the gain
applied by the processor at that sample (envelopes) or the output/input RMS ratio (static, noise).

## Public references

- R. Bristow-Johnson, "Cookbook formulae for audio EQ biquad filter coefficients" (HPF/LPF).
- D. Giannoulis, M. Massberg, J. D. Reiss, "Digital Dynamic Range Compressor Design — A Tutorial and Analysis",
  JAES 60(6), 2012 (peak detector, hard-knee gain computer, attack/release smoothing).
- U. Zölzer (ed.), "DAFX", dynamics processing chapter (program-dependent release concepts).
- JUCE 8 and CLAP / clap-juce-extensions documentation.

## Questions / assumptions

1. **Release model (open question 1).** Two-stage, hold-history-dependent release as described under §5,
   fitted to vectors B, C′, D within the §8 tolerances (worst errors: B 0.62 dB, C′ 0.46 dB, D 3.4 dB at +192
   where ±4 is allowed). The single 28.8 ms constant of §5.3 is not used literally because it fails B by up to
   3 dB. The history time constants (100 ms charge, 1 s forget) are my choice; only the 5 ms burst and the 1 s
   sustained cases are specified.
2. **Measurement method of 9.4.** Assumed to be the instantaneous gain applied to the 10 kHz tone at that sample
   offset (consistent with the attack vectors). Level changes happen at the sample boundary, sine phase continuous.
3. **Freq quantisation (open question 3).** Freq is quantised to integer Hz (the displayed value).
4. **Automation smoothing (open question 2).** 10 ms glides / crossfades (D2 allows ≤ 20 ms).
5. **BandPass low-pass clamp.** "Just below fs/2" = 0.4999·fs.
6. **Mono product.** Shipped as a separate product "Craft De-Esser Mono" (`CDsM`, CLAP id
   `com.craftaudio.de-esser.mono`) rather than a second bus layout, so every host sees a fixed 1-in/1-out plug-in.
7. **Bypass.** Host bypass parameter with a 10 ms crossfade (not in the spec; harmless addition).
8. **Meters.** Sidechain meter shows the detector envelope (dBFS), attenuation meter the deepest gain per GUI
   frame, output meters the block peak. Ballistics are mine (spec §7 leaves them open).

## Status

- Windows x64 Release (MSVC 2022): VST3, CLAP, Standalone for both products build; installed locally.
- Unit tests: see the test table; all pass on Windows (MSVC) and Linux (GCC).
- Linux plug-in targets are not built locally (no X11/ALSA dev packages on the build host); CI builds them.
