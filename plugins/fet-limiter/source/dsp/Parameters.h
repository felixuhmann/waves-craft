// Craft FET Limiter - parameter mappings and display strings (spec r1 section 2). Host independent.
#pragma once

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <string>

#include "Tables.h"

namespace craft::fet_limiter::dsp
{

enum class Analog
{
    Hz50 = 0,
    Hz60 = 1,
    Off = 2
};

enum class MeterMode
{
    GR = 0,
    In = 1,
    Out = 2
};

// Complete processing settings (display / physical units).
struct Settings
{
    double inputDisplay = -30.0;  // -60 (= -inf) .. 0
    double outputDisplay = -18.0; // -60 (= -inf) .. 0
    double attack = 3.0;          // 1 (slowest) .. 7 (fastest)
    double release = 4.0;         // 1 (slowest) .. 7 (fastest)
    Ratio ratio = Ratio::R4;
    Analog analog = Analog::Off;
    Revision revision = Revision::A;
    bool compOff = false;
    double mixPercent = 100.0;
    double trimDb = 0.0;
};

namespace param
{
    inline double snapTo (double v, double invStep) { return std::round (v * invStep) / invStep + 0.0; }

    // Input / Output: r = 0 -> -inf; r <= 0.25: -60 + 96 r; r >= 0.25: -36 + 48 (r - 0.25); 0.1 dB steps.
    // The value -60 stands for -inf.
    struct Level
    {
        static constexpr double minValue = -60.0, maxValue = 0.0;
        static double fromNorm (double r)
        {
            r = std::clamp (r, 0.0, 1.0);
            if (r <= 0.0)
                return minValue;
            const double d = r <= 0.25 ? -60.0 + 96.0 * r : -36.0 + 48.0 * (r - 0.25);
            return snap (d);
        }
        static double toNorm (double d)
        {
            if (d <= minValue)
                return 0.0;
            d = std::min (d, maxValue);
            return d <= -36.0 ? (d + 60.0) / 96.0 : 0.25 + (d + 36.0) / 48.0;
        }
        static double snap (double d) { return std::clamp (snapTo (d, 10.0), minValue, maxValue); }
    };

    // Attack / Release: 1 + 6 r, two decimals; 1 = slowest, 7 = fastest.
    struct Time
    {
        static constexpr double minValue = 1.0, maxValue = 7.0;
        static double fromNorm (double r) { return snap (1.0 + 6.0 * std::clamp (r, 0.0, 1.0)); }
        static double toNorm (double v) { return std::clamp ((v - 1.0) / 6.0, 0.0, 1.0); }
        static double snap (double v) { return std::clamp (snapTo (v, 100.0), minValue, maxValue); }
    };

    // Mix: 100 r %, 0.1 % steps.
    struct Mix
    {
        static constexpr double minValue = 0.0, maxValue = 100.0;
        static double fromNorm (double r) { return snap (100.0 * std::clamp (r, 0.0, 1.0)); }
        static double toNorm (double v) { return std::clamp (v / 100.0, 0.0, 1.0); }
        static double snap (double v) { return std::clamp (snapTo (v, 10.0), minValue, maxValue); }
    };

    // Trim: -18 + 36 r dB, 0.1 dB steps.
    struct Trim
    {
        static constexpr double minValue = -18.0, maxValue = 18.0;
        static double fromNorm (double r) { return snap (-18.0 + 36.0 * std::clamp (r, 0.0, 1.0)); }
        static double toNorm (double v) { return std::clamp ((v + 18.0) / 36.0, 0.0, 1.0); }
        static double snap (double v) { return std::clamp (snapTo (v, 10.0), minValue, maxValue); }
    };

    // Ratio: r < 0.1125 -> 20; < 0.3625 -> 12; < 0.6125 -> 8; < 0.8625 -> 4; else All. Index 0..4 = 20/12/8/4/All;
    // the canonical host value of index k is k / 4 (inside its band).
    inline int ratioIndexFromNorm (double r)
    {
        if (r < 0.1125) return 0;
        if (r < 0.3625) return 1;
        if (r < 0.6125) return 2;
        if (r < 0.8625) return 3;
        return 4;
    }
    inline double ratioNormFromIndex (int i) { return std::clamp (i, 0, 4) / 4.0; }

    // Analog: r < 0.2375 -> 50 Hz; < 0.7375 -> 60 Hz; else Off. Canonical host values 0 / 0.5 / 1.
    inline int analogIndexFromNorm (double r)
    {
        if (r < 0.2375) return 0;
        if (r < 0.7375) return 1;
        return 2;
    }
    inline double analogNormFromIndex (int i) { return std::clamp (i, 0, 2) / 2.0; }

    // Revision: r < 0.5 -> B (index 0); else A (index 1).
    inline int revisionIndexFromNorm (double r) { return r < 0.5 ? 0 : 1; }
    inline double revisionNormFromIndex (int i) { return i <= 0 ? 0.0 : 1.0; }

    // Meter (display only): GR / In / Out, canonical host values 0 / 0.5 / 1.
    inline int meterIndexFromNorm (double r) { return r < 0.25 ? 0 : (r < 0.75 ? 1 : 2); }
} // namespace param

inline const char* ratioLabel (int index)
{
    static const char* labels[] = { "20", "12", "8", "4", "All" };
    return labels[std::clamp (index, 0, 4)];
}
inline const char* analogLabel (int index)
{
    static const char* labels[] = { "50 Hz", "60 Hz", "Off" };
    return labels[std::clamp (index, 0, 2)];
}
inline const char* revisionLabel (int index) { return index <= 0 ? "B" : "A"; }

inline std::string formatLevel (double d)
{
    if (d <= param::Level::minValue)
        return "-inf dB";
    char buf[32];
    if (std::abs (d) < 0.05)
        d = 0.0;
    std::snprintf (buf, sizeof buf, "%.1f dB", d);
    return buf;
}

inline std::string formatDb (double d)
{
    char buf[32];
    if (std::abs (d) < 0.05)
        d = 0.0;
    std::snprintf (buf, sizeof buf, "%+.1f dB", d);
    if (d == 0.0)
        std::snprintf (buf, sizeof buf, "0.0 dB");
    return buf;
}

inline std::string formatTime (double v)
{
    char buf[32];
    std::snprintf (buf, sizeof buf, "%.2f", v);
    return buf;
}

inline std::string formatPercent (double v)
{
    char buf[32];
    std::snprintf (buf, sizeof buf, "%.1f %%", v);
    return buf;
}

// Tolerant number parser: accepts "-12", "-12.0 dB", "  3,5", "-inf".
inline double parseNumber (std::string text, double fallback)
{
    std::replace (text.begin(), text.end(), ',', '.');
    const char* s = text.c_str();
    while (*s == ' ' || *s == '\t')
        ++s;
    std::string lower (s);
    std::transform (lower.begin(), lower.end(), lower.begin(), [] (unsigned char c) { return char (std::tolower (c)); });
    if (lower.rfind ("-inf", 0) == 0 || lower.rfind ("inf", 0) == 0 || lower.rfind ("off", 0) == 0)
        return -1.0e9;
    char* end = nullptr;
    const double v = std::strtod (s, &end);
    return end == s ? fallback : v;
}

} // namespace craft::fet_limiter::dsp
