// Craft Mastering Limiter - parameter mappings and display strings (spec r1 section 2). Host independent.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace craft::mastering_limiter::dsp
{

enum class Dither
{
    None = 0,
    Type1 = 1, // TPDF +-1 LSB
    Type2 = 2  // low-level dither, silent on digital silence
};

enum class Shaping
{
    None = 0,
    Moderate = 1,
    Normal = 2,
    Ultra = 3
};

inline constexpr int wordLengths[5] = { 24, 22, 20, 18, 16 }; // choice index -> bits (spec 2)

inline int wordLengthForIndex (int index) noexcept { return wordLengths[std::clamp (index, 0, 4)]; }

// Complete processing settings (physical units).
struct Settings
{
    double thresholdDb = 0.0;  // -30 .. 0
    double ceilingDb = 0.0;    // -30 .. 0
    double releaseMs = 1.0;    // 0.01 .. 1000
    bool adaptiveRelease = true;
    int wordLength = 24;       // 24 / 22 / 20 / 18 / 16
    Dither dither = Dither::Type1;
    Shaping shaping = Shaping::Normal;
};

namespace param
{
    // round to a decimal grid; dividing by the inverse step keeps grid values exact (-12.0, not -12.000000000000002)
    inline double snapTo (double v, double invStep) { return std::round (v * invStep) / invStep; }

    // Threshold / Out Ceiling: -30 + 30 r dB, 0.1 dB resolution.
    struct Level
    {
        static constexpr double minValue = -30.0, maxValue = 0.0, defaultValue = 0.0;
        static double fromNorm (double r) { return snap (-30.0 + 30.0 * std::clamp (r, 0.0, 1.0)); }
        static double toNorm (double db) { return std::clamp ((db + 30.0) / 30.0, 0.0, 1.0); }
        static double snap (double db) { return std::clamp (snapTo (db, 10.0), minValue, maxValue) + 0.0; }
    };

    // Release: 0.01 * 10^(5 r) ms; the value is the 2-decimal display value (0.01 ms steps).
    struct Release
    {
        static constexpr double minValue = 0.01, maxValue = 1000.0, defaultValue = 1.0;
        static double fromNorm (double r) { return snap (0.01 * std::pow (10.0, 5.0 * std::clamp (r, 0.0, 1.0))); }
        static double toNorm (double ms) { return std::clamp (std::log10 (std::max (ms, minValue) / 0.01) / 5.0, 0.0, 1.0); }
        static double snap (double ms) { return std::clamp (snapTo (ms, 100.0), minValue, maxValue); }
    };
} // namespace param

// "-12.0 dB" (U+2212 is not used so the text is plain ASCII in every host)
inline std::string formatDb (double db)
{
    char buf[32];
    if (std::abs (db) < 0.05)
        db = 0.0;
    std::snprintf (buf, sizeof buf, "%.1f dB", db);
    return buf;
}

inline std::string formatMs (double ms)
{
    char buf[32];
    std::snprintf (buf, sizeof buf, "%.2f ms", ms);
    return buf;
}

// Tolerant number parser: accepts "-12", "-12.0 dB", "  3,5 ms" (comma as decimal separator).
inline double parseNumber (std::string text, double fallback)
{
    std::replace (text.begin(), text.end(), ',', '.');
    const char* s = text.c_str();
    while (*s == ' ' || *s == '\t')
        ++s;
    char* end = nullptr;
    const double v = std::strtod (s, &end);
    return end == s ? fallback : v;
}

} // namespace craft::mastering_limiter::dsp
