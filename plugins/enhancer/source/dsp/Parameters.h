// Craft Enhancer - parameter mappings (spec r1 section 1). Host independent.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>

#include "enhancer/SpecTables.h"

namespace craft::enhancer::dsp
{

inline constexpr int numBands = 5;
inline constexpr int numCrossovers = 4;

// Complete settings in physical units (already quantised).
struct Settings
{
    double inputDb = 0.0;                      // -18 .. +18
    double outputDb = 0.0;                     // -18 .. +18
    double directDb = 0.0;                     // -48 (= Off) or -46.8 .. 0
    std::array<double, numBands> bandDb {};    // 0 (= Off) or 1.2 .. 48
    std::array<double, numCrossovers> crossoverHz { 90.0, 500.0, 2594.0, 9310.0 };
    std::array<double, numBands> width { 1.0, 1.0, 1.0, 1.0, 1.0 };
    double punch = 3.0;                        // 0 .. 5
    std::array<bool, numBands> solo {};
    std::array<bool, numBands> bypass {};
};

namespace param
{
    inline double snapTo (double v, double invStep) { return std::round (v * invStep) / invStep + 0.0; }

    // Input / Output gain: -18 + 36 r dB, 0.1 dB steps.
    struct Gain
    {
        static constexpr double minValue = -18.0, maxValue = 18.0, defaultValue = 0.0;
        static double fromNorm (double r) { return snap (-18.0 + 36.0 * std::clamp (r, 0.0, 1.0)); }
        static double toNorm (double db) { return std::clamp ((db + 18.0) / 36.0, 0.0, 1.0); }
        static double snap (double db) { return std::clamp (snapTo (db, 10.0), minValue, maxValue); }
    };

    // Direct gain: r = 0 -> Off (represented by -48 dB), else -48 + 48 r dB (0.1 dB steps, at least -46.8 dB).
    struct Direct
    {
        static constexpr double minValue = -48.0, maxValue = 0.0, defaultValue = 0.0, offValue = -48.0, minOn = -46.8;
        static bool isOff (double db) { return db <= offValue; }
        static double fromNorm (double r) { return r <= 0.0 ? offValue : snap (-48.0 + 48.0 * std::min (r, 1.0)); }
        static double toNorm (double db) { return isOff (db) ? 0.0 : std::clamp ((db + 48.0) / 48.0, 0.0, 1.0); }
        static double snap (double db)
        {
            if (db <= offValue)
                return offValue;
            return std::clamp (snapTo (db, 10.0), minOn, maxValue);
        }
    };

    // Band gain: r = 0 -> Off (represented by 0 dB), else 48 r dB (0.1 dB steps, at least 1.2 dB).
    struct BandGain
    {
        static constexpr double minValue = 0.0, maxValue = 48.0, defaultValue = 0.0, offValue = 0.0, minOn = 1.2;
        static bool isOff (double db) { return db <= offValue; }
        static double fromNorm (double r) { return r <= 0.0 ? offValue : snap (48.0 * std::min (r, 1.0)); }
        static double toNorm (double db) { return isOff (db) ? 0.0 : std::clamp (db / 48.0, 0.0, 1.0); }
        static double snap (double db)
        {
            if (db <= offValue)
                return offValue;
            return std::clamp (snapTo (db, 10.0), minOn, maxValue);
        }
    };

    // Crossover: taper of tables/crossover_taper.csv, log-linear interpolation; display rounded to 1 Hz.
    struct Crossover
    {
        static constexpr double minValue = 16.0, maxValue = 21357.0;
        static double fromNorm (double r)
        {
            namespace t = tables;
            r = std::clamp (r, 0.0, 1.0);
            const auto n = t::crossover_taper_rows;
            for (std::size_t i = 1; i < n; ++i)
            {
                const double r0 = t::crossover_taper[i - 1][0], r1 = t::crossover_taper[i][0];
                if (r <= r1 || i == n - 1)
                {
                    const double a = (r - r0) / (r1 - r0);
                    const double l0 = std::log (t::crossover_taper[i - 1][1]), l1 = std::log (t::crossover_taper[i][1]);
                    return snap (std::exp (l0 + a * (l1 - l0)));
                }
            }
            return maxValue;
        }
        static double toNorm (double hz)
        {
            namespace t = tables;
            hz = std::clamp (hz, minValue, maxValue);
            const double lh = std::log (hz);
            const auto n = t::crossover_taper_rows;
            for (std::size_t i = 1; i < n; ++i)
            {
                const double l0 = std::log (t::crossover_taper[i - 1][1]), l1 = std::log (t::crossover_taper[i][1]);
                if (lh <= l1 || i == n - 1)
                {
                    const double r0 = t::crossover_taper[i - 1][0], r1 = t::crossover_taper[i][0];
                    return std::clamp (r0 + (lh - l0) / (l1 - l0) * (r1 - r0), 0.0, 1.0);
                }
            }
            return 1.0;
        }
        static double snap (double hz) { return std::clamp (std::round (hz), minValue, maxValue); }
    };

