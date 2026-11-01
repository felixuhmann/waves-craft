# Implementation notes — Craft Level Rider (spec r2)

Clean-room implementation (builder side) of [`spec/level-rider/r2/SPEC.md`](../../spec/level-rider/r2/SPEC.md)
(approved 2026-10-29, supersedes r1; r1 verification:
[docs/verification/level-rider-r1.md](../../docs/verification/level-rider-r1.md)). Only the spec, this repository, public DSP references and this implementation were used.
No original product, analyst workspace, vendor asset, manual or installation was accessed or run.

Products (one shared engine, like the imager variants):

| Product | Code | CLAP id | Difference |
|---|---|---|---|
| **Craft Level Rider** | `CLrd` | `com.craftaudio.level-rider` | standard product |
| **Craft Level Rider Live** | `CLrl` | `com.craftaudio.level-rider-live` | adds the *Spill* gate (§7.4) |

Manufacturer **Craft Audio** (`Crft`), version 0.1.0. Formats: VST3, CLAP, Standalone. Platforms: Windows x64
(MSVC), Linux x64 (GCC). Stereo is the primary layout; mono is the same product with a mono main bus
(in = out, mono or stereo). An optional **Music** sidechain input bus (mono or stereo, disabled by default)
feeds §8. Licence: AGPL-3.0 like the rest of the repository (JUCE), see [THIRD_PARTY.md](../../THIRD_PARTY.md).

## Architecture

All paths are relative to `plugins/level-rider/`.

```
CMakeLists.txt       targets (level_rider_add_variant), header-only DSP library level_rider_dsp
source/dsp/          host-independent DSP core (no JUCE), double precision
  Parameters.h         §2 mappings (r -> value, 0.1 dB), Range rule, Settings, AutomationMode
  Filters.h            §4 weighting (HP + shelf, fitted to T6), 90-degree all-pass pair (analytic power)
  Dynamics.h           §5 static law, §6 level detector + constants, §7 activity gate, §7.3 idle speed
  RiderEngine.h        the complete processor: 1 ms blocks, gain interpolation, idle, §8 music, §9 modes, D6
source/plugin/       JUCE AudioProcessor (one class, Live chosen per target), parameter layout
source/ui/           editor, look-and-feel, controls, Target bar and faders (all vector-drawn)
tests/               Catch2 tests against T1-T6 and the §1/§3/§8/§9 rules (registered with CTest)
tools/Snapshot.cpp   optional dev tool rendering the editor to PNG (-DCRAFT_BUILD_SNAPSHOT=ON)
```

`ci/package.py` needs no change: it scans every `*_artefacts/Release` directory and finds
`Craft Level Rider` and `Craft Level Rider Live` (unique product names → `craft-level-rider-*.zip`,
`craft-level-rider-live-*.zip`).

## How each spec section is implemented

**§1 Scope / §3 signal flow.** `RiderEngine::process`: the detector input is the channel mean
`(L+R)/2` (D5), weighted (§4) and converted to analytic power; the power is averaged over blocks of
`round(fs · 1 ms)` samples (48 at 48 kHz, 44 at 44.1 kHz, 96 at 96 kHz; every time constant uses the true
block duration, so times are identical in seconds at all rates). At the end of each block the gate, detector,
static law, gain smoothing / idle movement are updated once; the applied linear gain
`10^((g + Output)/20)` is interpolated linearly per sample from the previous block's value to the new one.
Output = input × that gain on every channel — no filter, delay or other processing reaches the output.
Latency 0 (`getLatencySamples() == 0`). A constant gain is applied as an exact scalar (Output +6 gives
+6.000 dB; with the sidechain inactive the output is bit-identical, §8).

**§2 Parameters.** All host-normalised values are the spec's `r`. Mappings and 0.1 dB quantisation in
`Parameters.h`, applied by the JUCE `NormalisableRange` lambdas: Target `−42+42r`, Vocal Sensitivity
`−12+24r`, Music Sensitivity `r = 0 → Off` else `−12+24r` (physical value of *Off* is −12.1 so that Off and
−12.0 dB stay distinct; the processor decides on the normalised value), Range Max `−11+23r` (default +6),
Range Min `−12+23r` (default −6), Idle `−11.5+23r` (no unit), Rider `−12+24r`, Output `−12+24r`, Attack a
two-entry choice (r < 0.5 Slow), Spill `−48+36r` (Live only). Plus a host Bypass (10 ms fade to an exact unity
gain). **Range rule:** `resolveRange()`; the processor tracks which of Min/Max changed last on the audio
thread, uses the resolved pair immediately and pushes the losing parameter by the 1 dB minimum distance from
its message-thread timer (with begin/end gestures).

