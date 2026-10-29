# Hybrid delay — behavioural specification r3

Status: APPROVED r3 (2026-10-29). Supersedes r2. Review gate applied with standing user decisions (§11).
Frozen — changes require a new revision.

Derived only from black-box measurement of the licensed reference processor in a VST3 host (user-generated
impulses, sweeps, tones, click trains, ramps, silence), host-visible parameter metadata, the vendor's user guide
(paraphrased), and verification renders of the r1 and r2 implementations against the reference. No code, disassembly or
vendor assets used or included. Product naming, parameter IDs and GUI are the implementer's own.

## Changes from r2

| § | Change | Reason |
|---|---|---|
| 9.2 | Analog colouration **and** distortion act **once on the wet output** (after the loop), not on every repeat. Noise stays inside the loop (§9.3 unchanged). | Click with Feedback 80 %: the reference's mode/Off echo ratio stays constant from echo 1 to echo 8 (mode 1 −0.34 → −0.22 dB, mode 2 −0.92 → −0.57 dB, mode 3 −0.42 → −0.22 dB, mode 4 −0.02 dB; 0.4–16 kHz band ratios constant within 0.3 dB). In-loop colouration would compound (mode 2's −1.5 dB above 2 kHz would reach ≈ −12 dB by echo 8). r2 §9.2 contradicted §9.3's +16.7 dB noise rise. |
| 9.2.1 | New transient (click) test vectors per mode. | The r2 build met every steady-tone table but gave clicks +3.6 dB (mode 1) / +1.8 dB (mode 4) too loud; steady tones alone under-specify level-dependent modes. |
| 8 | Modulation note reworded: the table is normative; the reference is better described by a smoothing low-pass whose time constant grows with the delay time than by a delay-slope limit. | A single slope limit cannot meet both the D 50 ms/Depth 50 % row and the D 200 ms row. |
| 10 | Tolerance row for §9.2.1 added; per-echo core tolerance clarified as relative to the §4 model. | The reference's fractional-offset smear (§11.1, not reproduced) moves its click-echo peaks by up to ±0.5 dB after several passes. |

## Changes from r1

| § | Change | Reason |
|---|---|---|
| 2 | HiPass / LoPass end points corrected: the frequency formula holds over the whole range; Off only at the extreme position (HiPass r = 0, LoPass r = 1). | r1 range wording ("≈27 Hz…", "…14.2 kHz") was misleading; measured displays 20–27 Hz for r 0.005–0.049 and 14.3–19.4 kHz for r 0.951–0.995. |
| 4 | Memoryless saturation `v/(1+|v|)` replaced by an envelope-controlled saturation (functional form + target tables). | r1 formula matched the fundamental at 100 Hz but gave a 3rd harmonic ≈6.5 dB too high and no frequency dependence; it contradicted r1's own §9 "Off" row. |
| 4 | Short-delay per-pass loss added (consequence of the new saturation; table for 1/2/3/5/10 ms). | Measured extra loss −1.85…−0.25 dB/pass below 10 ms. |
| 8 | Modulation targets restated as explicit test vectors, incl. large/slow excursions. | Two r1 points were missed by the r1 build (D 50 ms/Depth 50 %; D ≈ 1 s/Depth 10 %). |
| 9 | Analog noise is injected inside the feedback loop; noise-vs-feedback table added. | Measured noise build-up with feedback (resolves r1 open question 3). |
| 9 | Distortion table re-measured with stationary 4 s 100 Hz tones (a = 0.03 column added; mode 2 H2 at a = 0.1 now −60 dBc instead of −51). | Stationary stimulus, consistent with the verification renders. |
| 9 | Analog mode 3 response row replaced by re-measured values with the stimulus stated, plus a stationary-tone table; widened tolerance at 50–500 Hz. | r1 value at 100 Hz (−13.3 dB) not reproduced (−8.9 dB measured); mode 3 is level/stimulus dependent. |
| 3, 9.3 | High sample rates: wet path full-band (review decision); LoFi at 44.1 kHz noted. | Reference band-limits the wet path to ≈24 kHz at 96 kHz — not reproduced. |
| 10–12 | Tolerances, review decisions and open questions updated. | — |

