"""Low-pass model (spec r2 section 4, Appendix A.2): poles of an analogue pole pair (f0, Q) mapped by impulse
invariance, a fixed normalised second-order numerator 1 + c1 z^-1 + c2 z^-2 (same coefficients at every sample
rate, so the near-Nyquist roll-off scales with fs as on the reference) and unity gain at DC. Fitted at 48 kHz."""
import numpy as np, warnings
warnings.filterwarnings('ignore')
from scipy.optimize import least_squares
from data import rows, F
from mb import poles2, dig_db

def design(f0, Q, c1, c2, fs):
    a1, a2 = poles2(2 * np.pi * min(f0, 0.499 * fs * 2), Q, 1 / fs)
    a = np.array([1, a1, a2])
    b = np.array([1, c1, c2])
    b = b * (np.sum(a) / np.sum(b))
    return b, a

def minphase(c1, c2):
    z = np.roots([1, c1, c2]).astype(complex)
    z = np.where(np.abs(z) > 1, 1 / np.conj(z), z)
    p = np.real(np.poly(z))
    return p[1], p[2]

def fit():
    out = []
    x = [30000, 1.0, 0.6, 0.2]
    for r in ['1.00', '0.90', '0.80', '0.70', '0.60', '0.50', '0.40', '0.30', '0.20', '0.10']:
        y = rows['lpn_' + r]
        res = lambda p: np.nan_to_num(dig_db([design(p[0], p[1], p[2], p[3], 48000.)], 48000.) - y, nan=100.)
        best = None
        for x0 in ([3000 if r == '1.00' else out[-1][1], 0.67, 0.6, 0.2], [8000, 0.7, 0.6, 0.2], [20000, 1.0, 0.8, 0.3]):
            s = least_squares(res, x0, bounds=([1000, 0.3, -2, -1], [30000, 4, 2, 1]))
            if best is None or s.cost < best.cost:
                best = s
        f0, Q, c1, c2 = best.x
        c1, c2 = minphase(c1, c2)
        out.append((float(r), f0, Q, c1, c2, np.abs(res(best.x)).max()))
    return sorted(out)

if __name__ == '__main__':
    for r, f0, Q, c1, c2, e in fit():
        print('%.1f f0=%.1f Q=%.4f c1=%.5f c2=%.5f err48=%.3f' % (r, f0, Q, c1, c2, e))
