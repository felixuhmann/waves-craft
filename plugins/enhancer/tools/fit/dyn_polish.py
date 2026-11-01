# Per-band refinement of the shared fits (dyn_params_{0,3,5}.json) -> dyn_params_band.json: [band][anchor] = 13 values.
import numpy as np, json, os
from scipy.optimize import least_squares
import dyn_fit2 as F
H = os.path.dirname(os.path.abspath(__file__))
out = []
for b in range(5):
    row = []
    for P in (0, 3, 5):
        v = json.load(open(os.path.join(H, f'dyn_params_{P}.json')))
        v0 = np.array(v[:11] + [v[11 + b], v[16 + b]])
        d = F.STEPS[(F.BANDS[b], P)]
        times = np.array([F.STARTS[si] + tm / 1000 for si, tm, g in d])
        tol = np.array([(8. if b == 0 and tm < 50 else 4. if tm <= 50 else 2.) for si, tm, g in d])
        gm = np.array([g for si, tm, g in d])
        fn = lambda x: (F.sim(np.abs(x[:11]), x[11], x[12], b, F.SET[(F.BANDS[b], P)], times) - gm) / tol
        best = None
        rng = np.random.default_rng(b * 10 + P)
        for k in range(6):
            s0 = v0.copy()
            if k: s0[:11] = s0[:11] * np.exp(rng.normal(0, 0.5, 11)) + 1e-4
            s = least_squares(fn, s0, diff_step=1e-4, max_nfev=120)
            if best is None or np.sum(s.fun**2) < best[0]: best = (np.sum(s.fun**2), s.x)
        r = fn(best[1])
        print(F.BANDS[b], P, 'max %.2f rms %.3f' % (np.abs(r).max(), np.sqrt(np.mean(r**2))), np.round(np.abs(best[1][:11]), 4), flush=True)
        row.append([float(abs(x)) for x in best[1][:11]] + [float(best[1][11]), float(best[1][12])])
    out.append(row)
json.dump(out, open(os.path.join(H, 'dyn_params_band.json'), 'w'))
