// Craft Colour Delay - parameter mappings and fixed laws (spec r1 sections 2, 3 and 6). No JUCE dependency.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>

namespace craft::cdl::param
{

inline double roundTo (double v, double step) noexcept { return std::round (v / step) * step; }

// Each mapping: r in [0,1] -> value (already quantised to the spec resolution), inverse, snap.
struct GainDb // Input, Output: -18 + 36 r dB, 0.1 dB
{
    static constexpr double minValue = -18.0, maxValue = 18.0, defaultValue = 0.0, step = 0.1;
    static double fromNorm (double r) noexcept { return snap (-18.0 + 36.0 * r); }
    static double toNorm (double v) noexcept { return std::clamp ((v + 18.0) / 36.0, 0.0, 1.0); }
    static double snap (double v) noexcept { return std::clamp (roundTo (v, step), minValue, maxValue); }
};

struct DelayMs // 2000 r ms, 4 ms steps
{
    static constexpr double minValue = 0.0, maxValue = 2000.0, defaultValue = 500.0, step = 4.0;
    static double fromNorm (double r) noexcept { return snap (2000.0 * r); }
    static double toNorm (double v) noexcept { return std::clamp (v / 2000.0, 0.0, 1.0); }
    static double snap (double v) noexcept { return std::clamp (roundTo (v, step), minValue, maxValue); }
};

struct Percent // 100 r %, 0.1 %
{
    static constexpr double minValue = 0.0, maxValue = 100.0, defaultValue = 50.0, step = 0.1;
    static double fromNorm (double r) noexcept { return snap (100.0 * r); }
    static double toNorm (double v) noexcept { return std::clamp (v / 100.0, 0.0, 1.0); }
    static double snap (double v) noexcept { return std::clamp (roundTo (v, step), minValue, maxValue); }
};

struct FilterHz // 16 (21357/16)^r Hz, 1 Hz; HP off at r = 0, LP off at r = 1
{
    static constexpr double minValue = 16.0, maxValue = 21357.0, step = 1.0;
    static double fromNorm (double r) noexcept { return snap (16.0 * std::pow (21357.0 / 16.0, r)); }
    static double toNorm (double v) noexcept
    {
        return std::clamp (std::log (std::max (v, minValue) / 16.0) / std::log (21357.0 / 16.0), 0.0, 1.0);
    }
    static double snap (double v) noexcept { return std::clamp (roundTo (v, step), minValue, maxValue); }
};

struct VerbSize // 1 + 99 r %, 0.1 %
{
    static constexpr double minValue = 1.0, maxValue = 100.0, defaultValue = 54.9, step = 0.1;
    static double fromNorm (double r) noexcept { return snap (1.0 + 99.0 * r); }
    static double toNorm (double v) noexcept { return std::clamp ((v - 1.0) / 99.0, 0.0, 1.0); }
    static double snap (double v) noexcept { return std::clamp (roundTo (v, step), minValue, maxValue); }
};

struct Detune // -100 + 200 r cents, 1 cent
{
    static constexpr double minValue = -100.0, maxValue = 100.0, defaultValue = 8.0, step = 1.0;
    static double fromNorm (double r) noexcept { return snap (-100.0 + 200.0 * r); }
    static double toNorm (double v) noexcept { return std::clamp ((v + 100.0) / 200.0, 0.0, 1.0); }
    static double snap (double v) noexcept { return std::clamp (roundTo (v, step), minValue, maxValue); }
};

struct PhaserRate // 20 exp(8.28 (r - 1)) Hz, 0.01 Hz (values below 0.01 Hz are kept unrounded)
{
    static constexpr double minValue = 0.00507, maxValue = 20.0, defaultValue = 0.34, step = 0.01;
    static double rawMin() noexcept { return 20.0 * std::exp (-8.28); }
    static double fromNorm (double r) noexcept { return snap (20.0 * std::exp (8.28 * (r - 1.0))); }
    static double toNorm (double v) noexcept
    {
        return std::clamp (1.0 + std::log (std::max (v, rawMin()) / 20.0) / 8.28, 0.0, 1.0);
    }
    static double snap (double v) noexcept
    {
        v = std::clamp (v, rawMin(), maxValue);
        return v < step ? v : roundTo (v, step);
    }
};

// Feedback, Dry/Wet, Level, Send, Width defaults (all Percent mappings)
inline constexpr double feedbackDefault = 29.9;
inline constexpr double dryWetDefault = 100.0;
inline constexpr double distSendDefault = 82.2;
inline constexpr double distWidthDefault = 0.0;
inline constexpr double levelDefault = 50.0;

// ---------------------------------------------------------------------------------------------
// Note values (spec section 2): 19 values evenly spaced over r, ascending. Length in quarter notes.
inline constexpr int numNotes = 19;
inline constexpr int noteDefaultIndex = 13; // 1/4
inline constexpr std::array<const char*, numNotes> noteNames {
    "1/64T", "1/64", "1/32T", "1/64D", "1/32", "1/16T", "1/32D", "1/16", "1/8T", "1/16D",
    "1/8",   "1/4T", "1/8D",  "1/4",   "1/2T", "1/4D",  "1/2",   "1/2D", "Whole"
};

// Exact note length as a rational number of quarter notes: num / den.
struct NoteRatio { long long num, den; };

inline NoteRatio noteRatio (int index) noexcept
{
    // straight base b = 1/2^k quarters; triplet = b * 2/3; dotted = b * 3/2
    static constexpr NoteRatio table[numNotes] = {
        { 1, 24 }, { 1, 16 }, { 1, 12 }, { 3, 32 }, { 1, 8 }, { 1, 6 }, { 3, 16 }, { 1, 4 }, { 1, 3 }, { 3, 8 },
        { 1, 2 },  { 2, 3 },  { 3, 4 },  { 1, 1 },  { 4, 3 }, { 3, 2 }, { 2, 1 },  { 3, 1 }, { 4, 1 }
    };
    return table[std::clamp (index, 0, numNotes - 1)];
}

inline double noteQuarters (int index) noexcept
{
    const auto r = noteRatio (index);
    return (double) r.num / (double) r.den;
}

inline int noteIndexFromNorm (double r) noexcept { return std::clamp ((int) std::lround (r * (numNotes - 1)), 0, numNotes - 1); }

// Delay-line length in samples (spec section 3a); limited to 2.0 s.
inline int delaySamplesMs (double ms, double fs) noexcept
{
    const double d = std::round (ms * fs / 1000.0);
    return (int) std::clamp (d, 1.0, std::floor (2.0 * fs));
}

// Host mode (spec r2 section 3a): D = max(1, floor(N fs)), N = beats * 60 / BPM the exact note duration, limited
// to 2.0 s. Evaluated as num * 60 * fs / (den * BPM) in long double with a 1e-9 relative guard before flooring, so
// lengths that are mathematically integers are never lost to rounding.
inline int delaySamplesNote (int noteIndex, double bpm, double fs) noexcept
{
    bpm = bpm > 0.0 ? bpm : 120.0;
    const auto r = noteRatio (noteIndex);
    long double n = (long double) r.num * 60.0L * (long double) fs / ((long double) r.den * (long double) bpm);
    n = std::min (n, 2.0L * (long double) fs);
    return (int) std::max (1.0L, std::floor (n * (1.0L + 1.0e-9L)));
}

// ---------------------------------------------------------------------------------------------
// Dry/Wet law (spec section 6): wet gain in dB at 0, 5, ..., 100 %.
inline constexpr std::array<double, 21> wetLawDb { -1000.0, -26.2, -20.0, -17.0, -14.0, -12.2, -10.5, -9.2, -8.0, -7.0, -6.0,
                                                   -5.2,    -4.4,  -3.8,  -3.1,  -2.5,  -1.9,  -1.4,  -0.9, -0.4, 0.0 };

inline double wetGain (double mixPercent) noexcept
{
    const double m = std::clamp (mixPercent, 0.0, 100.0);
    if (m < 5.0) // linear in amplitude between 0 and 5 %
        return std::pow (10.0, wetLawDb[1] / 20.0) * (m / 5.0);
    const double pos = m / 5.0;
    const int i = std::min (19, (int) std::floor (pos));
    const double frac = pos - i;
    const double db = frac == 0.0 ? wetLawDb[(size_t) i] : wetLawDb[(size_t) i] + frac * (wetLawDb[(size_t) i + 1] - wetLawDb[(size_t) i]);
    return std::pow (10.0, db / 20.0);
}

inline double dryGain (double mixPercent) noexcept { return wetGain (100.0 - std::clamp (mixPercent, 0.0, 100.0)); }

// ---------------------------------------------------------------------------------------------
// Effect level laws (spec section 7): (Level/100)^3 for the reverb, ^3.5 for the other three.
inline double reverbLevelGain (double levelPercent) noexcept { return std::pow (std::clamp (levelPercent, 0.0, 100.0) / 100.0, 3.0); }
inline double effectLevelGain (double levelPercent) noexcept { return std::pow (std::clamp (levelPercent, 0.0, 100.0) / 100.0, 3.5); }

} // namespace craft::cdl::param
