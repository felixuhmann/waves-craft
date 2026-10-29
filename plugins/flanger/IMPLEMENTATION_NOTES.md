# Implementation notes: Craft Flanger (spec r1)

This is the builder side of a clean-room implementation of [`spec/flanger/r1/SPEC.md`](../../spec/flanger/r1/SPEC.md)
(approved 2026-10-29). I used only that spec, this repository (the conventions and the stereo-imager reference
code), public DSP literature and my own work. I did not access any original product, analyst workspace, vendor
asset or vendor installation.

**Products** (one shared processor; the variant is chosen per CMake target, like the stereo imager):

| Target | Product | Plugin code | CLAP id | I/O (spec §1) |
|---|---|---|---|---|
| `CraftFlanger` | Craft Flanger | `CFlg` | `com.craftaudio.flanger` | Stereo 2→2 |
| `CraftFlangerMono` | Craft Flanger Mono | `CFlm` | `com.craftaudio.flanger.mono` | Mono 1→1 |
| `CraftFlangerMonoStereo` | Craft Flanger Mono-Stereo | `CFms` | `com.craftaudio.flanger.mono-stereo` | Mono→Stereo 1→2 |

- **Manufacturer:** Craft Audio (`Crft`).
- **Version:** 0.1.0.
- **Formats:** VST3, CLAP and Standalone.
- **Platforms:** Windows x64 (MSVC) and Linux x64 (GCC).
- **Licence:** AGPL-3.0, the same as the repository (JUCE is used under its AGPL licence).

## Architecture

All paths below are relative to `plugins/flanger/`.

```
CMakeLists.txt        flanger_dsp (INTERFACE), three plugin targets (flanger_add_variant), optional snapshot tool
source/dsp/           host-independent, header-only DSP core (no JUCE)
  Parameters.h          §2 mappings r -> value with internal quantisation, inverse mappings, snap
  Format.h              §2 display formats (std::string; shared by host text and GUI)
  Lfo.h                 §5 sine / triangle, phase wrap, §5.3 1024-step stereo offset
  FirFilter.h           §6 Kaiser(9, 4) windowed-sinc kernel design (LP normalised, HP = delta - h_raw)
  Link.h                §5.5 Rate x Depth coupling (pure control logic)
  Smoothing.h           one-pole, second-order (two cascaded one-poles), linear ramp
  FlangerEngine.h       complete per-sample processor + closed-form spec quantities (namespace model)
source/plugin/        JUCE AudioProcessor shared by the three variants, parameter layout, Link handling
source/ui/            editor, look-and-feel, value boxes, icon toggles, LFO display, meters (all drawn in code)
tests/                Catch2 tests of the DSP core, registered with CTest as flanger/*
tools/Snapshot.cpp    renders the editors to PNG (-DCRAFT_BUILD_SNAPSHOT=ON, development aid only)
```

`FlangerEngine` runs in double precision. It processes in place:

- **Stereo:** ch0 and ch1 are left and right.
- **Mono:** ch0 only.
- **Mono→Stereo:** input from ch0, output to ch0 and ch1.

The processor reads the quantised parameter values once per block, takes the tempo and transport state from the
play head, and passes everything to the engine.

## Spec mapping

**§1 Variants and latency.**
- The bus layouts are fixed per variant (`isBusesLayoutSupported`).
- Mono runs only the left channel processor.
- Mono→Stereo feeds input 1 to both channel processors and ignores input 2. A test checks this.
- Latency is 0. The engine is linear (tested at +20 dB).

**§2 Parameters.**
- The host's normalised value is the spec's `r`.
- The custom `NormalisableRange` lambdas apply the mapping and the quantisation, so the host and the GUI only see
  step values:
  - Delay: 0.1 ms.
  - Percentages: 1 %.
  - Rate: 0.005 Hz, and values below 0.01 become 0.
  - Gain: 0.1 dB.
  - Stereo: 1°.
  - Filter Freq: see assumption 1.
- The engine re-derives integers from the values:
  - Delay in tenths of a millisecond.
  - Depth in whole percent.
  - `c = (Depth·D)/100` in integer arithmetic, so `0.29·100` cannot truncate to 28.
