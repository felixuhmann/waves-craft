# Implementation notes — Craft stereo imager (spec r1)

Clean-room implementation (builder side) of [`spec/r1/SPEC.md`](spec/r1/SPEC.md)
(SHA-256 `dad6a104…955dcc`, approved 2026-10-29). Only the spec, `docs/process.md`, public documentation and
this implementation were used. No original product, analyst workspace, vendor asset or vendor installation
was accessed.

Products: **Craft Shuffler** (`CShf`, spec "Full"), **Craft Imager** (`CImg`), **Craft MS Matrix** (`CMsm`);
manufacturer **Craft Audio** (`Crft`); version 0.1.0. Formats: VST3, CLAP, Standalone. Platforms: Windows x64
(MSVC), Linux x64 (GCC).

## Licence

JUCE 8 is dual-licensed AGPL-3.0 / commercial. This repository is public and publishes binaries, so it uses
JUCE under the **AGPL-3.0** and the whole project is licensed AGPL-3.0 ([LICENSE](LICENSE)). Dependencies and
their licences: [THIRD_PARTY.md](THIRD_PARTY.md). Switching to a commercial JUCE licence would be required for
closed-source distribution.

## Architecture

```
source/dsp/       host-independent, header-only DSP core (no JUCE)
  Parameters.h      §3 mappings r -> value, quantisation, inverse mappings
  Matrix.h          Settings, Variant, §4 steps 1-6 + 8 as a 2x2 matrix (input -> M/S), encode
  Shuffler.h        §5 activity rule, low-frequency gains, bilinear first-order section
  Smoothing.h       one-pole, second-order (two cascaded one-poles), linear ramp
  ImagerEngine.h    the complete per-sample processor (smoothing, shuffler fade, bypass fade)
source/plugin/    JUCE AudioProcessor shared by the three variants (variant chosen per target)
source/ui/        editor, look-and-feel, faders/value boxes, vector display, meters (all vector-drawn)
tests/            Catch2 unit tests of the DSP core (spec §8/§9)
tools/Snapshot.cpp  optional dev tool rendering the editors to PNG (-DCRAFT_BUILD_SNAPSHOT=ON)
ci/package.py     release packaging; .github/workflows/release.yml  CI + releases
```

One `CraftProcessor` class takes the `Variant` at construction; `createPluginFilter()` picks it from the
compile definition `CRAFT_VARIANT` set per CMake target (`craft_add_variant` in `CMakeLists.txt`).

## How each spec section is implemented

**§1 Scope / variants.** Stereo-only bus layout (`isBusesLayoutSupported`). The Imager variant does not expose
Shuffle/Frequency/Bass Trim; the engine forces Shuffle 1.0 / Trim 0.0 for it, so its output is the Full
variant's at those settings (test: ≤ 6e-8). MS Matrix uses the fixed matrix `B = 0.5·I` before encode, i.e.
`out1 = (in1+in2)/2`, `out2 = (in1−in2)/2` exactly. Latency 0 (`setLatencySamples(0)`, no look-ahead).

**§2/§4 Signal path.** `broadbandToMS()` builds `B = Gain · Rot · Asym · Width · Decode · Swap · Polarity` as a
2×2 matrix from the two inputs to (M, S), in the spec's order. The gain (step 8) is folded into `B`: it is a
scalar on M and S and therefore commutes with the LTI shuffler. Multiples of 90° use exact sin/cos so neutral
and ±90° settings have exact 0/±1 coefficients. Per sample: `[M;S] = B·x`, optional shuffler, `L = M+S`,
`R = M−S`. While the shuffler is out of circuit the encode is folded into the matrix, so neutral settings give
identity coefficients and a bit-exact passthrough. Internal arithmetic is double precision (the plug-in also
supports 64-bit host buffers).

**§3 Parameters.** Each host parameter's normalised value is the spec's `r`. JUCE `NormalisableRange` custom
lambdas apply the §3 mapping **and the quantisation** in `convertFrom0to1`, so hosts and the GUI only ever see
step values; the processor re-derives the quantised double value from `r` (`readSettings()`), so values
between steps give bit-identical output (tested). Rounding is half away from zero (spec allows either at ties).
Text formats follow §12 (`formatValue`). Toggles (Swap, Polarity 1/2, Input Mode) are automatable bool
parameters; Bypass is the host bypass parameter (`getBypassParameter`).

**§5 Shuffler.** Active iff Shuffle ≠ 1.0 or Bass Trim ≠ 0.0 after quantisation (compared as integer steps).
Gains `G_M = T·sqrt((1+1/s²)/2)`, `G_S = s·G_M`. Each of M and S runs a transposed-direct-form-II first-order
section with coefficients from the spec's `H(z)` with `k = tan(π·Frequency/fs)` at the **actual** sample rate
(review decision) — DC gain G, gain −1 at Nyquist, exactly −90° and `sqrt(G)` at Frequency at every rate.
§5.1: no extra HF loss at 96/192 kHz (tested against the analytic response).

