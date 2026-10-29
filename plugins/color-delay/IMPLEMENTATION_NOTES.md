# Implementation notes — Craft Colour Delay (spec r3)

Clean-room implementation (builder side) of [`spec/color-delay/r3/SPEC.md`](../../spec/color-delay/r3/SPEC.md)
(SHA-256 `ba002d66…d36a2177`, approved 2026-10-29; supersedes r2 `080593b0…97d8bc76` and r1 `a07b0579…02eb55`). Only the
specs, `docs/process.md`, the verification reports [`color-delay-r1.md`](../../docs/verification/color-delay-r1.md) and
[`color-delay-r2.md`](../../docs/verification/color-delay-r2.md), public literature and this implementation were used. No original product, analyst workspace, vendor asset or vendor
installation was accessed.

### Changes for r3 (after the r2 verification)

1. All four colour effects are fed **after** the compressor's gain reduction and the wet gain (r3 §3.4); the r2 build's
   pre-compressor tap is gone.
2. Doubler gain +13.0 dB re its own (post-compressor) input (output gain +13.2 dB: tone +13.2, broadband ≈ +12.6 dB).
   End-to-end at 48 kHz: +13.2 / +9.4 / +1.3 / −4.3 dB for a 1 kHz tone at −40 / −20 / −8 / 0 dBFS (r3: 13.0 / 9.3 /
   1.2 / −4.5).
3. Distortion refitted to r3's end-to-end tables (they include the compressor): stage targets = table − compressor gain
   at that level, at the correspondingly lower stage input level. New knee, voicing, D/M; Send ≤ 25 % is linear in the
   stage; an extra voicing blended in below Send 50 % gives the narrower Send 25 % response; gate −32 dBFS post-drive;
   above 60 kHz a 9 kHz low-pass restores the 48 kHz-bilinear high-frequency droop the fit was made with.
4. Overload bound kept (r3 §7.5).

### Changes for r2 (after the r1 verification)

1. Host-sync lengths are the exact rational floor (`num·60·fs / (den·BPM)` in long double with a 1e-9 guard); the r1
   triplet factor 0.6666666 is gone (1/16T at 120 BPM = 4000, at 100 BPM = 4800, 1/8D at 100 BPM = 21600, …).
2. Reverb: energy is now `E = Σ(hL²+hR²)` for a left impulse (r1 build normalised the mean of L and R, i.e. 3 dB hot);
   output tone shaping matches the octave-band energy profile; the pre-delay settles on whole samples (a fractional
   pre-delay acted as a low-pass at 44.1 kHz); above 60 kHz a 20 kHz low-pass keeps the energy in the audio band, so E
   and the profile are the same at 44.1 / 48 / 96 kHz.
3. Distortion: processed per channel (no L+R sum); Width is a per-channel all-pass pair (no mixing, equal level,
   correlation 0.6 for an identical 1 kHz tone); voicing, knee and drive/make-up refitted to the r2 Send 50/75/100 %
   small-signal and gain-vs-level tables; gate per channel, Send 25 % passes −40 dBFS; first-order antiderivative
   anti-aliasing in the saturator (heavy drive aliased onto the H2 bin at 44.1 kHz).
4. Doubler level: r2 build tapped the effects before the compressor and used +9.4 dB — both superseded by r3.

Product: **Craft Colour Delay** (`CCdl`, CLAP id `com.craftaudio.color-delay`); manufacturer **Craft Audio**
(`Crft`); version 0.1.0. Formats: VST3, CLAP, Standalone. Platforms: Windows x64 (MSVC), Linux x64 (GCC).
Bus layouts: stereo → stereo, and mono → stereo (spec §1 "Mono-to-stereo variant", see assumption 1).

## Architecture

All paths are relative to `plugins/color-delay/`.

