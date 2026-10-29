"""Generate the console-e EQ/filter parameter tables from the spec appendix (fit + interpolation grid)."""
import numpy as np, warnings, sys, json
warnings.filterwarnings('ignore')
from scipy.optimize import least_squares
from data import rows, F, peaks
from mb import matched2, matched1, dig_db, lp2, bell, lowshelf, highshelf

W = np.where(F <= 12000, 1.0, 0.5)
LFK = [(0, 30), (.2, 50), (.5, 200), (.8, 300), (1, 450)]
LMK = [(0, .2), (.2, .3), (.5, 1.0), (.8, 2.0), (1, 2.5)]
HMK = [(0, .6), (.2, .8), (.5, 3.0), (.8, 6.0), (1, 7.0)]
HFK = [(0, 1.5), (.2, 2.0), (.5, 8.0), (.8, 14.0), (1, 16.0)]
QK = [(0, 3.5), (.2, 2.5), (.5, 1.5), (.8, .5), (1, .1)]
# Rows labelled 'Q 0.5' coincide exactly with the Q-sweep at r = 0.9 (lmq_0.90 == gm_1k_q0.5_+12.0)
QROW = {3.5: 0.0, 1.5: 0.5, 0.5: 0.9, 0.1: 1.0}

def r_of(v, knots):
    for (r0, v0), (r1, v1) in zip(knots[:-1], knots[1:]):
        if min(v0, v1) - 1e-9 <= v <= max(v0, v1) + 1e-9:
            return round(r0 + (r1 - r0) * (v - v0) / (v1 - v0), 6)
    raise ValueError(v)

LSH_K, LSH_Q = 15.0, 0.4   # LF-shelf companion bell: frequency ratio and Q

# --- parameter -> analog sections -----------------------------------------------------------
def protos(kind, p):
    if kind == 'bell':
        f0, G, Q, h = np.exp(p[0]), p[1], np.exp(p[2]), p[3]
        A = 10 ** (G / 40); w = 2 * np.pi * f0
        return [((10 ** (h / 20), A * w / Q, w * w), (1, w / (A * Q), w * w))]
    if kind == 'lshelf':
        f = np.exp(p[0])
        return [lowshelf(f, p[1], np.exp(p[2])), bell(f * LSH_K, p[3], LSH_Q)]
    if kind == 'hshelf':
        fz, Qz, fp, Qp = np.exp(p)
        wz, wp = 2 * np.pi * fz, 2 * np.pi * fp
        k = (wp * wp) / (wz * wz)
        return [((k, k * wz / Qz, k * wz * wz), (1, wp / Qp, wp * wp))]
    raise ValueError(kind)

def digital(kind, p, fs):
    if kind == 'bell' and p[1] < 0:
        q = np.array(p, float).copy(); q[1] = -q[1]; q[3] = -q[3]
        b, a = matched2(*protos('bell', q)[0], fs)
        return [(a / b[0], b / b[0])]
    return [matched2(n, d, fs) for n, d in protos(kind, p)]

def resp(kind, p, fs=48000., f=F):
    return dig_db(digital(kind, p, fs), fs, f)

def identity(kind, p):
    p = np.array(p, dtype=float).copy()
    if kind == 'bell':
        p[1] = 0; p[3] = 0
    elif kind == 'lshelf':
        p[1] = 0; p[3] = 0
    else:
        m = 0.5 * (p[0] + p[2]); q = 0.5 * (p[1] + p[3])
        p[:] = [m, q, m, q]
    return p

# --- row classification ----------------------------------------------------------------------
def classify(k):
    t = k.split('_')
    if k.startswith('lf_shelf'): return 'LFS', r_of(float(t[2]), LFK), None, float(t[3])
    if k.startswith('lf_bell'): return 'LFB', r_of(float(t[2]), LFK), None, float(t[3])
    if k.startswith('hf_shelf'): return 'HFS', r_of(float(t[2]), HFK), None, float(t[3])
    if k.startswith('hf_bell'): return 'HFB', r_of(float(t[2]), HFK), None, float(t[3])
    if k.startswith('lff_'): return 'LFB', float(t[1]), None, 12.0
    if k.startswith('hff_'): return 'HFB', float(t[1]), None, 12.0
    if k.startswith('lmq_'): return 'LMF', 0.5, float(t[1]), 12.0
    if k.startswith('gl_bell60'): return 'LFB', r_of(60, LFK), None, float(t[2])
    if k.startswith('gl_shelf60'): return 'LFS', r_of(60, LFK), None, float(t[2])
    if k.startswith('gh_bell8k'): return 'HFB', .5, None, float(t[2])
    if k.startswith('gh_shelf8k'): return 'HFS', .5, None, float(t[2])
    if k.startswith('gm_1k'): return 'LMF', .5, QROW[float(t[2][1:])], float(t[3])
    if k.startswith('lmf_') or k.startswith('hmf_'):
        b = 'LMF' if k.startswith('lmf') else 'HMF'
        if len(t) == 2: return b, float(t[1]), 0.5, 12.0
        return b, r_of(float(t[1]), LMK if b == 'LMF' else HMK), QROW[float(t[2])], float(t[3])
    raise ValueError(k)

