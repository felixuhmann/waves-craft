# Implementation notes — Craft Console G (spec console-g r2)

Clean-room implementation (builder side) of [`spec/console-g/r2/SPEC.md`](../../spec/console-g/r2/SPEC.md) and its
normative tables [`spec/console-g/r2/tables/*.csv`](../../spec/console-g/r2/tables) (SHA-256 `dae1e2cc…333c7c`,
approved 2026-11-01; supersedes r1, see [verification of r1](../../docs/verification/console-g-r1.md)). Only the spec, its tables, `docs/process.md`, the repository's own code,
public DSP literature (below) and this implementation were used. No reference product, analyst workspace, vendor
asset or vendor installation was accessed; no third-party plug-in was loaded.

Product: **Craft Console G** (`CCnG`, CLAP id `com.craftaudio.console-g`, bundle id `audio.craft.CCnG`);
manufacturer **Craft Audio** (`Crft`); version 0.1.0. Formats: VST3, CLAP, Standalone. Platforms: Windows x64
(MSVC 2022), Linux x64 (GCC). One plug-in: the host chooses **stereo** (2→2, linked dynamics) or **mono** (1→1)
through the bus layout (spec D5: mono = one channel of the same processing).

## Architecture

All paths relative to `plugins/console-g/`.

```
CMakeLists.txt        targets; embeds the spec tables: every CSV line -> a string literal in the generated
                      header <build>/generated/console_g/SpecTables.h (configure time; nothing is read at run time)
source/dsp/           host-independent, header-only DSP core (no JUCE), double precision
  Csv.h                 tiny CSV reader for the embedded tables (units "dB"/"s"/"Hz"/"KHz", "Out", "+Inf")
  MathUtil.h            linear interpolation, PCHIP (Fritsch–Carlson monotone cubic), dB helpers
  Tapers.h              §2: the 20 41-point tapers r -> displayed value, display text, inverse for typed values
  Biquad.h              biquad (TDF-II), bilinear transform WITHOUT prewarp, magnitude-matched fit (Vicanek)
  Filters.h             §3: HP prototype factorisation / interpolation; LP target + per-rate fitted biquads
  Equaliser.h           §4: shelf prototypes (q-representation, PCHIP/log-f), LMF/HMF effective-bell tables
  Dynamics.h            §5/§6 tables: comp_static curves, comp_model_fit timing, expander/gate constants
  DynamicsSection.h     run-time detector, compressor gain computer, expander/gate depth state
  ConsoleEngine.h       complete strip: polarity, trim, baseline curve, routing, sections, noise, output, smoothing
source/plugin/        JUCE AudioProcessor + parameter layout (host value = spec r)
source/ui/            editor, look-and-feel, knobs / fader / switches, meters (all vector-drawn)
tests/                Catch2 tests of the DSP core (CTest prefix "console-g/")
tools/Snapshot.cpp    optional dev tool rendering the editor to PNG (-DCRAFT_BUILD_SNAPSHOT=ON)
```

### Signal flow (per sample, `ConsoleEngine::process`)

`x → polarity (smoothed ±1) → input trim (smoothed) → routing → noise → output gain (smoothed) → y = x + 0.00316·x³ → safety → out`.
Routing (spec §1/§7; `filters` = HP→LP, `EQ` = LF→LMF→HMF→HF, `dyn` = linked gain):

| switches | audio path | detector input |
|---|---|---|
| default | dyn → filters → EQ | dynamics input |
| split | filters → dyn → EQ | dynamics input (filtered) |
| Ch Out (post-EQ) | filters → EQ → dyn | dynamics input |
| filters → side-chain | dyn → EQ (or EQ → dyn with Ch Out) | filters(dynamics input) |

"Filters to side-chain" takes precedence over "split". Bypass switches remove a section by a 10 ms cross-fade (the
section keeps running, so re-enabling is click-free); HP/LP "Off" (r = 0) works the same way. A routing change
fades the output to zero in 3 ms, switches, and fades back in (D6). Continuous parameters are smoothed in the
host-value domain (one-pole, τ = 4 ms → 99 % in 18 ms, D6), coefficients are recomputed every 16 samples while a
value moves. Latency is 0 samples in every setting.

## How each spec section is implemented

**§1.1 Baseline curve.** `v += 0.00316·v³` as the last stage after the output gain (r2), always (also with both sections bypassed). Tested
bit-exact against the formula and by the H3 vectors (−98/−74/−62 dBc) and the coefficient estimate (±2 %).

