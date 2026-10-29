# Hybrid delay — behavioural specification (candidate r1)

Status: APPROVED r1 (2026-10-29). Review gate applied with standing user decisions (§11). Frozen — changes require a new revision.

Derived only from black-box measurement of the licensed reference processor in a VST3 host (user-generated
impulses, sweeps, tones, click trains, ramps), host-visible parameter metadata, and the vendor's user guide
(paraphrased). No code, disassembly or vendor assets used or included. Product naming, parameter IDs and GUI
are the implementer's own.

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
| HiPass | Off (r=0) or `19 · 1058^r` Hz (≈27 Hz…20.1 kHz) | Off |
| LoPass | `19 · 1058^r` Hz (≈19 Hz…14.2 kHz) or Off (r=1) | Off |
| Filter Link | GUI control moving both filter frequencies by the same ratio | — |
| Tap | GUI: delay = interval of the last two taps; quantised to nearest note value in Host sync; sets BPM in BPM sync | — |

Display formats: "375 ms"; note names as listed; Feedback "60"; Mix "100"; Output "0.0 dB"; Rate "0.10 Hz", "1.15k Hz";
Depth "0 %"; filters "437Hz", "1.24kHz", "Off".

## 3. Delay time

- ms mode: delay `D = t_ms · fs / 1000` samples, rounded to the nearest sample (review decision §11.2).
- Note modes: `t = beats(note) · 60 / tempo`. If `t > 3500 ms`, use the **longest note value in the list whose
  duration ≤ 3500 ms** (e.g. at 60 BPM, "1 bar" and "2 bars" both give 3.0 s = 1/2D). Host mode uses the host tempo.
- Delay is exact in milliseconds at every sample rate (verified 44.1/48/96 kHz).

## 4. Signal flow (normal stereo mode, per channel c ∈ {L, R}, Analog Off, LoFi off, Depth 0)

```
v[n]   = x_c[n] + g · w[n − D − 1]            g = Feedback/100   (feedback loop length is D + 1 samples)
w[n]   = HP( LP( sat(v[n]) ) )                 filters only when enabled (§6)
wet[n] = w[n − D]                              (first echo at exactly D samples)
sat(v) = v / (1 + |v|)
```

- The saturation is in the loop: each repeat is re-saturated. Example (impulse 0.5 at 100 ms, Feedback 60 %):
  echoes ≈ 0.333, 0.166, 0.090, 0.051 … at 4800, 9601, 14402, 19203 samples (48 kHz).
- Feedback > 100 % grows until the saturation bounds it (self-oscillating, bounded by sat).
- Phase L / Phase R negate that channel's wet signal.
- Output stage: `y = G · ( cos(m·π/2) · x + sin(m·π/2) · wet )`, m = Mix/100 (equal-power), G = 10^(Output/20).
  Mix and Output act after the saturation.
- Mix and Output changes are effectively immediate (complete within ~5 ms; no audible ramp).

## 5. Ping Pong (stereo variant)

Input to the delay network is the mono sum `xm = (L + R)/√2`. Two lines L and R (each as in §4: sat, filters,
loop length D + 1):

```
wL[n] = F( sat( injL·xm[n] + g² · wR[n − D − 1] ) )
wR[n] = F( sat( injR·xm[n] + 1  · wL[n − D − 1] ) )
wetL[n] = wL[n − D],  wetR[n] = wR[n − D]
```

`injL = 1, injR = 0` for start Left; the reverse for start Right. Note the fixed, asymmetric hop gains
(L→R unity, R→L g²) — observed on the reference for both start directions and several feedback values;
reproduce them (review decision §11.3). Echoes alternate sides every D + 1 samples.

Example (impulse 0.5 on L, 100 ms, Feedback 60 %, start Left; amplitudes): L 0.261 @4800, R 0.206 @9601,
L 0.068 @14402, R 0.063 @19203, L 0.022 @24004. Start Right: R 0.261 @4800, L 0.085 @9601, R 0.078 @14402,
L 0.027 @19203.

## 6. Filters (in the loop, after the saturation)

