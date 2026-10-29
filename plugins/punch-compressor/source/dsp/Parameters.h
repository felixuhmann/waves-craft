// Craft Punch Compressor - parameter maps, display formats and tempo-sync note values (spec r1 section 2).
// Host-independent: no JUCE dependency.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>

namespace craft::punch
{

inline double clamp01 (double r) noexcept { return r < 0.0 ? 0.0 : (r > 1.0 ? 1.0 : r); }
inline double quantise (double x, double stepsPerUnit) noexcept { return std::round (x * stepsPerUnit) / stepsPerUnit; }

// Round to `digits` significant digits (display resolution of Attack / Release).
inline double roundSignificant (double v, int digits) noexcept
{
    if (v <= 0.0)
        return v;
    const double e = std::floor (std::log10 (v)) - (digits - 1);
    const double scale = std::pow (10.0, e);
    return std::round (v / scale) * scale;
}

namespace param
{
    // Each parameter: range, default, mapping r -> value (quantised to the displayed resolution) and the
    // inverse value -> r of the continuous mapping.

    // Threshold T = -48 r dB, 0.1 dB, default -18 dB
    struct Threshold
    {
        static constexpr double minValue = -48.0, maxValue = 0.0, defaultValue = -18.0, stepsPerUnit = 10.0;
        static double fromNorm (double r) noexcept { return quantise (-48.0 * clamp01 (r), stepsPerUnit) + 0.0; }
        static double toNorm (double v) noexcept { return clamp01 (-v / 48.0); }
    };

    // Ratio: 41-point table, log R interpolated linearly in r; 2 decimals, default 3.00
    struct Ratio
    {
        static constexpr std::array<double, 41> table { 1.00, 1.12, 1.24, 1.39, 1.55, 1.72, 1.92, 2.13, 2.37,
                                                        2.63, 2.92, 3.24, 3.59, 3.97, 4.40, 4.87, 5.38, 5.95,
                                                        6.57, 7.25, 8.00, 8.82, 9.72, 10.70, 11.77, 12.95, 14.23,
                                                        15.63, 17.15, 18.81, 20.63, 22.60, 24.74, 27.08, 29.61,
                                                        32.36, 35.35, 38.59, 42.09, 45.89, 50.00 };
        static constexpr double minValue = 1.0, maxValue = 50.0, defaultValue = 3.0, stepsPerUnit = 100.0;
        static double continuous (double r) noexcept
        {
            const double x = clamp01 (r) * 40.0;
            const int k = std::min (39, int (std::floor (x)));
            const double f = x - k;
            return std::exp ((1.0 - f) * std::log (table[size_t (k)]) + f * std::log (table[size_t (k + 1)]));
        }
        static double fromNorm (double r) noexcept { return std::clamp (quantise (continuous (r), stepsPerUnit), minValue, maxValue); }
        static double toNorm (double v) noexcept
        {
            if (v <= table.front()) return 0.0;
            if (v >= table.back()) return 1.0;
            int k = 0;
            while (k < 39 && table[size_t (k + 1)] < v)
                ++k;
            const double l0 = std::log (table[size_t (k)]), l1 = std::log (table[size_t (k + 1)]);
            return clamp01 ((k + (std::log (v) - l0) / (l1 - l0)) / 40.0);
        }
    };

    // Mix m = 100 r %, 0.1, default 100
    struct Mix
    {
        static constexpr double minValue = 0.0, maxValue = 100.0, defaultValue = 100.0, stepsPerUnit = 10.0;
        static double fromNorm (double r) noexcept { return quantise (100.0 * clamp01 (r), stepsPerUnit); }
        static double toNorm (double v) noexcept { return clamp01 (v / 100.0); }
    };

    // Attack A = 0.5 * 200^r ms, 3 significant digits, default 7.00 ms
    struct Attack
    {
        static constexpr double minValue = 0.5, maxValue = 100.0, defaultValue = 7.0;
        static double fromNorm (double r) noexcept
        {
            return std::clamp (roundSignificant (0.5 * std::pow (200.0, clamp01 (r)), 3), minValue, maxValue);
        }
        static double toNorm (double v) noexcept { return v <= minValue ? 0.0 : clamp01 (std::log (v / 0.5) / std::log (200.0)); }
    };

