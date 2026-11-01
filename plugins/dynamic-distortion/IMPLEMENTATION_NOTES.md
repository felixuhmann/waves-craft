# Implementation notes — Craft Dynamic Distortion (spec r2)

Clean-room implementation built only from [`spec/dynamic-distortion/r2/SPEC.md`](../../spec/dynamic-distortion/r2/SPEC.md)
and public DSP literature. Product **Craft Dynamic Distortion** by Craft Audio, plugin code `CDds`, CLAP id
`com.craftaudio.dynamic-distortion`, formats VST3 / CLAP / Standalone (Windows x64, Linux x64).

Stereo in / stereo out with fully independent channels; mono via the bus layout (mono → mono, or mono → stereo
where both outputs carry the same processed signal). Reported latency: **37 samples** at every host rate
(the 4x oversampler of the anti-aliased distortion path, review decision D1; ≤ 64).

## Architecture

```
source/dsp/            host-independent, header-only DSP core (no JUCE)
  Parameters.h         section 3 mappings (r -> value), section 4.1 level laws, section 4.2 drive table
  Filters.h            Biquad (TDF II, bilinear with pre-warp; matched first-order LP), fitted analog prototypes
                       of the tone EQ, HP and LP
  Expansion.h          peak follower (attack / release / 2 ms hold) and the expansion curve
  Clipper.h            hard clipper with first-order antiderivative anti-aliasing + magnitude compensation
  Oversampling.h       2x / 4x linear-phase half-band up/down sampler (shared design with the Craft Motion Filter)
  DynamicDistortion.h  the engine: settings, smoothing, per-channel signal flow, latency
source/plugin/         JUCE AudioProcessor, APVTS parameter layout, value text
source/ui/             editor, look and feel, knob / meter controls, tone response display (all vector drawn)
tests/                 Catch2 tests (SpecTables.h = transcription of the normative tables)
tools/Calibrate.cpp    development aid: refits the expansion curve and prints the full spec comparison
tools/fit_filters.py   development aid: fits the filter prototypes to the spec tables (numpy / scipy)
tools/Snapshot.cpp     development aid: renders the editor to PNG (CRAFT_BUILD_SNAPSHOT)
```

Signal flow per channel (`DynamicDistortion::processSample`):

```
x ─► ×Input ─┬─────────────────────────────────────────────────────────────── delay(37) ─► ×Direct ──┐
             └─► 4x up ─┬─► peak follower ─► expansion gain E                                         │
                        └─► Bass ─► Mid ─► Treble ─► ×Drive ─► ×E ─► ADAA clip                       + ─► ×Output
                                ─► HP ─► LP (off at r = 1) ─► 4x down ─► ×Distortion Level ────────────────────┘
```

Everything in the distortion path, including the detector, runs at 4x the host rate (176.4–768 kHz), so all
filters follow their analog prototypes up to 20 kHz at every host rate (section 1 / 9 sample-rate independence).
The direct path is an integer delay of the input; with Input/Output 0 dB and Direct 100 % it is bit-exact.

## Spec mapping

