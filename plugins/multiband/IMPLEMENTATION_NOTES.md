# Implementation notes — Craft Multiband (spec r1)

Clean-room implementation (builder side) of [`spec/multiband/r1/SPEC.md`](../../spec/multiband/r1/SPEC.md)
(SHA-256 `2733d3bb…74eb6bf`, approved 2026-10-29). Only the spec, `docs/process.md`, the repository's own code,
public documentation / literature and this implementation were used. No original product, analyst workspace,
vendor asset or vendor installation was accessed.

Product: **Craft Multiband** (`CMbd`, CLAP id `com.craftaudio.multiband`, bundle id `audio.craft.CMbd`);
manufacturer **Craft Audio** (`Crft`); version 0.1.0. Formats: VST3, CLAP, Standalone. Platforms: Windows x64
(MSVC), Linux x64 (GCC). Stereo (linked) is the primary layout; mono is offered through the bus layout of the same
plug-in (spec open question 4: same processing on one channel).

## Architecture

All paths are relative to `plugins/multiband/`.

```
CMakeLists.txt       targets of this plugin (added automatically by the root CMakeLists.txt)
source/dsp/          host-independent, header-only DSP core (no JUCE)
  Parameters.h         §2 mappings r -> value with quantisation, inverses, defaults, display formats, Settings
  Crossover.h          §3 w0(Q), bilinear sections, exact complex band responses, FourBandSplitter (12 biquads)
  GainCurve.h          §4 / D6 normative tables and f(x; Knee, Range) (node rule + monotone cubic)
  Envelope.h           instantaneous amplitude (IIR Hilbert pair) for the peak detector
  BandDynamics.h       §5 / §6 per-band gain computer (detector, attack, onset, charge/release, Electro/Opto, Auto)
  MultibandEngine.h    complete processor: smoothing, crossover glides, linked detectors, solo / band bypass,
                       output gain, host bypass, meters
source/plugin/       JUCE AudioProcessor + parameter layout
source/ui/           editor: frequency display, band columns, master section, output (all vector-drawn)
tests/               Catch2 unit tests of the DSP core (CTest prefix "multiband/"), SpecTables.h = §5/§6 vectors
tools/Snapshot.cpp   optional dev tool rendering the editor to PNG (-DCRAFT_BUILD_SNAPSHOT=ON)
```

### Signal flow (per sample)

1. Parameter smoothers (one-pole τ = 4 ms, lands exactly on target: 99 % after 18.4 ms ≤ 20 ms, D2) for Output
   Gain, Knee, every band's Gain / Range / Threshold, crossover frequencies (in log2 Hz) and Q. While a crossover
   or Q glides, the section coefficients are recomputed every 4 samples.
2. Per channel: `FourBandSplitter` → B1…B4 (§3, double precision, transposed direct form II).
3. Per band: analytic-signal envelope of each channel's band signal, linked by `max` over the channels (§6),
   → `BandDynamics` → normalised gain change ν → linear dynamic gain `g = 1 + ν (10^(Range/20) − 1)`.
4. Band gain `10^(Gain/20) · g`, cross-faded (10 ms) to unity when the band is bypassed; multiplied by the solo
   weight (10 ms ramp) and the Output Gain; output = Σ bands.
5. Host bypass: 10 ms crossfade to the dry input, then the buffer is left untouched (bit-exact).

## How each spec section is implemented

**§1 Scope.** 0 samples latency (`setLatencySamples(0)`, no look-ahead; the crossover is minimum phase). With
every Range at 0 dB the processor is LTI (tested: scaling the input by 8 scales the output by exactly 8, and the
output equals the gain-weighted crossover to 1e-12).

**§2 Parameters.** The host-normalised value is the spec's `r`; JUCE `NormalisableRange` lambdas apply mapping and
quantisation (0.1 dB, 1 Hz, 0.01 Q / Knee, 0.01 ms), so hosts and GUI only see step values. Defaults as listed
(Crossovers 92 / 4000 / 11071 Hz, Q 0.60, Knee 0.50, Auto, Electro, Range −8.0 dB, per-band Threshold, Attack and
Release). Knee displays "Soft" at 0 and "Hard" at 0.75. Release mode and Behavior are bool parameters with
"Manual"/"Auto" and "Electro"/"Opto" texts. Bypass is the host bypass parameter.

