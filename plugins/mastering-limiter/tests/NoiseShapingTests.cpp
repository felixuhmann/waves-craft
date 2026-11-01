// Noise shaping: spec r1 sections 7.2 and 7.3 (tolerances spec 10), measured through the whole engine.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "TestHelpers.h"

using namespace mltest;
using Catch::Approx;

namespace
{
// spec 7.3 tables (kHz, dB) - transcribed independently of tools/design_noise_shapers.py
const std::vector<double> f48 { 0.1, 0.5, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23 };
const std::vector<std::vector<double>> t48 {
    { 0.3, -1.7, -5.4, -10.1, -12.1, -12.6, -12.0, -10.7, -8.9, -7.5, -6.8, -6.1, -4.8, -2.2, 1.7, 6.4, 9.1, 9.8, 9.8, 9.8, 9.8, 9.7, 9.8, 9.9, 9.8 },
    { -2.5, -5.5, -9.7, -13.9, -16.2, -17.0, -16.8, -15.7, -14.0, -11.9, -10.0, -8.5, -6.9, -4.8, -1.5, 3.5, 10.3, 15.0, 16.4, 16.5, 16.5, 16.4, 16.4, 16.5, 16.4 },
    { -5.5, -8.6, -12.1, -16.3, -18.8, -19.6, -19.5, -18.6, -17.0, -15.1, -13.2, -11.4, -9.7, -7.6, -4.6, -0.2, 6.4, 16.1, 21.5, 23.0, 23.1, 22.9, 22.8, 23.0, 23.0 },
};
const std::vector<double> f44 { 0.1, 0.5, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22 };
const std::vector<std::vector<double>> t44 {
    { 2.0, -0.9, -5.1, -9.6, -11.5, -11.9, -11.0, -9.4, -7.7, -6.7, -6.2, -5.4, -3.8, -1.1, 2.8, 7.1, 9.9, 10.9, 10.7, 10.5, 10.6, 10.7, 10.7, 10.6 },
    { -1.0, -4.3, -7.8, -12.4, -14.3, -14.8, -14.1, -12.6, -10.9, -9.7, -9.0, -8.2, -6.8, -4.3, -0.6, 4.6, 10.9, 16.0, 17.7, 17.7, 17.6, 17.8, 17.7, 17.6 },
    { -2.2, -5.8, -9.7, -14.3, -16.5, -16.8, -16.1, -14.7, -13.0, -11.8, -11.2, -10.5, -9.1, -6.7, -3.1, 2.0, 8.8, 17.3, 22.8, 24.3, 24.2, 24.3, 24.4, 24.3 },
};
const std::vector<double> f96 { 0.1, 1, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30, 32, 34, 36, 38, 40, 42, 44, 46 };
const std::vector<std::vector<double>> t96 {
    { -22.6, -22.6, -22.2, -21.5, -20.1, -18.1, -15.2, -11.2, -5.8, 2.1, 8.2, 7.4, 7.2, 8.2, 8.4, 7.9, 7.8, 8.2, 8.4, 8.2, 7.9, 8.0, 8.3, 8.4, 8.2 },
    { -27.4, -27.6, -27.4, -26.7, -25.8, -24.0, -22.0, -19.3, -15.8, -11.4, -6.0, 1.3, 10.3, 15.1, 14.9, 14.0, 13.8, 14.5, 15.2, 15.3, 15.0, 15.3, 15.5, 14.9, 14.0 },
    { -31.0, -30.6, -30.5, -30.0, -28.9, -27.4, -25.3, -22.7, -19.4, -15.2, -9.5, -1.6, 13.9, 16.5, 12.1, 14.0, 17.7, 19.6, 19.6, 18.6, 17.6, 17.8, 18.9, 19.4, 19.2 },
};

struct Table
{
    const std::vector<double>* f;
    const std::vector<std::vector<double>>* db;
};

Table tableFor (double fs)
{
    if (fs < 46000.0)
        return { &f44, &t44 };
    if (fs < 66000.0)
        return { &f48, &t48 };
    return { &f96, &t96 }; // 88.2 and 96 kHz (88.2 uses the 96 kHz curves at the same frequencies)
}

// Total output error in LSB for a signal `x` (the requantiser input equals x: Threshold = Ceiling = 0).
std::vector<double> totalError (double fs, int bits, dsp::Dither d, dsp::Shaping s, const std::vector<double>& x)
{
    auto st = plainSettings();
    st.wordLength = bits;
    st.dither = d;
    st.shaping = s;
    const auto y = runMono (fs, st, x, 1024);
    const int L = dsp::latencyConfigFor (fs).latency;
    const double lsb = std::ldexp (1.0, 1 - bits);
    std::vector<double> e;
    e.reserve (y.size());
    for (size_t i = size_t (L) + 4096; i < y.size(); ++i)
        e.push_back ((y[i] - x[i - size_t (L)]) / lsb);
    return e;
}

std::vector<double> sine (double fs, double db, size_t n)
{
    std::vector<double> x (n);
    for (size_t i = 0; i < n; ++i)
        x[i] = dbToLin (db) * std::sin (2.0 * pi * 997.0 * double (i) / fs);
    return x;
}

double halfWidthFor (double fkHz) { return fkHz < 0.3 ? 40.0 : 150.0; }
} // namespace