**§4 Weighting (T6).** First-order high-pass at 246.4 Hz in series with a first-order high-frequency shelf
(pole 4678 Hz, zero 10068 Hz), bilinear with pre-warping at each corner, normalised to 0 dB at 1 kHz. The spec's
starting point (HP 230 Hz + LP 8.5 kHz) misses T6 by up to ~1 dB at 4–16 kHz; the corners were least-squares
fitted to T6 at 44.1/48/96 kHz together (max error 0.29 dB). **"A steady sine is read at its weighted RMS
level":** a 1 ms block mean square of a low-frequency sine ripples (125 Hz would read +2.8 dB with a peak
detector), so the detector uses the *analytic power* `(a² + b²)/2` of a pair of all-pass chains 90° apart
(Olli Niemitalo's public-domain 4+4 section coefficients, ±0.7° from 0.0005·fs to 0.4995·fs). For a sine this
power is constant, so a steady sine is read at exactly its weighted RMS at every frequency and rate; for noise
it fluctuates like the block mean square, so the peak-weighted reading of noise (§6) is preserved.

**§5 Static law (normative).** `StaticLaw::gainDb` is the spec formula verbatim (a = 0.5162, b = 0.7252,
`W`, `c`, clamp to the range). The engine applies it to the detector level `L` and the (music-shifted, §8)
target.

**§6 Dynamics.** Level detector on the block power: attack one-pole when the block power exceeds the state, else
hold, then release; `L = 10·log10(state)`; `g` follows `g*` through a one-pole in dB with `τ_up` / `τ_down`, clamped to
the range. The §6 structure is non-binding and the tables govern, so this build adds a **program-dependent release**:
blocks within `nearDb` below the state release with a short time constant, blocks more than `farDb` below with a long
one, others with `τr`. It makes 6 dB upward rides quick and lets the wait after a large drop grow with its size. All
constants were fitted (Nelder–Mead, limited iterations) to T2, T3, T4 and T7 at 44.1, 48 and 96 kHz together:

| Constant | Spec Slow | **Used Slow** | Spec Fast | **Used Fast** |
|---|---|---|---|---|
| τa attack | 71 ms | **56.8 ms** | 2.1 ms | **1.28 ms** |
| h hold | 50 ms | **91.3 ms** | 450 ms | **561 ms** |
| τr release | 0.94 s | **1.58 s** | 1.30 s | **1.56 s** |
| near: range, τ | – | **11.5 dB, 0.404 s** | – | **9.7 dB, 0.299 s** |
| far: range, τ | – | **16.5 dB, 2.38 s** | – | **18.5 dB, 3.41 s** |
| τ_up | 0.45 s | **0.519 s** | 0.79 s | **0.806 s** |
| τ_down | 52 ms | **51.3 ms** | 15 ms | **9.1 ms** |

**§7.1 Absolute threshold.** `ActivityGate`: one-pole (τ = 0.1 s) on the block power, riding allowed above
`θ_on = −62 − Vocal Sensitivity`, stopped below `θ_on − 4 dB`. D2: the formula applies at all settings.

**§7.2 Relative threshold (r2).** Lref is a running maximum that decays while not exceeded, also while Idle. Idle starts
when the 10 ms level falls more than the entry value below Lref and ends when it is back within the re-entry value.
When riding resumes after a relative Idle the detector restarts from Lref + seed; after an absolute-threshold Idle
(silence, start-up) it restarts from the current 10 ms level. The detector is not frozen while Idle. Fitted per mode:

| | Lref source | entry | re-entry | decay | seed |
|---|---|---|---|---|---|
| Slow | 0.1 s level | 24.7 dB | 16.9 dB | 1.61 dB/s | +5.25 dB |
| Fast | 1 ms block level | 23.6 dB | 19.5 dB | 1.60 dB/s | +2.09 dB |

The Slow re-entry value is outside the spec's 18–21 dB in numbers, but it is measured from the 0.1 s level rather than
the higher block maximum, and the seed offsets that. The two modes use different Lref sources because that fitted best,
not because the spec implies it.

**§7.3 Idle.** Slow holds `g` for 0.5 s, Fast moves at once, then `g` moves toward the Idle value (clamped into the range)
at a position-dependent speed fitted to T4 (`idleSpeedDbPerSecond`), the same in both directions, stopping exactly at
Idle. Riding resumes from the current `g`. At start-up `g = Idle`.

**§7.4 Spill (Live).** Riding additionally requires the 0.1 s level to exceed Spill (on above Spill, off below
Spill − 3 dB, assumption). At the default −48 dBFS the Live product rides vocal-level material exactly like the
standard one (tested: identical gain traces).

**§8 Music sidechain (D3).** Optional bus "Music". Detector = same weighting + analytic power + level detector
with Slow constants, `M̄` = one-pole (τ = 10 s) on `M` in dB, `k = 10^(MusicSens/20) · 0.5`,
`T_eff = T + clamp(k (M − M̄), ±6 dB)`. "No sidechain signal" = bus absent/disabled or an all-zero block; then,
or with Music Sensitivity Off, the offset is exactly 0.0 and the output bit-identical (tested). When the music
(re)appears after ≥ 1 s of absence, detector and average are seeded with the current level, so the start of
the music does not produce a spurious offset. The GUI indicator shows `|T_eff − T|`.

**§9 Rider parameter and automation.** The Rider parameter (automatable, −12…+12 dB) reflects `g`.
Automation mode (GUI, saved in the plug-in state as a ValueTree property, not a host parameter):
- **Off** (default): internal engine. The Rider parameter's value follows `g` *silently* (`setValue` from a
  30 Hz message-thread timer, no host notification): hosts that query parameter values read the current ride
  gain, but nothing is recorded as automation.
