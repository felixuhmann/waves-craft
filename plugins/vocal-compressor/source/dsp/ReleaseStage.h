// Craft Vocal Compressor - compressor gain with program-dependent release (spec r1 sections 4.3 and 4.4).
// Host-independent: no JUCE dependency.
#pragma once

#include <algorithm>
#include <cmath>

#include "Primitives.h"

namespace craft::vocal
{

// The spec gives the release normatively only by the tables of sections 9.3 / 9.4 (decision D3). This
// implementation follows the informative structure of section 4.4 - a fast path (the look-ahead hold itself) and
// slower envelopes of the held level - with constants fitted to the tables (see IMPLEMENTATION_NOTES.md).
//
// All envelopes work on the "compression ratio" z = 1 + (k - 1) * level, for which the compressor's normalised gain
// is 1 / z (section 4.3: t = 1 / (1 + (k - 1) a)); z = 1 is no reduction. Each one-pole envelope moves in a warped
// domain u = (z^lambda - 1) / lambda (lambda = 1: amplitude, 0: dB, -1: linear gain), with separate time constants and
// warps for rising (attack) and falling (release) targets:
//   fast   : z_h  = 1 + (k - 1) * A[n]            A = forward max over the look-ahead window (instant attack/release)
//   charge : z_c  follows z_h                     charges slowly, follows falling levels instantly
//   medium : z_m  follows z_c                     the two-pole charge (S-shaped in time) and fast release give the
//                                                 short-lived extra reduction after bursts of intermediate length
//                                                 (table 9.4) while short transients pass with the fast path only
//   slow   : z_s1 follows z_held, z_s2 follows z_s1   z_held from the level held over `holdMs` (so a steady 50 Hz
//                                                 tone gives a constant target, section 4.4 item 4); the cascade gives
//                                                 the S-shaped charge and release with a plateau that grows with the
//                                                 amount of compression (table 9.3)
//   q = 1 / max(z_h, z_m, z_s2)
// The envelopes are stored as levels (z - 1) / (k - 1), so a change of Compression rescales them consistently.
struct ReleaseLaw
{
    struct Envelope
    {
        double attackMs, attackWarp, releaseMs, releaseWarp;
    };
    // releaseMs = 0: instantaneous release
    static constexpr Envelope charge { 111.894, 0.03, 0.0, 1.0 };
    static constexpr Envelope medium { 117.501, -0.006, 27.867, 1.313 };
    static constexpr Envelope slow1 { 206.882, -0.247, 58.261, -1.362 };
    static constexpr Envelope slow2 { 216.423, 0.04, 394.772, -0.63 };
    static constexpr double holdMs = 12.0;
};

class ReleaseStage
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        configure (charge, ReleaseLaw::charge);
        configure (medium, ReleaseLaw::medium);
        configure (slow1, ReleaseLaw::slow1);
        configure (slow2, ReleaseLaw::slow2);
        hold.prepare (std::max (1, int (std::lround (ReleaseLaw::holdMs * 1.0e-3 * fs))));
        reset();
    }

    void reset() noexcept
    {
        hold.reset (0.0);
        charge.level = medium.level = slow1.level = slow2.level = 0.0;
        q = 1.0;
    }

    // A: forward-max level of the aligned sample; km1 = k - 1 (>= 0). Returns q[n] (<= 1).
    double process (double A, double km1) noexcept
    {
        const double held = hold.push (A);
        if (km1 <= 0.0)
        {
            // no compression: envelopes track the level so that a later Compression change starts consistently
            charge.level = A;
            medium.level = A;
            slow1.level = held;
            slow2.level = held;
            q = 1.0;
            return q;
        }
        const double zh = 1.0 + km1 * A;
        const double zm = step (medium, step (charge, zh, km1), km1);
        const double z1 = step (slow1, 1.0 + km1 * held, km1);
        const double z2 = step (slow2, z1, km1);
        q = 1.0 / std::max (zh, std::max (zm, z2));
        return q;
    }

    double getState() const noexcept { return q; }

private:
    struct State
    {
        double ca = 0.0, cr = 0.0, la = 1.0, lr = 1.0;
        double level = 0.0;
    };

    void configure (State& s, const ReleaseLaw::Envelope& e) noexcept
    {
        s.ca = onePoleCoeff (e.attackMs, fs);
        s.cr = e.releaseMs > 0.0 ? onePoleCoeff (e.releaseMs, fs) : 1.0;
        s.la = e.attackWarp;
        s.lr = e.releaseWarp;
    }

    static double warp (double z, double lambda) noexcept
    {
        return std::abs (lambda) < 1.0e-6 ? std::log (z) : (std::pow (z, lambda) - 1.0) / lambda;
    }

    static double unwarp (double u, double lambda) noexcept
    {
        if (std::abs (lambda) < 1.0e-6)
            return std::exp (u);
        const double b = 1.0 + lambda * u;
        return b <= 1.0e-12 ? 1.0e12 : std::pow (b, 1.0 / lambda);
    }

    // One step of an envelope towards target z; returns the new z.
    static double step (State& s, double target, double km1) noexcept
    {
        double z = 1.0 + km1 * s.level;
        if (target != z)
        {
            const bool rising = target > z;
            if (! rising && s.cr >= 1.0)
            {
                s.level = (target - 1.0) / km1;
                return target;
            }
            const double lambda = rising ? s.la : s.lr;
            const double u = warp (z, lambda);
            z = unwarp (u + (warp (target, lambda) - u) * (rising ? s.ca : s.cr), lambda);
            z = std::max (1.0, z);
            s.level = (z - 1.0) / km1;
        }
        return z;
    }

    double fs = 48000.0;
    SlidingMax hold;
    State charge, medium, slow1, slow2;
    double q = 1.0;
};

} // namespace craft::vocal
