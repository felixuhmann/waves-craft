# Independent verification — spec r1 (Craft Hybrid Delay)

Performed 2026-10-29 by the verifier (analyst side), per step 5 of [docs/process.md](../process.md).
Reports observed differences only; no reference-side implementation detail is passed to the builder.

## Method
- Host: REAPER 7.83 (x64, Windows), unattended offline render, 32-bit float, at 44.1, 48 and 96 kHz.
- Build under test: `feat/h-delay` @ 4d5793f, Release VST3 from the local MSVC build (Stereo variant).
- Stimuli (user-generated): 0.5 impulses, exponential sweeps at −30/−12 dBFS, 100 Hz tones at 0.03/0.1/0.3/0.95,
  a 1 kHz tone (modulation and delay-glide demodulation), digital silence.
- The same configuration grid (97 configurations + 9 extra) was rendered through the build and the reference. All
  continuous controls were set by exact host-normalised value using the spec §2 mappings (display read back and
  checked), enums by value, note values by display text. Tempo 120 BPM (60 BPM for the 3500 ms fallback case).
- Compared against the approved r1 model (deterministic core) and against the reference.

## Results against the approved spec r1

| Area | Result | Numbers |
|---|---|---|
| Delay times, ms and note modes, 44.1/48/96 kHz | pass | exact to the sample; 3500 ms fallback (2 bars @60 BPM → 3.0 s) correct |
| Deterministic core (§4–§6): impulses, Feedback 0/60/100/120 %, 1 ms…3500 ms, Mix/Output/Phase | pass | build vs r1 model ≤ −134 dB re peak; per-echo 0.00 dB |
| Ping Pong (§5), both start sides | pass | build vs r1 model ≤ −147 dB; per-echo 0.00 dB |
| Filters HiPass/LoPass (§6), incl. in the loop | pass | ≤ 0.04 dB vs the reference in pass and stop bands |
| LoFi (§7) | pass | ≤ 0.3 dB to −12 dB at 48 kHz; stop band ≤ −51 dB from 9 kHz at all rates |
| Sine fundamental through the saturation (§10) | pass | within 0.2 dB of the §9 "Off" row |
| Analog modes: noise level (Feedback 0), response (±1 dB), fundamentals | pass | noise within 0.3 dB; mode 3 @100 Hz −12.3 dB re Off (table −13.3) |
| Analog "Off" harmonics vs §9 table (±4 dB) | **fail (spec conflict)** | H3 −35.9 / −27.3 / −19.7 dBc at a = 0.1/0.3/0.95 vs table −43/−34/−25; the build follows §4's formula, which itself contradicts the table |
| Modulation amplitude (§8, ±15 %) | **partial** | D 10 ms/Depth 10 %: 0.497/0.465/0.386/0.241 ms at 0.5/5/10/20 Hz (pass); D 200 ms: 7.75 ms (pass); **D 50 ms/Depth 50 %/0.5 Hz: 8.70 ms vs 12.4 ms (−30 %, fail)**; **D ≈1 s/Depth 10 %: 10.6 ms vs ≈12.8 ms (−17 %, fail)** |
| Delay-time glide (§9) | pass | τ = 0.19 s (100 → 200 ms) |
| Latency, dry path | pass | 0 samples; silence in → digital silence out (Analog Off) |

## Differences from the reference that are by design (review decisions §11)
- No extra fractional delay on the wet path: the reference smears each echo over about ±15 samples (side taps ≈3 %);
  the build's echoes are single samples.
- Note rounding: the reference lands one sample early for triplet values (and for all note values at 96 kHz); the build
  is exact.

## Differences from the reference not covered by r1 (fed back to the analyst as r2 candidate notes)
1. Saturation frequency dependence: at −12 dBFS the reference compresses more at high frequencies
   (−1.66 dB @20 Hz → −1.92 dB @≥2 kHz); the build is flat (−1.66 dB). The third harmonic of a 100 Hz tone is ≈6.5 dB
   lower on the reference.
2. Very short delays: the reference attenuates each feedback pass by an extra −1.85 / −1.24 / −0.81 / −0.25 dB at
   1 / 2 / 3 / 5 ms (none at ≥ 10 ms); the build follows r1 (no extra loss).
3. Analog noise with feedback: the reference's noise builds up with feedback (mode 2: −85.5 → −69.2 dBFS at 100 %);
   the build's noise does not (stays at −85.5 dBFS).
4. HiPass/LoPass end ranges: the reference keeps the frequency formula down to r → 0 (20–27 Hz) and up to r → 1
   (14.3–19.4 kHz), Off only at the very ends; the build switches to Off for r < 0.05 / r > 0.95.
5. At 96 kHz the reference's wet path is band-limited to ≈24 kHz; the build is full-band (inaudible).
6. LoFi at 44.1 kHz: reference −30.9 dB @8 kHz vs build −27.2 dB (both within r1 tolerance).
7. Analog mode 3 at 100 Hz (−30 dBFS sweep): reference −8.9 dB re Off in this run vs build −12.3 dB.

## Issues for the builder (against r1)
- Modulation smoothing is too strong for large, slow excursions: D 50 ms / Depth 50 % / 0.5 Hz gives ±8.7 ms
  (target ±12.4 ms, ±15 %); D ≈ 1 s / Depth 10 % / 0.5 Hz gives ±10.6 ms (target ≈ ±12.8 ms). Small-excursion points
  are within tolerance.
- Items 1–4 above will be specified in r2 by the analyst; against r1 only the modulation issue needs a code change.

## Not verified
GUI, Tap, BPM sync mode, Dual mode, Mono→Stereo routing, automation/smoothing of controls other than delay time,
CLAP hosting, Linux build (CI only), stereo LFO phase, 88.2 kHz.
