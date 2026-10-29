# Automatic vocal level rider — behavioural specification (r2)

Status: APPROVED r2 (2026-10-29; supersedes r1; review decisions D1–D7 in §10; see "Changes from r1" at the end). Frozen. Derived only from black-box measurement of a licensed reference processor in a host
(user-generated test signals, host-visible parameters) and its public user documentation. No vendor code, assets or
names. Measurements: 48 kHz (44.1/96 kHz checks), 32-bit float, REAPER offline render.

## 1. Scope
An automatic "fader rider": a single time-varying broadband gain (identical on all channels) that steers a vocal
track toward a target level, with an activity detector that freezes riding between phrases and returns the fader
to an idle position during longer silence. No filtering, no saturation, no other processing reaches the output.

Products: **Stereo** (primary, measured), **Mono** (same algorithm on one channel; assumed), and **Live** Stereo /
Mono, which add one control, *Spill* (§7.4; not measured). Optional sidechain input for a music bus (§8).

Latency: **0 samples**. Output = input × 10^((g + Output)/20) per sample, where g is the ride gain (dB). Measured:
residual of the output against a per-5-ms scalar gain ≤ −150 dB (no colouration); L and R identical gain;
Output trim exact (+6.000 dB for +6).

## 2. Parameters (host-normalised value r ∈ [0,1])

| Parameter | Mapping | Range | Default | Display |
|---|---|---|---|---|
| Target | −42 + 42·r dB | −42 … 0 dBFS | −21 | "−21.0 dB" (0.1 dB) |
| Vocal Sensitivity | −12 + 24·r dB | −12 … +12 | 0 | 0.1 dB |
| Music Sensitivity | r = 0: Off; else −12 + 24·r dB | Off, −12 … +12 | 0 (r ≈ 0.502) | "Off" / 0.1 dB |
| Range Max | −11 + 23·r dB | −11 … +12 | +6 (r = 0.739) | 0.1 dB |
| Range Min | −12 + 23·r dB | −12 … +11 | −6 (r = 0.261) | 0.1 dB |
| Idle | −11.5 + 23·r dB | −11.5 … +11.5 | 0 | 0.1 dB (no unit) |
| Rider (fader) | −12 + 24·r dB | −12 … +12 | Idle value | 0.1 dB — reflects the current ride gain g (see §9) |
| Output | −12 + 24·r dB | −12 … +12 | 0 | 0.1 dB |
| Attack | r < 0.5: Slow, else Fast | Slow / Fast | Slow | |
| Spill (Live only) | −48 + 36·r dBFS | −48 … −12 | −48 | 0.1 dB |

Range Min must stay at least 1 dB below Range Max (effective range 1 … 24 dB); if a host sets them crossed, the most
recently changed control wins and the other is pushed away by 1 dB.

## 3. Signal flow (per block of 1 ms, gain interpolated per sample)
1. Detector input: the mean of the channels' signals ((L+R)/2 for stereo — only L = R was measured), weighted (§4).
2. Activity gate (§7) decides Ride / Idle.
3. Ride: level detector with ballistics (§6) → static law (§5) → gain smoothing (§6) → g clamped to [Range Min, Range Max].
4. Idle: hold, then move g toward the Idle value (§7.3).
5. Output gain applied after riding.

## 4. Detector weighting
The detector responds to a band-limited version of the signal. Normative: table T6 (±1.0 dB). A first-order high-pass
at 230 Hz in series with a first-order low-pass at 8.5 kHz, normalised to 0 dB at 1 kHz, meets the table within about
±1.2 dB and is an acceptable starting point; adjust to meet ±1.0 dB. A steady sine is read at its weighted RMS level.

## 5. Static law (normative, exact on steady tones)
Let L be the detector level (dB, §6; for a steady sine the weighted RMS), T the Target, u = L − T,
W = (Range Max − Range Min)/2 and c = (Range Max + Range Min)/2. The steady-state ride gain is