**§3 Crossover (exact).** `crossoverW0(Q)` solves `u² + (2 − 1/Q²) u − 1 = 0` (u = w0²), i.e. each second-order
section is −3.01 dB at f_k. Each section is the bilinear transform of the analog prototype with `K = tan(π f_k/fs)`
(pre-warped at f_k): `LP_k` = two cascaded `w0²/D` biquads, `AP_k` one biquad, `HP_k = AP_k − LP_k` computed as a
difference of the two outputs, so every split is exactly complementary. Bands:
`B1 = AP3(AP2(LP1 x))`, `B2 = AP3(LP2(HP1 x))`, `B3 = LP3(HP2(HP1 x))`, `B4 = HP3(HP2(HP1 x))` — 12 biquads per
channel; `Σ B = AP1 AP2 AP3` (tested in the frequency domain to 1e-12 and sample-by-sample against an explicit
allpass cascade to 1e-12). D5: the same model at every rate. Crossover frequencies are clamped to ≤ 0.49 fs.
D4 (crossed crossovers): no reordering; each split stays complementary, so the output stays an allpass sum, stable
and click-free (tested with a glide into 5000/300/80 Hz).

**Solo / band bypass / output.** Solo: the output is the sum of the soloed bands (with their dynamics and gain);
Bypass: that band at unity, no dynamics, Gain ignored; Output Gain multiplies the sum (tested exactly).

**§4 Static curve.** `GainCurve` holds the three normative tables. For (Knee, Range) it builds the 17 nodes
x = −36 … +28 dB: Knee row (linear in Knee between Soft 0 / 0.15 / 0.30 / 0.50 / 0.71 / Hard 0.75) plus the
Range correction `f_R − f_{−12}` of the Knee-0.50 table, which for Knee > 0.50 blends linearly (w = (Knee−0.5)/0.25)
into the Hard-knee correction of §4.1 (D6). Range rows are interpolated linearly in Range. The nodes are clamped to
[0, 1], made monotonic and joined by a monotone cubic (Fritsch–Carlson PCHIP). Below −36 dB f decays by
`exp(0.09 dB⁻¹)`, above +28 dB the distance to 1 shrinks by `exp(−0.0578 dB⁻¹)` (gives f(+32) = 0.95 at Range −12 /
Knee 0.5 as measured). `x = L − Threshold` with L the sine-peak level of the band, so Thresholds shift the curve
exactly; Range 0 ⇒ fixed gain; the curve does not depend on Release mode or Behavior.

**§5 Dynamics** and **§6 detector** — see the next section.