**§1.2 Analog.** Per channel an independent xorshift white-noise generator (uniform, unit RMS) scaled to
−110.8 dBFS RMS, cross-faded in/out with the switch, added before the output gain (the noise level is specified for
output 0 dB; it follows the fader like the rest of the channel). Present on digital silence. Optional 5th harmonic:
not implemented.

**§2 Parameters.** Every continuous host parameter is the spec's r ∈ [0,1] (JUCE range 0…1); the displayed value
is the piecewise-linear table value (exact at the 41 points; tested for all 820 cells, text identical up to the
spelling "KHz"→"kHz", "Out"→"Off"). Audio uses the interpolated (unrounded) displayed value. Ratio between 90 and
+Inf is interpolated linearly in 1/R. Typed values are inverted through the taper. Defaults per §2 (e.g. threshold
0 dB = r 0.334375, LF 60 Hz = r 0.2306, output 0 dB = r 0.6667). Switches are bool parameters; Exp/Gate mode and
meter source are two-entry choices. Releases are shown in seconds (D1).

**§3.1 HP.** Each table denominator `s³+A2s²+A1s+A0` is factored (bisection for the real root) into the pole pair
(ω, ζ) and the real pole σ; between rows (ln ω, ln ζ, ln σ) are interpolated linearly in log10(display). The
numerator is `B3·s³` (spec: all zeros at s = 0; the tabulated B0…B2 are fit residue — the factored prototype matches
the full table prototype within 0.1 dB, tested). Digitised as a bilinear biquad + bilinear first-order section,
no prewarp, at the actual rate.

**§3.2 LP (r2).** The target table depends on the rate: `lp_target_44100.csv` below 46 kHz, `lp_target_48000.csv`
up to 88.2 kHz, `lp_target.csv` (96 kHz) from 88.2 kHz (linear in r and log f; above 18.2 kHz the last slope
continues). In `prepare`, a biquad is fitted on a 64-point log grid 20 Hz…min(20 kHz, 0.45 fs) at every table row and
3 intermediate r values (157 designs): poles from an analog pair via the bilinear transform, coarse grid + Nelder–Mead;
numerator by Vicanek's |H|² least squares. Arbitrary r interpolates neighbouring coefficients. Worst table error
0.27 dB (44.1 kHz), 0.29 dB (48 kHz), 0.03 dB (96 kHz).

**§4.1 Shelves.** Exactly the spec's representation q = (ln B2, ln ωz, ln ζz, ln ωp, ln ζp), PCHIP along display
gain including the flat point (from the nearest non-zero gain; ties = mean), linear in log10 f between frequency
rows; bilinear without prewarp. Exactly flat at 0 dB (numerator = denominator). The measured magnitude rows
`lf_450_extra.csv` (+6, +11.9 dB at 450 Hz, r2) are fitted to prototypes at start-up (Levenberg–Marquardt on q) and
join the 450 Hz row. Two deviations from a naïve reading,
both exact at all table points: (a) the single LF 40 Hz point is not a row (one gain only) and is skipped; (b) the
sparse rows (6 gains) get the gains they lack from their own PCHIP plus the residual that the same gain subset
leaves on the neighbouring dense rows (16 gains), interpolated in log f — this carries the dense rows' gain shape
over and brings the §9.1 off-grid vectors from 1.8 dB to ≤ 0.9 dB worst case.

**§4.2 LMF.** Effective (f0, Q_e, G_e) by a separable scheme that is exact at every table point:
`value = base(g) · qRatio(g, rQ) · fRatio(sign g, f)` — base: PCHIP along display gain at Q 1.5 / 500 Hz (G_e = 0
at 0 dB); qRatio: value(g_i,Q)/value(g_i,1.5) PCHIP along the host value rQ, then PCHIP along gain (precomputed on
a 401-point rQ grid so the audio thread never allocates); fRatio: ±12 dB frequency series, PCHIP in ln f, for f0
and — beyond the spec's wording — also for Q_e and G_e (makes the frequency-series points exact). ÷3 uses its own
log-log frequency line through the two ÷3 points (500 → 119 Hz, 2500 → 435 Hz) and their Q/G corrections.
Digitised by the bilinear transform without prewarp.

**§4.3 HMF.** Same table scheme; ×3 multiplies the effective centre by 2.88 (r2). Digitised magnitude-matched (D4):
matched poles, least-squares numerator, then two Sanathanan–Koerner passes that refine poles and numerator together
(linear in the |H|² representation; factorising A is always stable; kept only if better). ≤ 0.25 dB to 15 kHz and
≤ 1.5 dB to 20 kHz vs the analog target at 44.1/48/96 kHz at every table point.

**§5 / §5.1 Compressor (r2).** Linked: the detector takes `max_c |sidechain_c|`. Structure (`DynamicsSection.h`):

