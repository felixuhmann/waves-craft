// Craft Vocal Compressor - gate level detector and gain (spec r1 section 4.2, decision D5).
// Host-independent: no JUCE dependency.
#pragma once

#include <algorithm>
#include <cmath>

namespace craft::vocal
{

// Downward expander 1:2, hard knee:
//   d[n]  = max(A[n], decay(d[n-1]))      A = forward max over the look-ahead window (computed by the engine)
//   gg[n] = min(1, d[n] / T)
// decay: 63 dB/s (exponential in amplitude). Over the last `softFloorDb` above the current level A the decay rate is
// reduced in proportion to the remaining distance (an exponential approach in dB), which rounds the corner where the
// closing gate meets its final value as in the closing trajectories of section 9.6 (see IMPLEMENTATION_NOTES.md).
// Steady signals are unaffected: with constant A the detector equals A exactly.
class GateDetector
{
public:
    static constexpr double releaseDbPerSecond = 63.0;
    static constexpr double softFloorDb = 5.0;

    void prepare (double sampleRate) noexcept
    {
        decayDbPerSample = releaseDbPerSecond / sampleRate;
        fullDecay = std::pow (10.0, -decayDbPerSample / 20.0);
        softRatio = std::pow (10.0, softFloorDb / 20.0);
        reset();
    }

    void reset() noexcept { d = 0.0; }

    // Feeds the forward-max level A[n] and returns the detector value d[n].
    double process (double A) noexcept
    {
        double decayed;
        if (A > 0.0 && d < A * softRatio)
        {
            // within the soft floor: rate proportional to the remaining distance in dB
            const double distDb = 20.0 * std::log10 (std::max (d, A) / A);
            decayed = d * std::pow (10.0, -decayDbPerSample * (distDb / softFloorDb) / 20.0);
        }
        else
        {
            decayed = d * fullDecay;
        }
        d = std::max (A, decayed);
        return d;
    }

    double getLevel() const noexcept { return d; }

    // Gain for threshold T (linear amplitude, > 0).
    static double gain (double detector, double thresholdLin) noexcept
    {
        return detector >= thresholdLin ? 1.0 : detector / thresholdLin;
    }

private:
    double decayDbPerSample = 63.0 / 48000.0, fullDecay = 1.0, softRatio = 2.0;
    double d = 0.0;
};

} // namespace craft::vocal
