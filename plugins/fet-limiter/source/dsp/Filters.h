// Craft FET Limiter - filters of the colouration stage (spec 7.1, 9). Host independent.
//
// Frequency response = coupling high-pass (2 Hz) x low-frequency bell x top-end low-pass sections. All are analog
// prototypes fitted to tables/freq_response.csv (tools/design_hf_filters.py documents the fit):
//   rev A: bell 49.33 Hz / -0.3565 dB / Q 0.4831; low-pass 25773.757 Hz Q 0.7243
//   rev B: bell 95.76 Hz / -0.1813 dB / Q 0.6263; low-pass 11954.034 Hz Q 0.6284 and 18254.918 Hz Q 1.49
// Spec 9 asks to keep the analog response at every sample rate. The low-frequency parts are far below Nyquist
// (bilinear transform is exact enough); the top-end sections use biquads least-squares fitted to the analog
// prototype for the standard sample rates (HfCoefficients.h) and a prewarped bilinear design otherwise.
#pragma once

#include <array>
#include <cmath>

#include "Tables.h"

namespace craft::fet_limiter::dsp
{

inline constexpr double pi = 3.14159265358979323846;

struct BiquadCoeffs
{
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
};

class Biquad
{
public:
    void setCoeffs (const BiquadCoeffs& c) noexcept { k = c; }
    void reset() noexcept { s1 = s2 = 0.0; }

