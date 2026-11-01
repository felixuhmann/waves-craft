# Implementation notes — Craft Multiband (spec r2)

Clean-room implementation (builder side) of [`spec/multiband/r2/SPEC.md`](../../spec/multiband/r2/SPEC.md)
(SHA-256 `cc7d2a33…5efe16`, approved 2026-10-29; supersedes r1 — the r1 build 897ff20 was verified in
[`docs/verification/multiband-r1.md`](../../docs/verification/multiband-r1.md)). Only the spec, `docs/process.md`, the repository's own code,
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
  Crossover.h          §3 w0(Q), bilinear sections, exact complex band responses, FourBandSplitter (9 TPT SVFs)
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
2. Per channel: `FourBandSplitter` → B1…B4 (§3, double precision, trapezoidal state-variable sections).
3. Per band: analytic-signal envelope of each channel's band signal, linked by `max` over the channels (§6),
   → `BandDynamics` → normalised gain change ν → linear dynamic gain `g = 1 + ν (10^(Range/20) − 1)`.
4. Band gain `10^(Gain/20) · g`, cross-faded (10 ms) to unity when the band is bypassed; multiplied by the solo
   weight (10 ms ramp) and the Output Gain; output = Σ bands.
5. Host bypass: 10 ms crossfade to the dry input, then the buffer is left untouched (bit-exact).
6. Robustness (r2 §6): settings are sanitised (finite, clamped); every dynamics state is clamped and reset if it
   becomes non-finite; a non-finite output sample is replaced by 0 and the filter / detector states are restarted
   at the end of that block, so nothing can latch.

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
(pre-warped at f_k). It is realised as a trapezoidal (topology-preserving) state-variable filter with
`g = w0 · tan(π f_k/fs)`, `k = 1/Q`, which is exactly that bilinear transform but stays well-behaved while the
coefficients glide (r1 used direct-form biquads, which could produce large transients under fast crossover / Q
automation in the stress host). One SVF gives `w0²/D` and `AP_k` of the same input; a second SVF turns the former
into `LP_k`; `HP_k = AP_k − LP_k`, so every split is exactly complementary. Bands:
`B1 = AP3(AP2(LP1 x))`, `B2 = AP3(LP2(HP1 x))`, `B3 = LP3(HP2(HP1 x))`, `B4 = HP3(HP2(HP1 x))` — 9 SVFs per
channel; `Σ B = AP1 AP2 AP3` (tested in the frequency domain to 1e-12 and sample-by-sample against an explicit
allpass cascade of direct-form biquads to 1e-12). D5: the same model at every rate. Crossover frequencies are clamped to ≤ 0.49 fs.
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

## Dynamics law (spec r2 §5 / §6, open questions 1–3)

The spec gives envelope tables (normative) and only a suggested structure. Reading the tables in the **linear gain
domain** was the key observation: expressed as the normalised change `ν = (g − 1)/(10^(Range/20) − 1)`, the attack
and release fractions of Range −24, −12 and +12 coincide (e.g. 0.88 at 10 ms for all three Attack-5 rows; the
Rel-50 release rows agree in the log of the remaining linear distance, 0.81 / 0.82 / 0.87 at 100 ms), while they
differ substantially in dB. So the whole time behaviour is one Range-independent law on ν. r2 §5.3 showed that the
r1 law did not generalise (release after 100–300 ms overloads held 3–6 dB too much); the r2 law below is built
around the duration dependence that §5.3 makes normative (fast + slow component, as r2 §5 suggests):

- **Detector**: analytic-signal magnitude (instantaneous amplitude) of the band signal — a steady sine reads its
  peak with no rectifier ripple — linked over channels, then a peak follower on the squared envelope (instant rise,
  exponential fall `peakReleaseMs` / `peakReleaseOptoMs`). Level `L = 10 log10(p)` dBFS (sine peak).
- **Target**: `ν_t = (G^f − 1)/(G − 1)`, `G = 10^(Range/20)`, `f = curve(L − Threshold)`.
- **Fast component a**: rises toward ν_t with τ = `attackScale` · Attack; falls toward ν_t with
  `τ = τF · shape`, `τF = fastMinMs + fastSpanMs · Release/(Release + fastHalfMs)`, × `optoFastScale` in Opto and,
  in Auto, × `1 − (1 − autoFast) · Release/(Release + autoHalfMs)` (Auto shortens the fast part, hardly at all for
  short Release: Manual vs Auto ≤ 0.1 dB at Release 20 ms as §5.2 requires).
  Electro `shape = 1 + electroSlowGain · a^electroSlowPow` (slow from a large change, fast near unity);
  Opto `shape = 1 + optoSlowGain · (1 − a)^optoSlowPow` (photocell-like: fast from a large change, slow near unity —
  this gives the long small tail after a 5 ms Opto overload).