1. rectifier `e1 = max(p, e1·exp(−1/(0.06 ms·fs)))`;
2. level stage, linear domain, short constants: auto symmetric 1 ms (reads the mean of `e1`), fast 0.8 ms / 5 ms
   (reads nearer the peak; this reading difference plus 0.8 dB is the extra static reduction of fast attack);
3. `L = 20·log10(e2) − cal + Δ`; `cal` = mean auto reading of a 1 kHz sine + 0.1 dB, `Δ` includes
   2.1 dB·log10(release/0.4 s) (§9.2 release 4 s row);
4. `g_i = curve(L) − make-up`, curve = comp_static row (bilinear in T and 1−1/R);
5. smoother in dB: two parallel one-poles with the same attack (42 ms, fast 1.7 ms) and two releases, output
   `w·g1 + (1−w)·g2`, with (w, τ1, τ2) a two-exponential fit of the §5.1 rows interpolated in ln(release). It is
   linear in dB, so relative times cannot depend on threshold, ratio or depth, as r2 requires; there is no longer
   any T/R dependence of the time constants and `comp_model_fit.csv` is not used;
6. gain modulation: a small AC part of `g_i` (above ~80 Hz, scaled 0.018 auto / 0.33 fast, growing with the
   reduction) reaches the gain directly, reproducing the §5 H3 values within ±6 dB.

Results at 48 kHz: comp_static worst 0.04 dB; all §5.1 rows within ±25 % at 44.1/48/96 kHz (fast attack to the 1 ms
analysis resolution); noise vs sine −4.32/−3.66 (auto), −6.63/−5.34 (fast), −13.00/−11.88 dB (T−20 R∞); link
−11.61 dB; H3 auto −90/−75/−68/−66, fast −69/−57/−51/−49 dBc at −30/−18/−6/0 dBFS.

**§6 Expander / gate (r2).** Level from `e1` through its own smoother (2.5 ms / 3 ms, calibrated). Constants from
`exp_static.csv` at start-up:

- slope s(T) ≈ 1.2 dB/dB and onset Lth(T) by least squares on the settled range-40 down staircases; ranges 20 / 30
  get a small fitted onset shift;
- depth state D (dB), target `clamp(s·(Lth − L) + o, 0, a·range)`, attenuation `min(max(0, D − o), a·range − o)`, a = 1.375,
  o = 6.1 dB fitted to the settled floors;
- gate: hysteresis levels per T from the staircases (step midpoints); D target 0 or floor + o; floor −25.41 dB at
  range 20, −52.7 dB at range 40, linear in dB vs range;
- closing: D moves exponentially towards its full depth and stops at the target, τ through 0.205 / 0.77 / 2.8 s for
  release 0.2 / 1 / 4 s (log-log; these three points fit all §6 tables); the closing onset follows from D having to pass
  o, which gives the depth-dependent 25–100 ms of r2. Opening τ 1.9 ms (fast 0.2 ms);
- the state starts open and holds during digital silence (detector < −120 dBFS).

Gains of compressor and expander add in dB. `exp_static` worst 0.65 dB, within tolerance; §6 timing tables and
long-release staircases pass at 44.1/48/96 kHz.

**D7.** The expander floor is finite by construction (≤ a·range dB); all detector/filter states are sanitised per
block (non-finite or runaway state → reset), non-finite input samples are treated as 0. Tested: 300 random
dynamics/routing settings with silence, −80 dB and full-scale noise, then neutral settings pass a tone at unity.

**§8 Sample rate.** LF/HF/LMF/HP: bilinear without prewarp at the actual rate (tested: digital response = analog
prototype at the warped frequency, 44.1/48/96 kHz). HMF: magnitude target, rate independent. LP: per-rate tables. Time constants in
seconds; noise level and curve rate independent.

**§11 GUI.** See below.

## GUI (spec §11)

Original vector-drawn design in the Craft Audio house style (graphite panels; knob groups colour coded:
compressor amber, expander teal, filters lavender, HF rose, HMF leaf green, LMF sky blue, LF copper, master
silver); own emblem (three stacked bars). Left to right as §11 requires: (1) dynamics column — compressor (ratio,
threshold, release knobs; FAST switch; gain-reduction meter 0…−30 dB), expander/gate (range, threshold, release;
GATE and FAST switches), routing switches (DYN BYPASS, POST EQ, FILT > SC, SPLIT); (2) filters (HP, LP knobs, fully
counter-clockwise = Off); (3) EQ columns HF (gain, freq), HMF (gain, freq, Q, ×3), LMF (gain, freq, Q, ÷3),
LF (gain, freq), EQ BYPASS; (4) master — input trim knob, output fader, ANALOG and PHASE switches, input/output
meter with IN/OUT selector (display only). Knobs: rotary arc + pointer, name above, value below in the spec's
display format (release in seconds, D1); drag to change, double-click resets to the default (§11), click the value
to type one. Meters: instant rise, 20 dB/s fall, 1.5 s peak marker.

