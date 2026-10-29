# Implementation notes — Craft Console E (spec r1)

Clean-room implementation (builder side) of [`spec/console-e/r1/SPEC.md`](../../spec/console-e/r1/SPEC.md) and its
normative tables [`APPENDIX.md`](../../spec/console-e/r1/APPENDIX.md) (approved 2026-10-29). Only the spec, this
repository, public DSP references and this implementation were used. No original product, analyst workspace,
vendor asset or vendor installation was accessed.

Product: **Craft Console E** (`CCnE`, CLAP id `com.craftaudio.console-e`), manufacturer **Craft Audio** (`Crft`),
version 0.1.0. One product: stereo (2 in / 2 out, one linked dynamics detector) or mono (1 in / 1 out) selected by
the host's bus layout. Formats: VST3, CLAP, Standalone. Platforms: Windows x64 (MSVC), Linux x64 (GCC).
Licence: AGPL-3.0 like the rest of the repository (JUCE), see [THIRD_PARTY.md](../../THIRD_PARTY.md).

## Architecture

All paths are relative to `plugins/console-e/`.

```
CMakeLists.txt        targets of this plugin (added automatically by the root CMakeLists.txt)
source/dsp/           host-independent, header-only DSP core (no JUCE), double precision
  Parameters.h          §2 tapers (knots at r = 0, .2, .5, .8, 1), ratio lookup, filter labels, defaults
  MatchedBiquad.h       analogue-matched second-order design (no warping) + TDF-II biquad
  EqTables.h            GENERATED: per-band parameter grids reproducing Appendix A, HP/LP corner tables
  EqDesign.h            grid lookup -> analogue prototypes -> matched digital sections (§4, §5)
  Equaliser.h           filter section + 4 bands, control glides, coefficient updates every 16 samples
  StaticCurves.h        Appendix B.1/B.2 compressor curve and make-up, §7 expander/gate curves and floors
  Dynamics.h            linked detector with HF emphasis, compressor ballistics, expander/gate envelope
  Smoothing.h           linear ramps (D5)
  ConsoleEngine.h       §3 signal flow: trim, polarity, saturation, routing, fader, analogue noise
source/plugin/        JUCE AudioProcessor (mono/stereo bus layouts), parameter layout
source/ui/            editor, look-and-feel, controls and meters (all vector-drawn)
tests/                Catch2 tests; the Appendix tables are read straight from the spec file
tools/fit/            Python generator of EqTables.h (Appendix A -> fitted section parameters -> grids)
tools/TimingProbe.cpp dev aid: prints the dynamics timing / static measurements next to the spec targets
tools/Snapshot.cpp    dev aid rendering the editor to PNG (-DCRAFT_BUILD_SNAPSHOT=ON)
```

`ci/package.py` picks the plugin up automatically (it scans every `*_artefacts/Release` directory; the product
`Craft Console E` becomes `craft-console-e-<version>-<platform>.zip`).

## How each spec section is implemented

**§1 / §3 Signal flow** (`ConsoleEngine::process`, per sample, double precision):
`in → input trim → polarity → fixed saturation → [split: HP/LP] → A → B → output fader → [analogue] → out`, A/B =
dynamics/EQ (default) or EQ/dynamics (channel out). *EQ to sidechain* removes the EQ (and HP/LP unless split)
from the audio and runs it on the detector input instead; *EQ bypass* bypasses EQ and HP/LP (also in the split
position and in the sidechain); *dynamics bypass* stops applying the dynamics gain (the detector keeps running
for the meters). Neutral settings: every section is an exact identity (unity coefficients, gain exactly 1), so the
output is `saturate(input)` to double precision (test: < 1.4·10⁻⁷). Latency 0.

**§2 Parameters.** Every continuous control is stored as its host-normalised `r` (AudioParameterFloat 0..1); the
section 2 taper only converts it for display and for typed values (`ParameterLayout.cpp`, resolutions as in §2).
Ratio: lookup of the 21 values at `r = k/20`, linear in between, the last segment (80 → ∞) linear in `1/R`.
HP: `r = 0` Off, `(0, 0.2]` labelled 17–20 Hz. LP: `r = 0` Off. Switches are AudioParameterBool. Defaults as §2.

**§4 Filters.** HP: third-order Butterworth (first-order pole + pole pair Q = 1 at f_HP), f_HP per position from a
fit of A.1 (16.9, 17.8, 28.2, 55.7, 102.5, 131.6, 174.0, 230.3, 322.3, 413.8 Hz at r = 0.1…1; fit error ≤ 0.14 dB,
within 1 Hz of the §4 table), log-linear in between. LP: the §4 two-pole (f, Q) values do not reproduce A.2 within
tolerance (1.4–1.6 dB at 8–12 kHz, see questions); an analogue fit of A.2 needs a two-pole section **plus a
first-order pole** (≈ 10–12 kHz for r ≥ 0.4); with it A.2 is met within 0.04 dB below 12 kHz (≤ 1.1 dB above).
Off is an exact bypass.

