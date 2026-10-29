// Craft Level Rider - detector weighting (spec r1 section 4) and the analytic-power front end.
// Host-independent: no JUCE dependency.
#pragma once

#include <cmath>
#include <complex>

namespace craft::levelrider::dsp
{

inline constexpr double pi = 3.14159265358979323846;

// First-order IIR section y = b0 x + b1 x[n-1] - a1 y[n-1] (direct form I, double precision).
struct FirstOrder
{
    double b0 = 1.0, b1 = 0.0, a1 = 0.0;
    double x1 = 0.0, y1 = 0.0;

    double process (double x) noexcept
    {
        const double y = b0 * x + b1 * x1 - a1 * y1;
        x1 = x;
        y1 = y;
        return y;
    }
    void reset() noexcept { x1 = y1 = 0.0; }
    void flush() noexcept
    {
        if (std::abs (y1) < 1e-30) y1 = 0.0;
        if (std::abs (x1) < 1e-30) x1 = 0.0;
    }
    std::complex<double> response (double f, double fs) const noexcept
    {
        const auto z1 = std::polar (1.0, -2.0 * pi * f / fs);
        return (b0 + b1 * z1) / (1.0 + a1 * z1);
    }

    // Bilinear transform of (c1 s + c0) / (d1 s + d0), pre-warped at fw (Hz).
    static FirstOrder bilinear (double c1, double c0, double d1, double d0, double fw, double fs) noexcept
    {
        const double k = 2.0 * pi * fw / std::tan (pi * fw / fs);
        const double a0 = d0 + d1 * k;
        FirstOrder s;
        s.b0 = (c0 + c1 * k) / a0;
        s.b1 = (c0 - c1 * k) / a0;
        s.a1 = (d0 - d1 * k) / a0;
        return s;
    }
};

// Section 4 detector weighting: a first-order high-pass in series with a first-order high-frequency shelf
// (pole + zero), normalised to 0 dB at 1 kHz. The corners were fitted to table T6 (see IMPLEMENTATION_NOTES):
// the spec's starting point (HP 230 Hz + LP 8.5 kHz) is within about 1.2 dB, this fit within 0.3 dB at
// 44.1 / 48 / 96 kHz.
class Weighting
{
public:
    static constexpr double highPassHz = 246.39;
    static constexpr double shelfPoleHz = 4678.0;
    static constexpr double shelfZeroHz = 10068.0;

    void prepare (double fs) noexcept
    {
        const double wh = 2.0 * pi * highPassHz, wp = 2.0 * pi * shelfPoleHz, wz = 2.0 * pi * shelfZeroHz;
        hp = FirstOrder::bilinear (1.0, 0.0, 1.0, wh, highPassHz, fs);        // s / (s + wh)
        shelf = FirstOrder::bilinear (1.0 / wz, 1.0, 1.0 / wp, 1.0, shelfPoleHz, fs); // (1 + s/wz) / (1 + s/wp)
        const double g1k = std::abs (hp.response (1000.0, fs) * shelf.response (1000.0, fs));
        shelf.b0 /= g1k;
        shelf.b1 /= g1k;
        reset();
    }
    void reset() noexcept
    {
        hp.reset();
        shelf.reset();
    }
    void flush() noexcept
    {
        hp.flush();
        shelf.flush();
    }
    double process (double x) noexcept { return shelf.process (hp.process (x)); }

    // Magnitude response relative to 1 kHz (dB) at the prepared rate.
    double responseDb (double f, double fs) const noexcept
    {
        return 20.0 * std::log10 (std::abs (hp.response (f, fs) * shelf.response (f, fs)));
    }

private:
    FirstOrder hp, shelf;
};

// Pair of all-pass chains whose outputs are 90 degrees apart (within +-0.7 degrees from 0.0005 fs to
// 0.4995 fs). Public-domain coefficient set by Olli Niemitalo ("Hilbert transform IIR", yehar.com).
// Their outputs form an analytic signal, so |a|^2 + |b|^2 of a steady sine is constant: the detector
// reads a steady sine at its (weighted) RMS level at any frequency without ripple (section 4).
class QuadraturePair
{
public:
    void reset() noexcept
    {
        for (auto& s : sections)
            s = {};
        delayed = 0.0;
    }

    // Returns the mean power (a^2 + b^2) / 2 of the analytic signal at this sample.
    double power (double x) noexcept
    {
        double a = x, b = x;
        for (int i = 0; i < 4; ++i)
            a = sections[i].process (coeffA[i], a);
        for (int i = 0; i < 4; ++i)
            b = sections[4 + i].process (coeffB[i], b);
        const double aDelayed = delayed; // path A carries one extra sample of delay
        delayed = a;
        return 0.5 * (aDelayed * aDelayed + b * b);
    }

    void flush() noexcept
    {
        for (auto& s : sections)
            s.flush();
        if (std::abs (delayed) < 1e-30)
            delayed = 0.0;
    }

private:
    // y[n] = c (x[n] + y[n-2]) - x[n-2]   (H = (c - z^-2) / (1 - c z^-2))
    struct Section
    {
        double x1 = 0, x2 = 0, y1 = 0, y2 = 0;
        double process (double c, double x) noexcept
        {
            const double y = c * (x + y2) - x2;
            x2 = x1;
            x1 = x;
            y2 = y1;
            y1 = y;
            return y;
        }
        void flush() noexcept
        {
            for (double* v : { &x1, &x2, &y1, &y2 })
                if (std::abs (*v) < 1e-30)
                    *v = 0.0;
        }
    };

    // published values are the square roots of the section coefficients
    static constexpr double coeffA[4] = { 0.6923878 * 0.6923878, 0.9360654322959 * 0.9360654322959,
                                          0.9882295226860 * 0.9882295226860, 0.9987488452737 * 0.9987488452737 };
    static constexpr double coeffB[4] = { 0.4021921162426 * 0.4021921162426, 0.8561710882420 * 0.8561710882420,
                                          0.9722909545651 * 0.9722909545651, 0.9952884791278 * 0.9952884791278 };

    Section sections[8];
    double delayed = 0.0;
};

// Weighted analytic power of one detector input sample (section 4 + section 6 input).
class WeightedPower
{
public:
    void prepare (double fs) noexcept
    {
        weighting.prepare (fs);
        quad.reset();
    }
    void reset() noexcept
    {
        weighting.reset();
        quad.reset();
    }
    void flush() noexcept
    {
        weighting.flush();
        quad.flush();
    }
    double process (double x) noexcept { return quad.power (weighting.process (x)); }
    const Weighting& getWeighting() const noexcept { return weighting; }

private:
    Weighting weighting;
    QuadraturePair quad;
};

} // namespace craft::levelrider::dsp
