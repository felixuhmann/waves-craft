#!/usr/bin/env python3
"""Design aid (not part of the build): fits the output response (spec table T8) with
    flat gain k (dB) * 1st-order high-pass (fc) * 1st-order high shelf (corner, gain dB)
evaluated at 44.1 / 48 / 96 kHz (bilinear with pre-warping, as implemented in source/dsp/Filters.h).
Prints the constants copied into source/dsp/OutputStage.h and the worst error per rate.
"""
import numpy as np
from scipy.optimize import least_squares

F = np.array([20, 30, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 15000, 18000, 20000, 22000.0])
T = np.array([-0.27, -0.17, -0.10, -0.05, -0.03, -0.03, -0.05, -0.09, -0.34, -0.69, -0.69, -0.59, -0.52, -0.48])


def hp1(fc, fs, f):
    k = np.tan(np.pi * fc / fs)
    b0 = 1 / (1 + k)
    a1 = (k - 1) / (k + 1)
    z = np.exp(-2j * np.pi * f / fs)
    return b0 * (1 - z) / (1 + a1 * z)


def shelf1(fc, gdb, fs, f):
    # 1st-order high shelf H(s) = (G s + w) / (s + w), bilinear with the corner pre-warped
    G = 10 ** (gdb / 20)
    k = np.tan(np.pi * fc / fs)
    z = np.exp(-2j * np.pi * f / fs)
    return ((G + k) + (k - G) * z) / ((1 + k) + (k - 1) * z)


def resp(p, fs, f):
    k, fc, sf, sg = p
    return k + 20 * np.log10(np.abs(hp1(fc, fs, f) * shelf1(sf, sg, fs, f)))


RATES = [44100.0, 48000.0, 96000.0]


def res(p):
    r = []
    for fs in RATES:
        m = F < 0.48 * fs
        r.append((resp(p, fs, F[m]) - T[m]))
    return np.concatenate(r)


s = least_squares(res, [-0.03, 5, 5000, -0.7])
print("k_dB, hpHz, shelfHz, shelfDb =", ", ".join(f"{v:.6g}" for v in s.x))
for fs in RATES:
    m = F < 0.48 * fs
    e = resp(s.x, fs, F[m]) - T[m]
    print(f"{fs:.0f}: max |err| {np.max(np.abs(e)):.3f} dB", np.round(e, 3))
