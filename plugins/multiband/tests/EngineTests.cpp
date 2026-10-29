// Spec r1 sections 1, 3 and 6: LTI at Range 0, solo / band bypass / output gain, host bypass bit-exact,
// parameter smoothing.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "TestHelpers.h"

using namespace craft::multiband;
using Catch::Approx;

namespace
{
Settings neutral()
{
    Settings s = Settings::defaults();
    for (auto& b : s.bands)
        b.rangeDb = 0.0;
    return s;
}

// Output of the crossover with per-band linear weights (reference model).
std::vector<double> weightedBands (const std::vector<double>& x, std::array<double, 4> w, double fs, double outGainDb = 0.0)
{
    std::array<CrossoverSection, 3> secs;
    for (int k = 0; k < 3; ++k)
        secs[size_t (k)] = CrossoverSection::design (param::defaultCrossover[size_t (k)], 0.6, fs);
    FourBandSplitter sp;
    sp.setSections (secs);
    std::vector<double> y (x.size());
    std::array<double, 4> b {};
    const double og = test::dbToLin (outGainDb);
    for (size_t i = 0; i < x.size(); ++i)
    {
        sp.process (x[i], b);
        y[i] = og * (w[0] * b[0] + w[1] * b[1] + w[2] * b[2] + w[3] * b[3]);
    }
    return y;
}

double maxDiff (const std::vector<double>& a, const std::vector<double>& b, size_t from = 0)
{
    double m = 0.0;
    for (size_t i = from; i < a.size(); ++i)
        m = std::max (m, std::abs (a[i] - b[i]));
    return m;
}
} // namespace

TEST_CASE ("Linear time-invariant when every Range is 0 dB", "[engine]")
{
    const double fs = 48000.0;
    Settings s = neutral();
    s.bands[0].gainDb = 4.0;
    s.bands[2].gainDb = -7.5;
    s.outputGainDb = 1.5;
    const auto x = test::whiteNoise (24000, 2, 0.1);
    auto run = [&] (double scale) {
        std::vector<double> v (x);
        for (auto& e : v) e *= scale;
        MultibandEngine e;
        e.setSettings (s);
        e.prepare (fs, 2);
        return test::runStereo (e, v, v).first;
    };
    const auto a = run (1.0), b = run (8.0);
    double m = 0.0;
    for (size_t i = 0; i < a.size(); ++i)
        m = std::max (m, std::abs (8.0 * a[i] - b[i]));
    CHECK (m < 1e-12);
    // and equals the weighted crossover
    const auto ref = weightedBands (x, { test::dbToLin (4.0), 1.0, test::dbToLin (-7.5), 1.0 }, fs, 1.5);
    CHECK (maxDiff (a, ref) < 1e-12);
}

TEST_CASE ("Solo: output is the sum of the soloed bands; band bypass: unity, Gain ignored", "[engine]")
{
    const double fs = 48000.0;
    const auto x = test::whiteNoise (24000, 4, 0.1);
    Settings s = neutral();
    s.bands[1].gainDb = 6.0;
    s.bands[1].solo = true;
    s.bands[3].solo = true;
    s.bands[3].gainDb = -3.0;
    s.bands[3].bypass = true; // bypassed band: unity, its Gain ignored
    MultibandEngine e;
    e.setSettings (s);
    e.prepare (fs, 2);
    const auto y = test::runStereo (e, x, x).first;
    const auto ref = weightedBands (x, { 0.0, test::dbToLin (6.0), 0.0, 1.0 }, fs);
    CHECK (maxDiff (y, ref) < 1e-12);

    // a bypassed band has no dynamics either
    Settings d = Settings::defaults();
    d.bands[1].bypass = true;
    d.bands[1].thresholdDb = -80.0;
    d.bands[1].rangeDb = -24.0;
    const auto g = test::runBandGain (d, fs, test::toneSegments (fs, 1000.0, { { 0.5, -6.0 } }), 1);
    CHECK (g.back() == Approx (0.0).margin (1e-9));
}

