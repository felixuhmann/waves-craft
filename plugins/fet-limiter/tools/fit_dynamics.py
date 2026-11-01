#!/usr/bin/env python3
"""Fits the gain-cell parameters (source/dsp/Dynamics.h) to the spec r2 timing, trajectory and fast-burst tables.

Development aid, not shipped. Usage (from plugins/fet-limiter):
    g++ -O3 -shared -fPIC -Isource tools/simwrap.cpp -o /tmp/libfetsim.so
    python3 tools/fit_dynamics.py /tmp/libfetsim.so [previous_output] > fit.txt
The last line of the output is the JSON that tools/emit_dynamics_map.py turns into source/dsp/DynamicsMap.h.
"""
import csv
import json
import math
import multiprocessing
import sys

import numpy as np
from scipy.optimize import least_squares

import fetfit_common as fc

NAMES = ["tauA", "beta", "tauC", "tauMc", "tauMd", "tF", "tS", "knee", "depth", "pow", "depthExp", "beta2", "tauC2", "cap", "kneeS"]
ATTACK = [0, 1, 2, 11, 12, 13]
RELEASE = [3, 4, 5, 6, 7, 8, 9, 14]
ALL = ATTACK + RELEASE
LOG = [True, False, True, True, True, True, True, True, True, False, False, False, True, True, True]
LO = [0.01, -0.6, 0.3, 0.3, 5.0, 0.2, 0.2, 0.001, 3.0, 0.0, 0.0, -0.8, 0.3, 5.0, 0.001]
HI = [20.0, 0.6, 20000.0, 5000.0, 20000.0, 5000.0, 5000.0, 40.0, 200.0, 10.0, 4.0, 0.8, 300.0, 1000.0, 200.0]
GIN = {("A", -12.0): 37.07, ("A", -24.0): 24.74, ("B", -12.0): 40.58}
INIT = [0.69, -0.07, 413.0, 25.0, 500.0, 110.0, 170.0, 4.0, 35.0, 2.0, 1.74, 0.0, 10.0]


def to_t(p):
    return np.array([math.log(v) if l else v for v, l in zip(p, LOG)])


def from_t(t):
    return np.array([math.exp(v) if l else v for v, l in zip(t, LOG)])


TIMING = fc.load_timing()
TIMES, TRAJ = fc.load_traj()
with open(fc.TABLES / "fast_bursts.csv") as f:
    BURSTS = [r for r in csv.DictReader(f) if "informative" not in r["note"]]


def soft(r):
    """Residual in tolerances; exceeding the tolerance costs much more (we want every point inside)."""
    return r + np.sign(r) * max(0.0, abs(r) - 0.8) * 4.0


def cross(rel, level, fs=fc.FS):
    idx = np.nonzero(rel <= level)[0]
    return max(idx[0], 0.5) / fs * 1000.0 if len(idx) else 20000.0


def x_of_config(rows):
    rev, ratio = rows[0]["revision"], rows[0]["ratio"]
    if ratio == "All" and rev == "A":
        return 27.07  # the +30 dB step ends at -10 dBFS, Input -12 (fast_bursts.csv); its creep is r2 13.4
    long = [r for r in rows if float(r["hold_ms"]) == 2000.0][0]
    target = -float(long["gr_end_db"])
    if "approx" in long["note"]:
        target = 21.3 if rev == "A" else 22.7
    return fc.x_for_reduction(rev, ratio, target)