- Filter Type and Waveform are choice parameters. All other toggles are bool parameters, plus the host bypass.
- Display formats come from `Format.h`: "2.2 ms", "50 %", "0.20 Hz" / "12.5 Hz", "0.0 dB", "90°", "1.0k" / "12k".
- When Tempo Sync is on, the Rate text changes to "1.00 x/Beat". The host string and the GUI follow it through a
  shared flag.

**§3 Signal flow.** The flow is `G_in` at the input, then the delay line `v = x + p_fb·F_loop·w`, then the
linear-interpolated read `w`:

- The feedback is taken from `w` before the FIR.
- The FIR runs on the wet signal only.
- The wet gain is `sqrt(m)·s_wet·(1 − F^(20·Delay_ms)/2)` and the dry gain is `sqrt(1 − m)`.
- `F_loop = min(F, 0.999)`.

**§4 Trajectory.**
- `D = floor(tenths·fs/10000)` gives 105 / 97 / 211 at 48 / 44.1 / 96 kHz.
- Normal mode: `d = D + 0.95·depth·D·L`.
- Tape mode: `d = max(1, c + (c−1)·L)`, and the dry signal is read from its own delay line at `max(1, c)`.
- Reads use `k = floor(d)`, `a = d − k`, `(1−a)·v(n−k) + a·v(n−k−1)`. The buffer is sized for 1.95·D_max at the
  current sample rate (up to 192 kHz).

**§5 LFO.**
- The phase is kept in cycles and is 0 at the first processed sample. The right channel adds
  `floor(Stereo·1024/360)/1024`.
- The triangle follows the sine's phase convention.
- Rate 0 holds the phase. Stop freezes it (see the smoothing notes) and the LFO resumes from there.
- Link (§5.5) works as follows:
  - `RateDepthLink` holds the product Rate × Depth. It is captured when Link is switched on, on state load, or when
    both values change together.
  - A Rate change sets Depth to `product/Rate`, clamped to 0…100 %. A Depth change sets Rate to `product/Depth`,
    clamped to 0…20.
  - The coupling runs on the message thread (`AsyncUpdater`). It therefore applies equally to GUI and host changes:
    host-set Rate is honoured and Depth follows, per review decision 11.2.
  - The DSP always uses the current values.

**§6 Filter.**
- `h_raw[n] = (2fc/fs)·sinc(2fc·n/fs)·Kaiser(9, β = 4)`.
- LP is normalised to unity DC gain. HP is `δ − h_raw` and is not renormalised (−1.96 dB DC at 1 kHz; tested).
- Filter off is the identity, with no added delay.
- The kernel is recomputed only while the frequency or type is moving.

**§7 Smoothing** (review decision 11.1). Settled values are exactly the targets, because every smoother snaps, so
static behaviour is the bit-exact model.
- One-pole smoothers with τ = 15 ms:
  - input gain, dry gain, wet gain;
  - |F_loop|, depth, LFO rate;
  - stereo offset (a phase glide);
  - filter frequency.
- Delay `D` and tape centre `c` use a second-order glide (two one-poles, 10–90 % = 50 ms), so a Delay change moves
  the read position smoothly instead of jumping.
- Toggles use 15 ms linear crossfades:
  - **Wet and Feedback polarity:** the sign ramps through 0.
  - **Tape:** the normal and tape read taps of the same delay line, and the undelayed and delayed dry signals, are
    crossfaded. The fed-back signal is the crossfaded tap.
  - **Filter on/off:** `w` and `FIR(w)` are crossfaded.
  - **Filter type:** the LP and HP kernels are interpolated.
  - **Waveform:** the sine and triangle are morphed.
  - **Stop:** the phase increment ramps to 0 and back.
  - **Host bypass:** crossfade to the unprocessed input.

**§9 Measured details.**
- `prepareToPlay` resets the engine, so θ = 0 at the first processed sample.
- The processor restarts the LFO at every transport start (not playing → playing), independent of the song position.
- Tempo Sync gives `f = Rate·BPM/60`, using the host BPM (120 when the host gives none).