```
CMakeLists.txt        targets of this plug-in (added automatically by the root CMakeLists.txt)
source/dsp/           host-independent, header-only DSP core (no JUCE)
  Parameters.h          §2 mappings r -> value + quantisation, note table / host-sync lengths, §6 Dry/Wet law, §7 level laws
  Smoothing.h           one-pole smoother (snaps exactly to its target), linear ramp
  FeedbackDelay.h       §3a integer-sample feedback delay, crossfaded delay-time changes
  Filters.h             first-order / biquad sections, §4 third-order HP/LP pair (+ analog prototype for tests)
  Compressor.h          §5 stereo-linked compressor model
  Reverb.h              §7.1 16-line FDN reverb
  Distortion.h          §7.2 per-channel voiced soft-clip distortion with gate and width
  Doubler.h             §7.3 synchronised dual-tap pitch shifter
  Phaser.h              §7.4 four-notch phaser
  ColourDelayEngine.h   §3 complete signal path (per-sample, double precision), Settings struct
source/plugin/        JUCE AudioProcessor, parameter layout, Delay Link / HP-LP Link logic
source/ui/            editor, look-and-feel, knobs / LED buttons / segmented switches / meters (all vector-drawn)
tests/                Catch2 unit tests of the DSP core (registered with CTest, prefix "color-delay/")
tools/Snapshot.cpp    optional dev tool rendering the editor to PNG (-DCRAFT_BUILD_SNAPSHOT=ON)
```

## How each spec section is implemented

**§1 Scope.** One product; `isBusesLayoutSupported` accepts stereo→stereo and mono→stereo. With a mono input the
processor copies the input to both channels before the engine (identical channel inputs). Latency 0 (no look-ahead;
the dry path is sample-aligned; the minimum delay is 1 sample).

**§2 Parameters.** Every host parameter's normalised value is the spec's `r`. `NormalisableRange` lambdas apply the
mapping **and** the quantisation in `convertFrom0to1` (Input/Output 0.1 dB, Delay 4 ms, percentages 0.1 %, HP/LP
1 Hz, Size 0.1 %, Detune 1 cent, Rate 0.01 Hz), so hosts and the editor only see step values. Toggles are
`AudioParameterBool`; Mode (`ms`/`Host`), note values (19) and Monitor (`L`/`L+R`/`R`) are `AudioParameterChoice`,
whose index is `round(r·(n−1))` — this gives exactly the spec's thresholds (Monitor: r < 0.25 → L, r > 0.75 → R).
Defaults as in the table (Mode Host, 500 ms, 1/4, 29.9 %, Link on, 100 % wet, filters off, effects off, Size 54.9 %,
Levels 50 %, Send 82.2 %, Width 0 %, Detune +8, Rate 0.34 Hz, Monitor L+R). A host Bypass parameter is added.
*Delay Link*: the engine uses the left time/feedback for both channels while linked; additionally the processor mirrors
the left parameters into the right ones (message thread, `AsyncUpdater`), so the linked right controls display the left
values and unlinking keeps the right channel at the values it had while linked. Moving a right control while linked
moves the left one. *HP/LP Link*: on engaging, the product `f_HP·f_LP` is captured (and stored with the state); moving
one filter sets the other to `product / f`, and both stop at `sqrt(product)` where they meet.

**§3 Signal path** (`ColourDelayEngine::processSample`): input gain → per-channel feedback delay
`w[n] = xi[n−D] + g·w[n−D]` (no cross-feed, `g` up to exactly 1.0) → HP → LP → stereo-linked compressor → wet gain →
the four colour effects in parallel on that post-compressor, post-wet-gain signal →
`y = dry·xi + wet + Σ effects` → output gain → Monitor. `D = max(1, round(ms·fs/1000))` in ms mode; in Host mode
`D = max(1, floor(num·60·fs / (den·BPM)))` with the note as an exact fraction `num/den` of a quarter note, evaluated in
long double with a 1e-9 relative guard and limited to 2 s, using the host tempo from the play head (120 BPM if the host
provides none). Internal arithmetic is double precision.

