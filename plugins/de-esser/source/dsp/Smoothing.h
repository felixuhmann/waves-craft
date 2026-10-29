// Craft De-Esser - click-free parameter changes and mode crossfades (spec r1 decision D2).
#pragma once

#include <algorithm>
#include <cmath>

namespace craft::deesser::dsp
{

// Linear ramp from the current value to a target in a fixed time; lands exactly on the target.
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
    double rampSamples = 480.0, step = 0.0, current = 0.0, target = 0.0;
};

// Frequency glide along a logarithmic axis; while settled the exact target value is returned.
class LogGlide
{
public:
    void setRampTime (double seconds, double sampleRate) noexcept { ramp.setRampTime (seconds, sampleRate); }

    void setTarget (double hz) noexcept
    {
        if (hz == targetHz)
            return;
        targetHz = hz;
        ramp.setTarget (std::log (hz));
    }
    void snapToTarget() noexcept { ramp.snapToTarget(); }
    bool isSettled() const noexcept { return ramp.isSettled(); }
    double getTarget() const noexcept { return targetHz; }

    double next() noexcept
    {
        if (ramp.isSettled())
            return targetHz;
        const double v = ramp.next();
        return ramp.isSettled() ? targetHz : std::exp (v);
    }

private:
    LinearRamp ramp;
    double targetHz = 0.0;
};

} // namespace craft::deesser::dsp
