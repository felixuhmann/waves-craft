// Craft Dynamics - parameter mappings, quantisation and display strings (spec r1 section 2).
// Host-independent: no JUCE dependency. The normalised host value r follows the spec's reference mappings.
#pragma once

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <string>

namespace craft::dynamics::dsp
{

inline constexpr double minusInfinityDb = -std::numeric_limits<double>::infinity();

inline double clamp01 (double r) noexcept { return r < 0.0 ? 0.0 : (r > 1.0 ? 1.0 : r); }
inline double clampTo (double v, double lo, double hi) noexcept { return v < lo ? lo : (v > hi ? hi : v); }
inline double quantise (double x, double stepsPerUnit) noexcept { return std::round (x * stepsPerUnit) / stepsPerUnit; }

// Round to three significant digits (time constants, frequencies).
inline double roundSignificant (double v, int digits = 3) noexcept
{
    if (v <= 0.0)
        return v;
    const double mag = std::pow (10.0, std::floor (std::log10 (v)) - (digits - 1));
    return std::round (v / mag) * mag;
}

inline double dbToGain (double db) noexcept { return std::pow (10.0, db / 20.0); }
inline double gainToDb (double g) noexcept { return g > 0.0 ? 20.0 * std::log10 (g) : -400.0; }

// ---------------------------------------------------------------------------------------------
// Ratio: the host value r selects a position on the curve family of Appendix A. Displayed ratio per spec 2.
namespace ratio
{
    inline constexpr double neutralPos = 0.218;
    // known (position, ratio) pairs for r <= 0.8 (spec 2 / Appendix A headers) incl. the neutral point
    inline constexpr int numCompPoints = 13;
    inline constexpr double compPos[numCompPoints] = { 0.0, 0.1, 0.15, 0.2, neutralPos, 0.25, 0.3, 0.35, 0.4, 0.5, 0.6, 0.7, 0.8 };
    inline constexpr double compRatio[numCompPoints] = { 0.50, 0.65, 0.77, 0.93, 1.00, 1.15, 1.45, 1.87, 2.46, 4.58, 9.29, 20.62, 50.0 };

    // Displayed ratio for position r (negative values are the cancellation ratios).
    inline double fromPosition (double r) noexcept
    {
        r = clamp01 (r);
        if (r <= 0.8)
        {
            for (int i = 0; i < numCompPoints - 1; ++i)
                if (r <= compPos[i + 1])
                {
                    const double t = (r - compPos[i]) / (compPos[i + 1] - compPos[i]);
                    return std::exp (std::log (compRatio[i]) + t * (std::log (compRatio[i + 1]) - std::log (compRatio[i])));
                }
            return 50.0;
        }
        // cancellation: -50:1 just above 0.8, -15.90:1 at 0.9, -5.00:1 at 1 (log-interpolated magnitude)
        if (r <= 0.9)
        {
            const double t = (r - 0.8) / 0.1;
            return -std::exp (std::log (50.0) + t * (std::log (15.9) - std::log (50.0)));
        }
        const double t = (r - 0.9) / 0.1;
        return -std::exp (std::log (15.9) + t * (std::log (5.0) - std::log (15.9)));
    }

