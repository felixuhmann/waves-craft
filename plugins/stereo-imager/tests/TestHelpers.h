#pragma once

#include <cmath>
#include <complex>
#include <random>
#include <vector>

#include "../source/dsp/ImagerEngine.h"
#include "../source/dsp/Parameters.h"

namespace testutil
{
using namespace craft::dsp;

inline ImagerEngine makeEngine (Variant v, const Settings& s, double fs)
{
    ImagerEngine e (v);
    e.prepare (fs);
    e.setSettings (s);
    e.reset();
    return e;
}

// Steady-state broadband matrix measured with unit impulses (one sample each).
inline Matrix2 measureMatrix (ImagerEngine& e)
{
    double l[2] = { 1.0, 0.0 }, r[2] = { 0.0, 0.0 };
    e.process (l, r, 1);
    const double a00 = l[0], a10 = r[0];
    e.reset();
    l[0] = 0.0;
    r[0] = 1.0;
    e.process (l, r, 1);
    return { a00, l[0], a10, r[0] };
}

// Impulse responses of the engine (double precision) for an impulse on input 1 or input 2.
struct ImpulseResponse
{
    std::vector<double> out1, out2;
};

inline ImpulseResponse impulseResponse (ImagerEngine& e, int input, int length)
{
    e.reset();
    ImpulseResponse ir { std::vector<double> (size_t (length), 0.0), std::vector<double> (size_t (length), 0.0) };
    (input == 0 ? ir.out1 : ir.out2)[0] = 1.0;
    e.process (ir.out1.data(), ir.out2.data(), length);
    return ir;
}

// DTFT of a (decayed) impulse response at an arbitrary frequency.
inline std::complex<double> dtft (const std::vector<double>& h, double f, double fs)
{
    const double w = 2.0 * pi * f / fs;
    std::complex<double> acc { 0.0, 0.0 };
    for (size_t n = 0; n < h.size(); ++n)
        acc += h[n] * std::polar (1.0, -w * double (n));
    return acc;
}

inline double magDb (std::complex<double> h) { return 20.0 * std::log10 (std::abs (h)); }
inline double phaseDeg (std::complex<double> h) { return std::arg (h) * 180.0 / pi; }

// M/S-domain responses of the engine: MS input mode, impulse on M (input 1) or S (input 2),
// read M = (L+R)/2 and S = (L-R)/2 at the output.
struct MsResponse
{
    std::complex<double> m, s;
};

inline MsResponse msResponseAt (Variant v, Settings st, double fs, double f, int length = 1 << 16)
{
    st.msInput = true;
    auto e = makeEngine (v, st, fs);
    auto irM = impulseResponse (e, 0, length);
    auto irS = impulseResponse (e, 1, length);
    std::vector<double> m (static_cast<size_t> (length)), s (static_cast<size_t> (length));
    for (size_t n = 0; n < size_t (length); ++n)
    {
        m[n] = 0.5 * (irM.out1[n] + irM.out2[n]);
        s[n] = 0.5 * (irS.out1[n] - irS.out2[n]);
    }
    return { dtft (m, f, fs), dtft (s, f, fs) };
}

inline std::vector<float> noise (int n, unsigned seed, float amplitude = 0.5f)
{
    std::mt19937 rng (seed);
    std::uniform_real_distribution<float> d (-amplitude, amplitude);
    std::vector<float> v (static_cast<size_t> (n));
    for (auto& x : v)
        x = d (rng);
    return v;
}

// Logarithmic sine sweep 20 Hz .. 0.45 fs.
inline std::vector<float> sweep (int n, double fs, float amplitude = 0.5f)
{
    std::vector<float> v (static_cast<size_t> (n));
    const double f0 = 20.0, f1 = 0.45 * fs, T = n / fs, k = std::log (f1 / f0);
    for (int i = 0; i < n; ++i)
    {
        const double t = i / fs;
        v[size_t (i)] = float (amplitude * std::sin (2.0 * pi * f0 * T / k * (std::exp (t / T * k) - 1.0)));
    }
    return v;
}

// ---------------------------------------------------------------------------------------------
// Independent straight-line reference model of spec sections 4 and 5, written directly from
// the step-by-step equations (it deliberately does not reuse the engine's matrix code).
struct ReferenceModel
{
    Settings p;
    Variant v;
    double fs;
    bool shuffle;
    // shuffler difference equation state, per M and S
    double bM[2], aM, bS[2], aS;
    double xm1 = 0, ym1 = 0, xs1 = 0, ys1 = 0;

    ReferenceModel (Settings s, Variant var, double sampleRate) : p (s), v (var), fs (sampleRate)
    {
        shuffle = var == Variant::Full && (std::lround (p.shuffle * 10) != 10 || std::lround (p.bassTrimDb * 10) != 0);
        const double T = std::pow (10.0, p.bassTrimDb / 20.0);
        const double GM = T * std::sqrt ((1.0 + 1.0 / (p.shuffle * p.shuffle)) / 2.0);
        const double GS = T * std::sqrt ((1.0 + p.shuffle * p.shuffle) / 2.0);
        design (GM, bM, aM);
        design (GS, bS, aS);
    }

    void design (double G, double* b, double& a)
    {
        // H(z) = [g k (1 + z^-1) - (1 - z^-1)] / [(k/g)(1 + z^-1) + (1 - z^-1)]
        const double k = std::tan (pi * p.frequencyHz / fs), g = std::sqrt (G);
        const double n0 = g * k - 1.0, n1 = g * k + 1.0, d0 = k / g + 1.0, d1 = k / g - 1.0;
        b[0] = n0 / d0;
        b[1] = n1 / d0;
        a = d1 / d0;
    }

    void process (double in1, double in2, double& out1, double& out2)
    {
        if (v == Variant::MsMatrix)
        {
            out1 = (in1 + in2) / 2;
            out2 = (in1 - in2) / 2;
            return;
        }
        double c1 = p.polarity1 ? -in1 : in1, c2 = p.polarity2 ? -in2 : in2;
        if (p.swap)
            std::swap (c1, c2);
        double M = p.msInput ? c1 : (c1 + c2) / 2, S = p.msInput ? c2 : (c1 - c2) / 2;
        const double w = p.width;
        if (w <= 1)
            S *= w;
        else
        {
            const double g = std::sqrt ((5 + 3 / (w * w)) / 8);
            M *= g;
            S *= g * w;
        }
        const double a = p.asymmetryDeg * pi / 180;
        M = M - std::sin (a) * S;
        S = std::cos (a) * S;
        const double f = p.rotationDeg * pi / 180;
        const double M2 = std::cos (f) * M + std::sin (f) * S;
        const double S2 = -std::sin (f) * M + std::cos (f) * S;
        M = M2;
        S = S2;
        if (shuffle)
        {
            const double ym = bM[0] * M + bM[1] * xm1 - aM * ym1;
            xm1 = M;
            ym1 = ym;
            const double ys = bS[0] * S + bS[1] * xs1 - aS * ys1;
            xs1 = S;
            ys1 = ys;
            M = ym;
            S = ys;
        }
        const double g = std::pow (10.0, p.gainDb / 20);
        M *= g;
        S *= g;
        out1 = M + S;
        out2 = M - S;
    }
};

} // namespace testutil
