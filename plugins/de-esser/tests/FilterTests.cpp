// Spec r1 sections 4, 6 and test vectors 9.1 / 9.5: sidechain filter, monitor level, split filter.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "TestHelpers.h"

using namespace craft::deesser::test;

namespace
{
// Independent reference of the section 4 definition: cookbook biquads in long double, direct form I.
struct RefBiquad
{
    long double b0, b1, b2, a1, a2, x1 = 0, x2 = 0, y1 = 0, y2 = 0;
    RefBiquad (bool high, long double f0, long double fs)
    {
        const long double w = 2.0L * 3.14159265358979323846264338327950288L * f0 / fs;
        const long double alpha = std::sin (w) / (2.0L * 0.70710678118654752440084436210484903L);
        const long double cw = std::cos (w), a0 = 1.0L + alpha;
        if (high) { b0 = (1 + cw) / 2; b1 = -(1 + cw); b2 = (1 + cw) / 2; }
        else      { b0 = (1 - cw) / 2; b1 = (1 - cw); b2 = (1 - cw) / 2; }
        b0 /= a0; b1 /= a0; b2 /= a0;
        a1 = -2 * cw / a0;
        a2 = (1 - alpha) / a0;
    }
    long double operator() (long double x)
    {
        const long double y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
        x2 = x1; x1 = x; y2 = y1; y1 = y;
        return y;
    }
};

std::vector<double> referenceMonitor (const Stereo& in, double freq, double fs, bool bandPass)
{
    RefBiquad h1 (true, 0.84L * freq, fs), h2 (true, 0.84L * freq, fs), h3 (true, 0.84L * freq, fs);
    long double lpf = 1.457107L * freq;
    if (lpf >= 0.4999L * fs)
        lpf = 0.4999L * fs;
    RefBiquad lp (false, lpf, fs);
    std::vector<double> out (in.size());
    for (size_t i = 0; i < in.size(); ++i)
    {
        long double d = h3 (h2 (h1 ((static_cast<long double> (in.l[i]) + in.r[i]) / 2)));
        if (bandPass)
            d = lp (d);
        out[i] = double (d / std::sqrt (2.0L));
    }
    return out;
}

double sidechainMagnitudeDb (double freq, SidechainMode mode, double toneHz, double fs = 48000.0)
{
    auto e = makeEngine (fs, -80.0, freq, mode, AudioMode::Split, MonitorMode::Sidechain);
    const size_t n = size_t (fs * 0.3);
    Stereo in (n);
    in.l = steppedSine (toneHz, fs, { { 0.0, n } }); // left only, R silent
    const auto out = runStereo (e, in);
    const auto p = phasor (out.l, toneHz, fs, n / 2, n);
    const auto q = phasor (out.r, toneHz, fs, n / 2, n);
    REQUIRE (std::abs (std::abs (p) - std::abs (q)) <= 1e-12); // both outputs identical
    return gainToDb (std::abs (p));
}
} // namespace

TEST_CASE ("9.1 sidechain / monitor magnitude matches the spec table", "[filter][9.1]")
{
    const double tones[] = { 1000, 2000, 3000, 4000, 5000, 5506, 6000, 8000, 11000, 16000, 20000 };
    struct Row
    {
        double freq;
        SidechainMode mode;
        double db[11];
    };
    const Row rows[] = {
        { 2000, SidechainMode::HighPass, { -37.73, -14.27, -10.21, -9.40, -9.18, -9.13, -9.10, -9.05, -9.03, -9.03, -9.03 } },
        { 2000, SidechainMode::BandPass, { -37.79, -15.12, -13.49, -16.13, -19.41, -21.03, -22.56, -28.13, -35.33, -47.14, -60.48 } },
        { 5506, SidechainMode::HighPass, { -90.42, -54.45, -34.53, -22.66, -16.07, -14.07, -12.68, -10.10, -9.24, -9.04, -9.03 } },
        { 5506, SidechainMode::BandPass, { -90.42, -54.47, -34.59, -22.85, -16.55, -14.78, -13.69, -13.08, -17.20, -28.12, -41.39 } },
        { 11000, SidechainMode::HighPass, { -131.8, -95.46, -74.03, -58.70, -46.82, -41.75, -37.29, -23.58, -13.28, -9.36, -9.05 } },
        { 11000, SidechainMode::BandPass, { -131.8, -95.46, -74.03, -58.71, -46.83, -41.76, -37.31, -23.63, -13.55, -12.33, -22.51 } },
    };
    for (const auto& row : rows)
        for (int i = 0; i < 11; ++i)
        {
            CAPTURE (row.freq, int (row.mode), tones[i]);
            const double m = sidechainMagnitudeDb (row.freq, row.mode, tones[i]);
            const double tol = row.db[i] < -100.0 ? 0.06 : 0.0055; // table rounding (0.1 / 0.01 dB) + measurement
            CHECK (m == Catch::Approx (row.db[i]).margin (tol));
        }
}

TEST_CASE ("Sidechain / monitor output matches the section 4/6 definition within -100 dB", "[filter][monitor]")
{
    for (double fs : { 44100.0, 48000.0, 96000.0 })
        for (double freq : { 2000.0, 5506.0, 11000.0, 16000.0 })
            for (auto mode : { SidechainMode::HighPass, SidechainMode::BandPass })
            {
                CAPTURE (fs, freq, int (mode));
                auto e = makeEngine (fs, -80.0, freq, mode, AudioMode::Split, MonitorMode::Sidechain);
                const size_t n = size_t (fs * 0.2);
                Stereo in (n);
                std::mt19937 rng (7);
                std::uniform_real_distribution<double> u (-1.0, 1.0);
                for (size_t i = 0; i < n; ++i)
                {
                    in.l[i] = u (rng);
                    in.r[i] = 0.5 * u (rng);
                }
                const auto out = runStereo (e, in);
                const auto ref = referenceMonitor (in, freq, fs, mode == SidechainMode::BandPass);
                double peak = 0, err = 0;
                for (size_t i = 0; i < n; ++i)
                {
                    peak = std::max (peak, std::abs (ref[i]));
                    err = std::max (err, std::abs (out.l[i] - ref[i]));
                    REQUIRE (out.l[i] == out.r[i]);
                }
                CHECK (gainToDb (err / peak) <= -100.0);
            }
}

