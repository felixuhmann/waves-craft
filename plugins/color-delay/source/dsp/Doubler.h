// Craft Colour Delay - doubler colour (spec r1 section 7.3, [target]).
//
// Own design: a two-tap delay-line pitch shifter with synchronised splicing (textbook technique, cf. Zoelzer,
// DAFX, "time-domain pitch shifting"; Dattorro, "Effect Design Part 2"). Each tap's delay moves at
// (1 - 2^(cents/1200)) samples per sample, so every grain carries exactly the shifted pitch. Grains overlap by
// half with complementary sin^2 gains. When a tap's grain ends (gain 0) it restarts at a delay chosen within a
// search span so that it is in phase with the other (currently dominant) tap: the lag between the two taps
// maximises the normalised autocorrelation of the input. This avoids the comb cancellation and splice phase
// jumps of a free-running shifter, so sustained tones come out at exactly the shifted frequency.
// All tap delays stay within 3 ... 10 ms (latency, spec: 3-10 ms). Each channel is shifted independently through its
// own buffer (no channel mixing); the two channels share the tap positions (same shift on both).
#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

#include "Filters.h"
#include "Smoothing.h"

namespace craft::cdl
{

class Doubler
{
public:
    static constexpr double minDelaySeconds = 0.003, maxDelaySeconds = 0.010;
    static constexpr double travelSeconds = 0.0027; // delay travel per grain
    // re input at Level 100 %: spec +9.2 dB; +0.2 dB so that tones (+9.4) and band-limited broadband input
    // (about +8.8; the sin^2 crossfade of partially correlated taps loses some energy) straddle the target
    static constexpr double outputGainDb = 9.4;

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        dMin = minDelaySeconds * fs;
        dMax = maxDelaySeconds * fs;
        travel = travelSeconds * fs;
        span = std::max (1.0, dMax - dMin - travel);
        corrHalf = std::max (16, (int) std::lround (0.0027 * fs));
        size = (int) std::ceil (dMax) + corrHalf + 8;
        for (auto& b : buf)
            b.assign ((size_t) size, 0.0);
        centsSmoother.setTimeConstant (0.02, fs);
        reset();
    }

    void reset() noexcept
    {
        for (auto& b : buf)
            std::fill (b.begin(), b.end(), 0.0);
        writePos = 0;
        // tap 0 at full gain at the centre of the window (pure delay at Detune 0), tap 1 silent and due to restart
        taps[0] = { 0.5 * (dMin + dMax), 0.5, false };
        taps[1] = { 0.5 * (dMin + dMax), 0.0, true };
        centsSmoother.snapToTarget();
    }

    void setDetune (double cents) noexcept { centsSmoother.setTarget (std::clamp (cents, -100.0, 100.0)); }
    void snapParameters() noexcept { centsSmoother.snapToTarget(); }

    void process (double inLeft, double inRight, double& outLeft, double& outRight) noexcept
    {
        buf[0][(size_t) writePos] = inLeft;
        buf[1][(size_t) writePos] = inRight;

        const double rate = std::pow (2.0, centsSmoother.next() / 1200.0) - 1.0; // > 0: pitch up (delay decreases)
        if (rate != 0.0)
        {
            const double ageStep = std::abs (rate) / travel;
            for (int k = 0; k < 2; ++k)
            {
                auto& t = taps[k];
                if (t.needsRestart)
                    restart (k, rate);
                t.delay = std::clamp (t.delay - rate, dMin, dMax);
                t.age += ageStep;
                if (t.age >= 1.0)
                {
                    t.age -= 1.0;
                    restart (k, rate);
                }
            }
        }

        outLeft = outRight = 0.0;
        for (const auto& t : taps)
        {
            const double s = std::sin (pi * t.age);
            const double g = s * s;
            if (g == 0.0)
                continue;
            outLeft += g * read (0, t.delay);
            outRight += g * read (1, t.delay);
        }
        const double gain = std::pow (10.0, outputGainDb / 20.0);
        outLeft *= gain;
        outRight *= gain;

        if (++writePos == size)
            writePos = 0;
    }

    double getTapDelay (int k) const noexcept { return taps[k].delay; }

private:
    struct Tap
    {
        double delay = 0.0, age = 0.0;
        bool needsRestart = false;
    };

    double sample (int k) const noexcept // mono sum, k samples ago
    {
        int i = writePos - k;
        while (i < 0) i += size;
        return buf[0][(size_t) i] + buf[1][(size_t) i];
    }

    // New grain for tap k: start delay within the span at the far end of the travel direction, in phase with
    // the other tap (normalised autocorrelation of the input at the lag between the two read positions).
    void restart (int k, double rate) noexcept
    {
        auto& t = taps[k];
        t.needsRestart = false;
        const double lo = rate > 0.0 ? dMax - span : dMin;
        const double hi = rate > 0.0 ? dMax : dMin + span;
        const double other = taps[1 - k].delay;
        const int ref = (int) std::lround (other);
        const double frac = other - ref;

        int kLo = (int) std::ceil (lo - other), kHi = (int) std::floor (hi - other);
        double best = -2.0;
        int bestLag = (int) std::lround (0.5 * (lo + hi) - other);
        double eRef = 0.0;
        for (int i = -corrHalf; i < corrHalf; ++i)
        {
            const double a = sample (ref - i);
            eRef += a * a;
        }
        if (eRef > 1.0e-20)
        {
            for (int lag = kLo; lag <= kHi; ++lag)
            {
                if (lag == 0)
                    continue;
                double ab = 0.0, bb = 0.0;
                for (int i = -corrHalf; i < corrHalf; ++i)
                {
                    const double a = sample (ref - i), b = sample (ref + lag - i);
                    ab += a * b;
                    bb += b * b;
                }
                const double c = ab / std::sqrt (eRef * bb + 1.0e-30);
                if (c > best)
                {
                    best = c;
                    bestLag = lag;
                }
            }
        }
        t.delay = std::clamp (ref + bestLag + frac, dMin, dMax);
    }

    double read (int ch, double d) const noexcept
    {
        const auto& b = buf[(size_t) ch];
        const int di = (int) std::floor (d);
        const double f = d - di;
        auto at = [&] (int k) {
            int i = writePos - k;
            while (i < 0) i += size;
            return b[(size_t) i];
        };
        // 4-point Lagrange around delays di-1 .. di+2
        const double ym1 = at (di - 1), y0 = at (di), y1 = at (di + 1), y2 = at (di + 2);
        const double c0 = -f * (f - 1.0) * (f - 2.0) / 6.0;
        const double c1 = (f + 1.0) * (f - 1.0) * (f - 2.0) / 2.0;
        const double c2 = -(f + 1.0) * f * (f - 2.0) / 2.0;
        const double c3 = (f + 1.0) * f * (f - 1.0) / 6.0;
        return c0 * ym1 + c1 * y0 + c2 * y1 + c3 * y2;
    }

    double fs = 48000.0, dMin = 144.0, dMax = 480.0, travel = 130.0, span = 200.0;
    int corrHalf = 128, size = 0, writePos = 0;
    std::vector<double> buf[2];
    Tap taps[2];
    OnePoleSmoother centsSmoother;
};

} // namespace craft::cdl
