// Craft Hybrid Delay - parameter mappings and value formats (spec r1 section 2).
// Host-independent: no JUCE dependency. `r` is always the host-normalised value 0..1.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>

namespace craft::hdelay
{

inline double clamp01 (double r) noexcept { return r < 0.0 ? 0.0 : (r > 1.0 ? 1.0 : r); }

namespace param
{
    // ---------------------------------------------------------------------------------------------
    // Delay (ms mode): 1..3500 ms. Lower half linear (1 + 1000 r); upper half a monotone cubic
    // (Fritsch-Carlson / PCHIP) through the spec's points. Values are quantised to 1 ms (display "375 ms").
    struct DelayMs
    {
        static constexpr double minValue = 1.0, maxValue = 3500.0, defaultValue = 375.0;

        static constexpr std::array<double, 11> knotMs { 501.0, 636.0, 815.0, 1024.0, 1265.0, 1540.0,
                                                        1852.0, 2201.0, 2591.0, 3023.0, 3500.0 };
        static constexpr double knotStep = 0.05; // knots at r = 0.50, 0.55, ..., 1.00

        // Continuous taper r -> ms (not quantised).
        static double taper (double r) noexcept
        {
            r = clamp01 (r);
            if (r <= 0.5)
                return 1.0 + 1000.0 * r;

            const auto& slopes = knotSlopes();
            const double x = (r - 0.5) / knotStep;
            const int i = std::min (int (x), int (knotMs.size()) - 2);
            const double t = x - double (i);
            const double h = knotStep;
            const double y0 = knotMs[size_t (i)], y1 = knotMs[size_t (i + 1)];
            const double m0 = slopes[size_t (i)] * h, m1 = slopes[size_t (i + 1)] * h;
            const double t2 = t * t, t3 = t2 * t;
            return (2 * t3 - 3 * t2 + 1) * y0 + (t3 - 2 * t2 + t) * m0 + (-2 * t3 + 3 * t2) * y1 + (t3 - t2) * m1;
        }

        static double fromNorm (double r) noexcept { return std::round (taper (r)); }

        static double toNorm (double ms) noexcept
        {
            if (ms <= 1.0) return 0.0;
            if (ms >= 3500.0) return 1.0;
            if (ms <= 501.0) return (ms - 1.0) / 1000.0;
            double lo = 0.5, hi = 1.0; // the taper is strictly increasing: bisection
            for (int k = 0; k < 60; ++k)
            {
                const double mid = 0.5 * (lo + hi);
                (taper (mid) < ms ? lo : hi) = mid;
            }
            return 0.5 * (lo + hi);
        }

        // PCHIP slopes (ms per unit r) at the knots; the first equals the lower half's slope (1000)
        // so the taper is C1 at r = 0.5.
        static const std::array<double, 11>& knotSlopes() noexcept
        {
            static const std::array<double, 11> s = [] {
                std::array<double, 11> m {};
                std::array<double, 10> d {};
                for (size_t i = 0; i < 10; ++i)
                    d[i] = (knotMs[i + 1] - knotMs[i]) / knotStep;
                m[0] = 1000.0;
                m[10] = d[9];
                for (size_t i = 1; i < 10; ++i)
                    m[i] = (d[i - 1] * d[i] <= 0.0) ? 0.0 : 2.0 / (1.0 / d[i - 1] + 1.0 / d[i]); // harmonic mean
                return m;
            }();
            return s;
        }
    };

    // ---------------------------------------------------------------------------------------------
    // Note values (index 0..19). Duration in beats (1/4 = one beat, T = x2/3, D = x1.5).
    inline constexpr int numNotes = 20;
    inline constexpr int defaultNote = 11; // 1/8D
    inline constexpr std::array<const char*, numNotes> noteNames {
        "1/64T", "1/64", "1/64D", "1/32T", "1/32", "1/32D", "1/16T", "1/16", "1/16D", "1/8T",
        "1/8",   "1/8D", "1/4T",  "1/4",   "1/4D", "1/2T",  "1/2",   "1/2D", "1 bar", "2 bars"
    };

    inline double noteBeats (int index) noexcept
    {
        index = std::clamp (index, 0, numNotes - 1);
        if (index == 18) return 4.0;  // 1 bar (4/4)
        if (index == 19) return 8.0;  // 2 bars
        // groups of three (T, straight, D) for 1/64 .. 1/2
        const int group = index / 3;                     // 0: 1/64 ... 5: 1/2
        const double straight = 4.0 / std::pow (2.0, 6 - group); // 1/64 = 1/16 beat ... 1/2 = 2 beats
        switch (index % 3)
        {
            case 0:  return straight * 2.0 / 3.0;
            case 1:  return straight;
            default: return straight * 1.5;
        }
    }

    // ---------------------------------------------------------------------------------------------
    inline constexpr int numSyncModes = 3;
    enum class Sync { Bpm = 0, Host = 1, Ms = 2 };
    inline constexpr std::array<const char*, numSyncModes> syncNames { "BPM", "Host", "ms" };

    inline constexpr int numAnalogModes = 5;
    inline constexpr std::array<const char*, numAnalogModes> analogNames { "Off", "1", "2", "3", "4" };
    inline constexpr int defaultAnalog = 2;