    double process (double x) noexcept
    {
        const double y = k.b0 * x + s1;
        s1 = k.b1 * x - k.a1 * y + s2;
        s2 = k.b2 * x - k.a2 * y;
        return y;
    }

private:
    BiquadCoeffs k;
    double s1 = 0.0, s2 = 0.0;
};

// First-order high-pass (bilinear).
inline BiquadCoeffs highPass1 (double fc, double fs)
{
    const double k = std::tan (pi * fc / fs);
    const double n = 1.0 / (1.0 + k);
    return { n, -n, 0.0, (k - 1.0) * n, 0.0 };
}

// RBJ peaking equaliser (bilinear, prewarped at f0).
inline BiquadCoeffs peaking (double f0, double gainDb, double q, double fs)
{
    const double A = std::pow (10.0, gainDb / 40.0);
    const double w0 = 2.0 * pi * f0 / fs;
    const double alpha = std::sin (w0) / (2.0 * q);
    const double c = std::cos (w0);
    const double a0 = 1.0 + alpha / A;
    return { (1.0 + alpha * A) / a0, -2.0 * c / a0, (1.0 - alpha * A) / a0, -2.0 * c / a0, (1.0 - alpha / A) / a0 };
}

// Second-order low-pass (bilinear, prewarped at min(f0, 0.45 fs)).
inline BiquadCoeffs lowPass2 (double f0, double q, double fs)
{
    const double k = std::tan (pi * std::min (f0, 0.45 * fs) / fs);
    const double n = 1.0 / (1.0 + k / q + k * k);
    const double b0 = k * k * n;
    return { b0, 2.0 * b0, b0, 2.0 * (k * k - 1.0) * n, (1.0 - k / q + k * k) * n };
}

struct RevisionResponse
{
    double bellHz, bellDb, bellQ;
    int numLowPass;
    double lpHz[2], lpQ[2];
};

inline const RevisionResponse& revisionResponse (Revision rev)
{
    static const RevisionResponse a { 49.3271, -0.3565, 0.4831, 1, { 25773.757, 0.0 }, { 0.7243, 0.0 } };
    static const RevisionResponse b { 95.7612, -0.1813, 0.6263, 2, { 11954.0335, 18254.9179 }, { 0.6284, 1.49 } };
    return rev == Revision::A ? a : b;
}

// Top-end sections fitted to the analog prototypes at the standard sample rates (tools/design_hf_filters.py).
struct HfFit
{
    double fs;
    BiquadCoeffs sections[2];
};

inline const HfFit* findHfFit (Revision rev, double fs)
{
    static const HfFit fitA[] = {
        { 44100.0, { { 0.674888238375, 1.43629422258, 0.674883800057, 1.34105480103, 0.443078509321 } } },
        { 48000.0, { { 0.755904508361, 1.5585038757, 0.755902013103, 1.51742050477, 0.550440743356 } } },
        { 88200.0, { { -0.0144817732608, 0.726113954633, 0.237469578606, -0.128977689083, 0.0780796250479 } } },
        { 96000.0, { { -0.0118142335769, 0.679909269016, 0.218059899678, -0.210524014866, 0.0966790414444 } } },
        { 176400.0, { { -0.00347859614403, 0.345871475947, 0.103588905235, -0.835541347371, 0.281523133308 } } },
        { 192000.0, { { 0.0917679321984, 0.307688978528, -0.00297997537626, -0.915592722329, 0.31206965815 } } },
    };
    static const HfFit fitB[] = {
        { 44100.0, { { -0.174931989528, 0.85723861816, -0.174932006308, -0.460080650274, 0.069836847753 },
                     { 0.92569786486, 1.58557594913, 0.659879063903, 1.31131695477, 0.327658916742 } } },
        { 48000.0, { { 0.415351217462, 0.843851939758, 0.415343170869, -0.433033375963, -0.0423718888144 },
                     { -0.0265868235606, 1.47041715841, -0.769648557803, 1.07117861484, 0.0811734303405 } } },
        { 88200.0, { { -0.0066897829181, 0.364086676302, 0.0154080427931, -0.883530753985, 0.257904754329 },
                     { 0.317529085619, 0.601364248476, 0.0648938275174, -0.438124780913, 0.417788726067 } } },
        { 96000.0, { { -0.00512327307766, 0.324987062859, -0.00512384115616, -0.954908016436, 0.287839379374 },
                     { 0.170805740256, 0.580650982236, 0.170795270531, -0.5766066847, 0.448466890898 } } },
        { 176400.0, { { -0.00127564119021, 0.131602749856, -0.0012744032794, -1.37812242219, 0.50784343019 },
                      { 0.0590396744545, 0.214373068766, 0.0590491317728, -1.3156162925, 0.64636537663 } } },
        { 192000.0, { { -0.00107350618162, 0.114587888453, -0.001075225581, -1.42410395235, 0.53658666698 },
                      { 0.0505739644689, 0.184363624647, 0.0505655636999, -1.38430198224, 0.669694577194 } } },
    };
    const auto& table = rev == Revision::A ? fitA : fitB;
    for (const auto& f : table)
        if (std::abs (f.fs - fs) < 1.0)
            return &f;
    return nullptr;
}

// Analog prototype magnitude (dB) of a revision's response, for tests and the editor.
inline double analogResponseDb (Revision rev, double f)
{
    const auto& r = revisionResponse (rev);
    auto mag2 = [] (double re, double im) { return re * re + im * im; };
    const double w = f;
    // high-pass 2 Hz
    double m = mag2 (0.0, w) / mag2 (2.0, w);
    // bell
    const double A = std::pow (10.0, r.bellDb / 40.0), f0 = r.bellHz;
    m *= mag2 (f0 * f0 - w * w, w * (A / r.bellQ) * f0) / mag2 (f0 * f0 - w * w, w / (A * r.bellQ) * f0);
    for (int i = 0; i < r.numLowPass; ++i)
        m *= std::pow (r.lpHz[i], 4.0) / mag2 (r.lpHz[i] * r.lpHz[i] - w * w, w * r.lpHz[i] / r.lpQ[i]);
    return 10.0 * std::log10 (m);
}

// One channel of the response chain.
class ResponseFilter
{
public:
    void prepare (Revision rev, double fs)
    {
        setRevision (rev, fs);
        reset();
    }

    // Switches the coefficients and keeps the filter state (no reset, used when the Revision changes).
    void setRevision (Revision rev, double fs)
    {
        const auto& r = revisionResponse (rev);
        hp.setCoeffs (highPass1 (2.0, fs));
        bell.setCoeffs (peaking (r.bellHz, r.bellDb, r.bellQ, fs));
        for (int i = r.numLowPass; i < 2; ++i)
            lp[size_t (i)].reset();
        numLp = r.numLowPass;
        if (const auto* fit = findHfFit (rev, fs))
        {
            for (int i = 0; i < numLp; ++i)
                lp[size_t (i)].setCoeffs (fit->sections[i]);
        }
        else
        {
            for (int i = 0; i < numLp; ++i)
                lp[size_t (i)].setCoeffs (lowPass2 (r.lpHz[i], r.lpQ[i], fs));
        }
    }

    void reset() noexcept
    {
        hp.reset();
        bell.reset();
        for (auto& f : lp)
            f.reset();
    }

    double process (double x) noexcept
    {
        double y = bell.process (hp.process (x));
        for (int i = 0; i < numLp; ++i)
            y = lp[size_t (i)].process (y);
        return y;
    }

private:
    Biquad hp, bell;
    std::array<Biquad, 2> lp;
    int numLp = 1;
};

} // namespace craft::fet_limiter::dsp
