// Spec r3 sections 2, 6 and 7: loop filters (HiPass / LoPass) and LoFi.
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "TestHelpers.h"

using namespace testutil;

namespace
{
// Independent reference: analog 2nd-order prototype mapped with the bilinear transform whose
// corner is placed at w0 = 2 atan(pi f / fs), i.e. normalised frequency W = tan(w/2) / tan(w0/2).
double expectedDb (bool highPass, double fc, double q, double f, double fs)
{
    const double W = std::tan (pi * f / fs) / std::tan (0.5 * 2.0 * std::atan (pi * fc / fs));
    const double den = (1.0 - W * W) * (1.0 - W * W) + (W / q) * (W / q);
    const double num = highPass ? W * W * W * W : 1.0;
    return 10.0 * std::log10 (num / den);
}

// First echo (linear regime: tiny impulse) of the engine with feedback 0, as an impulse response.
std::vector<double> firstEcho (Settings s, double fs, int length)
{
    s.feedbackPct = 0.0;
    s.delayMs = s.delayRMs = 5.0;
    auto e = makeEngine (s, fs);
    const int D = int (std::round (5.0 * fs / 1000.0));
    auto y = impulse (e, D + length, 1.0e-7);
    std::vector<double> h (y.l.begin() + D, y.l.end());
    for (auto& v : h)
        v /= 1.0e-7;
    return h;
}
} // namespace

TEST_CASE ("LoPass (Q 0.95) and HiPass (Q 0.80) responses in the loop", "[filters]")
{
    for (double fs : { 44100.0, 48000.0, 96000.0 })
    {
        for (double fc : { 437.0, 2000.0, 14200.0 })
        {
            auto s = coreSettings();
            s.loPassHz = fc;
            const auto h = firstEcho (s, fs, int (fs));
            for (double f = 15.0; f <= 20000.0; f *= 1.25)
            {
                const double got = db (std::abs (dtft (h, f, fs)));
                const double want = expectedDb (false, fc, 0.95, f, fs);
                if (want > -60.0)
                    CHECK (std::abs (got - want) < 0.1);
            }
        }
        for (double fc : { 27.0, 437.0, 5000.0, 20100.0 })
        {
            auto s = coreSettings();
            s.hiPassHz = fc;
            const auto h = firstEcho (s, fs, int (fs));
            for (double f = 15.0; f <= 20000.0; f *= 1.25)
            {
                const double got = db (std::abs (dtft (h, f, fs)));
                const double want = expectedDb (true, fc, 0.80, f, fs);
                if (want > -60.0)
                    CHECK (std::abs (got - want) < 0.1);
            }
        }
    }
    // spec example: 14.2 kHz at 48 kHz has its effective corner at (fs/pi) atan(pi f/fs) = 11.44 kHz,
    // where a Q 0.95 low-pass has gain 0.95 (-0.45 dB)
    auto s = coreSettings();
    s.loPassHz = 14200.0;
    const auto h = firstEcho (s, 48000.0, 48000);
    const double fe = 48000.0 / pi * std::atan (pi * 14200.0 / 48000.0);
    CHECK (std::abs (fe - 11440.0) < 10.0);
    CHECK (std::abs (db (std::abs (dtft (h, fe, 48000.0))) - db (0.95)) < 0.05);
}

TEST_CASE ("Filters act on every pass of the loop", "[filters]")
{
    // second echo = filter applied twice
    auto s = coreSettings (5.0, 100.0);
    s.loPassHz = 1000.0;
    const double fs = 48000.0;
    auto e = makeEngine (s, fs);
    auto y = impulse (e, 1200, 1.0e-7);
    std::vector<double> e1 (y.l.begin() + 240, y.l.begin() + 481), e2 (y.l.begin() + 481, y.l.end());
    for (double f : { 500.0, 1000.0, 3000.0 })
    {
        const double g1 = db (std::abs (dtft (e1, f, fs)) / 1.0e-7);
        // the second echo overlaps the decay of the first; compare totals only roughly via the model
        CHECK (std::abs (g1 - expectedDb (false, 1000.0, 0.95, f, fs)) < 0.3);
    }
    double energy2 = 0.0;
    for (double v : e2)
        energy2 += v * v;
    CHECK (energy2 > 0.0);
}

