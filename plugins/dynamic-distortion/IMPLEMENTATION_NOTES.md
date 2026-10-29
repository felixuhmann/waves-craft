# Implementation notes — Craft Dynamic Distortion (spec r1)

Clean-room implementation built only from [`spec/dynamic-distortion/r1/SPEC.md`](../../spec/dynamic-distortion/r1/SPEC.md)
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
                       of the tone EQ, HP, LP and the clip emphasis pair
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
                        └─► Bass ─► Mid ─► Treble ─► ×Drive ─► ×E ─► pre-emph ─► ADAA clip ─► de-emph   + ─► ×Output
                                ─► HP ─► LP ─► LP ─► 4x down ─► ×Distortion Level ────────────────────┘
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
| §4.2 drive | `laws::driveDb` (table, linear in dB) plus a constant correction for the analog HP and LP-21 kHz response at 1 kHz, so the measured 1 kHz small-signal gain equals the table |
| §4.3 expansion | `expansion::Curve`: the §4.3 table with RMS levels converted to the detector's peak scale (+3.01 dB), linear in dB between knots, 0 dB below the first knot, 0.3 dB/dB above the table, capped at +8 dB |
| §4.3 detector | `PeakFollower`: branching full-wave peak follower in the linear domain with τ_att = Attack/3.67, τ_rel = Release/1.99 (§8) and a 2 ms hold before the release phase (see assumptions) |
| §4.4 clipper | memoryless symmetric hard clip (`AdaaClipper`) inside a small-signal-transparent first-order emphasis pair; long clipped half-cycles settle at c = 0.310 (−10.2 dBFS), see assumptions |
| §4.5 tone EQ | Bass: RBJ low shelf, 100 Hz, Q 0.40. Mid / Treble: bell with fixed zeros `(s²+s/Qn+1)/(s²+s/(g·Qn)+1)` (Treble 7947 Hz, Qn 0.3239; Mid: centre ratio and Qn per Mid Freq anchor 150 / 1252 / 5000 Hz, interpolated in log f). Cut depth 18·\|v\|/50 dB; boost = exact inverse of the cut × broadband −6·v/50 dB (Bass, Treble) / −9·v/50 dB (Mid) |
| §4.6 LP | two identical first-order sections 1/(1+s/ωc) (−6.02 dB at the set frequency), matched design (pole impulse-invariant, zero matching the analog Nyquist gain) |
| §4.6 HP | 2nd-order 24.437 Hz / Q 0.92207 × 1st-order 25.357 Hz (fit error ≤ 0.01 dB) |
| §5 small signal | linear below the expansion onset; tests check h2/h3 ≤ −50 dBc at −50 dBFS |
| §6.1 / §6.2 | consequences of the above; verified cell by cell (tests) |
| §6.3 linking | none: per-channel state only; a left-only tone leaves the right output exactly 0 |
| §7 / D1 aliasing | 4x oversampling (63-tap root-half-band + 23-tap half-band FIRs) **and** first-order ADAA. 15 kHz / −6 dBFS / Drive 50 %: 3 kHz alias −54.6 dBc (48 kHz), worst alias −42.9 (44.1 kHz), −54.6 (48 kHz), −55.1 (96 kHz) dBc |
| D1 latency | 37 samples, reported via `setLatencySamples`, direct path delayed to match |
| D2 smoothing | gains, EQ values, Mid Freq and LP (log domain) and bypass glide with a 4 ms one-pole (99 % in 18 ms); filter coefficients are redesigned every 16 samples while gliding; Attack/Release change the follower coefficients directly (click-free by construction) |
| D3 sag | not modelled as such (the emphasis pair produces a similar sag at low frequencies as a side effect) |
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

33 test cases, 123 576 assertions (bit-exactness loops included). Normative checks covered:

