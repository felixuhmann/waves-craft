// Craft Level Rider - parameter mappings and settings (spec r1 section 2).
// Host-independent: no JUCE dependency.
#pragma once

#include <cmath>

namespace craft::levelrider::dsp
{

inline double clamp01 (double r) noexcept { return r < 0.0 ? 0.0 : (r > 1.0 ? 1.0 : r); }
inline double clampTo (double x, double lo, double hi) noexcept { return x < lo ? lo : (x > hi ? hi : x); }

// Round to 0.1 dB (the display resolution of every parameter in section 2).
inline double quantise01 (double x) noexcept { return std::round (x * 10.0) / 10.0; }

// Linear dB parameter v = offset + span * r, 0.1 dB steps.
template <int OffsetTenths, int SpanTenths, int DefaultTenths>
struct LinearDb
{
    static constexpr double minValue = OffsetTenths / 10.0;
    static constexpr double maxValue = (OffsetTenths + SpanTenths) / 10.0;
    static constexpr double defaultValue = DefaultTenths / 10.0;
    static double fromNorm (double r) noexcept { return quantise01 (minValue + (maxValue - minValue) * clamp01 (r)); }
    static double toNorm (double v) noexcept { return clamp01 ((v - minValue) / (maxValue - minValue)); }
};

namespace param
{
    using Target     = LinearDb<-420, 420, -210>; // -42 + 42 r dBFS, default -21
    using VocalSens  = LinearDb<-120, 240, 0>;    // -12 + 24 r dB, default 0
    using RangeMax   = LinearDb<-110, 230, 60>;   // -11 + 23 r dB, default +6 (r = 0.739)
    using RangeMin   = LinearDb<-120, 230, -60>;  // -12 + 23 r dB, default -6 (r = 0.261)
    using Idle       = LinearDb<-115, 230, 0>;    // -11.5 + 23 r dB, default 0
    using Rider      = LinearDb<-120, 240, 0>;    // -12 + 24 r dB (reflects the ride gain g)
    using Output     = LinearDb<-120, 240, 0>;    // -12 + 24 r dB, default 0
    using Spill      = LinearDb<-480, 360, -480>; // -48 + 36 r dBFS, default -48 (Live products only)

    // Music Sensitivity: r = 0 is "Off", otherwise -12 + 24 r dB; default 0 dB (r = 0.5).
    struct MusicSens
    {
        static constexpr double minValue = -12.0, maxValue = 12.0, defaultValue = 0.0;
        static bool isOff (double r) noexcept { return r <= 0.0; }
        static double fromNorm (double r) noexcept { return quantise01 (-12.0 + 24.0 * clamp01 (r)); }
        static double toNorm (double v) noexcept { return clamp01 ((v + 12.0) / 24.0); }
    };

    // Attack: r < 0.5 Slow, else Fast.
    inline bool attackIsFast (double r) noexcept { return r >= 0.5; }

    template <typename P>
    double snap (double v) noexcept { return P::fromNorm (P::toNorm (v)); }
} // namespace param

// Section 2: Range Min stays at least 1 dB below Range Max; the most recently changed control wins and the
// other one is pushed away by 1 dB. Returns the resolved pair {min, max}.
struct RangePair
{
    double minDb, maxDb;
};

inline RangePair resolveRange (double minDb, double maxDb, bool maxChangedLast) noexcept
{
    if (maxDb - minDb >= 1.0 - 1e-9)
        return { minDb, maxDb };
    if (maxChangedLast)
        return { maxDb - 1.0, maxDb }; // Max in [-11, 12] -> Min in [-12, 11]: always reachable
    return { minDb, minDb + 1.0 };     // Min in [-12, 11] -> Max in [-11, 12]
}

enum class AutomationMode { Off = 0, Write = 1, Read = 2 };

// Complete, already mapped settings of the processor.
struct Settings
{
    double targetDb = param::Target::defaultValue;
    double vocalSensDb = param::VocalSens::defaultValue;
    bool musicOff = false;
    double musicSensDb = param::MusicSens::defaultValue;
    double rangeMaxDb = param::RangeMax::defaultValue;
    double rangeMinDb = param::RangeMin::defaultValue;
    double idleDb = param::Idle::defaultValue;
    double outputDb = param::Output::defaultValue;
    bool fast = false;
    bool spillEnabled = false; // Live products
    double spillDb = param::Spill::defaultValue;
};

} // namespace craft::levelrider::dsp
