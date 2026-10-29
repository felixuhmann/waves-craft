// Craft Console E - control tapers (spec r1 section 2). Host-normalised r in [0, 1] is the native value of every
// continuous control; the tapers below give the displayed value.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace craft::consolee::dsp::param
{

// Piecewise-linear taper with knots at r = 0, 0.2, 0.5, 0.8, 1 (spec section 2).
struct Taper
{
    std::array<double, 5> v;

    static constexpr std::array<double, 5> knots { 0.0, 0.2, 0.5, 0.8, 1.0 };

    double operator() (double r) const noexcept
    {
        r = std::clamp (r, 0.0, 1.0);
        for (int i = 0; i < 4; ++i)
            if (r <= knots[size_t (i + 1)])
                return v[size_t (i)] + (v[size_t (i + 1)] - v[size_t (i)]) * (r - knots[size_t (i)]) / (knots[size_t (i + 1)] - knots[size_t (i)]);
        return v[4];
    }

    // inverse (the tapers are monotonic); clamps to the range
    double toNorm (double value) const noexcept
    {
        const bool rising = v[4] > v[0];
        if (rising ? value <= v[0] : value >= v[0]) return 0.0;
        if (rising ? value >= v[4] : value <= v[4]) return 1.0;
        for (int i = 0; i < 4; ++i)
        {
            const double a = v[size_t (i)], b = v[size_t (i + 1)];
            if ((rising && value <= b) || (! rising && value >= b))
                return knots[size_t (i)] + (knots[size_t (i + 1)] - knots[size_t (i)]) * (value - a) / (b - a);
        }
        return 1.0;
    }
};

inline constexpr Taper inputTrim { { -18.0, -10.8, 0.0, 10.8, 18.0 } };
inline constexpr Taper outputFader { { -24.0, -16.8, -6.0, 4.8, 12.0 } };
inline constexpr Taper compThreshold { { 10.0, 4.0, -5.0, -14.0, -20.0 } };
inline constexpr Taper compRelease { { 0.1, 0.15, 0.4, 2.0, 4.0 } };
inline constexpr Taper expThreshold { { -30.0, -25.0, -12.0, 5.0, 10.0 } };
inline constexpr Taper expRange { { 0.0, 5.0, 20.0, 33.3, 40.0 } };
inline constexpr Taper expRelease { { 0.1, 0.15, 0.4, 2.0, 4.0 } };
inline constexpr Taper bandGain { { -15.0, -9.0, 0.0, 9.0, 15.0 } };
inline constexpr Taper lfFreq { { 30.0, 50.0, 200.0, 300.0, 450.0 } };       // Hz (label)
inline constexpr Taper lmfFreq { { 0.20, 0.30, 1.00, 2.00, 2.50 } };         // kHz (label)
inline constexpr Taper hmfFreq { { 0.60, 0.80, 3.00, 6.00, 7.00 } };         // kHz (label)
inline constexpr Taper hfFreq { { 1.5, 2.0, 8.0, 14.0, 16.0 } };             // kHz (label)
inline constexpr Taper midQ { { 3.5, 2.5, 1.5, 0.5, 0.1 } };

// Compressor ratio: lookup of 21 values at r = k/20, linear in between (spec section 2). The last segment
// (80 -> infinity) is interpolated in 1/R.
inline constexpr std::array<double, 20> ratioSteps { 1.0, 1.3, 1.5, 1.8, 2.0, 2.3, 2.7, 3.0, 3.7, 4.3,
                                                     5.0, 6.0, 7.0, 8.0, 12.0, 16.0, 20.0, 40.0, 60.0, 80.0 };

// 1/R for control position r (0 for infinity).
inline double ratioInverse (double r) noexcept
{
    r = std::clamp (r, 0.0, 1.0);
    const double x = r * 20.0;
    const int i = std::min (int (x), 19);
    const double t = x - i;
    if (i == 19)
        return (1.0 - t) / ratioSteps[19];
    const double R = ratioSteps[size_t (i)] + t * (ratioSteps[size_t (i + 1)] - ratioSteps[size_t (i)]);
    return 1.0 / R;
}

inline double ratioValue (double r) noexcept
{
    const double inv = ratioInverse (r);
    return inv > 0.0 ? 1.0 / inv : std::numeric_limits<double>::infinity();
}

inline double ratioToNorm (double R) noexcept
{
    if (! (R < 1e9)) return 1.0;
    if (R <= ratioSteps[0]) return 0.0;
    for (int i = 0; i < 19; ++i)
        if (R <= ratioSteps[size_t (i + 1)])
            return (i + (R - ratioSteps[size_t (i)]) / (ratioSteps[size_t (i + 1)] - ratioSteps[size_t (i)])) / 20.0;
    return (19.0 + (1.0 - ratioSteps[19] / R)) / 20.0; // 1/R linear in the last segment
}

// High-pass label (Hz): r = 0 is Off; (0, 0.2] covers 17-20 Hz, then the taper 20, 120, 300, 350.
inline double hpLabel (double r) noexcept
{
    if (r <= 0.0) return 0.0;
    if (r <= 0.2) return 17.0 + 15.0 * r;
    return Taper { { 0.0, 20.0, 120.0, 300.0, 350.0 } }(r);
}

// Low-pass label (kHz): r = 0 is Off; (0, 0.2] is labelled 20 -> 12 kHz (not stated in the spec), then 12, 5, 3.5, 3.
inline double lpLabel (double r) noexcept
{
    if (r <= 0.0) return 0.0;
    if (r <= 0.2) return 20.0 - 40.0 * r;
    return Taper { { 0.0, 12.0, 5.0, 3.5, 3.0 } }(r);
}

// Defaults (normalised). Where the spec gives an approximate r next to a default value, r is chosen so that the
// display shows exactly the default value (IMPLEMENTATION_NOTES.md, assumptions).
namespace defaults
{
    inline const double inputTrim = 0.5;
    inline const double outputFader = 0.5 + 0.3 * 6.0 / 10.8;            // 0 dB
    inline const double compThreshold = 0.2 + 0.3 * 4.0 / 9.0;           // 0 dB
    inline constexpr double compRatio = 0.0;                              // 1:1
    inline constexpr double compRelease = 0.5;                            // 0.4 s
    inline constexpr double expThreshold = 0.0;                           // -30 dB
    inline constexpr double expRange = 0.0;                               // 0 dB
    inline const double expRelease = 0.2 + 0.3 * 0.05 / 0.25;             // 0.20 s
    inline constexpr double hp = 0.0, lp = 0.0;                           // Off
    inline constexpr double gain = 0.5;                                   // 0 dB
    inline const double lfFreq = 0.2 + 0.3 * 10.0 / 150.0;                // 60 Hz
    inline const double lmfFreq = 0.2 + 0.3 * 0.2 / 0.7;                  // 0.50 kHz
    inline const double hmfFreq = 0.5 + 0.3 * 0.5 / 3.0;                  // 3.50 kHz
    inline constexpr double hfFreq = 0.5;                                 // 8.0 kHz
    inline constexpr double lmfQ = 0.2;                                   // 2.5
    inline constexpr double hmfQ = 0.5;                                   // 1.5
} // namespace defaults

} // namespace craft::consolee::dsp::param
