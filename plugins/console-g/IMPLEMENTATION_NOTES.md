# Implementation notes — Craft Console G (spec console-g r1)

Clean-room implementation (builder side) of [`spec/console-g/r1/SPEC.md`](../../spec/console-g/r1/SPEC.md) and its
normative tables [`spec/console-g/r1/tables/*.csv`](../../spec/console-g/r1/tables) (SPEC.md SHA-256
`988632d8…e2ef7f`, approved 2026-10-29). Only the spec, its tables, `docs/process.md`, the repository's own code,
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

`x → polarity (smoothed ±1) → input trim (smoothed) → y = x + 0.00316·x³ → routing → output gain (smoothed) → out`.
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

**§1.1 Baseline curve.** `v += 0.00316·v³` after trim, always (also with both sections bypassed). Tested
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

**§3.2 LP.** The target is the table (linear in r and log f; above 18.2 kHz the last slope continues). Per sample
rate (`prepare`), a biquad is fitted to the target on a 64-point log grid 20 Hz…min(20 kHz, 0.45 fs) at every
table row and 3 intermediate r values between rows (157 designs): poles from an analog pair (fc, Q) via the
bilinear transform (always stable), searched by a coarse grid + Nelder–Mead; numerator by the linear least-squares
magnitude fit of Vicanek's |H|² representation (relative error), then factorised. Arbitrary r interpolates the
coefficients of the neighbouring designs (stable: the stability triangle is convex). Worst table error
0.11 dB (44.1 kHz), 0.04 dB (48/96 kHz); off-grid r ≤ 0.5 dB up to 15 kHz.

**§4.1 Shelves.** Exactly the spec's representation q = (ln B2, ln ωz, ln ζz, ln ωp, ln ζp), PCHIP along display
gain including the flat point (from the nearest non-zero gain; ties = mean), linear in log10 f between frequency
rows; bilinear without prewarp. Exactly flat at 0 dB (numerator = denominator). Two deviations from a naïve reading,
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

**§4.3 HMF.** Same table scheme; ×3 multiplies the effective centre frequency by 3 (see assumptions). Digitised
magnitude-matched (D4): matched (impulse-invariant) poles, least-squares numerator over 20 Hz…0.49 fs plus a
weighted centre point; ≤ 0.5 dB to 15 kHz and ≤ 1.5 dB to 20 kHz vs the analog target at 44.1/48/96 kHz (tested at
every table point and for ×3 at 7 kHz).

**§5 Compressor.** Linked: the detector takes `max_c |sidechain_c|`, one gain for all channels.
Structure (`DynamicsSection.h`):

1. rectifier `e1 = max(p, e1·exp(−1/(0.04 ms·fs)))`;
2. level stage, linear domain: `e2` charges towards `e1` with the attack constant (auto: the `comp_model_fit`
   attack constant interpolated in T and 1−1/R, ×2.1, speeding up by up to 5× within the last 20 dB of the gap —
   "program dependent"; fast: 1.8 ms) and discharges with 6.7 % of the attack constant (fast: 5 ms);
3. level `L = 20·log10(e2) − cal + Δ`; `cal` = the simulated mean reading of a steady 1 kHz sine at the setting's
   level-stage constants (so a sine of peak level L dBFS reads L); `Δ` = 3.3 dB for fast attack and
   2.1 dB·log10(release/0.4 s) — the residual static shifts of the §9.2 fast / 4 s rows;
4. static gain `g_i = curve(L)`, `curve` = the comp_static row for (T, R), bilinear in T and 1−1/R (R = 1 → 0 dB),
   linear in L between the 2 dB points, constant below −54, last slope above 0 dBFS; make-up is part of the table;
5. gain release stage in dB: the output gain follows `g_i` downwards instantly and recovers exponentially in dB with
   the `comp_model_fit` release constant ×0.95 × release-control scaling (87/126/345 ms 63 % points for
   0.1/0.4/1.2 s, log-log extrapolated; fast: ×30/87…66/126), slowed by `1 + K·exp(g/4 dB)` for the last dB of
   reduction (K = 3·√(rel/0.4) auto, 6·(rel/0.4) fast) — reproduces the long 90 % release tails.

Results (unit tests, 48 kHz): all 1568 comp_static points within 0.2 dB (rms 0.04 dB); §9.2 fast / 4 s / trim rows
within tolerance; timing vs §5 table (attack/release 50/63/90 %): auto 0.4 s 28.5/38.5/73.5 · 81.5/117.5/348.5 ms,
0.1 s ·58.5/78.5/192.5, 1.2 s ·235.5/357.5/1052.5, fast 0.4 s 1.5/1.5/2.5 · 44.5/66.5/246.5, fast 0.1 s
·22.5/28.5/66.5 (all within ±25 %, fast attack within the 1 ms analysis resolution); noise vs sine (§9.3)
−4.55/−3.64 (auto), −6.52/−5.52 (fast), −13.28/−11.85 dB (T−20 R∞); link: R receives L's −11.59 dB.

**§6 Expander / gate.** Shares the detector (`e1`) through its own fast level smoother (0.3 ms / 15 ms, calibrated
like the compressor). Constants are derived from `exp_static.csv` at start-up:

- slope s(T) (≈ 1.2 dB/dB) and onset Lth(T) by least squares on the settled range-40 down staircases; ranges 20 / 30
  get a small fitted onset shift (their slope region lies ~0.5 / 0.2 dB above the range-40 line);
