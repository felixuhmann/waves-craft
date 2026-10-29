#!/usr/bin/env python3
"""Design aid (not part of the build): fits the memoryless tube-colour curve to spec tables T7 (PR 0) / T7b.

Colour curve on the post-make-up signal x (per channel):
    y = x + b * Xe * (sqrt(1 + (x/Xe)^2) - 1) + c * Xo * u|u| / (1 + |u|) + d * Xd * v^2.5 / (1 + v^1.5)
    u = x / Xo, v = |x / Xd|
The even term gives H2 growing 1 dB/dB at low level and saturating at high level (T7b), the odd term gives H3/H5
growing 1 dB/dB (H5 17 dB under H3, as u|u| does) and the small level-dependent gain rise of T7; the |x|^2.5 term
sets the H4 slope (about 1.5 dB/dB).
Prints the fitted constants and the residuals; the constants are copied into source/dsp/Colour.h.
"""
import numpy as np
from scipy.optimize import least_squares

N = 4096
th = 2 * np.pi * np.arange(N) / N


def harm(f, A):
    y = f(A * np.sin(th))
    Y = np.fft.rfft(y) / N * 2
    return np.abs(Y[1:6])


def mk(p):
    b, xe, c, xo, d, xd = p

    def f(x):
        ue = x / xe
        uo = x / xo
        ud = np.abs(x / xd)
        return (x + b * xe * (np.sqrt(1 + ue * ue) - 1) + c * xo * uo * np.abs(uo) / (1 + np.abs(uo))
                + d * xd * ud ** 2.5 / (1 + ud ** 1.5))

    return f


T2 = {0: -51.36, 10: -34.95, 20: -18.06, 32.28: -0.05, 40: 8.45, 50: 16.85, 60: 24.65, 80: 33.66, 100: 38.67}
rows = []  # (output peak level dBFS, harmonic number, target dBc)
t7 = {-40: (-78.8, -83.8, -130.3, -100.5), -30: (-69.1, -74.3, -110.8, -90.5), -18: (-58.5, -64.3, -89.2, -78.9),
      -10: (-52.8, -59.8, -77.1, -72.3), -3: (-49.3, -58.1, -69.2, -68.4), 0: (-48.2, -58.1, -66.7, -67.6)}
for L, v in t7.items():
    for k, t in enumerate(v):
        if t > -95:
            rows.append((L, k + 2, t))
t7b = {0: (-101.4, -103.6), 10: (-83.6, -88.7), 20: (-67.3, -72.5), 40: (-48.7, -58.0), 50: (-46.7, -60.0),
       60: (-46.0, -64.4), 80: (-45.7, -71.5), 100: (-45.7, -76.0)}
for g, (h2, h3) in t7b.items():
    L = -10 + T2[g]
    rows.append((L, 2, h2))
    rows.append((L, 3, h3))
# fundamental gain vs level relative to small signal (T7 PR 0, 1 kHz: -0.04 at -40 ... +0.06 at 0)
gainT = {-40: 0.0, -30: 0.0, -18: 0.02, -10: 0.05, -3: 0.09, 0: 0.10}


def res(p):
    f = mk(p)
    r = []
    for L, k, t in rows:
        H = harm(f, 10 ** (L / 20))
        r.append((20 * np.log10(H[k - 1] / H[0] + 1e-30) - t) / 3.0)
    for L, t in gainT.items():
        A = 10 ** (L / 20)
        H = harm(f, A)
        r.append((20 * np.log10(H[0] / A) - t) / 0.03)
    return np.array(r)


best = None
for xe0 in [0.2, 0.5, 1, 2]:
    for xo0 in [0.2, 0.5, 1, 2]:
        s = least_squares(res, [0.01, xe0, 0.01, xo0, 0.0, 0.5], bounds=([-1, 0.01, -1, 0.01, -1, 0.01], [1, 100, 1, 100, 1, 100]))
        if best is None or s.cost < best.cost:
            best = s
p = best.x
print("b, Xe, c, Xo, d, Xd =", ", ".join(f"{v:.6g}" for v in p), " cost", best.cost)
f = mk(p)
for L, k, t in rows:
    H = harm(f, 10 ** (L / 20))
    print(f"out {L:7.2f} dBFS  H{k}  target {t:7.1f}  fit {20 * np.log10(H[k - 1] / H[0]):7.1f}")
for L, t in gainT.items():
    A = 10 ** (L / 20)
    H = harm(f, A)
    print(f"out {L:5.1f} dBFS  gain target {t:+.3f}  fit {20 * np.log10(H[0] / A):+.3f}")