| Spec | Implementation |
|---|---|
| §3 parameters | `param::*` (`fromNorm` is the spec mapping of r); host parameters in `ParameterLayout.cpp` with the spec's display resolution (0.1 dB, 0.1 %, 0.1 ms, 0.01 ms, 0.5, 1 Hz) |
| §4 flow, detector placement | detector input = upsampled signal after Input gain, before the EQ; EQ/Drive/E before the clipper; HP, LP after it; Output = Output gain · (Direct + Distortion) |
| §4.1 level laws | `laws::directGain`, `laws::distortionGain`: the tables, linear in dB between rows, 0 % = silence |
| §4.2 drive | `laws::driveDb` (table, linear in dB) plus a constant correction for the analog HP response at 1 kHz, so the measured 1 kHz small-signal gain equals the table |
| §4.3 expansion | `expansion::Curve`: the §4.3 table on its own axis (peak dBFS), linear in dB between knots; small soft onset at −41.5…−40 dBFS, −14 dBFS knot 6.66 dB, flat above (see assumptions) |
| §4.3 detector | `PeakFollower`: branching full-wave peak follower in the linear domain with τ_att = Attack/3.67, τ_rel = Release/1.99 (§8) and a 2 ms hold before the release phase (see assumptions) |
| §4.4 clipper | plain memoryless symmetric hard clip at c = 0.307 (`AdaaClipper`), no emphasis |
| §4.5 tone EQ | Bass: RBJ low shelf, 100 Hz, Q 0.40. Mid / Treble: bell with fixed zeros `(s²+s/Qn+1)/(s²+s/(g·Qn)+1)` (Treble 7947 Hz, Qn 0.3239; Mid: centre ratio and Qn per Mid Freq anchor 150 / 1252 / 5000 Hz, interpolated in log f). Cut depth 18·\|v\|/50 dB; boost = exact inverse of the cut × broadband −6·v/50 dB (Bass, Treble) / −9·v/50 dB (Mid) |
| §4.6 LP | off (flat) at r = 1, crossfaded; otherwise two identical first-order sections 1/(1+s/ωc), −6.02 dB at the set frequency, matched design |
| §4.6 HP | 2nd-order 24.437 Hz / Q 0.92207 × 1st-order 25.357 Hz (fit error ≤ 0.01 dB) |
| §5 small signal | linear below the expansion onset; tests check h2/h3 ≤ −50 dBc at −50 dBFS |
| §6.1 / §6.2 | consequences of the above; verified cell by cell (tests) |
| §6.3 linking | none: per-channel state only; a left-only tone leaves the right output exactly 0 |
| §7 / D1 aliasing | 4x oversampling (63-tap root-half-band + 23-tap half-band FIRs) **and** first-order ADAA. Verified ≤ −40 dBc at 44.1 / 48 / 96 kHz by the tests (15 kHz, −6 dBFS peak, Drive 50 %). |
| D1 latency | 37 samples, reported via `setLatencySamples`, direct path delayed to match |
| D2 smoothing | gains, EQ values, Mid Freq and LP (log domain) and bypass glide with a 4 ms one-pole (99 % in 18 ms); filter coefficients are redesigned every 16 samples while gliding; Attack/Release change the follower coefficients directly (click-free by construction) |
| D3 sag | not modelled |
| §12 UI | see below |

### Tone EQ fits (tools/fit_filters.py)

| Band | Prototype | Max fit error vs table |
|---|---|---|
| Bass (−50, −25; +25/+50 by mirror) | RBJ low shelf 100.0 Hz, Q 0.400 | 0.01 dB |
| Mid 150 Hz | bell, centre 149.97 Hz, Qn 1.9995 | 0.01 dB |
| Mid 1252 Hz | bell, centre 1244.4 Hz, Qn 1.184 | 0.16 dB |
| Mid 5000 Hz | bell, centre 4831 Hz, Qn 1.020 | 0.33 dB |
| Treble (−50 and +25 jointly) | bell, centre 7947 Hz, Qn 0.3239 | 0.20 dB |

### User interface (§12)

Original vector-drawn editor (860 × 500) in the Craft family style (dark panels, coloured section ticks):

- **Distortion section**: Input knob with stereo input meter, a large Drive knob, a *Dynamics* group with
  Attack and Release and a live expansion-gain bar (0…+8 dB), a *Mix* group with Direct and Distortion
  level, Output knob with stereo output meter.
- **Tone section (pre-distortion)**: Bass, Mid, Mid Freq, Treble and Low-pass knobs with a response curve
  of EQ + LP (drawn from the same analog prototypes as the DSP; markers at Mid Freq and the LP corner).
- Every knob shows its value; double-click or Alt-click resets to the default; clicking the value readout
  lets the user type a value (units optional, `k` suffix for kHz, `s` for seconds). Header: wordmark,
  version and latency, Bypass button (host bypass parameter, latency-compensated crossfade).

## Tests (`tests/`, Catch2, registered with CTest as `dynamic-distortion/*`)

Test cases and assertions: see Status. Normative checks covered:

| Area | Checks |
|---|---|
| §3 mappings, defaults, round trips | all 12 parameters at 6 positions |
| §4.1 level laws | 26 table rows (law + measured through the processor), 50–100 % linearity, monotonicity |
| §4.2 drive | 21 table rows measured at 1 kHz (±0.1 dB) + interpolation |
| §4.3 expansion | `expansion::Curve`: the §4.3 table on its own axis (peak dBFS), linear in dB between knots; small soft onset at −41.5…−40 dBFS, −14 dBFS knot 6.66 dB, flat above (see assumptions) |
| detector placement | EQ does not move the onset, Input gain moves it 1:1 |
| §4.4 clipper | plain memoryless symmetric hard clip at c = 0.307 (`AdaaClipper`), no emphasis |
| §4.5 tone EQ | 13 rows × 12 frequencies at 44.1 / 48 / 96 kHz (±0.5 dB, ±1 dB above 15 kHz); cut/boost mirror + level drop for 5 values × 3 bands × 7 frequencies; depth and boost peaks; Mid centre tracking |
| §4.6 LP / HP | LP 3 rows × 12 frequencies (r2 tolerances), −6.02 dB at fc, flat at r = 1 at 3 rates; HP 7 points at 3 rates; HP absent from the direct path |
| pre/post order | Bass −50 prevents a 50 Hz tone from clipping; LP 1 kHz still clips a 2 kHz tone |
| §5 | harmonics ≤ −50 dBc at −50 dBFS for 18 settings; composite small-signal gain |
| §6.1 | 5 tables × 24 cells × (fundamental, h3) with the §6.1 tolerance rules = 240 checks at 48 kHz |
| §6.2 | 8 rows × 12 columns (↓+10 and the Release 3000 rising columns informative) at 44.1 / 48 / 96 kHz |
| §6.3 / mono | left-only tone → right exactly 0; mono processing equals the stereo channel |
| §7 / D1 | aliases ≤ −40 dBc at 44.1 / 48 / 96 kHz; latency 37 ≤ 64 at 22.05–192 kHz; direct path bit-exact and aligned |
| §9 sample rate | every §6.1 fundamental at 44.1 and 96 kHz within ±0.2 dB of 48 kHz (240 checks) |
| D2 | 11 parameter jumps: no step larger than the steady signal's, settled within 0.25 dB after 20 ms |
| robustness | silence → exact silence at extreme settings, finite output for 1e6 / 1e-30 inputs, bypass bit-exact, float = double |

Run: `ctest --test-dir build -R dynamic-distortion` (≈ 2 min serial in Release). `dynamic_distortion_calibrate report
[fs]` prints every §4.3 / §6.1 / §6.2 / §7 comparison with pass/fail marks.

## Questions / assumptions

All §6 checks now run with r2's peak levels and rules, with no widened bounds. Three choices were forced by the tables
and differ from a literal reading of r2; the review gate should look at them:

1. **Detector hold (2 ms) kept.** r2 says it is not needed, but without it, with a curve that meets §4.3 ±0.4 dB, this
   build fails A 500 ↑+990 (−3.00 vs −0.25 dB, ±2.5) and A 0.1 / R 5 h3 at −20 dBFS (200 Hz −31, 500 Hz −37 dBc; limit
   −40). The §8 model's lower 0.253 dB/dB slope and 5.86 dB cap do not meet §4.3 at −14 dBFS, so it is not an option here.
2. **c = 0.307, not 0.304** (inside r2's ±0.01). With 0.304 and the lowest curve §4.3 allows, A 100 / 50 Hz / −10 dBFS has
   h3 −19.86 dBc against the ≤ −20 dBc rule. Margin at 0.307 is small.
3. **Curve at the top.** −14 dBFS knot 6.66 dB (table 7.05 ±0.4, near the tolerance edge) and flat above, for the same
   h3 rule; a soft onset of +0.05 dB at −40 dBFS for the Drive 50, 50 Hz, −40 dBFS onset cell (table −38 dBc).

Other assumptions: "EQ neutral" = Bass/Mid/Treble 0 and LP off; level laws below 5 % fade linearly in amplitude; Mid
between anchors is interpolated in log2 f and uses the bell family that fits Treble at two depths; mono = one channel of
the stereo processing, mono → stereo processes both outputs identically; bypass crossfades (4 ms) to the aligned dry
input; §6.2 gain uses the 1 ms window ending at the stated time.

## Public references

- R. Bristow-Johnson, *Cookbook formulae for audio EQ biquad filter coefficients* (RBJ audio-EQ cookbook) — shelf
  prototype, bilinear transform with pre-warping.
- D. Giannoulis, M. Massberg, J. D. Reiss, "Digital dynamic range compressor design — a tutorial and analysis",
  JAES 60(6), 2012 — branching peak detector, soft knees.
- J. Parker, V. Zavalishin, E. Le Bivic, "Reducing the aliasing of nonlinear waveshaping using continuous-time
  convolution", DAFx 2016 — first-order antiderivative anti-aliasing of the hard clipper.
- A. V. Oppenheim, R. W. Schafer, *Discrete-Time Signal Processing* — Kaiser-window FIR design (oversampling
  filters), impulse-invariant / matched first-order sections.

## Status

Built to spec r2. Windows x64 Release: VST3, CLAP and Standalone build, installed; 33/33 tests pass (123 596 assertions on Linux GCC). MSVC reports C4723 in Clipper.h when inlined into the tests; the division is guarded. `craft_hoststress --seconds 120 --editor`: 22 M samples, 0 failures. The Linux plugin build is left to CI.