## Tests (`tests/`, Catch2)

TESTLINE, registered with CTest as `console-g/<name>`; MSVC 2022 Release and GCC 13.

- Tapers: all 820 cells, interpolation, inversion, defaults. HP: prototypes, bilinear rule at 44.1/48/96 kHz, −3 dB
  points, vectors. LP: per-rate tables at 44.1/48/96 kHz (≤ 0.5 / 1.5 dB), off-grid, §9.1 vectors (now within tolerance).
- Shelves on grid, flatness, LF 450 Hz extra rows ≤ 0.5 dB. LMF/HMF table points, HMF ≤ 0.25 dB to 15 kHz, ×3 peak at 9.4 kHz.
  All §9.1 rows through the strip, worst 0.72 dB.
- Compressor: all 1568 static points, §9.2 rows, §5.1 table at three rates, H3 auto and fast, noise vs sine, link.
- Expander/gate: all 1680 static points, §9.2 rows, §6 timing tables at three rates, long-release staircases, D7.
- Strip: baseline as last stage, analog noise, polarity, latency, routing, bypass, mono, smoothing, extreme settings.

Host stress (`tools/hoststress`, VST3, `--editor`): HOSTLINE

## Public references

- M. Vicanek, *Matched Second Order Digital Filters* (2016) — |H|² representation with φ0/φ1/φ2, matched poles.
- F. N. Fritsch, R. E. Carlson, *Monotone Piecewise Cubic Interpolation*, SIAM J. Numer. Anal. 17 (1980);
  C. Moler, *Numerical Computing with MATLAB*, ch. 3 (PCHIP slopes).
- Bilinear transform: A. V. Oppenheim, R. W. Schafer, *Discrete-Time Signal Processing*, ch. 7.
- J. A. Nelder, R. Mead, *A Simplex Method for Function Minimization*, Computer Journal 7 (1965).
- D. Giannoulis, M. Massberg, J. D. Reiss, *Digital Dynamic Range Compressor Design — A Tutorial and Analysis*,
  JAES 60(6), 2012 (detector / gain-smoothing structures).

## Questions / assumptions

Resolved by r2 and the r1 verification: LP vectors vs table (per-rate tables), HMF ×3 (2.88 on the effective centre),
HP numerator (`B3·s³` is fine), output safety (D9).

1. **LMF/HMF frequency series** scale Q_e and G_e as well as f0 (exact at the series points); ÷3 corrections of the two
   ÷3 points apply to both signs.
2. **Shelf rows.** LF 40 Hz (single point) is not a row; sparse rows borrow the dense rows' gain shape; ties of the flat
   point use the mean. The `lf_450_extra` rows are treated as analog magnitudes (450 Hz is far below every Nyquist).
3. **LP between 48 and 88.2 kHz** uses the 48 kHz table (not specified).
4. **Compressor law.** The static table holds at default timing by construction. Release-dependent static shift is a
   level shift (2.1 dB per decade of release), also below 0.4 s. Fast-attack static is only specified at T −10 / R 4; other
   ratios follow from the same level difference, and the r1 verifier's 1.39 dB at R ∞ could not be checked against any
   table here, so it needs re-measuring. Fast attack with release above 0.4 s scales the slow part like the auto law.
5. **H3 mechanism.** The direct AC term is a model of the measured values, not a claim about the reference's circuit.
6. **Expander/gate.** Hysteresis levels are staircase midpoints (2 dB data resolution). Fast opening 0.2 ms is not
   measured. The closing law is assumed the same for gate and expander.
7. **Gate floor below range 20**: linear to 0 dB at range 0 (no data).
8. **Analog noise** is added before the output gain. Optional 5th harmonic not implemented.
9. **Routing combinations** not in §1: side-chain wins over split; with Ch Out the detector sees filters(EQ output).
10. **Output safety** (D9): above +24 dBFS levels bend towards +30 dBFS, exact below.
11. **> 96 kHz** uses the same laws, unverified.

## Status

Complete for spec r1: DSP, parameters, GUI, tests, Windows x64 Release build (VST3 / CLAP / Standalone) installed
locally; Linux test build verified. Not done: independent verification (verifier side), optional 5th-harmonic
analog content (§1.2, optional). The low-pass designs are fitted when the sample rate changes (`prepareToPlay`,
a few hundred ms at most, cached for the current rate).
