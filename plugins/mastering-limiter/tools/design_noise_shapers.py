#!/usr/bin/env python3
"""Design the noise-transfer filters (NTF) of Craft Mastering Limiter from spec r1 section 7.3.

Development tool (not part of the build): fits a monic, minimum-phase FIR NTF
    H(z) = 1 + h1 z^-1 + ... + h(M-1) z^-(M-1)
to the tabulated NTF magnitudes of the spec, per sample rate and shaping mode, and writes
source/dsp/NoiseShapingCoefficients.h.

Method (public textbook material: Gerzon & Craven, "Optimal noise shaping and dither of digital signals",
AES 1989; Lipshitz, Vanderkooy & Wannamaker, "Minimally audible noise shaping", JAES 1991):
  1. The target log-magnitude is interpolated linearly (dB over linear frequency) between the table points.
     A monic minimum-phase filter has zero mean log-magnitude over 0..fs/2 (Gerzon-Craven theorem); the
     spec tables satisfy this to within a few tenths of a dB, the remainder is removed by a constant offset.
  2. Initial guess: minimum-phase impulse response from the real cepstrum of the target (folded cepstrum),
     truncated to M taps.
  3. Weighted non-linear least squares on the dB magnitude (table points weighted strongly, a dense grid
     weakly to keep the response smooth between the points), h0 fixed to 1.
  4. Minimum phase is verified from the polynomial roots (all strictly inside the unit circle).

Usage: python3 design_noise_shapers.py <output header>
"""
import sys

import numpy as np
from scipy.optimize import least_squares

# ---------------------------------------------------------------------------------------------
# spec r1 section 7.3 (dB, NTF magnitude = PSD of the shaped error relative to white)
F48 = [0.1, 0.5, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23]
T48 = {
    "Moderate": [0.3, -1.7, -5.4, -10.1, -12.1, -12.6, -12.0, -10.7, -8.9, -7.5, -6.8, -6.1, -4.8, -2.2, 1.7, 6.4, 9.1,
                 9.8, 9.8, 9.8, 9.8, 9.7, 9.8, 9.9, 9.8],
    "Normal": [-2.5, -5.5, -9.7, -13.9, -16.2, -17.0, -16.8, -15.7, -14.0, -11.9, -10.0, -8.5, -6.9, -4.8, -1.5, 3.5,
               10.3, 15.0, 16.4, 16.5, 16.5, 16.4, 16.4, 16.5, 16.4],
    "Ultra": [-5.5, -8.6, -12.1, -16.3, -18.8, -19.6, -19.5, -18.6, -17.0, -15.1, -13.2, -11.4, -9.7, -7.6, -4.6, -0.2,
              6.4, 16.1, 21.5, 23.0, 23.1, 22.9, 22.8, 23.0, 23.0],
}
F44 = [0.1, 0.5, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22]
T44 = {
    "Moderate": [2.0, -0.9, -5.1, -9.6, -11.5, -11.9, -11.0, -9.4, -7.7, -6.7, -6.2, -5.4, -3.8, -1.1, 2.8, 7.1, 9.9,
                 10.9, 10.7, 10.5, 10.6, 10.7, 10.7, 10.6],
    "Normal": [-1.0, -4.3, -7.8, -12.4, -14.3, -14.8, -14.1, -12.6, -10.9, -9.7, -9.0, -8.2, -6.8, -4.3, -0.6, 4.6, 10.9,
               16.0, 17.7, 17.7, 17.6, 17.8, 17.7, 17.6],
    "Ultra": [-2.2, -5.8, -9.7, -14.3, -16.5, -16.8, -16.1, -14.7, -13.0, -11.8, -11.2, -10.5, -9.1, -6.7, -3.1, 2.0, 8.8,
              17.3, 22.8, 24.3, 24.2, 24.3, 24.4, 24.3],
}
F96 = [0.1, 1, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30, 32, 34, 36, 38, 40, 42, 44, 46]
T96 = {
    "Moderate": [-22.6, -22.6, -22.2, -21.5, -20.1, -18.1, -15.2, -11.2, -5.8, 2.1, 8.2, 7.4, 7.2, 8.2, 8.4, 7.9, 7.8,
                 8.2, 8.4, 8.2, 7.9, 8.0, 8.3, 8.4, 8.2],
    "Normal": [-27.4, -27.6, -27.4, -26.7, -25.8, -24.0, -22.0, -19.3, -15.8, -11.4, -6.0, 1.3, 10.3, 15.1, 14.9, 14.0,
               13.8, 14.5, 15.2, 15.3, 15.0, 15.3, 15.5, 14.9, 14.0],
    "Ultra": [-31.0, -30.6, -30.5, -30.0, -28.9, -27.4, -25.3, -22.7, -19.4, -15.2, -9.5, -1.6, 13.9, 16.5, 12.1, 14.0,
              17.7, 19.6, 19.6, 18.6, 17.6, 17.8, 18.9, 19.4, 19.2],
}
MODES = ["Moderate", "Normal", "Ultra"]
TAPS = 40  # NTF length (h0 .. h39)
NGRID = 2048


