// Craft De-Esser - parameter mapping and quantisation (spec r3 section 3).
// Host-independent: no JUCE dependency.
#pragma once

#include <cmath>

namespace craft::deesser::dsp
{

inline double clamp01 (double r) noexcept { return r < 0.0 ? 0.0 : (r > 1.0 ? 1.0 : r); }

// Round x to the nearest multiple of 1/stepsPerUnit (ties away from zero).
inline double quantise (double x, double stepsPerUnit) noexcept { return std::round (x * stepsPerUnit) / stepsPerUnit; }

enum class SidechainMode { BandPass = 0, HighPass = 1 };
enum class AudioMode { Split = 0, Wideband = 1 };
enum class MonitorMode { Output = 0, Sidechain = 1 };

// Two-state switches: r < 0.5 selects the first state (spec section 3).
inline bool switchState (double r) noexcept { return r >= 0.5; }

namespace param
{
    // Threshold: -80 + 80 r dB, 0.1 dB resolution, default 0 dB (r = 1).
    struct Threshold
    {
        static constexpr double minValue = -80.0, maxValue = 0.0, defaultValue = 0.0, stepsPerUnit = 10.0;
        static double fromNorm (double r) noexcept { return quantise (-80.0 + 80.0 * clamp01 (r), stepsPerUnit); }
        static double toNorm (double v) noexcept { return clamp01 ((v + 80.0) / 80.0); }
    };

    // Freq: 2000 * 8^r Hz, integer Hz, default 5506 Hz (r = 0.4870).
    struct Frequency
    {
        static constexpr double minValue = 2000.0, maxValue = 16000.0, defaultValue = 5506.0, stepsPerUnit = 1.0;
        static double fromNorm (double r) noexcept { return quantise (2000.0 * std::pow (8.0, clamp01 (r)), stepsPerUnit); }
        static double toNorm (double v) noexcept
        {
            if (v <= 2000.0)
                return 0.0;
            return clamp01 (std::log (v / 2000.0) / std::log (8.0));
        }
    };

    // Snap an arbitrary physical value to the nearest value reachable through the mapping.
    template <typename P>
    double snap (double v) noexcept { return P::fromNorm (P::toNorm (v)); }
} // namespace param

// Complete, already mapped settings of the processor.
struct Settings
{
    double thresholdDb = param::Threshold::defaultValue;
    double frequencyHz = param::Frequency::defaultValue;
    SidechainMode sidechain = SidechainMode::HighPass;
    AudioMode audio = AudioMode::Split;
    MonitorMode monitor = MonitorMode::Output;
};

} // namespace craft::deesser::dsp