**§4 Filters.** The analog pair is factored as `LP = 1/(s+1) · (1−a s²)/(s²+(1+a)s+1)` and
`HP = s/(s+1) · (s²−a)/(s²+(1+a)s+1)` with `a = 0.132316`, and each section is discretised with the bilinear
transform prewarped at `fc` at the actual rate (`k = tan(π fc/fs)`). Off positions (HP 16 Hz, LP 21357 Hz) bypass the
filter; switching in/out crossfades over ~20 ms; frequency changes are smoothed in the log domain (coefficients
recomputed per sample while moving). `fc` is limited to 0.49 fs for low sample rates.

**§5 Compressor.** Exactly the spec model: detector `max(|L|,|R|)`, instant attack / 1.06 ms one-pole release, soft-knee
static curve (T −23.23 dB, R 6.196, W 23.22 dB), GR smoothing 8.17 ms attack / 439 ms release in dB, output gain
`(1−p)·10^(−GR/20) + p` with p = 0.0292, the same gain on both channels. r2 easing above +4 dBFS: the GR slope is
reduced by 0.25 above +4 dBFS detector level (soft knee 4 dB), so the normative +6 / +12 dBFS gains are met
(−20.85 / −23.09 dB, verified); exactly zero below +2 dBFS.

**§6 Dry/Wet law.** The 21-point wet table; linear in dB between grid points, linear in amplitude between 0 and 5 %;
dry(m) = wet(100 − m). Gains smoothed (~20 ms one-pole, snapping exactly to the target).

**§7 Colour effects** (fed the post-compressor, post-wet-gain stereo signal; on/off crossfaded over ~20 ms,
state cleared when fully off; Level law `(L/100)^3` for the reverb and `^3.5` for the others). Distortion and doubler
are strictly per channel; the reverb is stereo.

- *Reverb* (`Reverb.h`): pre-delay `13 + 0.11·Size` ms (whole samples once settled; fractional only while Size glides)
  → four series Schroeder all-pass diffusers per channel (g = 0.62) → 16-line FDN (14–42 ms lines, normalised Hadamard
  matrix followed by a signed cyclic shift, orthogonal but not involutory). Each line has an absorption filter (gain ×
  first-order low shelf at 300 Hz × high shelves at 1.8 and 5.5 kHz) fitted per line by least squares + Gauss-Newton so
  that its attenuation equals `−60 dB·length/(RT60(f)·fs)` at the seven octave centres of the RT60 table (interpolated
  linearly over Size) and at 16 kHz (RT extrapolated geometrically from 4/8 kHz). Inputs/outputs use mutually orthogonal
  (scrambled Hadamard) vectors, so the two outputs are decorrelated; a small ipsilateral direct share gives the onset at
  the pre-delay. Output tone shaping per channel: first-order high shelf at 3 kHz (−3.3 dB at Size 1 % … −2.4 dB at
  100 %), −1.3 dB peak dip at 250 Hz, 38 Hz high-pass, and a 20 kHz low-pass above 60 kHz (−1.8 dB dip at 15 kHz at
  44.1/48 kHz instead). The output gain makes `E = Σ(hL²+hR²)` (left impulse) follow −0.7 / −0.1 / +0.5 / +0.7 / +0.8 dB
  at Size 1/25/50/75/100 %: an energy model of the network, corrected by measuring the network's impulse response once
  per sample rate at the five Size rows during `prepare` (cached, ~0.1 s). Because the response is limited to the audio
  band, that calibration is sample-rate independent. Measured octave-band density profile re 1 kHz at 44.1/48/96 kHz is
  within 1.3 dB of the spec table (63 Hz – 16 kHz, Size 1/50/100 %).
