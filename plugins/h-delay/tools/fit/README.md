# Offline fits (development aid, not built)

Python 3 + NumPy/SciPy scripts that produced the constants in `source/dsp/` from the tables of
`spec/h-delay/r1/SPEC.md`. They only use the spec's numbers; run them from this directory.

| Script | Produces | Notes |
|---|---|---|
| `lofi_fit2.py 8 58 9000` | `LoFi.h` sections | starts from an 8th-order inverse-Chebyshev prototype (uses `lofi_fit.py` for the model) |
| `mod_fit3.py slewfirst` | `Modulation.h` constants | grid search over slope limit, low-pass and leak |
| `analog_fit.py <mode> <trials> <nfev>` | `Analog.h` modes 1, 2, 4 | model of the Analog chain + saturation, least squares on the §9 tables |
| `refine3.py` | `Analog.h` mode 3 | refinement of mode 3 (requires `analog_fit.py`) |