    // Punch P = 30 r (ms of side-chain delay), 0.1, default 1.0
    struct Punch
    {
        static constexpr double minValue = 0.0, maxValue = 30.0, defaultValue = 1.0, stepsPerUnit = 10.0;
        static double fromNorm (double r) noexcept { return quantise (30.0 * clamp01 (r), stepsPerUnit); }
        static double toNorm (double v) noexcept { return clamp01 (v / 30.0); }
    };

    // Release Rel = 3 * 1000^r ms, 3 significant digits, default 100 ms
    struct Release
    {
        static constexpr double minValue = 3.0, maxValue = 3000.0, defaultValue = 100.0;
        static double fromNorm (double r) noexcept
        {
            return std::clamp (roundSignificant (3.0 * std::pow (1000.0, clamp01 (r)), 3), minValue, maxValue);
        }
        static double toNorm (double v) noexcept { return v <= minValue ? 0.0 : clamp01 (std::log (v / 3.0) / std::log (1000.0)); }
    };

    // Output O = -18 + 36 r dB, 0.1, default 0
    struct Output
    {
        static constexpr double minValue = -18.0, maxValue = 18.0, defaultValue = 0.0, stepsPerUnit = 10.0;
        static double fromNorm (double r) noexcept { return quantise (-18.0 + 36.0 * clamp01 (r), stepsPerUnit) + 0.0; }
        static double toNorm (double v) noexcept { return clamp01 ((v + 18.0) / 36.0); }
    };

    // Manual tempo for the BPM sync mode: 30 ... 300 BPM or Off. Implementation choice (spec gives no map):
    // linear 29 ... 300 with 0.1 BPM steps, every value below 30 means "Off" (host tempo).
    struct Tempo
    {
        static constexpr double minValue = 29.0, maxValue = 300.0, defaultValue = 29.0, offBelow = 30.0, stepsPerUnit = 10.0;
        static double fromNorm (double r) noexcept { return quantise (29.0 + 271.0 * clamp01 (r), stepsPerUnit); }
        static double toNorm (double v) noexcept { return clamp01 ((v - 29.0) / 271.0); }
        static bool isOff (double v) noexcept { return v < offBelow; }
    };

    // Release note index k = floor(40 r) (spec: the value for r in [k/40, (k+1)/40)), default 1/32D (k = 10)
    struct Note
    {
        static constexpr double minValue = 0.0, maxValue = 40.0, defaultValue = 10.0;
        static double fromNorm (double r) noexcept { return std::min (40.0, std::floor (clamp01 (r) * 40.0 + 1.0e-9)); }
        static double toNorm (double v) noexcept { return clamp01 (std::round (v) / 40.0); }
    };

    template <typename P>
    double snap (double v) noexcept { return P::fromNorm (P::toNorm (v)); }

    // Choice parameters (host value r -> index by rounding, as the spec's thresholds give)
    enum class Sync { Bpm = 0, Host = 1, Ms = 2 };
    inline Sync syncFromNorm (double r) noexcept { return r < 0.25 ? Sync::Bpm : (r < 0.75 ? Sync::Host : Sync::Ms); }
    inline int analogFromNorm (double r) noexcept { return r < 0.125 ? 0 : r < 0.375 ? 1 : r < 0.625 ? 2 : r < 0.875 ? 3 : 4; }
    enum class Meter { In = 0, Gr = 1, Out = 2 };

    // --- release note table (section 2) --------------------------------------------------------------
    struct NoteValue
    {
        const char* name;
        double beats; // duration in quarter-note beats
    };