- *Distortion* (`Distortion.h`), per channel: drive `D(Send)` → gate on the post-drive peak envelope of that channel
  (−32 dBFS, 2 dB hysteresis, 1 ms / 20 ms) → low-Send voicing blend (below Send 50 %, fully at ≤ 25 %: 1st-order HP
  242 Hz, LP 3.86 kHz Q 0.2, peak 9.24 kHz +8.7 dB; 0 dB at 1 kHz) → pre voicing (2× HP 96.9 Hz Q 0.87, LP 1.29 kHz Q 0.3,
  peak 750 Hz +3.9 dB Q 0.87, LP 13.3 kHz Q 1.41; 0 dB at 1 kHz) → symmetric saturator (linear up to |x| = 0.465,
  rational soft limit to 1 above; odd harmonics only; first-order ADAA) → post voicing (peak 2.26 kHz +6.0 dB Q 0.59,
  LP 7.16 kHz Q 1.27; 0 dB at 1 kHz; plus 9 kHz Q 0.6 LP above 60 kHz) → make-up `M(Send)` → 6.5 ms delay → width
  all-pass. `D` (dB) over Send 0/10/25/50/75/100 % = −6/−2/10/24.36/38.09/51.62, `M` = −7.85/−7.85/−11.7/−12.64/−12.72/
  −12.40. r3's tables are end-to-end (plugin input → isolated effect output, compressor included). The stage targets are
  the table value minus the §5 compressor gain at that 1 kHz level (−0.14 / −0.81 / −2.03 / −6.10 / −11.92 / −17.53 dB at
  −32 / −28 / −24 / −16 / −8 / 0 dBFS), at a stage input level lowered by the same amount. Voicing, knee and D/M at
  50/75/100 % were fitted offline to them (least squares on a simulation of this chain at 48 kHz). At Send ≤ 25 % the
  stage stays below the knee (linear, −1.7 dB at 25 %, −9.85 dB at 10 %), so the end-to-end decline is the compressor.
  Width: left ends in a first-order all-pass `(c + z⁻¹)/(1 + c z⁻¹)`, right in the same with `−c`, `c = Width·c_max`,
  `c_max` solved in `prepare` for a 53.1° phase difference at 1 kHz (correlation 0.60, equal level, no mixing). The r3
  informative note on the reference's Width-100 % voicing change is not reproduced.
- *Doubler* (`Doubler.h`): per channel, dual-tap delay-line pitch shifter, delays within 3–10 ms, sin² crossfades, 4-point
  Lagrange interpolation, +13.2 dB output gain (r3: +13.0 dB re its post-compressor input). Each tap's delay moves at `1 − 2^(cents/1200)` samples per sample, so every
  grain carries exactly the shifted pitch. When a grain ends, the tap restarts at the delay (within a 4.3 ms search span)
  whose lag to the other tap maximises the normalised autocorrelation of the input (L+R used only for this search),
  which removes the comb cancellation / splice phase jumps of a free-running shifter. Same tap positions on both channels.
- *Phaser* (`Phaser.h`): four second-order all-pass sections in series (Q 2, centres `f0·{1, 1.45, 2.1, 3.05}`) with
  0.25 feedback, output `0.5·(x + allpass)` → four notches; sine LFO at exactly Phaser Rate sweeps `f0` exponentially
  2.3–4.3 kHz, so the notches cover ≈ 2–15 kHz.

**§8 Parameter changes.** Delay-time changes crossfade linearly (30 ms) between the old and new read positions (a
further change waits for the running crossfade). All other controls use ~20 ms one-pole smoothing that snaps exactly to
the target (so settled settings are bit-exact to the static equations).

**§12 GUI.** Original vector-drawn editor (940×610), dark graphite with an amber delay section and one colour per
effect; own two-ring mark and "Colour Delay" badge with four colour dots. Upper delay panel: Feedback L (small knob),
Link (LED button), badge, Mode switch (ms/Host), Feedback R; large Delay L/R knobs (show ms in ms mode, the note value in
Host mode) with note drop-downs beneath (enabled in Host mode); between them Dry/Wet, HP, Link, LP. Lower row: Reverb,
Distortion, Doubler, Phaser modules, each with an on/off LED button, a Level knob and Size / Send + Width / Detune /
Rate. Left edge: input meter above Input; right edge: output meter above Output and the L/ST/R Monitor switch. Small
knobs show their value while hovered/dragged; double-click or Alt-click resets; linked right controls are dimmed and
mirror the left values; tooltips explain the toggles. The header shows the host tempo in Host mode.

