# Implementation notes: Craft FET Limiter (spec r1)

Builder side of a clean-room implementation of [`spec/fet-limiter/r1/SPEC.md`](../../spec/fet-limiter/r1/SPEC.md)
(approved 2026-10-29) and its normative tables [`spec/fet-limiter/r1/tables/`](../../spec/fet-limiter/r1/tables).
I used only that spec and its tables, this repository (conventions, the structure of the merged Craft plug-ins),
public DSP literature (listed below) and my own work. I did not access any original product, analyst workspace,
vendor installation, manual, preset or image, and did not load any third-party plug-in.

| Target | Product | Plugin code | CLAP id | I/O (spec §1) |
|---|---|---|---|---|
| `CraftFetLimiter` | Craft FET Limiter | `CFet` | `com.craftaudio.fet-limiter` | Stereo 2→2 (one linked detector) or Mono 1→1 via the bus layout |

- **Manufacturer:** Craft Audio (`Crft`). **Version:** 0.1.0. **Formats:** VST3, CLAP, Standalone.
- **Platforms:** Windows x64 (MSVC) and Linux x64 (GCC). **Licence:** AGPL-3.0 (as the repository).
- **Latency:** 0 samples (§1). Both revisions are one product; **Revision** (A / B) is a host parameter.

## Architecture

All paths are relative to `plugins/fet-limiter/`.

```
CMakeLists.txt             fet_limiter_dsp (INTERFACE), CraftFetLimiter plug-in, optional snapshot tool
cmake/EmbedTables.cmake    turns spec/fet-limiter/r1/tables/*.csv into build/.../generated/fet_limiter/SpecTables.h
                           (one string array per table, regenerated when a CSV changes; nothing is read at run time)
source/dsp/                host-independent, header-only DSP core (no JUCE), double precision
  Tables.h                   CSV parser, piecewise-linear curves, the parsed tables (thread-safe static)
  Parameters.h               §2 host-value mappings, display strings, Settings
  GainLaws.h                 §4 Input / Output gain laws, All makeup, §5 static law as gain-cell target
  Dynamics.h                 §6 peak detector and gain-cell state network (attack, program-dependent release)
  DynamicsMap.h              GENERATED: fitted gain-cell parameters per Revision / Ratio / Attack / Release
  Colouration.h              §7.2 asymmetric saturator, output protection
  Filters.h                  §7.1 response filters (coupling high-pass, bell, top-end low-pass sections)
  Noise.h                    §7.3 Analog noise and mains hum
  AutoMakeup.h               §8 editor linkage (Output follows Input gestures)
  FetEngine.h                §3 signal flow, smoothing (D3), meters
source/plugin/             JUCE AudioProcessor + parameter layout
source/ui/                 editor, look and feel, knobs, button groups, needle meter (all vector graphics)
tests/                     Catch2 tests (CTest prefix "fet-limiter/")
tools/                     development aids (not built by default, not shipped):
  simwrap.cpp                exposes Dynamics.h to Python (shared library)
  fetfit_common.py           table loading + simulation wrapper
  fit_dynamics.py            staged least-squares fit of the gain-cell parameters to timing / trajectory tables
  emit_dynamics_map.py       writes source/dsp/DynamicsMap.h from the fit output
  design_hf_filters.py       fits the top-end biquads to the analog prototypes per standard sample rate
  Snapshot.cpp               renders the editor to PNG (CRAFT_BUILD_SNAPSHOT)
```

Per sample: `u = x · Gin` (smoothed linear gain); the detector takes `max(|uL|, |uR|)`; its envelope (sine peak)
gives the internal level `x_dB`; the static law gives the target reduction `T`; the gain cell gives `GR(t)`;
`s = u · 10^(−GR·on/20) · makeup` (on = 0 for Comp Off, makeup = All small-signal gain); `w = filters(saturate(s)) · Gout`
(+ noise / hum); `y = protect(Trim · (Mix · w + (1 − Mix) · x))`.

## Spec mapping

