#!/usr/bin/env python3
"""Fits the colouration curve (source/dsp/Colouration.h) to the 1 kHz Comp Off rows and spec r2 section 13 item 8.
Development aid, not shipped. Low-level term fixed from the rows: one-sided square law, H2/H3/H5 = 0 / -9.4 / -26 dB."""
import numpy as np
from scipy.optimize import least_squares

N = 4800
ph = 2 * np.pi * 10 * np.arange(N) / N
# x (internal dB), gain change, H2, H3, H5 (H5 weak; None = not given)
DATA = {"A": (0.0100, [(7.07, 0.08, -45.0, -54.4, -71.2), (19.07, -0.82, -17.9, -24.5, -52.5),
                       (31.07, -2.99, -8.8, -31.4, -33.5), (37.07, -6.07, -11.5, -14.8, None)]),
        "B": (0.0082, [(10.58, 0.10, -43.2, -52.5, -69.4), (22.58, -2.33, -11.9, -23.4, -30.8),
                       (34.58, -4.15, -8.8, -23.9, -34.9), (40.58, -8.88, -13.4, -12.4, None)])}


def curve(s, p, a):
    cp, np_, cn, nn, S = p
    c = np.where(s >= 0, cp, cn)
    n = np.where(s >= 0, np_, nn)
    y = s / (1 + np.abs(s / c) ** n) ** (1 / n)
    u2 = s * s / (1 + (s / S) ** 2)
    return y + a * u2 * (s > 0)


def harm(A, p, a):
    Y = np.abs(np.fft.rfft(curve(A * np.sin(ph), p, a))) / N * 2
    return 20 * np.log10(Y[10] / A), [20 * np.log10(max(Y[10 * k], 1e-12) / Y[10]) for k in (2, 3, 5)]


for rev, (a, rows) in DATA.items():
    def unpack(q):
        return [np.exp(q[0]), 1.2 + 8 / (1 + np.exp(-q[1])), np.exp(q[2]), 1.2 + 8 / (1 + np.exp(-q[3])), np.exp(q[4])]

    def res(q):
        r = []
        for x, g, h2, h3, h5 in rows:
            G, H = harm(10 ** (x / 20), unpack(q), a)
            r += [(G - g) / 0.5, (H[0] - h2) / 3, (H[1] - h3) / 3]
            if h5 is not None:
                r.append((H[2] - h5) / 12)
        return r

    best = None
    for c0 in (20, 40, 80):
        for n0 in (5, 12, 25):
            for S0 in (30, 100, 300):
                s = least_squares(res, [np.log(c0), 0, np.log(n0), 0, np.log(S0)], max_nfev=60)
                if best is None or s.cost < best.cost:
                    best = s
    p = unpack(best.x)
    print(rev, "cPos,nPos,cNeg,nNeg,S =", [round(float(v), 4) for v in p], "a =", a, "cost", round(best.cost, 2))
    for x, g, h2, h3, h5 in rows:
        G, H = harm(10 ** (x / 20), p, a)
        print(f"  x {x}: gain {G:.2f} ({g}) H2 {H[0]:.1f} ({h2}) H3 {H[1]:.1f} ({h3}) H5 {H[2]:.1f} ({h5})")
