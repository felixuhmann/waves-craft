// Craft Colour Delay - stereo-linked wet-path compressor (spec r1 section 5).
#pragma once

#include <algorithm>
#include <cmath>

namespace craft::cdl
{

class Compressor
{
public:
    // Static curve (spec section 5)
    static constexpr double threshold = -23.23, ratio = 6.196, knee = 23.22, parallel = 0.0292;
    static constexpr double detectorReleaseS = 1.06e-3, gainAttackS = 8.17e-3, gainReleaseS = 0.439;

    // High-level taper (implementation addition, see IMPLEMENTATION_NOTES.md): above about +4 dBFS the measured
    // static gains of spec section 5 (+6 dBFS: -20.83 dB, +12 dBFS: -23.07 dB) compress less than the fitted
    // curve; the slope of GR is reduced by `taperAmount` above `taperStart` with a soft knee of `taperKnee`.
    // Below +2 dBFS detector level this term is exactly zero, i.e. the spec model is unchanged.
    static constexpr double taperStart = 4.0, taperKnee = 4.0, taperAmount = 0.25;

    static double softKnee (double over, double width) noexcept
    {
        if (2.0 * over < -width) return 0.0;
        if (2.0 * over > width) return over;
        const double t = over + width / 2.0;
        return t * t / (2.0 * width);
    }

    static double staticGainReductionDb (double levelDb, bool withTaper = true) noexcept
    {
        double gr = (1.0 - 1.0 / ratio) * softKnee (levelDb - threshold, knee);
        if (withTaper)
            gr -= taperAmount * softKnee (levelDb - taperStart, taperKnee);
        return gr;
    }

    static double outputGain (double grDb) noexcept { return (1.0 - parallel) * std::pow (10.0, -grDb / 20.0) + parallel; }

    void prepare (double fs) noexcept
    {
        detRelease = std::exp (-1.0 / (detectorReleaseS * fs));
        grAttack = std::exp (-1.0 / (gainAttackS * fs));
        grRelease = std::exp (-1.0 / (gainReleaseS * fs));
        reset();
    }

    void reset() noexcept
    {
        env = 0.0;
        grDb = 0.0;
        gain = 1.0;
    }

    void setTaperEnabled (bool on) noexcept { taper = on; }

    // Returns the gain to apply to both channels for this sample.
    double process (double l, double r) noexcept
    {
        const double lvl = std::max (std::abs (l), std::abs (r));
        env = lvl > env ? lvl : lvl + detRelease * (env - lvl);

        // below the knee GR is exactly 0 (signals <= about -35 dBFS): skip the log
        double target = 0.0;
        if (env > kneeStartLinear)
            target = staticGainReductionDb (20.0 * std::log10 (env), taper);

        const double c = target > grDb ? grAttack : grRelease;
        grDb = target + c * (grDb - target);
        if (grDb < 1.0e-12)
        {
            grDb = 0.0;
            gain = 1.0;
        }
        else
        {
            gain = outputGain (grDb);
        }
        return gain;
    }

    double getGainReductionDb() const noexcept { return grDb; }
    double getGain() const noexcept { return gain; }

private:
    static inline const double kneeStartLinear = std::pow (10.0, (threshold - knee / 2.0) / 20.0);
    double detRelease = 0.0, grAttack = 0.0, grRelease = 0.0;
    double env = 0.0, grDb = 0.0, gain = 1.0;
    bool taper = true;
};

} // namespace craft::cdl