    // Inverse of fromPosition (bisection on the monotonic branches).
    inline double toPosition (double ratioValue) noexcept
    {
        double lo, hi;
        if (ratioValue >= 0.0)
        {
            ratioValue = clampTo (ratioValue, 0.5, 50.0);
            lo = 0.0, hi = 0.8;
            for (int i = 0; i < 60; ++i)
            {
                const double mid = 0.5 * (lo + hi);
                (fromPosition (mid) < ratioValue ? lo : hi) = mid;
            }
        }
        else
        {
            const double m = clampTo (-ratioValue, 5.0, 50.0);
            lo = 0.8 + 1.0e-9, hi = 1.0;
            for (int i = 0; i < 60; ++i)
            {
                const double mid = 0.5 * (lo + hi);
                (-fromPosition (mid) > m ? lo : hi) = mid;
            }
        }
        return 0.5 * (lo + hi);
    }
} // namespace ratio

// ---------------------------------------------------------------------------------------------
// Floor (spec 2 / 6): one physical value v covering three regions
//   v in [-100, 12]    : floor in dB (F = 10^(v/20))
//   v in (-110, -100)  : -inf (F = 0)       - the parameter default (v = -105)
//   v in [-200, -110]  : negative polarity "-y N" with y = v + 210 (v = -200 -> "-10 N", v = -110 -> "-100 N"),
//                        F = -10^(-y/20); expander mode only (acts as -inf in gate mode)
namespace floorvalue
{
    inline constexpr double offValue = -105.0;
    inline bool isOff (double v) noexcept { return v > -110.0 && v < -100.0; }
    inline bool isNegative (double v) noexcept { return v <= -110.0; }
    inline double negativeDb (double v) noexcept { return v + 210.0; } // y of "-y N"
    inline double linear (double v, bool expanderMode) noexcept
    {
        if (isOff (v))
            return 0.0;
        if (isNegative (v))
            return expanderMode ? -std::pow (10.0, -negativeDb (v) / 20.0) : 0.0;
        return std::pow (10.0, v / 20.0);
    }
} // namespace floorvalue

// ---------------------------------------------------------------------------------------------
// Each continuous parameter: range, default, mapping r -> value (quantised to the internal resolution)
// and the inverse value -> r.
namespace param
{
    // Threshold: -100 + 100 r dB, 0.1 dB (Comp Threshold; GateOpen/GateClose where -100 means -inf)
    template <int DefaultDb>
    struct ThresholdT
    {
        static constexpr double minValue = -100.0, maxValue = 0.0, defaultValue = DefaultDb;
        static double fromNorm (double r) noexcept { return quantise (-100.0 + 100.0 * clamp01 (r), 10.0); }
        static double toNorm (double v) noexcept { return clamp01 ((v + 100.0) / 100.0); }
    };
    using CompThreshold = ThresholdT<0>;
    using GateThreshold = ThresholdT<-100>;
    inline bool isMinusInf (double gateThresholdDb) noexcept { return gateThresholdDb <= -100.0 + 1.0e-9; }

    // Ratio position r itself, 0.001 resolution, default 1.00:1
    struct Ratio
    {
        static constexpr double minValue = 0.0, maxValue = 1.0, defaultValue = ratio::neutralPos;
        static double fromNorm (double r) noexcept { return quantise (clamp01 (r), 1000.0); }
        static double toNorm (double v) noexcept { return clamp01 (v); }
    };

    // Attack: 0.01 * 10^(5r) ms (Comp and Gate)
    struct Attack
    {
        static constexpr double minValue = 0.01, maxValue = 1000.0, defaultValue = 2.0;
        static double fromNorm (double r) noexcept { return clampTo (roundSignificant (0.01 * std::pow (10.0, 5.0 * clamp01 (r))), minValue, maxValue); }
        static double toNorm (double v) noexcept { return v <= minValue ? 0.0 : clamp01 (std::log10 (v / 0.01) / 5.0); }
    };

    // Release: 10^(4r) ms
    template <int DefaultMs>
    struct ReleaseT
    {
        static constexpr double minValue = 1.0, maxValue = 10000.0, defaultValue = DefaultMs;
        static double fromNorm (double r) noexcept { return clampTo (roundSignificant (std::pow (10.0, 4.0 * clamp01 (r))), minValue, maxValue); }
        static double toNorm (double v) noexcept { return v <= minValue ? 0.0 : clamp01 (std::log10 (v) / 4.0); }
    };
    using CompRelease = ReleaseT<50>;
    using GateRelease = ReleaseT<30>;

    // PDR: displayed 0,1,2,5,10,22,47,102,218,467,1000 ms at r = 0, 0.1, ..., 1 (linear 0..1 ms, then geometric)
    struct Pdr
    {
        static constexpr double minValue = 0.0, maxValue = 1000.0, defaultValue = 0.0;
        static constexpr double points[11] = { 0.0, 1.0, 2.0, 5.0, 10.0, 22.0, 47.0, 102.0, 218.0, 467.0, 1000.0 };
        static double raw (double r) noexcept
        {
            r = clamp01 (r) * 10.0;
            const int i = std::min (9, int (r));
            const double t = r - i;
            if (i == 0)
                return t * points[1];
            return std::exp (std::log (points[i]) + t * (std::log (points[i + 1]) - std::log (points[i])));
        }
        static double fromNorm (double r) noexcept
        {
            const double v = raw (r);
            return v < 10.0 ? quantise (v, 10.0) : std::round (v);
        }
        static double toNorm (double v) noexcept
        {
            if (v <= 0.0) return 0.0;
            if (v >= 1000.0) return 1.0;
            double lo = 0.0, hi = 1.0;
            for (int k = 0; k < 50; ++k)
            {
                const double mid = 0.5 * (lo + hi);
                (raw (mid) < v ? lo : hi) = mid;
            }
            return 0.5 * (lo + hi);
        }
    };

