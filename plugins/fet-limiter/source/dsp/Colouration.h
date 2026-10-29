// Craft FET Limiter - colouration stage: asymmetric saturation (spec 7.2). Host independent.
//
// Memoryless curve on the internal signal s (internal sine-peak level x dB <=> amplitude 10^(x/20)):
//   y = s / (1 + |s / c|^n)^(1/n) + a2 sp^2 + a3 sp^3,   c, n = cPos, nPos for s >= 0, cNeg, nNeg for s < 0,
//   sp = 120 tanh(s / 120) (keeps the polynomial bounded for extreme inputs).
// The negative half-wave saturates much earlier than the positive one (even-order content, spec 7.2 "asymmetric
// curve"); the small polynomial gives the low-level H2/H3. Constants were fitted to the Comp Off rows of
// tables/harmonics.csv (1 kHz, Input -12 / Output 0), see IMPLEMENTATION_NOTES. Small-signal slope is 1, so the
// gain staging of spec 4 is unaffected. The DC produced by the asymmetry is removed by the 2 Hz coupling high-pass.
#pragma once

#include <cmath>

#include "Tables.h"

namespace craft::fet_limiter::dsp
{

struct SaturatorParams
{
    double cPos, nPos, cNeg, nNeg, a2, a3;
};

inline const SaturatorParams& saturatorParams (Revision rev)
{
    static const SaturatorParams a { 35.7616, 8.0, 7.2785, 2.8623, 0.0028517, 1.03875e-4 };
    static const SaturatorParams b { 37.9593, 8.0, 5.9498, 4.7062, 0.0024892, 6.3204e-5 };
    return rev == Revision::A ? a : b;
}

inline double saturate (const SaturatorParams& p, double s) noexcept
{
    const bool pos = s >= 0.0;
    const double c = pos ? p.cPos : p.cNeg, n = pos ? p.nPos : p.nNeg;
    const double r = std::abs (s) / c;
    double y;
    if (r < 1.0e-4)
        y = s; // (1 + r^n)^(1/n) - 1 < 1e-10 here
    else
        y = s / std::pow (1.0 + std::pow (r, n), 1.0 / n);
    const double sp = 120.0 * std::tanh (s * (1.0 / 120.0));
    return y + sp * sp * (p.a2 + p.a3 * sp);
}

} // namespace craft::fet_limiter::dsp