## Tests (`tests/`, Catch2, 35 test cases / 3225 assertions, ~18 s with `ctest -j`; effects at 44.1/48/96 kHz)

- §2 mappings, quantisation round trips, note table; host-sync lengths: all r2 §11 examples (120/100/137/140 BPM) and
  every note × 9 tempi × 3 rates against an exact integer rational floor; 2 s limit; ms lengths; Delay Link.
- §6 Dry/Wet law at all 21 grid points, interpolation; §7 level laws (−17.5 / −37 / −21 dB within ±1.5 dB), also
  through the engine (Dry/Wet 25 % lowers the effects by the wet law).
- §11 vectors: impulse 0.001 FS, 100 ms, 50 % → 0.001, 0.0005, 0.00025 at 100/200/300 ms, nothing else, R silent;
  Dry/Wet 25 % → 0.74989 dry / 0.24547 wet; HP/LP 1 kHz values (±0.02 dB); Input +6 dB → +5.99 dB ± 0.03;
  1 kHz static gains at −30/−12/0 dBFS; gain recovery after −6 → −40 dBFS; distortion routing end-to-end (L only →
  +11.7 dB left, right silent; both → +11.7 dB each); doubler end-to-end on a 1 kHz tone on both channels at −40 / −20 /
  −8 / 0 dBFS (13.0 / 9.3 / 1.2 / −4.5 ± 1 dB, both channels) and on a −40 dBFS one-sided sweep (12.9 ± 1 dB, nothing on
  the right); distortion Send 10 % silent at −32 dBFS.
- §10 [exact]: the engine against an independent straight-line model (single third-order polynomial bilinear filter in
  long double, plain delay recursion) on noise, random settings, 44.1/48/96 kHz: ≤ −90 dB re peak. Filters: −3.01 dB at
  fc, LP + HP all-pass and agreement with the analog prototype.
- §5/§10 compressor: static gains at 1 kHz (−33 … +12 dBFS) within ±0.3 dB; frequency dependence 120 Hz – 4 kHz within
  ±0.3–0.5 dB, 8/16 kHz and 60 Hz within ±1 dB; envelope vs an independent implementation of the §5 model ≤ 0.5 dB RMS;
  stereo linking; 0.5 FS impulse within 1 dB of ×0.908.
- Reverb (44.1/48/96 kHz, Size 1/25/50/75/100 %): E within ±1.5 dB (measured within 0.01 dB), octave-band energy
  density profile re 1 kHz within ±2 dB (Size 1/50/100 %), RT60 per octave band (Schroeder T20) within ±15 %, pre-delay
  ±3 ms, |ρ| < 0.2.
- Distortion (44.1/48/96 kHz), end-to-end through the engine as in r3 (Delay 100 ms, Feedback 0, effect isolated by
  subtraction): small-signal response Send 50 % ±2 dB (100 Hz – 4 kHz), Send 25/75/100 % ±3 dB; 1 kHz gain vs level
  (Send 10/25/50/100 %) ±3 dB; H3 profile ±4 dB. Component level: H2 < −95 dB; no output on the
  undriven channel (< −90 dB, also at Width 100 % and loud input); gate per channel (Send 10 %: silent at −32, open at
  −28 dBFS; Send 0 %: silent at −28, open at −24; Send 25 %: open at −40); Width 0 % identical, Width 100 % correlation
  0.6 ± 0.1 and level difference ≤ 0.5 dB; latency 6.8 ± 1 ms.
