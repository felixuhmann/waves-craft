// Spec r1 section 3: crossover model, band responses, exact allpass sum, zero latency.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "TestHelpers.h"
#include "dsp/Crossover.h"

using namespace craft::multiband;
using Catch::Approx;

namespace
{
std::array<CrossoverSection, 3> sections (double f1, double f2, double f3, double q, double fs)
{
    return { CrossoverSection::design (f1, q, fs), CrossoverSection::design (f2, q, fs), CrossoverSection::design (f3, q, fs) };
}

double bandDb (const std::array<CrossoverSection, 3>& s, int band, double f, double fs)
{
    return 20.0 * std::log10 (std::abs (bandResponses (s, f, fs)[size_t (band)]));
}

// Analog-prototype evaluation straight from the spec formulas (s = j tan(pi f/fs) / tan(pi fk/fs)).
std::array<std::complex<double>, 4> specModel (const std::array<double, 3>& fk, double q, double f, double fs)
{
    const double w0 = crossoverW0 (q);
    std::complex<double> lp[3], ap[3], hp[3];
    for (int k = 0; k < 3; ++k)
    {
        const std::complex<double> s (0.0, std::tan (pi * f / fs) / std::tan (pi * fk[size_t (k)] / fs));
        const auto d = s * s + s * w0 / q + w0 * w0;
        const auto l = w0 * w0 / d;
        lp[k] = l * l;
        ap[k] = (s * s - s * w0 / q + w0 * w0) / d;
        hp[k] = ap[k] - lp[k];
    }
    return { lp[0] * ap[1] * ap[2], hp[0] * lp[1] * ap[2], hp[0] * hp[1] * lp[2], hp[0] * hp[1] * hp[2] };
}
} // namespace

TEST_CASE ("w0(Q) puts each second-order section at -3.01 dB at the crossover", "[crossover]")
{
    for (double q : { 0.1, 0.3, 0.5, 0.6, 0.707, 0.75 })
    {
        const double w0 = crossoverW0 (q);
        const std::complex<double> s (0.0, 1.0);
        const double mag = std::abs (w0 * w0 / (s * s + s * w0 / q + w0 * w0));
        CHECK (mag == Approx (1.0 / std::sqrt (2.0)).margin (1e-12));
    }
    CHECK (crossoverW0 (std::sqrt (0.5)) == Approx (1.0).margin (1e-12)); // Butterworth
}

TEST_CASE ("Band responses match the spec examples (48 kHz, 92/4000/11071 Hz)", "[crossover]")
{
    const double fs = 48000.0;
    const auto s = sections (92, 4000, 11071, 0.6, fs);
    CHECK (bandDb (s, 0, 92, fs) == Approx (-6.02).margin (0.01));
    // Deep stop-band examples: the spec lists them with its measured model agreement (complex error <= -51 dB re
    // unity); the exact model gives -76.43 / -41.65 dB, i.e. within -100 / -66 dB re unity of the listed values.
    auto lin = [] (double db) { return std::pow (10.0, db / 20.0); };
    CHECK (std::abs (lin (bandDb (s, 0, 1000, fs)) - lin (-76.7)) < lin (-51.0));
    CHECK (bandDb (s, 0, 1000, fs) == Approx (-76.7).margin (0.5));
    CHECK (bandDb (s, 1, 1000, fs) == Approx (-0.29).margin (0.01));
    CHECK (bandDb (s, 1, 4000, fs) == Approx (-6.02).margin (0.01));
    CHECK (bandDb (s, 2, 6000, fs) == Approx (-3.09).margin (0.01));
    CHECK (bandDb (s, 3, 15000, fs) == Approx (-1.51).margin (0.01));

    const auto s01 = sections (92, 4000, 11071, 0.1, fs);
    CHECK (std::abs (lin (bandDb (s01, 0, 1000, fs)) - lin (-42.2)) < lin (-51.0));
    CHECK (bandDb (s01, 0, 1000, fs) == Approx (-42.2).margin (0.6));
    const auto s075 = sections (92, 4000, 11071, 0.75, fs);
    CHECK (bandDb (s075, 0, 20, fs) == Approx (0.08).margin (0.01));
}

TEST_CASE ("Digital bands equal the spec's analog model with the pre-warped bilinear transform", "[crossover]")
{
    for (double fs : { 44100.0, 48000.0, 96000.0 })
        for (double q : { 0.1, 0.3, 0.6, 0.75 })
            for (const std::array<double, 3>& fk : { std::array<double, 3> { 92, 4000, 11071 }, std::array<double, 3> { 200, 1500, 6000 } })
            {
                const auto s = sections (fk[0], fk[1], fk[2], q, fs);
                for (double f = 10.0; f < 0.49 * fs; f *= 1.07)
                {
                    const auto a = bandResponses (s, f, fs);
                    const auto b = specModel (fk, q, f, fs);
                    for (int k = 0; k < 4; ++k)
                        CHECK (std::abs (a[size_t (k)] - b[size_t (k)]) < 1e-9);
                }
            }
}