```
G_lin = a · 10^(−W/20) + b · 10^(−(u + c)/20)          a = 0.5162, b = 0.7252
g*    = clamp( c + 20·log10(G_lin),  Range Min, Range Max )
```

(Equivalently: the fader position relative to the range centre c is a blend of a fixed term proportional to the
range floor and a term proportional to Target/level.) Verified against 1 kHz sine steps for ranges ±12, ±6, ±3,
(−2, +10) and both Attack modes: rms 0.06 dB, max 0.19 dB; identical at 44.1/48/96 kHz (≤ 0.04 dB). Table T1.

## 6. Dynamics while riding (normative by tables T2, T3; structure non-binding)
Normative: band-noise equilibria T2 (±0.6 dB) and step trajectories T3 (each value within ±1.5 dB, or reached within
±30 % of the stated time, whichever is looser). Both Attack modes. Times are in seconds at every sample rate.

Non-binding structure that reproduces T2/T3 to ~0.3–0.5 dB rms (worst transitions ±2 dB):
- Level detector on the weighted mean square of 1 ms blocks: power one-pole with attack τa when the block power
  exceeds the state, otherwise hold for h, then release τr. L = 10·log10(state).
- Ride gain follows g* (§5) through a one-pole in dB with τ_up (g rising) / τ_down (g falling).

| Constant | Slow | Fast |
|---|---|---|
| τa (attack) | 71 ms | 2.1 ms |
| h (hold) | 50 ms | 450 ms |
| τr (release) | 0.94 s | 1.30 s |
| τ_up (gain rising) | 0.45 s | 0.79 s |
| τ_down (gain falling) | 52 ms | 15 ms |

Consequences visible in the data: noisy (fluctuating) material is read above its RMS (band noise +3 dB in Slow,
about +5 dB in Fast), so Fast settles 1.5–2 dB lower than Slow on noise but not on steady tones; downward rides take
~0.1–0.3 s, upward rides start after a delay that grows with the size of the drop (≈0.2 s for 6 dB, ≈1–2 s for 12 dB)
and then accelerate.

## 7. Activity gate and idle behaviour (normative by tables T4, T5)
### 7.1 Absolute threshold
Riding requires the weighted detector level (smoothed, one-pole τ ≈ 0.1 s on power) to exceed
θ_on = −62 − VocalSensitivity (dBFS); riding stops when it falls below θ_off = θ_on − 4 dB. Normative: table T5 —
the Ride/Idle state at the end of each 4 s step must match, except that a cell adjacent to a state change may differ
(±3 dB threshold tolerance). Re-acquisition after Idle near the threshold takes up to ~0.5 s (Slow).
### 7.2 Relative threshold (spill / non-vocal rejection)
A level that falls more than ≈24 dB below the recent vocal level is treated as non-vocal (Idle). The recent vocal
level Lref is the running maximum of the block level, decaying at ≈1.5 dB/s while not exceeded (it keeps decaying
while Idle). Drops of 12 dB or less never trigger it. Normative: table T7 (trajectories after drops of 6–36 dB,
both Attack modes) and T2 column "−3 → −33"; tolerance as T3 (±1.5 dB or ±30 % in time).

