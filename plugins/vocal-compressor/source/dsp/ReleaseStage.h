// Craft Vocal Compressor - compressor gain with program-dependent release (spec r2 sections 4.3 and 4.4).
// Host-independent: no JUCE dependency.
#pragma once

#include <algorithm>
#include <cmath>

#include "Primitives.h"

namespace craft::vocal
{

// The spec gives the release normatively by the tables of sections 9.3 / 9.4 / 9.7 and the ripple table 9.8
// (decision D3). This implementation follows the informative structure of section 4.4 - a fast path (the look-ahead
// hold itself) and slower envelopes - with constants fitted to those tables (see IMPLEMENTATION_NOTES.md).
//
// All envelopes work on the "compression ratio" z = 1 + (k - 1) * level, for which the compressor's normalised gain
// is 1 / z (section 4.3: t = 1 / (1 + (k - 1) a)); z = 1 is no reduction. Each one-pole envelope moves in a warped
// domain u = (z^lambda - 1) / lambda (lambda = 1: amplitude, 0: dB, -1: linear gain, -2: gain squared ...), with
// separate time constants and warps for rising (attack) and falling (release) targets. The release time constant is
// program dependent:
//     tau = releaseMs * exp(releaseDest * x_target / 10 + releaseOwn * x_own / 10)
// with x_target = 20 log10(target z) (the reduction the signal now asks for, in dB) and x_own = 20 log10(z) (the
// envelope's own reduction). So a release toward a still strongly compressed level is slow, a release toward a
// lightly compressed level is fast (section 4.4 item 6).
//
//   fast      : z_h  = 1 + (k - 1) * A[n]          A = forward max over the look-ahead window (instant)
//   transient : z_t  follows z_h, per sample       its destination-dependent release holds the reduction after loud
//                                                  passages when the level drops moderately (table 9.7) and across the
//                                                  cycles of strongly compressed low-frequency tones (table 9.8), but
//                                                  lets go within milliseconds when the following level is only
//                                                  lightly compressed (table 9.4)
//   charge    : z_c  follows z_held                the level held over `holdMs`; charges while the signal is loud and
//                                                  keeps its charge through short pauses (slow release)
//   medium    : z_m  follows min(z_h, z_c)         once the level drops it releases quickly; its reduction is limited
//                                                  by the accumulated charge, so it grows with the time spent loud -
//                                                  also across repeated bursts (tables 9.4 and 9.7)
//   slow      : z_s1 follows z_held, z_s2 follows z_s1   S-shaped charge and release with a plateau that grows with
//                                                  the amount of compression (table 9.3)
//   q = 1 / max(z_h, z_t, z_m, z_s2)
// The envelopes are stored as levels (z - 1) / (k - 1), so a change of Compression rescales them consistently.
struct ReleaseLaw
{
    struct Envelope
    {
        double attackMs, attackWarp, releaseMs, releaseWarp, releaseDest, releaseOwn;
    };
    // Fitted constants (IMPLEMENTATION_NOTES.md, "Release law").
    Envelope transient { 10.09198, -2.99999, 0.58332, -0.55901, 4.06849, -0.70493 };
    Envelope charge { 90.40063, -0.52747, 1970.39571, -2.95697, 5.98508, 0.45515 };
    Envelope medium { 73.28579, 0.28534, 7.19103, 0.20588, 1.78802, 0.59626 };
    Envelope slow1 { 52.49309, -1.17733, 41.80864, -0.57309, 1.54241, -0.98166 };
    Envelope slow2 { 170.84475, -0.94753, 341.87348, 0.08523, 0.6698, -0.38402 };
    double holdMs = 5.0261;
    // The charge / medium / slow envelopes are updated at a control rate of about 12 kHz; the fast path and the
    // transient envelope run per sample. Each control step sees the maximum level of its block.
    double controlRateHz = 12000.0;
};

class ReleaseStage
{
public:
    // Replaces the release constants (development / fitting aid); call before prepare().
    void setLaw (const ReleaseLaw& l) { law = l; }
    const ReleaseLaw& getLaw() const noexcept { return law; }

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        decimation = std::max (1, int (std::lround (fs / law.controlRateHz)));
        controlRate = fs / decimation;
        configure (transient, law.transient, fs);
        configure (charge, law.charge, controlRate);
        configure (medium, law.medium, controlRate);
        configure (slow1, law.slow1, controlRate);
        configure (slow2, law.slow2, controlRate);
        hold.prepare (std::max (1, int (std::lround (law.holdMs * 1.0e-3 * fs))));
        reset();
    }

