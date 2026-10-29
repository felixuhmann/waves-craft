// Craft Console E - parameter smoothing (spec r1 decision D5: click-free, <= 20 ms).
#pragma once

#include <algorithm>
#include <cmath>

namespace craft::consolee::dsp
{

// Linear ramp to a target in a fixed time; lands exactly on the target.
class LinearRamp
{
public:
    void setRampTime (double seconds, double sampleRate) noexcept { rampSamples = std::max (1.0, seconds * sampleRate); }

    void setTarget (double t) noexcept
    {
        if (t == target)
            return;
        target = t;
        step = std::abs (target - current) / rampSamples;
    }
    void snap (double v) noexcept { current = target = v; }
    double getCurrent() const noexcept { return current; }
    double getTarget() const noexcept { return target; }
    bool isSettled() const noexcept { return current == target; }

    double next() noexcept
    {
        if (current < target)
            current = std::min (target, current + step);
        else if (current > target)
            current = std::max (target, current - step);
        return current;
    }

    // advance n samples at once
    double skip (int n) noexcept
    {
        if (current < target)
            current = std::min (target, current + step * n);
        else if (current > target)
            current = std::max (target, current - step * n);
        return current;
    }

private:
    double rampSamples = 480.0, step = 0.0, current = 0.0, target = 0.0;
};

} // namespace craft::consolee::dsp