TEST_CASE ("NTF magnitude per mode and sample rate, Type 1 on digital silence (spec 7.3, 10)")
{
    const double fs = GENERATE (44100.0, 48000.0, 88200.0, 96000.0);
    const int bits = GENERATE (16, 20);
    const size_t seg = fs > 66000.0 ? 8192 : 4096;
    const auto tab = tableFor (fs);
    for (int mode = 1; mode <= 3; ++mode)
    {
        const auto e = totalError (fs, bits, dsp::Dither::Type1, dsp::Shaping (mode), std::vector<double> (1 << 20, 0.0));
        const auto psd = welchPsd (e, seg);
        double inbandMeas = 0.0, inbandTable = 0.0;
        for (size_t i = 0; i < tab.f->size(); ++i)
        {
            const double fk = (*tab.f)[i];
            if (fk * 1000.0 >= fs / 2.0)
                continue;
            const double meas = 10.0 * std::log10 (bandMean (psd, fs, fk * 1000.0, halfWidthFor (fk)) / 0.25);
            const double want = (*tab.db)[size_t (mode - 1)][i];
            const double tol = (fs > 66000.0 && fk > 20.0) ? 2.5 : 1.5;
            CAPTURE (fs, bits, mode, fk, meas, want);
            CHECK (std::abs (meas - want) <= tol);
        }
        // integrated in-band (<= 15 kHz) noise power vs the table (dB interpolated linearly in frequency)
        if (fs < 66000.0 || fs == 96000.0)
        {
            const double binHz = fs / double (seg);
            for (size_t k = 1; double (k) * binHz <= 15000.0; ++k)
            {
                const double f = double (k) * binHz / 1000.0;
                size_t j = 0;
                while (j + 2 < tab.f->size() && (*tab.f)[j + 1] < f)
                    ++j;
                const double f0 = (*tab.f)[j], f1 = (*tab.f)[j + 1];
                const double d0 = (*tab.db)[size_t (mode - 1)][j], d1 = (*tab.db)[size_t (mode - 1)][j + 1];
                const double db = f < f0 ? d0 : d0 + (d1 - d0) * (f - f0) / (f1 - f0);
                inbandTable += std::pow (10.0, db / 10.0);
                inbandMeas += psd[k] / 0.25;
            }
            CAPTURE (fs, mode);
            CHECK (std::abs (10.0 * std::log10 (inbandMeas / inbandTable)) <= 1.0);
        }
    }
}

TEST_CASE ("Noise-shaping totals at 16 bit / 48 kHz (spec 7.2)")
{
    const double fs = 48000.0;
    const double type1[4] = { 0.50, 1.01, 1.98, 3.92 };
    const double type2[4] = { 0.41, 0.64, 1.19, 2.27 };
    const auto silence = std::vector<double> (1 << 19, 0.0);
    const auto sig = sine (fs, -60.0, 1 << 19);
    for (int mode = 0; mode <= 3; ++mode)
    {
        CAPTURE (mode);
        const double tol1 = mode == 0 ? 0.05 : 0.10 * type1[mode];
        const double tol2 = mode == 0 ? 0.05 : 0.10 * type2[mode];
        CHECK (rms (totalError (fs, 16, dsp::Dither::Type1, dsp::Shaping (mode), silence)) == Approx (type1[mode]).margin (tol1));
        CHECK (rms (totalError (fs, 16, dsp::Dither::Type1, dsp::Shaping (mode), sig)) == Approx (type1[mode]).margin (tol1));
        CHECK (rms (totalError (fs, 16, dsp::Dither::Type2, dsp::Shaping (mode), sig)) == Approx (type2[mode]).margin (tol2));
    }
}

