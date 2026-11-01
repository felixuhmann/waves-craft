#!/usr/bin/env python3
"""Design aid (not part of the build): coefficients of the 90-degree allpass pair used by the level detector.

Two cascades of N second-order allpass sections in z^-2,  A(z) = prod (a_k^2 - z^-2) / (1 - a_k^2 z^-2),
path Q additionally delayed by one sample. Classic polyphase-IIR quadrature design (public technique; see e.g.
R. Lyons, "Understanding DSP", and the allpass-pair literature). The coefficients are optimised here for a phase
difference of 90 degrees over a normalised band wide enough for 10 Hz ... 20 kHz at 44.1 - 192 kHz.
Prints the C++ arrays copied into source/dsp/Detector.h and the worst phase error.
"""
import sys
import numpy as np
from scipy.optimize import least_squares

N = int(sys.argv[1]) if len(sys.argv) > 1 else 6
LO = float(sys.argv[2]) if len(sys.argv) > 2 else 10.0 / 192000.0
HI = 20000.0 / 44100.0
w = 2 * np.pi * np.geomspace(LO, HI, 600)
z1 = np.exp(-1j * w)


def phase(coefs, delay):
    h = np.ones_like(z1)
    for a in coefs:
        a2 = a * a
        h *= (a2 - z1 ** 2) / (1 - a2 * z1 ** 2)
    if delay:
        h *= z1
    return np.unwrap(np.angle(h))


def err(p):
    pa, pb = p[:N], p[N:]
    d = phase(pa, False) - phase(pb, True)
    d = np.mod(d + np.pi, 2 * np.pi) - np.pi
    return np.degrees(np.abs(d) - np.pi / 2)


best = None
rng = np.random.default_rng(1)
for trial in range(40):
    x0 = np.sort(1 - np.geomspace(0.6, 1e-5, 2 * N) * rng.uniform(0.5, 1.5, 2 * N))
    p0 = np.concatenate([x0[0::2], x0[1::2]])
    s = least_squares(err, p0, bounds=(0.0, 0.9999999))
    m = np.max(np.abs(err(s.x)))
    if best is None or m < best[0]:
        best = (m, s.x)
m, p = best
print(f"max phase error {m:.4f} deg over {LO * 44100:.2f} Hz@44.1k .. {HI:.4f} fs")
print("I path:", ", ".join(f"{v:.12f}" for v in sorted(p[:N])))
print("Q path:", ", ".join(f"{v:.12f}" for v in sorted(p[N:])))
