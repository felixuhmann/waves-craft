"""Shared helpers for the fet-limiter development scripts (not shipped).

Loads the spec r1 tables and wraps the C++ gain-cell simulation (tools/simwrap.cpp -> libfetsim.so).
"""
import csv
import ctypes
import math
import pathlib

import numpy as np

HERE = pathlib.Path(__file__).resolve().parent
REPO = HERE.parent.parent.parent
TABLES = REPO / "spec" / "fet-limiter" / "r1" / "tables"
FS = 48000.0


def static_curve(rev, ratio):
    """Settled static curve per spec 5: E rows define the curve where they exist, A rows extend it beyond the
    x range of the E rows (and fill it completely when a curve has no E rows). Returns (x, gr_db)."""
    rows_e, rows_a = {}, {}
    with open(TABLES / "static_gr.csv") as f:
        r = csv.reader(f)
        next(r)
        for rv, ra, x, gr, src in r:
            if rv != rev or ra != ratio:
                continue
            (rows_e if src == "E" else rows_a)[float(x)] = float(gr)
    if rows_e:
        lo, hi = min(rows_e), max(rows_e)
        pts = dict(rows_e)
        for x, g in rows_a.items():
            if x < lo or x > hi:
                pts[x] = g
    else:
        pts = rows_a
    xs = sorted(pts)
    return np.array(xs), np.array([pts[x] for x in xs])


def all_makeup_db(rev):
    return 1.2 if rev == "A" else 0.6


def reduction_curve(rev, ratio):
    """Positive reduction R(x) >= 0 used as the gain-cell target; for All the small-signal makeup is removed."""
    x, g = static_curve(rev, ratio)
    if ratio == "All":
        top = g[0] if rev == "A" else 0.0  # rev A table includes the makeup, rev B does not
        r = np.maximum(0.0, top - g)
    else:
        r = np.maximum(0.0, -g)
    return x, r


def load_timing():
    rows = []
    with open(TABLES / "timing.csv") as f:
        for d in csv.DictReader(f):
            rows.append(d)
    return rows


def load_traj():
    out = {}
    with open(TABLES / "release_trajectories.csv") as f:
        r = csv.reader(f)
        hdr = next(r)
        times = [float(h.split("_at_")[1][:-2]) for h in hdr[2:-1]]
        for row in r:
            out[(row[0], float(row[1]))] = (np.array([float(v) for v in row[2:-1]]), row[-1])
    return times, out


_lib = None


def lib(path):
    global _lib
    if _lib is None:
        _lib = ctypes.CDLL(str(path))
        _lib.fet_sim.restype = ctypes.c_int
    return _lib


def simulate(libpath, params, rev, ratio, x_burst, burst_ms, post_ms=5200.0, pre_ms=300.0, fs=FS):
    sx, sr = reduction_curve(rev, ratio)
    n = int((burst_ms + post_ms) * 0.001 * fs) + 4
    out = np.zeros(n)
    p = np.ascontiguousarray(params, dtype=np.float64)
    dp = ctypes.POINTER(ctypes.c_double)
    w = lib(libpath).fet_sim(p.ctypes.data_as(dp), ctypes.c_double(fs), sx.ctypes.data_as(dp), sr.ctypes.data_as(dp),
                             ctypes.c_int(len(sx)), ctypes.c_double(x_burst), ctypes.c_double(burst_ms),
                             ctypes.c_double(pre_ms), ctypes.c_double(post_ms), out.ctypes.data_as(dp), ctypes.c_int(n))
    return out[:w]


def x_for_reduction(rev, ratio, target):
    """Smallest x where the reduction curve reaches target (dB)."""
    x, r = reduction_curve(rev, ratio)
    for i in range(1, len(x)):
        if r[i] >= target:
            t = (target - r[i - 1]) / (r[i] - r[i - 1]) if r[i] != r[i - 1] else 0.0
            return x[i - 1] + t * (x[i] - x[i - 1])
    return x[-1]


def crossing_time(g, frac, start_index, fs=FS, rising=False):
    """Time (ms) after start_index where g crosses frac * g[ref]."""
    seg = g[start_index:]
    if rising:
        ref = seg[-1] if False else None
    ref = g[start_index]
    thr = ref * frac
    idx = np.nonzero(seg <= thr)[0]
    if len(idx) == 0:
        return math.inf
    return idx[0] / fs * 1000.0
