#!/usr/bin/env python3
"""Fits the gain-cell parameters (source/dsp/Dynamics.h) to the spec r1 timing and trajectory tables.

Development aid, not shipped. Usage (from plugins/fet-limiter):
    g++ -O3 -shared -fPIC -Isource tools/simwrap.cpp -o /tmp/libfetsim.so
    python3 tools/fit_dynamics.py /tmp/libfetsim.so > fit.txt

Staged fit (each stage in parallel):
  1. rel_4_a3 (= ratio 4, Attack 3, Release 4): all parameters.
  2. att_1/5/7: attack parameters only (release fixed to stage 1); rel_1/2/3/5/6/7: release parameters only
     (attack fixed to stage 1); rat_8/20/All_a3_r4 and revB_a3_r4: all parameters.
  3. rat_8/20/All_a7_r7, revB_a7_r7, revB_a1_r1: all parameters.
Prints residual reports and a JSON dict of the parameter vectors, which source/dsp/DynamicsMap.h transcribes.
"""
import json
import math
import multiprocessing
import sys

import numpy as np
from scipy.optimize import least_squares

import fetfit_common as fc

NAMES = ["tauA", "beta", "tauC", "tauHc", "tauKc", "tau1", "tau2", "tau3", "offset", "accel", "depthExp"]
ATTACK = [0, 1, 2]
RELEASE = [3, 4, 5, 6, 7, 8, 9]
LOG = [True, False, True, True, True, True, True, True, False, False, False]
LO = [0.01, -0.6, 0.3, 0.3, 1.0, 3.0, 3.0, 3.0, 0.0, 0.0, 0.0]
HI = [20.0, 0.6, 300.0, 1000.0, 5000.0, 20000.0, 20000.0, 20000.0, 40.0, 10.0, 4.0]


def to_t(p):
    return np.array([math.log(v) if l else v for v, l in zip(p, LOG)])


def from_t(t):
    return np.array([math.exp(v) if l else v for v, l in zip(t, LOG)])


def config_rows(name):
    return [r for r in fc.load_timing() if r["config"] == name]


def burst_x(rows):
    rev, ratio = rows[0]["revision"], rows[0]["ratio"]
    long = [r for r in rows if float(r["hold_ms"]) == 2000.0][0]
    target = -float(long["gr_end_db"])
    if "approx" in long["note"]:  # settled GR of approx rows contradicts spec 5; use the regular burst level
        target = 21.3 if rev == "A" else 22.7
    # (All: the trajectories are relative to the resting gain, i.e. reductions without the All makeup)
    return fc.x_for_reduction(rev, ratio, target)


def metrics(g, burst_ms, times):
    fs = fc.FS
    nb = int(burst_ms * 0.001 * fs)
    gend = g[nb - 1]
    rel = g[nb - 1:]
    traj = np.array([rel[min(len(rel) - 1, int(t * 0.001 * fs))] for t in times])

    def cross(frac):
        idx = np.nonzero(rel <= gend * (1 - frac))[0]
        return max(idx[0], 0.5) / fs * 1000.0 if len(idx) else 9000.0

    idx = np.nonzero(g[:nb] >= 0.63 * gend)[0]
    t_att = max(idx[0], 0.5) / fs * 1000.0 if len(idx) else 9000.0
    return gend, traj, cross(0.63), cross(0.90), t_att


def residuals_for(p, lib, name, times, traj_tab):
    rows = config_rows(name)
    rev, ratio = rows[0]["revision"], rows[0]["ratio"]
    x = burst_x(rows)
    res = []
    for r in rows:
        h = float(r["hold_ms"])
        g = fc.simulate(lib, p, rev, ratio, x, h)
        gend, traj, t63, t90, tatt = metrics(g, h, times)
        meas, valid = traj_tab[(name, h)]
        if valid != "yes":
            res += list((traj / max(gend, 1e-3) - meas / meas[0]) * 21.0 / 3.0)
        else:
            res += list((-traj - meas) / 1.5)
            res.append((-gend - float(r["gr_end_db"])) / 1.5 * 0.5)
        if r["release_t63_ms"]:
            res.append(math.log(t63 / float(r["release_t63_ms"])) / 0.22 * 1.5)
        if r["release_t90_ms"]:
            res.append(math.log(t90 / float(r["release_t90_ms"])) / 0.22 * 1.5)
        if r["attack_t63_ms"] and float(r["attack_t63_ms"]) > 0.0:
            res.append(math.log(tatt / float(r["attack_t63_ms"])) / 0.22 * (2.0 if h == 50.0 else 0.7))
    return res


