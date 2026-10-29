# Implementation notes — Craft Peak Limiter (spec r1)

Clean-room implementation (builder side) of [`spec/peak-limiter/r1/SPEC.md`](../../spec/peak-limiter/r1/SPEC.md)
(SHA-256 `b25832e9…8f96778c`, approved 2026-10-29). Only the spec, `docs/process.md`, the repository's own code,
public documentation and this implementation were used. No original product, analyst workspace, vendor asset or
vendor installation was accessed.

Product: **Craft Peak Limiter** (`CPlm`, CLAP id `com.craftaudio.peak-limiter`, bundle id `audio.craft.CPlm`);
manufacturer **Craft Audio** (`Crft`); version 0.1.0. Formats: VST3, CLAP, Standalone. Platforms: Windows x64
(MSVC), Linux x64 (GCC). One plug-in serves both spec variants: the host chooses **stereo** (2 in / 2 out, linked)
or **mono** (1 in / 1 out) through the bus layout.

## Architecture

All paths are relative to `plugins/peak-limiter/`.

```
CMakeLists.txt     targets of this plugin (added automatically by the root CMakeLists.txt)
source/dsp/        host-independent, header-only DSP core (no JUCE)
  Parameters.h       §2 mappings r -> value with quantisation, inverse mappings, display formats, Settings
  Primitives.h       LookaheadConfig (§3/§4.5 lengths per rate), SlidingMin, Boxcar, DelayLine, Smoother
  ReleaseStage.h     §4 step 4 (fixed release) and §5 (Auto Release law, constants in AutoReleaseLaw)
  LimiterEngine.h    complete per-sample limiter: detection, hold, release, boxcars, D1 clamp, makeup, bypass
source/plugin/     JUCE AudioProcessor + parameter layout
source/ui/         editor, look-and-feel, meter faders, release knob, lamp switch, GR meter (all vector-drawn)
tests/             Catch2 unit tests of the DSP core (registered with CTest, prefix "peak-limiter/")
tools/Snapshot.cpp optional dev tool rendering the editor to PNG (-DCRAFT_BUILD_SNAPSHOT=ON)
```

### Signal flow (per sample, real time `t`; aligned time `n = t − L`)

1. Smoothed linear threshold `T_t` and ceiling `C_t` (one-pole, τ = 5 ms, snaps to the exact target).
2. Linked sample peak `p = max_c |x_c[t]|`, required gain `r[t] = min(1, T_t / p)`.
3. `h[n] = min(r[n+1..n+L])` = minimum of the last `L` required gains — a monotonic-deque sliding minimum
   (`SlidingMin`, O(1) amortised).
4. Release state `q[n]` from `h[n]` (`ReleaseStage`, fixed or auto).
5. `s[n] = MA_B(MA_A(q))[n]` — two causal boxcars (`Boxcar`: running sum, re-summed exactly once per buffer
   cycle so rounding cannot accumulate; division by the length, so a window of ones is exactly 1).
6. `g[n] = min(s[n], r[n])` (decision D1), output `y[n] = x[n] · g[n] · C_t / T_n` where `x[n]`, `r[n]` and
   `T_n` come out of `L`-sample delay lines. Using the delayed `T_n` keeps `|y| ≤ C_t` exact even while the
   threshold glides.

Internal arithmetic is double precision (the plug-in also supports 64-bit host buffers).

## How each spec section is implemented

**§1 Scope / variants.** `isBusesLayoutSupported` accepts mono→mono and stereo→stereo. Stereo uses one common
gain (max of both channels' peaks). Mono is the same engine with one channel (tested: identical to a stereo
run with the same signal on both channels). No dither / requantisation.

**§2 Parameters.** Host-normalised value = spec `r`. JUCE `NormalisableRange` lambdas apply the mapping **and**
the quantisation (Threshold/Ceiling 0.1 dB; Release `0.01·10^(5r)` ms rounded to 0.01 ms — the 2-decimal
displayed value is the internal value), so hosts and the GUI only ever see step values; the processor re-derives
the quantised values from `r` (`readSettings()`). Defaults: Threshold 0 dB, Ceiling 0 dB, Release 1.00 ms
(r = 0.4, decision D3), Auto Release off. Auto Release is a bool parameter (`r ≥ 0.5` = on); Bypass is the
host bypass parameter (`getBypassParameter`). Display: "−12.0" dB, "0.03" / "316.23" / "1000" ms.