**The level detector is not frozen while Idle (r2 clarification).** Measured behaviour after a relative-threshold
Idle: riding resumes once the current level has come back within roughly 18–21 dB of the decaying Lref (i.e. with
about 3–6 dB of hysteresis against the 24 dB entry criterion), and the level detector restarts from Lref (the
remembered vocal level, decayed by ≈1.5 dB/s since the drop) rather than from the level held at the start of Idle.
The ride therefore first dips toward the gain implied by the decayed Lref and then releases toward the new level
(T7: about −8, −5.7 and −3.8 dB for 24, 30 and 36 dB drops in Slow). After an Idle caused by the absolute
threshold (§7.1, silence), the detector restarts from the current input (no stale state; T4).
Informative structure: the §6 detector plus the two rules above (re-entry at Lref − 20 dB, detector seeded with
Lref) reduces the model error against the reference by about half (drops: 4.4 → 2.0 dB rms) but does not by itself
meet every T3/T7 cell; the tables govern.
### 7.3 Idle
At start-up g = Idle. When riding stops: Slow holds g for 0.5 s, then moves g toward Idle at ≈6.5 dB/s
(≈4–6 dB/s when starting from below −6 dB), stopping exactly at Idle; Fast starts moving immediately, accelerating
to ≈6 dB/s. When riding resumes, it starts from the current g (typically Idle). Short gaps (< 0.5 s in Slow) do not
move the fader. Normative: table T4 (±1.0 dB).
### 7.4 Spill (Live products only — not measured)
Riding additionally requires the weighted level to exceed the Spill value (dBFS). With Spill at −48 (default) the
Live product behaves like the standard one for vocal-level material.

## 8. Music sidechain (not characterised)
With no sidechain signal, or Music Sensitivity Off, the music input has no effect (measured: bit-identical output).
Behaviour with a sidechain was not measured. Implementation rule (review decision D3): when a sidechain is connected
and Music Sensitivity is not Off, the effective target becomes T_eff = T + k·(M − M̄), where M is the weighted
sidechain level (same detector as §6, Slow constants), M̄ its 10 s running average, and k = 10^(MusicSens/20)·0.5,
limited to ±6 dB; a status indicator shows |T_eff − T|.

## 9. Rider fader parameter and automation
The Rider parameter always shows the current g. Automation mode (GUI, saved with state): Off (internal engine),
Write (g is also written to the host as parameter changes with begin/end gestures), Read (g follows the host's
Rider automation; the internal engine is bypassed). Dragging the Rider fader overrides g while held; riding resumes
from that value on release.

## 10. Review decisions
- D1 Static law, ballistics, gate thresholds and idle behaviour reproduced as measured (tables normative).
- D2 Vocal Sensitivity −12: the reference becomes erratic/sticky (rides freeze at intermediate values, T5 row −12).
  Not reproduced; the §7.1 formula applies at all settings (T5 row −12 is informative only).
- D3 Music sidechain behaviour defined by the rule in §8 (reference not characterised).
- D4 Spill implemented as an extra absolute gate (§7.4), not measured.
- D5 Detection on the channel mean; identical gain on all channels.
- D7 (r2) The detector state after Idle follows §7.2 (not frozen); T7 added as normative.
- D6 Parameter changes take effect within one block; Target, Range and Output changes are smoothed over ≈20 ms
  (click-free); the reference applies changes asynchronously (not reproduced).

## 11. Tolerances (acceptance)
T1 ±0.25 dB; T2 ±0.6 dB; T3 ±1.5 dB or ±30 % in time; T4 ±1.0 dB; T5 state match with ±3 dB threshold tolerance;
T6 ±1.0 dB; T7 as T3 (±1.5 dB or ±30 % in time). Latency 0. Output a pure gain (no colouration, ≤ −120 dB residual vs a per-block scalar gain).
Results identical in dB and seconds at 44.1, 48 and 96 kHz.

## 12. Test vectors
Stimuli: "band noise" = Gaussian noise band-passed 300–3000 Hz (4th-order Butterworth), stated as RMS dBFS;
"sine" = 1 kHz unless stated. Gain = 20·log10(rms(out)/rms(in)) over 5 ms windows.

### T1 static law, 1 kHz sine (end of 8 s steps; dB)