KIND = {'LFS': 'lshelf', 'LFB': 'bell', 'HFB': 'bell', 'LMF': 'bell', 'HMF': 'bell', 'HFS': 'hshelf'}

# --- per-row fits ----------------------------------------------------------------------------
def fit(kind, y, x0, reg=None, lam=0.0):
    if kind == 'bell':
        lb, ub = [np.log(10), -30, np.log(0.05), -6], [np.log(30000), 30, np.log(10), 6]
    elif kind == 'lshelf':
        lb, ub = [np.log(10), -30, np.log(0.2), -6], [np.log(5000), 30, np.log(2), 6]
    else:
        lb, ub = [np.log(50), np.log(0.003), np.log(50), np.log(0.02)], [np.log(3e5), np.log(5), np.log(3e5), np.log(5)]
    x0 = np.clip(np.array(x0, float), np.array(lb) + 1e-6, np.array(ub) - 1e-6)
    def res(p):
        r = np.nan_to_num((resp(kind, p) - y) * W, nan=100., posinf=100., neginf=-100.)
        if reg is not None:
            r = np.concatenate([r, lam * (p - reg)])
        return r
    s = least_squares(res, x0, bounds=(lb, ub))
    return s.x

def fit_all():
    P = {}
    for k, y in rows.items():
        if k.startswith('hpn') or k.startswith('lpn'):
            continue
        b, fr, qr, g = classify(k)
        kind = KIND[b]
        if kind == 'bell':
            pk = peaks.get(k)
            f0 = pk[1] if pk else F[np.argmax(np.abs(y))]
            best = None
            for q in [0.3, 1.0, 2.5]:
                x = fit(kind, y, [np.log(f0), y[np.argmax(np.abs(y))], np.log(q), 0.0])
                c = np.sum(((resp(kind, x) - y) * W) ** 2)
                if best is None or c < best[0]:
                    best = (c, x)
            P[k] = best[1]
        elif kind == 'lshelf':
            hz = {'LFS': None}
            best = None
            for fg in [60, 150, 400, 1200]:
                x = fit(kind, y, [np.log(fg), y[0], np.log(0.47), -0.2 * np.sign(y[0])])
                c = np.sum(((resp(kind, x) - y) * W) ** 2)
                if best is None or c < best[0]:
                    best = (c, x)
            P[k] = best[1]
    # high shelf: continuation along gain at 8 kHz, then along frequency
    seeds = {}
    for chain in [['gh_shelf8k_+3.0', 'gh_shelf8k_+6.0', 'hf_shelf_8.0_+7.5', 'gh_shelf8k_+12.0', 'hf_shelf_8.0_+15.0'],
                  ['gh_shelf8k_-3.0', 'gh_shelf8k_-6.0', 'gh_shelf8k_-12.0', 'hf_shelf_8.0_-15.0']]:
        x = np.log([4000, 0.5, 4000, 0.5])
        for k in chain:
            x = fit('hshelf', rows[k], x, x, 0.05)
            P[k] = x
    for g in ['+7.5', '+15.0', '-15.0']:
        for ch in [['3.0', '1.5'], ['16.0']]:
            x = P['hf_shelf_8.0_' + g]
            for f in ch:
                k = 'hf_shelf_%s_%s' % (f, g)
                prev = 8.0 if f in ('3.0', '16.0') else 3.0
                sh = np.log(float(f) / prev)
                x0 = x + np.array([sh, 0, sh, 0])
                x = fit('hshelf', rows[k], x0, x0, 0.05)
                P[k] = x
    return P

if __name__ == '__main__':
    P = fit_all()
    out = {k: list(map(float, v)) for k, v in P.items()}
    json.dump(out, open('rowfits.json', 'w'), indent=1)
    for k, v in P.items():
        b, fr, qr, g = classify(k)
        e = resp(KIND[b], v) - rows[k]
        print('%-22s %s fr=%.4f qr=%s g=%5.1f  %s  err %.2f / %.2f' % (k, b, fr, qr, g, np.round(v, 3), np.abs(e[F <= 12000]).max(), np.abs(e[F > 12000]).max()))
