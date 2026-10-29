# Independent verification — Craft Mastering Limiter (spec mastering-limiter r1 → r2)

Performed 2026-10-29 by the verifier (analyst side), per step 5 of [docs/process.md](../process.md). Only observed
differences are reported; no reference-side implementation detail is passed to the builder.

## Method
- Host: REAPER 7.83 (x64, Windows), unattended offline render, 32-bit float, at 44.1 / 48 / 88.2 / 96 kHz.
- Build under test: `feat/mastering-limiter` @ a2850cd (Release VST3, local MSVC build). Reported latency 64 samples
  at 48 kHz (host PDC); controls set by raw value (the build's host mappings equal spec §2).
- Stimuli (user-generated): DC carrier with clicks/bursts/steps, 1 kHz level staircase, fs/4 inter-sample-peak sines,
  noise programme, digital silence, −60 dBFS sines, LSB ramps, constant DC at fractions of an LSB.
- Same configurations rendered through the reference (identical stimuli and settings); spec model = §4/§5 equations
  and the §7 tables.

## Results
| Area | Configs | Result |
|---|---|---|
| Latency / neutral (impulse) | 4 rates | bit-exact after host PDC (−400 dB) — reported = actual |
| Fixed-release limiter vs §4 model (requantisation off) | 40 at 48 kHz, 4 each at 44.1/96, 1 at 88.2 | worst −141.5 dB re peak (limit −100 / −85) |
| Fixed-release limiter vs reference | same | −89 … −139 dB (reference float32 release state, spec §4 note) |
| Ceiling never exceeded | all | max excess 3.9e-8 (float rounding of the ceiling constant) |
| Adaptive Release §5 table (±25 %, 5 ms t99 ±35 %) | 25 (3 Release settings × 7 rows, 44.1/96 checks) | all pass; worst +14 % (20 ms row t90 144 ms vs 126); Release control has no effect (as specified) |
| Requantisation without dither (§6) | ramps 16–24 bit | exact rounding (0 LSB error) |
| Type 1, no shaping, silence | 16/20/24 bit | 0.500 LSB rms, P(−1/0/+1) = 12.6/74.9/12.5 % |
| Type 1 / Type 2 shaped totals (§7.2, ±10 %) | 48 kHz | T1 0.998 / 1.936 / 3.783; T2 0.606 / 1.147 / 2.241 LSB (Mod/Norm/Ultra) — pass; T2 4.3–4.6 dB below T1 (4.8 ± 1) |
| Silence stays zero (None and Type 2, all shaping modes) | 16/20/24 bit, 44.1/96 kHz | pass |
| NTF §7.3 at 48 / 44.1 kHz | 3 modes each | all points within ±1.5 dB (worst +0.96 dB); in-band integral within −0.79 … +0.05 dB |
| NTF §7.3 at 96 kHz | 3 modes | all points pass except Ultra at 20 kHz: +1.88 dB vs ±1.5 (r1) — passes r2 (±3 dB transition band); the reference measures −1.03 / −3.76 dB at 20 / 22 kHz on the same points |
| 88.2 kHz shaping | 3 modes | r1 required the 96 kHz curves: Ultra at 20 kHz +2.12 dB (r1 fail); the reference itself deviates up to +10.9 dB there → r2 tolerance ±2.5 dB / integral ±1 dB: pass (worst 2.14 dB, integral +0.36 … +0.63 dB) |
| Type 2 on constant non-zero input (D3) | DC 0.25/0.5/3.3 LSB, 4 shaping modes | no idle tones (largest PSD line 5–35 dB above median; the reference shows limit-cycle lines ≥ 160 dB above median — not reproduced, by design). Totals 0.43–0.50 / 0.80 / 1.53–1.55 / 3.07–3.09 LSB (None/Mod/Norm/Ultra): above r1's literal "§7 level tolerance" by +1.9 … +2.6 dB; within r2's bound (≤ Type 1 total of the same mode) |
| Dither None + shaping on constant input | DC, 3 modes | deterministic shaped-rounding patterns, identical to the reference (spec requires only silence → zero for None) |
| Full-scale (fs/4 sine, peaks 1.06) | 24 bit no dither; 16 bit Type 1 Normal | output peaks exactly 1.0 (reference: 1.0 / 0.99924) — matches r2 §6 / D6 |

## Spec revision
Spec r2 (`spec/mastering-limiter/r2/SPEC.md`) supersedes r1; see its §15. It corrects r1 where r1 was unmeasured or
stricter than the reference itself (88.2 kHz curves, 96 kHz transition-band tolerance, Type 2 constant-input level
rule, full-scale rule). Against r2 the build has **no non-conformances**.

## Not verified
Linux and CLAP builds; mono layout; automation / parameter smoothing and the bypass crossfade in a host; 176.4/192 kHz;
the GUI; six late batch-A Adaptive-Release configurations whose render session exited early (their content is covered
by the batch-C Adaptive-Release set).
