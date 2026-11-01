// Requantisation and dither: spec r1 sections 6, 7.1 and the section 9 test vectors.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <map>

#include "TestHelpers.h"

using namespace mltest;
using Catch::Approx;

namespace
{
dsp::Settings format (int bits, dsp::Dither d, dsp::Shaping s)
{
    auto st = plainSettings();
    st.wordLength = bits;
    st.dither = d;
    st.shaping = s;
    return st;
}
} // namespace

TEST_CASE ("Without dither and shaping the requantiser rounds exactly to the word-length grid (spec 6, 9)")
{
    const int bits = GENERATE (24, 22, 20, 18, 16);
    const double lsb = std::ldexp (1.0, 1 - bits);
    // slow ramp over +-4 LSB, plus a few larger values; the step is irrational so no value is an exact tie
    std::vector<double> x;
    for (int i = 0; i < 20000; ++i)
        x.push_back ((-4.0 + 8.0 * double (i) / 19999.37) * lsb);
    for (int i = 0; i < 2000; ++i)
        x.push_back (0.8 * std::sin (0.001 * std::sqrt (3.0) * double (i)));
    const auto y = runMono (48000.0, format (bits, dsp::Dither::None, dsp::Shaping::None), x);
    for (size_t i = 64; i < y.size(); ++i)
    {
        const double v = x[i - 64];
        REQUIRE (y[i] == lsb * std::nearbyint (v / lsb));
    }
}

TEST_CASE ("Type 1 dither on digital silence: TPDF +-1 LSB, 0.50 LSB rms, P(+-1) = 12.5 % (spec 7.1, 9)")
{
    const int bits = 16;
    const double lsb = std::ldexp (1.0, 1 - bits);
    const auto y = runMono (48000.0, format (bits, dsp::Dither::Type1, dsp::Shaping::None), std::vector<double> (1 << 20, 0.0));
    std::map<int, size_t> hist;
    std::vector<double> e;
    for (size_t i = 64; i < y.size(); ++i)
    {
        const double q = y[i] / lsb;
        REQUIRE (q == std::round (q));
        ++hist[int (q)];
        e.push_back (q);
    }
    CHECK (hist.size() == 3);
    const double n = double (e.size());
    CHECK (double (hist[1]) / n == Approx (0.125).margin (0.003));
    CHECK (double (hist[-1]) / n == Approx (0.125).margin (0.003));
    CHECK (rms (e) == Approx (0.5).margin (0.01));

    // white: flat within +-0.2 dB (eight octave-ish bands)
    const auto psd = welchPsd (e, 4096);
    for (double f : { 200.0, 1000.0, 3000.0, 6000.0, 10000.0, 14000.0, 18000.0, 22000.0 })
        CHECK (10.0 * std::log10 (bandMean (psd, 48000.0, f, 1000.0) / 0.25) == Approx (0.0).margin (0.2));
}

TEST_CASE ("Type 1 dither: total error 0.50 LSB rms on a signal, independent per channel (spec 7.1)")
{
    const double fs = 48000.0, lsb = std::ldexp (1.0, -15);
    std::vector<std::vector<double>> x (2, std::vector<double> (1 << 19));
    for (size_t i = 0; i < x[0].size(); ++i)
        x[0][i] = x[1][i] = 0.001 * std::sin (2.0 * pi * 997.0 * double (i) / fs);
    dsp::MasteringEngine e;
    e.setSettings (format (16, dsp::Dither::Type1, dsp::Shaping::None));
    e.prepare (fs);
    const auto y = runEngine (e, x);
    std::vector<double> err[2];
    double cross = 0.0;
    for (size_t i = 64; i < y[0].size(); ++i)
    {
        for (int ch = 0; ch < 2; ++ch)
            err[ch].push_back ((y[size_t (ch)][i] - x[size_t (ch)][i - 64]) / lsb);
        cross += err[0].back() * err[1].back();
    }
    CHECK (rms (err[0]) == Approx (0.5).margin (0.05));
    CHECK (rms (err[1]) == Approx (0.5).margin (0.05));
    CHECK (std::abs (cross / double (err[0].size())) < 0.01); // channels are independent
}

