"""Low-pass and high-pass position tables (spec r3 section 4, Appendix A.1, A.2, C.1, C.2).

LP: per sample rate (44.1 / 48 / 96 kHz) and control position (r = 0.05 ... 1 in 0.05 steps) one biquad: pole pair
(f0, Q) by impulse invariance, normalised numerator 1 + c1 z^-1 + c2 z^-2, unity at DC. The tables are fitted to the
measured rows of that rate; odd positions at 44.1 / 96 kHz are not measured, their target is the 48 kHz row (C.1)
plus the mean rate difference of the two neighbouring even positions.
HP: third-order Butterworth corner per position, fitted to A.1 + C.1."""
import numpy as np, warnings
warnings.filterwarnings('ignore')
from scipy.optimize import least_squares
from data import rows, F, C, CF
from mb import poles2, dig_db

RATES = [44100, 48000, 96000]
POS = [round(0.05 * k, 2) for k in range(1, 21)]

def design(f0, Q, c1, c2, fs):
    a1, a2 = poles2(2 * np.pi * min(f0, 0.998 * fs), Q, 1 / fs)
    a = np.array([1, a1, a2])
    b = np.array([1, c1, c2])
    return b * (np.sum(a) / np.sum(b)), a

def minphase(c1, c2):
    z = np.roots([1, c1, c2]).astype(complex)
    z = np.where(np.abs(z) > 1, 1 / np.conj(z), z)
    p = np.real(np.poly(z))
    return p[1], p[2]

def target48(r):
    k = 'lpn_%.2f' % r
    if k in C[48000]:
        return CF, C[48000][k]
    return F, rows[k]

def target(r, fs):
    """(frequencies, dB) for position r at rate fs"""
    if fs == 48000:
        return target48(r)
    k = 'lpn_%.2f' % r
    if k in C[fs]:
        return CF, C[fs][k]
    f48, y48 = target48(r)
    d = 0
    for n in (round(r - 0.05, 2), round(r + 0.05, 2)):
        if n < 0.1:
            n = round(r + 0.05, 2)
        fn, yn = target48(n)
        d = d + 0.5 * (C[fs]['lpn_%.2f' % n] - np.interp(np.log(CF), np.log(fn), yn))
    return CF, np.interp(np.log(CF), np.log(f48), y48) + d

def fit_lp():
    """{fs: [(r, f0, Q, c1, c2, maxerr)]}"""
    out = {}
    for fs in RATES:
        res_all = []
        prev = None
        for r in reversed(POS):
            f, y = target(r, fs)
            res = lambda p: np.nan_to_num(dig_db([design(p[0], p[1], p[2], p[3], float(fs))], float(fs), f) - y, nan=100.)
            best = None
            starts = [[8000, 0.7, 0.6, 0.2], [20000, 1.0, 0.8, 0.3]]
            if prev is not None:
                starts.insert(0, list(prev))
            for x0 in starts:
                s = least_squares(res, x0, bounds=([1000, 0.3, -2, -1], [30000, 4, 2, 1]))
                if best is None or s.cost < best.cost - 1e-9:
                    best = s
            prev = best.x
            f0, Q, c1, c2 = best.x
            e = np.abs(res(best.x)).max()
            c1, c2 = minphase(c1, c2)
            res_all.append((r, f0, Q, c1, c2, e))
        out[fs] = sorted(res_all)
    return out

def fit_hp():
    """[corner Hz] for r = 0.05 ... 1"""
    out = []
    for r in POS:
        k = 'hpn_%.2f' % r
        f, y = (CF, C[48000][k]) if k in C[48000] else (F, rows[k])
        def model(fc):
            x2 = (f / fc) ** 2
            return 10 * np.log10(x2 ** 3 / (1 + x2 ** 3))
        s = least_squares(lambda p: model(np.exp(p[0])) - y, [np.log(100)])
        out.append((float(np.exp(s.x[0])), float(np.abs(model(np.exp(s.x[0])) - y).max())))
    return out

if __name__ == '__main__':
    for fs, tab in fit_lp().items():
        print(fs)
        for r, f0, Q, c1, c2, e in tab:
            print('  %.2f f0=%8.1f Q=%.3f c1=%.3f c2=%.3f err=%.3f' % (r, f0, Q, c1, c2, e))
    print([(round(a, 1), round(b, 2)) for a, b in fit_hp()])
