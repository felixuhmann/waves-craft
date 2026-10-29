// Craft Dynamic Distortion - envelope detector and expansion law (spec r1 sections 4.3, 6.2, 8).
//
// Detector: per-channel full-wave peak follower in the linear amplitude domain (Giannoulis, Massberg, Reiss,
// "Digital dynamic range compressor design", JAES 2012, branching peak detector):
//     e = |u| > e ? a e + (1 - a) |u|  :  r e + (1 - r) |u|,   a = exp(-1 / (tau_att fs)), r = exp(-1 / (tau_rel fs))
// with tau_att = Attack / 3.67 and tau_rel = Release / 1.99 (section 4.3 / 8).
//
// The release phase starts only after the input has stayed below the envelope for a hold time of 2 ms
// (implementation choice for open question 1, see IMPLEMENTATION_NOTES.md): it keeps the detector on the
// waveform's peak from ~250 Hz up (steady state independent of Attack, section 6.2) while it still follows
// low-frequency waveforms with fast Attack (the low-frequency distortion of section 6.1).
//
// Expansion: static curve dG(L) of the detector level L = 20 log10(e) (dB re full-scale peak), piecewise
// linear in dB; above the section 4.3 table it continues with 0.3 dB/dB and is capped at +8 dB.
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
    inline constexpr double slopeAbove = 0.3, capDb = 8.0;

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

    // The section 4.3 table with its RMS levels converted to the detector's peak scale (+3.0103 dB): with the
    // 2 ms detector hold the follower sits at the sine's peak in steady state, so the table applies directly
    // (tools/Calibrate.cpp "fit 2" reproduces these values within 0.002 dB). Knot 0 is the onset 3 dB below.
    inline const Curve& defaultCurve() noexcept
    {
        static const Curve c { { {
            { -39.9897, 0.00 },
            { -36.9897, 0.00 },
            { -34.9897, 0.09 },
            { -32.9897, 0.26 },
            { -30.9897, 0.52 },
            { -28.9897, 0.88 },
            { -26.9897, 1.36 },
            { -24.9897, 1.96 },
            { -22.9897, 2.65 },
            { -20.9897, 3.41 },
            { -18.9897, 4.21 },
            { -16.9897, 4.99 },
            { -14.9897, 5.74 },
            { -12.9897, 6.42 },
            { -10.9897, 7.05 },
        } } };
        return c;
    }
} // namespace expansion

class PeakFollower
{
public:
    static constexpr double attackDivisor = 3.67, releaseDivisor = 1.99;

    void setTimes (double attackMs, double releaseMs, double holdMs, double fs) noexcept
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
