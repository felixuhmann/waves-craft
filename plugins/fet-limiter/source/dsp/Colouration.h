// Craft FET Limiter - colouration stage: asymmetric saturation (spec 7.2, r2 13.8, 13.9). Host independent.
//
// On the internal signal s (internal sine-peak level x dB <=> amplitude 10^(x/20)):
//   y = s / (1 + |s / c|^n)^(1/n) + a u^2 / (1 + (u / S)^2) [s + u > 0],   c, n = cPos, nPos for s >= 0, cNeg, nNeg for s < 0
//   u = s through a first-order high-pass (150 Hz rev A, 79 Hz rev B)
// The second term is a one-sided square law. It sets the low-level distortion: its H2 / H3 / H5 stand in the fixed
// relation 0 / -9.4 / -26 dB that the harmonics rows show at 1 kHz and 5 kHz at every level. u makes it frequency
// dependent (H2 about 9 dB lower at 100 Hz, and more H3 because the gate and u are out of phase). The first term is the
// saturation of Comp Off, fitted by tools/fit_saturator.py. Small-signal slope is 1, so the gain staging of spec 4 is
// unaffected. The DC produced by the asymmetry is removed by the 2 Hz coupling high-pass.
#pragma once

#include <cmath>

#include "Filters.h"
#include "Tables.h"

namespace craft::fet_limiter::dsp
{

struct SaturatorParams
{
    double cPos, nPos, cNeg, nNeg, a, s, hpHz;
};

inline const SaturatorParams& saturatorParams (Revision rev)
{
    static const SaturatorParams ra { 40.3322, 9.2, 5.459, 7.1922, 0.0100, 49.4193, 150.0 };
    static const SaturatorParams rb { 52.594, 9.2, 5.4106, 9.2, 0.0082, 28.9739, 79.0 };
    return rev == Revision::A ? ra : rb;
}

class Saturator
{
public:
    void prepare (double sampleRate) { fs = sampleRate; reset(); }
    void setRevision (Revision rev) { hp.setCoeffs (highPass1 (saturatorParams (rev).hpHz, fs)); }
    void reset() noexcept { hp.reset(); }

    double process (const SaturatorParams& p, double s) noexcept
    {
        const double u = hp.process (s);
        const bool pos = s >= 0.0;
        const double c = pos ? p.cPos : p.cNeg, n = pos ? p.nPos : p.nNeg;
        const double r = std::abs (s) / c;
        double y = s;
        if (r >= 0.02) // below, r^n < 1e-12 for every n in use
            y = s / std::pow (1.0 + std::pow (r, n), 1.0 / n);
        if (s + u > 0.0) // gate half-way between s and u, so its mismatch makes the 100 Hz H3 but stays small at high level
        {
            const double v = u / p.s;
            y += p.a * u * u / (1.0 + v * v);
        }
        return y;
    }

private:
    Biquad hp;
    double fs = 48000.0;
};

// Output protection (r2 13.10): transparent up to +36.9 dBFS, above every table level (Comp Off, 0 dBFS input: peaks
// about +36 dBFS); soft ceiling at +39.5 dBFS. Reached only with Trim boosts on top of the saturated Comp Off level.
inline double protectOutput (double y) noexcept
{
    constexpr double knee = 70.0, ceiling = 95.0;
    const double a = std::abs (y);
    if (a <= knee)
        return y;
    const double v = knee + (ceiling - knee) * std::tanh ((a - knee) / (ceiling - knee));
    return y < 0.0 ? -v : v;
}

} // namespace craft::fet_limiter::dsp