**§5 Equaliser.** Every Appendix A row was fitted with one analogue prototype per band, realised with the
matched design below (the fit runs on the realised 48 kHz response):
* bells (LF/HF bell, LMF, HMF): RBJ-type peaking section with a free high-frequency tilt
  `H(s) = (h·s² + A·ω/Q·s + ω²)/(s² + ω/(A·Q)·s + ω²)` (parameters ln f0, peak dB, ln Q, tilt h). Cuts are realised
  as the reciprocal of the matching boost (`1/H(+|G|, −h)`), which reproduces the measured cut responses near
  Nyquist (the direct realisation misses some HMF/HF cuts by up to 1.5 dB above 12 kHz);
* LF shelf: RBJ second-order low shelf plus a companion bell at 15·f, Q 0.4 (gives the measured overshoot on the
  flat side);
* HF shelf: a general second-order section `(k s² + k ωz/Qz s + k ωz²)/(s² + ωp/Qp s + ωp²)` (fitted with
  continuation along gain and frequency so the parameters vary smoothly) — needed for the measured fall of
  moderate boosts toward 20 kHz.
All 275 rows fit within 0.12 dB below 12 kHz (≤ 0.7 dB above). The per-row parameters are then placed on a
rectilinear grid per band — axes: frequency control r (0, 0.1, …, 1 plus the label positions of the rows), Q
control r (0…1 in 0.1, mid bands), gain label (−15, −12, −9, −6, −3, 0, 3, 6, 7.5, 9, 12, 15 dB) — and the
plug-in interpolates the four parameters multilinearly. Grid nodes at measured rows hold the fitted values
exactly; the other nodes are completed by linear interpolation plus residuals borrowed from the measured sweeps:
gain residuals from the 60 Hz / 8 kHz / 1 kHz gain sweeps (A.14–A.16; HMF borrows the LMF shape scaled to its
+7.5 dB gain), Q residuals from the LMF Q sweep (A.13, shared by HMF), frequency residuals from the frequency
sweeps (A.9–A.12; the shelves borrow the log-frequency shift of their band's bell). Gain 0 is an exact identity.
`tools/fit/` regenerates `EqTables.h`: `python3 gen.py && python3 build.py && python3 emit.py ../../source/dsp/EqTables.h`
(numpy + scipy).

**Matched design (no warping).** `MatchedBiquad.h`: poles by impulse invariance (`z = e^{sT}`), numerator by
magnitude matching at DC, Nyquist and one in-band frequency (min(ω0, π·fs/2)) — the scheme of M. Vicanek,
*Matched Second Order Digital Filters* (2016), generalised to an arbitrary analogue numerator; first-order
sections matched at DC and Nyquist. Coefficients are designed for the actual sample rate, so the response follows
the analogue prototype at every rate (D2).

**§6 Compressor.** `Dynamics.h` / `StaticCurves.h`.
* Detector: per channel a fixed emphasis biquad (analogue `(s² + ωz/Q s + ωz²)/(s² + ωp/Q s + ωp²)`, fz 1419 Hz,
  fp 1727 Hz, Q 0.414, normalised to 0 dB at 1 kHz — a fit of the §6.4 table through the B.1 curve), squared,
  one-pole mean square (4 ms; 1.2 ms in the fast-attack modes); the largest channel sets the level
  `L = 10·log10(2·ms)` (a sine reads its peak dBFS). One gain for both channels (§6, D6).
* Static: `gain = M(T,R) + GR_R(u)`, `u = L − 0.864·T`, B.1 interpolated linearly in u and in 1 − 1/R
  (R = 1 → none); the open B.1 cells (u > +10 for R = 1.5, 3, 12, 20) are extrapolated with the slope of the last
  two tabulated points. Make-up B.2 interpolated in T and 1 − 1/R; the open −20 dB cells are filled by scaling the
  −10 dB value with the −20/−10 ratio interpolated in 1 − 1/R (1.5: 2.56, 3: 5.71, 12: 7.23, 20: 7.65 dB).
* Ballistics: the static gain reduction passes a 0.6 ms response delay, then two parallel one-pole states in dB
  (attack 42 ms in slow mode; fast mode 0.2 ms for the first and 3.5 ms for the second state) whose release time
  constants and weight are tabulated at the four release values of the §6 table and interpolated log-linearly
  (two-component release `w·e^{−t/τ1} + (1−w)·e^{−t/τ2}`), then a short smoothing stage. Measured with the test
  setup below: slow attack 6.7 / 44.7 / 99.8 ms, fast 0.90 / 1.90 / 5.1 ms; releases within 12 % of every table cell.

**§7 Expander / gate.** Same linked detector. Knee (dBFS) = threshold − 10.9 dB (§7.1: threshold −20 bends at
−30.9 dBFS). Expander gain `−1.2·(knee − env)` (fit of §7.1, ≤ 0.12 dB) limited by the measured floors
(0.8 / 7.7 / 14.5 / 21.4 dB at range 5 / 10 / 15 / 20, then +1.38 dB per dB); gate: unity at or above the knee,
floor below (25.4 dB at 20, 52.7 dB at 40, linear). The level envelope follows rises instantly and falls at a
constant rate (389 / 157 / 71.4 / 19.3 dB/s at release 0.1 / 0.4 / 1 / 4 s, log-interpolated) — this reproduces the
almost linear-in-time expander closing of the §7 table; the gate gain additionally closes with a one-pole in dB
(0.14 / 0.35 / 0.77 / 1.7 s). Opening: one-pole 1.2 ms (fast 0.15 ms). Measured: expander within 9 %, gate within
21 % of every §7 cell; opening t90 3–4 ms (fast 0.4–0.6 ms).

**§8 Routing** as in §3; the §8 example values are reproduced within 0.75 dB (test).

**§9 Saturation / analogue.** `y = x + 0.0032·x³` after trim and polarity (+0.021 dB, H3 −62 dBc at 0 dBFS,
+12 dB H3 per +6 dB). The output fader is after it. Analogue On: independent Gaussian noise, −110.7 dBFS rms per
channel, plus −69 dB cross-feed between the channels (stereo), faded in/out over 10 ms.

**§10 D5 smoothing.** Trim/fader gains and analogue mix: 10 ms linear ramps. EQ and dynamics controls glide over
10–12 ms (coefficients recomputed every 16 samples while moving). Structural switches (polarity, routing, bypasses,
split, filter on/off, bell/shelf, expander/gate) fade the output out over 5 ms, switch at silence (clearing the EQ
states when the routing changes) and fade back in over 5 ms.

**§13 UI.** Three vector-drawn panels in the Craft palette (graphite panels, amber/teal accents, plain disc knobs
with a value arc; no console look, colour coding or knob caps imitated): *Dynamics* (compressor ratio /
threshold / release / fast + gain-reduction meter; expander/gate range / threshold / release / fast /
expander-gate selector + activity meter; DYN BYPASS, POST EQ), *Equaliser* (HP / LP with SPLIT, HF with BELL,
HMF and LMF with Q, LF with BELL; EQ BYPASS, EQ TO SIDECHAIN), *Input / Output* (input trim, POLARITY, ANALOGUE,
IN/OUT meter selector, stereo level meter, output fader). Values use the §2 resolutions; double-click a value to
type, double-click / Alt-click a control to reset.

## Tests (`tests/`, Catch2, registered with CTest as `console-e/…`)

* **EqTests** — every Appendix A magnitude row (275) at 48 kHz within §12; the same rows at 44.1 / 88.2 / 96 /
  192 kHz; HP corners (−3 dB at the §4 table) and slope; LP monotonic / Off; A.17 peak gains (192 rows, ≤ 0.5 dB)
  and peak frequencies (±10 %); proportional-Q character and reciprocity; exact identity at neutral; stability
  of every section over the whole control space at three rates.
* **DynamicsTests** — test vector 3 (1 kHz staircases −51…+3 dBFS for all B ratios at thresholds +10, 0, −10,
  −20 against B.1 + B.2 read from the spec, ±0.5 dB), make-up gain, §6.4 frequency dependence, linking, attack and
  release tables (±25 %), 96 kHz time constants, §7.1 static points, expander floors, gate floors and hard
  switching, expander and gate closing tables and opening times.
* **EngineTests** — test vector 1 (neutral), zero latency, test vector 5 (trim / fader exact to 0.002 dB, exact
  polarity inversion), §9 saturation (gain, H3 at 0 / −6 dBFS, fader independence), test vector 6 (analogue noise,
  independence, leakage), §8 routing values, bypasses, mono == one stereo channel, click-free control jumps.
* **ParameterTests** — tapers at the knots and in between, inverse tapers, ratio lookup, labels, defaults.

Timing setup used by the tests (the spec does not give it): ratio 4, threshold −10 dB, 1 kHz sine stepping
between −36.5 and −6.5 dBFS (≈ 13 dB reduction); expander/gate: threshold −20, range 40, 1 kHz sine falling
from 6 dB above to 35 dB below the knee (and back for opening); release 0.4 s rows with fast attack as listed.

## Public references

* M. Vicanek, *Matched Second Order Digital Filters*, 2016 (magnitude-matched numerator, impulse-invariant poles).
* R. Bristow-Johnson, *Cookbook formulae for audio EQ biquad filter coefficients* (analogue peaking / shelving
  prototypes).
* Butterworth filter design (textbook); RMS level detection and dB-domain gain smoothing as in
  D. Giannoulis, M. Massberg, J. D. Reiss, *Digital Dynamic Range Compressor Design — A Tutorial and Analysis*,
  JAES 2012.
* JUCE 8 and clap-juce-extensions documentation.

## Questions / assumptions

1. **Q 0.5 rows.** Rows labelled "Q 0.5" (A.7, A.8, A.16) are identical to the Q-sweep row at r = 0.9
   (`lmq_0.90 == gm_1k_q0.5_+12.0`) but 0.9 dB away from `lmq_0.80`, while §2 puts Q 0.5 at r = 0.8. The data are
   self-consistent only with "Q 0.5" = r 0.9, so the grids (and the tests) place those rows at r = 0.9; the
   displayed Q keeps the §2 taper (r = 0.9 shows 0.30). Please confirm which is right.
2. **LP §4 parameters vs A.2.** The §4 (f_LP, Q) pairs miss A.2 by up to 1.6 dB below 12 kHz; A.2 (normative) is
   followed with an extra first-order pole. "r = 0.1–0.3 … ≤ 2.7 dB loss at 20 kHz" contradicts A.2 (r = 0.3:
   −7.7 dB at 20 kHz); A.2 is followed.
3. **Other sample rates.** §12 read literally would compare 96 kHz against the 48 kHz tables with ±0.5 dB up to
   20 kHz, which the reference itself does not meet (§1). Tests use ±0.5 dB up to 12 kHz and ±1.5 dB above
   (±3 dB LP; HF band ≥ 8 kHz ±1.5 dB everywhere) at 44.1 / 88.2 / 96 / 192 kHz.
4. **Test vector 1** (output = input within −100 dB at ≤ −20 dBFS) contradicts §9 (cubic term is −90 dB at
   −20 dBFS). Implemented §9; tested: output = saturate(input) exactly, and within −100 dB at −30 dBFS.
5. **H2 ≈ −94 dBc** of §9 is not reproduced: an even term would break the exact polarity inversion (§3, TV5).
6. **Expander threshold reference**: one data point (§7.1) → knee = threshold − 10.9 dB at every threshold; the
   gate switches at the same knee, without hysteresis.
7. **Dynamics timing setup** (levels, ratio, range) is not specified; the assumed setup is listed above.
   The slow attack is a fixed 42 ms one-pole (the program dependence of the reference is not specified).
8. **Defaults** where §2 gives an approximate r next to the value: LF 60 Hz at r = 0.22 (not 0.23), LMF 0.50 kHz at
   r = 0.2857 (not 0.26), expander release 0.20 s at r = 0.26 (not 0.35) — r is chosen so the display shows the
   default value.
9. **Labels outside the given knots**: LP `(0, 0.2]` is labelled 20 → 12 kHz; HP/LP corners for `0 < r < 0.1`
   extrapolate the first table segment.
10. **EQ bypass** also bypasses HP/LP in the split position and the EQ in the sidechain; *dynamics bypass* with
    *EQ to sidechain* leaves the audio without EQ (as §3 says the EQ is removed from the audio path).
11. **Untabulated combinations** (e.g. HMF gains other than ±15/+7.5, Q between the measured values at other
    frequencies, shelf frequencies between label positions) follow the borrowed-residual interpolation above —
    plausible but not measured.
12. **Stereo linking** uses the larger channel mean square; mono = one channel of the stereo engine (§14.4).

## Status

* Windows x64 Release (MSVC 17 2022): VST3, CLAP, Standalone build; 31/31 Catch2 test cases pass (also on Linux
  GCC, DSP tests only).
* `tools/hoststress` (240 s with editor, seed 3; 90 s, seed 11): no crash, no NaN/Inf, no hang. It reports
  "runaway" samples of +40 … +43 dBFS: these are bounded and legitimate — the hoststress feeds full-scale squares
  while automating every control to its extremes, and the strip has +18 dB trim, up to ≈ +19 dB per EQ band
  (several bands can stack), +8.7 dB make-up and +12 dB fader. No output limiter was added, since the spec has
  none.