| Range / mode | -45 | -41 | -37 | -33 | -29 | -25 | -21 | -17 | -13 | -9 | -5 | -1 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| ±12 (Min −12, Max +12) | +12.00 | +12.00 | +12.00 | +9.47 | +5.84 | +2.22 | -1.27 | -4.55 | -7.53 | -10.13 | -12.00 | -12.00 |
| default (−6/+6) | +6.00 | +6.00 | +6.00 | +6.00 | +6.00 | +3.03 | -0.10 | -2.89 | -5.27 | -6.00 | -6.00 | -6.00 |
| −3/+3 | +3.00 | +3.00 | +3.00 | +3.00 | +3.00 | +3.00 | +0.77 | -1.72 | -3.00 | -3.00 | -3.00 | -3.00 |
| Min −2, Max +10 | +10.00 | +10.00 | +10.00 | +10.00 | +6.97 | +3.88 | +1.10 | -1.27 | -2.00 | -2.00 | -2.00 | -2.00 |
| ±12, Attack Fast | +12.00 | +12.00 | +12.00 | +9.40 | +5.78 | +2.16 | -1.33 | -4.61 | -7.58 | -10.18 | -12.00 | -12.00 |

(Target −21 dBFS; input level = sine RMS in dBFS.)

### T2 band-noise equilibria (300–3000 Hz noise, RMS dBFS; 12 s holds, last 0.3 s; dB)

| Config | -21 | -9 | -21 | -15 | -21 | -27 | -21 | -33 | -33 | -27 | -9 | -15 | -9 | -3 | -33 | -39 | -21 | -21 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| ±12 | -3.9 | -11.8 | -3.8 | -8.4 | -3.9 | +1.2 | -4.0 | +6.6 | +6.4 | +1.1 | -12.0 | -8.5 | -11.9 | -12.0 | -4.6 | +11.9 | -4.1 | -4.0 |
| default ±6 | -2.4 | -6.0 | -2.3 | -6.0 | -2.4 | +2.1 | -2.4 | +6.0 | +6.0 | +2.0 | -6.0 | -6.0 | -6.0 | -6.0 | -3.0 | +6.0 | -2.5 | -2.4 |
| ±3 | -1.3 | -3.0 | -1.2 | -3.0 | -1.3 | +2.8 | -1.3 | +3.0 | +3.0 | +2.7 | -3.0 | -3.0 | -3.0 | -3.0 | -0.6 | +3.0 | -1.4 | -1.3 |
| ±12 Fast | -5.6 | -12.0 | -5.5 | -10.0 | -5.6 | -0.6 | -5.7 | +4.7 | +4.4 | -0.7 | -12.0 | -10.2 | -12.0 | -12.0 | -6.9 | +10.0 | -5.8 | -5.8 |
| ±6 Fast | -3.8 | -6.0 | -3.7 | -6.0 | -3.8 | +0.5 | -3.8 | +5.3 | +5.0 | +0.4 | -6.0 | -6.0 | -6.0 | -6.0 | -4.8 | +6.0 | -3.9 | -3.9 |
| ±12, Target −12 | +3.9 | -6.1 | +4.0 | -1.5 | +3.9 | +9.6 | +3.8 | +12.0 | +12.0 | +9.4 | -6.3 | -1.7 | -6.3 | -10.2 | +2.1 | +12.0 | +3.7 | +3.8 |
| ±12, Target −33 | -11.9 | -12.0 | -11.8 | -12.0 | -11.9 | -8.3 | -12.0 | -4.0 | -4.2 | -8.4 | -12.0 | -12.0 | -12.0 | -12.0 | -9.9 | +0.7 | -12.0 | -11.9 |

Columns are consecutive 12 s segments (the stimulus alternates levels; each level is approached from the previous one).

### T3 step trajectories (band noise, ±12 range, Slow and Fast; gain dB at time after the step)