**§6 Stereo / sample rate / smoothing.** Detectors are linked (max of the channels' squared envelopes) so both
channels receive one gain set by the louder channel. All time constants are in ms and converted with the actual
sample rate (`1 − exp(−Δt/τ)`), the crossover uses the §3 model at every rate.

## Dynamics law (spec §5 / §6, open questions 1–3)

The spec gives envelope tables (normative) and only a suggested structure. Reading the tables in the **linear gain
domain** was the key observation: expressed as the normalised change `ν = (g − 1)/(10^(Range/20) − 1)`, the attack
and release fractions of Range −24, −12 and +12 coincide (e.g. 0.88 at 10 ms for all three Attack-5 rows; the
release rows agree to ±0.03 in ln-distance), while they differ substantially in dB. So the whole time behaviour is
one Range-independent law on ν:

- **Detector**: analytic-signal magnitude (instantaneous amplitude) of the band signal — a steady sine reads its
  peak with no rectifier ripple — linked over channels, then a peak follower on the squared envelope (instant rise,
  exponential fall `peakReleaseMs`). Level `L = 10 log10(p)` dBFS (sine peak).
- **Target**: `ν_t = (G^f − 1)/(G − 1)`, `G = 10^(Range/20)`, `f = curve(L − Threshold)`.
- **Fast state a**: one-pole toward ν_t, τ = Attack when rising, `fastReleaseMs` when falling.
- **Onset** (the spec's "short initial fast component"): `onsetDepth · max(0, LP₁(ν_t) − LP₂(LP₁(ν_t)))` with
  τ₁ = `onsetMs`, τ₂ = `onsetDecayMs` — a band-passed copy of the rising target that adds a little gain change in the
  first 1–2 ms and is gone after ≈ 5 ms, whatever the Attack (tables: ≈ −1 dB at 1 ms even for Attack 200 ms).
- **Charged state s** (program dependence): while `a > s` it charges toward a with τ = `chargeMs`; while `a < s` it
  releases toward a with `τ = hold + Release_eff / k(s)`:
  Electro `k = e0 + e1 (1 − s)^e2` (slow while the change is large, faster near the target),
  Opto `k = o0 + o1 s^o2` (fast while the change is large, slower near the target); `hold = holdMs` / `holdOptoMs`.
  After a 30 ms burst s has only charged partly, so the release is fast; after a sustained overload s is fully
  charged and the Release law dominates — this is what makes the burst table differ from the sustained table even in
  Manual mode.
- **Memory m** (Auto): charges toward 1 while s charges (τ = `memoryMs`) and decays at `memoryDecay ×` the release
  time constant during release. Auto: `Release_eff = Release · (autoMin + (1 − autoMin) m)`; Manual:
  `Release_eff = Release`. After a sustained overload m ≈ 1 ⇒ Auto = Manual; after a short burst m is small ⇒ faster.
- **Output**: `ν = min(max(a, s) + onset, max(a, s, ν_t))`.

The 17 constants (`DynamicsLaw` in `source/dsp/BandDynamics.h`) were fitted offline (Nelder–Mead with restarts,
minimax-like cost Σ(err/tol)⁸, scratch tool not shipped) to all §5 tables (attack 90 values, sustained release
112 values for Manual and again for Auto, burst 70 values) and the §6 noise vector (6 levels × 2 settings), by
simulating exactly this engine at 48 kHz.

Fitted values: `peakReleaseMs` 1.55, `fastReleaseMs` 1.97, `chargeMs` 46.8, `holdMs` 59.9, `holdOptoMs` 35.6,
Electro `e0, e1, e2` = 0.481, 2.637, 1.400, Opto `o0, o1, o2` = 1.100, 203.8, 7.564, `memoryMs` 365,
onset 0.883 / 0.928 ms, depth 0.486. Two constants were set by hand after the fit because the tables do not
constrain them and the optimiser drove them to degenerate extremes: `memoryDecay` = 20 (the memory must eventually
forget a past overload) and `autoMin` = 0.05 (Auto never shortens the Release below 5 %); the fit quality was
unchanged (worst point 0.995 of its tolerance).

Result at 48 kHz: every table value is within its tolerance; the worst point uses 99.5 % of it. Points above 80 %:
Attack 1 ms at 1 ms (−5.7 vs −7.5, tol 1.84), Range +12 at 1 ms (+4.4 vs +3.5), Rel 5 Electro at 20 ms (−7.8 vs
−7.0), the burst rows at 20 ms (Manual Electro ≈ −3.3 vs −1.2 … −4.0, Opto ≈ −3.5 vs −4.6 … −4.9 — the model's
burst release is less Release-dependent than the table), the noise vector at −12.1 dB (−11.0 vs −10.4) and at
−32.5 dB with Attack 5 (−5.25 vs −5.9). The shape of the early burst release (first 20 ms) is the weakest part of
the law; everything is within tolerance but with little margin there.

## GUI (spec §8)

Original vector-drawn design in the Craft Audio house style (graphite panels, one hue per band, amber master
accents, no bitmaps or bundled fonts); own logo mark (four coloured bars).

- **Frequency display** (20 Hz – 20 kHz log, −30 … +24 dB): per band a shaded region between its Gain line (solid)
  and Gain + Range (dashed) over the band's frequency span; faint crossover lines; four numbered handles. Handles 1–3
  sit at the Low / Mid / High crossover at their band's Gain: drag sideways = move that crossover, up/down = that
  band's Gain, Shift- (or Cmd/Ctrl-) drag = that band's Range, Alt = fine, double-click resets Gain
  (Shift-double-click: Range). Handle 4 (band 4 has no crossover above it) sits in the middle of band 4's region and
  only changes Gain / Range. The animated white line is the instantaneous total response
  `|Σ g_b B_b(f)|` (+ Output Gain) computed from the current band gains and the exact crossover responses (30 Hz).
- **Band columns**: input-level meter (band sine-peak level, 0 … −80 dBFS; brighter above the Threshold) with a
  draggable Threshold line, value boxes for Threshold, Gain, Range, Attack, Release (click to type, drag, Shift fine,
  double-click / Alt-click reset), Solo and Bypass buttons, live band gain readout in the title.
- **Master**: Threshold / Gain / Range / Attack / Release offset boxes — a drag moves that parameter of all four
  bands by the same amount in the host-normalised domain (equal dB; for the logarithmic Attack / Release an equal
  ratio), limited so no band reaches an end stop, so the differences are preserved; Knee, Crossover Q value boxes;
  Release (Manual / Auto) and Behavior (Electro / Opto) segmented switches.
- **Output**: Output Gain fader (±18 dB) beside the L/R output peak meters (0 … −60 dBFS, 1.5 s peak line).

## Tests (`tests/`, Catch2)

33 test cases / 188 045 assertions; all pass with MSVC 2022 (Release, via CTest) and GCC 13 (Linux, Release).

- **Crossover** (§3): w0(Q) gives −3.01 dB per section; spec examples (|B1| 92 Hz −6.02, |B2| 1 kHz −0.29 / 4 kHz
  −6.02, |B3| 6 kHz −3.09, |B4| 15 kHz −1.51, Q 0.75 |B1| 20 Hz +0.08 dB) to 0.01 dB, deep stop-band examples
  within the spec's model agreement (see assumption 3); digital bands equal the spec's analog formula with the
  pre-warped substitution (1e-9, 3 rates × 4 Q × 2 crossover sets); Σ bands = AP1 AP2 AP3, |Σ| = 1 (1e-12) in
  frequency and time domain; the response starts at sample 0 (zero latency); the full engine at neutral settings
  equals the allpass cascade (1e-12); crossover / Q glides incl. crossed settings are stable and click-free.
- **Static curve** (§4, D6): all Knee rows, Range rows and Hard-knee rows reproduced exactly at the nodes; spot
  checks (±0.02); Knee interpolation linear; D6 blend; monotonic, 0 / 1 limits, f(+6.4) = 0.5 ± 0.03 for 8 Knees × 11
  Ranges; through the engine: −10.17 dB at −10 dBFS for fast and slow time constants, Gain + Range · f within
  ±0.3 / ±0.4 dB for 3 Knees × 4 Ranges × 7 levels; Threshold shift exact (0.02 dB); Range 0 = fixed gain;
  rising vs falling ≤ 0.024 dB.
- **Dynamics** (§5): all attack, sustained-release (Manual and Auto), and burst (Manual and Auto) table values
  within tolerance; Auto ≈ Manual after sustained overload; Auto faster than Manual after a burst; Release 20 ms
  Manual vs Auto ≤ 0.1 dB; Electro/Opto shape.
- **Detector / stereo / rates** (§6): noise vector (6 levels × Attack 0.5/Release 5 and Attack 5/Release 50) within
  ±0.75 dB; stereo link (L −10 / R −40 and swapped → both −10.17 dB; Range +12 → +10.43 dB, both channels equal);
  mono = stereo with identical channels; 44.1 / 96 kHz envelopes within 0.05 dB of 48 kHz (0.3 dB at 1–2 ms) and
  the tables hold there (with 5 % tolerance headroom for one-sample timing at 1 ms).
- **Engine**: LTI at Range 0; solo / band bypass / output gain exact; click-free solo, bypass and parameter changes
  (settled within 1 % after 20 ms); host bypass bit-exact after the crossfade.
- **Parameters** (§2): mappings, defaults, round trips, display formats.

## Questions / assumptions

1. **Dynamics law** (open questions 2–3): own law fitted to the tables (above). The burst table does not state the
   Attack; Attack 5 ms (as for the release tables) is assumed. The noise vector is evaluated as an energy-weighted
   band gain with Manual / Electro.
2. **Peak-detector law** (open question 1): analytic-signal magnitude + peak follower. The 2 × 4-section Hilbert
   pair is accurate (≤ ±0.05 dB envelope ripple) from ≈ 20 Hz at 44.1/48 kHz; at 88.2/96 kHz the lower edge
   doubles (≈ ±1.3 dB ripple at 20 Hz, ±0.5 dB at 30 Hz). Only the detector sees this; the audio path is the exact
   §3 model.
3. **Spec examples in the deep stop band**: the exact §3 model gives |B1| at 1 kHz = −76.43 dB (Q 0.6) and −41.65 dB
   (Q 0.1) where the spec lists −76.7 / −42.2 dB; the differences are −107 / −66 dB re unity, inside the spec's own
   stated model agreement (≤ −51 dB). All other examples match to 0.01 dB. The structure is implemented exactly as
   written.
4. **Hard-knee Range rows** exist only for Range −24 / −12 / +18; other Ranges at Knee > 0.5 interpolate those rows
   linearly in Range (D6 does not say how to interpolate between them).
5. **Mono** (open question 4): same processing on one channel through the mono bus layout (tested equal to stereo
   with identical channels).
6. **Crossed crossovers** (D4): processed as set (no sorting); stable, the sum stays the allpass.
7. **Smoothing** (D2): one-pole 4 ms (≤ 20 ms to 99 %); solo, band bypass and host bypass use 10 ms linear
   crossfades. Attack / Release / Knee changes act immediately on the gain computer (Knee is also smoothed).
8. **Band 4 handle** in the display has no crossover of its own (see GUI); master offsets are editor gestures that
   write the band parameters, not separate host parameters.
9. **Meter scales**: band meters 0 … −80 dBFS (the Threshold range); output meters 0 … −60 dBFS.

## Public references and provenance

- Allpass-complementary crossovers / Linkwitz–Riley: S. H. Linkwitz, "Active Crossover Networks for Noncoincident
  Drivers", JAES 24(1), 1976; P. A. Regalia, S. K. Mitra, P. P. Vaidyanathan, "The Digital All-Pass Filter: A
  Versatile Signal Processing Building Block", Proc. IEEE 76(1), 1988; U. Zölzer (ed.), *DAFX*, 2nd ed., Wiley
  2011 (filters, dynamics).
- Bilinear transform with pre-warping: A. V. Oppenheim, R. W. Schafer, *Discrete-Time Signal Processing*, 3rd ed.
- Gain computer / attack-release smoothing / program-dependent release: D. Giannoulis, M. Massberg, J. D. Reiss,
  "Digital Dynamic Range Compressor Design — A Tutorial and Analysis", JAES 60(6), 2012.
- IIR Hilbert transformer (phase-difference network, 2 × 4 allpass sections, public coefficients): O. Niemitalo,
  "Hilbert transform" / "Polyphase IIR half-band filters" notes (yehar.com, public domain).
- Monotone cubic interpolation: F. N. Fritsch, R. E. Carlson, "Monotone Piecewise Cubic Interpolation", SIAM J.
  Numer. Anal. 17(2), 1980.
- Nelder–Mead simplex: J. A. Nelder, R. Mead, The Computer Journal 7(4), 1965.
- JUCE 8 public API documentation, clap-juce-extensions README, Catch2 v3 docs; the repository's own
  `plugins/peak-limiter` (structure, value box, look-and-feel conventions).

## Building

Same as every plugin in the monorepo (from the repository root):

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build -R multiband --output-on-failure
```

Artefacts: `build/plugins/multiband/CraftMultiband_artefacts/Release/{VST3,CLAP,Standalone}/`.
`-DCRAFT_BUILD_PLUGINS=OFF` builds only the DSP tests (no JUCE); `-DCRAFT_BUILD_SNAPSHOT=ON` adds
`multiband_snapshot` (renders the editor to PNG). `ci/package.py` picks the plug-in up automatically (it scans for
`*_artefacts/Release`), producing `craft-multiband-<version>-<platform>.zip`.

## Status

- Windows x64 Release build (VST3, CLAP, Standalone) with MSVC 2022, no compiler warnings in this plugin's sources;
  33/33 tests pass (MSVC and GCC 13). Installed locally: VST3 to `%LOCALAPPDATA%\Programs\Common\VST3` and
  `C:\Program Files\Common Files\VST3`, CLAP to `%LOCALAPPDATA%\Programs\Common\CLAP`. Editor rendered with the
  snapshot tool (default and edited states) and checked visually. `ci/package.py` finds VST3 + CLAP + Standalone
  and writes `craft-multiband-0.1.0-windows-x64.zip`.
- Linux: DSP tests verified natively (GCC 13); the full plug-in build is left to CI.
- Not yet done: host-level validation (pluginval / DAW session) and the independent verification against the
  reference (process step 5). The animated total-gain curve ignores Solo (it shows the processing, not the
  monitoring).
