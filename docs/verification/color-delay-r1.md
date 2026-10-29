# Independent verification — colour delay, spec r1

Performed 2026-10-29 by the verifier (analyst side), per step 5 of [docs/process.md](../process.md).
Only observed differences are reported; no reference-side implementation detail is passed to the builder.

## Method
- Host: REAPER 7.83 (x64, Windows), unattended offline renders, 32-bit float. Build under test: `feat/color-delay`
  @ d168ed7, Release VST3 "Craft Colour Delay".
- The reference measurement grids were re-rendered through the build with **identical stimuli and settings**
  (impulses, −40 dBFS and −12/−2 dBFS sweeps, a dynamics programme of tone staircases/steps/bursts/frequency
  steps, and an effects programme of sweeps/staircase/impulse train/tone), ~250 configurations at 48 kHz, plus
  core and effect grids at 44.1 and 96 kHz and host-sync renders at 90/100/120/137/140 BPM for both the build and
  the reference.
- Comparisons: (a) against the spec r1 model/tolerances, (b) against the reference with the same settings.

## Results

| Area | Result | Numbers |
|---|---|---|
| Delay / feedback / filters / dry-wet / gains / monitor (quiet signals, exact class) | **Pass** | Build vs spec model: median −147 dB re peak (36 sweep configs) and ≤ −145 dB (33 impulse configs); vs reference ≤ −91 dB wherever the compressor is inactive |
| Same at 44.1 / 96 kHz | **Pass** | ≤ −170 dB vs model, −109 … −135 dB vs reference (identical to the model's own residual) |
| Compressor static curve (1 kHz staircase, −33 … +12 dBFS) | **Pass** | Within 0.07 dB of the reference everywhere; +6 / +12 dBFS: −20.85 / −23.09 dB vs reference −20.83 / −23.07 dB (the build's high-level easing is correct) |
| Compressor envelope | **Pass** | 10 ms RMS envelope error vs spec model 0.00 dB (0.01 dB with 50 % feedback); vs reference 0.57 dB (1.26 dB with feedback) — same as the spec model's own residual |
| Recovery after −6 → −40 dBFS step | Pass (within r2 tolerance) | +100 ms −10.82 (ref −10.79), +500 ms −4.45 (ref −5.35), +1.2 s −0.91 (ref ≈ 0) dB |
| Compressor 30 Hz | Pass (r2 tolerance) | −13.1 dB vs reference −12.0 dB (window-dependent; spec r1 quoted −10.3) |
| Host-sync delay lengths | **Fail** | 11 of 33 tempo/note cases are 1 sample short: integer-valued note lengths lose a sample (e.g. 100 BPM 1/16T 4799 vs 4800; 100 BPM 1/8D 21599 vs 21600; 120 BPM 1/8T 7999 vs 8000; 1/2T 31999 vs 32000); 137 BPM 1/32T 1751 vs reference 1752 (reference irregular here, see r2) |
| Reverb RT60 per octave | **Pass** | Max deviation from the spec table 8 % (Size 1 %), 2–5 % otherwise |
| Reverb pre-delay, L/R correlation, level law | **Pass** | Pre-delay within 0.2 ms of the reference; |ρ| ≤ 0.04; Level 25/50 %: −36.1/−18.1 dB (ref −37.5/−18.0) |
| Reverb energy | **Fail** | E = Σ(hL²+hR²) for a left impulse: +2.1 / +2.0 / +2.4 / +2.8 / +3.15 dB at Size 1/25/50/75/100 % vs reference −0.7 / −0.1 / +0.5 / +0.7 / +0.8 dB (tolerance ±1.5 dB). Too much energy above 2 kHz: relative to 1 kHz the build is +1.5 … +3.7 dB hotter at 4–16 kHz |
| Distortion small-signal, Send 50 % | **Pass** | Within 0.4 dB of the reference 50 Hz–8 kHz; latency 6.6 ms (ref 6.8) |
| Distortion routing | **Fail** | The build mixes the channels: a left-only input gives identical output on both channels (reference: output on the left only). Centred (L = R) material is driven 6 dB harder than the reference (1 kHz −40 dBFS: +17.7 vs +11.7 dB). Width 100 % leaks a one-sided input into the other channel |
| Distortion Send 75 / 100 % | **Fail** | Small-signal gain at 1 kHz +19.5 / +16.0 dB vs reference +24.4 / +28.7 dB (−4.9 / −12.7 dB); Send 100 % gain-vs-level table 9–13 dB low across −40 … 0 dBFS |
| Distortion Send 25 % / gate | **Fail** | Send 25 %: build silent on a −40 dBFS sweep, reference passes it (−1.7 dB at 1 kHz). Send 10 %: build opens at ≈ −36 dBFS (centred) vs reference between −32 and −28 dBFS, and is ≈ 7 dB louder once open |
| Distortion harmonics | Pass | No H2; H3 at Send 50 % / 0 dBFS −10 dB (ref −11); Send 100 % −7 dB (ref −5 … −9) |
| Distortion Width 100 % | **Fail** | L/R level differs by 3.7 dB (spec: equal level); build correlation 0.00 vs reference 0.60 for an identical 1 kHz tone on both channels |
| Doubler pitch / latency | **Pass** | Within 0.05 cent of Detune (ref within 0.25 cent); onset 5.8–6.6 ms |
| Doubler level | **Fail** | +5.4 dB per channel vs reference +9.25 dB (tolerance ±1 dB) on a 1 kHz tone; one-sided broadband sweep 4.3 dB low as well |
| Effects at 44.1 / 96 kHz | Same as 48 kHz, plus one finding | RT60, pre-delay, level law, distortion Send 50 % response, doubler pitch and phaser rate as at 48 kHz; distortion routing/drive and doubler level fail identically. Reverb energy depends on the sample rate: +2.1 … +3.8 dB at 44.1 kHz, −0.6 … +1.2 dB at 96 kHz (48 kHz: +2.0 … +3.2 dB) |
| Phaser | Pass | 4 notches between 1.9 and 14.6 kHz; LFO rate 1.00 Hz at Rate 1 Hz (other rates not resolvable with this stimulus) |

## Spec revision
The measurements showed that spec r1 was wrong or ambiguous on several points (sync rounding example, high-level
compressor model, LF claim, effect channel routing, missing distortion level data). An approved revision
`spec/color-delay/r2/SPEC.md` supersedes r1; see its "Changes from r1". The failures above are stated against r2.

## Issues for the builder (against r2)
1. Host-sync: compute the note length so that mathematically integer lengths are exact (drop the 0.6666666 factor).
2. Reverb: reduce total energy by ≈ 2–2.5 dB and match the octave-band energy profile (less energy above 2 kHz);
   make the energy independent of the sample rate (currently ≈ 3 dB lower at 96 kHz than at 44.1 kHz).
3. Distortion: process each channel independently (no L+R sum); Width must not mix channels and must keep equal L/R
   level; match the Send 75/100 % small-signal response and gain-vs-level tables; Send 25 % must pass −40 dBFS input;
   gate thresholds per channel as specified.
4. Doubler: raise the level by ≈ 3.9 dB to +9.2 dB per channel.

## Not verified
GUI, automation/parameter smoothing in a host, delay-time change behaviour, HP/LP Link and Delay Link interaction
in a host, Monitor in combination with effects, mono-to-stereo layout, CLAP hosting, the Linux build, 88.2/176.4/192 kHz.
