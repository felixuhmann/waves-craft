// Section 10: analog modes (measured with Ratio 1, Output 0, Mix 100) - tolerances of section 13:
// magnitude +-0.5 dB, fundamental +-0.5 dB (Mode 3 +-1 dB), harmonics +-4 dB, noise +-3 dB.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "TestHelpers.h"

using namespace testutil;
using Catch::Approx;

namespace
{
Settings analogSettings (int mode)
{
    Settings s = neutralSettings();
    s.analog = mode;
    return s;
}

const double magFreqs[10] = { 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000 };
const double magTable[5][10] = {
    {},
    { -0.11, -0.17, -0.21, -0.24, -0.26, -0.26, -0.27, -0.27, -0.28, -0.32 },
    { 0.01, 0.01, -0.05, -0.26, -0.63, -0.76, -0.78, -0.68, -0.42, -0.08 },
    { 0.02, 0.17, 0.30, -0.28, -2.78, -4.27, -4.65, -3.62, -1.79, -0.37 },
    { -0.36, -0.34, -0.29, -0.34, -0.34, -0.31, -0.18, -0.08, -0.07, -0.22 },
};

struct Tone
{
    double f, a;
};
const Tone tones[7] = { { 100, 0.03 }, { 100, 0.1 }, { 100, 0.3 }, { 100, 0.95 }, { 1000, 0.03 }, { 1000, 0.3 }, { 1000, 0.95 } };
const double toneTable[5][7][3] = {
    {},
    { { -0.21, -82.9, -87.7 }, { -0.21, -74.2, -77.6 }, { -0.2, -65.2, -68.8 }, { -0.17, -56.6, -60.8 }, { -0.26, -81.2, -84.6 }, { -0.24, -62.3, -65.9 }, { -0.2, -54.0, -58.3 } },
    { { -0.05, -97.8, -94.1 }, { -0.05, -94.5, -83.4 }, { -0.05, -89.3, -74.2 }, { -0.05, -79.1, -64.7 }, { -0.76, -92.0, -75.5 }, { -0.7, -70.8, -56.4 }, { -0.57, -61.8, -47.8 } },
    { { 0.31, -61.3, -62.3 }, { 0.28, -51.4, -52.9 }, { 0.22, -43.6, -46.0 }, { 0.13, -37.8, -42.4 }, { -4.3, -33.1, -34.7 }, { -1.64, -23.0, -28.4 }, { -0.49, -21.9, -32.2 } },
    { { -0.33, -109.3, -36.8 }, { -0.1, -125.2, -47.2 }, { -0.03, -132.8, -56.7 }, { -0.01, -141.0, -66.7 }, { -0.31, -116.7, -36.6 }, { -0.03, -147.0, -56.6 }, { -0.0, -147.9, -66.6 } },
};
} // namespace

TEST_CASE ("Analog Off is the identity", "[analog]")
{
    const auto x = gaussianNoise (48000, 0.1, 3u);
    const auto y = runAligned (stereo (x), 48000.0, analogSettings (0)).l;
    for (size_t i = 0; i < x.size(); ++i)
        REQUIRE (y[i] == x[i]);
}

TEST_CASE ("Analog small-signal magnitude at -30 dBFS (+-0.5 dB)", "[analog]")
{
    for (int m = 1; m <= 4; ++m)
        for (int i = 0; i < 10; ++i)
        {
            const auto r = measureTone (48000.0, analogSettings (m), magFreqs[i], dbToLin (-30.0), 0.5, 0.5);
            INFO ("Mode " << m << " " << magFreqs[i] << " Hz: " << r.gainDb);
            CHECK (r.gainDb == Approx (magTable[m][i]).margin (0.5));
        }
}