- Doubler: pitch within ±0.5 cent (median of 20 ms frequency estimates; 440 Hz / 1 kHz / 2.5 kHz × ±1…±100 cents),
  level (component, re its input) 13.0 ± 1 dB tone / ± 1.5 dB band-limited noise, latency 3–10 ms.
- Phaser: ≥ 4 notches at four LFO positions, notch range 1.9–14.6 kHz; LFO rate within ±5 % (0.34, 2, 7 Hz).
- Monitor, effects silent at Dry/Wet 0 %, effects off contribute nothing (bit-identical), click-free delay change,
  Feedback 100 % lossless, host bypass bit-exact once faded, runaway protection (Feedback 100 %, +18 dB in/out,
  full-scale square, all effects: finite and ≤ +30 dBFS).

End-to-end distortion results at 48 kHz (1 kHz, dB re plugin input; r3 table within 0.2 dB everywhere): Send 100 %
−40…0 dBFS 28.7 / 21.3 / 13.5 / 5.6 / −2.4 / −10.4; Send 50 % 11.7 / 10.7 / 8.6 / 2.4 / −4.9 / −12.3; Send 25 % −1.7 / −1.9 /
−3.7 / −7.8 / −13.6 / −19.3; Send 10 % gated at −32, −10.7 / −11.9 / −16.0 / −21.8 / −27.4 at −28 / −24 / −16 / −8 / 0 dBFS.
Small signal Send 25 % 100 Hz…8 kHz −5.7 / −1.7 / −10.7 / −24.2 / −41.1; Send 50 % 100 Hz…8 kHz 12.1 / 14.7 / 13.9 / 11.7 /
6.5 / −3.2 / −18.2; Send 100 % 26.2 / 28.7 / 30.3 / 24.3 / 9.3 (44.1/96 kHz within 1 dB up to 8 kHz). H3 −8.3 (spec −9),
−6.3 (−5), −20.1 (−20), −10.0 (−11) dB.

## Questions / assumptions (for the review gate)

1. **Mono-to-stereo** is offered as an additional bus layout of the same product (mono input copied to both channels),
   not as a separate product; hosts pick it on mono tracks.
2. **Host-sync rounding** (r2): exact rational floor; the reference's occasional one-sample irregularity is not
   reproduced (per r2).
3. **Phaser Rate resolution**: 0.01 Hz steps; below 0.01 Hz (r ≈ 0 … 0.08) the unrounded value is kept so the bottom of
   the range (≈ 0.005 Hz) stays reachable.
4. **Compressor** (r2): +6 / +12 dBFS easing as above; recovery at 500 ms −4.45 dB and 30 Hz −13.1 dB are the §5
   model's values and within the r2 tolerances. Single-sample transients (§13.1) and in-loop interaction with feedback
   (§13.2) are not modelled.
5. **Effect routing** (r3): the effects take the post-compressor, post-wet-gain signal (the r2 build's pre-compressor
   tap was non-conformant). The phaser's output level is `0.5·(x + allpass)` at Level 100 % (not characterised).
6. **Distortion routing / Width** (r2): strictly per channel; Width realised as opposite-sign first-order all-passes
   (my choice of mechanism; r2 fixes only no mixing, equal level and correlation ≈ 0.6 at 1 kHz).
7. **Distortion voicing**: the Send 100 % "small-signal" data are taken in saturation, so they define the post voicing;
   pre = Send 50 % shape minus post; the Send 25 % shape needs a Send-dependent extra voicing (blended between 50 and
   25 %). Make-up at Send 0 % is unspecified (taken equal to 10 %).
8. **Gate**: per channel on the broadband post-drive level (before the voicing), −32 dBFS with 2 dB hysteresis,
   1 ms attack / 20 ms release (spec gives only thresholds; Send 0 / 10 / 25 % thresholds land as specified).
9. **Doubler latency** is interpreted as the 3–10 ms range of the sweeping taps (6.5 ms at Detune 0). Pitch accuracy is
   per grain. Gain +13.2 dB: tones +13.2, broadband ≈ +12.6 dB (r3 +13.0 / +12.9).
