# Verification — Craft Console E against spec r2 (build da39fcf)

Same control positions and stimuli on build and reference; differences only.

| Area | Result |
|---|---|
| Tapers and displays | Pass: 936 displayed values identical |
| EQ at 48 kHz | Pass, except r = 0.35 low-pass (1.87 dB, off-grid; r3 C.1), high-pass r = 0.65 (0.77 dB at 20 Hz; r3 C.1), one row 0.52 dB |
| Low-pass, 44.1 kHz | 0.58–0.69 dB near 12 kHz at r = 0.3–0.4 |
| Low-pass, 96 kHz | **Fail: build up to 3.4 dB brighter** (r = 0.4); rate-scaled model does not match the reference |
| HF bell 16 kHz, 96 kHz | **Fail: up to 2.08 dB too wide at 3–8 kHz**; the 16–20 kHz blend is within 0.4 dB |
| §6.5 static vs mode/release | Pass: ≤ 0.35 dB |
| §6.6 attack | Pass (fast/4 s t63 1.3× is within the 1 ms measuring resolution) |
| Release timing | Pass |
| §7.1 expander/gate static | Pass: ≤ 0.49 dB. Long-release differences on the lowest staircase steps come from the render's starting state, not behaviour |
| Expander closing vs range/threshold | **Fail: too fast at small ranges** (range 15: 0.21×; range 5: 0.05×); new r3 §7.2 |
| Ratio ∞, threshold −10 | Reproducible reference deviation at the top step only (0.75 dB at +3 dBFS); accepted via r3 tolerance, no table needed |
| §6 vs §6.6 fast attack | §6.6 is correct; r3 §6 defers to it |

## Issues for the builder (spec r3)
1. Low-pass: meet C.1 and C.2 (drop the rate-scaling model).
2. EQ at 44.1/96 kHz: meet C.3, in particular the HF bell at 16 kHz.
3. High-pass at odd positions: C.1.
4. Expander closing time per §7.2.

## Not verified
Linux, CLAP, host automation, GUI, mono, 88.2 kHz and above, dynamics timing at 44.1/96 kHz.
