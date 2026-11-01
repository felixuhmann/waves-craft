#!/usr/bin/env python3
"""Offline fits for Craft Opto Compressor (development aid, needs numpy + scipy; results are pasted into model.json).

Usage: fit_filters.py <spec tables dir> <output json>

Fits, against spec r1 tables only:
  * the quadrature all-pass network of the envelope detector (equal-ripple 90 degree phase difference),
  * the low-level colouration response (tables/freq_response.csv),
  * the detector weighting W(f, Hi Freq) (tables/hifreq_gain.csv).
"""
import collections
import csv
import json
import sys

import numpy as np
from scipy.optimize import least_squares

TABLES, OUT = sys.argv[1], sys.argv[2]


# ---------------------------------------------------------------------------------------------
# Quadrature network
def ap_phase(c, w):
    z = np.exp(1j * w)
    return np.unwrap(np.angle((c + 1 / z) / (1 + c / z)))


def hilbert_poles(x, fs):
    w1, w2 = np.tan(np.pi * 12.0 / fs), np.tan(np.pi * 0.47 * fs / fs)
    p = w1 * (w2 / w1) ** np.asarray(x)
    return (p - 1) / (p + 1)


def hilbert_err(x, fs):
    w = 2 * np.pi * np.geomspace(20.0, min(20000.0, 0.45 * fs), 500) / fs
    c = hilbert_poles(np.sort(x), fs)
    a = sum(ap_phase(k, w) for k in c[0::2])
    b = sum(ap_phase(k, w) for k in c[1::2])
    d = a - b
    return d - np.pi / 2 * np.sign(np.mean(d))


NH = 16
hx = least_squares(lambda x: np.concatenate([hilbert_err(x, fs) for fs in (44100.0, 48000.0, 96000.0)]),
                   (np.arange(NH) + 0.5) / NH).x
hx = np.sort(hx)
for fs in (44100.0, 48000.0, 96000.0, 192000.0):
    print("quadrature error deg at", fs, np.degrees(np.abs(hilbert_err(hx, fs)).max()))


# ---------------------------------------------------------------------------------------------
# Biquad helpers (RBJ forms; prewarped, valid at every rate)
def bell(f0, q, db, fs):
    A = 10 ** (db / 40)
    w = 2 * np.pi * f0 / fs
    al = np.sin(w) / (2 * q)
    return np.array([1 + al * A, -2 * np.cos(w), 1 - al * A]), np.array([1 + al / A, -2 * np.cos(w), 1 - al / A])


def lowshelf(f0, db, fs):
    A = 10 ** (db / 40)
    w = 2 * np.pi * f0 / fs
    al = np.sin(w) / np.sqrt(2)
    c, r = np.cos(w), 2 * np.sqrt(A) * al
    return (np.array([A * ((A + 1) - (A - 1) * c + r), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - r)]),
            np.array([(A + 1) + (A - 1) * c + r, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - r]))


def resp(ba, f, fs):
    z = np.exp(-2j * np.pi * np.asarray(f) / fs)
    b, a = ba
    return (b[0] + b[1] * z + b[2] * z * z) / (a[0] + a[1] * z + a[2] * z * z)


def matched_hf(fz, qz, fp, qp, fs):
    """Second-order HF section: matched poles, numerator magnitude-matched at three frequencies (Vicanek's form)."""
    def analog2(f):
        s = 1j * np.asarray(f)
        return np.abs((s * s + s * fz / qz + fz * fz) / (s * s + s * fp / qp + fp * fp) * (fp * fp) / (fz * fz)) ** 2
    wp = 2 * np.pi * fp / fs
    rp = np.exp(-wp / (2 * qp))
    a = np.array([1.0, -2 * rp * np.cos(wp * np.sqrt(max(1 - 1 / (4 * qp * qp), 0.0))), rp * rp])
    pts = np.array([0.0, 0.25, 0.47]) * fs if fs < 60000 else np.array([0.0, 0.11, 0.22]) * fs
    sn = np.sin(np.pi * pts / fs) ** 2
    phi = np.stack([1 - sn, sn, 4 * (1 - sn) * sn], axis=1)
    den = phi @ np.array([(a[0] + a[1] + a[2]) ** 2, (a[0] - a[1] + a[2]) ** 2, -4 * a[2]])
    A = np.linalg.solve(phi, analog2(pts) * den)
    W = 0.5 * (np.sqrt(max(A[0], 0)) + np.sqrt(max(A[1], 0)))
    b0 = 0.5 * (W + np.sqrt(W * W + A[2]))
    return np.array([b0, 0.5 * (np.sqrt(max(A[0], 0)) - np.sqrt(max(A[1], 0))), -A[2] / (4 * b0)]), a


# ---------------------------------------------------------------------------------------------
# Colouration response
rows = sorted((float(r["freq_hz"]), float(r["gain_dB_re_1k"])) for r in csv.DictReader(open(TABLES + "/freq_response.csv")))
fr, gr = np.array(rows).T


def colour_db(p, f, fs):
    h = resp(bell(p[0], p[1], p[2], fs), f, fs) * resp(bell(p[3], p[4], p[5], fs), f, fs) * resp(matched_hf(p[6], p[7], p[8], p[9], fs), f, fs)
    return 20 * np.log10(np.abs(h))


