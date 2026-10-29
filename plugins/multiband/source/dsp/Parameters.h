// Craft Multiband - parameter mappings, quantisation and display formats (spec r1 section 2).
// Host-independent: no JUCE dependency.
#pragma once

#include <array>
#include <cmath>
#include <cstdio>
#include <string>

namespace craft::multiband
{

inline constexpr int numBands = 4;
inline constexpr int numCrossovers = 3;

inline double clamp01 (double r) noexcept { return r < 0.0 ? 0.0 : (r > 1.0 ? 1.0 : r); }

// Round x to the nearest multiple of 1/stepsPerUnit.
inline double quantise (double x, double stepsPerUnit) noexcept { return std::round (x * stepsPerUnit) / stepsPerUnit; }

// Each parameter: physical range, mapping r -> value (quantised to the displayed resolution) and the inverse
// value -> r of the continuous mapping. The host-normalised value is the spec's r.
namespace param
{
    // Linear dB mapping lo + (hi - lo) r with 0.1 dB resolution.
    template <int Lo, int Hi>
    struct LinearDb
    {
        static constexpr double minValue = Lo, maxValue = Hi, stepsPerUnit = 10.0;
        static double fromNorm (double r) noexcept { return quantise (minValue + (maxValue - minValue) * clamp01 (r), stepsPerUnit); }
        static double toNorm (double v) noexcept { return clamp01 ((v - minValue) / (maxValue - minValue)); }
    };

    using OutputGain = LinearDb<-18, 18>; // default 0.0 dB
    using BandGain = LinearDb<-18, 18>;   // default 0.0 dB
    using BandRange = LinearDb<-24, 18>;  // default -8.0 dB
    using BandThreshold = LinearDb<-80, 0>;

    // Crossover: 16 * (21357/16)^r Hz, 1 Hz resolution.
    struct Crossover
    {
        static constexpr double minValue = 16.0, maxValue = 21357.0, stepsPerUnit = 1.0;
        static double fromNorm (double r) noexcept
        {
            const double v = quantise (minValue * std::pow (maxValue / minValue, clamp01 (r)), stepsPerUnit);
            return v < minValue ? minValue : (v > maxValue ? maxValue : v);
        }
        static double toNorm (double v) noexcept
        {
            if (v <= minValue) return 0.0;
            return clamp01 (std::log (v / minValue) / std::log (maxValue / minValue));
        }
    };

    // Crossover Q: 0.10 + 0.65 r, 0.01 resolution.
    struct CrossoverQ
    {
        static constexpr double minValue = 0.10, maxValue = 0.75, stepsPerUnit = 100.0;
        static double fromNorm (double r) noexcept { return quantise (0.10 + 0.65 * clamp01 (r), stepsPerUnit); }
        static double toNorm (double v) noexcept { return clamp01 ((v - 0.10) / 0.65); }
    };

    // Knee: value 0.75 r (Soft = 0, Hard = 0.75), 0.01 resolution in the display.
    struct Knee
    {
        static constexpr double minValue = 0.0, maxValue = 0.75, stepsPerUnit = 100.0;
        static double fromNorm (double r) noexcept { return quantise (0.75 * clamp01 (r), stepsPerUnit); }
        static double toNorm (double v) noexcept { return clamp01 (v / 0.75); }
    };

    // Attack: 0.5 * 1000^r ms, 0.01 ms resolution.
    struct Attack
    {
        static constexpr double minValue = 0.5, maxValue = 500.0, stepsPerUnit = 100.0;
        static double fromNorm (double r) noexcept
        {
            const double v = quantise (0.5 * std::pow (1000.0, clamp01 (r)), stepsPerUnit);
            return v < minValue ? minValue : (v > maxValue ? maxValue : v);
        }
        static double toNorm (double v) noexcept { return v <= minValue ? 0.0 : clamp01 (std::log (v / 0.5) / std::log (1000.0)); }
    };

    // Release: 5 * 1000^r ms, 0.01 ms resolution.
    struct Release
    {
        static constexpr double minValue = 5.0, maxValue = 5000.0, stepsPerUnit = 100.0;
        static double fromNorm (double r) noexcept
        {
            const double v = quantise (5.0 * std::pow (1000.0, clamp01 (r)), stepsPerUnit);
            return v < minValue ? minValue : (v > maxValue ? maxValue : v);
        }
        static double toNorm (double v) noexcept { return v <= minValue ? 0.0 : clamp01 (std::log (v / 5.0) / std::log (1000.0)); }
    };

    // Snap an arbitrary physical value to the nearest value reachable through the mapping.
    template <typename P>
    double snap (double v) noexcept { return P::fromNorm (P::toNorm (v)); }

    // Defaults (spec section 2).
    inline constexpr double defaultOutputGain = 0.0;
    inline constexpr std::array<double, numCrossovers> defaultCrossover { 92.0, 4000.0, 11071.0 };
    inline constexpr double defaultQ = 0.60;
    inline constexpr double defaultKnee = 0.50;
    inline constexpr double defaultBandGain = 0.0;
    inline constexpr double defaultBandRange = -8.0;
    inline constexpr std::array<double, numBands> defaultThreshold { -27.0, -19.2, -30.1, -35.3 };
    inline constexpr std::array<double, numBands> defaultAttack { 34.99, 16.03, 4.99, 2.00 };
    inline constexpr std::array<double, numBands> defaultRelease { 40.27, 25.00, 9.98, 5.00 };

    // Display formats (without unit).
    inline std::string formatFixed (double v, int decimals)
    {
        const double step = std::pow (10.0, decimals);
        v = std::round (v * step) / step;
        if (v == 0.0)
            v = 0.0; // no "-0.0"
        char buf[32];
        std::snprintf (buf, sizeof (buf), "%.*f", decimals, v);
        return buf;
    }
    inline std::string formatDb (double v) { return formatFixed (v, 1); }
    inline std::string formatHz (double v) { return formatFixed (v, 0); }
    inline std::string formatMs (double v) { return formatFixed (v, 2); }
    inline std::string formatQ (double v) { return formatFixed (v, 2); }
    inline std::string formatKnee (double v)
    {
        if (v <= 0.0) return "Soft";
        if (v >= 0.75) return "Hard";
        return formatFixed (v, 2);
    }
} // namespace param

// Engine settings in physical units.
struct BandSettings
{
    double gainDb = 0.0;
    double rangeDb = -8.0;
    double thresholdDb = -30.0;
    double attackMs = 10.0;
    double releaseMs = 50.0;
    bool solo = false;
    bool bypass = false;
};

struct Settings
{
    double outputGainDb = 0.0;
    std::array<double, numCrossovers> crossoverHz = param::defaultCrossover;
    double q = param::defaultQ;
    double knee = param::defaultKnee; // 0 = Soft ... 0.75 = Hard
    bool autoRelease = true;
    bool opto = false; // Behavior: false = Electro, true = Opto
    std::array<BandSettings, numBands> bands {};

    static Settings defaults()
    {
        Settings s;
        for (int b = 0; b < numBands; ++b)
        {
            auto& bs = s.bands[size_t (b)];
            bs.gainDb = param::defaultBandGain;
            bs.rangeDb = param::defaultBandRange;
            bs.thresholdDb = param::defaultThreshold[size_t (b)];
            bs.attackMs = param::defaultAttack[size_t (b)];
            bs.releaseMs = param::defaultRelease[size_t (b)];
        }
        return s;
    }
};

} // namespace craft::multiband