- a depth state D (dB) with target `clamp(s·(Lth − L) + o, 0, a·range)`; attenuation `min(max(0, D − o), a·range − o)`
  with a = 1.375, o = 6.1 dB fitted to the settled floors (range 5 → −0.79, 10 → −7.66, 20 → −21.41,
  30 → −35.15, 40 → −48.9 dB) — this single law reproduces the non-linear floor-vs-range relation and the
  first-step-after-silence values;
- gate: hysteresis with open/close levels per T taken from the up/down staircases (step midpoints), D target 0
  (open) or floor + o (closed), floor −25.41 dB at range 20 (table), −52.7 dB at range 40 (spec text), linear in dB
  vs range (0 at range 0);
- closing (release): D moves exponentially towards its full depth with τ = 1.05 × release (gate 1.1 ×) and stops at
  the target (small staircase steps settle fast, a full closing takes ~release); opening: τ = 3 ms (fast 0.3 ms);
- during digital silence (detector < −120 dBFS) the state holds (start state: open).

Gains of compressor and expander add in dB (§9.2 "cg" row tested). All 1680 exp_static points are within tolerance
(worst 0.55 dB at a knee point).

**D7.** The expander floor is finite by construction (≤ a·range dB); all detector/filter states are sanitised per
block (non-finite or runaway state → reset), non-finite input samples are treated as 0. Tested: 300 random
dynamics/routing settings with silence, −80 dB and full-scale noise, then neutral settings pass a tone at unity.

**§8 Sample rate.** LF/HF/LMF/HP: bilinear without prewarp at the actual rate (tested: digital response = analog
prototype at the warped frequency, 44.1/48/96 kHz). HMF/LP: magnitude targets, rate independent. Time constants in
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

TEST_COUNT_PLACEHOLDER

## Public references

- M. Vicanek, *Matched Second Order Digital Filters* (2016) — |H|² representation with φ0/φ1/φ2, matched poles.
- F. N. Fritsch, R. E. Carlson, *Monotone Piecewise Cubic Interpolation*, SIAM J. Numer. Anal. 17 (1980);
  C. Moler, *Numerical Computing with MATLAB*, ch. 3 (PCHIP slopes).
- Bilinear transform: A. V. Oppenheim, R. W. Schafer, *Discrete-Time Signal Processing*, ch. 7.
- J. A. Nelder, R. Mead, *A Simplex Method for Function Minimization*, Computer Journal 7 (1965).
- D. Giannoulis, M. Massberg, J. D. Reiss, *Digital Dynamic Range Compressor Design — A Tutorial and Analysis*,
  JAES 60(6), 2012 (detector / gain-smoothing structures).

## Questions / assumptions

1. **LP §9.1 vectors vs. target.** The three LP rows of §9.1 (measured at 48 kHz) deviate from `lp_target.csv`
   (96 kHz) by up to 0.7 dB below 10 kHz and ~3 dB at 20 kHz (e.g. LP 8 k at 20 kHz: vector −13.94, target
   −10.65). §8/§10 make the rate-independent target normative for LP, so the implementation follows the table; the
   vectors are checked with ≤ 1.5 dB (≤ 12 kHz) / ≤ 3.5 dB above. Please confirm.
2. **HP numerator.** Implemented as `B3·s³` (spec: zeros at s = 0); the tabulated B0–B2 are ignored.
3. **HMF ×3.** "×3 multiplies the display frequency by 3" cannot be applied literally through the frequency
   mapping (3.5 k×3 → 10.5 k display would map to ≈ 7.4 kHz, not the stated ≈ 10.7 kHz); the effective centre
   frequency is multiplied by 3 instead (3.27 → 9.8 kHz), which also meets the 0.6 k×3 vector (§9.1) within 1 dB.
4. **LMF/HMF frequency series** scale Q_e and G_e as well as f0 (exact at the series points; the spec mentions f0).
   Negative gains use the −12 dB series, positive the +12 dB series; ÷3 corrections of the two ÷3 points are applied
   to both signs.
5. **Shelf rows.** LF 40 Hz (single point) is not used as a row; sparse rows borrow the dense rows' gain shape (see
   §4.1 above). Flat-point ties (±2.5 dB on dense rows) use the mean of both.
6. **Compressor law.** The spec gives tables, not the law (open questions 1/2). The structure above reproduces the
   static table at default timing by construction; the timing-dependent static shifts of §9.2 (fast attack,
   release 4 s) are modelled as level shifts (3.3 dB, 2.1 dB per decade of release relative to 0.4 s; also applied
   to releases below 0.4 s as a reduction of the shift). Program dependence is limited to the attack speed-up near
   the target and the release tail; the per-setting T/R dependence comes from `comp_model_fit.csv`.
7. **Expander/gate.** Hysteresis levels are the staircase midpoints (2 dB resolution of the data). Expander attack
   (opening) 3 ms, fast 0.3 ms — not measured by the spec. The expander holds its state during digital silence and
   starts open; this reproduces the "first step after silence" column of §9.2.
8. **Gate floor below range 20** is interpolated linearly to 0 dB at range 0 (no data).
9. **Analog noise** is added before the output fader; with output 0 dB it is −110.8 dBFS RMS.
10. **Routing combinations** not described in §1 (side-chain + split, side-chain + Ch Out): side-chain wins over
    split; with Ch Out the detector sees filters(EQ output).
11. **> 96 kHz** works (same laws); not verified against data (open question 5).

## Status

STATUS_PLACEHOLDER
