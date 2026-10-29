import numpy as np
from data import F

def poles2(w0, Q, T):
    # z-plane denominator via impulse invariance (exact pole mapping)
    z = 1.0 / (2 * Q)
    if z < 1:
        wd = w0 * np.sqrt(1 - z * z)
        a1 = -2 * np.exp(-z * w0 * T) * np.cos(min(wd * T, np.pi))
    else:
        wd = w0 * np.sqrt(z * z - 1)
        a1 = -2 * np.exp(-z * w0 * T) * np.cosh(wd * T)
    a2 = np.exp(-2 * z * w0 * T)
    return a1, a2

def analog_mag2(num, den, w):
    # num, den: coefficients (s^2, s, 1)
    s = 1j * w
    return np.abs(np.polyval(num, s) / np.polyval(den, s)) ** 2

def matched2(num, den, fs, wm=None):
    """num=(n2,n1,n0), den=(1,d1,d0) analog; returns b,a digital."""
    T = 1 / fs
    d1, d0 = den[1], den[2]
    w0 = np.sqrt(d0)
    Q = w0 / d1
    a1, a2 = poles2(w0, Q, T)
    A0 = (1 + a1 + a2) ** 2
    A1 = (1 - a1 + a2) ** 2
    A2 = -4 * a2
    def phis(w):
        s = np.sin(w / 2) ** 2
        return 1 - s, s, 4 * (1 - s) * s
    def Amag2(w):
        p0, p1, p2 = phis(w)
        return A0 * p0 + A1 * p1 + A2 * p2
    H0 = analog_mag2(num, den, 0.0) if num[2] != 0 else 0.0
    HN = analog_mag2(num, den, np.pi * fs)
    B0 = H0 * A0
    B1 = HN * A1
    if wm is None:
        wm = min(w0, 0.5 * np.pi * fs)
    wmd = wm * T
    p0, p1, p2 = phis(wmd)
    B2 = (analog_mag2(num, den, wm) * Amag2(wmd) - B0 * p0 - B1 * p1) / p2
    sB0, sB1 = np.sqrt(B0), np.sqrt(B1)
    W = 0.5 * (sB0 + sB1)
    b0 = 0.5 * (W + np.sqrt(max(W * W + B2, 0.0)))
    b1 = 0.5 * (sB0 - sB1)
    b2 = -B2 / (4 * b0)
    return np.array([b0, b1, b2]), np.array([1, a1, a2])

def matched1(num, den, fs):
    """num=(n1,n0), den=(1,d0)."""
    T = 1 / fs
    a1 = -np.exp(-den[1] * T)
    A0 = (1 + a1) ** 2
    A1 = (1 - a1) ** 2
    s = 1j * 0.0
    H0 = abs(num[1] / den[1]) ** 2
    HN = abs(np.polyval(num, 1j * np.pi * fs) / np.polyval(den, 1j * np.pi * fs)) ** 2
    sB0, sB1 = np.sqrt(H0 * A0), np.sqrt(HN * A1)
    return np.array([(sB0 + sB1) / 2, (sB0 - sB1) / 2, 0]), np.array([1, a1, 0])

def dig_db(sections, fs, f=F):
    w = 2 * np.pi * np.asarray(f) / fs
    z = np.exp(-1j * w)
    H = np.ones_like(z)
    for b, a in sections:
        H *= (b[0] + b[1] * z + b[2] * z * z) / (a[0] + a[1] * z + a[2] * z * z)
    return 20 * np.log10(np.abs(H))

def ana_db(sections, f=F):
    s = 2j * np.pi * np.asarray(f)
    H = np.ones_like(s)
    for num, den in sections:
        H *= np.polyval(num, s) / np.polyval(den, s)
    return 20 * np.log10(np.abs(H))

# --- prototypes ---------------------------------------------------------
def lp2(f, Q):
    w = 2 * np.pi * f
    return (0, 0, w * w), (1, w / Q, w * w)

def hp2(f, Q):
    w = 2 * np.pi * f
    return (1, 0, 0), (1, w / Q, w * w)

def bell(f, Gdb, Q):
    # RBJ-style symmetric bell: peak gain Gdb at f
    A = 10 ** (Gdb / 40)
    w = 2 * np.pi * f
    return (1, A * w / Q, w * w), (1, w / (A * Q), w * w)

def lowshelf(f, Gdb, Q):
    A = 10 ** (Gdb / 40)
    w = 2 * np.pi * f
    sA = np.sqrt(A)
    # H(s) = A * (s^2 + sqrt(A)/Q w s + A w^2) / (A s^2 + sqrt(A)/Q w s + w^2)
    return (1, sA / Q * w, A * w * w), (1, sA / Q * w / A, w * w / A)

def highshelf(f, Gdb, Q):
    A = 10 ** (Gdb / 40)
    w = 2 * np.pi * f
    sA = np.sqrt(A)
    # H(s) = A * (A s^2 + sqrt(A)/Q w s + w^2) / (s^2 + sqrt(A)/Q w s + A w^2)
    return (A * A, A * sA / Q * w, A * w * w), (1, sA / Q * w, A * w * w)

def mdig(protos, fs=48000.0, f=F):
    secs = [matched2(n, d, fs) for n, d in protos]
    return dig_db(secs, fs, f)
