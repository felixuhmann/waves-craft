// Craft Flanger - parameter mappings and quantisation (spec r1 section 2).
// Host-independent: no JUCE dependency.
#pragma once

#include <cmath>

namespace craft::flanger::dsp
{

// Round x to the nearest multiple of 1/stepsPerUnit (ties away from zero).
inline double quantise (double x, double stepsPerUnit) noexcept
{
    return std::round (x * stepsPerUnit) / stepsPerUnit;
}

inline double clamp01 (double r) noexcept { return r < 0.0 ? 0.0 : (r > 1.0 ? 1.0 : r); }
inline double clampTo (double v, double lo, double hi) noexcept { return v < lo ? lo : (v > hi ? hi : v); }

// Each continuous parameter: range, default, mapping r -> value (quantised to the internal resolution)
// and the inverse value -> r (exact inverse of the continuous mapping, unquantised).
namespace param
{
    // Delay: 0.1 * 500^r ms, resolution 0.1 ms, default 2.2 ms
    struct Delay
    {
        static constexpr double minValue = 0.1, maxValue = 50.0, defaultValue = 2.2;
        static double fromNorm (double r) noexcept
        {
            return clampTo (quantise (0.1 * std::pow (500.0, clamp01 (r)), 10.0), minValue, maxValue);
        }
        static double toNorm (double v) noexcept
        {
            if (v <= minValue) return 0.0;
            return clamp01 (std::log (v / 0.1) / std::log (500.0));
        }
    };

    // Linear percentage 0 ... 100 %, resolution 1 % (Depth, Mix, Feedback)
    template <int Default>
    struct Percent
    {
        static constexpr double minValue = 0.0, maxValue = 100.0, defaultValue = Default;
        static double fromNorm (double r) noexcept { return quantise (100.0 * clamp01 (r), 1.0); }
        static double toNorm (double v) noexcept { return clamp01 (v / 100.0); }
    };
    using Depth = Percent<100>;
    using Mix = Percent<50>;
    using Feedback = Percent<50>;

    // Rate: 0.005 * 4000^r Hz, rounded to 0.005 Hz; values below 0.01 Hz become 0 (static LFO). Default 0.20 Hz.
    struct Rate
    {
        static constexpr double minValue = 0.0, maxValue = 20.0, defaultValue = 0.2;
        static double fromNorm (double r) noexcept
        {
            const double v = clampTo (quantise (0.005 * std::pow (4000.0, clamp01 (r)), 200.0), 0.0, maxValue);
            return v < 0.01 - 1.0e-9 ? 0.0 : v;
        }
        static double toNorm (double v) noexcept
        {
            if (v <= 0.005) return 0.0;
            return clamp01 (std::log (v / 0.005) / std::log (4000.0));
        }
    };

    // Output Gain: -12 ... +12 dB linear in r, resolution 0.1 dB, default 0 dB
    struct Gain
    {
        static constexpr double minValue = -12.0, maxValue = 12.0, defaultValue = 0.0;
        static double fromNorm (double r) noexcept { return quantise (-12.0 + 24.0 * clamp01 (r), 10.0); }
        static double toNorm (double v) noexcept { return clamp01 ((v + 12.0) / 24.0); }
    };

    // Stereo: 180 r degrees, resolution 1 degree, default 0
    struct Stereo
    {
        static constexpr double minValue = 0.0, maxValue = 180.0, defaultValue = 0.0;
        static double fromNorm (double r) noexcept { return quantise (180.0 * clamp01 (r), 1.0); }
        static double toNorm (double v) noexcept { return clamp01 (v / 180.0); }
    };

    // Filter Freq: 100 * 200^r Hz; quantised to the display resolution (100 Hz below 10 kHz, 1 kHz from 10 kHz).
    struct FilterFreq
    {
        static constexpr double minValue = 100.0, maxValue = 20000.0, defaultValue = 1000.0;
        static double quantiseHz (double hz) noexcept
        {
            double q = quantise (hz, 0.01); // 100 Hz steps
            if (q >= 10000.0)
                q = quantise (hz, 0.001); // 1 kHz steps
            return clampTo (q, minValue, maxValue);
        }
        static double fromNorm (double r) noexcept { return quantiseHz (100.0 * std::pow (200.0, clamp01 (r))); }
        static double toNorm (double v) noexcept
        {
            if (v <= minValue) return 0.0;
            return clamp01 (std::log (v / 100.0) / std::log (200.0));
        }
    };

    // Snap an arbitrary physical value to the nearest value reachable through the mapping.
    template <typename P>
    double snap (double v) noexcept { return P::fromNorm (P::toNorm (v)); }
} // namespace param

} // namespace craft::flanger::dsp
