// Craft Vocal Compressor - parameter mapping and display (spec r1 section 3, decision D5).
// Host-independent: no JUCE dependency.
#pragma once

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <string>

namespace craft::vocal
{

// Round x to the nearest multiple of 1/stepsPerUnit.
inline double quantise (double x, double stepsPerUnit) noexcept { return std::round (x * stepsPerUnit) / stepsPerUnit; }

inline double clamp01 (double r) noexcept { return r < 0.0 ? 0.0 : (r > 1.0 ? 1.0 : r); }

namespace param
{
    // Values are quantised to the 0.1 dB display resolution (section 3: "values may be quantised").
    constexpr double stepsPerDb = 10.0;

    // Compression: -36 + 36 r dB, default 0 dB.
    struct Compression
    {
        static constexpr double minValue = -36.0, maxValue = 0.0, defaultValue = 0.0;
        static double fromNorm (double r) noexcept { return quantise (-36.0 + 36.0 * clamp01 (r), stepsPerDb); }
        static double toNorm (double v) noexcept { return clamp01 ((v + 36.0) / 36.0); }
    };

    // Gain: same mapping as Compression.
    struct Gain : Compression
    {
    };

    // Gate: r = 0 is "off" (D5); any r > 0 is a threshold of -80 + 80 r dB. The physical value used for "off" is
    // `offValue` (-80 dB, the bottom of the range), which is reported as off. The smallest step above off is -79.9 dB.
    struct Gate
    {
        static constexpr double minValue = -80.0, maxValue = 0.0, defaultValue = -80.0, offValue = -80.0;
        static double fromNorm (double r) noexcept
        {
            r = clamp01 (r);
            if (r <= 0.0)
                return offValue;
            const double v = quantise (-80.0 + 80.0 * r, stepsPerDb);
            return v <= offValue ? offValue + 1.0 / stepsPerDb : v; // r > 0 is never off
        }
        static double toNorm (double v) noexcept { return v <= offValue ? 0.0 : clamp01 ((v + 80.0) / 80.0); }
        static bool isOff (double v) noexcept { return v <= offValue; }
    };

    // Snap an arbitrary physical value to the nearest value reachable through the mapping.
    template <typename P>
    double snap (double v) noexcept
    {
        return P::fromNorm (P::toNorm (v));
    }

    // "-12.3" (0.1 dB, no "-0.0").
    inline std::string formatDb (double v)
    {
        v = quantise (v, stepsPerDb);
        if (v == 0.0)
            v = 0.0;
        char buf[32];
        std::snprintf (buf, sizeof (buf), "%.1f", v);
        return buf;
    }

    // Gate: "-Inf" when off.
    inline std::string formatGate (double v) { return Gate::isOff (v) ? std::string ("-Inf") : formatDb (v); }

    // Parses typed text for the gate ("-inf", "off" or a number).
    inline double parseGate (const std::string& text)
    {
        std::string t;
        for (char c : text)
            if (c != ' ')
                t += char (std::tolower (static_cast<unsigned char> (c)));
        if (t.empty() || t.find ("inf") != std::string::npos || t == "off")
            return Gate::offValue;
        return snap<Gate> (std::atof (t.c_str()));
    }
} // namespace param

// The settings the engine runs with (physical units, already quantised).
struct Settings
{
    double compressionDb = 0.0; // <= 0
    double gateDb = param::Gate::offValue; // offValue = off
    double gainDb = 0.0;
    bool gateOff() const noexcept { return param::Gate::isOff (gateDb); }
};

} // namespace craft::vocal
