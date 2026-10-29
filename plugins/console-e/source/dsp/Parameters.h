// Craft Console E - control tapers (spec r2 section 2). Host-normalised r in [0, 1] is the native value of every
// continuous control; the 21-point tapers below give the value (and display) - linear in r between r = k/20.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace craft::consolee::dsp::param
{

struct Taper
{
    std::array<double, 21> v;

    double operator() (double r) const noexcept
    {
        const double x = std::clamp (r, 0.0, 1.0) * 20.0;
        const int i = std::min (int (x), 19);
        return v[size_t (i)] + (x - i) * (v[size_t (i + 1)] - v[size_t (i)]);
    }

    // inverse (the tapers are monotonic); clamps to the range
    double toNorm (double value) const noexcept
    {
        const bool rising = v[20] > v[0];
        if (rising ? value <= v[0] : value >= v[0]) return 0.0;
        if (rising ? value >= v[20] : value <= v[20]) return 1.0;
        for (int i = 0; i < 20; ++i)
        {
            const double a = v[size_t (i)], b = v[size_t (i + 1)];
            if ((rising && value <= b) || (! rising && value >= b))
                return (i + (b != a ? (value - a) / (b - a) : 0.0)) / 20.0;
        }
        return 1.0;
    }
};

inline constexpr Taper inputTrim { { -18.0, -16.2, -14.4, -12.6, -10.8, -9.0, -7.2, -5.4, -3.6, -1.8, 0.0, 1.8, 3.6, 5.4, 7.2, 9.0, 10.8, 12.6, 14.4, 16.2, 18.0 } };
inline constexpr Taper outputFader { { -24.0, -22.2, -20.4, -18.6, -16.8, -15.0, -13.2, -11.4, -9.6, -7.8, -6.0, -4.2, -2.4, -0.6, 1.2, 3.0, 4.8, 6.6, 8.4, 10.2, 12.0 } };
inline constexpr Taper compThreshold { { 10.0, 8.5, 7.0, 5.5, 4.0, 2.5, 1.0, -0.5, -2.0, -3.5, -5.0, -6.5, -8.0, -9.5, -11.0, -12.5, -14.0, -15.5, -17.0, -18.5, -20.0 } };
inline constexpr Taper compRelease { { 0.10, 0.11, 0.13, 0.14, 0.15, 0.17, 0.18, 0.20, 0.27, 0.33, 0.40, 0.60, 0.80, 1.00, 1.33, 1.67, 2.00, 2.50, 3.00, 3.50, 4.00 } };
inline constexpr Taper expThreshold { { -30.0, -28.7, -27.5, -26.2, -25.0, -22.8, -20.7, -18.5, -16.3, -14.2, -12.0, -9.2, -6.3, -3.5, -0.7, 2.2, 5.0, 6.3, 7.5, 8.7, 10.0 } };
inline constexpr Taper expRange { { 0.0, 1.3, 2.5, 3.8, 5.0, 6.7, 8.3, 10.0, 13.3, 16.7, 20.0, 23.3, 26.7, 30.0, 31.7, 33.3, 35.0, 36.3, 37.5, 38.7, 40.0 } };
inline constexpr Taper expRelease = compRelease;
inline constexpr Taper bandGain { { -15.0, -13.5, -12.0, -10.5, -9.0, -7.5, -6.0, -4.5, -3.0, -1.5, 0.0, 1.5, 3.0, 4.5, 6.0, 7.5, 9.0, 10.5, 12.0, 13.5, 15.0 } };
inline constexpr Taper lfFreq { { 30, 35, 40, 45, 50, 67, 83, 100, 133, 167, 200, 233, 267, 300, 321, 343, 364, 386, 407, 429, 450 } };           // Hz
inline constexpr Taper lmfFreq { { 0.20, 0.23, 0.25, 0.28, 0.30, 0.47, 0.63, 0.80, 0.87, 0.93, 1.00, 1.17, 1.33, 1.50, 1.67, 1.83, 2.00, 2.13, 2.25, 2.37, 2.50 } }; // kHz
inline constexpr Taper hmfFreq { { 0.60, 0.65, 0.70, 0.75, 0.80, 1.03, 1.27, 1.50, 2.00, 2.50, 3.00, 3.50, 4.00, 4.50, 5.00, 5.50, 6.00, 6.25, 6.50, 6.75, 7.00 } }; // kHz
inline constexpr Taper hfFreq { { 1.50, 1.63, 1.75, 1.88, 2.00, 3.00, 4.00, 5.00, 6.00, 7.00, 8.00, 8.67, 9.33, 10.00, 11.33, 12.67, 14.00, 14.50, 15.00, 15.50, 16.00 } }; // kHz
inline constexpr Taper midQ { { 3.50, 3.25, 3.00, 2.75, 2.50, 2.25, 2.00, 1.88, 1.75, 1.63, 1.50, 1.37, 1.25, 1.13, 1.00, 0.88, 0.75, 0.62, 0.50, 0.30, 0.10 } };
// filter labels; entry 0 (r = 0) is Off, (0, 0.05) shows the first label
inline constexpr Taper hpTaper { { 17, 17, 18, 19, 20, 37, 53, 70, 87, 103, 120, 147, 173, 200, 233, 267, 300, 313, 325, 337, 350 } };   // Hz
inline constexpr Taper lpTaper { { 19.5, 19.5, 17.0, 14.5, 12.0, 10.7, 9.3, 8.0, 7.0, 6.0, 5.0, 4.7, 4.3, 4.0, 3.8, 3.7, 3.5, 3.4, 3.3, 3.1, 3.0 } }; // kHz

// Compressor ratio: the 21 points of section 2 (infinity at r = 1); the last segment (80 -> infinity) is linear in 1/R.
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

// High-pass label (Hz) / low-pass label (kHz); 0 = Off (r = 0)
inline double hpLabel (double r) noexcept { return r <= 0.0 ? 0.0 : hpTaper (r); }
inline double lpLabel (double r) noexcept { return r <= 0.0 ? 0.0 : lpTaper (r); }

// Defaults (normalised, spec r2 section 2; they display exactly the stated values).
namespace defaults
{
    inline constexpr double inputTrim = 0.5;
    inline constexpr double outputFader = 2.0 / 3.0;   // 0.0 dB
    inline constexpr double compThreshold = 1.0 / 3.0; // 0.0 dB
    inline constexpr double compRatio = 0.0;           // 1:1
    inline constexpr double compRelease = 0.5;         // 0.40 s
    inline constexpr double expThreshold = 0.0;        // -30 dB
    inline constexpr double expRange = 0.0;            // 0 dB
    inline constexpr double expRelease = 0.35;         // 0.20 s
    inline constexpr double hp = 0.0, lp = 0.0;        // Off
    inline constexpr double gain = 0.5;                // 0 dB
    inline constexpr double lfFreq = 0.23;             // 60 Hz
    inline constexpr double lmfFreq = 0.26;            // 0.50 kHz
    inline constexpr double hmfFreq = 0.55;            // 3.50 kHz
    inline constexpr double hfFreq = 0.5;              // 8.00 kHz
    inline constexpr double lmfQ = 0.2;                // 2.50
    inline constexpr double hmfQ = 0.5;                // 1.50
} // namespace defaults

} // namespace craft::consolee::dsp::param