def timing_res(p, lib, name):
    rows = [r for r in TIMING if r["config"] == name]
    rev, ratio = rows[0]["revision"], rows[0]["ratio"]
    x = x_of_config(rows)
    res = []
    for r in rows:
        h = float(r["hold_ms"])
        if ratio == "All" and h == 5.0:
            continue  # first burst of a stream, informative (r2 13.3)
        g = fc.simulate(lib, p, rev, ratio, x, h, post_ms=9000.0 if float(r["release"]) < 2.5 else 5200.0)
        nb = int(h * 0.001 * fc.FS)
        gend = g[nb - 1]
        rel = g[nb - 1:]
        tr = np.array([rel[int(t * 0.001 * fc.FS)] for t in TIMES])
        meas, valid = TRAJ[(name, h)]
        if valid != "yes":
            res += [soft(v) for v in (tr / max(gend, 1e-3) - meas / meas[0]) * 21.0 / 3.0]
        else:
            res += [soft(v) for v in (-tr - meas) / 1.5]
        if r["release_t63_ms"]:
            res.append(soft(math.log(cross(rel, gend * 0.37) / float(r["release_t63_ms"])) / 0.22))
        if r["release_t90_ms"]:
            res.append(soft(math.log(cross(rel, gend * 0.10) / float(r["release_t90_ms"])) / 0.22))
        if r["attack_t63_ms"] and float(r["attack_t63_ms"]) > 0 and h == 50.0:
            idx = np.nonzero(g[:nb] >= 0.63 * gend)[0]
            res.append(soft(math.log(max(idx[0], 0.5) / fc.FS * 1000.0 / float(r["attack_t63_ms"])) / 0.22))
    return res


def burst_res(p, lib, rev, ratio, att, rel_, report=None):
    res = []
    for r in BURSTS:
        if (r["revision"], r["ratio"], float(r["attack"]), float(r["release"])) != (rev, ratio, att, rel_):
            continue
        gin = GIN[(rev, float(r["input_display"]))]
        shift = -0.25 * max(0.0, GIN[(rev, -12.0)] - gin) if ratio == "All" else 0.0  # see FetEngine.h
        h = float(r["burst_ms"])
        g = fc.simulate(lib, p, rev, ratio, float(r["burst_level_dbfs"]) + gin + shift, h, post_ms=900.0, x_base=-40.0 + gin + shift)
        nb = int(h * 0.001 * fc.FS)
        rel = g[nb - 1:]
        at = lambda t: rel[int(t * 0.001 * fc.FS)]
        e0 = -at(0.5) - float(r["gr_db_at_0p5ms_after_burst"])
        res.append(soft(e0 / 1.5))
        for t in (10, 25, 50, 100, 200, 400):
            res.append(soft((-at(t) - float(r[f"gr_db_at_{t}ms"])) / 1.5))
        g0 = rel[0]
        tab_t63 = float(r["release_t63_ms"])
        if tab_t63 > 0:
            res.append(soft(math.log(cross(rel, g0 * 0.37) / tab_t63) / 0.22))
            hold_tab = float(r["hold_ms_until_1db_recovered"])
            hold = cross(rel, g0 - 1.0)
            res.append(soft((hold - hold_tab) / max(0.3 * hold_tab, 2.0)))
    return res


def compose(att, rel):
    v = list(rel)
    for i in ATTACK:
        v[i] = att[i]
    return v


def _job(job):
    """job: name, base vector, free indices, items. item: ('t', config, mode) or ('b', rev, ratio, att, rel, mode).
    mode 'self' uses the vector being fitted; (attVec, None) takes the release group from it, (None, relVec) the attack
    group from it, keeping the other group fixed."""
    lib, name, base, free, items, nfev = job
    tb = to_t(base)
    lo, hi = to_t(LO)[free], to_t(HI)[free]

    def vec(tf, mode):
        t = tb.copy()
        t[free] = tf
        p = list(from_t(t))
        if mode == "self":
            return p
        fixed_att, fixed_rel = mode
        return compose(fixed_att if fixed_att is not None else p, fixed_rel if fixed_rel is not None else p)

    def res(tf):
        out = []
        for it in items:
            if it[0] == "t":
                out += timing_res(vec(tf, it[2]), lib, it[1])
            else:
                out += burst_res(vec(tf, it[5]), lib, *it[1:5])
        return np.array(out)

    start = np.clip(tb[free], lo + 1e-6, hi - 1e-6)
    sol = least_squares(res, start, bounds=(lo, hi), max_nfev=nfev, diff_step=2e-3)
    t = tb.copy()
    t[free] = sol.x
    r = res(sol.x)
    print(f"{name}: rms {np.sqrt(np.mean(r * r)):.2f} over-tol {int(np.sum(np.abs(r) > 1.0))}/{len(r)} "
          + " ".join(f"{n}={v:.4g}" for n, v in zip(NAMES, from_t(t))), flush=True)
    return name, list(from_t(t))