**§3 Latency.** `LookaheadConfig::forSampleRate`: fs ≤ 50 kHz → L = 64 (A, B = 38, 27); ≤ 100 kHz → 128
(75, 54); above → 256 (150, 107). `setLatencySamples(L)` in `prepareToPlay`; the output is delayed by exactly `L`
(tested at 44.1/48/88.2/96/176.4/192 kHz).

**§4 Gain computer.** Steps 1–6 exactly as written (see signal flow). Fixed release:
`q = h if h < q, else h − (h − q)·c`, `c = exp(−ln10 / (Release_s·fs))` — the target is `h`, not 1 (click-pair
vector). Stream start (D2): `q = 1`, boxcar histories 1, required-gain history 1, audio history silent; i.e. the
model starts at real-time stream start (aligned `n = −L`), so a peak inside the first `L` samples is anticipated by
a partial ramp exactly as in a running stream.

**§5 Auto Release.** See "Auto Release law" below.

**§6 Parameter changes.** Threshold and Ceiling are smoothed in the linear domain (one-pole τ = 5 ms, ≈ 99 %
after 23 ms, i.e. "~20 ms"); Release and Auto Release apply on the next sample. Switching Auto Release on starts
both auto branches from the current release state; switching it off continues the fixed law from `min(fast, slow)`.

**§7 Out of scope.** Not implemented: dither, requantisation, true-peak detection.

**§8 Tolerances.** All verified by tests (below). The ceiling guarantee holds by construction:
`|x[n]|·r[n] ≤ T_n`, so `|y| ≤ C_t` up to one rounding (tests: ≤ 1e-6 relative on hot programme with clicks
up to +17 dBFS, 0.01…316 ms, fixed and auto, 44.1…192 kHz).

**Host bypass.** 10 ms linear crossfade between the limited signal and the dry signal delayed by `L` (latency
stays constant, so bypassing never shifts the timeline); bit-exact delayed dry once faded. `processBlockBypassed`
(hosts bypassing without our parameter) runs the same path with bypass forced on.

## Auto Release law (spec §5, open question 1)

The spec fixes only the attack/look-ahead/smoothing (unchanged) and the §5 recovery table; the law is mine.
Reading the table (all in the linear-gain deficit, times from the end of the overload) suggested two components:
after long overloads the recovery is close to linear in dB at ≈ 8–9 dB per Release time; after short overloads a
fast part recovers most of the reduction, and the remaining tail again falls linearly in dB from a level that grows
with the overload duration (≈ 2 dB after 5 ms, ≈ 4 dB after 100 ms, ≈ 10 dB after 500 ms at 12 dB over); the
growth is roughly exponential in the *linear* gain deficit with a time constant of ≈ 0.2 s that does not scale with
Release. `ReleaseStage` therefore runs two branches on the hold target `h` (both with instantaneous attack, so the
attack is identical to the fixed mode) and uses `q = min(fast, slow)`:

- **fast**: recovers toward `h` linearly in dB at `fastDbPerRelease` = 117 dB per Release time;
- **slow** ("overload memory"): while `h` is below it, it immediately takes `instantFraction` = 0.227 of the
  target's linear-gain deficit and then charges its deficit toward `h` with τ = `chargeMs` = 238 ms (absolute
  time); when `h` is above it, its GR in dB shrinks per Release time by `slowDbPerRelease` = 8.35 dB plus
  `slowExpPerRelease` = 0.16 × its current GR (slightly faster for deep reduction).

Recovery is monotonic and continuous (the output is still boxcar-smoothed); Release scales both release rates, so
it remains the governing time. The constants were fitted (Nelder–Mead, minimax-weighted, offline in a scratch
script — not shipped) to the 45 numbers of the §5 table by simulating exactly this engine at 48 kHz. Result
(measured by the unit test, t63 / t90 / t99 deviation vs. the table):