- **Onset** (the spec's "short initial fast component"): `onsetDepth · max(0, LP₁(ν_t) − LP₂(LP₁(ν_t)))` —
  gone after ≈ 5 ms whatever the Attack.
- **Slow (charged) component s**: while `a > s` it charges toward a with τ = `chargeMs` (≈ 130 ms; Opto
  `chargeOptoMs` ≈ 110 ms — the 100–200 ms the spec suggests); while `a < s` it releases toward a with
  `τ = hold + Release / k(s)`: Electro `k = e0 + e1 (1 − s)^e2`, Opto `k = o0 + o1 s^o2`.
  **This makes the release depend on how long the overload lasted in every mode and Behavior**: a 5 ms overload
  charges s hardly at all (fast release), 100 / 300 ms charge it partly, a sustained overload fully (the Release /
  Behavior law of the §5 release table).
- **Output**: `ν = min(max(a, s) + onset, max(a, s, ν_t))`.

The r1 "overload memory" and its Auto rule (forget after 20 × Release, minimum 5 %) were not confirmed by the
verification and are gone; Auto now only shortens the fast component, so after a sustained overload Auto equals
Manual up to the first few tens of ms (≤ 0.6 dB, Opto at +20 ms; within the table tolerances).

Fitting: the constants (`DynamicsLaw` in `source/dsp/BandDynamics.h`) were fitted offline (Nelder–Mead with
restarts and parameter subsets, cost Σ(err/tol)⁸ + small quadratic term; scratch tool not shipped) to **all** r2
§5 / §6 data simulated through exactly this engine at 48 kHz: attack 90 values, sustained release 112 values
(Manual and again Auto), §5.2 burst 70 values (with the r2 pre-history: 1 s overload, 2.5 s at −50 dBFS), §5.3
250 values (one continuous stimulus per setting, tolerance start = the table's start column), the §6 noise vector
(6 levels × 2 timings) plus a penalty on the difference between the two noise timings, and the Release-20 ms
Manual-vs-Auto ≤ 0.1 dB condition. The last round used a tolerance scaled by 0.95 for margin.

Result at 48 kHz: every point within tolerance, worst 0.92 of it (Attack 1 ms at 1 ms: −5.9 vs −7.5, tol 1.84).
Other points above 75 %: Rel 500 Opto at 100 ms, the Electro 30 ms burst at Release 500/1000 at +20 ms (−2.9 / −3.2
vs −4.0 / −4.5), §5.3 Release 2000 Manual after 5 ms (+20 ms) and after 100 ms (+20 ms), Hard / Release 300 / Auto
after 1 s @ −20 dBFS (+20 ms), and the noise vector at −32.5 dB with Attack 5 (−5.28 vs −5.9). Noise: the two
timings differ by ≤ 0.12 dB (r1: up to 0.69 dB of the vector's 0.75 dB used; now ≤ 0.62 dB). The same tables pass
at 44.1 and 96 kHz (tested with the full tolerance).

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

37 test cases / 193 754 assertions; all pass with MSVC 2022 (Release, via CTest) and GCC 13 (Linux, Release).
All dynamics tables are checked at 44.1, 48 and 96 kHz with the full spec tolerance.

- **Crossover** (§3): w0(Q) gives −3.01 dB per section; spec examples (incl. the r2 model values −76.43 / −41.65 dB)
  to 0.01 dB; digital bands equal the spec's analog formula with the pre-warped substitution (1e-9, 3 rates × 4 Q ×
  2 crossover sets); Σ bands = AP1 AP2 AP3, |Σ| = 1 (1e-12) in frequency and time domain; the response starts at
  sample 0 (zero latency); the engine at neutral settings equals the allpass cascade (1e-12); glides incl. crossed
  settings are stable and click-free; random crossover / Q jumps on a full-scale square stay bounded.
- **Static curve** (§4, D6): all table rows reproduced at the nodes; spot checks; Knee interpolation; D6 blend;
  monotonic, limits, f(+6.4) = 0.5 ± 0.03; through the engine −10.17 dB at −10 dBFS, Gain + Range · f within
  ±0.3 / ±0.4 dB for 3 Knees × 4 Ranges × 7 levels; Threshold shift exact; Range 0 = fixed gain; no hysteresis.
- **Dynamics** (§5): attack, sustained release (Manual and Auto), §5.2 burst (Manual and Auto) and the §5.3
  250-point table at 3 rates; Auto ≈ Manual after sustained overload; Auto faster after a burst; Release 20 ms
  Manual vs Auto ≤ 0.1 dB; Electro / Opto shape.
- **Detector / stereo / rates** (§6): noise vector at both timings within ±0.75 dB and the two timings within
  0.3 dB of each other; stereo link; mono = stereo; 44.1 / 96 kHz envelopes within 0.05 dB of 48 kHz (0.3 dB at
  1–2 ms).
- **Engine / robustness** (§1, §6): LTI at Range 0; solo / band bypass / output gain exact; click-free switching and
  smoothing (≤ 20 ms); host bypass bit-exact; random parameter changes every 2048 samples (Knee alternating
  0.30 / 0.71, all other parameters random, 3 rates) keep the output finite and the processor passes signal again
  afterwards; NaN / Inf input samples do not poison the state.
- **Parameters** (§2): mappings, defaults, round trips, display formats.

## Questions / assumptions

1. **Dynamics law** (open questions 2–3): own law fitted to the r2 tables (above). The noise vector is evaluated as an
   energy-weighted band gain with Manual / Electro. §5.3 "start" values are used as the tolerance start (as r2
   defines); they are not checked points (the 250 points are the five times after each overload).
2. **Peak-detector law** (open question 1): analytic-signal magnitude + peak follower. The 2 × 4-section Hilbert
   pair is accurate (≤ ±0.05 dB envelope ripple) from ≈ 20 Hz at 44.1/48 kHz; at 88.2/96 kHz the lower edge
   doubles (≈ ±1.3 dB ripple at 20 Hz, ±0.5 dB at 30 Hz). Only the detector sees this; the audio path is the exact
   §3 model.
3. **Spec examples in the deep stop band**: resolved in r2 (the model values −76.43 / −41.65 dB are now stated).
4. **Hard-knee Range rows** exist only for Range −24 / −12 / +18; other Ranges at Knee > 0.5 interpolate those rows
   linearly in Range (D6 does not say how to interpolate between them).
5. **Mono** (open question 4): same processing on one channel through the mono bus layout (tested equal to stereo
   with identical channels).
6. **Crossed crossovers** (D4): processed as set (no sorting); stable, the sum stays the allpass.
7. **Stress host and +40 dBFS**: `tools/hoststress` (main) reports output above +40 dBFS as a failure. With the
   spec's maximum settings (band Gain +18, Range +18 = upward gain, Output +18: +54 dB) full-scale test signals
   legitimately exceed +40 dBFS, so those reports are expected; non-finite output never occurs (see Status).
8. **Smoothing** (D2): one-pole 4 ms (≤ 20 ms to 99 %); solo, band bypass and host bypass use 10 ms linear
   crossfades. Attack / Release / Knee changes act immediately on the gain computer (Knee is also smoothed).
9. **Band 4 handle** in the display has no crossover of its own (see GUI); master offsets are editor gestures that
   write the band parameters, not separate host parameters.
10. **Meter scales**: band meters 0 … −80 dBFS (the Threshold range); output meters 0 … −60 dBFS.

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
- Trapezoidal (topology-preserving transform) state-variable filter: A. Simper, "Linear Trapezoidal Integrated
  State Variable Filter" (Cytomic technical paper, 2013); V. Zavalishin, *The Art of VA Filter Design*, 2012.
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

- r2 implemented on `feat/multiband`. Windows x64 Release build (VST3, CLAP, Standalone) with MSVC 2022, no compiler
  warnings in this plugin's sources; 37/37 tests pass (MSVC and GCC 13). Installed locally: VST3 to
  `%LOCALAPPDATA%\Programs\Common\VST3` and `C:\Program Files\Common Files\VST3`, CLAP to
  `%LOCALAPPDATA%\Programs\Common\CLAP`. `ci/package.py` packages it as `craft-multiband-<version>-<platform>.zip`.
- `craft_hoststress` (tools/hoststress from main, built locally only) 2 × 4–5 min with `--editor` (≈ 300–570 M
  samples, ≈ 500 k parameter moves, 1 000+ state restores, 180+ re-prepares, 55–77 editor opens per run): no
  non-finite output, no crash or hang. All reported "failures" are levels above +40 dBFS (assumption 7); after
  switching to SVF sections the largest output was +62 dBFS for full-scale noise / square / DC input at extreme gain
  settings (the r1 direct-form crossover reached +78 dBFS under the same automation).
- Linux: DSP tests verified natively (GCC 13); the full plug-in build is left to CI.
- Not yet done: independent verification of r2. The animated total-gain curve ignores Solo.
