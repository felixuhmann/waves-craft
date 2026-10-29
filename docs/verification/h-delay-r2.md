# Independent verification — hybrid delay against spec r2 (build c914d75)

Performed 2026-10-29 by the verifier (analyst side), step 5 of [docs/process.md](../process.md). Only observed
differences are reported; no reference implementation detail is passed to the builder. Supersedes the open items
of [h-delay-r1.md](h-delay-r1.md). Result: the build meets every r2 table; two issues remain, and one r2 statement
was wrong, which led to spec r3 ([spec/h-delay/r3](../../spec/h-delay/r3/SPEC.md)).

## Method
REAPER 7.83 (x64, Windows), unattended offline renders, 32-bit float. Build `feat/h-delay` @ c914d75 (Release VST3).
Reference renders from the r1 verification were reused, plus new reference renders for this pass with identical
settings and stimuli. Controls set by exact raw values (both share the spec §2 mappings). 48 kHz unless stated;
core grid also at 44.1 and 96 kHz. About 150 configurations.

## Results against spec r2

| Area | Result | Numbers |
|---|---|---|
| Delay times, note values | Pass | exact at 44.1/48/96 kHz |
| Core echoes vs §4 model (Off, impulses ≤ 0.5, Feedback 0–120 %) | Pass | build vs model ≤ −140 dB re peak |
| §4.2 compression vs frequency | Pass | worst −0.09 dB (20 Hz, −12 dBFS); tolerance ±0.15 |
| §4.3 100 Hz tone, Off | Pass | fundamental within 0.2 dB; H3 −53.2/−43.2/−34.9/−28.1 dBc vs −52.9/−42.6/−33.5/−24.7 (±2.5/±4) |
| §4.4 short-delay per-pass ratios | Pass | worst +0.23 dB (1 ms, pass 3→4); tolerance ±0.3 |
| Ping Pong, Mix/Output/Phase | Pass | build vs model ≤ −141 dB |
| HiPass/LoPass (437 Hz, 1240 Hz) | Pass | within 0.02 dB of the reference |
| HiPass near the bottom end (r = 0.01, 0.03) | **Fail (minor)** | 0.3 dB more attenuation than the reference at 15 Hz, 0.17 dB at 20 Hz (±0.1 dB) — corner ≈ 2–3 % high |
| LoPass end points (r = 0.97, 0.99, 1.0) | Pass | within 0.05 dB |
| LoFi | Pass | stopband ≤ −52 dB from 9 kHz; at 44.1 kHz 8 kHz reads −27.2 dB vs reference −30.9 (outside the ±1 dB zone, informative) |
| §8 modulation table (12 rows) | Pass | worst −12 % (10 ms/20 Hz); D 50 ms/Depth 50 %: 11.66 vs 12.4 ms (−6 %); D 1024 ms/Depth 10 %: 12.25 vs 12.8 ms (−4 %) |
| §9.1 delay glide | Pass | τ 0.19 s (reference 0.19 s) |
| §9.2 mode responses | Pass | all within ±1 dB (mode 3 sweep row within ±3 dB) |
| §9.2 distortion table | Pass | fundamentals within 0.3 dB, harmonics within ±4 dB |
| §9.3 noise vs feedback (modes 1–4, 0–100 %) | Pass | worst −0.8 dB (mode 2, 100 %); mode 2 100 %/D 10 ms −67.3 vs −62.3 dBFS (informative row) |
| Clicks through Analog modes (new in r3 §9.2.1) | **Fail** | mode 1: click 0.5/Feedback 60 % echo 1 is +3.6 dB above the reference (+2.2, +1.3, +0.8, +0.4, +0.3 for echoes 2–6); mode 4: click 0.3/Feedback 80 % echo 1 +1.8 dB; mode 2 −0.5…−1.0 dB; mode 3 within 0.3 dB |
| Wet-path bandwidth at 96 kHz | Pass (measured to 20 kHz) | flat within 0.02 dB; the reference's 24 kHz band limit is not reproduced by design |

## Analog colouration placement (r2 ambiguity) — verdict
The reference applies colouration and distortion **once** to the wet output: the mode/Off ratio of click echoes is
constant from echo 1 to echo 8 in every mode (e.g. 0.4–1.5 kHz and 1.5–6 kHz band ratios constant within 0.3 dB;
broadband echo ratios in §9.2.1 of r3). Only the noise accumulates in the loop. The build's choice (chain once on
the wet output, noise in the loop) is correct; spec r3 §9.2 now says so.

## By-design differences
- Click-echo peaks differ from the reference by up to ±0.5 dB after several passes at high feedback because the
  reference smears echoes with its fractional offset (r2 §11.1, not reproduced). The build matches the §4 model.
- At 96 kHz the reference's first echo is one sample early and band-limited (r2 §11.1, §11.5).

## Remaining issues for the builder (spec r3)
1. Analog modes 1 and 4 raise transient level: meet r3 §9.2.1 (click echo ratios vs Off ±0.5 dB; no mode above Off).
   Steady-tone tables must stay met.
2. HiPass near r = 0: corner ≈ 2–3 % high (0.3 dB at 15 Hz with r = 0.01); the formula `19·1058^r` must hold exactly.

## Not verified
GUI, Tap, BPM-sync mode, Dual mode, Mono→Stereo routing, automation other than delay time, CLAP hosting, Linux build,
stereo LFO phase, 88.2 kHz, wet-path response above 20 kHz at 96 kHz.
