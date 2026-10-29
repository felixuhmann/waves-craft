// Craft Hybrid Delay - delay-time modulation (spec r1 section 8).
//
// Triangle LFO -> nominal offset  D * (Depth/100) * tri / 2  (samples) -> smoothing:
//   1. slope compression: the per-sample change of the offset is passed through a soft limiter
//        dy = S tanh(dx / S),  S = 0.021 samples/sample (= 21 ms of delay change per second),
//      plus a slow leak (tau 5 s) towards the nominal offset so the output cannot drift;
//   2. a second-order low-pass (20 Hz, Q 1.2).
// The spec leaves the exact law open (12.1); this model was fitted to the section 8 tables (all points
// within +-15 %, see tests/ModulationTests.cpp). Both channels share one LFO phase (assumption 12.2).
#pragma once

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
    static constexpr double slopeLimit = 0.021;   // samples of delay change per sample (21 ms/s)
    static constexpr double leakSeconds = 5.0;
    static constexpr double lowPassHz = 20.0, lowPassQ = 1.2;

    void prepare (double fs) noexcept
    {
        leak = 1.0 / (leakSeconds * fs);
        lp.setCoeffs (BiquadCoeffs::lowPass (BiquadCoeffs::cornerStandard (lowPassHz, fs), lowPassQ));
        reset();
    }

    void reset() noexcept
    {
        y = previous = 0.0;
        lp.reset();
        idle = true;
    }

    // x: nominal offset in samples. Returns the smoothed offset.
    double process (double x) noexcept
    {
        if (idle)
        {
            if (x == 0.0)
                return 0.0;
            idle = false;
        }
        const double dx = x - previous;
        previous = x;
        y += slopeLimit * std::tanh (dx / slopeLimit) + (x - y) * leak;
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
    double y = 0.0, previous = 0.0, leak = 0.0;
    bool idle = true;
};

} // namespace craft::hdelay