| Mode | Step | 0.01s | 0.03s | 0.06s | 0.1s | 0.2s | 0.3s | 0.5s | 1s | 1.5s | 2s | 3s | 4s | 6s |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Slow | -21→-9 | -4.6 | -5.9 | -7.5 | -8.6 | -10.3 | -11.1 | -11.5 | -11.8 | -12.0 | -12.0 | -11.8 | -11.9 | -11.9 |
| Slow | -9→-21 | -11.9 | -11.9 | -11.9 | -11.8 | -11.8 | -11.7 | -11.6 | -11.2 | -10.5 | -9.5 | -6.2 | -4.0 | -4.0 |
| Slow | -21→-15 | -4.0 | -4.7 | -5.6 | -6.1 | -7.2 | -7.6 | -8.0 | -8.3 | -8.3 | -8.3 | -8.4 | -8.2 | -8.4 |
| Slow | -15→-21 | -8.3 | -8.2 | -8.2 | -8.1 | -7.8 | -7.5 | -6.9 | -5.0 | -4.1 | -4.0 | -3.9 | -4.0 | -3.8 |
| Slow | -21→-27 | -3.9 | -3.9 | -3.8 | -3.7 | -3.3 | -3.0 | -2.3 | -0.1 | +0.9 | +1.1 | +1.1 | +1.1 | +1.1 |
| Slow | -27→-21 | +1.0 | +0.3 | -0.5 | -1.3 | -2.3 | -2.8 | -3.6 | -3.7 | -3.9 | -4.0 | -3.9 | -3.9 | -4.0 |
| Slow | -21→-33 | -4.1 | -4.0 | -4.0 | -4.0 | -3.9 | -3.8 | -3.7 | -3.0 | -2.2 | -0.8 | +3.4 | +6.4 | +6.4 |
| Slow | -33→-27 | +6.0 | +5.3 | +4.6 | +4.0 | +2.8 | +1.9 | +1.7 | +1.1 | +1.1 | +1.3 | +1.1 | +1.0 | +1.1 |
| Slow | -33→-39 | -4.2 | -4.1 | -4.1 | -4.1 | -4.0 | -3.8 | -3.6 | -2.7 | -1.5 | +0.2 | +5.0 | +10.4 | +11.9 |
| Slow | -39→-21 | +10.9 | +8.5 | +5.5 | +2.6 | -0.9 | -2.3 | -3.3 | -4.0 | -3.9 | -3.8 | -3.8 | -3.9 | -3.9 |
| Fast | -21→-9 | -7.7 | -10.1 | -11.2 | -11.8 | -12.0 | -12.0 | -12.0 | -12.0 | -12.0 | -12.0 | -12.0 | -12.0 | -12.0 |
| Fast | -9→-21 | -12.0 | -12.0 | -12.0 | -12.0 | -12.0 | -12.0 | -12.0 | -11.9 | -11.7 | -11.2 | -9.4 | -6.1 | -5.8 |
| Fast | -21→-15 | -5.9 | -7.1 | -8.1 | -8.7 | -9.3 | -9.4 | -9.5 | -9.8 | -9.8 | -9.9 | -10.0 | -9.6 | -9.8 |
| Fast | -15→-21 | -9.7 | -9.7 | -9.6 | -9.6 | -9.4 | -9.2 | -8.8 | -7.5 | -6.1 | -5.7 | -5.8 | -5.7 | -5.6 |
| Fast | -21→-27 | -5.7 | -5.7 | -5.6 | -5.5 | -5.3 | -5.1 | -4.6 | -3.0 | -1.0 | -0.8 | -0.9 | -0.7 | -0.8 |
| Fast | -27→-21 | -1.5 | -2.8 | -3.9 | -4.4 | -4.9 | -5.1 | -5.6 | -5.5 | -5.6 | -5.8 | -5.7 | -5.7 | -5.7 |
| Fast | -21→-33 | -5.8 | -5.8 | -5.7 | -5.7 | -5.7 | -5.7 | -5.6 | -5.4 | -5.1 | -4.5 | -2.3 | +1.5 | +4.6 |
| Fast | -33→-27 | +3.4 | +2.3 | +1.3 | +0.9 | -0.0 | -0.7 | -0.6 | -1.2 | -1.1 | -0.9 | -0.8 | -0.8 | -0.8 |
| Fast | -33→-39 | -6.7 | -6.7 | -6.7 | -6.7 | -6.7 | -6.7 | -6.7 | -6.6 | -6.5 | -6.3 | -5.4 | -3.3 | +5.2 |
| Fast | -39→-21 | +6.0 | +0.3 | -2.7 | -3.8 | -4.9 | -5.5 | -5.9 | -5.7 | -5.7 | -5.6 | -5.6 | -5.4 | -5.7 |