    inline constexpr double whole = 4.0;
    // note durations: 1/n of a bar of 4 beats; T = x 2/3, D = x 3/2
    inline const std::array<NoteValue, 41>& noteTable()
    {
        static const std::array<NoteValue, 41> t = [] {
            auto n = [] (double div, char kind) {
                const double b = whole / div;
                return kind == 'T' ? b * 2.0 / 3.0 : (kind == 'D' ? b * 1.5 : b);
            };
            return std::array<NoteValue, 41> { {
                { "1/64T", n (64, 'T') }, { "1/64T", n (64, 'T') }, { "1/64", n (64, ' ') }, { "1/64", n (64, ' ') },
                { "1/64D", n (64, 'D') }, { "1/64D", n (64, 'D') }, { "1/32T", n (32, 'T') }, { "1/32T", n (32, 'T') },
                { "1/32", n (32, ' ') },  { "1/32", n (32, ' ') },  { "1/32D", n (32, 'D') }, { "1/32D", n (32, 'D') },
                { "1/16T", n (16, 'T') }, { "1/16T", n (16, 'T') }, { "1/16", n (16, ' ') },  { "1/16", n (16, ' ') },
                { "1/16D", n (16, 'D') }, { "1/16D", n (16, 'D') }, { "1/8T", n (8, 'T') },   { "1/8T", n (8, 'T') },
                { "1/8", n (8, ' ') },    { "1/8", n (8, ' ') },    { "1/8", n (8, ' ') },    { "1/8D", n (8, 'D') },
                { "1/8D", n (8, 'D') },   { "1/4T", n (4, 'T') },   { "1/4T", n (4, 'T') },   { "1/4", n (4, ' ') },
                { "1/4", n (4, ' ') },    { "1/4D", n (4, 'D') },   { "1/4D", n (4, 'D') },   { "1/2T", n (2, 'T') },
                { "1/2T", n (2, 'T') },   { "1/2", n (2, ' ') },    { "1/2", n (2, ' ') },    { "1/2D", n (2, 'D') },
                { "1/2D", n (2, 'D') },   { "1B", 4.0 },            { "1B", 4.0 },            { "2B", 8.0 },
                { "2B", 8.0 } } };
        }();
        return t;
    }

    inline const NoteValue& note (int k) noexcept { return noteTable()[size_t (std::clamp (k, 0, 40))]; }

    // Note duration in ms at a tempo (section 2: 1/8 at 120 BPM = 250 ms).
    inline double noteMs (int k, double bpm) noexcept { return note (k).beats * 60000.0 / std::max (1.0, bpm); }

    // --- display -------------------------------------------------------------------------------------
    inline std::string fmt (const char* f, double v)
    {
        char buf[48];
        std::snprintf (buf, sizeof (buf), f, v);
        return buf;
    }
    inline std::string formatTenth (double v)
    {
        v = quantise (v, 10.0);
        if (v == 0.0) v = 0.0; // no "-0.0"
        return fmt ("%.1f", v);
    }
    inline std::string formatRatio (double v) { return fmt ("%.2f", quantise (v, 100.0)); }
    // 3 significant digits: "0.500", "7.00", "31.6", "100", "3000"
    inline std::string formatMs (double v)
    {
        v = roundSignificant (v, 3);
        if (v < 1.0) return fmt ("%.3f", v);
        if (v < 10.0) return fmt ("%.2f", v);
        if (v < 100.0) return fmt ("%.1f", v);
        return fmt ("%.0f", v);
    }
    inline std::string formatTempo (double v) { return Tempo::isOff (v) ? std::string ("Off") : fmt ("%.1f", quantise (v, 10.0)); }
} // namespace param

// The settings the engine runs with (physical units, already mapped).
struct Settings
{
    double thresholdDb = param::Threshold::defaultValue;
    double ratio = param::Ratio::defaultValue;
    double mixPct = param::Mix::defaultValue;
    double attackMs = param::Attack::defaultValue;
    double punch = param::Punch::defaultValue;
    double releaseMs = param::Release::defaultValue;
    param::Sync sync = param::Sync::Ms;
    int note = 10;
    double manualBpm = param::Tempo::defaultValue; // < 30: Off
    double hostBpm = 120.0;                         // from the host play head
    int analog = 2;                                 // 0 = Off, 1 ... 4
    double outputDb = 0.0;
    bool limiter = false; // false = "Clipper"; both are the same look-ahead peak stage (decision D3)

    // Effective tempo of the sync modes and effective release time (section 2).
    double syncBpm() const noexcept
    {
        if (sync == param::Sync::Bpm && ! param::Tempo::isOff (manualBpm))
            return manualBpm;
        return hostBpm > 0.0 ? hostBpm : 120.0;
    }
    double effectiveReleaseMs() const noexcept
    {
        return sync == param::Sync::Ms ? releaseMs : param::noteMs (note, syncBpm());
    }
};

} // namespace craft::punch