TEST_CASE ("Type 2 shaped error has the same shape, 4.8 dB below Type 1 (spec 7.1)")
{
    const double fs = GENERATE (44100.0, 48000.0, 96000.0);
    const size_t seg = fs > 66000.0 ? 8192 : 4096;
    const auto sig = sine (fs, -60.0, 1 << 20);
    const auto tab = tableFor (fs);
    for (int mode = 1; mode <= 3; ++mode)
    {
        const auto p1 = welchPsd (totalError (fs, 16, dsp::Dither::Type1, dsp::Shaping (mode), sig), seg);
        const auto p2 = welchPsd (totalError (fs, 16, dsp::Dither::Type2, dsp::Shaping (mode), sig), seg);
        for (double fk : *tab.f)
        {
            if (fk * 1000.0 >= fs / 2.0 || std::abs (fk - 0.997) < 0.2)
                continue; // skip the test tone itself
            const double hw = std::max (halfWidthFor (fk), 400.0);
            const double diff = 10.0 * std::log10 (bandMean (p1, fs, fk * 1000.0, hw) / bandMean (p2, fs, fk * 1000.0, hw));
            CAPTURE (fs, mode, fk, diff);
            CHECK (std::abs (diff - 4.8) <= 1.0);
        }
    }
}

TEST_CASE ("NTF identical for every word length (spec 7.3)")
{
    const double fs = 48000.0;
    const auto silence = std::vector<double> (1 << 18, 0.0);
    for (int mode = 1; mode <= 3; ++mode)
    {
        const double r16 = rms (totalError (fs, 16, dsp::Dither::Type1, dsp::Shaping (mode), silence));
        const double r24 = rms (totalError (fs, 24, dsp::Dither::Type1, dsp::Shaping (mode), silence));
        CHECK (r16 == Approx (r24).epsilon (0.03));
    }
}

TEST_CASE ("NTF designs are monic and minimum phase; 176.4 / 192 kHz stay below the 96 kHz curve up to 20 kHz")
{
    for (int r = 0; r < dsp::ntf::numRates; ++r)
        for (int m = 0; m < 3; ++m)
        {
            const double* h = dsp::ntf::coefficients[r][m];
            CHECK (h[0] == 1.0);
            // minimum phase <=> mean log magnitude over the band is 0 (Gerzon-Craven), and > 0 otherwise
            const double fs = dsp::ntf::designRates[size_t (r)];
            const int n = 4096;
            double meanLog = 0.0;
            for (int k = 0; k < n; ++k)
            {
                const double w = pi * (double (k) + 0.5) / n;
                double re = 0.0, im = 0.0;
                for (int t = 0; t < dsp::ntf::numTaps; ++t)
                {
                    re += h[t] * std::cos (w * t);
                    im -= h[t] * std::sin (w * t);
                }
                const double mag = std::sqrt (re * re + im * im);
                meanLog += std::log (mag) / n;
                const double f = w / pi * fs / 2.0 / 1000.0;
                if (fs > 170000.0 && f <= 20.0 && f >= 0.1)
                {
                    size_t j = 0;
                    while (j + 2 < f96.size() && f96[j + 1] < f)
                        ++j;
                    const double lim = t96[size_t (m)][j] + (t96[size_t (m)][j + 1] - t96[size_t (m)][j]) * (f - f96[j]) / (f96[j + 1] - f96[j]);
                    CAPTURE (fs, m, f);
                    CHECK (20.0 * std::log10 (mag) <= lim + 0.5);
                }
            }
            CAPTURE (r, m);
            CHECK (std::abs (meanLog) < 1e-3);
        }
}

TEST_CASE ("176.4 / 192 kHz: shaped Type 1 noise below 20 kHz is lower than unshaped")
{
    for (double fs : { 176400.0, 192000.0 })
        for (int mode = 1; mode <= 3; ++mode)
        {
            const auto e = totalError (fs, 16, dsp::Dither::Type1, dsp::Shaping (mode), std::vector<double> (1 << 19, 0.0));
            const auto psd = welchPsd (e, 8192);
            double acc = 0.0;
            int cnt = 0;
            for (double f = 500.0; f <= 15000.0; f += 500.0, ++cnt)
                acc += bandMean (psd, fs, f, 200.0) / 0.25;
            CHECK (10.0 * std::log10 (acc / cnt) < -10.0);
        }
}
