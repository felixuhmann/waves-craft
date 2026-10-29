// Spec r1 section 4 / table T6: detector weighting; a steady sine is read at its weighted RMS level.
#include <catch2/catch_test_macros.hpp>

#include "TestHelpers.h"

using namespace craft::levelrider::test;

namespace
{
const double t6Freq[] = { 125, 250, 500, 1000, 2000, 4000, 8000, 16000 };
const double t6Rel[] = { -6.6, -2.4, -0.5, 0.0, -0.2, -1.4, -3.4, -5.6 };

// Effective detector level of a steady sine at -27 dBFS RMS, from the settled ride gain (+-12 range, T -21).
double readSine (double fs, double hz, double* gain = nullptr)
{
    auto e = makeEngine (fs, -12, 12);
    const auto tr = runSegments (e, { { -27.0, 8.0 } }, Stimulus::Sine, hz);
    const double g = tr.mean (7.5, 8.0);
    if (gain != nullptr)
        *gain = g;
    return levelFromGain (g, -21.0, -12.0, 12.0);
}
} // namespace

TEST_CASE ("T6: detector weighting through the engine (48 kHz)", "[T6][detector]")
{
    const double ref = readSine (48000.0, 1000.0);
    CHECK (std::abs (ref - (-27.0)) <= 0.1); // the 1 kHz sine is read at its RMS level
    for (size_t i = 0; i < std::size (t6Freq); ++i)
    {
        INFO (t6Freq[i] << " Hz");
        CHECK (std::abs (readSine (48000.0, t6Freq[i]) - ref - t6Rel[i]) <= 1.0);
    }
    // 63 Hz: read at least 9 dB lower, i.e. the rider sits at its +12 dB limit
    double g63 = 0.0;
    readSine (48000.0, 63.0, &g63);
    CHECK (g63 >= 11.9);
}

TEST_CASE ("T6: weighting response at 44.1 / 48 / 96 kHz", "[T6][detector][rates]")
{
    for (double fs : { 44100.0, 48000.0, 96000.0 })
    {
        Weighting w;
        w.prepare (fs);
        CHECK (std::abs (w.responseDb (1000.0, fs)) < 1e-9);
        for (size_t i = 0; i < std::size (t6Freq); ++i)
        {
            INFO (t6Freq[i] << " Hz at " << fs);
            CHECK (std::abs (w.responseDb (t6Freq[i], fs) - t6Rel[i]) <= 0.6);
        }
        CHECK (w.responseDb (63.0, fs) <= -9.0);
    }
}

TEST_CASE ("T6: low and high frequency sines also at 44.1 and 96 kHz", "[T6][detector][rates]")
{
    for (double fs : { 44100.0, 96000.0 })
    {
        const double ref = readSine (fs, 1000.0);
        for (size_t i : { size_t (0), size_t (1), size_t (7) })
        {
            INFO (t6Freq[i] << " Hz at " << fs);
            CHECK (std::abs (readSine (fs, t6Freq[i]) - ref - t6Rel[i]) <= 1.0);
        }
    }
}

TEST_CASE ("Analytic power: a steady sine gives a ripple-free power reading", "[detector]")
{
    for (double fs : { 44100.0, 48000.0, 96000.0 })
        for (double hz : { 63.0, 125.0, 1000.0, 9000.0 })
        {
            QuadraturePair q;
            q.reset();
            double lo = 1e9, hi = 0.0;
            const int n = int (fs);
            for (int i = 0; i < n; ++i)
            {
                const double p = q.power (std::sin (2.0 * pi * hz * double (i) / fs));
                if (i > n / 2)
                {
                    lo = std::min (lo, p);
                    hi = std::max (hi, p);
                }
            }
            INFO (hz << " Hz at " << fs);
            CHECK (10.0 * std::log10 (hi / 0.5) < 0.1);
            CHECK (10.0 * std::log10 (lo / 0.5) > -0.1);
        }
}

TEST_CASE ("Noise is read above its RMS (peak-weighted ballistics), white noise about like the 1 kHz sine", "[detector]")
{
    // informative (section 12 note under T6): band noise ~ +3 dB in Slow, white noise ~ the 1 kHz sine
    auto e = makeEngine (48000.0, -12, 12);
    const auto tr = runSegments (e, { { -27.0, 10.0 } });
    const double band = levelFromGain (tr.mean (9.0, 10.0), -21.0, -12.0, 12.0) + 27.0;
    CHECK (band > 1.5);
    CHECK (band < 4.5);

    RiderEngine w = makeEngine (48000.0, -12, 12);
    std::mt19937 rng (3);
    std::normal_distribution<double> nd;
    const double a = dbToGain (-27.0);
    std::vector<double> l (480), r (480);
    double gsum = 0.0;
    int count = 0;
    for (int blk = 0; blk < 1000; ++blk)
    {
        for (size_t i = 0; i < l.size(); ++i)
            l[i] = r[i] = a * nd (rng);
        double* ch[2] = { l.data(), r.data() };
        w.process (ch, 2, (const double* const*) nullptr, 0, 480);
        if (blk >= 900)
        {
            gsum += w.getGainDb();
            ++count;
        }
    }
    const double white = levelFromGain (gsum / count, -21.0, -12.0, 12.0) + 27.0;
    CHECK (std::abs (white) < 1.5);
}
