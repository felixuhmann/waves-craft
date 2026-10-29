# Implementation notes — Craft Motion Filter (spec r2)

Clean-room implementation (builder side) of [`spec/motion-filter/r2/SPEC.md`](../../spec/motion-filter/r2/SPEC.md)
(approved 2026-10-29; supersedes r1 after the [verification of the r1 build](../../docs/verification/motion-filter-r1.md)).
Only the spec, the repository, public documentation and this
implementation were used. No original product, analyst workspace, vendor asset or vendor installation was accessed.

Product: **Craft Motion Filter** (`CMfl`, CLAP id `com.craftaudio.motion-filter`), manufacturer **Craft Audio**
(`Crft`), version 0.1.0. Formats: VST3, CLAP, Standalone. Platforms: Windows x64 (MSVC), Linux x64 (GCC).
Licence: AGPL-3.0 like the rest of the repository (JUCE under AGPL), see [THIRD_PARTY.md](../../THIRD_PARTY.md).

## Architecture

All paths relative to `plugins/motion-filter/`.

```
CMakeLists.txt     targets (added automatically by the root CMakeLists.txt)
source/dsp/        host-independent, header-only DSP core (no JUCE), double precision
  Parameters.h       §3 mappings r -> value, choice lists, Settings (physical values), Transport
  Laws.h             the measured laws: Q(Res), spread, comb gain, tapers, follower/amp laws, mixer and delay-mix
                     tables, drive, crush, fixed-chain constants
  Blocks.h           one-pole smoother, biquad (§4 wet high-pass, fitted §4 output response), TPT SVF section,
                     Hermite delay buffer, comb, saturator, crusher, feedback guard, PRNG
  Vcf.h              per-channel filter stage: all six types run continuously, crossfaded by smoothed weights
  Modulators.h       LFO, step sequencer (swing), envelope follower, transport-locked clock
  Delay.h            delay channel with digital (20 ms crossfade) / analog (100 ms glide) time changes
  MotionEngine.h     complete per-sample processor at the engine rate (sections 2-10)
  Oversampling.h     63-tap root-half-band + 23-tap half-band FIRs, 2x / 4x up/down with integer latency
  MotionFilter.h     top level: engine at >= 176.4 kHz (4x at 44.1/48, 2x at 88.2/96), §4 output response at base rate
source/plugin/     JUCE AudioProcessor (buses, parameters, playhead, MIDI) — ParameterLayout, PluginProcessor
source/ui/         editor, look-and-feel, knobs / mod sliders / toggles, response display, step editor
tests/             Catch2 tests of the DSP core (registered with CTest, prefix "motion-filter/")
tools/Snapshot.cpp optional editor-to-PNG renderer (-DCRAFT_BUILD_SNAPSHOT=ON)
```

Per channel (engine, `MotionEngine.h`):

```
x = in * Input
Route pre:      line <- guard(x + fb*tap);  p = dDry*x + dWet*tap;  wet = Chain(p);  y = mixDry*x + mixWet*wet
Route feedback: wet = Chain(x);  line <- guard(Chain'(x + fb*tap));  y = dDry*(mixDry*x + mixWet*wet) + dWet*tap
out = y * Output          (then down-sampling and the §4 output response at the host rate)
Chain = saturator(drive) -> VCF -> crush -> 21.3 Hz Butterworth high-pass * +0.35 dB
```

`Chain'` is a second instance of the same chain inside the delay loop, so in feedback route each repeat is
filtered again, while the delay mix at −100 still gives just the filtered signal.

## How each spec section is implemented

**§1 Scope.** Stereo in/out; mono→stereo and mono→mono bus layouts (mono input feeds both channels; mono out
takes the left result). Optional stereo/mono sidechain bus for the follower. The engine runs oversampled with
linear-phase FIRs (outer stage: "root half-band" for both up and down filter, so the round trip of the passband
is flat up to the base Nyquist; inner 4x stage: half-band): 4x at 44.1/48 kHz (latency 37 samples), 2x at 88.2/96 kHz (31 samples), 1x at
≥ 120 kHz (0); dry and wet pass the same filters, so they stay aligned; reported via `setLatencySamples`
(≤ 64 as required).

**§3 Parameters.** Every host parameter's normalised value is the spec's r; the JUCE `NormalisableRange`
lambdas apply the §3 mappings. Choice lists use `AudioParameterChoice` (r mapped uniformly, nearest). The
shared Freq/Delay/Gain knob is one parameter (value in Hz = 30·600^r); its text follows the current filter type
(Hz, ms, dB). Res/Feedback is one parameter. Last step is an int 1–16. Additional: host Bypass (5 ms one-pole
crossfade to the dry input; the bypassed signal is the latency-compensated input).