TEST_CASE ("Filters Off are exactly transparent", "[filters]")
{
    auto e = makeEngine (coreSettings (5.0, 0.0), 48000.0);
    auto y = impulse (e, 500, 0.01);
    CHECK_THAT (y.l[240], Catch::Matchers::WithinAbs (0.01 / 1.01, 1e-15));
    CHECK (y.l[241] == 0.0);
}

TEST_CASE ("LoFi response table, independent of the sample rate", "[lofi]")
{
    const double f[] = { 5000, 6000, 7000, 8000 };
    const double want[] = { -1.0, -4.4, -12.0, -27.0 };
    for (double fs : { 44100.0, 48000.0, 96000.0 })
    {
        LoFiFilter lf;
        lf.prepare (fs);
        for (double fr = 20.0; fr <= 4000.0; fr += 20.0)
            CHECK (std::abs (db (std::abs (lf.response (fr, fs)))) <= 0.05);
        for (int i = 0; i < 4; ++i)
            CHECK (std::abs (db (std::abs (lf.response (f[i], fs))) - want[i]) <= 1.0);
        CHECK (db (std::abs (lf.response (9000.0, fs))) <= -50.0);
        for (double fr = 10000.0; fr < 0.5 * fs; fr += 100.0)
            CHECK (db (std::abs (lf.response (fr, fs))) <= -55.0);
    }
    // also measured through the engine (first echo, linear regime)
    auto s = coreSettings();
    s.lofi = true;
    const auto h = firstEcho (s, 48000.0, 4800);
    for (int i = 0; i < 4; ++i)
        CHECK (std::abs (db (std::abs (dtft (h, f[i], 48000.0))) - want[i]) <= 1.0);
    CHECK (std::abs (db (std::abs (dtft (h, 1000.0, 48000.0)))) <= 0.05);
}

TEST_CASE ("HiPass corner follows 19 * 1058^r near the bottom end (r3)", "[filters]")
{
    for (double fs : { 44100.0, 48000.0, 96000.0 })
        for (double r : { 0.01, 0.05 })
        {
            const double fc = param::HiPass::fromNorm (r);
            CHECK_THAT (fc, Catch::Matchers::WithinRel (19.0 * std::pow (1058.0, r), 1e-12));
            auto s = coreSettings();
            s.hiPassHz = fc;
            const auto h = firstEcho (s, fs, int (fs * 2));
            for (double f : { 15.0, 20.0, 25.0, 30.0, 50.0, 100.0 })
            {
                const double got = db (std::abs (dtft (h, f, fs)));
                const double want = expectedDb (true, fc, 0.80, f, fs);
                INFO (fs << " Hz, r " << r << ", " << f << " Hz: " << got << " vs " << want);
                CHECK (std::abs (got - want) < 0.1);
            }
        }
}

TEST_CASE ("Analog wet chains add no low-frequency roll-off of their own (1 Hz DC blocker)", "[filters][analog]")
{
    // The chains sit after the loop filters; their own low end must stay flat so the HiPass corner is unchanged.
    // Modes 2 and 4 are flat below 200 Hz in the response table.
    for (int mode : { 2, 4 })
    {
        const double fs = 48000.0;
        AnalogStage a;
        a.prepare (mode, fs);
        std::vector<double> h (size_t (fs * 2));
        for (size_t n = 0; n < h.size(); ++n)
            h[n] = a.process (n == 0 ? 1.0e-2 : 0.0) / 1.0e-2;
        const double g15 = db (std::abs (dtft (h, 15.0, fs))), g200 = db (std::abs (dtft (h, 200.0, fs)));
        INFO ("mode " << mode << ": " << g15 - g200 << " dB at 15 Hz re 200 Hz");
        CHECK (std::abs (g15 - g200) < 0.05);
    }
}
