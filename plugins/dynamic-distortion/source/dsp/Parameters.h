// Craft Dynamic Distortion - parameter mappings and level laws (spec r1 sections 3, 4.1, 4.2).
// Host-independent, no JUCE. Every host parameter's normalised value is the spec's r; fromNorm() is the
// spec mapping of r to the physical value.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>

namespace craft::dd
{

inline double clamp01 (double r) noexcept { return r < 0.0 ? 0.0 : (r > 1.0 ? 1.0 : r); }
inline double clampd (double x, double lo, double hi) noexcept { return x < lo ? lo : (x > hi ? hi : x); }
inline double dbToGain (double db) noexcept { return std::pow (10.0, db / 20.0); }
inline double gainToDb (double g) noexcept { return 20.0 * std::log10 (std::max (g, 1.0e-30)); }

namespace param
{
    // Linear mapping lo + (hi - lo) r.
    template <int Lo, int Hi>
    struct Linear
    {
        static constexpr double minValue = Lo, maxValue = Hi;
        static double fromNorm (double r) noexcept { return Lo + double (Hi - Lo) * clamp01 (r); }
        static double toNorm (double v) noexcept { return clamp01 ((v - Lo) / double (Hi - Lo)); }
    };

    // Exponential mapping base * ratio^r.
    template <typename Self>
    struct Exp
    {
        static double fromNorm (double r) noexcept { return Self::base * std::pow (Self::ratio, clamp01 (r)); }
        static double toNorm (double v) noexcept
        {
            return v <= Self::base ? 0.0 : clamp01 (std::log (v / Self::base) / std::log (Self::ratio));
        }
    };

    // Section 3 table.
    struct Input : Linear<-18, 18> { static constexpr double defaultValue = 0.0; };          // dB [0.1]
    struct Output : Linear<-18, 18> { static constexpr double defaultValue = 0.0; };         // dB [0.1]
    struct DirectLevel : Linear<0, 100> { static constexpr double defaultValue = 100.0; };   // % [0.1]
    struct Drive : Linear<0, 100> { static constexpr double defaultValue = 50.0; };          // % [0.1]
    struct DistLevel : Linear<0, 100> { static constexpr double defaultValue = 62.0; };      // % [0.1]
    struct Bass : Linear<-50, 50> { static constexpr double defaultValue = -50.0; };         // [0.5]
    struct Mid : Linear<-50, 50> { static constexpr double defaultValue = 0.0; };            // [0.5]
    struct Treble : Linear<-50, 50> { static constexpr double defaultValue = -15.0; };       // [0.5]

    struct Attack : Exp<Attack> // 0.1 * 5000^r ms [0.1 ms]
    {
        static constexpr double base = 0.1, ratio = 5000.0, minValue = 0.1, maxValue = 500.0, defaultValue = 1.0;
    };
    struct Release : Exp<Release> // 5 * 1000^r ms [0.01 ms]
    {
        static constexpr double base = 5.0, ratio = 1000.0, minValue = 5.0, maxValue = 5000.0, defaultValue = 31.8;
    };
    struct MidFreq : Exp<MidFreq> // 150 * (100/3)^r Hz [1 Hz]
    {
        static constexpr double base = 150.0, ratio = 100.0 / 3.0, minValue = 150.0, maxValue = 5000.0,
                                defaultValue = 1252.0;
    };
    struct LowPass : Exp<LowPass> // 500 * 42^r Hz [1 Hz]
    {
        static constexpr double base = 500.0, ratio = 42.0, minValue = 500.0, maxValue = 21000.0, defaultValue = 13015.0;
    };
} // namespace param

// ---------------------------------------------------------------------------------------------
// Section 4.1 / 4.2: piecewise-linear tables, linear interpolation in dB between rows.
namespace laws
{
    struct Row
    {
        double x, db;
    };

    template <size_t N>
    double interpolate (const std::array<Row, N>& t, double x) noexcept
    {
        if (x <= t[0].x)
            return t[0].db;
        for (size_t i = 1; i < N; ++i)
            if (x <= t[i].x)
            {
                const double f = (x - t[i - 1].x) / (t[i].x - t[i - 1].x);
                return t[i - 1].db + f * (t[i].db - t[i - 1].db);
            }
        return t[N - 1].db;
    }

    inline constexpr std::array<Row, 13> directTable { { { 5, -60.0 }, { 10, -50.0 }, { 20, -24.0 }, { 25, -22.0 },
                                                         { 30, -20.0 }, { 40, -16.0 }, { 50, -12.0 }, { 60, -9.6 },
                                                         { 70, -7.2 }, { 75, -6.0 }, { 80, -4.8 }, { 90, -2.4 },
                                                         { 100, 0.0 } } };
    inline constexpr std::array<Row, 13> distortionTable { { { 5, -55.0 }, { 10, -40.0 }, { 20, -30.0 }, { 25, -28.0 },
                                                             { 30, -26.0 }, { 40, -22.0 }, { 50, -18.0 }, { 60, -14.4 },
                                                             { 70, -10.8 }, { 75, -9.0 }, { 80, -7.2 }, { 90, -3.6 },
                                                             { 100, 0.0 } } };
    inline constexpr std::array<Row, 21> driveTable { { { 0, -4.70 }, { 5, 1.30 }, { 10, 7.30 }, { 15, 10.30 },
                                                        { 20, 13.30 }, { 25, 16.80 }, { 30, 20.30 }, { 35, 22.80 },
                                                        { 40, 25.30 }, { 45, 26.60 }, { 50, 27.80 }, { 55, 29.10 },
                                                        { 60, 30.30 }, { 65, 31.80 }, { 70, 33.30 }, { 75, 33.61 },
                                                        { 80, 33.80 }, { 85, 34.10 }, { 90, 34.30 }, { 95, 34.80 },
                                                        { 100, 35.30 } } };

    // Level law in dB for 5..100 %; 0 % is silence. Between 0 and 5 % (not tabulated) the gain fades linearly
    // in amplitude from 0 to the 5 % value (implementation choice, see IMPLEMENTATION_NOTES.md).
    template <size_t N>
    double levelGain (const std::array<Row, N>& t, double percent) noexcept
    {
        percent = clampd (percent, 0.0, 100.0);
        if (percent <= 0.0)
            return 0.0;
        if (percent < t[0].x)
            return dbToGain (t[0].db) * percent / t[0].x;
        return dbToGain (interpolate (t, percent));
    }

    inline double directGain (double percent) noexcept { return levelGain (directTable, percent); }
    inline double distortionGain (double percent) noexcept { return levelGain (distortionTable, percent); }
    // Small-signal gain of the distortion path at 1 kHz (section 4.2), dB.
    inline double driveDb (double percent) noexcept { return interpolate (driveTable, clampd (percent, 0.0, 100.0)); }
} // namespace laws

} // namespace craft::dd