**§4 Fixed chain.** Wet: bilinear 2nd-order Butterworth high-pass at 21.3 Hz (−3.6 dB @ 20 Hz, −0.14 @ 50 Hz)
with +0.35 dB. Output HF response: a biquad least-squares fitted offline (in dB) to the five spec points for
44.1 kHz and for 48 kHz respectively (coefficients in `designOutputTone`, zeros reflected to minimum phase);
rates in 40–60 kHz use the nearest fit; below 40 kHz and at ≥ 60 kHz it is flat (review decision 5). Because the
outer resampling filters are power-complementary (|H(f)|² + |H(fs−f)|² = 1), the resampler adds < 0.4 dB at 22 kHz
for a 44.1 kHz host (r1 build: −5.5 dB from a plain half-band pair, verification issue 6). Baseline saturation
comes from the saturator (0.24 dB at −3 dBFS, drive 0).

**§5 Filter.** Zavalishin TPT state-variable filter, prewarped g = tan(π fc/fs) so fc equals the knob at any
rate (review decision 1), run at ≥ 176.4 kHz so the magnitudes follow the analog prototypes up to 16 kHz
(no bilinear cramping; at 2x a 5 kHz LP was still 1.6–2 dB too low at 12.5–16 kHz). Outputs (r2) LP = 1/D,
**BP = s/D** (peak gain Q: +3.0 dB at Res 50, +6.5 dB at Res 90), HP = s²/D, BR = (1 + s²)/D; 24 dB = two
identical sections (BP peak Q²). fc = knob · 2^(mod + keyboard + spread), clamped to [30 Hz, 18.6 kHz] (and
0.45·engine rate). Q law (r2): 1/Q = 1 − 0.00586·Res up to 90 (Q 2.12), then linear to Q = 2.40 at 100. The SVF
is linear — Q is level-independent (r1's level-dependent damping was removed). No self-oscillation. Spread ±0.0144 oct/% on L/R. Comb: feedback
comb y[n] = x[n−D] + g·y[n−D], D = displayed ms (review decision 2), Hermite-interpolated, g = 0.5 + 0.0029·Fb%.
Amp: displayed dB, −80 dB = mute. Type and slope changes are ~20 ms crossfades (all types always run).

**§6 Modulation.** Per destination the source offsets are summed and slewed by a one-pole with τ = Smooth%·1 ms
(0 = none), per channel. Depth tapers T_k with k = 3.57 (LFO) / 3.04 (sequencer), applied to the
smoothed amount. Freq: sequencer 5.17·v·T, LFO 9.23·l·T. Follower (r2 §6.3 table): raw = 6.4·A·T_f(|a|)/T_f(0.5)
octaves with a follower taper k_f = 4.26 (so +100 % has 9.4× the +50 % slope: +0.60 oct at A = 0.01); positive
offsets saturate softly at +1.82 oct (raw/(1 + (raw/1.82)⁴)^¼); negative offsets are linear with a mild
expansion (×(1 + 0.25·(raw/3.26)⁴)) so −50 % at A = 0.51 reaches −4.1 oct (table: ≤ −4). Res: 100 points per sequencer unit (clamped 0–100). Amp gain:
g = max(0, g_knob + 0.0647(e^(1.502m) − 1)). Comb delay and delay time: offset in units of the knob range
(octaves / 9.23): comb r' = r + oct/9.23; delay time × 20^(oct/9.23).
LFO: phase 0 at plugin start and at transport start; when synced and the transport runs, the phase follows the
host ppq position (so it is locked to the bar); free rates exact. Sequencer: starts on step 1 (review decision 4),
loops 1..Last, synced = one step per note length, locked to ppq while playing. Follower: per channel peak
detector, attack 0.3 ms, release 20 ms, from the (gain-staged) input or the sidechain bus.
Keyboard tracking: last MIDI note-on n, sample accurate: +(n−60)/12 octaves; comb delay × 2^(−(n−60)/12).

**§7 Mixer.** Input/Output plain gains; filter mix dry (1−m)/2, wet (1+m)/2.

**§8 Delay.** Delay time = displayed ms (review decision 3) or note length at host tempo; right channel × 1 /
1.5 / 2; feedback exactly linear, guarded by a soft limiter that is linear up to |x| = 1 and saturates towards 2.
Delay mix: Catmull-Rom interpolation of the 9-point dry/tap table. Digital: new times via a 20 ms crossfade
between old and new taps; analog: read position glides with a 100 ms one-pole (pitch bend). Max 8.5 s.

**§9 Drive / Crush.** Drive: gain 10^(0.073·d/20) into c·tanh(u/c) (odd-symmetric) with
c = 2.104·e^(−0.005925·d). The verification found 1.82 dB instead of 2.7 dB at 100 % for its "−3 dBFS" stimulus
although a −3 dBFS 1 kHz sine at the plug-in input gave exactly 2.7 dB; all three r1 build readings were ×0.66 of
the sine values, i.e. the harness's stimulus reaches the saturator ≈ 1.8 dB lower. The ceiling is therefore fitted
to the mean of both level references: 0.24/0.48/0.95/3.13 dB at −3 dBFS and 0.16/0.32/0.65/2.27 dB at −4.8 dBFS
for 0/25/50/100 % — both within the spec's 0.2/0.4/0.8/2.7 ±0.5 dB. (Placement unchanged: saturator directly
before the filter; with the filter wide open nothing else compresses now that the SVF is linear.)
Crush (r2): sample-and-hold with hold length N = 1 + 64·(Crush/100)² samples at 48 kHz, implemented as a hold
rate 48 000/N Hz on a phase accumulator at the engine rate, so the hold time N/48 000 s is the same at every host
rate; held values keep the level; 0 = off.