10. **Reverb**: E as defined in r2 (sum over both outputs, left impulse); the octave-band profile is read as mean power
    density per octave band re the 1 kHz band (FFT of the impulse responses); L/R energies are within 0.3 dB of each
    other (r2 informs that the reference's right channel is 1.6–1.7 dB lower; not targeted). Pre-delay is the onset at
    −40 dB re peak; Size changes glide (pre-delay smoothed, absorption redesigned every 64 samples).
11. **Delay Link editing**: while linked, moving a right control moves the left one (both always equal); the reference
    behaviour for this case is not specified.
12. **HP/LP Link** product is captured when the link is switched on and saved with the plug-in state; with a filter at its
    off position the other filter is computed from 16 Hz / 21357 Hz respectively.
13. **Bypass**: host bypass parameter, 20 ms crossfade to the unprocessed input (delay keeps running).
14. **Meters**: input meter shows the level after the Input gain; peak meters −48 … +6 dBFS, ~36 dB/s fall, 1.5 s hold.
15. **Tail length** reported as 10 s (Feedback 100 % repeats indefinitely).
16. Fixed editor size 940×610; not resizable.
17. **Runaway protection** (not in the spec): a soft clip that is exactly transparent up to +24 dBFS and bounded at
    +30 dBFS sits on the value written into the feedback delay line and on the final output. Feedback 100 % with a
    sustained full-scale input otherwise grows without bound (found with `tools/hoststress`); nothing the spec measures
    reaches +24 dBFS.

## Public references and provenance

- J.-M. Jot, A. Chaigne, "Digital delay networks for designing artificial reverberators", AES 90th Conv., 1991; J. Stautner,
  M. Puckette, "Designing multi-channel reverberators", CMJ 6(1), 1982 (FDN with orthogonal feedback matrix and
  per-line absorption); M. R. Schroeder, "Natural sounding artificial reverberation", JAES 10(3), 1962 (all-pass
  diffusers); J. Dattorro, "Effect Design, Part 1/2", JAES 45(9)/(10), 1997 (diffusion, delay modulation, pitch shifting).
- U. Zölzer (ed.), *DAFX*, 2nd ed., 2011 — delay-line pitch shifting with crossfades and synchronised splicing,
  phasers with all-pass chains, soft-clipping distortion, shelving filters.
- D. Giannoulis, M. Massberg, J. D. Reiss, JAES 60(6), 2012 — soft-knee curve and smoothing (only to read §5).
- R. Bristow-Johnson, "Audio EQ Cookbook" — biquad low/high-pass, peaking and all-pass designs, bilinear prewarping.
- M. R. Schroeder, "New method of measuring reverberation time", JASA 37, 1965 — backward integration (tests).
- JUCE 8 public API documentation, clap-juce-extensions README, Catch2 v3 documentation.

## Building

From the repository root (all plug-ins):

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure -R color-delay
```

Artefacts: `build/plugins/color-delay/CraftColourDelay_artefacts/Release/{VST3,CLAP,Standalone}`. `ci/package.py` finds
them generically (product zip `craft-colour-delay-<version>-<platform>.zip`). Editor snapshot:
`-DCRAFT_BUILD_SNAPSHOT=ON`, then run `color_delay_snapshot <dir>`.

## Status

r3: Windows x64 Release build (MSVC 2022, VST3 + CLAP + Standalone, no warnings in this plug-in) and all 35 unit tests
pass. `tools/hoststress` (from main) ran 4 + 3 min against the VST3 (44.1/48/96 kHz, mono/stereo in, float/double,
blocks 1…4096, random automation, state restores, editor open/close): 0 failures (also on Linux / GCC 13 for the DSP core); `ci/package.py` produces `craft-colour-delay-<version>-windows-x64.zip`.
The Linux plug-in targets were not built locally; they use the same portable CMake as the other Craft plug-ins and are
covered by CI.
