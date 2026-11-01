// Craft Enhancer - analytic-signal envelope and instantaneous frequency of a band signal.
//
// A pair of all-pass chains with a 90 degree phase difference (public-domain design by Olli Niemitalo,
// "Hilbert transform / 90-degree phase difference IIR filter", 2 x 4 second-order sections in z^-2, ~0.7 degree
// error between ~20 Hz and 0.45 fs at 44.1/48 kHz) gives I and Q; |I + jQ| is the amplitude of a sinusoid without
// ripple and with almost no delay, so detectors can be fast without modulating the gain at the signal frequency.
// Above 60 kHz the sections run in z^-4 (z^-8 above 120 kHz) so that the low band edge stays at ~20 Hz; the images of
// that network only affect content above 24 kHz.
#pragma once

#include <array>
#include <cmath>

namespace craft::enhancer::dsp
{

class AnalyticFilter
{
public:
    void prepare (double sampleRate) noexcept
    {
        stride = sampleRate > 120000.0 ? 4 : (sampleRate > 60000.0 ? 2 : 1);
        reset();
    }

    void reset() noexcept
    {
        for (auto& s : sections)
            s = {};
        delay = {};
        pos = 0;
    }

    // Processes one sample; returns I (re) and Q (im).
    inline void process (double x, double& re, double& im) noexcept
    {
        // path 1 (delayed by `stride` samples), path 2
        double p1 = x, p2 = x;
        for (int k = 0; k < 4; ++k)
        {
            p1 = sections[(size_t) k].process (p1, c1[k], pos, stride);
            p2 = sections[(size_t) k + 4].process (p2, c2[k], pos, stride);
        }
        // delay path 1 by `stride` samples
        delay[(size_t) pos] = p1;
        re = delay[(size_t) ((pos + 8 - stride) % 8)];
        im = p2;
        pos = (pos + 1) % 8;
    }

private:
    // y[n] = c (x[n] + y[n-2s]) - x[n-2s], c = a^2
    struct Section
    {
        std::array<double, 8> xs {}, ys {};
        inline double process (double x, double c, int pos, int s) noexcept
        {
            const int back = (pos + 8 - 2 * s) % 8;
            const double y = c * (x + ys[(size_t) back]) - xs[(size_t) back];
            xs[(size_t) pos] = x;
            ys[(size_t) pos] = y;
            return y;
        }
    };

    static constexpr double c1[4] = { 0.6923878 * 0.6923878, 0.9360654322959 * 0.9360654322959, 0.9882295226860 * 0.9882295226860, 0.9987488452737 * 0.9987488452737 };
    static constexpr double c2[4] = { 0.4021921162426 * 0.4021921162426, 0.8561710882420 * 0.8561710882420, 0.9722909545651 * 0.9722909545651, 0.9952884791278 * 0.9952884791278 };

    std::array<Section, 8> sections {};
    std::array<double, 8> delay {};
    int pos = 0, stride = 1;
};

} // namespace craft::enhancer::dsp
