// Craft Colour Delay - phaser colour (spec r1 section 7.4, [target]).
//
// Own design: four second-order all-pass sections in series (centre frequencies f0 * {1, 1.75, 3.0, 5.2}),
// with feedback, mixed 50/50 with the input -> four notches. A sine LFO at Phaser Rate sweeps f0 exponentially
// from 1.4 to 2.9 kHz, so the notches move across roughly 1.4 ... 15 kHz.
#pragma once

#include <array>
#include <cmath>

#include "Filters.h"

namespace craft::cdl
{

class Phaser
{
public:
    static constexpr int numSections = 4;
    static constexpr double spread[numSections] = { 1.0, 1.45, 2.1, 3.05 };
    static constexpr double sectionQ = 2.0, feedback = 0.25;
    static constexpr double sweepLowHz = 2300.0, sweepHighHz = 4300.0;

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        reset();
    }

    void reset() noexcept
    {
        for (auto& ch : sections)
            for (auto& s : ch)
                s.reset();
        fb[0] = fb[1] = 0.0;
        lfoPhase = 0.0;
        counter = 0;
        updateCoefficients();
    }

    void setRate (double hz) noexcept { rate = hz; }
    double getRate() const noexcept { return rate; }
    double getLfoPhase() const noexcept { return lfoPhase; }
    void setLfoPhase (double p) noexcept { lfoPhase = p - std::floor (p); updateCoefficients(); }

    double currentF0() const noexcept
    {
        const double m = 0.5 * (1.0 + std::sin (2.0 * pi * lfoPhase));
        return sweepLowHz * std::pow (sweepHighHz / sweepLowHz, m);
    }

    void process (double inLeft, double inRight, double& outLeft, double& outRight) noexcept
    {
        if ((counter++ & 7) == 0)
            updateCoefficients();
        lfoPhase += rate / fs;
        lfoPhase -= std::floor (lfoPhase);

        const double in[2] = { inLeft, inRight };
        double out[2];
        for (int c = 0; c < 2; ++c)
        {
            double v = in[c] + feedback * fb[c];
            for (auto& s : sections[(size_t) c])
                v = s.process (v);
            fb[c] = v;
            out[c] = 0.5 * (in[c] + v);
        }
        outLeft = out[0];
        outRight = out[1];
    }

private:
    void updateCoefficients() noexcept
    {
        const double f0 = currentF0();
        for (int i = 0; i < numSections; ++i)
        {
            const double f = std::min (f0 * spread[i], 0.45 * fs);
            sections[0][(size_t) i].setAllPass (f, sectionQ, fs);
            sections[1][(size_t) i].b0 = sections[0][(size_t) i].b0;
            sections[1][(size_t) i].b1 = sections[0][(size_t) i].b1;
            sections[1][(size_t) i].b2 = sections[0][(size_t) i].b2;
            sections[1][(size_t) i].a1 = sections[0][(size_t) i].a1;
            sections[1][(size_t) i].a2 = sections[0][(size_t) i].a2;
        }
    }

    double fs = 48000.0, rate = 0.34, lfoPhase = 0.0;
    double fb[2] {};
    unsigned counter = 0;
    std::array<std::array<Biquad, numSections>, 2> sections;
};

} // namespace craft::cdl