    void reset() noexcept
    {
        hold.reset (0.0);
        transient.level = charge.level = medium.level = slow1.level = slow2.level = 0.0;
        q = 1.0;
        zSlow = 1.0;
        blockHeld = blockA = 0.0;
        counter = 0;
    }

    // A: forward-max level of the aligned sample; km1 = k - 1 (>= 0). Returns q[n] (<= 1).
    double process (double A, double km1) noexcept
    {
        const double held = hold.push (A);
        blockHeld = std::max (blockHeld, held);
        blockA = std::max (blockA, A);
        if (++counter >= decimation)
        {
            controlStep (km1);
            counter = 0;
            blockHeld = blockA = 0.0;
        }
        if (km1 <= 0.0)
        {
            transient.level = A;
            q = 1.0;
            return q;
        }
        const double zh = 1.0 + km1 * A;
        const double zt = step (transient, zh, km1);
        q = 1.0 / std::max (std::max (zh, zt), zSlow);
        return q;
    }

    double getState() const noexcept { return q; }

private:
    struct State
    {
        double ca = 0.0, la = 1.0, lr = 1.0, releaseMs = 1.0, dest = 0.0, own = 0.0, rate = 48000.0;
        double level = 0.0;
    };

    void controlStep (double km1) noexcept
    {
        if (km1 <= 0.0)
        {
            // no compression: envelopes track the level so that a later Compression change starts consistently
            charge.level = medium.level = slow1.level = slow2.level = blockHeld;
            zSlow = 1.0;
            return;
        }
        const double zl = 1.0 + km1 * blockHeld;
        // the medium envelope follows the level, but no higher than the accumulated charge
        const double zm = step (medium, std::min (1.0 + km1 * blockA, step (charge, zl, km1)), km1);
        const double z2 = step (slow2, step (slow1, zl, km1), km1);
        zSlow = std::max (zm, z2);
    }

    static void configure (State& s, const ReleaseLaw::Envelope& e, double rate) noexcept
    {
        s.rate = rate;
        s.ca = onePoleCoeff (e.attackMs, rate);
        s.la = e.attackWarp;
        s.lr = e.releaseWarp;
        s.releaseMs = e.releaseMs;
        s.dest = e.releaseDest;
        s.own = e.releaseOwn;
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
            double lambda, c;
            if (target > z)
            {
                lambda = s.la;
                c = s.ca;
            }
            else
            {
                lambda = s.lr;
                constexpr double dbPerLn = 8.685889638065035; // 20 / ln(10)
                const double xTarget = dbPerLn * std::log (target), xOwn = dbPerLn * std::log (z);
                const double tauMs = s.releaseMs * std::exp ((s.dest * xTarget + s.own * xOwn) * 0.1);
                c = onePoleCoeff (tauMs, s.rate);
            }
            const double u = warp (z, lambda);
            z = std::max (1.0, unwarp (u + (warp (target, lambda) - u) * c, lambda));
            s.level = (z - 1.0) / km1;
        }
        return z;
    }

    double fs = 48000.0, controlRate = 6000.0;
    int decimation = 8, counter = 0;
    ReleaseLaw law;
    double blockHeld = 0.0, blockA = 0.0, zSlow = 1.0;
    SlidingMax hold;
    State transient, charge, medium, slow1, slow2;
    double q = 1.0;
};

} // namespace craft::vocal
