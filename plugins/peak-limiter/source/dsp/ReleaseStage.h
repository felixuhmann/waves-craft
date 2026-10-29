// Craft Peak Limiter - release stage (spec r1 section 4 step 4 and section 5).
// Host-independent: no JUCE dependency.
#pragma once

#include <algorithm>
#include <cmath>

namespace craft::limiter
{

// Constants of the program-dependent release (spec section 5 leaves the law open; these were fitted to the
// section 5 table, see IMPLEMENTATION_NOTES.md).
struct AutoReleaseLaw
{
    // Fast branch: recovers linearly in dB at this many dB per Release time.
    static constexpr double fastDbPerRelease = 117.0;
    // Slow branch ("overload memory"):
    //  - immediately holds this fraction of the linear-gain deficit of the target,
    static constexpr double instantFraction = 0.227;
    //  - charges its linear-gain deficit toward the target with this time constant (ms, independent of Release),
    static constexpr double chargeMs = 238.0;
    //  - recovers at slowDbPerRelease dB per Release time plus slowExpPerRelease * (its GR in dB) per Release time.
    static constexpr double slowDbPerRelease = 8.35;
    static constexpr double slowExpPerRelease = 0.16;
};

// Turns the look-ahead hold target h[n] (linear gain, <= 1) into the release state q[n].
//  - Fixed release: q = h if h < q, else q = h - (h - q) c, c = exp(-ln10 / (Release * fs)).
//  - Auto release: two branches, q = min(fast, slow). Attack (h < q) is instantaneous in both modes.
class ReleaseStage
{
public:
    void prepare (double sampleRate) noexcept
    {
        fs = sampleRate;
        chargeCoeff = 1.0 - std::exp (-1.0 / (AutoReleaseLaw::chargeMs * 1.0e-3 * fs));
        updateCoefficients();
    }

    void reset() noexcept { q = fast = slow = 1.0; }

    void setReleaseMs (double ms) noexcept
    {
        if (ms != releaseMs)
        {
            releaseMs = ms;
            updateCoefficients();
        }
    }

    void setAutoRelease (bool on) noexcept
    {
        if (on && ! autoMode)
            fast = slow = q; // continue from the current state
        autoMode = on;
    }

    bool isAuto() const noexcept { return autoMode; }
    double getState() const noexcept { return q; }

    double process (double h) noexcept
    {
        if (! autoMode)
        {
            q = h < q ? h : h - (h - q) * c;
            return q;
        }

        // fast branch: instantaneous attack, linear-in-dB recovery toward the target
        fast = h < fast ? h : std::min (h, fast * fastStep);

        // slow branch
        if (h < slow)
        {
            slow = std::min (slow, 1.0 - AutoReleaseLaw::instantFraction * (1.0 - h));
            slow += (h - slow) * chargeCoeff;
        }
        else if (slow < h)
        {
            // GR in dB shrinks by slowDb/Release + slowExp * GR/Release per second
            slow = std::min (h, std::pow (slow, slowExponent) * slowStep);
        }

        q = std::min (fast, slow);
        return q;
    }

private:
    void updateCoefficients() noexcept
    {
        const double releaseSamples = std::max (1.0e-9, releaseMs * 1.0e-3 * fs);
        c = std::exp (-std::log (10.0) / releaseSamples);
        fastStep = std::pow (10.0, AutoReleaseLaw::fastDbPerRelease / (20.0 * releaseSamples));
        slowStep = std::pow (10.0, AutoReleaseLaw::slowDbPerRelease / (20.0 * releaseSamples));
        slowExponent = std::clamp (1.0 - AutoReleaseLaw::slowExpPerRelease / releaseSamples, 0.0, 1.0);
    }

    double fs = 48000.0, releaseMs = 1.0;
    double c = 0.0, fastStep = 1.0, slowStep = 1.0, slowExponent = 1.0, chargeCoeff = 0.0;
    double q = 1.0, fast = 1.0, slow = 1.0;
    bool autoMode = false;
};

} // namespace craft::limiter
