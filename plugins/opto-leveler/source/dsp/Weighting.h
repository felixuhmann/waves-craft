// Craft Opto Leveler - Hi Freq sidechain weighting (spec 5, tables T4/T5). Host independent.
//
// A cascade of peaking sections at fixed centre frequencies whose gains (dB) are tabulated at Hi Freq
// 0 / 25 / 50 / 75 / 100 and interpolated linearly in Hi Freq (spec 5). The gains are fitted by
// tools/fit_weighting.py so that the steady-state gain reduction of the complete detector + cell matches T4 / T5.
#pragma once

#include <algorithm>
#include <array>

#include "Filters.h"
#include "WeightingTable.h"

namespace craft::opto_leveler::dsp
{

class Weighting
{
public:
    static constexpr int numBands = weighting_table::numBands;
    static constexpr int numNodes = weighting_table::numNodes;

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        current = -1.0;
        setHiFreq (target);
        reset();
    }

    void reset()
    {
        for (auto& b : bands)
            b.reset();
    }

    // Hi Freq 0 ... 100; recomputes the coefficients when the value changes.
    void setHiFreq (double hiFreq)
    {
        target = std::clamp (hiFreq, 0.0, 100.0);
        if (target == current)
            return;
        current = target;
        for (int b = 0; b < numBands; ++b)
            bands[size_t (b)].setCoefficients (designBand (b, current, fs));
    }

    double process (double x) noexcept
    {
        for (auto& b : bands)
            x = b.process (x);
        return x * weighting_table::outputGain;
    }

    // Gain (dB) of band b at a Hi Freq setting (linear interpolation between the nodes).
    static double bandGainDb (int b, double hiFreq)
    {
        const double pos = std::clamp (hiFreq, 0.0, 100.0) / 100.0 * double (numNodes - 1);
        const int i = std::min (int (pos), numNodes - 2);
        const double t = pos - double (i);
        const auto& g = weighting_table::gainDb[size_t (b)];
        return g[size_t (i)] + t * (g[size_t (i + 1)] - g[size_t (i)]);
    }

    static Biquad designBand (int b, double hiFreq, double sampleRate)
    {
        const double f0 = std::min (weighting_table::centreHz[size_t (b)], 0.45 * sampleRate);
        return Biquad::peaking (f0, weighting_table::q[size_t (b)], bandGainDb (b, hiFreq), sampleRate);
    }

    // Magnitude (linear) at f for a Hi Freq setting (tests / display).
    static double magnitude (double f, double hiFreq, double sampleRate)
    {
        double m = weighting_table::outputGain;
        for (int b = 0; b < numBands; ++b)
            m *= designBand (b, hiFreq, sampleRate).magnitude (f, sampleRate);
        return m;
    }

    double getHiFreq() const noexcept { return current; }

private:
    double fs = 48000.0, target = 100.0, current = -1.0;
    std::array<Biquad, numBands> bands {};
};

} // namespace craft::opto_leveler::dsp
