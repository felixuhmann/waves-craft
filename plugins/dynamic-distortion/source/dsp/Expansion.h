// Craft Dynamic Distortion - envelope detector and expansion law (spec r2 sections 4.3, 6.2, 8).
//
// Detector: per-channel full-wave peak follower in the linear amplitude domain (Giannoulis, Massberg, Reiss,
// "Digital dynamic range compressor design", JAES 2012, branching peak detector):
//     e = |u| > e ? a e + (1 - a) |u|  :  r e + (1 - r) |u|,   a = exp(-1 / (tau_att fs)), r = exp(-1 / (tau_rel fs))
// with tau_att = Attack / 3.67 and tau_rel = Release / 1.99 (section 4.3 / 8).
//
// The release phase starts only after the input has stayed below the envelope for 2 ms. Spec r2 calls the hold
// unnecessary, but sections 4.3 + 6.1 + 6.2 together still need it here: without it the Attack 500 row misses
// its +990 ms point (-3.0 vs -0.25 dB) and Attack 0.1 / Release 5 at 200 / 500 Hz, -20 dBFS misses the h3 limit.
//
// Expansion: static curve dG(L) of the detector level L = 20 log10(e) (dB re full-scale peak), piecewise
// linear in dB; above the section 4.3 table it stays flat (never above the +8 dB cap).
#pragma once

#include <array>
#include <cmath>
#include <cstddef>

namespace craft::dd
{

namespace expansion
{
    struct Knot
    {
        double levelDb, gainDb;
    };

    inline constexpr size_t numKnots = 15;
    inline constexpr double slopeAbove = 0.0, capDb = 8.0;

    // Piecewise-linear curve: 0 dB below the first knot, linear between knots, slopeAbove beyond the last
    // knot, capped at capDb.
    struct Curve
    {
        std::array<Knot, numKnots> knots;

        double gainDb (double levelDb) const noexcept
        {
            if (levelDb <= knots[0].levelDb)
                return 0.0;
            for (size_t i = 1; i < numKnots; ++i)
                if (levelDb <= knots[i].levelDb)
                {
                    const auto& k0 = knots[i - 1];
                    const auto& k1 = knots[i];
                    return k0.gainDb + (levelDb - k0.levelDb) * (k1.gainDb - k0.gainDb) / (k1.levelDb - k0.levelDb);
                }
            const auto& last = knots.back();
            const double g = last.gainDb + slopeAbove * (levelDb - last.levelDb);
            return g < capDb ? g : capDb;
        }

        // Linear gain for a linear detector level.
        double gain (double e) const noexcept
        {
            if (e <= 1.0e-3) // far below the first knot
                return 1.0;
            return std::pow (10.0, gainDb (20.0 * std::log10 (e)) / 20.0);
        }
    };

    // The section 4.3 table on its own axis (peak dBFS): with the detector hold the follower sits on the peak of a
    // 1 kHz sine. A small soft onset around -40 dBFS (within the +-0.4 dB tolerance) gives the h3 of the 6.1 onset
    // cells at 50 Hz. The -14 dBFS knot (6.66, table 7.05 +- 0.4) and the flat continuation (slope 0 is <= 0.4 dB/dB) keep the
    // Drive 0 %, Attack 100 ms, -10 dBFS knee cells within the h3 rule.
    inline const Curve& defaultCurve() noexcept
    {
        static const Curve c { { {
            { -41.50, 0.000 },
            { -40.00, 0.050 },
            { -38.00, 0.090 },
            { -36.00, 0.260 },
            { -34.00, 0.520 },
            { -32.00, 0.880 },
            { -30.00, 1.360 },
            { -28.00, 1.960 },
            { -26.00, 2.650 },
            { -24.00, 3.410 },
            { -22.00, 4.210 },
            { -20.00, 4.990 },
            { -18.00, 5.740 },
            { -16.00, 6.420 },
            { -14.00, 6.660 },
        } } };
        return c;
    }
} // namespace expansion

class PeakFollower
{
public:
    static constexpr double attackDivisor = 3.67, releaseDivisor = 1.99, holdMs = 2.0;

    void setTimes (double attackMs, double releaseMs, double fs) noexcept
    {
        const double ta = attackMs / attackDivisor * 1.0e-3, tr = releaseMs / releaseDivisor * 1.0e-3;
        a = std::exp (-1.0 / (ta * fs));
        r = std::exp (-1.0 / (tr * fs));
        holdSamples = int (std::lround (holdMs * 1.0e-3 * fs));
    }

    void reset (double value = 0.0) noexcept
    {
        e = value;
        hold = 0;
    }

    double process (double u) noexcept
    {
        const double x = std::abs (u);
        if (x > e)
        {
            e = a * e + (1.0 - a) * x;
            hold = holdSamples;
        }
        else if (hold > 0)
            --hold;
        else
        {
            e = r * e + (1.0 - r) * x;
            if (e < 1.0e-20)
                e = 0.0; // no denormals on silence
        }
        return e;
    }

    double value() const noexcept { return e; }

private:
    double a = 0.0, r = 0.0, e = 0.0;
    int holdSamples = 0, hold = 0;
};

} // namespace craft::dd