TEST_CASE ("The four bands sum to a unit-magnitude allpass AP1 AP2 AP3", "[crossover]")
{
    for (double fs : { 44100.0, 48000.0, 96000.0 })
        for (double q : { 0.1, 0.45, 0.75 })
        {
            const auto s = sections (92, 4000, 11071, q, fs);
            for (double f = 5.0; f < 0.5 * fs; f *= 1.03)
            {
                const auto b = bandResponses (s, f, fs);
                const auto sum = b[0] + b[1] + b[2] + b[3];
                CHECK (std::abs (sum) == Approx (1.0).margin (1e-12));
                const auto ap = s[0].allpass (f, fs) * s[1].allpass (f, fs) * s[2].allpass (f, fs);
                CHECK (std::abs (sum - ap) < 1e-12);
            }
        }
}

TEST_CASE ("Time domain: splitter bands sum to the allpass cascade, the response starts at sample 0", "[crossover]")
{
    const double fs = 48000.0;
    const auto s = sections (92, 4000, 11071, 0.6, fs);
    FourBandSplitter sp;
    sp.setSections (s);
    Biquad a1, a2, a3;
    a1.c = s[0].ap;
    a2.c = s[1].ap;
    a3.c = s[2].ap;
    const auto x = test::whiteNoise (20000, 3);
    std::array<double, 4> b {};
    double maxErr = 0.0;
    for (size_t i = 0; i < x.size(); ++i)
    {
        sp.process (x[i], b);
        const double ref = a3.process (a2.process (a1.process (x[i])));
        maxErr = std::max (maxErr, std::abs (b[0] + b[1] + b[2] + b[3] - ref));
    }
    CHECK (maxErr < 1e-12);

    // zero latency: an impulse appears at the output immediately (minimum-phase allpass, b0 = product of sections)
    FourBandSplitter imp;
    imp.setSections (s);
    imp.process (1.0, b);
    const double first = b[0] + b[1] + b[2] + b[3];
    CHECK (std::abs (first) > 0.01);
    CHECK (first == Approx (s[0].ap.b0 * s[1].ap.b0 * s[2].ap.b0).margin (1e-15));
}

TEST_CASE ("Engine at neutral settings is the crossover allpass, with 0 samples latency", "[crossover][engine]")
{
    const double fs = 48000.0;
    Settings st = Settings::defaults();
    for (auto& b : st.bands)
        b.rangeDb = 0.0;
    MultibandEngine e;
    e.setSettings (st);
    e.prepare (fs, 2);
    auto x = test::whiteNoise (24000, 5, 0.3);
    auto out = test::runStereo (e, x, x);

    const auto s = sections (92, 4000, 11071, 0.6, fs);
    Biquad a1, a2, a3;
    a1.c = s[0].ap;
    a2.c = s[1].ap;
    a3.c = s[2].ap;
    double maxErr = 0.0;
    for (size_t i = 0; i < x.size(); ++i)
    {
        const double ref = a3.process (a2.process (a1.process (x[i])));
        maxErr = std::max ({ maxErr, std::abs (out.first[i] - ref), std::abs (out.second[i] - ref) });
    }
    CHECK (maxErr < 1e-12);
}

TEST_CASE ("Crossover glides are stable and click-free, also when crossed", "[crossover][engine]")
{
    const double fs = 48000.0;
    MultibandEngine e;
    Settings st = Settings::defaults();
    for (auto& b : st.bands)
        b.rangeDb = 0.0;
    e.setSettings (st);
    e.prepare (fs, 2);
    auto x = test::toneSegments (fs, 300.0, { { 1.0, -6.0 } });
    double maxStep = 0.0, maxAbs = 0.0, prev = 0.0;
    for (size_t i = 0; i < x.size(); i += 64)
    {
        if (i == 12000) { st.crossoverHz = { 5000, 300, 80 }; st.q = 0.1; e.setSettings (st); }   // crossed
        if (i == 30000) { st.crossoverHz = { 92, 4000, 11071 }; st.q = 0.75; e.setSettings (st); }
        std::vector<double> l (x.begin() + long (i), x.begin() + long (std::min (x.size(), i + 64))), r = l;
        double* io[2] = { l.data(), r.data() };
        e.process (io, int (l.size()));
        for (double v : l)
        {
            maxStep = std::max (maxStep, std::abs (v - prev));
            maxAbs = std::max (maxAbs, std::abs (v));
            prev = v;
        }
    }
    CHECK (std::isfinite (maxAbs));
    CHECK (maxAbs < 1.0);
    // a 300 Hz sine at 0.5 peak moves at most 0.5 * 2 pi 300 / 48000 = 0.02 per sample
    CHECK (maxStep < 0.05);
}
