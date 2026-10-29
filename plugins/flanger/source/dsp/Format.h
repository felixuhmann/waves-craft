// Craft Flanger - value display formats (spec r1 section 2). Host-independent.
#pragma once

#include <cmath>
#include <cstdio>
#include <string>

namespace craft::flanger::dsp
{

enum class Quantity
{
    Delay,     // "2.2"  (ms)
    Percent,   // "50"   (%)
    Rate,      // "0.20" below 10, "12.5" from 10  (Hz, or x/Beat in Tempo Sync)
    Gain,      // "0.0"  (dB)
    Stereo,    // "90"
    FilterFreq // "1.0k" below 10 kHz, "12k" from 10 kHz
};

inline std::string formatFixed (double v, int decimals)
{
    const double q = std::pow (10.0, decimals);
    v = std::round (v * q + (v >= 0.0 ? 1.0e-7 : -1.0e-7)) / q; // values sit on a decimal grid: round ties up
    if (v == 0.0)
        v = 0.0; // no "-0.0"
    char buf[32];
    std::snprintf (buf, sizeof (buf), "%.*f", decimals, v);
    return buf;
}

// Numeric text without the unit.
inline std::string formatNumber (Quantity q, double v)
{
    switch (q)
    {
        case Quantity::Delay:   return formatFixed (v, 1);
        case Quantity::Percent: return formatFixed (v, 0);
        case Quantity::Rate:    return formatFixed (v, std::round (v * 100.0 + 1.0e-7) < 1000.0 ? 2 : 1);
        case Quantity::Gain:    return formatFixed (v, 1);
        case Quantity::Stereo:  return formatFixed (v, 0);
        case Quantity::FilterFreq:
            return v < 9999.5 ? formatFixed (v / 1000.0, 1) + "k" : formatFixed (v / 1000.0, 0) + "k";
    }
    return formatFixed (v, 2);
}

} // namespace craft::flanger::dsp