if __name__ == "__main__":
    lib = sys.argv[1]
    P = {}
    if len(sys.argv) > 2:
        P = json.loads(open(sys.argv[2]).read().strip().splitlines()[-1])
    nfev = 40
    only = set(sys.argv[3].split(",")) if len(sys.argv) > 3 else None

    def get(name, default):
        return P.get(name, default)

    with multiprocessing.Pool(20) as pool:
        def run(jobs):
            if only is not None:
                nfev_local = 90
                jobs = [j for j in jobs if j[0] in only]
                for n, v in pool.map(_job, [(lib, *j, nfev_local) for j in jobs]):
                    P[n] = v
                return
            for n, v in pool.map(_job, [(lib, *j, nfev) for j in jobs]):
                P[n] = v
        # 1: Attack 3 / Release 4 with the Input -24 rows (depth dependence)
        run([("rel_4_a3", get("rel_4_a3", INIT), ALL + [10], [("t", "rel_4_a3", "self"), ("t", "lvl_in-24_a3_r4", "self")])])
        b = P["rel_4_a3"]
        # 2: attack / release groups of ratio 4, Attack 7 and Release 7 jointly with their fast bursts
        jobs = [(f"att_{a}_r4", get(f"att_{a}_r4", b), ATTACK, [("t", f"att_{a}_r4", "self")]) for a in (1, 5)]
        jobs += [(f"rel_{r}_a3", get(f"rel_{r}_a3", b), RELEASE, [("t", f"rel_{r}_a3", "self")]) for r in (1, 2, 3, 5, 6)]
        v77 = [0.08, 0.0, 400.0, 25.0, 500.0, 6.0, 25.0, 6.0, 30.0, 2.0, b[10], -0.05, 10.0]
        jobs.append(("a4_77", get("a4_77", v77), ALL, [("t", "att_7_r4", (None, b)), ("t", "rel_7_a3", (b, None)),
                                                       ("b", "A", "4", 7.0, 7.0, "self")]))
        # complete sets per ratio / revision at their measured pairs
        for n in ("rat_8_a3_r4", "rat_20_a3_r4", "revB_a3_r4", "rat_All_a3_r4"):
            items = [("t", n, "self")]
            if "All" in n:
                items.append(("b", "A", "All", 3.0, 4.0, "self"))
            init = compose(b, b) if "All" not in n else [0.25, 0.05, 3000.0, 30.0, 500.0, 16.0, 16.0, 0.01, 18.0, 4.0, b[10], 0.0, 10.0]
            jobs.append((n, get(n, init), ALL, items))
        allinit = [0.1, 0.1, 4000.0, 30.0, 500.0, 2.4, 2.4, 0.01, 19.0, 3.5, b[10], -0.1, 10.0]
        jobs.append(("rat_All_a7_r7", get("rat_All_a7_r7", allinit), ALL, [("t", "rat_All_a7_r7", "self"), ("b", "A", "All", 7.0, 7.0, "self")]))
        jobs.append(("all_55", get("all_55", [0.2, 0.05, 3000.0, 30.0, 500.0, 10.0, 10.0, 0.01, 17.0, 4.0, b[10], 0.0, 10.0]), ALL, [("b", "A", "All", 5.0, 5.0, "self")]))
        jobs.append(("allB_77", get("allB_77", [0.1, 0.05, 3000.0, 30.0, 500.0, 1.0, 1.0, 0.01, 19.0, 4.0, b[10], 0.0, 10.0]), ALL, [("b", "B", "All", 7.0, 7.0, "self")]))
        for r in ("8", "20"):
            jobs.append((f"rat_{r}_a7_r7", get(f"rat_{r}_a7_r7", v77), ALL, [("t", f"rat_{r}_a7_r7", "self"), ("b", "A", r, 7.0, 7.0, "self")]))
        jobs.append(("revB_a7_r7", get("revB_a7_r7", v77), ALL, [("t", "revB_a7_r7", "self")]))
        jobs.append(("revB_a1_r1", get("revB_a1_r1", b), ALL, [("t", "revB_a1_r1", "self")]))
        run(jobs)
        P["att_3_r4"] = P["rel_4_a3"]
        P["att_7_r4"] = P["a4_77"]
        P["rel_7_a3"] = P["a4_77"]
    print(json.dumps(P))