TEST_CASE ("Analog steady tones: fundamental, H2, H3", "[analog]")
{
    for (int m = 1; m <= 4; ++m)
        for (int i = 0; i < 7; ++i)
        {
            const auto r = measureTone (48000.0, analogSettings (m), tones[i].f, tones[i].a, 0.5, 2.0);
            const auto& ref = toneTable[m][i];
            INFO ("Mode " << m << " " << tones[i].f << " Hz a=" << tones[i].a << ": " << r.gainDb << " / " << r.h2Dbc << " / "
                          << r.h3Dbc);
            CHECK (r.gainDb == Approx (ref[0]).margin (m == 3 ? 1.0 : 0.5));
            // Very low harmonics are at or below the analysis noise floor of any measurement (Mode 2: noise at
            // -82.7 dBFS; Mode 4 H2 below -100 dBc): there only an upper bound is checked.
            const bool floor2 = ref[1] + linToDb (tones[i].a) < -125.0 || ref[1] < -100.0;
            const bool floor3 = ref[2] + linToDb (tones[i].a) < -125.0;
            if (floor2)
                CHECK (r.h2Dbc < ref[1] + 4.0);
            else
                CHECK (r.h2Dbc == Approx (ref[1]).margin (4.0));
            if (floor3)
                CHECK (r.h3Dbc < ref[2] + 4.0);
            else
                CHECK (r.h3Dbc == Approx (ref[2]).margin (4.0));
        }
}

TEST_CASE ("Analog Mode 3 follows the post-Output level", "[analog]")
{
    // Ratio 1, input -50 dBFS: -5.1 dB with Output -12, -4.75 dB with Output +12 (1 kHz)
    for (const auto& [out, ref] : { std::pair { -12.0, -5.1 }, std::pair { 12.0, -4.75 } })
    {
        Settings s = analogSettings (3);
        s.outputDb = out;
        const auto r = measureTone (48000.0, s, 1000.0, dbToLin (-50.0), 0.5, 0.5);
        INFO ("Output " << out << ": " << r.gainDb - out);
        CHECK (r.gainDb - out == Approx (ref).margin (0.5));
    }
}

TEST_CASE ("Analog noise floors with silent input (+-3 dB), uncorrelated channels", "[analog]")
{
    const double ref[5] = { 0.0, -101.6, -82.7, -100.9, -108.7 };
    const size_t n = 96000;
    for (int m = 0; m <= 4; ++m)
    {
        Signal in;
        in.l.assign (n, 0.0);
        in.r.assign (n, 0.0);
        const auto y = runAligned (in, 48000.0, analogSettings (m));
        const double rl = rms (y.l, 4800, n), rr = rms (y.r, 4800, n);
        if (m == 0)
        {
            CHECK (rl == 0.0);
            CHECK (rr == 0.0);
            continue;
        }
        INFO ("Mode " << m << ": " << linToDb (rl));
        CHECK (linToDb (rl) == Approx (ref[m]).margin (3.0));
        CHECK (linToDb (rr) == Approx (ref[m]).margin (3.0));
        double c = 0.0;
        for (size_t i = 4800; i < n; ++i)
            c += y.l[i] * y.r[i];
        c /= double (n - 4800) * rl * rr;
        CHECK (std::abs (c) < 0.05);
    }
}

TEST_CASE ("Analog clicks: no mode raises a transient above its input", "[analog]")
{
    const double ref[5][2] = { { 0.3, 0.9 }, { 0.2906, 0.8743 }, { 0.2882, 0.8679 }, { 0.2547, 0.7982 }, { 0.299, 0.899 } };
    for (int m = 0; m <= 4; ++m)
        for (int k = 0; k < 2; ++k)
        {
            const double a = k == 0 ? 0.3 : 0.9;
            std::vector<double> x (9600, 0.0);
            x[4800] = a;
            const auto y = runAligned (stereo (x), 48000.0, analogSettings (m)).l;
            const double peak = maxAbs (y);
            INFO ("Mode " << m << " a " << a << ": " << peak);
            CHECK (peak <= a);
            CHECK (peak == Approx (ref[m][k]).margin (0.03 * a));
        }
}

TEST_CASE ("Analog levels are sample-rate independent (44.1 / 96 kHz within 0.05 dB at 100 Hz / 1 kHz)", "[analog][rates]")
{
    for (int m = 1; m <= 4; ++m)
        for (double f : { 100.0, 1000.0 })
            for (double a : { 0.03, 0.3 })
            {
                const double ref = measureTone (48000.0, analogSettings (m), f, a, 0.5, 0.5).gainDb;
                for (double fs : { 44100.0, 96000.0 })
                {
                    INFO ("Mode " << m << " f " << f << " a " << a << " fs " << fs);
                    CHECK (measureTone (fs, analogSettings (m), f, a, 0.5, 0.5).gainDb == Approx (ref).margin (0.05));
                }
            }
}