| Release | Overload | t63 | t90 | t99 |
|---|---|---|---|---|
| 31.62 ms | 5 ms | +12 % | +3 % | +13 % |
| 31.62 ms | 20 ms | +7 % | +12 % | +10 % |
| 31.62 ms | 100 ms | +5 % | +5 % | +5 % |
| 31.62 ms | 500 ms | −8 % | −6 % | −6 % |
| 31.62 ms | 2000 ms | −9 % | −7 % | −6 % |
| 31.62 ms | 500 ms, 6 dB | −8 % | −5 % | −5 % |
| 31.62 ms | 500 ms, 24 dB | −8 % | −7 % | −6 % |
| 316.23 ms | 5 ms | +11 % | −14 % | −8 % |
| 316.23 ms | 20 ms | +3 % | −10 % | −6 % |
| 316.23 ms | 100 ms | −12 % | −4 % | −3 % |
| 316.23 ms | 500 ms | −10 % | −7 % | −6 % |
| 316.23 ms | 2000 ms | −9 % | −7 % | −6 % |
| 316.23 ms | 500 ms, 6 dB | −9 % | −6 % | −5 % |
| 316.23 ms | 500 ms, 24 dB | −11 % | −9 % | −8 % |
| 1.00 ms | 500 ms | +16 % | +7 % | +3 % |

Worst case 15.7 % (tolerance ±20 %). The 1 ms row is dominated by the 1.3 ms look-ahead smoothing and is the
least robust (one sample = 1.8 %). At 44.1 and 96 kHz the same table holds within ±25 % (tested; the spec's table
is for 48 kHz).

## GUI (spec §12)

Original vector-drawn design in the Craft Audio house style (graphite panels, amber/teal accents, no bitmaps or
bundled fonts); own logo mark (a waveform flattened under a bar). Layout left to right, as the spec requires:

- **Threshold**: vertical fader (0 … −30 dB, scale 0/6/12/18/24/30) drawn as a level line with a grip tab directly
  over the input meters (two bars for stereo, one for mono), value box below.
- **Out Ceiling**: the same over the output meters (teal handle).
- **Release**: rotary knob, logarithmic in `r`, tick ring, value arc; the value is shown and editable in its centre
  ("ms" under it); end labels "0.01" and "1000". Below it the **Auto Release** on/off lamp switch.
- **Gain reduction**: vertical bar 0 … 30 dB hanging from the top ("ATTEN"), peak marker, numeric **peak GR**
  readout below (click the meter to reset).
- Header: product name, version; the release panel also shows the current look-ahead (samples @ rate).

Interaction: faders and knob drag (Shift on value boxes = fine), double-click or Alt-click resets every control to
its default; value boxes accept typed values (single click) and vertical drag. Meter ballistics (display only,
§7): instant attack, ~27 dB/s fall, 1.5 s peak line; GR meter ~30 dB/s return.

## Tests (`tests/`, Catch2)

23 test cases / 276 212 assertions; all pass with MSVC 2022 (Release, via CTest) and GCC 13 (Linux).

- **Latency** (§3): 64/64/128/128/256/256 at 44.1/48/88.2/96/176.4/192 kHz, `A + B − 1 = L`, an impulse
  appears exactly `L` samples later.
- **Bit-exact** passthrough at Threshold = Ceiling = 0 without overs (4 rates); fs/4 sine with 0.707 sample
  peaks passes unchanged (sample-peak detection only).
- **Static curve** (§9 table, 1 kHz sine, all 30 cells) within ±0.005 dB.
- **Attack S-curve** (§9) at 48/44.1 kHz and 96/88.2 kHz for three click heights within ±6e-6; gain starts moving
  exactly `L` samples before the peak and reaches `g_min` one sample before it.
