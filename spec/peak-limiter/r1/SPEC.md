# Look-ahead peak limiter — behavioural specification (candidate r1)

Status: APPROVED r1 (2026-10-29; review decisions D1–D4 in §10 under the standing policy: match the reference, do not reproduce clear defects, original visuals, own naming). Frozen — changes require a new revision.

Derived only from black-box measurement of the licensed reference processor in a VST3 host
(user-generated stimuli: DC carrier + clicks/bursts, steps, sine level staircases, noise bursts, synthetic
program), host-visible parameter metadata, and the publicly shipped user guide. No code, disassembly or
vendor assets were used or are included. The reference model below matches the reference output to
≤ −99 dB re peak on 69/71 deterministic test configurations at 48 kHz (the two exceptions are a
stream-start edge case and a 3.6e-5 reference-side rounding overshoot), and to ≤ 3e-5 in gain at 44.1/96 kHz.

Product naming, parameter IDs, GUI and branding are the implementer's own.

## 1. Scope
Wideband brick-wall peak limiter with look-ahead, in two variants sharing one algorithm:
**Stereo** (2 in / 2 out, linked detection) and **Mono** (1 in / 1 out). No requantisation, dither or
noise shaping (out of scope).

## 2. Parameters
Host-normalised value `r ∈ [0,1]`.

| Parameter | Mapping | Range | Default | Resolution |
|---|---|---|---|---|
| Threshold | `−30 + 30 r` dB | −30 … 0 dB | 0 dB | 0.1 dB |
| Out Ceiling | `−30 + 30 r` dB | −30 … 0 dB | 0 dB | 0.1 dB |
| Release | `0.01 · 10^(5r)` ms | 0.01 … 1000 ms | 1.00 ms (r = 0.4) | 0.01 ms (internal value is the 2-decimal displayed value; measured) |
| Auto Release | toggle (r ≥ 0.5 = on) | off/on | off | — |
| Bypass | host bypass | | off | — |

Release display: two decimals ("0.03 ms", "316.23 ms", "1000 ms"). Threshold/Ceiling "−12.0 dB".

## 3. Latency
Reported and actual latency `L` (output delayed by exactly L samples; all other sample positions below are
in *aligned* time, i.e. relative to the input sample that the output sample corresponds to):

| Sample rate | L |
|---|---|
| ≤ 48 kHz (44.1, 48 measured) | 64 samples |
| 88.2 / 96 kHz (96 measured) | 128 samples |
| 176.4 / 192 kHz | 256 samples (assumed by extrapolation) |

## 4. Gain computer (fixed release, Auto Release off)
In aligned time, for each sample n (stereo: one common gain for both channels):

1. Peak: `p[n] = max(|x_L[n]|, |x_R[n]|)` (mono: `|x[n]|`). Sample-peak only (no inter-sample/true-peak
   detection: an fs/4 sine with 0.707 sample peaks passes unchanged at Threshold 0).
2. Required gain: `r[n] = min(1, T / p[n])`, `T = 10^(Threshold/20)`.
3. Look-ahead hold over the **following** L samples (the current sample is not included):
   `h[n] = min(r[n+1], …, r[n+L])`.
4. Release state (initial `q[−1] = 1`):
   `q[n] = h[n]` if `h[n] < q[n−1]`, else `q[n] = h[n] − (h[n] − q[n−1])·c`,
   `c = exp(−ln10 / (Release_s · fs))` — i.e. the linear-gain distance to the current hold target shrinks
   by a factor 10 per Release time. Note the target is `h[n]`, not 1: a smaller over-threshold peak still
   inside the look-ahead window slows the recovery after a larger one.
5. Smoothing: two causal moving averages (boxcars) in cascade, lengths `A` and `B`:
   `s = MA_B(MA_A(q))`, with `(A, B) = (38, 27)` at ≤ 48 kHz and `(75, 54)` at 88.2/96 kHz
   (`A + B − 1 = L`). (192 kHz: `(150, 107)`, assumed.)