    inline constexpr double crossoverDefaults[numCrossovers] = { 90.0, 500.0, 2594.0, 9310.0 };

    // Band width: w = r + 2 r^2, 0..3, 0.01 steps.
    struct Width
    {
        static constexpr double minValue = 0.0, maxValue = 3.0, defaultValue = 1.0;
        static double fromNorm (double r)
        {
            r = std::clamp (r, 0.0, 1.0);
            return snap (r + 2.0 * r * r);
        }
        static double toNorm (double w) { return std::clamp ((-1.0 + std::sqrt (1.0 + 8.0 * std::clamp (w, 0.0, 3.0))) / 4.0, 0.0, 1.0); }
        static double snap (double w) { return std::clamp (snapTo (w, 100.0), minValue, maxValue); }
    };

    // Punch: 5 r, 0.1 steps.
    struct Punch
    {
        static constexpr double minValue = 0.0, maxValue = 5.0, defaultValue = 3.0;
        static double fromNorm (double r) { return snap (5.0 * std::clamp (r, 0.0, 1.0)); }
        static double toNorm (double p) { return std::clamp (p / 5.0, 0.0, 1.0); }
        static double snap (double p) { return std::clamp (snapTo (p, 10.0), minValue, maxValue); }
    };
} // namespace param

// Crossover ordering (spec 1): a crossover cannot lie below its lower or above its upper neighbour. The lower
// crossover wins when the stored values contradict each other.
inline std::array<double, numCrossovers> orderedCrossovers (const std::array<double, numCrossovers>& f)
{
    std::array<double, numCrossovers> o = f;
    for (int k = 1; k < numCrossovers; ++k)
        o[(size_t) k] = std::max (o[(size_t) k], o[(size_t) k - 1]);
    return o;
}

// ---------------------------------------------------------------------------------------------
// Display strings
inline std::string formatDb (double db)
{
    char buf[32];
    if (std::abs (db) < 0.05)
        db = 0.0;
    std::snprintf (buf, sizeof buf, "%.1fdB", db);
    return buf;
}
inline std::string formatDirect (double db) { return param::Direct::isOff (db) ? std::string ("Off") : formatDb (db); }
inline std::string formatBandGain (double db) { return param::BandGain::isOff (db) ? std::string ("Off") : formatDb (db); }
inline std::string formatHz (double hz)
{
    char buf[32];
    std::snprintf (buf, sizeof buf, "%.0f Hz", hz);
    return buf;
}
inline std::string formatWidth (double w)
{
    char buf[32];
    std::snprintf (buf, sizeof buf, "%.2f", w);
    return buf;
}
inline std::string formatPunch (double p)
{
    char buf[32];
    std::snprintf (buf, sizeof buf, "%.1f", p);
    return buf;
}

// Tolerant number parser: accepts "-12", "-12.0 dB", "  3,5", "2.5k"; "off" gives `offValue`.
inline double parseNumber (std::string text, double fallback, double offValue)
{
    std::string lower;
    for (char c : text)
        lower += (char) std::tolower ((unsigned char) c);
    if (lower.find ("off") != std::string::npos)
        return offValue;
    std::replace (text.begin(), text.end(), ',', '.');
    const char* s = text.c_str();
    while (*s == ' ' || *s == '\t')
        ++s;
    char* end = nullptr;
    double v = std::strtod (s, &end);
    if (end == s)
        return fallback;
    while (*end == ' ')
        ++end;
    if (*end == 'k' || *end == 'K')
        v *= 1000.0;
    return v;
}

} // namespace craft::enhancer::dsp
