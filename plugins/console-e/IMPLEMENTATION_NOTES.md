# Implementation notes — Craft Console E (spec r2)

Clean-room implementation (builder side) of [`spec/console-e/r2/SPEC.md`](../../spec/console-e/r2/SPEC.md) and its
normative tables [`APPENDIX.md`](../../spec/console-e/r2/APPENDIX.md) (approved 2026-10-30, supersedes r1; r1
verification: [docs/verification/console-e-r1.md](../../docs/verification/console-e-r1.md)). Only the spec, this
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
  Parameters.h          §2 21-point tapers, ratio lookup, filter labels, defaults
  MatchedBiquad.h       analogue-matched second-order design (no warping) + TDF-II biquad
  EqTables.h            GENERATED: per-band parameter grids reproducing Appendix A, HP corners, LP model
  EqDesign.h            grid lookup -> analogue prototypes -> matched digital sections (§4, §5)
  Equaliser.h           filter section + 4 bands, control glides, coefficient updates every 16 samples
  StaticCurves.h        Appendix B.1/B.2 compressor curve and make-up, §7.1 expander/gate table and floors
  Dynamics.h            linked detector with HF emphasis, compressor ballistics (§6.5/§6.6), expander/gate envelope
  Smoothing.h           linear ramps (D5)
  ConsoleEngine.h       §3 signal flow: trim, polarity, saturation, routing, fader, analogue noise
source/plugin/        JUCE AudioProcessor (mono/stereo bus layouts), parameter layout
source/ui/            editor, look-and-feel, controls and meters (all vector-drawn)
tests/                Catch2 tests; the Appendix tables and the §2 taper table are read straight from the spec
tools/fit/            Python generator of EqTables.h (Appendix A -> fitted section parameters -> grids, LP model)
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
for the meters). Neutral settings: every section is an exact identity, so the output is `saturate(input)` to double
precision (test: < 1.4·10⁻⁷). Latency 0. No output limiting (§3 gain staging); output stays finite.

**§2 Parameters.** Every continuous control is stored as its host-normalised `r` (AudioParameterFloat 0..1) and
follows the exact r2 21-point taper (linear between `r = k/20`) for its value and display (`ParameterLayout.cpp`,
resolutions as in §2). Ratio: the 21 points, linear in between, the last segment (80 → ∞) linear in `1/R`.
HP/LP: `r = 0` Off, the label table otherwise ((0, 0.05) shows the first label). Switches are AudioParameterBool.
Defaults are the r2 positions (0.667 → exactly 2/3, 0.333 → 1/3), which display exactly the stated values.
Appendix A rows are keyed by the displayed value (e.g. LMF 0.50 kHz = r 0.26, Q 0.50 = r 0.90, LF 100 Hz = r 0.35).

**§4 Filters.** HP: third-order Butterworth (first-order pole + pole pair Q = 1 at f_HP), f_HP per position from a
fit of A.1 (16.9, 17.8, 28.2, 55.7, 102.5, 131.6, 174.0, 230.3, 322.3, 413.8 Hz at r = 0.1…1; fit error ≤ 0.14 dB),
log-linear in between. LP (`tools/fit/lowpass.py`): one biquad whose poles are an analogue pole pair (f0, Q)
mapped by impulse invariance and whose numerator is a fixed **normalised** `1 + c1 z⁻¹ + c2 z⁻²` (unity at DC),
fitted to A.2 at 48 kHz. The fitted pole pairs coincide with the informative §4 f/Q pairs (e.g. 3342 Hz / 0.669 at
r = 1) and A.2 is met within 0.005 dB at all 17 columns (and therefore smoothly between them — the r1 build was
0.6 dB too dark at 10–12 kHz). The A.2 rows deviate from the analogue two-pole by an almost r-independent extra
roll-off (−1.5 dB at 8 kHz, −3.3 dB at 12 kHz), the reference's 48 kHz realisation. As the numerator coefficients
are the same at every rate, that roll-off scales with the sample rate: a little steeper at 44.1 kHz, analogue-like
at 96 kHz (§4). For r < 0.4 (pole pair at 0.8–1 × Nyquist at 48 kHz, high Q) the section moves between 48 and
96 kHz toward a plain matched two-pole with Q 0.85 / 0.66 / 0.60 (r = 0.1 / 0.2 / 0.3, matching A.2 at 12.5–20 kHz),
so it never resonates inside the band. Off is exact.

