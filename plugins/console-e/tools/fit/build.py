"""Build dense interpolation grids for the EQ bands from the per-row fits (rowfits.json).

Measured rows are grid nodes and are reproduced exactly; other nodes are completed by linear interpolation
plus residuals borrowed from the measured sweeps (gain, Q, frequency), see IMPLEMENTATION_NOTES.md.
"""
import json, numpy as np, warnings, sys
warnings.filterwarnings('ignore')
from gen import classify, KIND, resp, identity, r_of, LFK, LMK, HMK, HFK, QK
from data import rows, F

P = {k: np.array(v) for k, v in json.load(open('rowfits.json')).items()}
BANDS = ['LFB', 'LFS', 'LMF', 'HMF', 'HFB', 'HFS']
R = lambda x: round(float(x), 4)
CORE_F = {'LFB': [0, r_of(60, LFK), .3, .5, 1], 'LFS': [0, r_of(60, LFK), .3, .5, 1],
          'LMF': [0, r_of(.5, LMK), .5, 1], 'HMF': [0, r_of(1.5, HMK), r_of(3.5, HMK), 1],
          'HFB': [0, r_of(3, HFK), .5, 1], 'HFS': [0, r_of(3, HFK), .5, 1]}
CORE_F = {b: [R(x) for x in v] for b, v in CORE_F.items()}
CORE_Q = [0.0, .5, .9, 1.0]
TEN = [R(0.1 * i) for i in range(11)]
GS = [-15.0, -12.0, -9.0, -6.0, -3.0, 0.0, 3.0, 6.0, 7.5, 9.0, 12.0, 15.0]
GREF = {'LFB': ('LFB', R(r_of(60, LFK)), 0.0), 'LFS': ('LFS', R(r_of(60, LFK)), 0.0),
        'HFB': ('HFB', .5, 0.0), 'HFS': ('HFS', .5, 0.0)}

def has_q(b):
    return b in ('LMF', 'HMF')

data = {b: {} for b in BANDS}
for k, p in P.items():
    b, fr, qr, g = classify(k)
    data[b][(R(fr), R(qr) if qr is not None else 0.0, float(g))] = p

def lin(xs, x):
    if x <= xs[0]: return 0, 0.0
    if x >= xs[-1]: return len(xs) - 2, 1.0
    i = max(j for j in range(len(xs) - 1) if xs[j] <= x + 1e-12)
    return i, (x - xs[i]) / (xs[i + 1] - xs[i])

def interp1(xs, vals, x):
    i, w = lin(xs, x)
    return (1 - w) * vals[i] + w * vals[i + 1]

def gscale(b, g):
    s = g / 12.0
    if KIND[b] == 'bell':
        return np.array([1, s, 1, s])
    if KIND[b] == 'lshelf':
        return np.array([1, s, 1, s])
    return np.ones(4)

def measured_gains(b, f, q):
    return sorted(g for (ff, qq, g) in data[b] if ff == f and qq == q)

def anchors_line(b, f, q, gains):
    """linear interpolation along gain through the measured values at (f,q) restricted to `gains` (+ identity at 0)."""
    vals = {g: data[b][(f, q, g)] for g in gains if g != 0}
    pos = [g for g in gains if g > 0]
    ref = vals[min(pos)] if pos else vals[max(gains)]
    vals[0.0] = identity(KIND[b], ref)
    xs = sorted(vals)
    return xs, [vals[x] for x in xs]

# ---- step A: complete the gain axis on the core (f, q) nodes ----------------------------------
def gain_ref(b, q):
    if b in ('LMF', 'HMF'):
        return 'LMF', .5, min(max(q, .5), .9)
    return GREF[b]

def residual_from_ref(b, f, q, anchors, g):
    rb, rf, rq = gain_ref(b, q)
    if rb == 'LMF' and rq not in (.5, .9):
        w = (rq - .5) / .4
        return (1 - w) * residual_from_ref(b, f, .5, anchors, g) + w * residual_from_ref(b, f, .9, anchors, g)
    ref_gains = measured_gains(rb, rf, rq)
    xs, vs = anchors_line(rb, rf, rq, ref_gains)
    full = interp1(xs, vs, g)                       # reference (dense measured gains)
    a = [x for x in anchors if x in ref_gains or x == 0]
    xa, va = anchors_line(rb, rf, rq, [x for x in a if x != 0])
    coarse = interp1(xa, va, g)
    r = full - coarse
    if b == 'HMF':
        r = r * np.array([1, HMF_RATIO, 1, 1])
    return r

def ratio_g75():
    h = np.mean([v[1] for (f, q, g), v in data['HMF'].items() if g == 7.5 and f in CORE_F['HMF'] and q in CORE_Q])
    l = np.mean([v[1] for (f, q, g), v in data['LMF'].items() if g == 7.5 and f in CORE_F['LMF'] and q in CORE_Q])
    return h / l
HMF_RATIO = ratio_g75()