| Spec | Implementation |
|---|---|
| §1 variants | stereo with one detector (`max(|L|,|R|)`) and one gain; mono = the same engine on one channel (D5). Channels never mix (silent right stays exactly 0). Latency 0. |
| §2 parameters | `ParameterLayout.cpp`. Input/Output: custom `NormalisableRange` implementing r=0 → −∞, `−60+96r` / `−36+48(r−0.25)`, 0.1 dB snap; display "-inf dB" at r=0. Attack/Release `1+6r` (0.01). Ratio/Analog/Revision/Meter are stepped float parameters whose value is the index and whose host-value→index thresholds are exactly the spec's (0.1125/0.3625/…; 0.2375/0.7375; 0.5) — `AudioParameterChoice` would round at different thresholds. Canonical host values k/(n−1) lie inside the bands. Comp Off / Auto Makeup bool. Mix `100r` (0.1 %), Trim `−18+36r` (0.1 dB). Defaults as §2. |
| §3 flow | `FetEngine::process`. Mix 0 % with Trim 0 dB returns the input bit-exactly (`1·x + 0·w`, gains land exactly on their targets). Trim is an exact dB gain after Mix. |
| §3 Comp Off | GR forced to 0 through a 3 ms smoothed factor; gain staging, saturation, filters, noise unchanged; the All makeup is part of the GR path and is also removed (assumption G3). |
| §4 gain staging | `inputGain` / `outputGain` interpolate the tables linearly in display dB (normative law). Between the −∞ row and the first finite Input row the amplitude is interpolated linearly (G1). Output display −60 (r=0) is −∞; any r>0 follows the table (row −60 = −73.98 dB). All: +1.2 dB (A, the table's own low-level value 1.21) / +0.6 dB (B) small-signal makeup. |
| §5 static law | `StaticLaw` holds `R(x) = max(0, top − GR_table(x))` per revision/ratio (top = 1.21 for All A, 0 otherwise); the gain cell's steady state equals the target exactly, so the steady-state gain change is the table value (B All: table + 0.6 dB, G2). Settled curve per S1. |
| §6.1 detector | Peak detector with instantaneous attack, 6 ms hold (refreshed by peaks within 0.5 % of the held value) and 4 ms decay: a steady sine ≥ 84 Hz gives a ripple-free sine-peak level. Feed-forward detector with a stateful gain cell (the spec leaves the law open, §11.1); the "feedback" character (overshoot, creep, depth-dependent attack) is reproduced by the gain cell. |
| §6.2–6.3 dynamics | `GainCell` (Dynamics.h): states C (level memory), F (output), H, K (charge stages) and M (peak memory). Attack towards `T − β(T − C)` with `τA · (21 dB/T)^p`; release F→H→K→T−offset with an acceleration term. Parameters per setting from `DynamicsMap.h` (fit described below). All times in ms, so behaviour is sample-rate independent (§9). |
| §6.4 link | max(|L|,|R|) detection, the same gain on both channels. |
| §7.1 response | Filters.h: 2 Hz coupling high-pass, LF bell, top-end low-pass section(s), analog prototypes fitted to `freq_response.csv` (rev A: bell 49.3 Hz −0.357 dB Q 0.483, LP 25.77 kHz Q 0.724; rev B: bell 95.8 Hz −0.181 dB Q 0.626, LP 11.95 kHz Q 0.628 + 18.25 kHz Q 1.49). The top-end sections use biquads least-squares fitted to the analog prototype at 44.1/48/88.2/96/176.4/192 kHz (tools/design_hf_filters.py, max. error 0.2 dB up to 22 kHz), a prewarped bilinear design at other rates. |
| §7.2 harmonics | Colouration.h: memoryless asymmetric curve `s/(1+|s/c|^n)^(1/n) + a2·sp² + a3·sp³` (negative half saturates earlier) on the internal signal after the gain cell; constants fitted to the Comp Off rows (1 kHz levels −30/−18/−6 dBFS at Input −12, plus the 100 Hz rows), unit small-signal slope. With compression the post-GR internal level is low, giving H2 ≈ −45…−55 dBc. |
| §7.3 noise | Noise.h: rev A low-frequency noise (white → 1 Hz HP → two 6 Hz LP poles), −71.8 dBFS rms; rev B white noise + mains line (50/60 Hz, −89 dBFS peak) + 3rd harmonic (−95.5 dBFS peak), total −76.1 dBFS rms. Added after the Output gain, independent of Input; independent noise per channel. Off: nothing is added (silence in → silence out). |
| §8 Auto Makeup | AutoMakeup.h + editor: on a user gesture on Input (drag, wheel, typed value, reset) the editor moves Output (as a host-recorded gesture) so that the static steady-state level of a −18 dBFS sine stays constant, computed from the §4/§5 laws relative to the gesture start; Output is limited to its range (stops following at 0 dB). Host automation of Input does not move Output (the linkage lives in the editor only). |
| §9 sample rate | All time constants physical; detector hold/decay in ms; response per §7.1 analog prototype. |
| §10 D1–D6 | D1 1 = slowest; D2 non-linear scales reproduced; D3 3 ms one-pole smoothing (99 % after ~14 ms) of Input/Output/Trim/Mix/Comp Off/makeup gains, dynamics parameter changes act on a continuous state; D4 noise off by default; D5 mono = same processing; D6 editor-only linkage. |
| §12 UI | One panel, original vector drawing in the Craft style: Gain section (large Input/Output rotaries with −∞ … 0 scale labels at their host positions and readouts), Dynamics (Attack/Release rotaries "slow < > fast", 1…7 scale; latching Ratio column 20/12/8/4/All), Meter (original moving-needle meter with OVER lamp; GR/In/Out column; Comp Off), bottom strip (Auto Makeup, Analog 50 Hz/60 Hz/Off, Revision A/B, Mix, Trim). Revision A uses an amber theme, B an ice-blue theme. Double-click / Alt-click resets, click on a readout types a value. |

## Dynamics fit (how the tables are met)

The gain-cell structure (Dynamics.h) was designed from the shapes described in §6: fast attack with a slower
creep (or overshoot) towards the settled value, release that is fast and roughly exponential after short bursts and
slower and closer to linear after long holds, near-linear "after a short delay" recovery for All. Its 11 parameters
were fitted by least squares (tools/fit_dynamics.py, calling the C++ code through tools/simwrap.cpp) to the
`timing.csv` t63/t90 values (log error, ±25 % scale), the `release_trajectories.csv` points (±1.5 dB scale; approx
rows by shape, ±3 dB) and the burst-end GR, in stages: (1) Attack 3 / Release 4 together with the Input −24 rows
(which fixes the depth dependence), (2) the attack group per Attack setting 1/5/7 and the release group per Release
setting 1…7, and complete sets for ratios 8/20/All and revision B at their measured setting pairs, (3) the fast
pairs (7/7) and rev B 1/1. `DynamicsMap.h` interpolates between the anchors (log-linear for times, as §6.2 asks)
and applies ratio/revision modifiers (D2).

## Tests

`tests/` (Catch2, CTest prefix `fet-limiter/`). The tables are the embedded CSVs, so every row is checked through
the complete engine (not the gain laws in isolation):

- GainStagingTests: every `gain_input.csv` / `gain_output.csv` row (±0.2 dB), −∞ = silence, interpolation
  between rows, All makeup per revision, 44.1/96 kHz equality (0.07 dB).
- StaticLawTests: every settled `static_gr.csv` point per revision/ratio at 1 kHz (±1.0 dB, All ±2.5 dB), knee
  positions, maximum reduction, Release influence ≤ 0.3 dB, 44.1/96 kHz vs 48 kHz ≤ 0.07 dB.
- DynamicsTests: every `timing.csv` row — attack t63 (50 ms holds), release t63/t90 (±25 %) — and every
  `release_trajectories.csv` point (±1.5 dB; approx rows by shape ±3 dB), monotonic Attack/Release,
  ratio ordering, stereo link, 96 kHz vs 48 kHz times (2 %).
- ColourationTests: `freq_response.csv` per revision at 48 kHz (±0.3 dB below 15 kHz, ±1 dB above) and at
  44.1/96 kHz; Comp Off harmonic rows (1 k / 5 k / 100 Hz), the §7.2 typical ranges with compression, even-order
  content; silence in → silence out; noise rms and line levels (±2 dB), independent of Input.
- EngineTests: Mix 0 bit-exact (float/double), Trim exactness, Mix crossfade, zero latency, stereo link, no
  crosstalk, mono = stereo-left, Comp Off, click-free parameter steps.
- ParameterTests: all §2 mappings and thresholds, defaults, Auto Makeup (−18 dBFS sine within ±2 dB).

## Status / results

- Windows x64 (MSVC 2022, Release): VST3, CLAP, Standalone build without warnings; **38/38 CTest cases pass**
  (Catch2: 242 779 assertions; same on Linux x64 / GCC). Installed to the user and common VST3 folders and the
  user CLAP folder.
- `craft_hoststress` (VST3, `--editor`, 300 s, seed 7): 1.35·10⁹ samples, 335 prepares, 1840 state restores,
  2.86 M parameter moves, 109 editor opens, **0 failures** (an earlier 120 s run was also clean).
- Gain staging, static law, frequency response (48 / 44.1 / 96 kHz), Comp Off harmonics, noise, mix/trim, latency,
  link, sample-rate behaviour and Auto Makeup meet the spec tolerances at every table row tested.

### Dynamics: known deviations

Of the 1 275 normative timing checks (84 timing rows: 1 092 trajectory points, 84 t63, 79 t90, 20 attack t63)
1 220 (95.7 %) are within the spec tolerance (±25 %, ±1.5 dB, approx rows ±3 dB by shape). The remaining 55 lie
in the configurations below; the test holds them to the stated wider bounds (and every other configuration to the
spec tolerance) so they cannot regress unnoticed:

| Configuration | Bound used | Where it deviates |
|---|---|---|
| ratio 4, Attack 3 / Release 4 (`att_3_r4`, `rel_4_a3`) | 2.0 dB | 5 ms burst: recovery 200–400 ms up to 1.8 dB ahead |
| Attack 5 / 7 (`att_5_r4`, `att_7_r4`) | 2.5 / 3.5 dB | 5 ms burst: recovery 100–800 ms up to 2.4 / 3.5 dB ahead |
| Release 2 (approx) | 3.1 dB shape | 3.0 dB at 3.2 s |
| Release 7 (`rel_7_a3`) | 2.3 dB, 40 % | 5 ms burst t90 +37 %, GR still rising 10 ms after the burst; 100 ms point of long holds 1.6 dB |
| ratio 8 / 20, 7 / 7 | 2.0 / 2.3 dB, 40 / 55 % | 5 ms burst t90 +40 / +53 %; early recovery of long holds 1.6–1.9 dB late |
| All, 3 / 4 | 2.9 dB | recovery shape up to 2.8 dB |
| All, 7 / 7 | 9.0 dB, 32 % | 5 ms burst: the reference holds ≈ 30 ms before its fast drop, the model drops at once (8.7 dB at 100 ms, t63 −30 %); long holds up to 3.3 dB |
| rev B, 7 / 7 | 45 % | attack t63 0.29 ms vs 0.21 ms |
| Input −24 (12 dB GR), 3 / 4 | 1.9 dB, 40 % | release t63 +38 % after 50 ms burst |

The common cause is structural: one parameter set per setting cannot give both the very fast, almost
exponential recovery after 5 ms bursts and the slower, more linear one after long holds at the fastest settings,
and the depth dependence (Input −24 rows) pulls the Release 4 parameters towards a linear recovery. A literal
feedback model of the gain cell (§11.1, open) would be the next step.

## Questions / assumptions

- **S1 (static table precedence).** "E rows take precedence over A rows at the same x; interpolate linearly." Read
  literally, interleaving E and A rows makes the All rev A curve zig-zag by up to 2.7 dB (its A staircase was not
  settled). I use the E rows as the settled curve wherever they exist and the A rows only outside the E rows' x
  range (and for curves without E rows). Tests check every E row and every A row outside that range.
- **G1.** Between the Input table's −∞ row (display −60) and its first finite row the gain is interpolated linearly
  in amplitude (the dB interpolation is undefined there). Output display −60 is the r = 0 end and means −∞; r > 0
  follows the table (whose −60 row is −73.98 dB).
- **G2.** Rev B All: §4 asks for +0.6 dB small-signal makeup but the rev B All table rows are ≈ 0 at low level; I add
  +0.6 dB to the whole rev B All curve (within its ±2.5 dB tolerance).
- **G3.** Comp Off also removes the All makeup (it is part of the GR table).
- **D1 (detector).** Feed-forward peak detector + stateful gain cell instead of a literal feedback loop (§11.1 leaves
  the law open; dynamics are table-normative).
- **D2 (unmeasured combinations).** Ratio 12 dynamics = mean of the ratio 8 and 20 modifiers; revision B with ratio ≠ 4
  combines the revision and ratio modifiers; settings between anchors are interpolated, outside held.
- **D3 (approx rows).** The rel_1/rel_2/revB_a1_r1 rows end their bursts at 14.6 … 20 dB GR although the same input
  gives ≈ 21 dB at other Release settings — which contradicts §5 (Release changes GR by ≤ 0.3 dB). I follow §5 and
  compare these trajectories by shape (normalised to the burst-end GR, scaled to 21 dB, ±3 dB).
- **D4.** Burst tests use the burst level whose settled static reduction equals the 2 s-hold `gr_end` of the row
  (§6.3 "≈ 21 dB GR").
- **C1.** Harmonic rows other than Comp Off do not state their settings (and their gains show history from earlier
  steps), so they are checked against the §7.2 typical ranges rather than row by row.
- **C2.** The saturator has no oversampling (zero latency); aliasing only matters for Comp Off at extreme levels.
- **C4 (known deviation).** The Comp Off rows at −30 dBFS / 100 Hz differ from the same rows at 1 kHz (rev A H2
  −53.7 vs −45.0 dBc, rev B H3 −58.3 vs −52.5 dBc: a frequency-dependent low-level behaviour). The memoryless
  saturator gives the 1 kHz values at every frequency (measured here: rev A H2 ≈ −43 dBc, rev B H3 −53.4 dBc), so
  these two rows are not asserted; all other Comp Off rows (1 kHz, 5 kHz, 100 Hz at −18 / −6 dBFS) are.
- **C3 (output protection).** A soft ceiling above +30 dBFS (max. +35.6 dBFS) protects hosts from Comp Off + Input 0 +
  full-scale input + Trim +18 dB (otherwise > +40 dBFS). It never acts in any measured configuration.
- **N1.** Line levels in `noise.csv` are taken as sine peak levels (FFT dBFS); noise spectra beyond the stated rms /
  lines are my choice (rev A: < 10 Hz, rev B: white).
- **A1 (Auto Makeup).** "No movement below threshold" and "keep a −18 dBFS sine within ±2 dB" conflict when the
  −18 dBFS sine is below threshold; the requirement (keep the level) wins.
- **M1 (meter).** The meter scale (−20 … +3, 0 = −18 dBFS in In/Out modes) and its ballistics are my choice.

## Public references

- U. Zölzer (ed.), *DAFX – Digital Audio Effects*, 2nd ed., Wiley 2011, ch. 4 (dynamics processing, peak detectors,
  attack/release smoothing).
- D. Giannoulis, M. Massberg, J. D. Reiss, "Digital Dynamic Range Compressor Design — A Tutorial and Analysis",
  JAES 60(6), 2012 (detector topologies, feed-forward vs feedback, smoothing in the log domain).
- M. Vicanek, "Matched Second Order Digital Filters", 2016 (matched-magnitude biquads; idea of keeping the analog
  prototype near Nyquist).
- R. Bristow-Johnson, "Cookbook formulae for audio EQ biquad filter coefficients" (peaking EQ, low-pass).
- JUCE 8 and CLAP / clap-juce-extensions documentation.
