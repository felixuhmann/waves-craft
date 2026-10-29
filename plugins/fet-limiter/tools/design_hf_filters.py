#!/usr/bin/env python3
"""Fits the top-end roll-off biquads of both revisions to their analog prototypes (spec r1 section 7.1 / 9).

Development aid, not shipped. The analog prototypes (second-order low-pass sections, frequencies in Hz) were fitted
to tables/freq_response.csv; spec 9 asks to keep the analog response at every sample rate rather than scaling it
with fs. A bilinear transform cannot do that near Nyquist, so for each standard sample rate the digital biquads
are least-squares fitted (in dB, 20 Hz .. min(22 kHz, 0.97 fs/2)) to the analog cascade. The printed C++ table
goes into source/dsp/HfCoefficients.h; other sample rates use the impulse-invariant / magnitude-matched design
of source/dsp/Filters.h.
"""
import numpy as np
from scipy.optimize import least_squares

PROTOTYPES = {
    "A": [(25773.757, 0.7243)],
    "B": [(11954.0335, 0.6284), (18254.9179, 1.49)],
}
RATES = [44100, 48000, 88200, 96000, 176400, 192000]


def analog_db(secs, f):
    s = 1j * np.asarray(f, float)
    h = np.ones(len(f), complex)
    for f0, q in secs:
        h *= f0 * f0 / (s * s + s * f0 / q + f0 * f0)
    return 20 * np.log10(np.abs(h))


def digital_db(coefs, f, fs):
    z1 = np.exp(-1j * 2 * np.pi * np.asarray(f, float) / fs)
    h = np.ones(len(f), complex)
    for b0, b1, b2, a1, a2 in coefs:
        h *= (b0 + b1 * z1 + b2 * z1 * z1) / (1 + a1 * z1 + a2 * z1 * z1)
    return 20 * np.log10(np.abs(h))


def bilinear_lp(f0, q, fs):
    k = np.tan(np.pi * min(f0, 0.45 * fs) / fs)
    norm = 1 / (1 + k / q + k * k)
    b0 = k * k * norm
    return [b0, 2 * b0, b0, 2 * (k * k - 1) * norm, (1 - k / q + k * k) * norm]


def fit(rev, fs):
    secs = PROTOTYPES[rev]
    f = np.geomspace(20.0, min(22000.0, 0.97 * fs / 2), 300)
    target = analog_db(secs, f)
    x0 = np.concatenate([bilinear_lp(f0, q, fs) for f0, q in secs])

    def res(x):
        c = x.reshape(-1, 5)
        r = digital_db(c, f, fs) - target
        w = np.where(f < 15000, 3.0, 1.0)
        pen = []
        for b0, b1, b2, a1, a2 in c:  # keep poles inside the unit circle
            pen.append(max(0.0, abs(a2) - 0.98) * 100)
            pen.append(max(0.0, abs(a1) - (1 + a2) + 0.01) * 100)
        return np.concatenate([r * w, pen])

    sol = least_squares(res, x0, max_nfev=20000)
    c = sol.x.reshape(-1, 5)
    err = np.max(np.abs(digital_db(c, f, fs) - target))
    return c, err


if __name__ == "__main__":
    for rev in PROTOTYPES:
        print(f"// revision {rev}")
        for fs in RATES:
            c, err = fit(rev, fs)
            secs = ", ".join("{ " + ", ".join(f"{v:.12g}" for v in s) + " }" for s in c)
            print(f"    {{ {fs}.0, {{ {secs} }} }}, // max error {err:.3f} dB")