**§5 Equaliser.** Every Appendix A row was fitted with one analogue prototype per band, realised with the
matched design below (the fit runs on the realised 48 kHz response):
* bells (LF/HF bell, LMF, HMF): RBJ-type peaking section with a free high-frequency tilt
  `H(s) = (h·s² + A·ω/Q·s + ω²)/(s² + ω/(A·Q)·s + ω²)` (parameters ln f0, peak dB, ln Q, tilt h). Cuts are realised
  as the reciprocal of the matching boost (`1/H(+|G|, −h)`);
* LF shelf: RBJ second-order low shelf plus a companion bell at 15·f, Q 0.4 (the measured flat-side overshoot);
* HF shelf: a general second-order section `(k s² + k ωz/Qz s + k ωz²)/(s² + ωp/Qp s + ωp²)` (fitted with
  continuation along gain and frequency so the parameters vary smoothly).

All 275 rows fit within 0.12 dB below 12 kHz (≤ 0.7 dB above). The per-row parameters are placed on a rectilinear
grid per band — axes: frequency control r (0, 0.1, …, 1 plus the label positions of the rows), Q control r (0…1 in
0.1, mid bands), gain label (−15, −12, −9, −6, −3, 0, 3, 6, 7.5, 9, 12, 15 dB) — and interpolated multilinearly.
Grid nodes at measured rows hold the fitted values exactly; the other nodes are completed by linear interpolation
plus residuals borrowed from the measured sweeps: gain residuals from the 60 Hz / 8 kHz / 1 kHz gain sweeps
(A.14–A.16; HMF borrows the LMF shape scaled to its +7.5 dB gain), Q residuals from the LMF Q sweep (A.13, shared
by HMF), frequency residuals from the frequency sweeps (A.9–A.12; the shelves borrow the log-frequency shift of
their band's bell). Gain 0 is an exact identity.
HF bell above 48 kHz: the 16 / 20 kHz columns carry the reference's 48 kHz realisation, so a second parameter set
(the analogue prototype fitted to the columns up to 12.5 kHz, regularised toward the 48 kHz fit; cuts unchanged)
is blended in linearly from 48 to 96 kHz (r1 verification: 16 kHz bell at 96 kHz was 1.65 dB off).
`tools/fit/` regenerates `EqTables.h` (numpy + scipy): `python3 gen.py && python3 gen.py hi && python3 build.py &&
ROWFITS=rowfits_hi.json GRIDS=grids_hi.json python3 build.py && python3 emit.py ../../source/dsp/EqTables.h`.

**Matched design (no warping).** `MatchedBiquad.h`: poles by impulse invariance (`z = e^{sT}`), numerator by
magnitude matching at DC, Nyquist and one in-band frequency (min(ω0, π·fs/2)) — the scheme of M. Vicanek,
*Matched Second Order Digital Filters* (2016), generalised to an arbitrary analogue numerator; first-order
sections matched at DC and Nyquist. Coefficients are designed for the actual sample rate (D2).

**§6 Compressor.** `Dynamics.h` / `StaticCurves.h`.
* Detector: per channel a fixed emphasis biquad (analogue `(s² + ωz/Q s + ωz²)/(s² + ωp/Q s + ωp²)`, fz 1419 Hz,
  fp 1727 Hz, Q 0.414, 0 dB at 1 kHz — a fit of §6.4 through the B.1 curve), squared, one-pole mean square
  (4 ms; 1.2 ms in the fast-attack modes); the largest channel sets the level `L = 10·log10(2·ms)` (a sine reads
  its peak dBFS). One gain for both channels (D6).
* Static: `gain = M(T,R) + GR_R(u)`, `u = L − 0.864·T`, B.1 interpolated linearly in u and in 1 − 1/R
  (R = 1 → none); open B.1 cells (u > +10 for R = 1.5, 3, 12, 20) extrapolated with the last slope. Make-up B.2
  interpolated in T and 1 − 1/R; open −20 dB cells filled by scaling the −10 dB value with the −20/−10 ratio
  interpolated in 1 − 1/R (1.5: 2.56, 3: 5.71, 12: 7.23, 20: 7.65 dB). The staircases of all B ratios at
  +10/0/−10/−20 follow B within 0.03 dB (slow attack, 0.4 s).
* §6.5: the static reduction is shifted by ΔG(|GR|) per attack mode and release — the §6.5 rows read as a function
  of the Appendix B reduction at their levels (2.9 / 6.0 / 9.5 / 13.3 / 17.2 dB), linear in between, extended with
  the last slope (the "scale with the reduction" rule for other ratios/thresholds), log-interpolated in release.
  The fast-mode rows are corrected for the engine's own ballistic bias (≈ −0.13 dB); measured ΔG = table ± 0.02 dB.
* Ballistics: the static gain reduction passes a 0.85 ms response delay, then two parallel one-pole states in dB
  whose attack and release time constants and weight are tabulated at 0.1 / 0.4 / 1 / 4 s and interpolated
  log-linearly (two-component release `w·e^{−t/τ1} + (1−w)·e^{−t/τ2}`; the attack lengthens with the release,
  §6.6), then a short smoothing stage. Measured (setup below): slow attack 6.8/43/95 ms (0.1–1 s) and 8.5/59/133 ms
  (4 s); fast 1.42/3.2/5.7, 1.38/3.0/5.8, 1.42/3.4/7.1, 1.71/5.1/9.9 ms at 0.1/0.4/1/4 s; releases within 12 %.

**§7 Expander / gate.** Same linked detector. Expander gain = the r2 §7.1 range-40 rows (thresholds −30/−20/−10/0,
−57…−21 dBFS) interpolated linearly in level and threshold (shifted 0.8 dB per dB of threshold outside −30…0,
extrapolated below −57 dBFS with the row's slope), plus a small range-dependent offset (+0.8 / 0.5 / 0.43 / 0.27 /
0.1 / 0 dB at range 5 / 10 / 15 / 20 / 30 / 40, from the other rows), limited by the measured floors (0.8 / 7.7 /
14.5 / 21.4 dB at range 5 / 10 / 15 / 20, then +1.38 dB per dB). Gate: unity at or above the switching level
(expander knee + 1.5 dB: −38.4 dBFS at threshold −20, −30.6 at −10), the floor below (25.4 dB at 20, 52.7 dB at
40, linear). The level envelope follows rises instantly and falls at a constant rate (389 / 157 / 71.4 / 19.3 dB/s
at release 0.1 / 0.4 / 1 / 4 s) — the almost linear-in-time expander closing of the §7 table; the gate gain closes
with a one-pole in dB (0.14 / 0.35 / 0.77 / 1.7 s). Opening: one-pole 1.2 ms (fast 0.15 ms). All twelve §7.1 rows
within 0.5 dB; closing within 9 % (expander) / 21 % (gate) of the §7 table; opening t90 3–4 ms (fast < 1 ms).

**§8 Routing** as in §3; the §8 example values are reproduced within 0.75 dB (test).

**§9 Saturation / analogue.** `y = x + 0.0032·x³` after trim and polarity (+0.021 dB, H3 −62 dBc at 0 dBFS). The
output fader is after it. H2 (informative in r2) is not reproduced, keeping polarity exact. Analogue On:
independent Gaussian noise, −110.7 dBFS rms per channel, plus −69 dB cross-feed (stereo), faded over 10 ms.

**§10 D5 smoothing.** Trim/fader gains and analogue mix: 10 ms linear ramps. EQ and dynamics controls glide over
10–12 ms (coefficients recomputed every 16 samples while moving). Structural switches (polarity, routing, bypasses,
split, filter on/off, bell/shelf, expander/gate) fade the output out over 5 ms, switch at silence and fade back in.

**§13 UI.** Three vector-drawn panels in the Craft palette (graphite panels, amber/teal accents, plain disc knobs
with a value arc; no console look, colour coding or knob caps imitated): *Dynamics* (compressor ratio / threshold /
release / fast + gain-reduction meter; expander/gate range / threshold / release / fast / expander-gate selector +
activity meter; DYN BYPASS, POST EQ), *Equaliser* (HP / LP with SPLIT, HF with BELL, HMF and LMF with Q, LF with
BELL; EQ BYPASS, EQ TO SIDECHAIN), *Input / Output* (input trim, POLARITY, ANALOGUE, IN/OUT meter selector, stereo
level meter, output fader). Values use the §2 resolutions; double-click a value to type, double-click / Alt-click a
control to reset.

## Tests (`tests/`, Catch2, registered with CTest as `console-e/…`)

* **EqTests** — every Appendix A magnitude row (275) at 48 kHz within §12 (absolute bands); the same rows at 44.1 /
  88.2 / 96 / 192 kHz (LP above 50 kHz: ±0.5 dB up to 4 kHz, above that between A.2 and the analogue two-pole of
  its pole pair ±0.5 dB — §4: the 48 kHz roll-off is not present at high rates); HP corners and slope; LP
  monotonic, the §4 20 kHz losses (−0.6 / −2.0 / −7.7 dB), no boost > 0.5 dB at any rate, Off; A.17 peak gains
  (192 rows, ≤ 0.5 dB) and frequencies (±10 %); proportional-Q character and reciprocity; exact identity at
  neutral; stability over the whole control space at three rates.
* **DynamicsTests** — test vector 3 (1 kHz staircases −51…+3 dBFS for all B ratios at +10, 0, −10, −20 against
  B.1 + B.2 read from the spec, ±0.5 dB), make-up, §6.4, linking, §6.5 table (±0.5 dB), §6.6 attack table and §6
  release table (±25 %), 96 kHz time constants, all §7.1 rows (±0.5 dB), expander floors, expander and gate
  closing tables and opening times.
* **EngineTests** — test vector 1 (neutral, ≤ −30 dBFS), zero latency, test vector 5 (trim / fader exact to
  0.002 dB, exact polarity inversion), §9 saturation, test vector 6 (noise, independence, leakage), §8 routing,
  bypasses, mono == one stereo channel, click-free control jumps.
* **ParameterTests** — every taper, the ratio and the HP/LP labels at all 21 points of the §2 table (read from the
  spec), mid-points, inverses, defaults.

Timing setup (not given by the spec): ratio 4, threshold −10 dB, 1 kHz sine stepping between −36.5 and −6.5 dBFS
(≈ 13 dB reduction); expander/gate: threshold −20, range 40, 1 kHz sine falling from 6 dB above the knee /
switching level to 35 dB below the threshold's internal level (−73 dBFS), and back for opening. At fast attack /
0.4 s §6 (≈ 1/2/5 ms) and §6.6 (1.5/3.0/6.0 ms) differ; the newer §6.6 is followed and tested.

## Public references

* M. Vicanek, *Matched Second Order Digital Filters*, 2016 (magnitude-matched numerator, impulse-invariant poles).
* R. Bristow-Johnson, *Cookbook formulae for audio EQ biquad filter coefficients* (peaking / shelving prototypes).
* Butterworth filter design (textbook); RMS detection and dB-domain gain smoothing as in D. Giannoulis,
  M. Massberg, J. D. Reiss, *Digital Dynamic Range Compressor Design — A Tutorial and Analysis*, JAES 2012.
* JUCE 8 and clap-juce-extensions documentation.

## Questions / assumptions

The r1 questions were adjudicated in [docs/verification/console-e-r1.md](../../docs/verification/console-e-r1.md)
and are resolved by r2. Remaining choices:

1. **Low-pass at 96 kHz.** The model makes the 48 kHz-specific roll-off scale with the sample rate (analogue-like
   at 96 kHz, as the verification measured on the reference: the r1 build, which followed A.2 at 96 kHz, was
   1.24 dB darker there). Read literally, "A.2 ±0.5 dB to 12 kHz at every rate" is then not met at 96 kHz (up to
   +1.1 dB at 8 kHz, +2.8 dB at 12.5 kHz); the tests check the intended behaviour instead (see above).
2. **HF bell above 48 kHz**: the direction of the high-rate correction (brighter at 16–20 kHz) is inferred, not
   measured; the result stays within ±1.5 dB of A.2.
3. **Ratio ∞, threshold −10**: the build follows Appendix B.1/B.2 within 0.03 dB at every staircase step, so the
   0.76 dB the verification found is the reference's own deviation from B (§6: "0.5 dB at ∞"). It cannot be
   reproduced without the measured values — please supply an ∞ / −10 staircase if it must be matched.
4. **§6.5 for other ratios / thresholds** uses ΔG as a function of the Appendix B reduction (the informative k rule).
5. **§7.1 between the tabulated thresholds / ranges** follows the interpolation above; the gate switching level is
   the expander knee + 1.5 dB (inside both measured intervals), without hysteresis.
6. **EQ bypass** also bypasses HP/LP in the split position and the EQ in the sidechain; *dynamics bypass* with
   *EQ to sidechain* leaves the audio without EQ.
7. **Untabulated EQ combinations** (e.g. HMF gains other than ±15/+7.5, Q between the measured values at other
   frequencies, shelf frequencies between label positions) follow the borrowed-residual interpolation above.
8. **Stereo linking** uses the larger channel mean square; mono = one channel of the stereo engine (§14.4).

## Status

* Windows x64 Release (MSVC 17 2022): VST3, CLAP, Standalone; 32/32 Catch2 test cases pass (Windows and Linux GCC).
* `tools/hoststress`: no crash, no NaN/Inf, no hang. Samples above +40 dBFS are reported with full-scale input and
  all gains at maximum — expected (§3: no output limiting, as on the reference).
