// Craft Dynamics - parameter smoothing and crossfades (spec r1 section 8). Host-independent.
#pragma once

#include <algorithm>
#include <cmath>

namespace craft::dynamics::dsp
{

// First-order exponential smoother; snaps to the exact target once within `epsilon`, so settled values are
// bit-identical to their targets (needed for the bit-exact neutral path, spec 3 / D2).
class OnePoleSmoother
{
public:
    void setTimeConstant (double seconds, double sampleRate) noexcept { coeff = std::exp (-1.0 / (seconds * sampleRate)); }
    void setEpsilon (double e) noexcept { epsilon = e; }
    void setTarget (double t) noexcept { target = t; }
    void snapToTarget() noexcept { current = target; }
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

// Linear ramp between 0 and 1 (crossfades of switched settings).
class LinearRamp
{
public:
    void setRampTime (double seconds, double sampleRate) noexcept { step = 1.0 / std::max (1.0, seconds * sampleRate); }
    void setTarget (double t) noexcept { target = t; }
    void snapToTarget() noexcept { current = target; }
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

} // namespace craft::dynamics::dsp
