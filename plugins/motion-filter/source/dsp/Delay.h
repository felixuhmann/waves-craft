// Craft Motion Filter - one channel of the delay (spec r1 section 8).
#pragma once

#include <cmath>

#include "Blocks.h"

namespace craft::mf
{

// Delay line whose read position follows a target time. Digital mode: a new time is reached by a 20 ms
// crossfade between the old and the new tap. Analog mode: the read position glides (one-pole, 100 ms),
// which bends the pitch of the repeats like a tape/BBD delay.
class DelayChannel
{
public:
    static constexpr double maxSeconds = 8.5;

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        line.allocate (int (std::ceil (maxSeconds * fs)) + 8);
        glide = std::exp (-1.0 / (0.100 * fs));
        fadeStep = 1.0 / (0.020 * fs);
        reset();
    }

    void reset() noexcept
    {
        line.clear();
        initialised = false;
        fading = false;
        fade = 0.0;
    }

    void setAnalog (bool a) noexcept { analog = a; }
    double maxDelaySamples() const noexcept { return line.maxDelay(); }

    // Target delay in samples (clamped to the buffer).
    void setTarget (double samples) noexcept
    {
        target = clampd (samples, 3.0, line.maxDelay());
        if (! initialised)
        {
            current = next = target;
            initialised = true;
        }
    }

    // Tap output for this sample (call before write()).
    double read() noexcept
    {
        if (analog)
        {
            if (fading) // finish a digital crossfade instantly when switching mode
            {
                current = next;
                fading = false;
            }
            current = target + glide * (current - target);
            return line.read (current);
        }

        if (! fading && std::abs (target - current) > 0.5)
        {
            next = target;
            fading = true;
            fade = 0.0;
        }
        if (! fading)
        {
            current = target;
            return line.read (current);
        }
        const double y = (1.0 - fade) * line.read (current) + fade * line.read (next);
        fade += fadeStep;
        if (fade >= 1.0)
        {
            current = next;
            fading = false;
        }
        return y;
    }

    void write (double x) noexcept { line.write (x); }

    double currentDelay() const noexcept { return current; }

private:
    DelayBuffer line;
    double fs = 48000.0, glide = 0.0, fadeStep = 0.0;
    double target = 3.0, current = 3.0, next = 3.0, fade = 0.0;
    bool analog = false, fading = false, initialised = false;
};

} // namespace craft::mf