RBJ-cookbook biquads, digital corner `w0 = 2 · atan(π · f / fs)` (bilinear without pre-warping — the effective
corner is `(fs/π)·atan(π f/fs)`, e.g. 14.2 kHz → 11.44 kHz at 48 kHz), applied every pass of the loop:
- HiPass: 2nd-order high-pass, **Q = 0.80**.
- LoPass: 2nd-order low-pass, **Q = 0.95**.
Fit residual 0.000 dB over 15 Hz–20 kHz at 44.1/48/96 kHz.

## 7. LoFi (in the loop)

Fixed low-pass, independent of sample rate (same response in Hz at 44.1/48/96 kHz), magnitude relative to HiFi:

| f | ≤ 4 kHz | 5 kHz | 6 kHz | 7 kHz | 8 kHz | 9 kHz | ≥ 10 kHz |
|---|---|---|---|---|---|---|---|
| dB | 0 (±0.05) | −1.0 | −4.4 | −12 | −27 | −51 | ≤ −55 |

Weak non-harmonic images (≤ −59 dBc for a 7 kHz tone) exist on the reference; not required.

## 8. Modulation (delay-time LFO)

- Triangle LFO (both channels in phase — assumption, R not measured), rate as set (verified 0.1–2 Hz within 0.2 %).
- Instantaneous delay `D(t) = D · (1 + (Depth/100) · tri(t) / 2)` for short delays and slow rates: amplitude
  `A = Depth/100 · D/2` (measured ratio 1.01–1.03 for D ≤ 50 ms at ≤ 3 Hz; e.g. D = 50 ms, Depth 50 → ±12.4 ms).
- The modulation signal is smoothed before it reaches the delay line. Observed:
  - rate dependence (D = 10 ms, Depth 10 %): amplitude ratio 1.03 @0.5 Hz, 1.02 @2 Hz, 0.98 @5 Hz, 0.70 @10 Hz
    (sine-like), 0.50 @20 Hz, 0.03 @50 Hz;
  - long delays (Rate 0.5 Hz, Depth 10 %): ratio 0.95 @100 ms, 0.69 @200 ms, 0.43 @500 ms, 0.25 @1000 ms,
    0.07 @3500 ms (amplitude saturates around 12–13 ms) — consistent with a limit on the rate of delay change
    of roughly 35–40 ms/s.
  Implement a smoothing/slew stage reproducing these points within ±15 % (see §10); exact law is open (§12).
- Fractional delay must be interpolated (band-limited interpolation recommended; the reference's interpolation
  kernel is long, sinc-like).

## 9. Delay-time changes ("tape" behaviour) and Analog modes

**Delay-time changes** glide: the read delay follows a one-pole approach to the new value with τ ≈ 0.2 s
(63 % after 0.2 s, 92 % after 0.5 s), producing a temporary pitch shift (e.g. 100→200 ms: output pitch drops to
~0.55× and recovers). At the start of a change the wet output dips briefly (~50 ms when increasing,
~90 ms when decreasing). Tolerance: τ 0.15–0.25 s; dip optional but must be click-free.

**Analog** (applies to the wet path). Off = §4 exactly. Modes add colouration, distortion and noise; no wow/flutter
(timing jitter < 0.1 µs). Targets:

Small-signal magnitude relative to Off (−30 dBFS sweep):

| Mode | 20 Hz | 50 | 100 | 200 | 500 | 1 k | 2 k | 5 k | 10 k | 20 k |
|---|---|---|---|---|---|---|---|---|---|---|
| 1 | −3.4 | −3.0 | −1.7 | −0.8 | −0.4 | −0.3 | −0.3 | −0.3 | −0.4 | −0.5 |
| 2 | −0.7 | −0.7 | −0.7 | −0.7 | −0.8 | −1.0 | −1.4 | −1.5 | −1.7 | −1.7 |
| 3 | −2.1 | −6.1 | −13.3 | −7.5 | −2.3 | −1.5 | −1.0 | −0.6 | −0.5 | −0.5 |
| 4 | −0.4 | −0.3 | −0.4 | −0.5 | −0.5 | −0.5 | 0.0 | −0.2 | −0.2 | 0.0 |