TEST_CASE ("Monitor level: d / sqrt 2, without gain reduction, mono equals stereo L = R", "[filter][monitor]")
{
    // A loud 16 kHz tone far above threshold: the monitor carries the filtered sidechain unattenuated.
    const double fs = 48000.0;
    auto e = makeEngine (fs, -80.0, 2000.0, SidechainMode::HighPass, AudioMode::Wideband, MonitorMode::Sidechain);
    const size_t n = 24000;
    Stereo in (n);
    in.l = steppedSine (16000.0, fs, { { -6.0, n } });
    in.r = in.l;
    const auto out = runStereo (e, in);
    // |SC| at 16 kHz is -0.0x dB for Freq 2000 HP; monitor = d / sqrt 2 -> -6 - 3.01 dB
    const double lvl = gainToDb (std::abs (phasor (out.l, 16000.0, fs, n / 2, n)));
    CHECK (lvl == Catch::Approx (-6.0 - 3.0103 - 0.0).margin (0.02));
    CHECK (e.getCurrentGain() < 0.06); // detection keeps running

    auto m = makeEngine (fs, -80.0, 2000.0, SidechainMode::HighPass, AudioMode::Wideband, MonitorMode::Sidechain, 1);
    std::vector<double> mono = in.l;
    for (size_t pos = 0; pos < n; pos += 100)
    {
        double* ch[1] = { mono.data() + pos };
        m.process (ch, int (std::min<size_t> (100, n - pos)));
    }
    double err = 0;
    for (size_t i = 0; i < n; ++i)
        err = std::max (err, std::abs (mono[i] - out.l[i]));
    CHECK (err <= 1e-15);
}

TEST_CASE ("Band-pass low-pass corner is clamped below fs/2", "[filter]")
{
    // 1.457107 * 16000 = 23314 Hz > 22050 Hz at 44.1 kHz: must stay stable and finite.
    const auto c = SidechainCoeffs::design (16000.0, 44100.0);
    SidechainFilter f;
    double peak = 0;
    for (int i = 0; i < 44100; ++i)
    {
        const auto o = f.process (c, i == 0 ? 1.0 : 0.0);
        REQUIRE (std::isfinite (o.bandPass));
        peak = std::max (peak, std::abs (o.bandPass));
    }
    CHECK (peak < 2.0);
}

TEST_CASE ("9.5 split shelf at maximum attenuation (g = 1/19)", "[filter][9.5]")
{
    const double fs = 48000.0;
    const double tones[] = { 1000, 2000, 3000, 5506, 8000, 11000, 16000, 20000 };
    struct Row
    {
        double freq, k;
        double db[8], deg[8];
    };
    const Row rows[] = {
        { 3000, 0.06290, { -3.18, -7.26, -10.30, -15.26, -18.40, -21.04, -23.89, -25.15 }, { -43, -58, -63, -63, -58, -50, -33, -17 } },
        { 5506, 0.11916, { -1.14, -3.45, -5.75, -10.30, -13.61, -16.81, -21.27, -24.22 }, { -27, -45, -54, -63, -64, -61, -49, -29 } },
        { 11000, 0.27732, { -0.24, -0.88, -1.80, -4.52, -7.22, -10.30, -15.58, -20.84 }, { -13, -24, -33, -50, -58, -63, -63, -50 } },
    };
    const double g = 1.0 / 19.0;
    for (const auto& row : rows)
    {
        const auto c = SplitCoeffs::design (row.freq, fs);
        CHECK (c.k == Catch::Approx (row.k).margin (0.000005));
        for (int i = 0; i < 8; ++i)
        {
            CAPTURE (row.freq, tones[i]);
            const size_t n = 9600;
            const auto x = steppedSine (tones[i], fs, { { -40.0, n } });
            SplitHighBand hb;
            std::vector<double> y (n);
            for (size_t j = 0; j < n; ++j)
                y[j] = x[j] + (g - 1.0) * hb.process (c, x[j]); // LP1(x) + g HP1(x)
            const auto h = phasor (y, tones[i], fs, n / 2, n) / phasor (x, tones[i], fs, n / 2, n);
            CHECK (gainToDb (std::abs (h)) == Catch::Approx (row.db[i]).margin (0.1));
            CHECK (std::arg (h) * 180.0 / pi == Catch::Approx (row.deg[i]).margin (1.0));
        }
    }
}

TEST_CASE ("Split bands are complementary (LP1 + HP1 = 1)", "[filter]")
{
    const auto c = SplitCoeffs::design (5506.0, 48000.0);
    SplitHighBand hb;
    // impulse response of LP1 = delta - HP1 must be the analytic LP1(z) = k (1 + z^-1) / ((1 + k) + (k - 1) z^-1)
    double lpY1 = 0.0, lpX1 = 0.0;
    for (int i = 0; i < 200; ++i)
    {
        const double x = i == 0 ? 1.0 : 0.0;
        const double lpFromSplit = x - hb.process (c, x);
        const double lp = (c.k * (x + lpX1) - (c.k - 1.0) * lpY1) / (1.0 + c.k);
        lpX1 = x;
        lpY1 = lp;
        CHECK (lpFromSplit == Catch::Approx (lp).margin (1e-15));
    }
}
