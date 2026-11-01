"""Nelder-Mead fit of the ReleaseLaw constants with the C++ tune harness.
Usage: opt.py <tune binary> <csv> <max evals>; prints the best constants (fastMs mChargeMs mMaxMs mPerRel sChargeMs gateDecay holdScale sLeakMs)."""
import re, subprocess, sys
import numpy as np
from scipy.optimize import minimize
tune, csv, maxfev = sys.argv[1], sys.argv[2], int(sys.argv[3])
x0 = np.array([0.9, 4.0, 12.0, 0.15, 160.0, 3.0, 1.0, 80.0])
def cost(x):
    v = x0 * np.exp(x)
    out = subprocess.run([tune, csv] + ['%g' % a for a in v], capture_output=True, text=True).stdout
    c = 0.0
    for line in out.splitlines():
        p = line.split()
        if p[0] == 'B':
            early, late, r = float(p[3]), float(p[5]), float(p[7])
            c += 4 * max(0.0, early - 2.7) ** 2 + 8 * max(0.0, late - 1.3) ** 2 + r ** 2
            c += 0.3 * len(re.findall(r'\[', line))
        elif p[0] == 'T':
            c += sum(max(0.0, float(a) - 1.2) ** 2 for a in p[1:])
        elif p[0] == 'N':
            c += sum(4 * max(0.0, abs(float(a)) - 0.4) ** 2 for a in p[1:])
        elif p[0] == 'L':
            vals = re.findall(r'-?\d+\.\d+', line)
            c += sum(max(0.0, abs(float(a)) - 3.0) ** 2 for a in vals[:12]) + sum(max(0.0, abs(float(a)) - 3.0) ** 2 for a in vals[13::2])
    print('%.3f' % c, ' '.join('%.4g' % a for a in v), flush=True)
    return c
minimize(cost, np.zeros(8), method='Nelder-Mead', options=dict(maxfev=maxfev, initial_simplex=np.vstack([np.zeros(8), np.eye(8) * 0.15])))