Distortion of a 100 Hz tone (fundamental gain re input dB / H2 / H3 dBc), at input amplitudes 0.1 / 0.3 / 0.95:

| Mode | a = 0.1 | a = 0.3 | a = 0.95 |
|---|---|---|---|
| Off | −0.8 / −54 / −43 | −2.1 / −66 / −34 | −5.3 / −81 / −25 |
| 1 | −2.2 / −34 / −35 | −2.9 / −29 / −30 | −5.5 / −28 / −24 |
| 2 | −1.4 / −51 / −45 | −2.5 / −52 / −35 | −5.5 / −50 / −26 |
| 3 | −3.6 / −19 / −20 | −2.9 / −23 / −34 | −5.5 / −31 / −28 |
| 4 | −0.8 / −54 / −41 | −2.1 / −66 / −33 | −5.3 / −81 / −25 |

Mode 3 is strongly non-linear at low level (fundamental −10 dB, H2 −13 dBc at a = 0.03): a crossover-like
"starved" character. Output noise with silent input (wet, Mix 100): Off none (digital silence); 1: −103 dBFS;
2: −85.5 dBFS; 3: −106 dBFS; 4: −109 dBFS RMS, concentrated below ~100 Hz (strongest components ≈ 8–12 Hz,
46.6 Hz, 91.8 Hz).

## 10. Tolerances (acceptance)

| Item | Tolerance |
|---|---|
| Delay times (ms and note modes, all rates) | exact to ±1 sample |
| Deterministic core §4–§6 (Analog Off, LoFi off, Depth 0), impulses ≤ 0.5 and sweeps ≤ −12 dBFS | per-echo peak within ±0.1 dB for the first 6 echoes (Feedback ≤ 100 %); filter magnitude ±0.1 dB |
| Sine fundamental through sat (−30…0 dBFS) | within 0.3 dB of the §9 "Off" row |
| Ping Pong echo amplitudes (§5 example) | ±0.2 dB, timing exact |
| LoFi table | ±1 dB down to −30 dB; stopband ≤ −50 dB above 9 kHz |
| Modulation (§8 tables) | amplitude ±15 %, rate ±1 % |
| Analog tables | response ±1 dB; fundamental ±0.5 dB; harmonics ±4 dB; noise level ±3 dB |
| Smoothing | click-free; delay glide τ 0.15–0.25 s |

## 11. Review decisions (standing user policy: match the reference, do not reproduce clear defects)

1. The reference adds a constant extra fractional delay of ≈ 0.031 samples (0.65 µs) to the wet path via a
   sinc-like interpolator (visible as ±3 % side taps around each echo, −5° at 20 kHz). **Not reproduced**
   (defect); echoes land on exact samples for integer-sample delays.
2. The reference truncates note-value delays and lands one sample short for triplet values (e.g. 999 instead of
   1000 samples). **Not reproduced**: round to nearest.
3. Ping Pong asymmetric hop gains (§5): **reproduced** (audible character; not clearly a defect).
4. Note values longer than 3500 ms fall back to the longest fitting note (§3): **reproduced**.

## 12. Open questions

1. Exact modulation smoothing law (§8) — only the tabulated points are specified.
2. Whether modulation is identical on L and R; Mono and Mono→Stereo variants unmeasured.
3. Placement of the Analog stage relative to the loop; exact noise spectrum shape.
4. Delay-change dip shape (§9).

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
  LoPass knobs (20 Hz…20 kHz with Off at the respective ends) with a small Link ring between them.
- Double-click / Alt-click resets a control; values editable by typing.

## 14. Public references usable by the implementer
- T. I. Laakso et al., "Splitting the unit delay", IEEE Signal Processing Magazine, 1996 (fractional delay).
- J. Dattorro, "Effect Design Part 2: Delay-Line Modulation and Chorus", J. Audio Eng. Soc. 45(10), 1997.
- U. Zölzer (ed.), "DAFX: Digital Audio Effects", 2nd ed., Wiley 2011 (delay effects, filters, nonlinear processing).
- R. Bristow-Johnson, "Cookbook formulae for audio EQ biquad filter coefficients" (public).
