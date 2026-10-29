# Stereo imager — behavioural specification (candidate r1)

Status: APPROVED r1 (2026-10-29, review decisions: §5 uses the true sample rate; §5.1 HF anomaly not reproduced). Frozen — changes require a new revision.

Derived only from (a) black-box measurement of the licensed reference processor in a VST3 host with
user-generated test signals, (b) host-visible parameter metadata, and (c) public documentation.
No code, disassembly or vendor assets were used or are included. Evidence: analyst workspace
(not part of the handoff). Measurement precision: residual between this spec's model and the
reference was ≤ −120 dB re. peak for 112/115 configurations at 48 kHz (see §9).

Product naming, parameter IDs, GUI and branding are the implementer's own choice.

## 1. Scope

A stereo-in / stereo-out processor with three variants that share one signal path:

| Variant | Controls |
|---|---|
| **Full** ("shuffler") | all controls in §3 |
| **Imager** | all except Shuffle, Frequency, Bass Trim (those fixed at neutral: Shuffle 1.0, Bass Trim 0.0 dB). Output identical to Full at those settings (measured ≤ 6e-8 abs). |
| **MS matrix** | no controls: `out1 = (in1+in2)/2`, `out2 = (in1−in2)/2` |

Stereo only (2 in, 2 out). Latency: 0 samples. Linear and time-invariant for fixed settings
(no level dependence measurable at −30/−12/−3 dBFS: residual ≤ −150 dB).

## 2. Definitions

`M = (L + R) / 2`, `S = (L − R) / 2`; inverse `L = M + S`, `R = M − S`.

## 3. Parameters

Host-normalised value `r ∈ [0,1]`. Values are **quantised to the stated resolution** before use
(observed: settings between steps produce bit-identical output to the step value).

| Parameter | Mapping from r | Range | Default | Resolution |
|---|---|---|---|---|
| Gain | `−24 + 24 r` dB | −24 … 0 dB | 0 dB (r=1) | 0.1 dB |
| Width | `w = r + 2 r²` | 0 … 3 | 1.00 (r=0.5) | 0.01 |
| Rotation | `−45 + 90 r` degrees | −45 … +45° | 0° | 0.1° |
| Asymmetry | `−90 + 180 r` degrees | −90 … +90° | 0° | 1° |
| Shuffle | `s = 1 + 2 r` | 1 … 3 | 1.0 | 0.1 |
| Frequency | `350 · 4^r` Hz | 350 … 1400 Hz | 650 Hz (r≈0.447) | 1 Hz |
| Bass Trim | `−3 + 6 r` dB | −3 … +3 dB | 0 dB | 0.1 dB |
| Swap | toggle | off/on | off | — |
| Polarity 1, Polarity 2 | toggles | +/− | + | — |
| Input mode | toggle | LR / MS | LR | — |
| Bypass | toggle (host bypass) | | off | — |

Rounding ties: at exact half-step points the reference sometimes rounds down (e.g. Width 1.495 → 1.49);
not compatibility-critical.

## 4. Signal path (in this order)

1. **Input polarity**: Polarity 1 negates input channel 1; Polarity 2 negates input channel 2.
2. **Swap**: exchanges the two channels.
3. **Decode** to (M, S):
   - LR mode: `M = (c1 + c2)/2`, `S = (c1 − c2)/2`
   - MS mode: `M = c1`, `S = c2` (no scaling)
   (So in MS mode polarity/swap act on M/S, which matches the documented "perceived" effects.)
4. **Width** `w`:
   - `w ≤ 1`: `M' = M`, `S' = w·S`
   - `w > 1`: `g = sqrt((5 + 3/w²) / 8)`; `M' = g·M`, `S' = g·w·S`
   (continuous at w=1; at w=3, g = sqrt(2/3).)
5. **Asymmetry** `a` (degrees): `M' = M − sin(a)·S`, `S' = cos(a)·S`.
6. **Rotation** `φ` (degrees): `M' = cos φ·M + sin φ·S`, `S' = −sin φ·M + cos φ·S`
   (equivalently `L' = cos φ·L − sin φ·R`, `R' = sin φ·L + cos φ·R`; +45° maps an L-only input equally to both outputs).
7. **Shuffler** (Full variant only; see §5). Applied separately to M and S.
8. **Gain**: multiply both by `10^(Gain/20)`.
9. **Encode**: `L = M + S`, `R = M − S`.

Note: Asymmetry followed by Rotation with equal angle magnitudes behaves as a balance control
(e.g. Rotation +22.5°, Asymmetry +45° → L×0.5412, R×1.3066, no crosstalk).

## 5. Shuffler

Active **only if** Shuffle ≠ 1.0 **or** Bass Trim ≠ 0.0 dB (after quantisation). Otherwise the stage is
bypassed entirely (bit-exact passthrough; note the active stage is *not* transparent at unity gain because
of its phase response).

Low-frequency gains, with `T = 10^(BassTrim/20)`:

```
G_M = T · sqrt((1 + 1/s²) / 2)
G_S = s · G_M          ( = T · sqrt((1 + s²) / 2) )
```

Each of M and S passes through one first-order section with DC gain G and high-frequency gain −1
(unity magnitude, 180° phase), transition geometric-mean (|H| = sqrt(G) at the corner), defined in
continuous time as

```
H(s) = (sqrt(G)·ω_b − s) / (ω_b/sqrt(G) + s)
```

discretised by the bilinear transform at the **actual** sample rate `fs`
(review decision: the reference computes this with a fixed 44.1 kHz design rate, which shifts its corner by
fs/44100 at other rates; do NOT reproduce that):

```
k = tan(π · Frequency / fs),  g = sqrt(G)
H(z) = [ g·k·(1 + z⁻¹) − (1 − z⁻¹) ] / [ (k/g)·(1 + z⁻¹) + (1 − z⁻¹) ]
```

The corner f_b therefore equals Frequency at every sample rate (reference-identical at 44.1 kHz). M and S share nearly the same phase
(the documented "phase-compensated" property); the phase falls from 0° at DC to −180° at Nyquist.

### 5.1 High sample rates
The response must follow §5 at every supported rate (44.1–192 kHz) with no additional HF loss.
(The reference shows a setting-dependent HF loss above ~5.5 kHz at 96 kHz; review decision: not reproduced.)

## 6. Parameter smoothing (dynamic behaviour)

- Gain, Width, Rotation, Asymmetry, and the Swap/Polarity toggles: output transitions as a first-order
  exponential with time constant ≈ 40 ms (remaining error ×0.741 per 12 ms); toggles therefore crossfade
  rather than switch. Measured at 48 kHz with host automation steps; onset within ~3 ms of the step.
- Shuffle and Bass Trim: smoother S-shaped (≈ second-order) transition, 10–90 % ≈ 160 ms.
- Frequency smoothing: **not measured** (open question).
Exact smoothing law is not compatibility-critical; implementations must be click-free.

## 7. Behaviour not specified / out of scope
Audio behaviour of GUI elements (none: meters/display are read-only). Mono-in or >2 channels. Reference HF anomaly (§5.1).

## 8. Tolerances for acceptance
- Static response at any sample rate, any setting, versus the §4–§5 model: |error| ≤ −100 dB re. output peak on sweep/noise tests
  (reference achieves ≤ −120 dB vs. this model; float32 rounding).
- Latency 0 samples. No level dependence.
- Smoothing: τ within 30–50 ms for broadband controls; no discontinuities on toggles.

## 9. Test vectors (expected observations)

All at 48 kHz unless stated; matrices are steady-state, frequency-independent gains
`[L_out; R_out] = A · [L_in; R_in]`, rows = outputs.

| Setting | A (LR) |
|---|---|
| neutral | [1 0; 0 1] |
| Width 0 | [0.5 0.5; 0.5 0.5] |
| Width 0.48 (r=0.3) | [0.74 0.26; 0.26 0.74] |
| Width 1.32 (r=0.6) | [1.0633 −0.1467; −0.1467 1.0633] |
| Width 2.08 (r=0.8) | [1.2992 −0.4555; −0.4555 1.2992] |
| Width 3.00 | [1.6330 −0.8165; −0.8165 1.6330] |
| Rotation +45° | [0.7071 −0.7071; 0.7071 0.7071] |
| Rotation −18° | [0.9511 0.3090; −0.3090 0.9511] |
| Asymmetry +36° | [0.6106 0.3894; −0.1984 1.1984] |
| Asymmetry −90° | [1 0; 1 0] |
| Asymmetry +90° | [0 1; 0 1] |
| Gain −6.0 dB | 0.5012 · I |
| Swap | [0 1; 1 0] |
| Polarity 1 | [−1 0; 0 1] |
| MS input | [1 1; 1 −1] |
| MS input + Swap | [1 1; −1 1] |
| MS input + Width 2.08 | [0.8436 1.7547; 0.8436 −1.7547] |
| Width 2.08 + Rotation +22.5° | [1.3746 −0.9180; 0.0763 1.0259] |
| Rotation +22.5° + Asymmetry +45° | [0.5412 0; 0 1.3066] |
| Width 0.59 + Asymmetry +45° | [0.5 0.5; 0.0828 0.9172] |

Shuffler (M/S-domain magnitude/phase, Width 1, Bass Trim 0, **44.1 kHz** — reference-identical rate;
at other rates the same values hold at the same frequency ratio f/Frequency up to bilinear warping):