## 1. Scope and variants

| Variant | I/O | Notes |
|---|---|---|
| **Stereo** (required) | 2 in → 2 out | everything below; measured |
| **Mono** (should) | 1 in → 1 out | single delay line; no Ping Pong, one Phase control (not measured) |
| **Mono→Stereo** (should) | 1 in → 2 out | input feeds both lines; Ping Pong available; Phase L/R linked (not measured) |

Latency: 0 samples. Dry path is bit-transparent (Mix 0 → output equals input × Output gain).

## 2. Parameters

`r` = host-normalised value 0…1.

| Parameter | Range / mapping | Default |
|---|---|---|
| Delay (ms mode) | 1…3500 ms. Display taper: r 0–0.5 linear 1→501 ms (`1 + 1000 r`); upper half non-linear: r .55→636, .60→815, .65→1024, .70→1265, .75→1540, .80→1852, .85→2201, .90→2591, .95→3023, 1.0→3500 ms (any smooth monotonic taper through these points is acceptable) | 375 ms |
| Delay (note mode) | 20 discrete values, in order: 1/64T, 1/64, 1/64D, 1/32T, 1/32, 1/32D, 1/16T, 1/16, 1/16D, 1/8T, 1/8, 1/8D, 1/4T, 1/4, 1/4D, 1/2T, 1/2, 1/2D, 1 bar, 2 bars (T = ×2/3, D = ×1.5; 1/4 = one beat) | 1/8D |
| Delay R (ms / note) | as above; used only in Dual mode, otherwise follows Delay | = Delay |
| Dual | off / on (separate L and R delay times; all other controls shared) | off |
| Sync | BPM (manual tempo) / Host (host tempo) / ms | Host |
| BPM (manual tempo) | 30…300, linear (`30 + 270 r`) | 120 |
| Feedback | 0…200 %, linear (`200 r`) | 60 % |
| Ping Pong | off / on | off |
| Ping Pong start | Left / Right | Left |
| Phase L, Phase R | + / − (invert that channel's wet signal) | + |
| Mix (Dry/Wet) | 0…100, linear | 100 |
| Output | −18…+18 dB, linear in dB | 0 dB |
| Analog | Off, 1, 2, 3, 4 | 2 |
| LoFi | off / on | off |
| Mod Rate | 0.1…6000 Hz, log (`0.1 · 60000^r`) | 0.1 Hz |
| Mod Depth | 0…100 %, linear | 0 |
| HiPass | **Off at r = 0 exactly**; for every r > 0 the frequency is `19 · 1058^r` Hz (≈19.7 Hz at r = 0.005, 27 Hz at r = 0.05, 20.1 kHz at r = 1) | Off |
| LoPass | `19 · 1058^r` Hz for every r < 1 (19 Hz at r = 0, 14.2 kHz at r = 0.95, ≈19.4 kHz at r = 0.995); **Off at r = 1 exactly** | Off |
| Filter Link | GUI control moving both filter frequencies by the same ratio | — |
| Tap | GUI: delay = interval of the last two taps; quantised to nearest note value in Host sync; sets BPM in BPM sync | — |

Display formats: "375 ms"; note names as listed; Feedback "60"; Mix "100"; Output "0.0 dB"; Rate "0.10 Hz", "1.15k Hz";
Depth "0 %"; filters "437Hz", "1.24kHz", "Off".

## 3. Delay time

- ms mode: delay `D = t_ms · fs / 1000` samples, rounded to the nearest sample (review decision §11.2).
- Note modes: `t = beats(note) · 60 / tempo`. If `t > 3500 ms`, use the **longest note value in the list whose
  duration ≤ 3500 ms** (e.g. at 60 BPM, "1 bar" and "2 bars" both give 3.0 s = 1/2D). Host mode uses the host tempo.
- Delay is exact in milliseconds at every sample rate (verified 44.1/48/96 kHz).
- The wet path is full-band at every sample rate (review decision §11.5).

## 4. Signal flow (normal stereo mode, per channel c ∈ {L, R}, Analog Off, LoFi off, Depth 0)

```
v[n]   = x_c[n] + g · w[n − D − 1]            g = Feedback/100   (feedback loop length is D + 1 samples)
w[n]   = HP( LP( S(v)[n] ) )                   filters only when enabled (§6)
wet[n] = w[n − D]                              (first echo at exactly D samples)
```

### 4.1 Saturation S (in the loop; each repeat is re-saturated)

The saturation has memory: its gain depends on a peak envelope of its own input. A functional form that
reproduces every measurement below within tolerance (recommended):

```
a[n] = |v[n]|
e[n] = a[n]                                  if a[n] > e[n−1]      (instant attack)
     = e[n−1]·ρ + a[n]·(1 − ρ)                otherwise             (release, ρ = exp(−1/(τ·fs)), τ = 9.7 ms)
S(v)[n] = v[n] / (1 + 0.72·e[n] + 0.28·a[n])
```

Any implementation is acceptable if it meets the target tables (§4.2–§4.4) within §10's tolerances. Consequences:
a single impulse is saturated exactly like `v/(1+|v|)` (instant attack), low frequencies behave nearly memoryless,
higher frequencies are compressed on the envelope (less 3rd harmonic), and closely spaced repeats (short delays)
are compressed by the previous repeat's envelope.

Example (impulse 0.5 at 100 ms, Feedback 60 %): echoes ≈ 0.333, 0.166, 0.090, 0.051 … at 4800, 9601, 14402,
19203 samples (48 kHz). Feedback > 100 % grows until the saturation bounds it.

### 4.2 Target: compression vs frequency (Analog Off, Feedback 0, steady sine, gain of the fundamental re input)

| Level | 20 Hz | 50 Hz | 100 Hz | 200 Hz | 500 Hz | 1 kHz | 2 kHz | 5 kHz |
|---|---|---|---|---|---|---|---|---|
| −30 dBFS (a = 0.0316) | −0.23 | −0.24 | −0.24 | −0.25 | −0.26 | −0.27 | −0.27 | −0.27 |
| −12 dBFS (a = 0.251) | −1.66 | −1.71 | −1.74 | −1.82 | −1.88 | −1.91 | −1.92 | −1.92 |

(The recommended form gives −1.76 … −1.87 dB at −12 dBFS and −0.24 … −0.26 dB at −30 dBFS.)

### 4.3 Target: 100 Hz tone (Analog Off, Feedback 0)

| Amplitude a | 0.03 | 0.1 | 0.3 | 0.95 |
|---|---|---|---|---|
| Fundamental re input (dB) | −0.2 | −0.8 | −2.1 | −5.3 |
| 3rd harmonic (dBc) | −52.9 | −42.6 | −33.5 | −24.7 |
| 2nd harmonic | none (≤ −150 dBc) | | | |

### 4.4 Target: per-pass echo ratio at short delays (impulse 0.5, Feedback 60 %, Analog Off; ratio of echo k+1 to echo k peak)

| Delay | pass 1→2 | 2→3 | 3→4 | 4→5 | extra loss vs. memoryless `v/(1+|v|)` |
|---|---|---|---|---|---|
| 1 ms | 0.419 | 0.437 | 0.452 | 0.467 | −1.85 dB/pass |
| 2 ms | 0.436 | 0.466 | 0.491 | 0.511 | −1.24 dB/pass |
| 3 ms | 0.452 | 0.490 | 0.519 | 0.540 | −0.81 dB/pass |
| 5 ms | 0.479 | 0.526 | 0.555 | 0.573 | −0.25 dB/pass |
| ≥ 10 ms | 0.500 | 0.546 | 0.569 | 0.582 | 0 |

Identical at 44.1 and 48 kHz (time-based).

### 4.5 Output stage
- Phase L / Phase R negate that channel's wet signal.
- `y = G · ( cos(m·π/2) · x + sin(m·π/2) · wet )`, m = Mix/100 (equal-power), G = 10^(Output/20). Mix and Output act
  after the saturation and are effectively immediate (complete within ~5 ms; no audible ramp).

## 5. Ping Pong (stereo variant)

Input to the delay network is the mono sum `xm = (L + R)/√2`. Two lines L and R (each as in §4: S, filters,
loop length D + 1):

```
wL[n] = F( S_L( injL·xm[n] + g² · wR[n − D − 1] ) )
wR[n] = F( S_R( injR·xm[n] + 1  · wL[n − D − 1] ) )
wetL[n] = wL[n − D],  wetR[n] = wR[n − D]
```

`injL = 1, injR = 0` for start Left; the reverse for start Right. The hop gains are fixed and asymmetric
(L→R unity, R→L g²) — observed for both start directions and several feedback values; reproduce them
(review decision §11.3). Echoes alternate sides every D + 1 samples. Each line has its own saturation envelope.

Example (impulse 0.5 on L, 100 ms, Feedback 60 %, start Left; amplitudes): L 0.261 @4800, R 0.206 @9601,
L 0.068 @14402, R 0.063 @19203, L 0.022 @24004. Start Right: R 0.261 @4800, L 0.085 @9601, R 0.078 @14402,
L 0.027 @19203.

## 6. Filters (in the loop, after the saturation)

RBJ-cookbook biquads, digital corner `w0 = 2 · atan(π · f / fs)` (bilinear without pre-warping — the effective
corner is `(fs/π)·atan(π f/fs)`, e.g. 14.2 kHz → 11.44 kHz at 48 kHz), applied every pass of the loop:
- HiPass: 2nd-order high-pass, **Q = 0.80**.
- LoPass: 2nd-order low-pass, **Q = 0.95**.
Fit residual 0.000 dB over 15 Hz–20 kHz at 44.1/48/96 kHz. Frequencies per §2 (formula valid to the end points).

## 7. LoFi (in the loop)

Fixed low-pass, same response in Hz at 48 and 96 kHz, magnitude relative to HiFi:

| f | ≤ 4 kHz | 5 kHz | 6 kHz | 7 kHz | 8 kHz | 9 kHz | ≥ 10 kHz |
|---|---|---|---|---|---|---|---|
| dB | 0 (±0.05) | −1.0 | −4.4 | −12 | −27 | −51 | ≤ −55 |

At 44.1 kHz the reference measures −30.9 dB at 8 kHz and −55.8 dB at 9 kHz (otherwise as above); the 48 kHz table
is the target at all rates. Weak non-harmonic images (≤ −59 dBc for a 7 kHz tone) exist on the reference; not required.

## 8. Modulation (delay-time LFO)

- Triangle LFO (both channels in phase — assumption, R not measured), rate as set (verified 0.1–2 Hz within 0.2 %).
- Nominal excursion `A = (Depth/100) · D / 2` (instantaneous delay `D(t) = D · (1 + (Depth/100) · tri(t) / 2)`).
- The modulation signal is smoothed before it reaches the delay line: fast rates and long delays are attenuated
  (consistent with a limit on the rate of delay change of roughly 35–40 ms/s), while slow, moderate excursions
  pass at full amplitude. The behaviour is well described by a smoothing low-pass on the modulation signal whose
  time constant grows with the delay time (e.g. a one-pole with τ ≈ D followed by a gentle fixed low-pass); a single
  limit on the delay slope does **not** fit all rows. The table is normative — implement any smoothing that meets
  these test vectors (half peak-to-peak delay excursion of a 1 kHz tone, ±15 %):

| D | Depth | Rate | Target excursion (± ms) | Ratio to nominal A |
|---|---|---|---|---|
| 10 ms | 10 % | 0.5 Hz | 0.51 | 1.02 |
| 10 ms | 10 % | 2 Hz | 0.51 | 1.02 |
| 10 ms | 10 % | 5 Hz | 0.45 | 0.90 |
| 10 ms | 10 % | 10 Hz | 0.36 | 0.72 |
| 10 ms | 10 % | 20 Hz | 0.25 | 0.50 |
| 10 ms | 10 % | 50 Hz | 0.015 | 0.03 |
| **50 ms** | **50 %** | **0.5 Hz** | **12.4** | **0.99** |
| 100 ms | 10 % | 0.5 Hz | 4.75 | 0.95 |
| 200 ms | 10 % | 0.5 Hz | 7.3 | 0.73 |
| 500 ms | 10 % | 0.5 Hz | 10.8 | 0.43 |
| **≈1 s (1000–1024 ms)** | **10 %** | **0.5 Hz** | **12.8** | **0.25** |
| 3500 ms | 10 % | 0.5 Hz | 12.3 | 0.07 |

  Note the large-excursion rows: a slow LFO with a 25 ms/s maximum delay slope (D 50 ms, Depth 50 %, 0.5 Hz) must
  pass essentially unattenuated, while the long-delay rows saturate around 12–13 ms.
- Fractional delay must be interpolated (band-limited interpolation recommended).

## 9. Delay-time changes ("tape" behaviour) and Analog modes

### 9.1 Delay-time changes
The read delay follows a one-pole approach to the new value with τ ≈ 0.2 s (63 % after 0.2 s, 92 % after 0.5 s),
producing a temporary pitch shift (e.g. 100→200 ms: output pitch drops to ~0.55× and recovers). At the start of a
change the wet output dips briefly (~50 ms when increasing, ~90 ms when decreasing). Tolerance: τ 0.15–0.25 s; dip
optional but must be click-free.

### 9.2 Analog modes (colouration and distortion once on the wet output; noise inside the loop)
Off = §4 exactly. Modes add colouration, distortion and noise; no wow/flutter (timing jitter < 0.1 µs).
Colouration and distortion are applied **once to the wet output** (after the feedback loop): every repeat receives
the same colouration, it does not accumulate per pass. The noise of each mode is injected inside the loop (§9.3).

Small-signal magnitude relative to Off (−30 dBFS exponential sine sweep, 10 Hz → 22.8 kHz over 6 s, Feedback 0):

| Mode | 20 Hz | 50 | 100 | 200 | 500 | 1 k | 2 k | 5 k | 10 k | 20 k |
|---|---|---|---|---|---|---|---|---|---|---|
| 1 | −3.4 | −3.0 | −1.7 | −0.8 | −0.4 | −0.3 | −0.3 | −0.3 | −0.4 | −0.5 |
| 2 | −0.7 | −0.7 | −0.7 | −0.7 | −0.8 | −1.0 | −1.4 | −1.5 | −1.7 | −1.7 |
| 3 | −2.1 | −6.3 | −8.9 | −5.8 | −3.6 | −2.1 | −1.1 | −0.6 | −0.5 | −0.5 |
| 4 | −0.4 | −0.3 | −0.4 | −0.5 | −0.5 | −0.5 | 0.0 | −0.2 | −0.2 | 0.0 |

Mode 3 is strongly level-dependent (crossover-like "starved" character), so its response depends on the stimulus. The
normative mode-3 target is the stationary 100 Hz tone table below; the sweep row above is informative (±3 dB at 50–500 Hz).

Distortion of a 100 Hz tone (fundamental gain re input dB / H2 / H3 dBc), Feedback 0:

| Mode | a = 0.03 | a = 0.1 | a = 0.3 | a = 0.95 |
|---|---|---|---|---|
| Off | −0.2 / — / −52.9 | −0.8 / — / −42.6 | −2.1 / — / −33.5 | −5.3 / — / −24.7 |
| 1 | −2.1 / −42 / −44 | −2.2 / −34 / −35.5 | −2.9 / −29 / −30 | −5.5 / −28 / −24 |
| 2 | −0.9 / −70 / −55 | −1.4 / −60 / −45 | −2.5 / −53 / −35 | −5.5 / −50 / −26 |
| 3 | −10.6 / −13.6 / −9.3 | −3.6 / −19.5 / −20.2 | −2.9 / −23.2 / −33.8 | −5.5 / −30.6 / −27.5 |
| 4 | −0.6 / — / −36 | −0.8 / — / −40.6 | −2.1 / — / −33.4 | −5.3 / — / −24.7 |

("—" = H2 below −110 dBc.)

### 9.2.1 Target: clicks (transient level per mode)
Ratio of each echo's peak in mode m to the same echo with Analog Off (same settings, D = 100 ms, 48 kHz), dB:

| Stimulus | Mode | echo 1 | 2 | 3 | 4 | 5 | 6 |
|---|---|---|---|---|---|---|---|
| click 0.3, Feedback 80 % | 1 | −0.34 | −0.29 | −0.27 | −0.24 | −0.22 | −0.22 |
| click 0.3, Feedback 80 % | 2 | −0.92 | −0.79 | −0.71 | −0.66 | −0.61 | −0.57 |
| click 0.3, Feedback 80 % | 3 | −0.42 | −0.35 | −0.30 | −0.27 | −0.24 | −0.22 |
| click 0.3, Feedback 80 % | 4 | −0.02 | −0.02 | −0.02 | −0.01 | −0.02 | −0.02 |
| click 0.5, Feedback 60 % | 1 | −0.28 | −0.24 | −0.21 | −0.21 | −0.21 | −0.21 |
| click 0.5, Feedback 60 % | 3 | −0.29 | −0.23 | −0.21 | −0.17 | −0.15 | −0.16 |

(A click is a single sample of the stated amplitude on the left input; peak = largest absolute sample within ±3
samples of the echo position.) None of the modes may raise transient levels above Off.

### 9.3 Analog noise (inside the loop)
Noise is added to the loop signal every pass (before the saturation), so it accumulates with feedback.
Output noise, silent input, Mix 100, D = 100 ms, RMS dBFS (measured over 6 s after 2 s):

| Mode | Fb 0 | 25 % | 50 % | 75 % | 100 % |
|---|---|---|---|---|---|
| Off | digital silence | | | | |
| 1 | −102.7 | −102.4 | −101.4 | −99.0 | −86.0 |
| 2 | −85.5 | −85.3 | −84.3 | −82.0 | −68.8 |
| 3 | −106.0 | −105.8 | −104.8 | −102.5 | −89.2 |
| 4 | −108.7 | −108.6 | −107.5 | −105.2 | −92.4 |

For g < 1 the rise equals the loop power sum `10·log10(1/(1−g²))` (+0.3 / +1.2 / +3.6 dB at 25/50/75 %). At 100 %
the noise power keeps accumulating per pass (mode 2, 100 %: −62.3 dBFS at D = 10 ms, −75.8 dBFS at D = 500 ms over
the same window). Spectrum concentrated below ~100 Hz (strongest components ≈ 8–12 Hz, 46.6 Hz, 91.8 Hz).

## 10. Tolerances (acceptance)

| Item | Tolerance |
|---|---|
| Delay times (ms and note modes, all rates) | exact to ±1 sample |
| Deterministic core §4–§6 (Analog Off, LoFi off, Depth 0), impulses ≤ 0.5, Feedback ≤ 100 %, D ≥ 10 ms | per-echo peak within ±0.1 dB of the §4 model for the first 6 echoes |
| Click levels per mode §9.2.1 | ±0.5 dB per echo ratio |
| Saturation targets §4.2 | ±0.15 dB |
| Saturation targets §4.3 | fundamental ±0.2 dB; H3 ±2.5 dB (±4 dB at a = 0.95) |
| Short-delay per-pass ratios §4.4 | ±0.3 dB per ratio |
| Filter magnitude §6 | ±0.1 dB |
| Ping Pong echo amplitudes (§5 example) | ±0.2 dB, timing exact |
| LoFi table | ±1 dB down to −30 dB; stopband ≤ −50 dB above 9 kHz |
| Modulation (§8 table) | amplitude ±15 %, rate ±1 % |
| Analog response (§9.2) | ±1 dB; mode 3 sweep row ±3 dB at 50–500 Hz |
| Analog distortion (§9.2) | fundamental ±0.5 dB (mode 3 at a = 0.03: ±1.5 dB); harmonics ±4 dB |
| Analog noise (§9.3) | ±3 dB at every feedback value up to 75 %; at 100 % ±6 dB |
| Wet-path bandwidth at 88.2/96 kHz | full band (no cutoff below 0.45·fs) |
| Smoothing | click-free; delay glide τ 0.15–0.25 s |

## 11. Review decisions (standing user policy: match the reference, do not reproduce clear defects)

1. The reference adds a constant extra fractional delay of ≈ 0.031 samples to the wet path via a long sinc-like
   interpolator (±3 % side taps around each echo). **Not reproduced**; echoes land on exact samples.
2. The reference truncates note-value delays and lands one sample short (triplets; all note values at 96 kHz).
   **Not reproduced**: round to nearest.
3. Ping Pong asymmetric hop gains (§5): **reproduced**.
4. Note values longer than 3500 ms fall back to the longest fitting note (§3): **reproduced**.
5. At 88.2/96 kHz the reference band-limits the wet path to ≈24 kHz (−0.2 dB at 22 kHz, −6 dB at 24 kHz, −34 dB at
   26 kHz). **Not reproduced** (internal half-rate artefact; full-band wet path required).
6. The envelope-controlled saturation (§4.1), the short-delay loss (§4.4) and noise inside the loop (§9.3) are
   **reproduced** (audible character, consistent across all measurements).

## 12. Open questions

1. Exact modulation smoothing law (§8) — only the tabulated points are specified.
2. Whether modulation is identical on L and R; Mono and Mono→Stereo variants unmeasured.
3. Delay-change dip shape (§9.1).
4. Exact noise spectrum shape (§9.3).

## 13. User interface (functional layout — visuals must be original)

Same controls and arrangement as the reference so its users find everything; all artwork, colours, fonts,
names and logos original (vector-drawn).
- **Left:** large square Tap pad; LoFi toggle below it; product name label.
- **Centre-left:** large Delay knob (label "Delay", scale ends "1/64T" … "2 bars" in note modes, "1"…"3500" in ms).
- **Centre top:** a row of four toggles: Phase L, Ping Pong, Dual, Phase R. Below: a dark display showing the
  delay time(s) — one value, or L and R values in Dual mode; in Host/BPM modes the note value plus tempo, in ms
  mode milliseconds; in Ping Pong a start-direction selector (L/R). Below the display: three sync buttons
  BPM / Host / ms. Values in the display can be dragged.
- **Centre-right:** large Feedback knob (0…200).
- **Right column:** vertical output meter (0 … −48 dBFS) with a peak/clip LED; Dry/Wet knob (0…100);
  Output knob (−18…+18 dB); Analog selector knob (Off, 1, 2, 3, 4).
- **Bottom row:** Modulation section — Depth (0…100) and Rate (0.1 Hz…6 kHz) knobs; Filters section — HiPass and
  LoPass knobs (20 Hz…20 kHz with Off at the respective extreme ends) with a small Link ring between them.
- Double-click / Alt-click resets a control; values editable by typing.

## 14. Public references usable by the implementer
- T. I. Laakso et al., "Splitting the unit delay", IEEE Signal Processing Magazine, 1996 (fractional delay).
- J. Dattorro, "Effect Design Part 2: Delay-Line Modulation and Chorus", J. Audio Eng. Soc. 45(10), 1997.
- U. Zölzer (ed.), "DAFX: Digital Audio Effects", 2nd ed., Wiley 2011 (delay effects, filters, nonlinear processing,
  envelope followers / dynamics).
- D. Giannoulis, M. Massberg, J. D. Reiss, "Digital Dynamic Range Compressor Design — A Tutorial and Analysis",
  J. Audio Eng. Soc. 60(6), 2012 (peak envelope detectors with attack/release).
- R. Bristow-Johnson, "Cookbook formulae for audio EQ biquad filter coefficients" (public).