- **Release trajectory** and **click pair** (§9) within ±6e-6 (click-pair k = 0 is the D1-clamped 0.125594,
  i.e. within 1.5e-5 of the reference's 0.12560).
- **Model match** (§8): engine vs. an independent brute-force implementation of §4 (written in the test helpers),
  5 rates × stereo/mono × 6 random settings on programme-like noise with clicks, odd block sizes:
  error ≤ −100 dB re output peak (actual ≈ −300 dB, i.e. rounding).
- **Ceiling** never exceeded by more than 1e-6 relative: 4 rates × 5 Release values × fixed/auto × 5
  threshold/ceiling pairs on hot programme.
- **Link**: a click on L only reduces L and R identically. **Mono** equals stereo with identical channels.
- **Smoothing** (§6): Threshold/Ceiling jump is click-free (largest output step < 0.01), settled within 1 % after
  25 ms. **Bypass**: dry signal delayed by exactly L.
- **Auto Release** (§5/§8): all 45 table values within ±20 % with monotonic recovery; 44.1/96 kHz within ±25 %;
  longer overload / longer Release recover more slowly; attack identical to fixed mode; click-free on programme.
- **Parameters** (§2): mappings, 0.1 dB / 0.01 ms quantisation, default Release 1.00 ms at r = 0.4, display formats.
- **Primitives**: sliding minimum and boxcar against brute force; boxcar exact for constant windows; delay line.

## Questions / assumptions

1. **Auto Release law** (open question 1): own two-branch law fitted to the §5 table (details above).
2. **176.4/192 kHz** (open question 2): L = 256 with boxcars (150, 107) as the spec extrapolates; any rate above
   100 kHz uses these, any rate ≤ 50 kHz uses L = 64.
3. **Mono** (open question 3): same algorithm on one channel, offered as the mono bus layout of the same plug-in
   (no separate product / plug-in code).
4. **Parameter smoothing** (open question 4 / §6): one-pole τ = 5 ms on the linear threshold and ceiling.
   During a glide the output stays under the *current* (gliding) ceiling; a downward ceiling move reaches the new
   ceiling after ~20 ms.
5. **Stream start** (D2): interpreted at real-time stream start (see §4 above); the reference behaviour there is
   unspecified and excluded from the tolerance.
6. **Host bypass**: 10 ms crossfade to the latency-compensated dry signal.
7. **Meter scales/ballistics**: input/output meters use the faders' 0 … −30 dB scale (signal below −30 dBFS shows
   empty); GR meter 0 … 30 dB. Peak GR readout shows the largest reduction since the last click.
8. **Release knob mapping**: the knob is linear in the host-normalised value (i.e. logarithmic in ms).
9. **Release 1000 ms display**: "1000" (no decimals), all other values two decimals, as listed in §2.

## Public references and provenance

- Look-ahead limiting with a hold (sliding minimum) followed by smoothing: standard textbook/paper material —
  U. Zölzer (ed.), *DAFX: Digital Audio Effects*, 2nd ed., Wiley 2011, ch. 4 (dynamics processing, look-ahead
  limiter); D. Giannoulis, M. Massberg, J. D. Reiss, "Digital Dynamic Range Compressor Design — A Tutorial and
  Analysis", JAES 60(6), 2012 (gain computer / release smoothing and program-dependent release concepts).
- Sliding-window minimum with a monotonic deque: D. Lemire, "Streaming Maximum-Minimum Filter Using No More than
  Three Comparisons per Element", Nordic Journal of Computing 13(4), 2006 (arXiv cs/0610046).
- JUCE 8 public API documentation (juce.com) and `docs/CMake API.md`; clap-juce-extensions README; Catch2 v3 docs.
- The repository's own `plugins/stereo-imager` code (structure, value box and look-and-feel conventions).

## Building

Same as every plugin in the monorepo (from the repository root):

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build -R peak-limiter --output-on-failure
```

Artefacts: `build/plugins/peak-limiter/CraftPeakLimiter_artefacts/Release/{VST3,CLAP,Standalone}/`.
`-DCRAFT_BUILD_PLUGINS=OFF` builds only the DSP tests (no JUCE); `-DCRAFT_BUILD_SNAPSHOT=ON` adds
`peak_limiter_snapshot` (renders the editor to PNG). `ci/package.py` picks the plug-in up automatically
(it scans for `*_artefacts/Release`), producing `craft-peak-limiter-<version>-<platform>.zip`.

## Status

- Windows x64 Release build (VST3, CLAP, Standalone) verified locally with MSVC 2022, no compiler warnings in
  this plugin's sources; 23/23 test cases pass (MSVC and GCC 13). Editor rendered with the snapshot tool (stereo,
  mono, limiting) and checked visually. `ci/package.py` finds VST3 + CLAP + Standalone and writes
  `craft-peak-limiter-0.1.0-windows-x64.zip`.
- Linux: DSP tests verified natively (GCC 13). A full local Linux plug-in build was not run (JUCE system packages
  need sudo); CI builds it.
- Not yet done: host-level validation (pluginval / DAW session) and the independent verification against the
  reference (process step 5).