**§6 Smoothing.** Broadband: the four coefficients of `B` are smoothed by one-pole filters with τ = 40 ms
(error ×0.741 per 12 ms), so Gain/Width/Rotation/Asymmetry and all toggles (including Input Mode) crossfade.
Shuffle, Bass Trim and Frequency: second-order smoothing (two cascaded identical one-poles, 10–90 % = 160 ms,
τ = 47.65 ms each); Frequency is smoothed in log2 domain; shuffler coefficients are recomputed per sample while
these move. Smoothers snap to the exact target once within 1e-9 (matrix) / 1e-7 (shuffle values), so settled
output is exactly the static model. Shuffler engage/disengage and host bypass are linear crossfades (50 ms /
20 ms); see assumptions.

**§8 Tolerances / §9 vectors.** See tests below.

**§12 GUI.** Original vector-drawn design (dark graphite panels, amber/teal accents, own two-ring logo mark;
no images or bundled fonts). Left block: faders Gain (0/−12/−24), Width (3.0/2.0/1.0/0.0), Shuffle (3.0/2.0/1.0),
Freq (1.4k/650/350) with titles above and value boxes below; under them Input Mode switch L-R/M-S, the
polarity 1 / swap ↔ / polarity 2 buttons and the Bass Trim box (Full only). Centre: vector display with
Asymmetry and Rotation sliders and value boxes underneath. Right: L/R output meters (0…−24 dB), click-to-reset
peak-hold boxes and the Meter Mode switch L-R/M-S (M/S shows (L+R)/2 and (L−R)/2). Header: Reset button
(Width, Asymmetry, Rotation, Shuffle). Double-click or Alt-click resets faders, sliders and value boxes;
value boxes accept typed values (single click) and vertical drag. The vector display computes the §4
broadband matrix (no shuffler) from the current parameter values, draws the three probe lines
(left-only, centre, right-only) with `θ = atan2(Lo−Ro, Lo+Ro)` and radius `(dB+24)/24` clamped to [−24, 0] dB,
fills the probe triangle, and supports drag (vertical Gain, horizontal Rotation; Alt: Width/Asymmetry;
Shift: fine). MS Matrix shows a minimal panel stating its function.

## Tests (`tests/`, Catch2, 24 test cases / 8755 assertions)

All run the DSP core on impulses, sweeps or noise and measure:

- §9 LR matrices (all 20 rows; Full and Imager), tolerance ±0.6e-4 (table precision).
- §9 shuffler magnitude/phase table at 44.1 kHz (DTFT of the M and S impulse responses) within ±0.0015 dB /
  ±0.06°; the same rows at 48 and 96 kHz up to 1.5 kHz within ±0.01 dB / ±0.25° (bilinear warping only).
- §9 DC gains (Shuffle 1.5…3.0, Trim ±3 dB) at 44.1/48/96 kHz within ±0.0015 dB.
- Corner: exactly −90° and `|H| = sqrt(G)` at Frequency for 44.1/48/88.2/96/176.4/192 kHz × several G/Frequency.
- No HF loss at 96/192 kHz (§5.1).
- §8 static accuracy vs. an independent straight-line reference model of §4–§5, 5 sample rates × 12 random
  settings × 3 variants on sweeps and noise: ≤ −100 dB re. peak.
- Neutral → bit-exact passthrough (Full/Imager, 3 rates; Frequency alone does not activate the shuffler);
  bit-exact again after automating back to neutral; host bypass bit-exact once faded.
- Quantisation: mapping spot values and round trips for every step; between-step settings bit-identical.
- Zero latency; no level dependence (−30/−12/−3 dBFS, ≤ −150 dB in 64-bit).
- Smoothing: broadband τ within 30–50 ms and ×0.741 after 12 ms, onset < 3 ms; toggles have no discontinuity;
  Shuffle 10–90 % within 130–190 ms and S-shaped; shuffler engage/disengage click-free.

## Questions / assumptions (for the review gate)

1. **Frequency smoothing** (spec §10.1 open): same second-order law as Shuffle, applied to log2(Frequency).
2. **Smoothing law** (§10.2): first-order on the broadband matrix coefficients (τ 40 ms); two cascaded
   one-poles for Shuffle/Bass Trim/Frequency (10–90 % 160 ms).
3. **Shuffler on/off transition** (not specified): the stage fades in over 50 ms as soon as it becomes active
   (while Shuffle/Trim glide from neutral), and fades out over 50 ms only after Shuffle/Trim have glided back to
   neutral (where the stage is a pure all-pass). During that fade, dry and all-passed signals are mixed (brief,
   click-free comb colouration). Afterwards the passthrough is bit-exact.
