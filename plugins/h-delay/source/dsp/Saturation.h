// Craft Hybrid Delay - envelope-controlled loop saturation (spec r2 section 4.1).
//
//   a[n] = |v[n]|
//   e[n] = a[n]                          if a[n] > e[n-1]   (instant attack; starts a 1.1 ms hold)
//        = e[n-1]                         during the hold
//        = rho e[n-1] + (1 - rho) a[n]   otherwise          (release, tau = 8 ms)
//   S(v) = v / (1 + 0.72 e + 0.28 a)
//
// This is the spec's recommended form with a short peak hold added and the release shortened from 9.7 to
// 8 ms: with the plain 9.7 ms release the 1 ms row of the short-delay table (4.4) misses by 0.32-0.37 dB
// (tolerance 0.3 dB). Checked against 4.2-4.4 by tools/fit/sat_check_r2.py and tests/CoreTests.cpp.
// A single isolated impulse is saturated exactly like v / (1 + |v|); steady high-frequency tones are
// compressed on the envelope (less 3rd harmonic); closely spaced repeats are compressed by the envelope
// of the previous repeat. Peak detector with attack / hold / release: Giannoulis, Massberg & Reiss,
// JAES 60(6), 2012; Zoelzer, DAFX (dynamics processing).
#pragma once

#include <cmath>

namespace craft::hdelay
{

class EnvelopeSaturator
{
public:
    static constexpr double releaseSeconds = 0.008, holdSeconds = 0.0011;
    static constexpr double envelopeWeight = 0.72, instantWeight = 0.28;

    void prepare (double fs) noexcept
    {
        rho = std::exp (-1.0 / (releaseSeconds * fs));
        holdSamples = int (std::lround (holdSeconds * fs));
        reset();
    }
    void reset() noexcept
    {
        env = 0.0;
        hold = 0;
    }

    double process (double v) noexcept
    {
        const double a = std::abs (v);
        if (a > env)
        {
            env = a;
            hold = holdSamples;
        }
        else if (hold > 0)
            --hold;
        else
        {
            env = env * rho + a * (1.0 - rho);
            if (env < 1.0e-30)
                env = 0.0; // no denormals in silence
        }
        return v / (1.0 + envelopeWeight * env + instantWeight * a);
    }

private:
    double rho = 0.0, env = 0.0;
    int holdSamples = 0, hold = 0;
};

} // namespace craft::hdelay
