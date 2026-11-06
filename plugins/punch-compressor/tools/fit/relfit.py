"""Closed-form fit of the slow release law (gamma, a, K) to the 1000 ms / -6 dBFS rows of spec r2 table 7.5.
Usage: python3 relfit.py <release_after_burst.csv>"""
import csv, math, sys
import numpy as np
from scipy.optimize import least_squares
rows = [r for r in csv.DictReader(open(sys.argv[1])) if r['burst_ms'] == '1000' and r['burst_dbfs'] == '-6']
T = [0, 2, 5, 10, 20, 50, 100, 200, 500, 1000, 2000, 3400]
G0 = 10 ** (-12.93 / 20)
def traj(rel, gam, a, K, top):
    t = np.array(T, float) + 0.5
    z0, zt = G0 ** gam, K ** gam
    z = zt + (z0 - zt) * np.exp(-t / (a * rel))
    return 20 * np.log10(np.minimum(z, 1.0) ** (1 / gam)) + top
for r in rows:
    rel = float(r['release_ms'])
    ref = np.array([float(r['t%d' % t]) for t in T])
    best = None
    for K0 in (1.5, 3.0, 8.0):
        f = lambda p: traj(rel, p[0], p[1], p[2], ref[-1]) - ref
        s = least_squares(f, (0.8, 3.0, K0), bounds=([0.05, 0.05, 1.0], [3.0, 200.0, 30.0]))
        if best is None or s.cost < best.cost:
            best = s
    e = best.fun
    print('%5.0f gamma %.3f a %.3f K %.2f max %.2f' % (rel, *best.x, np.abs(e[3:]).max()))
