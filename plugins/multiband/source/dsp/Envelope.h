// Craft Multiband - instantaneous amplitude envelope (sine-peak level) of a band signal.
// Host-independent: no JUCE dependency.
//
// A pair of allpass chains whose outputs differ by 90 degrees over most of the audio band (an IIR Hilbert
// transformer, "phase-difference network"; the public 2 x 4 section design by O. Niemitalo) gives the analytic
// signal; its magnitude is the instantaneous amplitude, i.e. a steady sine reads as its peak value without the
// ripple of a rectifier. Only the detector uses it - the audio path never sees this filter.
#pragma once

#include <array>
#include <cmath>

namespace craft::multiband
{

class HilbertEnvelope
{
public:
    void reset() noexcept
    {
        for (auto& s : stateA) s = {};
        for (auto& s : stateB) s = {};
        delayed = 0.0;
    }

    // Squared instantaneous amplitude of x.
    double processSquared (double x) noexcept
    {
        double a = x;
        for (size_t i = 0; i < 4; ++i)
            a = section (stateA[i], coeffA[i], a);
        double b = x;
        for (size_t i = 0; i < 4; ++i)
            b = section (stateB[i], coeffB[i], b);
        const double re = delayed; // path A is delayed by one sample
        delayed = a;
        return re * re + b * b;
    }

private:
    struct State
    {
        double x1 = 0.0, x2 = 0.0, y1 = 0.0, y2 = 0.0;
    };

    // Second-order allpass in z^-2: y[n] = c (x[n] + y[n-2]) - x[n-2].
    static double section (State& s, double c, double x) noexcept
    {
        const double y = c * (x + s.y2) - s.x2;
        s.x2 = s.x1;
        s.x1 = x;
        s.y2 = s.y1;
        s.y1 = y;
        return y;
    }

    static constexpr std::array<double, 4> coeffA { 0.47940086558884, 0.87621849353931, 0.976597589508199, 0.997499255935944 };
    static constexpr std::array<double, 4> coeffB { 0.161758498367701, 0.733028932341480, 0.945349700329112, 0.990599156684529 };

    std::array<State, 4> stateA {}, stateB {};
    double delayed = 0.0;
};

} // namespace craft::multiband
