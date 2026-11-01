// Craft Colour Delay - integer-sample feedback delay line (spec r1 section 3a).
//   w[n] = xi[n - D] + g * w[n - D]
// Delay-time changes crossfade between the old and the new read position (spec section 8).
#pragma once

#include <algorithm>
#include <vector>

#include "Smoothing.h"

namespace craft::cdl
{

class FeedbackDelay
{
public:
    static constexpr double crossfadeSeconds = 0.030;

    void prepare (double sampleRate, int maxDelaySamples)
    {
        buffer.assign ((size_t) maxDelaySamples + 2, 0.0);
        fade.setRampTime (crossfadeSeconds, sampleRate);
        reset();
    }

    void reset() noexcept
    {
        std::fill (buffer.begin(), buffer.end(), 0.0);
        writePos = 0;
        current = next = target;
        fade.reset (0.0);
    }

    // Target delay in samples (>= 1). Applied immediately by reset(), otherwise crossfaded.
    void setDelay (int samples) noexcept
    {
        target = std::clamp (samples, 1, (int) buffer.size() - 2);
    }
    void snapDelay() noexcept { current = next = target; fade.reset (0.0); }

    int getDelay() const noexcept { return current; }

    // One sample: returns w[n] (the delayed, fed-back signal) and stores xi[n] + g w[n].
    double process (double xi, double g) noexcept
    {
        if (fade.isSettled() && fade.getCurrent() == 0.0 && target != current)
        {
            next = target;
            fade.setTarget (1.0);
        }

        double w = read (current);
        if (! (fade.isSettled() && fade.getCurrent() == 0.0))
        {
            const double x = fade.next();
            w += x * (read (next) - w);
            if (x >= 1.0)
            {
                current = next;
                fade.reset (0.0);
            }
        }

        buffer[(size_t) writePos] = safetyClip (xi + g * w); // bounded loop (transparent below +24 dBFS)
        if (++writePos == (int) buffer.size())
            writePos = 0;
        return w;
    }

private:
    double read (int d) const noexcept
    {
        int i = writePos - d;
        if (i < 0)
            i += (int) buffer.size();
        return buffer[(size_t) i];
    }

    std::vector<double> buffer;
    int writePos = 0, current = 1, next = 1, target = 1;
    LinearRamp fade;
};

} // namespace craft::cdl