**§10 Smoothing.** Knob values ramp with 10 ms one-poles (freq in the log/knob domain, gains linear, delay-mix
gains, drive gain and ceiling, tapered mod amounts); types/slope crossfade ~20 ms; bypass 5 ms.

**§13 UI.** Original vector-drawn editor (1000×640, dark graphite panels, colour per section: amber filter, teal
LFO, violet sequencer, blue follower, coral delay/character; own "resonant curve + three source dots" logo).
Header: name, version, bypass. Filter: six type buttons with drawn response icons; Freq/Delay/Gain and
Res/Feedback knobs (titles follow the type), each with three bipolar mod sliders LFO/SEQ/FOLLOW; Slope 12/24 and
key-track toggles. Response display: live (modulated) L and R curves computed from the prototypes (comb: exact
comb magnitude; amp: flat), grid, handle at the knob position; dragging sets Freq (x) and Res (y), double-click
resets. Character: Spread, Smooth, Drive, Crush. Mixer: Mix, Input, Output. Follower source Input/SC. LFO: rate
knob (shows the sync value when Sync is on), Free/Sync, five shape buttons with drawn waveforms. Sequencer:
16 bipolar bars (current step highlighted, steps after Last dimmed), Last (drag/scroll box), Swing, Rate
(free/sync), Clock Free/Sync, Edit Drag/Draw (Drag moves one bar, Shift = fine; Draw paints across bars;
double-click resets a bar). Delay: Time knob (free/sync) with three mod sliders, Free/Sync, Right ×1/×1.5/×2,
Feedback, Dry/Wet, Digital/Analog, Pre/Fbk.

## Tests (`tests/`, Catch2)

45 test cases, 8747 assertions, all passing on Windows (MSVC, ctest) and Linux (GCC). Coverage:

- §3 mappings, ranges, defaults, choice lists.
- §5 small-signal magnitude of LP/HP/BP/BR × 12/24 × fc 100/1k/5k × Res 0/50/90 at 44.1/48/96 kHz through the
  complete processor (chain removed by dividing by a transparent reference), vs. the prototypes within ±1.0 dB
  (12 dB LP/HP/BR) / ±1.5 dB (BP, 24 dB) where |H| > −20 dB; cutoff within ±3 % and |LP(fc)| = Q ±10 % at
  44.1/48/96/192 kHz up to 10 kHz; BP peak gain = Q (Q² for 24 dB, +3.0/+6.5 dB at Res 50/90) and −3 dB
  bandwidth; Q at Res 90/95/100 (2.12/2.26/2.40) level-independent (−30 vs −3 dBFS within 0.05); no self-oscillation; Q law; spread; cutoff clamps; comb delay ±1 sample and feedback ratio ±0.01 at 3 rates;
  amp gains and mute; click-free type/slope changes.
- §6 tapers; sequencer→freq at all 9 measured points (±0.15 oct / 8 %); LFO ±0.39/±1.33 oct; res and amp
  destinations; delay-time destination; LFO waveforms, S&H one value per cycle; free LFO period ±0.1 %; synced
  periods at 120 BPM (1/2 = 1 s, 1/6, 1/12, 1/1), phase locked to ppq; sequencer start on step 1, free and
  synced step timing, Last loop, swing law; follower: all 14 r2 table entries (±50 %, +100 %, A = 0.01…0.51) at 44.1/48/96 kHz within ±0.2 oct /
  ±25 %, −50 % at A = 0.51 ≤ −4 oct, ballistics; Smooth τ ±20 %;
  keyboard tracking.