**§12 GUI.** The visuals are original vector drawings in the Craft family style: graphite panels with amber and teal
accents, a header with my own three-sine logo mark, and no images or fonts bundled. The editor is a fixed 970×336
and has four panels:

1. **Mix / Feedback / Filter.**
   - Mix value box with a wet-polarity (+/−) toggle.
   - Feedback value box with a feedback-polarity toggle.
   - Filter row: LP/HP selector with a response sketch, frequency box, power toggle.
2. **Delay / Rate / Depth.**
   - Delay box with a TAPE toggle (reel icon).
   - Rate box with a SYNC toggle (note icon).
   - Depth box.
   - A chain-icon LINK toggle sits between Rate and Depth, with a bracket that lights when Link is on.
3. **LFO.**
   - Sine/Triangle selector with small waveform pictures.
   - STOP toggle.
   - LFO display: one waveform cycle with moving L (amber) and R (teal) markers, plus a sweep bar showing each
     channel's current modulation value. It shows "STOPPED" or "STATIC" when the LFO is held.
   - Stereo value box and a phase dial: the L hand points up, the R hand is rotated by the quantised lead, and an arc
     marks the lead.
4. **Output.**
   - Gain fader (−12…+12 dB) with its value box.
   - L/R (or mono) peak meters from +6 to −48 dB with a 0 dBFS line.
   - Clip LEDs (click to reset) and numeric peak-hold boxes (click to reset; they also clear the clip LED).

Value boxes: vertical drag (Shift for fine), a single click to type (the "k" suffix is accepted), double-click or
Alt-click to reset, and the mouse wheel. The Mono variant hides the Stereo controls. It also has no Stereo
parameter, because Stereo has no effect there.

## Tests

The tests are in `tests/`. They use Catch2 and are registered with CTest under the `flanger/` prefix.

- **§10 impulse-response vectors, every row:**
  - Mix/Feedback/Delay taps.
  - Feedback decay including the 0.999 limit.
  - Wet gain compensation, including the §3 examples 0.5928 / 0.6683 / 0.8208 / 0.9970.
  - Both polarities.
  - Gain ±12 dB.
  - Tape at Depth 0 and Depth 50.
  - LP 1 kHz, HP 1 kHz and HP 10 kHz kernels. These are compared against the table (1e-6, or the spec's stated
    2e-3 for HP) and against the formula (1e-12).
  - LP filtering with feedback: each echo is filtered exactly once.
- **§10 trajectories,** checked per sample against the closed form within 1e-6 samples (the spec allows 0.1):
  - Sine at Depth 100 and Depth 50, including t = 0.25 → 204.75 and t = 0.75 → 5.25.
  - Triangle.
  - Stereo 90° and 30° (29.88°).
  - Tape (105 + 104·sin).
  - Delay 10 at Depth 30 (480 + 136.8·sin).
  - 50 ms reaching 1.95·D at 192 kHz.
  - Tempo Sync (Rate 1 at 120 BPM = 2 Hz).
- **LFO behaviour:**
  - Rate below 0.01 is static.
  - Stop holds the phase at start, freezes while running, and resumes.
  - `restartLfo` returns θ to 0.
- **§8 modulated accuracy:** the engine is compared against an independent straight-line model in `TestHelpers.h`
  over 2 s of noise. That model uses whole-signal arrays, phase computed from `n`, and `std::cyl_bessel_i` for the
  window.
  - It covers 8 settings (feedback ±, tape, triangle, LP/HP, gain, tiny delay, tempo sync) at
    44.1/48/96/192 kHz.
  - Required: ≤ −60 dB, and also ≤ −120 dB.
- **§8 static accuracy:** at 44.1/48/88.2/96/176.4/192 kHz, normal and tape modes must be ≤ −100 dB.
- **Linearity:** a 20 dB level change matches to ≤ −200 dB.
- **Parameters:**
  - Mappings, ranges, defaults and monotonicity.
  - Rate on its 0.005 grid with values below 0.01 → 0.
  - Filter-frequency quantisation.
  - Display formats.