def target_points(rate, mode):
    """Return (freqs Hz, dB, tolerance dB) the design must meet at this rate."""
    if rate == 44100:
        return np.array(F44) * 1e3, np.array(T44[mode]), np.full(len(F44), 1.5)
    if rate == 48000:
        return np.array(F48) * 1e3, np.array(T48[mode]), np.full(len(F48), 1.5)
    if rate in (88200, 96000):
        f = np.array(F96) * 1e3
        keep = f < rate / 2
        tol = np.where(f > 20e3, 2.5, 1.5)
        return f[keep], np.array(T96[mode])[keep], tol[keep]
    # 176.4 / 192 kHz: own design (spec 7.3 / open question 3): the 96 kHz curve minus 1 dB up to 20 kHz,
    # a plateau above 26 kHz whose level is set by the zero-mean log-magnitude constraint.
    f = np.array([x for x in F96 if x <= 20]) * 1e3
    return f, np.array(T96[mode])[: len(f)] - 1.0, np.full(len(f), 1.5)


def dense_target(rate, mode, grid):
    f, db, _ = target_points(rate, mode)
    if rate in (176400, 192000):
        # plateau level P: solve mean(dB) = 0 over the grid with the transition 20 -> 26 kHz
        def build(P):
            ff = np.concatenate([f, [26e3, rate / 2]])
            dd = np.concatenate([db, [P, P]])
            return np.interp(grid, ff, dd)
        lo, hi = -10.0, 40.0
        for _ in range(60):
            mid = 0.5 * (lo + hi)
            if np.mean(build(mid)) > 0:
                hi = mid
            else:
                lo = mid
        return build(0.5 * (lo + hi))
    t = np.interp(grid, f, db)
    if rate == 88200:
        # the 96 kHz curve cut at 44.1 kHz has a negative mean: lift it in proportion to the tolerance
        # (+-1.5 dB up to 20 kHz, +-2.5 dB above) so that the zero-mean constraint can be met
        w = np.where(grid > 20e3, 2.5, 1.0)
        return t - np.mean(t) * w / np.mean(w)
    return t - np.mean(t)  # Gerzon-Craven: a monic minimum-phase NTF has zero mean log magnitude