### T4 idle behaviour (±12; signal falls to −80 dBFS noise after a burst; gain dB vs time after the fall)

| Config | Burst | 0.1s | 0.25s | 0.5s | 0.75s | 1s | 1.5s | 2s | 2.5s | 3s | 4s |
|---|---|---|---|---|---|---|---|---|---|---|---|
| Idle 0, Slow | -9 | -11.91 | -11.91 | -11.91 | -10.97 | -9.92 | -7.56 | -4.89 | -1.96 | +0.00 | +0.00 |
| Idle 0, Slow | -33 | +6.69 | +6.69 | +6.69 | +5.02 | +3.35 | +0.13 | +0.00 | +0.00 | +0.00 | +0.00 |
| Idle 0, Slow | -21 | -3.70 | -3.70 | -3.70 | -2.24 | -0.70 | +0.00 | +0.00 | +0.00 | +0.00 | +0.00 |
| Idle +6, Slow | -9 | -11.91 | -11.91 | -11.91 | -10.97 | -9.92 | -7.56 | -4.89 | -1.96 | +1.16 | +6.00 |
| Idle +6, Slow | -33 | +6.69 | +6.69 | +6.69 | +6.00 | +6.00 | +6.00 | +6.00 | +6.00 | +6.00 | +6.00 |
| Idle +6, Slow | -21 | -3.70 | -3.70 | -3.70 | -2.24 | -0.70 | +2.49 | +5.81 | +6.00 | +6.00 | +6.00 |
| Idle −6, Slow | -9 | -11.91 | -11.91 | -11.91 | -10.97 | -9.92 | -7.56 | -6.00 | -6.00 | -6.00 | -6.00 |
| Idle −6, Slow | -33 | +6.69 | +6.69 | +6.69 | +5.02 | +3.35 | +0.13 | -2.94 | -5.79 | -6.00 | -6.00 |
| Idle −6, Slow | -21 | -3.70 | -3.70 | -3.70 | -5.11 | -6.00 | -6.00 | -6.00 | -6.00 | -6.00 | -6.00 |
| Idle 0, Fast | -9 | -11.83 | -11.26 | -10.24 | -9.13 | -7.93 | -5.30 | -2.41 | +0.00 | +0.00 | +0.00 |
| Idle 0, Fast | -33 | +4.37 | +3.41 | +1.78 | +0.18 | +0.00 | +0.00 | +0.00 | +0.00 | +0.00 | +0.00 |
| Idle 0, Fast | -21 | -5.27 | -4.43 | -2.97 | -1.47 | +0.00 | +0.00 | +0.00 | +0.00 | +0.00 | +0.00 |

### T5 activity gate (±12; 4 s steps of band noise, falling then rising; gain at end of each step)