TEST_CASE ("Type 2 dither without shaping: 0.41 LSB rms (spec 7.1, 7.2)")
{
    const double fs = 48000.0, lsb = std::ldexp (1.0, -15);
    std::vector<double> x (1 << 19);
    for (size_t i = 0; i < x.size(); ++i)
        x[i] = 0.001 * std::sin (2.0 * pi * 997.0 * double (i) / fs); // -60 dBFS
    const auto y = runMono (fs, format (16, dsp::Dither::Type2, dsp::Shaping::None), x);
    std::vector<double> err;
    for (size_t i = 64; i < y.size(); ++i)
        err.push_back ((y[i] - x[i - 64]) / lsb);
    CHECK (rms (err) == Approx (0.41).margin (0.05));
}

TEST_CASE ("Digital silence stays exactly zero without Type 1 dither, every shaping mode (spec 6, 7.1, D3)")
{
    const int shaping = GENERATE (0, 1, 2, 3);
    const int dither = GENERATE (0, 2);
    const double fs = GENERATE (44100.0, 48000.0, 96000.0);
    const int bits = GENERATE (16, 24);
    CAPTURE (shaping, dither, fs, bits);
    // a loud passage first, so the error-feedback history is non-zero when the silence starts
    std::vector<double> x (40000, 0.0);
    for (size_t i = 0; i < 20000; ++i)
        x[i] = 0.5 * std::sin (0.01 * double (i)) + 0.3;
    const int L = dsp::latencyConfigFor (fs).latency;
    const auto y = runMono (fs, format (bits, dsp::Dither (dither), dsp::Shaping (shaping)), x);
    for (size_t i = 20000 + size_t (L); i < y.size(); ++i)
        REQUIRE (y[i] == 0.0);
    // and silence from the start
    const auto z = runMono (fs, format (bits, dsp::Dither (dither), dsp::Shaping (shaping)), std::vector<double> (20000, 0.0));
    for (double v : z)
        REQUIRE (v == 0.0);
}

namespace
{
// Largest narrow-band peak of the total error (y - v) relative to its local spectral neighbourhood, in dB.
// Shaped noise is smooth (about 0 dB); a limit cycle / idle tone shows up as a peak tens of dB high.
double tonality (dsp::Dither dither, dsp::Shaping shaping, double dcLsb, double* rmsOut = nullptr, int n = 1 << 17)
{
    dsp::Requantiser rq;
    rq.prepare (48000.0, 1234u);
    rq.setFormat (16, dither, shaping);
    const double lsb = rq.getLsb();
    std::vector<double> err;
    for (int i = 0; i < n; ++i)
        err.push_back (rq.process (dcLsb * lsb) / lsb - dcLsb);
    double mean = 0.0;
    for (double v : err)
        mean += v / double (n);
    for (auto& v : err)
        v -= mean;
    if (rmsOut != nullptr)
        *rmsOut = rms (err);
    if (rms (err) < 1e-3)
        return -100.0; // constant output: no pattern at all
    const auto psd = welchPsd (err, 1024);
    double worst = -100.0;
    for (size_t k = 8; k + 8 < psd.size(); ++k)
    {
        double local = 0.0;
        int cnt = 0;
        for (size_t j = k - 8; j <= k + 8; ++j)
            if (j + 2 < k || j > k + 2)
                local += psd[j], ++cnt;
        local /= cnt;
        if (local > 1e-30)
            worst = std::max (worst, 10.0 * std::log10 (psd[k] / local));
        else if (psd[k] > 1e-20)
            worst = std::max (worst, 100.0);
    }
    return worst;
}
} // namespace

TEST_CASE ("Type 2 dither breaks limit cycles: no idle tones for constant input (D3)")
{
    const int shaping = GENERATE (1, 2, 3);
    const double dc = GENERATE (0.3, 0.5, 1.5, 7.25, -2.7, 100.125); // in LSB
    CAPTURE (shaping, dc);
    double r2 = 0.0, r0 = 0.0;
    const double t = tonality (dsp::Dither::Type2, dsp::Shaping (shaping), dc, &r2);
    const double t0 = tonality (dsp::Dither::None, dsp::Shaping (shaping), dc, &r0);
    UNSCOPED_INFO ("type 2 tonality " << t << " dB rms " << r2 << ", undithered " << t0 << " dB rms " << r0);
    CHECK (t < 6.0);
}

TEST_CASE ("Output is clipped at full scale (spec 6 implementer rule)")
{
    dsp::Requantiser rq;
    rq.prepare (48000.0, 1u);
    rq.setFormat (16, dsp::Dither::Type1, dsp::Shaping::Ultra);
    double peak = 0.0;
    for (int i = 0; i < 100000; ++i)
        peak = std::max (peak, std::abs (rq.process (i % 2 ? 1.0 : -1.0)));
    CHECK (peak <= 1.0);
}