- **Link:** the coupling and its clamping.
- **Smoothing:**
  - After arbitrary changes the response settles to exactly the fresh static response (≤ 1e-12).
  - A Delay change glides (largest read-position step under 1 sample, continuous output).
  - Each toggle (wet/feedback polarity, tape, filter, waveform, stop, mix, gain) is click-free.
- **Engine:**
  - The variants are equivalent (Mono equals left; Mono→Stereo ignores input 2 and output 2 leads).
  - Bypass is bit-exact once faded.
  - Block-size independence, and float/double agreement.

## Questions / assumptions

1. **Filter Freq internal quantisation.** I quantise to the display resolution: 100 Hz steps below 10 kHz and 1 kHz
   steps from 10 kHz. So the low end moves in coarse steps (100, 200, 300 Hz, …).
2. **Minimum read delay in normal mode.** With small D, `0.05·D` drops below 1 sample (for example 0.1 ms gives
   D = 4 and a minimum of 0.2). §3 states `d ≥ 1` always, so I clamp the normal-mode trajectory to at least 1 sample.
   This affects only Delay below 1 sample ÷ 0.05 (D < 20) at high Depth.
3. **Smoothing constants.**
   - τ = 15 ms for continuous values.
   - 50 ms 10–90 % for the read-position glide.
   - 15 ms linear crossfades for toggles.

   §7 asks for 10–50 ms and ≤ 20 ms; the exact laws are my choice. Rate, Stereo, Waveform and Stop act on the LFO
   (a glide or morph of the trajectory) rather than crossfading audio.
4. **Tape toggle.** It crossfades two read taps of the same delay line and feeds back the crossfaded tap. During
   those 15 ms the feedback signal is a blend.
5. **Link edge cases.**
   - If the held product is 0 (Rate or Depth was 0 when Link turned on), nothing is coupled until both are non-zero.
   - Rate 0 sets Depth to 100 %, and Depth 0 sets Rate to 20 Hz (the product is kept internally, so the change is
     reversible).
   - The product is captured from the parameter values; clamping does not change it.
6. **Transport restart.** The LFO restarts on every stopped→playing transition reported by the host, and on
   `prepareToPlay`. It does not restart in the Standalone app, which has no transport.
7. **Tempo Sync.** The host BPM is used; it defaults to 120 when unknown. I did not add beat-division presets (the
   spec makes them optional). Rate keeps its 0.005 grid as cycles per beat.
8. **Three products.** The variants are shipped as three products, which was cheap with the shared processor. The
   stereo product keeps the requested CLAP id `com.craftaudio.flanger`; the other two add `.mono` and `.mono-stereo`.
9. **Host bypass.** I added a host bypass parameter (15 ms crossfade to the dry input); the spec does not list one.
   The Stereo parameter is omitted from the Mono variant.
10. **Meters.**
    - Peak meters from +6 to −48 dB with a ~24 dB/s fall.
    - The clip LED latches above 0 dBFS until clicked.
    - The peak-hold boxes show dB with one decimal, or "-inf".

    These ballistics are my choice. Tail length is reported as 2 s.
11. **Rounding ties** are half away from zero. Display rounding of on-grid values such as Rate 9.995 rounds up, so it
    shows "10.0".

## Public references

- J. Dattorro, "Effect Design, Part 2: Delay-Line Modulation and Chorus", JAES 45(10), 1997: modulated delay lines
  and through-zero flanging.
- U. Zölzer (ed.), *DAFX*, chapter on delay-based effects: flanger/chorus structure and feedback.
- T. Laakso et al., "Splitting the Unit Delay", IEEE SP Magazine 1996: linear fractional-delay interpolation.
- A. Oppenheim & R. Schafer, *Discrete-Time Signal Processing*: windowed-sinc FIR design and the Kaiser window
  (I0 power series).
- JUCE 8 and clap-juce-extensions documentation.

## Status

See the report at the end of this file's commit history: the Windows Release build, the tests and the installation.
