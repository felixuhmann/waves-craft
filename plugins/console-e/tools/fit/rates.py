"""HF band parameter grids for 44.1 and 96 kHz (spec r3 Appendix C.3): the 48 kHz grid plus a parameter delta fitted
to the measured 16 kHz rows of that rate. The delta is linear in the gain label between the measured gains (0 at
gain 0) and ramps in linearly from frequency position 0.5 (no change needed there) to 1. Writes grids_rate.json."""
import json, numpy as np, warnings
warnings.filterwarnings('ignore')
from scipy.optimize import least_squares
from build import GRID, lookup, lin
from gen import resp
from data import C, CF

def delta(kind, band, row, g, fs, lam=0.02):
    y = C[fs][row]
    p0 = lookup(band, 1.0, 0.0, g)
    s = least_squares(lambda p: np.concatenate([np.nan_to_num((resp(kind, p, float(fs), CF) - y), nan=100.), lam * (p - p0)]), p0)
    e = resp(kind, s.x, float(fs), CF) - y
    print(fs, row, 'err %.2f / %.2f' % (np.abs(e[CF <= 12000]).max(), np.abs(e[CF > 12000]).max()))
    return s.x - p0

out = {}
for fs in (44100, 96000):
    for band, kind, name in (('HFB', 'bell', 'bell'), ('HFS', 'hshelf', 'shelf')):
        d = {g: delta(kind, band, 'hf_%s_16.0_%+.1f' % (name, g), g, fs) for g in (-15.0, 7.5, 15.0)}
        d[0.0] = np.zeros(4)
        gs = sorted(d)
        fa, qa, ga, vals = GRID[band]
        v = vals.copy()
        for i, f in enumerate(fa):
            w = max(0.0, (f - 0.5) / 0.5)
            for k, g in enumerate(ga):
                j, t = lin(gs, g)
                v[i, 0, k] += w * ((1 - t) * d[gs[j]] + t * d[gs[j + 1]])
        out[band + str(fs // 1000)] = {'f': fa, 'q': qa, 'g': ga, 'v': v.tolist()}
json.dump(out, open('grids_rate.json', 'w'))
