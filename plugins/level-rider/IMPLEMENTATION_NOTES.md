# Implementation notes — Craft Level Rider (spec r1)

Clean-room implementation (builder side) of [`spec/level-rider/r1/SPEC.md`](../../spec/level-rider/r1/SPEC.md)
(approved 2026-10-29). Only the spec, this repository, public DSP references and this implementation were used.
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

**§6 Dynamics.** Structure as described in the spec: level detector on the block power (attack one-pole
when the block power exceeds the state, else hold, then release one-pole toward the block power, all on
power), `L = 10·log10(state)`, `g` follows `g*` through a one-pole in dB with `τ_up` / `τ_down`, then
clamped to [Range Min, Range Max]. The structure is non-binding and its constants a starting point; with the
analytic-power front end the constants were re-fitted to the normative tables T2/T3 (Nelder–Mead on the T2
equilibria and T3 trajectories, 48 kHz, both modes):

| Constant | Spec Slow | **Used Slow** | Spec Fast | **Used Fast** |
|---|---|---|---|---|
| τa attack | 71 ms | **51.2 ms** | 2.1 ms | **1.37 ms** |
| h hold | 50 ms | **78 ms** | 450 ms | **482 ms** |
| τr release | 0.94 s | **1.004 s** | 1.30 s | **1.502 s** |
| τ_up | 0.45 s | **0.492 s** | 0.79 s | **0.751 s** |
| τ_down | 52 ms | **43 ms** | 15 ms | **14.3 ms** |

**§7.1 Absolute threshold.** `ActivityGate`: one-pole (τ = 0.1 s) on the block power, riding allowed above
`θ_on = −62 − Vocal Sensitivity`, stopped below `θ_on − 4 dB`. D2: the formula applies at all settings.

**§7.2 Relative threshold.** The reference is the running maximum of the 1 ms block level, decaying at
1.5 dB/s while not exceeded; a short-term level (power one-pole, τ = 10 ms) more than 24 dB below it is
non-vocal (Idle). The short-term level makes the idle decision fast enough for T4 (fader movement starts
0.5 s after the fall in Slow, almost at once in Fast, which the 0.1 s absolute level alone cannot do for a
−9 → −80 dBFS fall) without single-block dips toggling the gate.

**§7.3 Idle.** While idle the level detector is **frozen** (it keeps the level of the last phrase) and `g`
holds; Slow waits 0.5 s, Fast moves at once, then `g` moves toward the Idle value (clamped into the range) at a
fader-position-dependent speed fitted to T4 (`idleSpeedDbPerSecond`: 6.35 + 0.115 g − 0.0098 g² dB/s, i.e.
≈ 6.4–6.7 dB/s at and above 0 dB and ≈ 3.6–5 dB/s below −6 dB, the same in both directions), stopping exactly
at Idle. When riding resumes it starts from the current `g`. At start-up `g = Idle`, the gate is Idle and the
detector empty. The frozen detector is what reproduces T2 column "−3 → −33": on re-acquisition the rider first
rides down from Idle (the detector still holds the loud phrase) and then up as the detector releases.

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
- `DynamicsTests` — T2 (all 7 rows × 18 segments) and T3 (Slow/Fast, ±1.5 dB or ±30 % time), rate
  independence of the first T2/T3 segments at 44.1/96 kHz, summary count.
- `GateTests` — T4 (±1.0 dB, stops exactly at Idle), T5 (state at the end of each step, adjacent cells exempt,
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

Build/test status (2026-10-29): Windows x64 MSVC Release build of both products (VST3, CLAP, Standalone);
41/41 CTest cases pass on Windows (MSVC) and Linux (GCC, DSP tests). `craft_hoststress --editor` 240 s on
each VST3 (44.1/48/96 kHz, float/double, random blocks, state restores, editor open/close): 0 failures.
The Linux plug-in targets were not built locally (CI covers them).

All tables are checked by the unit tests (48 kHz unless stated; 44.1/96 kHz where noted).

| Table | Result |
|---|---|
| T1 static law (±0.25 dB) | all 60 cells pass, at 44.1, 48 and 96 kHz (formula max error 0.12 dB) |
| T6 weighting (±1.0 dB) | all pass (through the engine at 48 kHz; 125/250/16k Hz also at 44.1/96 kHz) |
| T4 idle (±1.0 dB) | all 120 cells pass; the fader stops exactly at Idle |
| T5 gate state | all non-adjacent cells pass (rows −6, 0, +6, +12, 0 Fast; row −12 informative per D2) |
| T2 equilibria (±0.6 dB) | 118 of 126 cells within tolerance |
| T3 trajectories (±1.5 dB / ±30 % time) | 241 of 260 cells within tolerance |
| Pure gain, latency 0, Output exact, sidechain bit-exact, rates | pass |

**Documented deviations** (checked by the tests at a wider bound so regressions are still caught):

1. **T2 segment 15 ("−3 → −33", the only 30 dB drop)** — a transitional value 12 s after the drop that depends
   on the instant of re-acquisition after the relative rejection (§7.2) and on the frozen detector afterwards.
   Deviations: ±12 +0.3, ±6 +0.4, ±3 −0.8, Target −12 +1.3, Target −33 −1.8, ±12 Fast −3.2, ±6 Fast −1.2 dB.
   Experiments showed the reference's implied re-acquisition time differs between configurations in a way
   no single deterministic rule in this structure reproduced (it depends on Target, which the gate does not
   see), so the spec's figures (24 dB, 1.5 dB/s) are kept literally; the §7.2 prose (idle within ≈ 3 s,
   resume after 4–10 s, ≤ 12 dB drops never idle) is met and tested. The T3 row "−33 → −39" starts from
   this state and inherits the offset (Fast: up to 5 dB in the first seconds, then converges).
2. **T3 upward rides after 6 dB drops at 1–1.5 s** (Slow −15→−21 / −21→−27: ≈ 1.6–2.2 dB slower than the
   reference; Fast the same rows at 1.5–2 s and −21→−33 at 2 s) — the reference releases faster after small
   drops than after large ones, which the non-binding single-release structure cannot do at the same time
   as the 12 dB rows and the T2 equilibria. All other T3 cells of these rows pass.
3. **T2 ±12 Fast at −15 dBFS (segments 4, 12)**: +0.7 dB (tolerance 0.6).

Fitting procedure: the non-binding §6 constants were optimised (Nelder–Mead) to minimise the squared T2/T3
deviations with the cells of item 1 excluded; the noise realisation is fixed (portable Gaussian generator), so
results are reproducible bit-for-bit across platforms.

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

1. **Analytic-power detector** (§4 "a steady sine is read at its weighted RMS level") instead of the plain
   1 ms block mean square of the non-binding structure; constants re-fitted accordingly (§6 table above).
2. **Weighting corners** fitted to T6 (the spec allows adjusting the starting point).
3. **Relative threshold details** (§7.2, open question 1): reference = running max of the 1 ms block level
   (−1.5 dB/s), compared with a 10 ms level; no extra re-acquisition delay. The spec's figures 24 dB and
   1.5 dB/s are used literally for both modes.
4. **Detector frozen while idle** (inferred from T2 "−3 → −33": the rider rides *down* after re-acquisition).
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