| Vocal Sensitivity | -80 | -21 | -27 | -33 | -39 | -45 | -51 | -57 | -63 | -69 | -75 | -69 | -63 | -57 | -51 | -45 | -39 | -33 | -27 | -21 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| −12 | +0.0 | -5.2 | -4.7 | -3.4 | +0.2 | +0.1 | +0.0 | +0.0 | +0.0 | +0.0 | +0.0 | +0.0 | +0.0 | +0.0 | +0.0 | +0.7 | +2.0 | +3.7 | -0.4 | -5.3 |
| −6 | +0.0 | -4.4 | +0.6 | +6.1 | +11.2 | +12.0 | +12.0 | +12.0 | +0.0 | +0.0 | +0.0 | +0.0 | +0.0 | +0.2 | +3.9 | +12.0 | +11.3 | +6.0 | +0.5 | -4.5 |
| 0 | +0.0 | -3.9 | +1.1 | +6.7 | +11.8 | +12.0 | +12.0 | +12.0 | +12.0 | +0.0 | +0.0 | +0.0 | +0.4 | +6.3 | +12.0 | +12.0 | +11.9 | +6.6 | +1.0 | -4.0 |
| +6 | +0.0 | -3.7 | +1.4 | +7.0 | +12.0 | +12.0 | +12.0 | +12.0 | +12.0 | +12.0 | +0.0 | +0.4 | +7.1 | +12.0 | +12.0 | +12.0 | +12.0 | +6.8 | +1.3 | -3.8 |
| +12 | +0.0 | -3.6 | +1.5 | +7.1 | +12.0 | +12.0 | +12.0 | +12.0 | +12.0 | +12.0 | +12.0 | +12.0 | +12.0 | +12.0 | +12.0 | +12.0 | +12.0 | +7.0 | +1.5 | -3.7 |
| 0 (Fast) | +0.0 | -5.6 | -0.7 | +4.4 | +9.9 | +12.0 | +12.0 | +12.0 | +12.0 | +0.0 | +0.0 | +0.0 | +0.4 | +5.9 | +12.0 | +12.0 | +10.0 | +4.6 | -1.0 | -5.9 |

### T6 detector frequency weighting (steady sine at −27 dBFS RMS; effective detector level relative to the 1 kHz sine; ±12 range)

| Frequency (Hz) | 63 | 125 | 250 | 500 | 1000 | 2000 | 4000 | 8000 | 16000 |
|---|---|---|---|---|---|---|---|---|---|
| Relative reading (dB) | ≤ −9 (ride at +12 limit) | −6.6 | −2.4 | −0.5 | 0.0 | −0.2 | −1.4 | −3.4 | −5.6 |

A steady 1 kHz sine is read at its RMS level (−0.08 dB). 300–3000 Hz band noise is read about 3 dB above its RMS
(peak-weighted ballistics, §6); full-band white noise at equal RMS is read about the same as the 1 kHz sine (weighting loss and ballistic gain cancel).


### T7 large level drops (band noise, ±12 range; gain dB at time after the drop; 12 s at the upper level first)