def min_phase_from_db(db_half):
    """Minimum-phase impulse response from a magnitude given on a half-spectrum grid (0..fs/2 inclusive)."""
    n = 2 * (len(db_half) - 1)
    logmag = np.log(10 ** (db_half / 20))
    full = np.concatenate([logmag, logmag[-2:0:-1]])
    cep = np.fft.ifft(full).real
    fold = np.zeros(n)
    fold[0] = cep[0]
    fold[1 : n // 2] = 2 * cep[1 : n // 2]
    fold[n // 2] = cep[n // 2]
    return np.fft.ifft(np.exp(np.fft.fft(fold))).real


def response_db(h, freqs, rate):
    w = 2 * np.pi * freqs / rate
    k = np.arange(len(h))
    H = np.exp(-1j * np.outer(w, k)) @ h
    return 20 * np.log10(np.maximum(np.abs(H), 1e-12))


def design(rate, mode):
    grid = np.linspace(0, rate / 2, NGRID + 1)
    tgt = dense_target(rate, mode, grid)
    h0 = min_phase_from_db(tgt)[:TAPS]
    h0 = h0 / h0[0]
    fpts, dbpts, _ = target_points(rate, mode)
    if rate == 88200:
        # same tolerance-proportional lift as in dense_target, evaluated exactly at the table points
        t = np.interp(grid, fpts, dbpts)
        wg = np.where(grid > 20e3, 2.5, 1.0)
        dbpts = dbpts - np.mean(t) * np.where(fpts > 20e3, 2.5, 1.0) / np.mean(wg)
    if rate in (176400, 192000):
        sel = grid[::8]
        fpts, dbpts = sel, np.interp(sel, grid, tgt)
        wpts = np.where(sel <= 20e3, 6.0, 1.0)
    else:
        wpts = np.full(len(fpts), 6.0)
    gsel = grid[::4]
    gt = np.interp(gsel, grid, tgt)
    freqs = np.concatenate([fpts, gsel])
    want = np.concatenate([dbpts, gt])
    wts = np.concatenate([wpts, np.full(len(gsel), 0.3)])
    E = np.exp(-1j * np.outer(2 * np.pi * freqs / rate, np.arange(TAPS)))
    k20 = 20 / np.log(10)

    def resid(x):
        H = E @ np.concatenate([[1.0], x])
        return wts * (k20 * np.log(np.maximum(np.abs(H), 1e-12)) - want)

    def jac(x):
        H = E @ np.concatenate([[1.0], x])
        J = k20 * np.real(np.conj(H)[:, None] * E[:, 1:]) / (np.abs(H) ** 2)[:, None]
        return wts[:, None] * J

    sol = least_squares(resid, h0[1:], jac=jac, method="lm", max_nfev=5000)
    h = np.concatenate([[1.0], sol.x])
    roots = np.roots(h)
    rmax = np.max(np.abs(roots))
    return h, rmax


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else "NoiseShapingCoefficients.h"
    rates = [44100, 48000, 88200, 96000, 176400, 192000]
    lines = []
    ok = True
    for rate in rates:
        for mode in MODES:
            h, rmax = design(rate, mode)
            f, db, tol = target_points(rate, mode)
            err = response_db(h, f, rate) - db
            if rate in (176400, 192000):
                # own design: only "NTF <= the 96 kHz curve below 20 kHz" (spec 7.3), checked with 0.3 dB slack
                err = np.maximum(response_db(h, f, rate) - (db + 1.0) - 0.3, 0.0)
            grid = np.linspace(0, rate / 2, 8193)
            power = np.mean(10 ** (response_db(h, grid, rate) / 10))
            print(f"{rate:6d} {mode:8s} max|err| {np.max(np.abs(err)):.2f} dB  sum h^2 {np.sum(h * h):7.3f}"
                  f" (TPDF total {0.5 * np.sqrt(np.sum(h * h)):.2f} LSB)  max|root| {rmax:.4f}")
            if rmax >= 0.999 or np.any(np.abs(err) > tol):
                ok = False
                print("   FAILED", np.round(err, 2))
            lines.append((rate, mode, h))

    with open(out, "w", encoding="utf-8") as fh:
        fh.write("// Generated by tools/design_noise_shapers.py from spec r1 section 7.3 - do not edit by hand.\n")
        fh.write("// Monic minimum-phase FIR noise-transfer filters H(z) = sum h[k] z^-k, h[0] = 1.\n")
        fh.write("#pragma once\n\n#include <array>\n\nnamespace craft::mastering_limiter::dsp::ntf\n{\n\n")
        fh.write(f"inline constexpr int numTaps = {TAPS};\n")
        fh.write(f"inline constexpr int numRates = {len(rates)};\n")
        fh.write("inline constexpr std::array<double, numRates> designRates { "
                 + ", ".join(f"{r}.0" for r in rates) + " };\n\n")
        fh.write("// [rate][mode: 0 Moderate, 1 Normal, 2 Ultra][tap]\n")
        fh.write("inline constexpr double coefficients[numRates][3][numTaps] = {\n")
        for ri, rate in enumerate(rates):
            fh.write(f"    {{ // {rate} Hz\n")
            for mi, mode in enumerate(MODES):
                h = lines[ri * 3 + mi][2]
                fh.write(f"        {{ // {mode}\n")
                for k in range(0, TAPS, 4):
                    fh.write("            " + ", ".join(f"{v: .17e}" for v in h[k : k + 4]) + ",\n")
                fh.write("        },\n")
            fh.write("    },\n")
        fh.write("};\n\n} // namespace craft::mastering_limiter::dsp::ntf\n")
    print("wrote", out)
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
