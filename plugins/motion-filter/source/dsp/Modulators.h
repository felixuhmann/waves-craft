// Craft Motion Filter - modulation sources (spec r1 section 6): LFO, step sequencer, envelope follower.
#pragma once

#include <array>
#include <cmath>

#include "Blocks.h"

namespace craft::mf
{

// Position counter in cycles (LFO) or steps (sequencer). Free-running between resyncs; when the host
// transport runs and the source is tempo-synced, the position follows the host's ppq position.
struct Clock
{
    double pos = 0.0; // cycles or steps since start
    double inc = 0.0; // per sample

    void reset() noexcept { pos = 0.0; }
    // Follow an externally given position; small numeric drift between blocks is ignored so a steady
    // transport never makes the position step backwards.
    void follow (double hostPos) noexcept
    {
        if (std::abs (hostPos - pos) > 1.0e-6 * std::max (1.0, std::abs (hostPos)) + 1.0e-4)
            pos = hostPos;
    }
    void advance() noexcept { pos += inc; }
};

// §6.1 LFO. Phase 0 at transport/plugin start: sine sin(2 pi phi); triangle starts at 0 rising; saw falls
// from +1 to -1; pulse +1 for the first half cycle; S&H draws a new uniform value in [-1, 1] each cycle.
class Lfo
{
public:
    Clock clock;

    void reset() noexcept
    {
        clock.reset();
        lastCycle = 0;
        held = random.nextBipolar();
    }
    void setShape (LfoShape s) noexcept { shape = s; }

    // Waveform value at the current position (then call advance()).
    double value() noexcept
    {
        const double fl = std::floor (clock.pos + 1.0e-9); // robust against accumulated rounding
        const auto cycle = static_cast<long long> (fl);
        if (cycle != lastCycle)
        {
            lastCycle = cycle;
            held = random.nextBipolar();
        }
        return waveform (shape, std::max (0.0, clock.pos - fl), held);
    }
    void advance() noexcept { clock.advance(); }

    static double waveform (LfoShape s, double phi, double sampleHoldValue) noexcept
    {
        switch (s)
        {
            case LfoShape::Sine:     return std::sin (2.0 * law::pi * phi);
            case LfoShape::Triangle: return phi < 0.25 ? 4.0 * phi : (phi < 0.75 ? 2.0 - 4.0 * phi : 4.0 * phi - 4.0);
            case LfoShape::Saw:      return 1.0 - 2.0 * phi;
            case LfoShape::Pulse:    return phi < 0.5 ? 1.0 : -1.0;
            case LfoShape::SampleHold: return sampleHoldValue;
        }
        return 0.0;
    }

private:
    LfoShape shape = LfoShape::Sine;
    long long lastCycle = 0;
    double held = 0.0;
    Random random;
};

// §6.2 step sequencer. Steps 1..last loop; position in steps; swing delays every second step of the
// global step grid by swing x one step (the pair keeps its total length 2 steps).
class StepSequencer
{
public:
    Clock clock;

    void reset() noexcept { clock.reset(); }

    // Step index (0-based) at the current position.
    static int stepIndexAt (double pos, int lastStep, double swingPercent) noexcept
    {
        const double sw = clampd (swingPercent, 0.0, 75.0) / 100.0;
        pos += 1.0e-9; // robust against accumulated rounding of the position
        const double pair = std::floor (pos / 2.0);
        const double within = pos - 2.0 * pair;
        const long long count = static_cast<long long> (pair) * 2 + (within < 1.0 + sw ? 0 : 1);
        const int last = std::max (1, std::min (16, lastStep));
        long long idx = count % last;
        if (idx < 0)
            idx += last;
        return int (idx);
    }

    int currentStep (int lastStep, double swingPercent) const noexcept { return stepIndexAt (clock.pos, lastStep, swingPercent); }
    void advance() noexcept { clock.advance(); }
};

// §6.3 envelope follower: fast peak detector (instant-ish attack 0.3 ms, release 20 ms).
struct EnvelopeFollower
{
    double env = 0.0, attackCoef = 0.0, releaseCoef = 0.0;
    void prepare (double fs) noexcept
    {
        attackCoef = std::exp (-1.0 / (0.0003 * fs));
        releaseCoef = std::exp (-1.0 / (0.020 * fs));
    }
    void reset() noexcept { env = 0.0; }
    double process (double x) noexcept
    {
        const double a = std::abs (x);
        env = a > env ? a + attackCoef * (env - a) : a + releaseCoef * (env - a);
        return env;
    }
};

} // namespace craft::mf
