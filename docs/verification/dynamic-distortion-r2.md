# Independent verification — dynamic-distortion, spec r2 build

Build under test: `feat/dynamic-distortion` @ 467d32e (Windows Release VST3). Host: REAPER, offline. Same stimuli and raw settings as the reference: 5 §6.1 grids at 44.1, 48 and 96 kHz, plus 10 response and 8 step settings at 48 kHz.

## Result: pass
896 checks, 0 failures against spec r2. Only differences from the reference are listed.

| Area | Difference from reference |
|---|---|
| §6.1 fundamentals | RMS 1.05 dB; largest +3.28 dB (Drive 0 %, Attack 0.1 ms, Release 5 ms, 200 Hz, 0 dBFS; limit ±3.5) |
| Envelope steps | Largest 1.12 dB (limit 2.5) |
| EQ, LP, HP | Largest 2.97 dB, in the LP stop band (limit ±3) |
| Steady 1 kHz at −12 dBFS, Drive 0 % | Build 0.91 dB lower: the reference's ceiling is softer than a hard limit |
| Aliasing, 48 kHz | −46.2 dBc (limit −40; reference −10.5) |

## Implementation departures
- 2 ms detector hold: accepted. The spec does not forbid it, and all tables pass.
- Clip level c = 0.307: accepted, within 0.304 ± 0.01.
- Expansion 6.66 dB at −14 dBFS and flat above: accepted. Within ±0.4 of 7.05 (margin 0.01 dB) and monotonic as §4.3 requires. The +0.05 dB onset at −40 dBFS is within tolerance.

## Spec note
The informative §8 model's 5.86 dB cap misses normative §4.3 at −16 and −14 dBFS (by up to 1.19 dB). §4.3 governs and §8 already allows using the table directly, so no new revision is needed.

## Not verified
Aliasing at 44.1 and 96 kHz, Linux and CLAP builds, automation in a host, GUI, mono layout, 88.2 kHz and above. A reduced set of settings was used: level laws, Drive rows other than 0/50/100 %, and part of the EQ set were not re-rendered.