A = {}
for b in BANDS:
    qs = CORE_Q if has_q(b) else [0.0]
    for f in CORE_F[b]:
        for q in qs:
            mg = measured_gains(b, f, q)
            xs, vs = anchors_line(b, f, q, mg)
            for g in GS:
                if (f, q, g) in data[b]:
                    A[(b, f, q, g)] = data[b][(f, q, g)]
                elif g == 0:
                    A[(b, f, q, g)] = vs[xs.index(0.0)]
                else:
                    A[(b, f, q, g)] = interp1(xs, vs, g) + residual_from_ref(b, f, q, mg, g)

# ---- step B: densify Q (LMF sweep at 1 kHz, +12 dB; shared shape for HMF) ----------------------
def coreq_interp(b, f, q, g):
    return interp1(CORE_Q, [A[(b, f, qq, g)] for qq in CORE_Q], q)

DQ = {}
for q in TEN:
    DQ[q] = data['LMF'][(.5, q, 12.0)] - coreq_interp('LMF', .5, q, 12.0)

Bn = {}
for b in BANDS:
    qa = TEN if has_q(b) else [0.0]
    for f in CORE_F[b]:
        for q in qa:
            for g in GS:
                if not has_q(b):
                    Bn[(b, f, q, g)] = A[(b, f, q, g)]
                elif (b, f, q, g) in A:
                    Bn[(b, f, q, g)] = A[(b, f, q, g)]
                else:
                    Bn[(b, f, q, g)] = coreq_interp(b, f, q, g) + DQ[q] * gscale(b, g)
                if (f, q, g) in data[b]:
                    Bn[(b, f, q, g)] = data[b][(f, q, g)]

# ---- step C: densify frequency (own +12 dB sweep; shelves borrow the bell's log-f shift) --------
def coref_interp(b, f, q, g):
    return interp1(CORE_F[b], [Bn[(b, ff, q, g)] for ff in CORE_F[b]], f)

DF = {}
for b in ['LFB', 'HFB', 'LMF', 'HMF']:
    q = .5 if has_q(b) else 0.0
    for f in TEN:
        if f in CORE_F[b]:
            continue
        DF[(b, f)] = data[b][(f, q, 12.0)] - coref_interp(b, f, q, 12.0)

def faxis(b):
    return sorted(set(TEN) | set(CORE_F[b]))

GRID = {}
for b in BANDS:
    fa, qa = faxis(b), (TEN if has_q(b) else [0.0])
    vals = np.zeros((len(fa), len(qa), len(GS), 4))
    for i, f in enumerate(fa):
        for j, q in enumerate(qa):
            for k, g in enumerate(GS):
                if f in CORE_F[b]:
                    v = Bn[(b, f, q, g)]
                else:
                    v = coref_interp(b, f, q, g)
                    if b == 'LFS':
                        v = v + np.array([DF[('LFB', f)][0], 0, 0, 0])
                    elif b == 'HFS':
                        d = DF[('HFB', f)][0]; v = v + np.array([d, 0, d, 0])
                    else:
                        v = v + DF[(b, f)] * gscale(b, g)
                if (f, q, g) in data[b]:
                    v = data[b][(f, q, g)]
                vals[i, j, k] = v
    GRID[b] = (fa, qa, GS, vals)

def lookup(b, fr, qr, g):
    fa, qa, ga, vals = GRID[b]
    i, wf = lin(fa, fr)
    k, wg = lin(ga, g)
    jj = [(0, 1.0)]
    if len(qa) > 1:
        j, wq = lin(qa, qr); jj = [(j, 1 - wq), (j + 1, wq)]
    out = 0
    for fi, fw in [(i, 1 - wf), (i + 1, wf)]:
        for qi, qw in jj:
            for gi, gw in [(k, 1 - wg), (k + 1, wg)]:
                out = out + fw * qw * gw * vals[fi, qi, gi]
    return out

if __name__ == '__main__':
    worst = []
    for k, y in rows.items():
        if k.startswith('hpn') or k.startswith('lpn'):
            continue
        b, fr, qr, g = classify(k)
        p = lookup(b, fr, qr if qr is not None else 0.0, g)
        for fs in [48000., 44100., 96000.]:
            e = resp(KIND[b], p, fs) - y
            lim = min(0.25 * fs, 12000.)
            lo = np.abs(e[F <= lim]).max(); hi = np.abs(e[F > lim]).max() if (F > lim).any() else 0
            worst.append((lo / 0.5, hi / 1.5, k, fs, lo, hi))
    worst.sort(key=lambda t: -max(t[0], t[1]))
    for w in worst[:20]:
        print('%-22s fs=%6.0f  lo %.2f  hi %.2f' % (w[2], w[3], w[4], w[5]))
    json.dump({b: {'f': GRID[b][0], 'q': GRID[b][1], 'g': GRID[b][2], 'v': GRID[b][3].tolist()} for b in BANDS}, open('grids.json', 'w'))