| Area | Checks |
|---|---|
| §3 mappings, defaults, round trips | all 12 parameters at 6 positions |
| §4.1 level laws | 26 table rows (law + measured through the processor), 50–100 % linearity, monotonicity |
| §4.2 drive | 21 table rows measured at 1 kHz (±0.1 dB) + interpolation |
| §4.3 expansion | 14 table levels ±0.4 dB at 44.1 / 48 / 96 kHz, constant below −40 dBFS, continuation slope ≤ 0.4 dB/dB, cap 8 dB |
| detector placement | EQ does not move the onset, Input gain moves it 1:1 |
| §4.4 clipper | low-frequency ceiling 0.30–0.32 (−10.1 ± 0.3 dBFS), fully clipped h3 −9.5 / h5 −14 dBc, no even harmonics |
| §4.5 tone EQ | 13 rows × 12 frequencies at 44.1 / 48 / 96 kHz (±0.5 dB, ±1 dB above 15 kHz); cut/boost mirror + level drop for 5 values × 3 bands × 7 frequencies; depth and boost peaks; Mid centre tracking |
| §4.6 LP / HP | LP 3 rows × 12 frequencies and −6.02 dB at fc at 3 rates; HP 7 points at 3 rates; HP absent from the direct path |
| pre/post order | Bass −50 prevents a 50 Hz tone from clipping; LP 1 kHz still clips a 2 kHz tone |
| §5 | harmonics ≤ −50 dBc at −50 dBFS for 18 settings; composite small-signal gain |
| §6.1 | 5 tables × 24 cells × (fundamental, h3) with the §6.1 tolerance rules = 240 checks at 48 kHz |
| §6.2 | 8 rows × 12 columns (↓+10 informative) at 44.1 / 48 / 96 kHz |
| §6.3 / mono | left-only tone → right exactly 0; mono processing equals the stereo channel |
| §7 / D1 | aliases ≤ −40 dBc at 44.1 / 48 / 96 kHz; latency 37 ≤ 64 at 22.05–192 kHz; direct path bit-exact and aligned |
| §9 sample rate | every §6.1 fundamental at 44.1 and 96 kHz within ±0.2 dB of 48 kHz (240 checks) |
| D2 | 11 parameter jumps: no step larger than the steady signal's, settled within 0.25 dB after 20 ms |
| robustness | silence → exact silence at extreme settings, finite output for 1e6 / 1e-30 inputs, bypass bit-exact, float = double |

Run: `ctest --test-dir build -R dynamic-distortion` (≈ 2 min serial in Release). `dynamic_distortion_calibrate report
[fs]` prints every §4.3 / §6.1 / §6.2 / §7 comparison with pass/fail marks.

## Known deviations (spec-internal conflicts)

1. **LP 500 / 2000 at 20 kHz** (§4.6 table: −61.34 / −38.82 dB). The normative text (two identical first-order
   sections, −6.02 dB at the set frequency, sample-rate independent) gives −64.09 / −40.09 dB there; every other
   cell of the three LP rows (including LP 8000 at 20 kHz, −17.21 dB) agrees with that structure within 0.3 dB.
   The implementation follows the text; the tests assert the literal structure and document the 2.75 / 1.27 dB
   table differences (both cells are below −38 dB).
2. **Drive 0 %, Attack 100 ms, −10 dBFS** (§6.1): the table shows an almost unclipped tone (h3 −41…−44 dBc) at a
   fundamental of −7.2 dBFS RMS (0.62 peak). The Drive 50 / 100 tables and §4.4 put the fully clipped fundamental
   at −8.0 dBFS (2 kHz) / −10.9 dBFS (50 Hz), i.e. below that level, and the expansion is limited to ≤ 0.4 dB/dB
   above −14 dBFS by §4.3 — no memoryless clipper satisfies both. The implementation clips these four cells
   (h3 −12…−17 dBc; fundamentals within 2 dB, inside the ±3.5 dB knee tolerance). The neighbouring cell
   **Attack 100 ms, 50 Hz, 0 dBFS** (table −7.1 dB) is 3.8 dB low (tolerance 3.5) for the same reason.
   These five of 240 §6.1 checks are asserted against documented wider bounds.

All other normative table entries are met at 44.1, 48 and 96 kHz.

## Questions / assumptions (for the review gate)

