// Craft Hybrid Delay - LoFi low-pass (spec r1 section 7).
// Fixed eighth-order low-pass whose response is defined in Hz, so it is the same at every sample rate.
// Four analog sections (two with transmission zeros) were fitted offline to the spec's table, starting
// from an inverse-Chebyshev prototype; each section is discretised by the bilinear transform with its
// resonance and notch frequencies pre-warped individually (see Biquad.h).
#pragma once

#include <array>

#include "Biquad.h"

namespace craft::hdelay
{

class LoFiFilter
{
public:
    struct Section
    {
        double f0, q, fz; // fz = 0: no transmission zero (all-pole section)
    };
    static constexpr std::array<Section, 4> sections { {
        { 6754.873, 2.73524, 9267.450 },
        { 8798.512, 2.00784, 10881.413 },
        { 5193.789, 1.42405, 0.0 },
        { 3795.928, 0.56892, 0.0 },
    } };

    void prepare (double sampleRate) noexcept
    {
        for (size_t i = 0; i < sections.size(); ++i)
            stages[i].setCoeffs (BiquadCoeffs::analogLowPassSection (sections[i].f0, sections[i].q, sections[i].fz, sampleRate));
        reset();
    }

    void reset() noexcept
    {
        for (auto& s : stages)
            s.reset();
    }

    double process (double x) noexcept
    {
        for (auto& s : stages)
            x = s.process (x);
        return x;
    }

    std::complex<double> response (double f, double fs) const noexcept
    {
        std::complex<double> h { 1.0, 0.0 };
        for (const auto& s : stages)
            h *= s.coeffs().response (f, fs);
        return h;
    }

private:
    std::array<Biquad, 4> stages;
};

} // namespace craft::hdelay