    // Makeup: -40 + 80 r dB, 0.1 dB
    struct Makeup
    {
        static constexpr double minValue = -40.0, maxValue = 40.0, defaultValue = 0.0;
        static double fromNorm (double r) noexcept { return quantise (-40.0 + 80.0 * clamp01 (r), 10.0); }
        static double toNorm (double v) noexcept { return clamp01 ((v + 40.0) / 80.0); }
    };

    // Hold: 0.01 * 500000^r ms
    struct Hold
    {
        static constexpr double minValue = 0.01, maxValue = 5000.0, defaultValue = 0.01;
        static double fromNorm (double r) noexcept { return clampTo (roundSignificant (0.01 * std::pow (500000.0, clamp01 (r))), minValue, maxValue); }
        static double toNorm (double v) noexcept { return v <= minValue ? 0.0 : clamp01 (std::log (v / 0.01) / std::log (500000.0)); }
    };

    // Floor (see floorvalue): r in [0, 0.15] negative polarity, (0.15, 0.2) -inf, [0.2, 1] -100 ... +12 dB
    struct Floor
    {
        static constexpr double minValue = -200.0, maxValue = 12.0, defaultValue = floorvalue::offValue;
        static double fromNorm (double r) noexcept
        {
            r = clamp01 (r);
            if (r <= 0.15)
                return quantise (-200.0 + 90.0 * r / 0.15, 10.0);
            if (r < 0.2)
                return floorvalue::offValue;
            return quantise (-100.0 + 112.0 * (r - 0.2) / 0.8, 10.0);
        }
        static double toNorm (double v) noexcept
        {
            if (floorvalue::isOff (v)) return 0.175;
            if (v <= -110.0) return clamp01 ((v + 200.0) / 90.0 * 0.15);
            return clamp01 (0.2 + (v + 100.0) / 112.0 * 0.8);
        }
    };

    // Filter Freq: 16 * 1335^r Hz, 1 Hz resolution (0.1 Hz below 100 Hz)
    struct FilterFreq
    {
        static constexpr double minValue = 16.0, maxValue = 21357.0, defaultValue = 7179.0;
        static double fromNorm (double r) noexcept
        {
            const double v = 16.0 * std::pow (1335.0, clamp01 (r));
            return clampTo (v < 100.0 ? quantise (v, 10.0) : std::round (v), minValue, maxValue);
        }
        static double toNorm (double v) noexcept { return v <= minValue ? 0.0 : clamp01 (std::log (v / 16.0) / std::log (1335.0)); }
    };

    // Filter Q: 0.1 + 0.5 r, 0.01
    struct FilterQ
    {
        static constexpr double minValue = 0.1, maxValue = 0.6, defaultValue = 0.6;
        static double fromNorm (double r) noexcept { return quantise (0.1 + 0.5 * clamp01 (r), 100.0); }
        static double toNorm (double v) noexcept { return clamp01 ((v - 0.1) / 0.5); }
    };

    // Input gain L/R: -24 ... 0 dB, 0.1 dB
    struct InputGain
    {
        static constexpr double minValue = -24.0, maxValue = 0.0, defaultValue = 0.0;
        static double fromNorm (double r) noexcept { return quantise (-24.0 + 24.0 * clamp01 (r), 10.0); }
        static double toNorm (double v) noexcept { return clamp01 ((v + 24.0) / 24.0); }
    };

    // Output gain: -40 + 58 r dB, 0.1 dB
    struct OutputGain
    {
        static constexpr double minValue = -40.0, maxValue = 18.0, defaultValue = 0.0;
        static double fromNorm (double r) noexcept { return quantise (-40.0 + 58.0 * clamp01 (r), 10.0); }
        static double toNorm (double v) noexcept { return clamp01 ((v + 40.0) / 58.0); }
    };