- **Write**: the timer opens a change gesture when Write is selected and sends `g` with
  `setValueNotifyingHost` (≥ 0.05 dB changes, ~30 Hz) until another mode is selected, so hosts in
  latch/touch/write automation modes record the ride. Hosts that echo the value back to the processor are
  ignored in this mode.
- **Read**: `g` follows the host's Rider value (5 ms smoothing); the engine (gate/detector) still runs for the
  displays but does not move `g`.
Dragging the Rider fader (or typing a value) overrides `g` while held, in any mode; riding resumes from that
value on release. Double-click returns `g` to Idle.

**D6.** Target, Range Min/Max, Idle and Output are ramped linearly over 20 ms (20 detector blocks); other
parameters take effect at the next block.

## User interface (§13)

Original vector artwork in the shared Craft palette (dark panels, amber/teal accents). Top: Target as a
horizontal bar (−60…0 dBFS) with the detected vocal level (0.1 s level, teal bar + peak tick) and the Target
handle/flag, typable value box; RIDE / IDLE lights; Attack SLOW | FAST switch; knobs Vocal Sens, Music Sens
(+ Spill in Live) with typable boxes; music-activity light whose brightness follows `|T_eff − T|` plus a text
readout. Main: Range fader (Max / Min handles, teal Idle arrow, MAX / IDLE / MIN boxes, "effective range"
readout), the large Rider fader (±12 dB, moves with `g`, teal band = range, Idle marker, typable value), Output
fader (±12 dB) with output peak strip and latched CLIP light (click clears). Bottom: Automation OFF / WRITE /
READ. Every value box: click to type, drag to change, double-click (or Alt-click) resets.

## Tests

`tests/` (Catch2, CTest prefix `level-rider/`):
- `ParameterTests` — §2 mappings, defaults, Range rule.
- `StaticLawTests` — T1: formula vs table, and 1 kHz sine steps through the engine at 44.1/48/96 kHz
  (±0.25 dB), steady gain identical across rates (≤ 0.05 dB).
- `DetectorTests` — T6 through the engine (±1.0 dB; 63 Hz rides at +12), weighting response at the three rates,
  ripple-free analytic power (63 Hz–9 kHz), noise read above RMS.
- `DynamicsTests` — T2 (7 rows × 18 segments), T3 and T7 (Slow/Fast, ±1.5 dB or ±30 % time) at 44.1/48/96 kHz;
  tables and evaluation in `Tables.h`, open misses in `KnownDeviations.h`.
- `GateTests` — T4 (±1.0 dB, 3 rates, stops exactly at Idle), T5 (3 rates, state at the end of each step, adjacent cells exempt,
  row −12 informative per D2), absolute threshold/hysteresis, start-up `g = Idle`, short gaps, relative rejection
  (30 dB drop: idle < 3 s, resume 4–10 s; drops ≤ 12 dB never idle), Live Spill.
- `EngineTests` — pure gain (identical L/R gain, residual vs per-block gain ≤ −120 dB, settled gain is a scalar
  per 5 ms window), impulse without delay, Output trim exact, mono = stereo with L = R, host block-size
  independence, float = double path, §8 bit-exact inactive sidechain and offset rule/limit, §9 Read/override,
  bypass exact unity, D6 smoothing.

Stimuli follow §12 (band noise = Gaussian noise through 4th-order Butterworth 300 Hz HP + 3 kHz LP, gain =
20 log10 of output/input RMS per 5 ms window). The Gaussian generator is a Box–Muller transform of
`std::mt19937` so the noise — and therefore every table value — is identical on every platform/standard library.

## Verification status

