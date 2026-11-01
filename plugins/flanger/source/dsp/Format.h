// Craft Flanger - value display formats (spec r1 section 2). Host-independent.
#pragma once

#include <cmath>
#include <cstdio>
#include <cstdlib>
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

// Full display string of spec 2, including the unit: "2.2 ms", "50 %", "0.20 Hz" (or "1.00 x/Beat" in Tempo
// Sync), "0.0 dB", "90", "1.0k" / "12k".
inline std::string formatDisplay (Quantity q, double v, bool tempoSync = false)
{
    const std::string n = formatNumber (q, v);
    switch (q)
    {
        case Quantity::Delay:   return n + " ms";
        case Quantity::Percent: return n + " %";
        case Quantity::Rate:    return n + (tempoSync ? " x/Beat" : " Hz");
        case Quantity::Gain:    return n + " dB";
        case Quantity::Stereo:
        case Quantity::FilterFreq: return n;
    }
    return n;
}

// Tolerant parser for typed values: units and spaces are optional ("2.2", "2.2 ms", "-3dB", "50 %"), and a
// trailing "k" multiplies by 1000 ("1.5k", "12 kHz"). Returns 0 for text without a number.
inline double parseNumber (const std::string& text)
{
    std::string num;
    bool kilo = false;
    for (char ch : text)
    {
        if ((ch >= '0' && ch <= '9') || ch == '.' || ch == '-' || ch == '+')
        {
            if (! kilo)
                num += ch;
        }
        else if ((ch == 'k' || ch == 'K') && ! num.empty())
            kilo = true;
    }
    if (num.empty())
        return 0.0;
    char* end = nullptr;
    const double v = std::strtod (num.c_str(), &end);
    return kilo ? v * 1000.0 : v;
}

} // namespace craft::flanger::dsp