- §4/§7/§8/§9: wet high-pass and +0.35 dB; output HF response at 44.1/48 kHz incl. 22 kHz and flat at 88.2/96/192 kHz; mix
  law; delay taps at displayed/synced times ±1 sample incl. right ratio and latency; feedback ratios ±0.01;
  delay mix table ±0.03; pre vs. feedback route; 100 % feedback sustain and guard; digital/analog time changes;
  drive gain and compression ±0.5 dB at −3 and −4.8 dBFS; baseline saturation; crush hold length N ±1 sample at
  48 kHz for Crush 25/50/75/100, hold rate at 44.1/48/96/192 kHz, level preserved; sample-rate
  independence; click-free parameter changes; robustness at extreme settings; bypass and latency.

## Questions / assumptions (for the review gate)

1. **Band-pass gain** (r1 contradiction): resolved by r2 — BP = s/D, peak gain Q.
2. **Q between Res 90 and 100**: r2 — linear 2.12 → 2.40, level-independent.
3. **Saturation** (§11 curve unspecified): tanh drive saturator with drive-dependent ceiling, fitted to the
   mean of the −3 dBFS and the verification harness's effective level (see §9 above).
4. **Oversampling**: 4x below 60 kHz (37 samples), 2x below 120 kHz (31), none above (0) — CPU in exchange for
   prototype-accurate filter shapes. The §4 output response is a separately fitted biquad at 44.1/48 kHz (not a
   by-product of the resampler); 40–60 kHz rates use the nearest fit, < 40 kHz is flat. The outer resampler is
   power-complementary (flat round trip to Nyquist); its image rejection is ~50 dB above 0.62·fs (vs ~80 dB for
   a plain half-band) — acceptable for the saturator's low-order harmonics.
4b. **Follower law** (r2 gives only ±50 % / +100 % points): own taper k_f = 4.26 for the follower amount, soft
   positive limit 1.82 oct, mild negative expansion; follower → Res/Amp keep the octave/5.17 scaling.
5. **Modulation scales not given by the spec**: LFO → Res uses 100·1.43·l·T (same LFO factor as the amp
   destination); follower → Res/Amp uses its octave offset / 5.17 (sequencer units); comb delay and delay time
   use the normalised knob-range rule (offset octaves / 9.23). Keyboard tracking enters the Freq sum and is slewed
   by Smooth too; the last note-on stays in effect (no reset on note-off). The LFO → Amp law stays the
   spec's approximate one (r2 notes it under-predicts the reference at small amounts).
6. **Spread on comb**: delay scaled by 2^(∓spread octaves) (inverse of the cutoff shift). Not applied to Amp.
7. **Swing law**: the second step of every pair of the global step grid starts (1 + swing) steps after the
   first (pair length unchanged); with an odd Last the swing follows the global grid.
8. **Transport**: the LFO/sequencer free-run when the transport stops (synced ones at the host tempo, 120 BPM
   without a playhead); positions reset on transport start; synced positions follow ppq while playing.
9. **Delay**: max 8.5 s per channel (1/1 at 30 BPM × 2 would exceed it and is clamped); digital time changes
   start a new 20 ms crossfade whenever the target moves by more than 0.5 samples (with modulation this gives a
   stepped, "digital" character); feedback guard linear to 0 dBFS.
10. **Feedback route** uses a second instance of the chain inside the loop (see Architecture) so the dry/wet
    of the delay still means "filtered signal vs. repeats".
11. **Follower** detects the post-Input-gain signal (or the raw sidechain); if Sidechain is selected but the
    bus is disabled, the input is used.
12. **Bypass**: 5 ms crossfade; at oversampled rates the bypassed signal is the latency-compensated
    (band-limited) input.
13. Editor fixed size 1000×640, not resizable; the step-edit mode is saved with the state (not automatable).

## Status

r2 changes on top of the verified r1 build: BP = s/D, linear Q 90→100 without level-dependent limiting, crush
hold-length law, follower table law, drive ceiling refit, power-complementary outer resampler.
Windows x64 Release build (VST3, CLAP, Standalone) succeeds without warnings in this plug-in's code; all
tests pass under ctest; `ci/package.py` packages `craft-motion-filter-<version>-<platform>.zip`. The editor was
checked with `tools/Snapshot.cpp`. Not yet done: host smoke tests (DAW automation, tempo sync, sidechain
routing) and the independent verification report. Linux: DSP tests built and run with GCC; the plug-in targets
were not built locally (no GUI dev packages on the build host), CI covers them.

## Public references

Zavalishin, *The Art of VA Filter Design* (TPT/ZDF state-variable filter, prewarping); Zölzer (ed.), *DAFX*
(delay lines, comb filters, fractional-delay interpolation, sample-rate reduction, oversampling);
Oppenheim/Schafer (Kaiser-window FIR design); R. Bristow-Johnson, *Audio EQ Cookbook* (bilinear biquads);
standard LFO / step-sequencer / envelope-follower designs. JUCE 8 and CLAP documentation for the plug-in layer.