6. Output: `y[n] = x[n] · g[n] · 10^((Ceiling − Threshold)/20)` with `g[n] = min(s[n], r[n])`
   (see decision D1), delayed by L in real time.

Consequences (all measured): the gain starts moving exactly L samples before an over-threshold peak and
reaches the required value one sample before it; the attack shape is the S-curve of the two boxcars
(quadratic–linear–quadratic, identical in linear gain for every peak height); below threshold the gain is the
constant `Ceiling − Threshold` dB; with Threshold = Ceiling = 0 and no overs the output is bit-exact.

## 5. Auto Release (program-dependent release)
When on, the recovery after gain reduction depends on the preceding overload (duration and depth) and
scales with the Release setting, which acts as the upper bound of the recovery time. Behaviour to reproduce
(attack, look-ahead, smoothing, static curve unchanged; only step 4's recovery changes):

- Recovery is close to linear in dB after long overloads (≈ 8–9 dB per Release time after 0.5 s), slower after
  longer/deeper overloads, and much faster after short overloads (≈ 0.09 × Release to 63 % after 5 ms).
- Measured recovery of the linear-gain deficit after a DC step at 12 dB over threshold (Threshold −12 dB,
  times from the end of the overload, 48 kHz):

| Release | Overload | t63 | t90 | t99 |
|---|---|---|---|---|
| 31.62 ms | 5 ms | 2.75 ms | 4.58 ms | 6.25 ms |
| 31.62 ms | 20 ms | 2.90 | 5.40 | 7.60 |
| 31.62 ms | 100 ms | 4.65 | 12.12 | 14.38 |
| 31.62 ms | 500 ms | 26.60 | 34.33 | 36.60 |
| 31.62 ms | 2000 ms | 34.12 | 41.88 | 44.12 |
| 31.62 ms | 500 ms, 6 dB over | 13.98 | 18.75 | 20.29 |
| 31.62 ms | 500 ms, 24 dB over | 44.81 | 55.08 | 57.92 |
| 316.23 ms | 5 ms | 22.90 | 48.21 | 70.27 |
| 316.23 ms | 20 ms | 24.69 | 61.08 | 83.31 |
| 316.23 ms | 100 ms | 49.44 | 126.75 | 148.98 |
| 316.23 ms | 500 ms | 263.58 | 340.90 | 363.12 |
| 316.23 ms | 2000 ms | 335.54 | 412.85 | 435.08 |
| 316.23 ms | 500 ms, 6 dB over | 134.98 | 182.79 | 197.35 |
| 316.23 ms | 500 ms, 24 dB over | 453.85 | 556.46 | 584.60 |
| 1.00 ms | 500 ms | 1.17 | 1.65 | 2.04 |

The exact law is not specified (open question 1); any monotonic, click-free program-dependent release that
meets §8's tolerances on this table is acceptable.

## 6. Parameter changes
Not measured on the reference. Implementations must be click-free when Threshold/Ceiling move (smooth the
static gain ~20 ms); Release/Auto Release changes take effect on the next sample.

## 7. Behaviour not specified / out of scope
Requantisation/dither/noise shaping/true-peak domain (belong to a different product). GUI meters' ballistics.

## 8. Tolerances
- Fixed release (§4): output vs. the §4 model ≤ −100 dB re output peak on any stimulus, excluding the first
  L samples after stream start, at 44.1/48/88.2/96 kHz.
- Sample-peak output never exceeds the Ceiling by more than 1e-6 (linear, relative).
- Versus the reference itself, outputs differ only on samples where the reference exceeds the ceiling (D1):
  the last over-threshold sample of an overload with very fast Release (≤ ~0.3 % at 0.01 ms / 12 dB GR).
- Auto Release: each t63/t90/t99 in §5's table within ±20 %; recovery monotonic.
- Latency exactly as §3 and reported to the host.

## 9. Test vectors (48 kHz unless stated)
Static (1 kHz sine, steady state, output peak dBFS):

| Threshold / Ceiling | In −40 | In −24 | In −12 | In −8 | In 0 | In +12 |
|---|---|---|---|---|---|---|
| 0 / 0 | −40.00 | −24.00 | −12.00 | −8.00 | 0.00 | 0.00 |
| −24 / 0 | −16.00 | 0.00 | 0.00 | 0.00 | 0.00 | 0.00 |
| −12 / 0 | −28.00 | −12.00 | 0.00 | 0.00 | 0.00 | 0.00 |
| −6 / 0 | −34.00 | −18.00 | −6.00 | −2.00 | 0.00 | 0.00 |
| −12 / −3 | −31.00 | −15.00 | −3.00 | −3.00 | −3.00 | −3.00 |

Attack progress `(1 − g)/(1 − g_min)` around a single over-threshold click at k = 0 (any height):
k −64: 0.00097, −60: 0.01462, −56: 0.04386, −48: 0.14912, −40: 0.31676, −33: 0.50000, −32: 0.52632,
−24: 0.73099, −16: 0.88304, −8: 0.97271, −4: 0.99415, −1: 1.00000.
At 96 kHz: k −128: 0.00025, −112: 0.03778, −96: 0.13852, −64: 0.51333, −32: 0.87753, −16: 0.97037, −1: 1.00000.

Release (Threshold −12, Release 10 ms, DC carrier 0.01 after a 0.5 s DC overload of 1.0; gain without makeup,
samples after the overload end): 0: 0.25120, 16: 0.25509, 32: 0.27488, 64: 0.36408, 128: 0.53219,
240: 0.72664, 480: 0.91356, 960: 0.99136, 1920: 0.99991.

Click pair (Threshold −12, Release 10 ms, carrier 0.01, click 2.0 at k = 0 then click 1.0 at k = 64;
gain without makeup): 0: 0.12560, 32: 0.12925, 63: 0.14350, 64: 0.14402, 96: 0.18108, 128: 0.28218,
200: 0.49182, 480: 0.86736. (A fixed release toward unity would give ≈ 0.23 at k = 63 — the target rule of §4.4
is essential.)

Linking: a click on the left channel only reduces both channels identically.

## 10. Review decisions (r1)
- **D1 (defect not reproduced):** the reference applies the first release step at the peak sample itself
  (the hold excludes the current sample), so with very fast Release and deep reduction the peak sample can
  exceed the ceiling (≈ 0.001·(1−g)/g relative; measured +2e-5 at 12 dB / 10 ms). The implementation clamps
  the final gain to `min(s[n], r[n])`, guaranteeing the ceiling; otherwise identical.
- **D2:** the reference's stream-start behaviour (first L samples) is not specified; start with `q = 1` and an
  all-pass history.
- **D3:** the user guide states a 100 ms Release default; the reference's actual default is 1.00 ms. Use 1.00 ms
  (match behaviour).
- **D4:** Threshold/Ceiling are single values (the reference GUI has per-channel faders with a link, but the
  host exposes one value each); the GUI shows one control each.

## 11. Open questions
1. Exact Auto Release law (only the table in §5 is specified).
2. 176.4/192 kHz look-ahead and smoothing lengths (extrapolated).
3. Mono variant measured only by parameter metadata; assumed identical algorithm on one channel.
4. Parameter smoothing of the reference (not measured).

## 12. User interface (functional layout — visuals must be original)
Same controls and arrangement as the reference so its users find everything in place; all artwork, colours,
fonts and names are the implementer's own, drawn from scratch.
- Left: **Threshold** vertical fader (0 … −30 dB, scale 0/6/12/18/24/30) drawn over the input level meter(s),
  value box below showing the current gain reduction-relevant threshold.
- Middle: **Out Ceiling** vertical fader (0 … −30 dB) over the output level meter(s), value box below.
- Right of the faders: **Release** rotary knob (log 0.01 … 1000 ms, value shown in its centre, end labels
  "0.01" and "1000") and an **Auto Release** on/off switch below it.
- Far right: vertical **gain reduction meter** 0 … −30 dB (arc or bar), with numeric peak-GR readout below.
- Stereo variant shows two meter bars per fader; mono one.
- Double-click/Alt-click resets a control; value boxes are editable.
