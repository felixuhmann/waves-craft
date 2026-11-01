# Independent verification — colour delay, spec r2 build

Performed 2026-10-29 by the verifier (analyst side), per step 5 of [docs/process.md](../process.md).
Only observed differences are reported; no reference-side implementation detail is passed to the builder.

## Method
- Host: REAPER 7.83 (x64, Windows), unattended offline renders, 32-bit float. Build under test: `feat/color-delay`
  @ 797e0ce (spec r2 implementation + overload bound), Release VST3.
- Same stimuli and settings as the reference measurements: effects programme (quiet sweeps, 1 kHz staircase
  −40 … 0 dBFS, impulse train, tone) at 44.1 / 48 / 96 kHz; impulses at 90/100/120/137/140 BPM for host sync;
  a full-scale 100 Hz square at Feedback 100 % (build and reference); regression grids for the core path and dynamics.

## Results (against spec r2, and r3 where noted)

| Area | Result | Numbers |
|---|---|---|
| Host-sync lengths | **Pass** | 40/40 tempo/note cases equal the exact rational floor |
| Reverb energy | **Pass** | E −1.2 … +0.2 dB (48 kHz), −1.0 … +0.5 dB (44.1 / 96 kHz) vs reference −0.7 … +0.8 dB |
| Reverb octave profile | **Pass** | Within 1.1 dB of the reference profile at every octave and size; sample-rate independent (≤ 0.3 dB) |
| Distortion per-channel | **Pass** | One-sided input produces no output on the other channel |
| Distortion Width 100 % | **Pass** | Equal L/R level (0.00 dB), tone correlation 0.54 (reference 0.60), no cross-feed |
| Distortion Send 50 / 75 / 100 % small-signal | **Pass** | Within 0.6 / 0.4 / 0.3 dB of the reference 100 Hz–4 kHz |
| Distortion gain-vs-level, Send 50 / 100 % | **Pass** | Within 1.2 / 0.1 dB of the reference tables |
| Distortion gate, Send 10 % | **Pass** | Opens between −32 and −28 dBFS, as specified |
| Doubler level at −20 dBFS tone (r2 vector) | Pass (r2) | +9.40 dB vs +9.25 dB |
| **Effect routing (r3)** | **Fail** | The build feeds the colour effects before the compressor's gain reduction. Reference effect outputs follow the compressor exactly; the build's do not (see below) |
| Doubler level vs input (r3) | **Fail** | Build +9.40 dB at every level; reference end-to-end +13.0 / +12.9 / +11.0 / +9.3 / +4.1 / +1.2 / −4.5 dB at −40 / −32 / −24 / −20 / −12 / −8 / 0 dBFS; quiet sweep +8.7 dB vs +12.85 dB |
| Distortion Send 10 / 25 % gain-vs-level (r3) | **Fail** | Build flat (Send 10 %: −13.6 dB from −28 to −4 dBFS; Send 25 %: −1.7 dB down to −16 dBFS); reference falls by the compressor's reduction (Send 10 %: −10.7 → −27.4 dB; Send 25 %: −1.8 → −19.3 dB) — up to 12.5 / 6.5 dB difference at 0 dBFS |
| Distortion Send 25 % small-signal shape (r3) | **Fail** | 100 Hz −0.8 vs −5.7, 2 kHz −6.5 vs −10.8, 4 kHz −16.0 vs −23.7 dB (1 kHz matches: −1.7) |
| Overload at Feedback 100 % | Pass (r3 §7.5) | Build bounded (output ≤ +1.9 dBFS wet, dry passes at +17.9 dBFS with +18 dB input). The reference hard-clips wet path and output at 0 dBFS; not reproduced by decision |
| Regression: core path, compressor | **Pass** | Quiet-sweep grid vs spec model median −146.7 dB (unchanged); compressor static gains within 0.10 dB of the reference (incl. +12 dBFS and 50 % feedback) |

## Routing adjudication
Measured on the reference with the 1 kHz staircase: every colour effect's output level tracks the compressor's gain
reduction exactly (doubler: +13.0 dB below threshold, +9.25 dB at −20 dBFS where the compressor reduces 3.75 dB,
−4.5 dB at 0 dBFS where it reduces 17.6 dB; distortion Send 10/25 %: the end-to-end gain falls by exactly the
compressor's reduction). The effects are therefore fed **after** the compressor. The r1 doubler shortfall was a level
error (its own gain is +13.0 dB, not +9.2 dB) — r1/r2 quoted the −20 dBFS end-to-end figure. Moving the sends before
the compressor is not correct. Spec r3 (`spec/color-delay/r3/SPEC.md`) states the routing explicitly, corrects the
doubler gain, declares the distortion tables end-to-end and adds Send 10/25 % tables and an overload clause.

## Issues for the builder (against r3)
1. Feed all four colour effects from the wet signal **after** the compressor's gain reduction and the wet gain.
2. Doubler: gain +13.0 dB re its (post-compressor) input.
3. Distortion: refit the drive so the end-to-end tables (which include the compressor reduction) are met with the
   post-compressor routing — Send 10/25/50/75/100 % — and the Send 25 % small-signal shape.
4. Keep the overload bound (r3 §7.5 accepts it).

## Not verified
GUI, automation/parameter smoothing in a host, delay-time change behaviour, link controls in a host, phaser beyond
1 Hz rate, mono-to-stereo layout, CLAP hosting, the Linux build, rates above 96 kHz.