1. **Clip level (§4.4 vs §4.3 / §6.1).** With a flat hard limit at c ≈ 0.304 a 1 kHz tone would already clip at
   −14 dBFS (contradicting the clean §4.3 measurement up to −14 dBFS) and the fully clipped fundamental at
   200 Hz–2 kHz would be 3.3 dB below the §6.1 values (the §8 model's "max 3.2 dB" error). The §6.1 ceilings rise
   from −10.9 dBFS (50 Hz) to −8.0 dBFS (2 kHz). I placed the memoryless hard clip (ceiling 0.4461) between a
   first-order low-shelf boost and its exact inverse (zero 93.37 Hz, pole 134.48 Hz): transparent for small
   signals (all §4–§5 responses unchanged), the clip ceiling is 0.446 at high and 0.310 at low frequencies,
   i.e. §4.4's c = 0.30–0.32 / −10.1 dBFS is the level long clipped half-cycles settle at. Fit error of the
   four ceilings: ≤ 0.02 dB.
2. **Attack/release law (open question 1).** The §8 branching follower alone leaves the 1 kHz steady state
   attack-dependent (A 500: −3.0 dB vs −0.25 dB in §6.2) and follows 200–500 Hz waveforms with A 0.1 / R 5
   (h3 −31 dBc vs ≤ −40 required). A 2 ms hold before the release phase fixes both while keeping the
   low-frequency (50 Hz) waveform following that §6.1 / §4.3 describe. With the hold the steady-state detector
   sits on the 1 kHz peak, so the §4.3 table is used directly as the curve (levels + 3.01 dB).
3. **h3 tolerance gap (§6.1).** Table values in (−50, −40] dBc are not covered by the rules; I apply the one-sided
   onset-band bound (≤ table + 18 dB), consistent with the one-sided rule for ≤ −50 dBc.
4. **§6.2 measurement.** Gain over the 1 ms window *ending* at the stated time (latency compensated). The
   Release 3000 row is preceded by 6 s at −50 dBFS as the spec requires; its "up" columns are then compared with
   the A 1 / R 31.8 row (the table's +0.85…+0.91 dB reflect the reference's preceding programme). ↓+10 ms is
   informative only and not asserted.
5. **Drive table conditions.** "EQ neutral" = Bass/Mid/Treble 0; LP taken as 21 kHz (as in §6.1; LP 13 kHz would
   differ by 0.03 dB).
6. **Level laws below 5 %** (not tabulated): linear fade in amplitude from silence at 0 % to the 5 % value.
7. **Mid band between anchors**: centre ratio and Qn interpolated linearly in log2 f between the three fitted
   anchors; the Mid response at intermediate depths uses the same fixed-zero bell family that fits Treble at
   two depths (not measurable from the spec).
8. **Mono variant**: one channel of the stereo processing (§10); mono → stereo layouts process the copy on both
   channels identically.
9. **Bypass**: crossfade (4 ms) to the latency-aligned dry input; the reported latency is unchanged.

## Public references

- R. Bristow-Johnson, *Cookbook formulae for audio EQ biquad filter coefficients* (RBJ audio-EQ cookbook) — shelf
  prototype, bilinear transform with pre-warping.
- D. Giannoulis, M. Massberg, J. D. Reiss, "Digital dynamic range compressor design — a tutorial and analysis",
  JAES 60(6), 2012 — branching peak detector, soft knees.
- J. Parker, V. Zavalishin, E. Le Bivic, "Reducing the aliasing of nonlinear waveshaping using continuous-time
  convolution", DAFx 2016 — first-order antiderivative anti-aliasing of the hard clipper.
- J. Pakarinen, D. T. Yeh, "A review of digital techniques for modeling vacuum-tube guitar amplifiers", CMJ 33(2),
  2009 — pre/post-emphasis around static nonlinearities.
- A. V. Oppenheim, R. W. Schafer, *Discrete-Time Signal Processing* — Kaiser-window FIR design (oversampling
  filters), impulse-invariant / matched first-order sections.

## Status

- DSP core, tests, plugin (VST3 / CLAP / Standalone) and editor complete.
- Windows x64 Release (MSVC 2022): VST3, CLAP and Standalone build without warnings in plugin code; 33/33 tests pass
  (also on Linux GCC: 123 576 assertions). Installed to the per-user and system VST3 folders and the per-user CLAP folder.
- `craft_hoststress --seconds 600 --editor` on the VST3: 179 M samples, 461 prepares, 2292 state restores,
  527 k parameter moves, 153 editor opens, 0 failures.
- `ci/package.py` packages the VST3, CLAP and Standalone as `craft-dynamic-distortion-<version>-<platform>.zip`.
