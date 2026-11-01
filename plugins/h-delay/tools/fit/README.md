# Offline fits (development aid, not built)

Python 3 + NumPy/SciPy scripts that produced the constants in `source/dsp/` from the tables of
`spec/h-delay/r1`, `r2` and `r3`. They only use the spec's numbers; run them from this directory.

| Script | Produces | Notes |
|---|---|---|
| `sat_check_r2.py 0.72 8 1.1` | `Saturation.h` | checks the r2 saturation against §4.2–4.4 (weight, release ms, hold ms) |
| `mod_fit_r2.py` | `Modulation.h` (r2) | one-pole tau = k·D + low-pass, grid search on the 12-row table |
| `analog_fit_r3.py <mode> [restarts] [seed]` | `Analog.h` (r3) | adds the §9.2.1 click vectors, 1 Hz DC blocker, no slow compressor, low-band distortion option |
| `analog_fit_r2.py <mode> <restarts> [seed]`, `eval_r2.py` | r2 `Analog.h` (used by r3 script) | chain after the r2 saturation, §9.2 tables |
| `lofi_fit2.py 8 58 9000` | `LoFi.h` sections | starts from an 8th-order inverse-Chebyshev prototype (uses `lofi_fit.py` for the model) |
| `mod_fit3.py slewfirst` | r1 `Modulation.h` (superseded) | grid search over slope limit, low-pass and leak |
| `analog_fit.py <mode> <trials> <nfev>` | r1 `Analog.h` (superseded) | model of the Analog chain + saturation, least squares on the §9 tables |
| `refine3.py` | r1 `Analog.h` mode 3 (superseded) | refinement of mode 3 (requires `analog_fit.py`) |
