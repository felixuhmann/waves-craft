// Craft Enhancer - parameter smoothing (spec r1 decision D2: click-free within 20 ms).
#pragma once

#include <algorithm>
#include <cmath>

namespace craft::enhancer::dsp
{

// Time constant of every parameter smoother: 5 tau = 20 ms.
inline constexpr double smoothingTau = 0.004;

// First-order exponential smoother; snaps to the exact target once within `epsilon`
// so that settled values are bit-identical to their targets.
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

// Linear ramp (masks for solo / bypass); reaches the exact target after `seconds`.
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

inline double dbToGain (double dB) noexcept { return dB == 0.0 ? 1.0 : std::pow (10.0, dB / 20.0); }

} // namespace craft::enhancer::dsp
