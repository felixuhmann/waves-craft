// Craft Peak Limiter - parameter mapping and quantisation (spec r1 section 2).
// Host-independent: no JUCE dependency.
#pragma once

#include <cmath>
#include <cstdio>
#include <string>

namespace craft::limiter
{

// Round x to the nearest multiple of 1/stepsPerUnit (ties away from zero).
inline double quantise (double x, double stepsPerUnit) noexcept
{
    return std::round (x * stepsPerUnit) / stepsPerUnit;
}

inline double clamp01 (double r) noexcept { return r < 0.0 ? 0.0 : (r > 1.0 ? 1.0 : r); }

// Each parameter: range, default, mapping r -> value (quantised to the displayed resolution)
// and the inverse value -> r (exact inverse of the continuous mapping).
namespace param
{
    // Threshold: -30 + 30 r dB, 0.1 dB resolution, default 0 dB
    struct Threshold
    {
        static constexpr double minValue = -30.0, maxValue = 0.0, defaultValue = 0.0, stepsPerUnit = 10.0;
        static double fromNorm (double r) noexcept { return quantise (-30.0 + 30.0 * clamp01 (r), stepsPerUnit); }
        static double toNorm (double v) noexcept { return clamp01 ((v + 30.0) / 30.0); }
    };

    // Out Ceiling: same mapping as Threshold
    struct Ceiling : Threshold
    {
    };

    // Release: 0.01 * 10^(5 r) ms, 0.01 ms resolution (the internal value is the 2-decimal displayed value),
    // default 1.00 ms (r = 0.4)
    struct Release
    {
        static constexpr double minValue = 0.01, maxValue = 1000.0, defaultValue = 1.0, stepsPerUnit = 100.0;
        static double fromNorm (double r) noexcept
        {
            const double v = quantise (0.01 * std::pow (10.0, 5.0 * clamp01 (r)), stepsPerUnit);
            return v < minValue ? minValue : (v > maxValue ? maxValue : v);
        }
        static double toNorm (double v) noexcept
        {
            if (v <= minValue) return 0.0;
            return clamp01 (std::log10 (v / 0.01) / 5.0);
        }
    };

    // Snap an arbitrary physical value to the nearest value reachable through the mapping.
    template <typename P>
    double snap (double v) noexcept { return P::fromNorm (P::toNorm (v)); }

    // Display formats (spec section 2): "-12.0" dB, "0.03" / "316.23" / "1000" ms.
    inline std::string formatDb (double v)
    {
        v = quantise (v, 10.0);
        if (v == 0.0)
            v = 0.0; // no "-0.0"
        char buf[32];
        std::snprintf (buf, sizeof (buf), "%.1f", v);
        return buf;
    }

    inline std::string formatReleaseMs (double v)
    {
        v = quantise (v, 100.0);
        char buf[32];
        if (v >= 1000.0)
            std::snprintf (buf, sizeof (buf), "%.0f", v);
        else
            std::snprintf (buf, sizeof (buf), "%.2f", v);
        return buf;
    }
} // namespace param

// The settings the engine runs with (physical units, already quantised).
struct Settings
{
    double thresholdDb = 0.0;
    double ceilingDb = 0.0;
    double releaseMs = 1.0;
    bool autoRelease = false;
};

} // namespace craft::limiter