    template <typename P>
    double snap (double v) noexcept { return P::fromNorm (P::toNorm (v)); }
} // namespace param

// ---------------------------------------------------------------------------------------------
// Display strings (units included)
inline std::string formatFixed (double v, int decimals)
{
    const double q = std::pow (10.0, decimals);
    v = std::round (v * q + (v >= 0.0 ? 1.0e-7 : -1.0e-7)) / q;
    if (v == 0.0)
        v = 0.0; // no "-0.0"
    char buf[48];
    std::snprintf (buf, sizeof (buf), "%.*f", decimals, v);
    return buf;
}

enum class Quantity
{
    Decibel,       // "-20.0 dB"
    GateThreshold, // "-inf" or "-40.0 dB"
    Ratio,         // "4.58:1" / "-15.90:1" (value is the position r)
    Time,          // "2.00 ms", "50.0 ms", "1000 ms", "1.20 s"
    Pdr,           // "Off", "22 ms"
    Floor,         // "-inf", "-27.4 dB", "-30 N"
    Frequency,     // "7179 Hz", "16.0 Hz", "21.4 kHz"
    Q              // "0.60"
};

inline std::string formatTimeMs (double ms)
{
    if (ms < 10.0) return formatFixed (ms, 2) + " ms";
    if (ms < 100.0) return formatFixed (ms, 1) + " ms";
    if (ms < 1000.0) return formatFixed (ms, 0) + " ms";
    return formatFixed (ms / 1000.0, ms < 10000.0 ? 2 : 1) + " s";
}

inline std::string formatDisplay (Quantity q, double v)
{
    switch (q)
    {
        case Quantity::Decibel: return formatFixed (v, 1) + " dB";
        case Quantity::GateThreshold: return param::isMinusInf (v) ? std::string ("-inf") : formatFixed (v, 1) + " dB";
        case Quantity::Ratio: return formatFixed (ratio::fromPosition (v), 2) + ":1";
        case Quantity::Time: return formatTimeMs (v);
        case Quantity::Pdr: return v <= 0.0 ? std::string ("Off") : (v < 10.0 ? formatFixed (v, 1) : formatFixed (v, 0)) + " ms";
        case Quantity::Floor:
            if (floorvalue::isOff (v)) return "-inf";
            if (floorvalue::isNegative (v)) return "-" + formatFixed (floorvalue::negativeDb (v), 0) + " N";
            return formatFixed (v, 1) + " dB";
        case Quantity::Frequency:
            if (v < 100.0) return formatFixed (v, 1) + " Hz";
            if (v < 10000.0) return formatFixed (v, 0) + " Hz";
            return formatFixed (v / 1000.0, 2) + " kHz";
        case Quantity::Q: return formatFixed (v, 2);
    }
    return formatFixed (v, 2);
}

// Tolerant number parser: units/spaces optional, "k" multiplies by 1000, "s" (seconds) by 1000 for times.
inline double parseNumber (const std::string& text, bool* sawInf = nullptr, bool* sawN = nullptr, bool* sawSeconds = nullptr)
{
    std::string num, lower;
    bool kilo = false;
    for (char ch : text)
        lower += char ((ch >= 'A' && ch <= 'Z') ? ch - 'A' + 'a' : ch);
    if (sawInf != nullptr) *sawInf = lower.find ("inf") != std::string::npos;
    if (sawN != nullptr) *sawN = lower.find ('n') != std::string::npos && lower.find ("inf") == std::string::npos;
    if (sawSeconds != nullptr)
    {
        const auto pos = lower.find_last_not_of (' ');
        *sawSeconds = pos != std::string::npos && lower[pos] == 's' && lower.find ("ms") == std::string::npos;
    }
    for (char ch : lower)
    {
        if ((ch >= '0' && ch <= '9') || ch == '.' || ch == '-' || ch == '+')
        {
            if (! kilo) num += ch;
        }
        else if (ch == ':')
            break; // "4.58:1"
        else if (ch == 'k' && ! num.empty())
            kilo = true;
    }
    if (num.empty())
        return 0.0;
    const double v = std::strtod (num.c_str(), nullptr);
    return kilo ? v * 1000.0 : v;
}

// Typed text -> physical value for a quantity (before clamping/snapping by the caller).
inline double parseDisplay (Quantity q, const std::string& text)
{
    bool inf = false, n = false, sec = false;
    const double v = parseNumber (text, &inf, &n, &sec);
    switch (q)
    {
        case Quantity::GateThreshold: return inf ? -100.0 : v;
        case Quantity::Ratio: return ratio::toPosition (v == 0.0 ? 1.0 : v);
        case Quantity::Time: return sec ? v * 1000.0 : v;
        case Quantity::Pdr: return v;
        case Quantity::Floor:
            if (inf) return floorvalue::offValue;
            if (n) return clampTo (std::abs (v), 10.0, 100.0) - 210.0;
            return v < -100.0 ? floorvalue::offValue : v;
        case Quantity::Decibel:
        case Quantity::Frequency:
        case Quantity::Q: return v;
    }
    return v;
}

} // namespace craft::dynamics::dsp
