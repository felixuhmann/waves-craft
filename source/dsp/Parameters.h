// Craft Audio stereo imager - parameter mapping and quantisation (spec r1 section 3).
// Host-independent: no JUCE dependency.
#pragma once

#include <cmath>

namespace craft::dsp
{

// Round x to the nearest multiple of 1/stepsPerUnit (ties away from zero).
// Dividing by the integer count keeps decimal steps as exact as possible in binary.
inline double quantise (double x, double stepsPerUnit) noexcept
{
    return std::round (x * stepsPerUnit) / stepsPerUnit;
}

inline double clamp01 (double r) noexcept { return r < 0.0 ? 0.0 : (r > 1.0 ? 1.0 : r); }

// Each parameter: range, default, quantisation resolution, mapping r -> value (quantised)
// and its inverse value -> r (unquantised, exact inverse of the continuous mapping).
namespace param
{
    // Gain: -24 + 24 r dB, resolution 0.1 dB, default 0 dB
    struct Gain
    {
        static constexpr double minValue = -24.0, maxValue = 0.0, defaultValue = 0.0, stepsPerUnit = 10.0;
        static double fromNorm (double r) noexcept { return quantise (-24.0 + 24.0 * clamp01 (r), stepsPerUnit); }
        static double toNorm (double v) noexcept { return clamp01 ((v + 24.0) / 24.0); }
    };

    // Width: w = r + 2 r^2, 0..3, resolution 0.01, default 1.00 (r = 0.5)
    struct Width
    {
        static constexpr double minValue = 0.0, maxValue = 3.0, defaultValue = 1.0, stepsPerUnit = 100.0;
        static double fromNorm (double r) noexcept { r = clamp01 (r); return quantise (r + 2.0 * r * r, stepsPerUnit); }
        static double toNorm (double v) noexcept
        {
            if (v <= 0.0) return 0.0;
            return clamp01 ((-1.0 + std::sqrt (1.0 + 8.0 * v)) / 4.0);
        }
    };

    // Rotation: -45 + 90 r degrees, resolution 0.1 deg, default 0
    struct Rotation
    {
        static constexpr double minValue = -45.0, maxValue = 45.0, defaultValue = 0.0, stepsPerUnit = 10.0;
        static double fromNorm (double r) noexcept { return quantise (-45.0 + 90.0 * clamp01 (r), stepsPerUnit); }
        static double toNorm (double v) noexcept { return clamp01 ((v + 45.0) / 90.0); }
    };

    // Asymmetry: -90 + 180 r degrees, resolution 1 deg, default 0
    struct Asymmetry
    {
        static constexpr double minValue = -90.0, maxValue = 90.0, defaultValue = 0.0, stepsPerUnit = 1.0;
        static double fromNorm (double r) noexcept { return quantise (-90.0 + 180.0 * clamp01 (r), stepsPerUnit); }
        static double toNorm (double v) noexcept { return clamp01 ((v + 90.0) / 180.0); }
    };

    // Shuffle: s = 1 + 2 r, resolution 0.1, default 1.0
    struct Shuffle
    {
        static constexpr double minValue = 1.0, maxValue = 3.0, defaultValue = 1.0, stepsPerUnit = 10.0;
        static double fromNorm (double r) noexcept { return quantise (1.0 + 2.0 * clamp01 (r), stepsPerUnit); }
        static double toNorm (double v) noexcept { return clamp01 ((v - 1.0) / 2.0); }
    };

    // Frequency: 350 * 4^r Hz, resolution 1 Hz, default 650 Hz
    struct Frequency
    {
        static constexpr double minValue = 350.0, maxValue = 1400.0, defaultValue = 650.0, stepsPerUnit = 1.0;
        static double fromNorm (double r) noexcept { return quantise (350.0 * std::pow (4.0, clamp01 (r)), stepsPerUnit); }
        static double toNorm (double v) noexcept
        {
            if (v <= 350.0) return 0.0;
            return clamp01 (std::log (v / 350.0) / std::log (4.0));
        }
    };

    // Bass Trim: -3 + 6 r dB, resolution 0.1 dB, default 0
    struct BassTrim
    {
        static constexpr double minValue = -3.0, maxValue = 3.0, defaultValue = 0.0, stepsPerUnit = 10.0;
        static double fromNorm (double r) noexcept { return quantise (-3.0 + 6.0 * clamp01 (r), stepsPerUnit); }
        static double toNorm (double v) noexcept { return clamp01 ((v + 3.0) / 6.0); }
    };

    // Snap an arbitrary physical value to the nearest value reachable through the mapping.
    template <typename P>
    double snap (double v) noexcept { return P::fromNorm (P::toNorm (v)); }
} // namespace param

} // namespace craft::dsp