def colour_resid(p):
    out = []
    for fs in (44100.0, 48000.0, 96000.0):
        d = colour_db(p, fr, fs)
        out.append(d - colour_db(p, [1000.0], fs)[0] - gr)
    return np.concatenate(out)


c0 = [38.0, 0.6, -0.3, 300.0, 1.0, 0.0, 16000.0, 0.7, 25000.0, 0.7]
cs = least_squares(colour_resid, c0, bounds=([15, .2, -2, 100, .2, -.5, 3e3, .2, 8e3, .2], [80, 3, 0, 1000, 3, .5, 3e4, 3, 6e4, 3])).x
print("colour", np.round(cs, 4), "max err", np.abs(colour_resid(cs)).max())

# ---------------------------------------------------------------------------------------------
# Detector weighting: equivalent level offset per table row, then filter fit
st = collections.defaultdict(list)
for r in csv.DictReader(open(TABLES + "/static_gr.csv")):
    if r["mode"] == "C" and r["pr"] == "6":
        st["l"].append(float(r["level_dBFS_sine_peak"]))
        st["g"].append(float(r["gain_dB"]))
sl, sg = np.array(st["l"]), np.array(st["g"])
# detector levels above the table's 0 dBFS row continue with the slope of the last 4 dB (same rule as the engine)
sl, sg = np.append(sl, 24.0), np.append(sg, sg[-1] + 24.0 * (sg[-1] - sg[-3]) / (sl[-1] - sl[-3]))
hf = collections.defaultdict(list)
for r in csv.DictReader(open(TABLES + "/hifreq_gain.csv")):
    hf[(float(r["freq_hz"]), int(r["hifreq"]))].append((float(r["level_dBFS_sine_peak"]), float(r["gain_dB_pr6_comp"])))
keys = sorted(hf)
cf = {f: colour_db(cs, [f], 48000.0)[0] - colour_db(cs, [1000.0], 48000.0)[0] + 0.015 for f, _ in keys}


def wdb(p, f, h, fs=48000.0):
    base = resp(lowshelf(p[0], p[1], fs), f, fs) * resp(bell(p[2], p[3], p[4], fs), f, fs)
    al = np.interp(h, [0, 25, 50, 75, 100], [p[6], p[7], p[8], p[9], 0.0])
    # complementary one-pole high-pass, parallel with the dry path
    z = np.exp(-2j * np.pi * f / fs)
    k = np.tan(np.pi * p[5] / fs)
    a1 = (k - 1) / (k + 1)
    hp = (1 - z) / (1 + k) / (1 + a1 * z)
    return 20 * np.log10(np.abs(base * (1 + al * hp)))


def band_drop(p, f, fs=48000.0):
    """Level-dependent narrowing of the weighting: 0 between 1 and 3 kHz, growing towards both band edges."""
    w0 = 2 * np.pi * 1732.0 / fs
    al = np.sin(w0) / (2 * p[10])
    ba = np.array([al, 0, -al]), np.array([1 + al, -2 * np.cos(w0), 1 - al])
    r = np.abs(resp(ba, f, fs)) / np.abs(resp(ba, 1000.0, fs))
    return p[11] * max(0.0, 1.0 - float(r) - 0.03) ** p[12]


def hf_resid(p):
    out = []
    for fs in (44100.0, 48000.0, 96000.0):
        n = wdb(p, 1000.0, 100, fs)
        for f, h in keys:
            w = wdb(p, f, h, fs) - n
            d = band_drop(p, f, fs) + p[14] * max(0.0, w - (wdb(p, f, 100, fs) - n))
            for l, g in hf[(f, h)]:
                dep = np.clip((l + w + p[13]) / p[13], 0.0, 1.0)
                pred = np.interp(l + w - d * dep, sl, sg) + cf[f]
                if g < -0.3:
                    e = (pred - g) * (0.4 if (f == 8000 and h == 0) else 1.0)
                    out.append(np.sign(e) * (abs(e) + 8 * e * e))  # leans towards minimax
    return np.array(out)


h0 = [600.0, -0.9, 3000.0, 0.6, 1.5, 5000.0, 2.5, 1.5, 0.5, 0.1, 0.7, 2.0, 1.0, 30.0, 0.1]
hs = least_squares(hf_resid, h0, bounds=([100, -4, 800, .2, 0, 500, 0, 0, 0, 0, .2, 0, .3, 6, 0], [3000, 0, 10000, 4, 6, 20000, 10, 10, 10, 10, 3, 8, 3, 60, 0.4])).x
print("hifreq", np.round(hs, 4), "max", np.abs(hf_resid(hs)).max())
for f, h in keys:
    w = wdb(hs, f, h) - wdb(hs, 1000.0, 100)
    d = band_drop(hs, f) + hs[14] * max(0.0, w - (wdb(hs, f, 100) - wdb(hs, 1000.0, 100)))
    err = [float(np.interp(l + w - d * np.clip((l + w + hs[13]) / hs[13], 0, 1), sl, sg) + cf[f] - g) for l, g in hf[(f, h)] if g < -0.3]
    print(int(f), h, round(float(w), 2), round(d, 2), "max err", round(max(map(abs, err)), 2))

json.dump({"hilbert_x": list(hx), "colour": list(cs), "hifreq": list(hs)}, open(OUT, "w"), indent=1)