TEST_CASE ("Solo and bypass switch click-free", "[engine]")
{
    const double fs = 48000.0;
    const auto x = test::toneSegments (fs, 200.0, { { 0.5, -6.0 } });
    Settings s = Settings::defaults();
    MultibandEngine e;
    e.setSettings (s);
    e.prepare (fs, 2);
    double prev = 0.0, maxStep = 0.0;
    for (size_t i = 0; i < x.size(); i += 32)
    {
        if (i == 6400) { s.bands[0].solo = true; e.setSettings (s); }
        if (i == 12800) { s.bands[0].bypass = true; s.bands[0].solo = false; e.setSettings (s); }
        std::vector<double> l (x.begin() + long (i), x.begin() + long (std::min (x.size(), i + 32))), r = l;
        double* io[2] = { l.data(), r.data() };
        e.process (io, int (l.size()));
        for (double v : l)
        {
            maxStep = std::max (maxStep, std::abs (v - prev));
            prev = v;
        }
    }
    CHECK (maxStep < 0.03); // 0.5 * 2 pi 200 / 48000 = 0.013 for the sine itself
}

TEST_CASE ("Host bypass is bit-exact once the crossfade has finished", "[engine]")
{
    const double fs = 48000.0;
    Settings s = Settings::defaults();
    s.bands[1].gainDb = 9.0;
    MultibandEngine e;
    e.setSettings (s);
    e.prepare (fs, 2);
    e.setBypassed (true);
    // first 10 ms crossfade, then exact
    auto warm = test::whiteNoise (2048, 8, 0.3);
    test::runStereo (e, warm, warm);
    std::vector<float> l (4096), r (4096);
    auto n = test::whiteNoise (4096, 9, 0.3);
    for (size_t i = 0; i < l.size(); ++i) { l[i] = float (n[i]); r[i] = float (-n[i] * 0.5); }
    const auto l0 = l, r0 = r;
    float* io[2] = { l.data(), r.data() };
    e.process (io, 4096);
    CHECK (l == l0);
    CHECK (r == r0);

    // the transition into bypass is a crossfade, not a jump
    MultibandEngine f;
    f.setSettings (s);
    f.prepare (fs, 2);
    const auto t = test::toneSegments (fs, 200.0, { { 0.2, -6.0 } });
    double prev = 0.0, maxStep = 0.0;
    for (size_t i = 0; i < t.size(); i += 64)
    {
        if (i == 4800) f.setBypassed (true);
        std::vector<double> a (t.begin() + long (i), t.begin() + long (std::min (t.size(), i + 64))), b = a;
        double* p[2] = { a.data(), b.data() };
        f.process (p, int (a.size()));
        for (double v : a) { maxStep = std::max (maxStep, std::abs (v - prev)); prev = v; }
    }
    CHECK (maxStep < 0.06);
}

TEST_CASE ("Parameter changes are smoothed: click-free, settled within 20 ms", "[engine]")
{
    const double fs = 48000.0;
    Settings s = neutral();
    MultibandEngine e;
    e.setSettings (s);
    e.prepare (fs, 1);
    std::vector<double> ones (size_t (0.1 * fs), 0.25);
    // DC is in band 1 only; step band-1 Gain by +18 dB and Output by -6 dB
    std::vector<double> y = ones;
    double* io[1] = { y.data() };
    e.process (io, 2400); // settle the crossover on DC
    s.bands[0].gainDb = 18.0;
    s.outputGainDb = -6.0;
    e.setSettings (s);
    double* rest[1] = { y.data() + 2400 };
    e.process (rest, int (y.size()) - 2400);
    const double target = 0.25 * test::dbToLin (12.0);
    double maxStep = 0.0;
    for (size_t i = 2401; i < y.size(); ++i)
        maxStep = std::max (maxStep, std::abs (y[i] - y[i - 1]));
    CHECK (maxStep < 0.01);                                    // a gradual glide (largest step << jump of 0.75)
    CHECK (y[2400 + size_t (0.020 * fs)] == Approx (target).epsilon (0.01)); // within 1 % after 20 ms
}