def fit(lib, name, base, free, init=None):
    times, traj_tab = fc.load_traj()
    base = np.array(base, float)
    if init is not None:  # warm start of the free parameters
        base = base.copy()
        base[free] = np.array(init, float)[free]
    tb = to_t(base)
    lo, hi = to_t(LO)[free], to_t(HI)[free]

    def res(tf):
        t = tb.copy()
        t[free] = tf
        r = []
        for n in name.split("+"):
            r += residuals_for(from_t(t), lib, n, times, traj_tab)
        return np.array(r)

    best = None
    rng = np.random.default_rng(7)
    starts = [np.clip(tb[free], lo + 1e-6, hi - 1e-6)]
    for _ in range(2):
        jitter = rng.normal(0, 0.5, len(free)) * np.array([0.2 if not LOG[i] else 1.0 for i in free])
        starts.append(np.clip(tb[free] + jitter, lo + 1e-6, hi - 1e-6))
    for s in starts:
        sol = least_squares(res, s, bounds=(lo, hi), max_nfev=60, diff_step=1e-3)
        if best is None or sol.cost < best.cost:
            best = sol
    t = tb.copy()
    t[free] = best.x
    return from_t(t)


def report(lib, name, p):
    if "+" in name:
        return "\n".join(report(lib, n, p) for n in name.split("+"))
    times, traj_tab = fc.load_traj()
    rows = config_rows(name)
    rev, ratio = rows[0]["revision"], rows[0]["ratio"]
    x = burst_x(rows)
    out = [f"== {name}: x={x:.2f}  " + " ".join(f"{n}={v:.4g}" for n, v in zip(NAMES, p))]
    for r in rows:
        h = float(r["hold_ms"])
        g = fc.simulate(lib, p, rev, ratio, x, h)
        gend, traj, t63, t90, tatt = metrics(g, h, times)
        meas, valid = traj_tab[(name, h)]
        err = np.max(np.abs(-traj - meas))
        out.append(f"  hold {h:6.0f}: gend {-gend:7.2f} ({r['gr_end_db']:>7})  att {tatt:6.3f} ({r['attack_t63_ms'] or '-':>6})"
                   f"  t63 {t63:7.1f} ({r['release_t63_ms'] or '-':>8})  t90 {t90:7.1f} ({r['release_t90_ms'] or '-':>8})"
                   f"  traj maxerr {err:5.2f} [{valid}]")
    return "\n".join(out)


def _job(args):
    lib, name, base, free = args
    warm = WARM.get(name)
    if warm is not None:
        warm = list(warm[:10]) + [base[10]]
    p = fit(lib, name, base, free, warm)
    return name, list(p), report(lib, name, p)


def run_stage(pool, lib, jobs):
    results = pool.map(_job, [(lib, n, b, f) for n, b, f in jobs])
    for n, p, text in results:
        print(text, flush=True)
    return {n: p for n, p, t in results}


WARM = {}
if len(sys.argv) > 2:  # optional warm start: output file of a previous run (JSON on its last line)
    WARM = json.loads(open(sys.argv[2]).read().strip().splitlines()[-1])
    WARM = {k: (v + [0.3 if "All" in k else 0.0, 1.5])[:11] for k, v in WARM.items()}
    WARM["rel_4_a3+lvl_in-24_a3_r4"] = WARM["rel_4_a3"]

if __name__ == "__main__":
    lib = sys.argv[1]
    ALL = list(range(11))
    NOEXP = list(range(10))  # depthExp is global: fitted in stage 1 only
    init = WARM.get("rel_4_a3", [0.6, 0.15, 15.0, 15.0, 40.0, 700.0, 600.0, 900.0, 1.0, 0.0, 1.5])
    params = {}
    with multiprocessing.Pool(16) as pool:
        params.update(run_stage(pool, lib, [("rel_4_a3+lvl_in-24_a3_r4", init, ALL)]))
        params["rel_4_a3"] = base = params.pop("rel_4_a3+lvl_in-24_a3_r4")
        stage2 = [(f"att_{a}_r4", base, ATTACK) for a in (1, 5, 7)]
        stage2 += [(f"rel_{r}_a3", base, RELEASE) for r in (1, 2, 3, 5, 6, 7)]
        stage2 += [(n, base, NOEXP) for n in ("rat_8_a3_r4", "rat_20_a3_r4", "rat_All_a3_r4", "revB_a3_r4")]
        params.update(run_stage(pool, lib, stage2))
        a7 = params["att_7_r4"][:3] + params["rel_7_a3"][3:]
        a1 = params["att_1_r4"][:3] + params["rel_1_a3"][3:]
        stage3 = [(n, a7, NOEXP) for n in ("rat_8_a7_r7", "rat_20_a7_r7", "rat_All_a7_r7", "revB_a7_r7")]
        stage3 += [("revB_a1_r1", a1, NOEXP)]
        params.update(run_stage(pool, lib, stage3))
    print(json.dumps(params))
