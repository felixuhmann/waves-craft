#!/usr/bin/env python3
"""Fits the analog filter prototypes of the Craft Dynamic Distortion to the spec r1 response tables.

Development aid (numpy + scipy). Prints the constants used in source/dsp/Filters.h and the residuals.
  Bass:   RBJ low shelf (corner, Q) fitted to bass -50 / -25 jointly (bass +25/+50 are the mirrored cuts).
  Mid:    bell with fixed zeros (s^2 + s/Qn + 1) / (s^2 + s/(g Qn) + 1), per Mid Freq anchor (centre, Qn).
  Treble: same bell family fitted to treble -50 and the mirrored treble +25 jointly.
  HP:     second-order x first-order high-pass fitted to the section 4.6 HP table (relative to 1 kHz).
  Clip emphasis: first-order shelf pair and clip ceiling fitted to the fully clipped fundamentals of the
          section 6.1 Drive 100 % / 0 dBFS row (50 / 200 / 500 / 2000 Hz).
"""
import numpy as np
from scipy.optimize import least_squares

F = np.array([20, 50, 100, 200, 500, 1000, 2000, 5000, 9000, 12000, 15000, 20000.0])
T = {
    'bass-50': [-16.51, -12.89, -9.00, -5.11, -1.48, -0.43, -0.11, -0.02, -0.00, -0.00, -0.00, +0.00],
    'bass-25': [-8.31, -6.48, -4.50, -2.52, -0.68, -0.19, -0.05, -0.01, -0.00, -0.00, +0.00, +0.00],
    'mid-50_f150': [-1.09, -4.93, -12.42, -14.36, -4.22, -1.34, -0.37, -0.06, -0.02, -0.01, -0.00, -0.00],
    'mid-50_f1252': [-0.05, -0.31, -1.12, -3.42, -9.92, -16.95, -14.48, -6.03, -2.62, -1.57, -1.01, -0.59],
    'mid-50_f5000': [-0.00, -0.03, -0.11, -0.44, -2.22, -5.72, -11.14, -18.00, -13.94, -10.99, -8.78, -6.43],
    'treble-50': [-0.02, -0.10, -0.40, -1.40, -5.21, -9.76, -14.16, -17.47, -18.00, -17.85, -17.43, -16.16],
    'treble25': [-3.00, -2.99, -2.95, -2.82, -2.00, -0.11, +2.80, +5.54, +6.00, +5.87, +5.50, +4.41],
}
T = {k: np.array(v) for k, v in T.items()}


def db(h):
    return 20 * np.log10(np.abs(h))


def low_shelf(f, f0, q, gain_db):
    a = 10 ** (gain_db / 40)
    s = 1j * f / f0
    return a * (s * s + np.sqrt(a) / q * s + a) / (a * s * s + np.sqrt(a) / q * s + 1)


def bell(f, f0, qn, depth_db):
    g = 10 ** (-depth_db / 20)
    s = 1j * f / f0
    return (s * s + s / qn + 1) / (s * s + s / (g * qn) + 1)


def show(name, p, r):
    print(f"{name}: {np.round(p, 5)}  max |error| {np.max(np.abs(r)):.3f} dB")


# Bass
r = lambda p: np.concatenate([db(low_shelf(F, p[0], p[1], -18)) - T['bass-50'], db(low_shelf(F, p[0], p[1], -9)) - T['bass-25']])
s = least_squares(r, [100, 0.7]); show('bass (f0, Q)', s.x, r(s.x))

# Mid anchors
for key, fc in [('mid-50_f150', 150), ('mid-50_f1252', 1252), ('mid-50_f5000', 5000)]:
    r = lambda p: db(bell(F, p[0], p[1], 18)) - T[key]
    s = least_squares(r, [fc, 1.0]); show(f'mid {fc} (centre, Qn) centre ratio {s.x[0] / fc:.5f}', s.x, r(s.x))

# Treble (cut -50 and the cut implied by the mirrored +25: -(table + 3))
r = lambda p: np.concatenate([db(bell(F, p[0], p[1], 18)) - T['treble-50'], db(bell(F, p[0], p[1], 9)) + (T['treble25'] + 3)])
s = least_squares(r, [8000, 0.33]); show('treble (centre, Qn)', s.x, r(s.x))

# HP
FH = np.array([20, 30, 50, 70, 100, 150, 200.0])
TH = np.array([-7.17, -1.85, -0.35, -0.14, -0.06, -0.02, -0.01])


def hp(f, f2, q, f1):
    s2, s1 = 1j * f / f2, 1j * f / f1
    return s2 * s2 / (s2 * s2 + s2 / q + 1) * s1 / (s1 + 1)


r = lambda p: db(hp(FH, *p)) - db(hp(np.array([1000.0]), *p)) - TH
s = least_squares(r, [30, 0.7, 20], bounds=(0.01, 1000)); show('hp (f2, Q, f1)', s.x, r(s.x))
hp_p = s.x

# Clip emphasis: fully clipped fundamental = square of the ceiling through the de-emphasis, HP and LP 21 kHz
fr = np.array([50, 200, 500, 2000.0])
tgt = np.array([-10.9, -8.7, -8.1, -8.0])  # section 6.1, Drive 100 %, 0 dBFS (gain = output level)


def ceiling_db(p, f):
    c, fz, fp = p
    post = (1j * f + fz) / (1j * f + fp)
    lp = 1 / (1 + 1j * f / 21000.0) ** 2
    return db(4 * c / np.pi / np.sqrt(2) * post * hp(f, *hp_p) * lp)


r = lambda p: ceiling_db(p, fr) - tgt
s = least_squares(r, [0.44, 85, 125], bounds=([0.1, 1, 1], [1, 5000, 5000]))
show('clip (ceiling, zero Hz, pole Hz)', s.x, r(s.x))
print(f"  low-frequency ceiling c = {s.x[0] * s.x[1] / s.x[2]:.4f} ({20 * np.log10(s.x[0] * s.x[1] / s.x[2]):.2f} dBFS)")
