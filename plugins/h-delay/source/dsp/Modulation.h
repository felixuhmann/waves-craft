// Craft Hybrid Delay - delay-time modulation (spec r2 section 8).
//
// Triangle LFO -> nominal offset  D * (Depth/100) * tri / 2  (samples) -> smoothing:
//   1. a one-pole low-pass whose time constant equals the current delay time (tau = 1.0 * D):
//      long delays are smoothed strongly (their excursion saturates around 12-13 ms at 0.5 Hz),
//      short delays with slow, large excursions pass at full amplitude;
//   2. a second-order low-pass (18 Hz, Q 1.0) for the rate dependence at short delays.
// The spec leaves the exact law open (12.1); constants fitted to the 12-row table of section 8
// (tools/fit/mod_fit_r2.py; every row within +-15 %). Both channels share one LFO phase (12.2).
#pragma once

#include <algorithm>
#include <cmath>

#include "Biquad.h"

namespace craft::hdelay
{

inline double triangle (double phase) noexcept // phase in cycles; tri(0.25) = 0, tri(0.5) = +1
{
    return 4.0 * std::abs (phase - std::floor (phase + 0.5)) - 1.0;
}

class TriangleLfo
{
public:
    void prepare (double fs) noexcept { sampleRate = fs; reset(); }
    void reset() noexcept { phase = 0.25; } // starts at the centre: no jump when depth is raised
    void setRate (double hz) noexcept { inc = hz / sampleRate; }
    double next() noexcept
    {
        const double v = triangle (phase);
        phase += inc;
        phase -= std::floor (phase);
        return v;
    }

private:
    double sampleRate = 48000.0, phase = 0.25, inc = 0.0;
};

class ModulationSmoother
{
public:
    static constexpr double tauPerDelay = 1.0;               // one-pole tau = 1.0 x delay time
    static constexpr double lowPassHz = 18.0, lowPassQ = 1.0;

    void prepare (double fs) noexcept
    {
        lp.setCoeffs (BiquadCoeffs::lowPass (BiquadCoeffs::cornerStandard (lowPassHz, fs), lowPassQ));
        reset();
    }

    void reset() noexcept
    {
        y = 0.0;
        lp.reset();
        idle = true;
        lastDelay = -1.0;
    }

    // x: nominal offset in samples, delaySamples: current (glided) delay. Returns the smoothed offset.
    double process (double x, double delaySamples) noexcept
    {
        if (idle)
        {
            if (x == 0.0)
                return 0.0;
            idle = false;
        }
        if (delaySamples != lastDelay)
        {
            lastDelay = delaySamples;
            coeff = std::exp (-1.0 / std::max (1.0, tauPerDelay * delaySamples));
        }
        y = x + coeff * (y - x);
        const double out = lp.process (y);
        // settle exactly to zero once the modulation has been switched off and has decayed
        if (x == 0.0 && std::abs (y) < 1.0e-9 && std::abs (out) < 1.0e-9)
        {
            reset();
            return 0.0;
        }
        return out;
    }

private:
    Biquad lp;
    double y = 0.0, coeff = 0.0, lastDelay = -1.0;
    bool idle = true;
};

} // namespace craft::hdelay