| Shuffle | Frequency | f | M | S |
|---|---|---|---|---|
| 3.0 | 650 | 5 Hz | −2.553 dB / −0.9° | +6.989 dB / −1.0° |
| 3.0 | 650 | 200 Hz | −2.330 dB / −34.5° | +6.337 dB / −36.3° |
| 3.0 | 650 | 650 Hz | −1.276 dB / −90.0° | +3.495 dB / −90.0° |
| 3.0 | 650 | 1500 Hz | −0.405 dB / −133.0° | +1.166 dB / −131.0° |
| 3.0 | 650 | 4 kHz | −0.063 dB / −161.8° | +0.188 dB / −160.7° |
| 3.0 | 650 | 20 kHz | 0.000 dB / −179.2° | 0.000 dB / −179.2° |
| 3.0 | 350 | 200 Hz | −1.921 dB / −59.8° | +5.201 dB / −61.4° |
| 3.0 | 1400 | 1400 Hz | −1.276 dB / −90.0° | +3.495 dB / −90.0° |
| 3.0 | 1400 | 4 kHz | −0.269 dB / −142.0° | +0.786 dB / −140.1° |

DC gains (dB) for M/S: Shuffle 1.5 → −1.413/+2.109; 2.0 → −2.041/+3.979; 2.5 → −2.366/+5.593; 3.0 → −2.553/+6.990.
Bass Trim adds its value in dB to both DC gains (e.g. Shuffle 3, Trim +3 → +0.447/+9.990). Shuffle 1.0 with
Trim ±3 dB → both M and S ±3.000 dB at DC with the §5 phase response.
For any G, the section's phase is exactly −90° and |H| = sqrt(G) at f = Frequency, at any sample rate.

## 10. Open questions
1. Frequency-parameter smoothing: not measured — use the same second-order smoothing as Shuffle (§6).
2. Exact smoothing law — only time constants are specified; any click-free implementation within §8 is acceptable.

## 11. Public references usable by the implementer
- A. D. Blumlein, British Patent 394,325 (1931) — width / shuffling concepts.
- M. A. Gerzon, "Applications of Blumlein Shuffling to Stereo Microphone Techniques", J. Audio Eng. Soc. 42(6):435–453, 1994.
- J. Eargle (ed.), "Stereophonic Techniques", AES anthology, 1986.
- Bilinear transform / first-order shelving filter design (any DSP textbook, e.g. Zölzer, "DAFX").

## 12. User interface (functional layout — visuals must be original)

The GUI must offer the same controls, arrangement and display semantics as the reference, so a user of the
reference finds everything in the same place, but **all artwork, textures, colours, fonts, logos and names must
be the implementer's own** (no copied images, no vendor branding). Build it from scratch (e.g. JUCE vector drawing).

Layout (Full variant; Imager omits Shuffle, Freq, Bass Trim):
- **Left block**, four vertical faders side by side with labels on top and a numeric value box underneath:
  Gain (scale 0 / −12 / −24 dB), Width (3.0 / 2.0 / 0.0, default 1.00), Shuffle (3.0 / 1.0), Freq (1.4 k / 350, default 650).
- **Under the faders**: "Input Mode" switch L-R / M-S, with two polarity buttons (+/−) on either side of a
  swap button (↔) — left button = channel 1 polarity, right = channel 2; and a "Bass Trim" numeric box (Full only).
- **Centre**: the stereo vector display (below). Underneath it two horizontal sliders with value boxes:
  Asymmetry (−90 … 90, centre 0) and Rotation (−45 … 45, centre 0).
- **Right block**: stereo output meters L and R (0 … −24 dB scale) with peak-hold value boxes that reset on click,
  and a "Meter Mode" switch L-R / M-S (M/S mode shows (L+R)/2 and (L−R)/2; display only).
- A Reset control restoring Width, Asymmetry, Rotation, Shuffle to neutral (not Gain, polarity, swap).
- Value formats: Gain "0.0", Width "1.00", Shuffle "1.0", Freq "650", Asymmetry "0", Rotation "0.0", Bass Trim "0.0".
- Double-click (or Alt-click) a control resets it to its default. Values editable by typing in the boxes.

Stereo vector display (semicircle, origin at bottom centre, radial dB rings, scale along the base
"0 −6 −12 −18 −24 −18 −12 −6 0", i.e. radius 0 at −24 dB, full radius at 0 dB):
- Draw three lines from the origin for the output of the broadband path (§4 steps 1–6 and 8, excluding the
  shuffler) for three probe inputs: left-only (1, 0), centre (1/√2, 1/√2), right-only (0, 1).
- For probe output (Lo, Ro): angle from vertical `θ = atan2(Lo − Ro, Lo + Ro)` (left positive; ±45° = speaker
  positions, beyond ±45° = outside the speakers), level `20·log10(sqrt(Lo² + Ro²))` dB mapped to radius,
  clamped to [−24, 0] dB.
- Fill the triangle spanned by the three line end points semi-transparently; neutral settings show lines at
  ±45° and vertical, all at 0 dB.
- Interaction: drag in the display — vertical changes Gain, horizontal changes Rotation; with Alt held,
  vertical changes Width and horizontal changes Asymmetry.
- The display reflects settings, not the input signal, and does not show shuffling.

MS matrix variant: no controls; a minimal original panel stating its function is sufficient.
