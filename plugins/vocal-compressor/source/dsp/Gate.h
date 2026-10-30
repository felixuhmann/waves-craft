// Craft Vocal Compressor - gate level detector and gain (spec r2 section 4.2, decision D5).
// Host-independent: no JUCE dependency.
#pragma once

#include <algorithm>
#include <cmath>

namespace craft::vocal
{

// Downward expander 1:2, hard knee, detector in dB (spec r2 section 4.2, implemented literally):
//   L_A = 20 log10(A[n])                  A = forward max over the look-ahead window (computed by the engine)
//   D   = L_d[n-1] - L_A
//   D <= 0 : L_d[n] = L_A                                  instant attack
//   D >  0 : L_d[n] = L_A + max(0, D - S(D) / fs)          release toward the current level
//   S(D) = 21.57 + 44.19 (1 - exp(-D / 8.03)) dB/s
//   gg[n] = min(1, 10^((L_d[n] - Gate) / 20))
class GateDetector
{
public:
    static constexpr double rateBase = 21.57;  // dB/s just above the level
    static constexpr double rateRange = 44.19; // + this far above it (total 65.76 dB/s)
    static constexpr double rateDistance = 8.03; // dB
    static constexpr double floorDb = -300.0;  // level used for digital silence

    void prepare (double sampleRate) noexcept
    {
        fs = sampleRate;
        reset();
    }

    void reset() noexcept { levelDb = floorDb; }

    // Release rate in dB/s at a distance of D dB above the current level.
    static double rate (double distanceDb) noexcept
    {
        return rateBase + rateRange * (1.0 - std::exp (-distanceDb / rateDistance));
    }

    // Feeds the forward-max level A[n] (linear) and returns the detector level L_d[n] in dB.
    double process (double A) noexcept
    {
        const double la = A > 1.0e-15 ? 20.0 * std::log10 (A) : floorDb;
        const double d = levelDb - la;
        levelDb = d <= 0.0 ? la : la + std::max (0.0, d - rate (d) / fs);
        return levelDb;
    }

    double getLevelDb() const noexcept { return levelDb; }

    // Gain for threshold `thresholdDb` given the detector level in dB.
    static double gain (double detectorDb, double thresholdDb) noexcept
    {
        return detectorDb >= thresholdDb ? 1.0 : std::pow (10.0, (detectorDb - thresholdDb) / 20.0);
    }

private:
    double fs = 48000.0;
    double levelDb = floorDb;
};

} // namespace craft::vocal