4. **Host bypass**: 20 ms linear crossfade to the dry input; bit-exact once complete.
5. **Rounding ties**: half away from zero (the reference reportedly sometimes rounds down; non-critical).
6. **Gain position**: applied to M/S before the shuffler instead of after; identical output for an LTI system.
7. **Meter Mode** is GUI-only state saved with the plug-in state, not a host parameter. Meter ballistics
   (peak, ~24 dB/s fall, hold boxes in dB with one decimal, "-inf" when silent, holds reset when the mode is
   switched) are my choice; the spec only fixes the scale and semantics.
8. **Reset** affects Width, Asymmetry, Rotation, Shuffle only (per spec), not Frequency or Bass Trim; placed in
   the header since the spec does not fix its position.
9. **Value boxes**: single click starts typing (after the double-click interval), vertical drag adjusts,
   double-click / Alt-click resets. Typed Frequency accepts a `k` suffix.
10. **Vector display**: uses the quantised parameter targets (not the smoothed values); probe lines pointing
    beyond ±90° (e.g. polarity-inverted) are drawn below the base line, clipped to the display; drag sensitivity
    is one display width for the full horizontal range and 1.5 radii for the full vertical range.
11. **MS Matrix** has a host bypass parameter (otherwise no controls).
12. **Version numbering**: the binaries report 0.1.0; CI release tags are `v0.1.<run_number>`.
13. Fixed editor size (840×420 Full, 692×420 Imager, 420×220 MS Matrix); not resizable.

## Public references and provenance

- Spec §11: Blumlein GB 394,325 (1931) and Gerzon, JAES 42(6) 1994 for the shuffling concept; Zölzer, *DAFX*
  (bilinear transform, first-order shelving/all-pass sections). Only standard textbook material was needed: the
  filter itself is fully defined by the spec's `H(s)`/`H(z)`.
- JUCE 8 public API (juce.com documentation, `docs/CMake API.md` in the JUCE repository) for the plug-in, CMake
  and GUI code.
- clap-juce-extensions README (GitHub) for `clap_juce_extensions_plugin()`.
- Catch2 v3 documentation (GitHub) for test macros and `catch_discover_tests`.
- GNU AGPL-3.0 text from https://www.gnu.org/licenses/agpl-3.0.txt.
- Release archives/commit IDs were taken from the GitHub release/tag listings of the respective projects.

## Building

Requirements: CMake ≥ 3.22, a C++17 compiler, git (for clap-juce-extensions), internet for the first configure.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Artefacts: `build/<Target>_artefacts/Release/{VST3,CLAP,Standalone}/` for `CraftShuffler`, `CraftImager`,
`CraftMsMatrix`. Options: `-DCRAFT_BUILD_PLUGINS=OFF` (DSP tests only, no JUCE download — fast),
`-DCRAFT_BUILD_TESTS=OFF`, `-DCRAFT_BUILD_SNAPSHOT=ON` (editor PNG tool).

- **Windows**: from a "x64 Native Tools" prompt (`vcvars64.bat`), commands as above. MSVC builds use the static
  runtime. Install by copying the `.vst3` folders to `%LOCALAPPDATA%\Programs\Common\VST3` (or
  `C:\Program Files\Common Files\VST3`) and the `.clap` files to `%LOCALAPPDATA%\Programs\Common\CLAP`.
  When working from WSL, build from a `C:` path (MSVC does not handle `\\wsl.localhost` paths well).
- **Linux**: `sudo apt-get install ninja-build libasound2-dev libjack-jackd2-dev libfreetype-dev
  libfontconfig1-dev libx11-dev libxcomposite-dev libxcursor-dev libxext-dev libxinerama-dev libxrandr-dev
  libxrender-dev libglu1-mesa-dev` (WebKit/curl not needed: `JUCE_WEB_BROWSER=0`, `JUCE_USE_CURL=0`). Install to
  `~/.vst3` and `~/.clap`.

CI (`.github/workflows/release.yml`) builds and tests on `windows-latest` and `ubuntu-22.04` for pushes,
pull requests and manual runs; pushes to `main` (and manual runs on `main`) publish one GitHub release
`v0.1.<run_number>` with per-plug-in and combined zips for each platform.

## Status

- Windows x64 Release build (VST3, CLAP, Standalone) verified locally; 24/24 test cases pass (MSVC and GCC).
- Linux: DSP tests verified natively (GCC 13); plug-in sources syntax-checked with GCC against the JUCE
  headers. A full local Linux plug-in build was not run (the JUCE system packages need sudo); CI builds it.
- Not yet done: host-level validation (e.g. pluginval / a DAW session) and the independent verification
  against the reference (process step 5).
