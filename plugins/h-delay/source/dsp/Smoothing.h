// Craft Hybrid Delay - parameter smoothing helpers.
#pragma once

#include <algorithm>
#include <cmath>

namespace craft::hdelay
{

// First-order exponential smoother that snaps to the exact target once within `epsilon`,
// so settled values are bit-identical to their targets.
class OnePole
{
public:
    void setTimeConstant (double seconds, double sampleRate) noexcept { coeff = std::exp (-1.0 / (seconds * sampleRate)); }
    void setEpsilon (double e) noexcept { epsilon = e; }
    void setTarget (double t) noexcept { target = t; }
    void snap() noexcept { current = target; }
    void snapTo (double v) noexcept { target = current = v; }
    double getTarget() const noexcept { return target; }
    double getCurrent() const noexcept { return current; }
    bool isSettled() const noexcept { return current == target; }

    double next() noexcept
    {
        if (current != target)
        {
            current = target + coeff * (current - target);
            if (std::abs (current - target) <= epsilon)
                current = target;
        }
        return current;
    }

private:
    double coeff = 0.0, current = 0.0, target = 0.0, epsilon = 1.0e-9;
};

// Linear ramp (crossfades).
class LinearRamp
{
public:
    void setRampTime (double seconds, double sampleRate) noexcept { step = 1.0 / std::max (1.0, seconds * sampleRate); }
    void setTarget (double t) noexcept { target = t; }
    void snapTo (double v) noexcept { target = current = v; }
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

private:
    double step = 0.001, current = 0.0, target = 0.0;
};

// Linear ramp that reaches any new target in a fixed number of samples (exact at the end).
class TimedRamp
{
public:
    void setRampTime (double seconds, double sampleRate) noexcept { length = std::max (1, int (seconds * sampleRate)); }
    void setTarget (double t) noexcept
    {
        if (t == target)
            return;
        target = t;
        remaining = length;
        step = (target - current) / double (length);
    }
    void snapTo (double v) noexcept { target = current = v; remaining = 0; }
    double getCurrent() const noexcept { return current; }
    double getTarget() const noexcept { return target; }
    bool isSettled() const noexcept { return remaining == 0; }

    double next() noexcept
    {
        if (remaining > 0)
            current = (--remaining == 0) ? target : current + step;
        return current;
    }

private:
    double current = 0.0, target = 0.0, step = 0.0;
    int length = 1, remaining = 0;
};

} // namespace craft::hdelay