**Spec r2 is not fully met.** 41 of 2022 checked T2/T3/T4/T7 cell-and-rate combinations (2.0 %) miss the §11 tolerance.
They are listed one by one, with values, in `tests/KnownDeviations.h`. The tests pass only because of that list: a listed
cell has to stay within 7 dB, and every unlisted cell must meet the spec tolerance. It records open non-conformance and
is not an accepted tolerance.

| Table (44.1 + 48 + 96 kHz) | Cells outside tolerance |
|---|---|
| T1, T5, T6 | 0 (T6 through the engine at 48 kHz; low and top frequencies also at 44.1 / 96 kHz) |
| T4 | 0 of 360 (see assumption on the stimulus below) |
| T2 | 16 of 378 |
| T3 | 13 of 780 |
| T7 | 12 of 504 |

Remaining misses:
- **T2 "−3 → −33"** (segment 15 in the test numbering, column 14 of the spec): ±3, Target −12 and Target −33, up to 2.0 dB.
- **T7 Slow −9→−27 at 3–5 s**: the rider rises too early at all rates, 2–5.4 dB above the table.
- **T3 −33→−39**: Slow at 3–4 s, up to 6.8 dB at 96 kHz; Fast at 2–3 s, up to 1.8 dB off.
- **T3 Fast −21→−33 at 1–4 s, 96 kHz only**: up to 5 dB. T3 Fast −39→−21 at 0.03 s, 44.1 kHz only.
- **T2 ±12 Fast at −15 dBFS and after −33→−39, ±12/±6 Fast segment 9**: 0.7–0.85 dB.
- **T7 Fast −3→−39 at 19.5 s**: up to 3.9 dB.

Sample-rate consistency improved but is not solved: the stimuli differ per rate because the noise is generated at each
rate, and some cells fail at only one rate. The T4 and T7 results depend on this builder's reconstruction of the stimuli,
and the fit used the same noise as the tests, so host renders against the reference may differ from these counts.

## Public references

- R. Bristow-Johnson, *Audio EQ Cookbook* (test-stimulus biquads).
- Bilinear transform with frequency pre-warping (e.g. Oppenheim & Schafer, *Discrete-Time Signal Processing*).
- O. Niemitalo, "Hilbert transform IIR filter" (public-domain 90° all-pass pair coefficients), yehar.com.
- Analytic signal / envelope detection: e.g. J. O. Smith, *Spectral Audio Signal Processing* (online).
- Peak/RMS level detectors with attack/hold/release, automatic gain control: D. Giannoulis, M. Massberg,
  J. D. Reiss, "Digital Dynamic Range Compressor Design — A Tutorial and Analysis", JAES 60(6), 2012.
- Nelder–Mead simplex (SciPy) for fitting the non-binding constants to the normative tables.
- JUCE 8 and clap-juce-extensions documentation (bus layouts, parameter gestures, `getBusBuffer`).

## Questions / assumptions

0. **T4 stimulus**: the spec does not give the burst or gap length. The tests render the bursts −9, −33, −21 dBFS in
   sequence per configuration, 12 s each, each followed by 6 s of −80 dBFS noise.

1. **Analytic-power detector** (§4 "a steady sine is read at its weighted RMS level") instead of the plain
   1 ms block mean square of the non-binding structure; constants re-fitted accordingly (§6 table above).
2. **Weighting corners** fitted to T6 (the spec allows adjusting the starting point).
3. **Relative threshold details** (§7.2): sources and values in the table above, fitted per mode.
4. **Program-dependent release** (near / far tiers) is this build's addition to the non-binding §6 structure.
   The r1 assumption "detector frozen while idle" was refuted by verification and removed.
5. **Idle speed** depends on the fader position (fitted to T4); Fast has no hold, Slow 0.5 s.
6. **Idle value clamped into [Range Min, Range Max]** so the fader never jumps when riding resumes.
7. **Spill hysteresis** 3 dB, Spill measured on the 0.1 s level (§7.4 not measured).
8. **Music sidechain** (§8, D3): `M̄` is a 10 s one-pole on `M` in dB; "connected" = bus enabled and non-zero
   samples; detector/average re-seeded after ≥ 1 s of silence.
9. **Mono**: same product with a mono main bus (in = out); mono-in / stereo-out is not offered.
10. **Automation modes** (§9) as described above; Off updates the Rider value silently (readable, not
    recorded). Hosts that do not query parameter values show the ride gain only in the plug-in GUI. Read mode
    applies the host value with 5 ms smoothing.
11. **Music Sensitivity −12.0 dB vs Off**: Off is r = 0 exactly (physical −12.1 internally).
12. **Bypass**: host bypass parameter with a 10 ms fade to an exact unity gain (detection keeps running).