    // BPM (manual tempo): 30 + 270 r, default 120
    struct Bpm
    {
        static constexpr double minValue = 30.0, maxValue = 300.0, defaultValue = 120.0;
        static double fromNorm (double r) noexcept { return 30.0 + 270.0 * clamp01 (r); }
        static double toNorm (double v) noexcept { return clamp01 ((v - 30.0) / 270.0); }
    };

    // Feedback: 200 r %, default 60
    struct Feedback
    {
        static constexpr double minValue = 0.0, maxValue = 200.0, defaultValue = 60.0;
        static double fromNorm (double r) noexcept { return 200.0 * clamp01 (r); }
        static double toNorm (double v) noexcept { return clamp01 (v / 200.0); }
    };

    // Mix: 0..100 linear, default 100
    struct Mix
    {
        static constexpr double minValue = 0.0, maxValue = 100.0, defaultValue = 100.0;
        static double fromNorm (double r) noexcept { return 100.0 * clamp01 (r); }
        static double toNorm (double v) noexcept { return clamp01 (v / 100.0); }
    };

    // Output: -18..+18 dB linear in dB, default 0
    struct Output
    {
        static constexpr double minValue = -18.0, maxValue = 18.0, defaultValue = 0.0;
        static double fromNorm (double r) noexcept { return -18.0 + 36.0 * clamp01 (r); }
        static double toNorm (double v) noexcept { return clamp01 ((v + 18.0) / 36.0); }
    };

    // Mod Rate: 0.1 * 60000^r Hz, default 0.1 Hz
    struct ModRate
    {
        static constexpr double minValue = 0.1, maxValue = 6000.0, defaultValue = 0.1;
        static double fromNorm (double r) noexcept { return 0.1 * std::pow (60000.0, clamp01 (r)); }
        static double toNorm (double v) noexcept { return v <= 0.1 ? 0.0 : clamp01 (std::log (v / 0.1) / std::log (60000.0)); }
    };

    // Mod Depth: 0..100 %, default 0
    struct ModDepth
    {
        static constexpr double minValue = 0.0, maxValue = 100.0, defaultValue = 0.0;
        static double fromNorm (double r) noexcept { return 100.0 * clamp01 (r); }
        static double toNorm (double v) noexcept { return clamp01 (v / 100.0); }
    };

    // Filters (spec r2 section 2): f = 19 * 1058^r Hz over the whole range; HiPass is Off only at r = 0,
    // LoPass is Off only at r = 1.
    // The host parameter value *is* r; 0 Hz encodes "Off".
    struct Filter
    {
        static constexpr double base = 19.0, ratio = 1058.0;
        static double freq (double r) noexcept { return base * std::pow (ratio, clamp01 (r)); }
        static double toNorm (double hz) noexcept { return hz <= base ? 0.0 : clamp01 (std::log (hz / base) / std::log (ratio)); }
    };
    struct HiPass
    {
        static constexpr double defaultNorm = 0.0, minOnNorm = 1.0e-4; // smallest r that is not Off (GUI/typing)
        static double fromNorm (double r) noexcept { return r <= 0.0 ? 0.0 : Filter::freq (r); }
    };
    struct LoPass
    {
        static constexpr double defaultNorm = 1.0, maxOnNorm = 1.0 - 1.0e-4; // largest r that is not Off
        static double fromNorm (double r) noexcept { return r >= 1.0 ? 0.0 : Filter::freq (r); }
    };

    // ---------------------------------------------------------------------------------------------
    // Display formats (spec section 2).
    inline std::string fmt (const char* f, double v)
    {
        char buf[64];
        std::snprintf (buf, sizeof (buf), f, v);
        return buf;
    }
    inline double noNegZero (double v, double q) noexcept
    {
        v = std::round (v * q) / q;
        return v == 0.0 ? 0.0 : v;
    }

    inline std::string formatMs (double ms) { return fmt ("%.0f ms", std::round (ms)); }
    inline std::string formatPercent (double v) { return fmt ("%.0f", noNegZero (v, 1.0)); }
    inline std::string formatDepth (double v) { return fmt ("%.0f %%", noNegZero (v, 1.0)); }
    inline std::string formatOutput (double db) { return fmt ("%.1f dB", noNegZero (db, 10.0)); }
    inline std::string formatBpm (double bpm) { return fmt ("%.1f", bpm); }

    inline std::string formatRate (double hz)
    {
        if (hz >= 1000.0) return fmt ("%.2fk Hz", hz / 1000.0);
        if (hz >= 100.0)  return fmt ("%.0f Hz", hz);
        if (hz >= 10.0)   return fmt ("%.1f Hz", hz);
        return fmt ("%.2f Hz", hz);
    }

    inline std::string formatFilter (double hz)
    {
        if (hz <= 0.0)      return "Off";
        if (hz >= 10000.0)  return fmt ("%.1fkHz", hz / 1000.0);
        if (hz >= 1000.0)   return fmt ("%.2fkHz", hz / 1000.0);
        return fmt ("%.0fHz", hz);
    }
} // namespace param

} // namespace craft::hdelay