| Mode | Drop | 0.5s | 1s | 2s | 3s | 4s | 5s | 6s | 8s | 10s | 12s | 15s | 19.5s |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Slow | -15->-21 (6 dB) | -7.0 | -5.1 | -4.1 | -3.8 | -3.9 | -3.8 | -3.9 | -3.9 | -3.9 | -3.9 | -4.0 | -3.9 |
| Slow | -15->-27 (12 dB) | -7.9 | -7.3 | -5.2 | -1.2 | +1.2 | +1.2 | +1.1 | +1.2 | +1.3 | +1.0 | +1.4 | +1.2 |
| Slow | -9->-27 (18 dB) | -11.9 | -11.9 | -11.8 | -11.6 | -11.0 | -9.4 | -6.2 | +0.9 | +1.2 | +1.3 | +1.0 | +0.9 |
| Slow | -9->-33 (24 dB) | -12.0 | -10.0 | -5.0 | -2.6 | -8.2 | -8.2 | -8.1 | -6.3 | +0.9 | +6.4 | +6.6 | +6.7 |
| Slow | -9->-36 (27 dB) | -11.8 | -9.7 | -4.7 | +0.0 | -1.3 | -5.4 | -6.2 | -5.8 | -2.8 | +6.5 | +9.3 | +9.2 |
| Slow | -6->-36 (30 dB) | -12.0 | -10.0 | -5.0 | +0.0 | +0.0 | +0.0 | +0.0 | -5.7 | -5.2 | -2.0 | +9.1 | +9.4 |
| Slow | -3->-39 (36 dB) | -12.0 | -10.0 | -5.0 | +0.0 | +0.0 | +0.0 | +0.0 | +0.0 | +0.0 | -3.8 | -1.4 | +11.9 |
| Fast | -15->-21 (6 dB) | -8.9 | -7.6 | -5.7 | -5.4 | -5.6 | -5.3 | -5.8 | -5.7 | -5.9 | -5.5 | -5.7 | -5.3 |
| Fast | -15->-27 (12 dB) | -9.7 | -9.5 | -8.7 | -6.7 | -3.1 | -0.6 | -0.8 | -0.6 | -0.9 | -0.8 | -0.5 | -0.7 |
| Fast | -9->-27 (18 dB) | -10.2 | -9.5 | -11.8 | -11.7 | -11.6 | -11.1 | -10.0 | -3.6 | -0.7 | -0.6 | -0.8 | -1.1 |
| Fast | -9->-33 (24 dB) | -10.2 | -7.9 | -2.4 | +0.0 | +0.0 | -5.5 | -7.8 | -7.4 | -4.3 | +4.0 | +4.8 | +4.5 |
| Fast | -9->-36 (27 dB) | -10.2 | -7.9 | -2.4 | +0.0 | +0.0 | +0.0 | -0.1 | -5.6 | -4.7 | +0.6 | +7.2 | +7.1 |
| Fast | -6->-36 (30 dB) | -10.2 | -7.9 | -2.4 | +0.0 | +0.0 | +0.0 | +0.0 | -2.5 | -6.0 | -5.0 | +4.7 | +7.4 |
| Fast | -3->-39 (36 dB) | -10.2 | -7.9 | -2.4 | +0.0 | +0.0 | +0.0 | +0.0 | +0.0 | +0.0 | -3.9 | -2.6 | +10.2 |

Stimulus: 300–3000 Hz band noise; each pair is 12 s at the upper level then 20 s at the lower level, pairs in the
order listed, Target −21, Range ±12, Idle 0. Values of 0.0 are Idle.

## 13. User interface (functional layout; visuals must be original)
- Top: Target as a horizontal bar/slider with a level indicator of the detected vocal level; Ride/Idle status
  lights; Attack Fast/Slow switch; knobs Vocal Sensitivity and Music Sensitivity (+ Spill in Live); a music-activity
  indicator whose brightness follows the sidechain influence.
- Main: three vertical faders — Range (two handles: Max and Min, plus an Idle arrow marker between them, with an
  "effective range" readout), the large Rider fader (±12 dB, moves automatically, shows g; a band marks the range),
  and Output (±12 dB) with a clip indicator.
- Bottom: Automation mode selector Off / Write / Read.
- All values editable by typing; double-click resets. Original artwork, colours and fonts only; no vendor branding.

## 14. Open questions
1. Exact structure of the activity detector (relative-threshold reference, onset confirmation) — tables govern.
2. Music sidechain behaviour (not measured; §8 rule is a decision).
3. Spill (Live) not measured.
4. Behaviour of channels with different content (only L = R measured).
5. Vocal Sensitivity −12 erratic behaviour (not reproduced).

## Changes from r1
- §7.2 rewritten: the detector is not frozen while Idle; after a relative-threshold Idle the ride resumes when the
  level returns within ≈18–21 dB of the decaying Lref and the detector restarts from Lref (measured dip, then
  release); after an absolute-threshold Idle it restarts from the current input. (r1 left this open; an
  implementation that froze the detector during Idle failed T4 by up to 6.5 dB and T2 column "−3 → −33".)
- New normative table T7 (large-drop trajectories, 6–36 dB, Slow and Fast) with T3-style tolerance.
- §11 adds the T7 tolerance; §10 adds D7. All r1 tables are unchanged.
