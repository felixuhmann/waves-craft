# Independent verification — dynamic-distortion, spec r1 build

Build under test: `feat/dynamic-distortion` @ f31f8cf (Windows Release VST3). Host: REAPER, offline, 32-bit float.
Method: the same stimuli and raw parameter settings through build and reference (113 settings: 48 kHz full grid, 5 settings each at 44.1 and 96 kHz); 1035 spec checks. The harness run on the reference's own data fails only the aliasing check, which is excluded by review decision D1. Only differences are reported.

## Result: spec r1 was wrong on the level convention; build must move to spec r2
Spec r1 labelled input tone levels as RMS, but they are sine peak levels. A build following r1 literally must differ from the reference.

| Area | Result |
|---|---|
| Level laws, Drive table, small-signal gain | Pass (within 0.003 dB of the reference) |
| EQ, HP | Pass |
| Latency | Pass; direct path aligned exactly. Distortion-path impulse peak 1 sample later than the reference (not normative) |
| Aliasing (15 kHz, −6 dB peak, Drive 50 %) | Pass: −52.3 dBc (limit −40; reference −10.5) |
| Channels | Pass: no crosstalk (only FIR pre-ringing before a segment onset) |
| Envelope steps | Pass: within ±2.5 dB at every normative point |
| Clipped output (§6.1, peak levels) | Fail: +3.0 dB vs reference in all clipped Drive 50/100 cells, +2.6…+3.0 dB at Drive 0 / 0 dBFS (500 Hz–2 kHz); 30 fundamental and 7 h3 failures per sample rate (37 of 37 repeat at 44.1 and 96 kHz) |
| Expansion (§4.3, peak levels) | Fail: −0.66 dB at −30, −1.18 dB at −20 (±0.4) |
| LP at 21 kHz | Fail: build −5.9 dB at 20 kHz; reference flat. All 14 LP table failures follow from this reference level |

## Adjudication of the flagged spec conflicts
1. Flat clip vs §4.3/§6.1: caused by the level mislabel. Plain hard clip at c = 0.304 is correct; the emphasis pair is unnecessary.
2. 2 ms detector hold: not needed.
3. h3 in (−50, −40] dBc: r2 adds a rule.
4. Release 3000 preconditioning: rising-step columns are programme-dependent; informative in r2.
5. LP at 20 kHz: the two-section form is correct; deep table values get ±3 dB in r2. The LP is off at its maximum.
6. Drive 0 % / Attack 100 ms cells: the conflict disappears with peak levels. The reference's ceiling is softer than a hard limit there; r2 gives these cells ≤ −20 dBc.

A literal implementation of the r2 text passes all 296 §6 checks and the added h3 and LP checks.

## Not verified
Linux and CLAP builds, automation/smoothing in a host, GUI, mono layout, 88.2 kHz and above.
