// Spec r1 sections 3, 4, 6, 8 and the fixed-release test vectors of section 9.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "TestHelpers.h"

using namespace testutil;
using Catch::Approx;

namespace
{
constexpr double pi = 3.14159265358979323846;

Settings make (double thr, double ceil, double relMs, bool autoRel = false)
{
    Settings s;
    s.thresholdDb = thr;
    s.ceilingDb = ceil;
    s.releaseMs = relMs;
    s.autoRelease = autoRel;
    return s;
}
} // namespace

TEST_CASE ("Latency per sample rate (section 3)", "[latency]")
{
    CHECK (LimiterEngine::latencyForSampleRate (22050.0) == 64);
    CHECK (LimiterEngine::latencyForSampleRate (44100.0) == 64);
    CHECK (LimiterEngine::latencyForSampleRate (48000.0) == 64);
    CHECK (LimiterEngine::latencyForSampleRate (88200.0) == 128);
    CHECK (LimiterEngine::latencyForSampleRate (96000.0) == 128);
    CHECK (LimiterEngine::latencyForSampleRate (176400.0) == 256);
    CHECK (LimiterEngine::latencyForSampleRate (192000.0) == 256);

    for (double fs : { 44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0 })
    {
        LimiterEngine e;
        e.prepare (fs, 2);
        const auto& c = e.getConfig();
        CHECK (c.boxA + c.boxB - 1 == c.latency);
        CHECK (e.getLatencySamples() == LimiterEngine::latencyForSampleRate (fs));

        // the output is delayed by exactly L samples: an impulse below threshold appears at index L
        Signal in;
        in.l.assign (1000, 0.0);
        in.r.assign (1000, 0.0);
        in.l[100] = 0.5;
        in.r[100] = -0.25;
        const auto out = runEngine (in, fs, make (0.0, 0.0, 1.0));
        const int L = e.getLatencySamples();
        for (std::size_t i = 0; i < out.size(); ++i)
        {
            const double expectL = (int (i) == 100 + L) ? 0.5 : 0.0;
            const double expectR = (int (i) == 100 + L) ? -0.25 : 0.0;
            REQUIRE (out.l[i] == expectL);
            REQUIRE (out.r[i] == expectR);
        }
    }
}

TEST_CASE ("Threshold = Ceiling = 0 without overs is bit-exact (section 4)", "[static]")
{
    for (double fs : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        auto in = noise (20000, true, 3, 0.2);
        for (auto* ch : { &in.l, &in.r })
            for (auto& v : *ch)
                v = std::clamp (v, -0.999, 0.999);
        const auto out = runEngine (in, fs, make (0.0, 0.0, 1.0));
        const int L = LimiterEngine::latencyForSampleRate (fs);
        for (std::size_t i = 0; i < in.size(); ++i)
        {
            REQUIRE (out.l[i + std::size_t (L)] == in.l[i]);
            REQUIRE (out.r[i + std::size_t (L)] == in.r[i]);
        }
    }
}

TEST_CASE ("Static curve: 1 kHz sine table (section 9)", "[static]")
{
    const double fs = 48000.0;
    struct Row { double thr, ceil; double expected[6]; };
    const double inputs[6] = { -40.0, -24.0, -12.0, -8.0, 0.0, 12.0 };
    const Row rows[] = {
        { 0.0, 0.0, { -40.0, -24.0, -12.0, -8.0, 0.0, 0.0 } },
        { -24.0, 0.0, { -16.0, 0.0, 0.0, 0.0, 0.0, 0.0 } },
        { -12.0, 0.0, { -28.0, -12.0, 0.0, 0.0, 0.0, 0.0 } },
        { -6.0, 0.0, { -34.0, -18.0, -6.0, -2.0, 0.0, 0.0 } },
        { -12.0, -3.0, { -31.0, -15.0, -3.0, -3.0, -3.0, -3.0 } },
    };
    for (const auto& row : rows)
        for (int k = 0; k < 6; ++k)
        {
            const double amp = dbToLin (inputs[k]);
            Signal in;
            const std::size_t n = 24000;
            in.l.resize (n);
            for (std::size_t i = 0; i < n; ++i)
                in.l[i] = amp * std::sin (2.0 * pi * 1000.0 * double (i) / fs);
            in.r = in.l;
            const auto out = runEngine (in, fs, make (row.thr, row.ceil, 1.0));
            double peak = 0.0;
            for (std::size_t i = n / 2; i < n; ++i)
                peak = std::max ({ peak, std::abs (out.l[i]), std::abs (out.r[i]) });
            INFO ("threshold " << row.thr << " ceiling " << row.ceil << " input " << inputs[k]);
            CHECK (linToDb (peak) == Approx (row.expected[k]).margin (0.005));
        }
}

TEST_CASE ("Sample-peak detection only: fs/4 sine with 0.707 sample peaks passes unchanged", "[static]")
{
    const double fs = 48000.0;
    Signal in;
    in.l.resize (4800);
    for (std::size_t i = 0; i < in.size(); ++i)
        in.l[i] = std::sin (2.0 * pi * 0.25 * double (i) + pi / 4.0); // samples at +-0.7071
    in.r = in.l;
    const auto out = aligned (runEngine (in, fs, make (0.0, 0.0, 1.0)), 64, in.size());
    for (std::size_t i = 0; i < in.size(); ++i)
        REQUIRE (out.l[i] == in.l[i]);
}

TEST_CASE ("Attack S-curve around a click (section 9)", "[attack]")
{
    struct Point { int k; double progress; };
    auto check = [] (double fs, int L, const std::vector<Point>& pts, double height) {
        std::vector<double> x (4 * std::size_t (L) + 400, 0.01);
        const std::size_t k0 = 2 * std::size_t (L);
        x[k0] = height;
        const auto g = dcGain (x, fs, make (-12.0, -12.0, 1000.0));
        const double gmin = *std::min_element (g.begin(), g.end());
        CHECK (gmin == Approx (dbToLin (-12.0) / height).epsilon (1e-12));
        for (const auto& p : pts)
        {
            const double progress = (1.0 - g[k0 + std::size_t (std::ptrdiff_t (p.k))]) / (1.0 - gmin);
            INFO ("fs " << fs << " k " << p.k << " height " << height);
            CHECK (progress == Approx (p.progress).margin (6e-6));
        }
        // the gain starts moving exactly L samples before the peak
        CHECK (g[k0 - std::size_t (L) - 1] == 1.0);
        CHECK (g[k0 - std::size_t (L)] < 1.0);
    };
    const std::vector<Point> p48 { { -64, 0.00097 }, { -60, 0.01462 }, { -56, 0.04386 }, { -48, 0.14912 },
                                   { -40, 0.31676 }, { -33, 0.50000 }, { -32, 0.52632 }, { -24, 0.73099 },
                                   { -16, 0.88304 }, { -8, 0.97271 },  { -4, 0.99415 },  { -1, 1.00000 } };
    const std::vector<Point> p96 { { -128, 0.00025 }, { -112, 0.03778 }, { -96, 0.13852 }, { -64, 0.51333 },
                                   { -32, 0.87753 },  { -16, 0.97037 },  { -1, 1.00000 } };
    for (double height : { 0.5, 2.0, 30.0 })
    {
        check (48000.0, 64, p48, height);
        check (44100.0, 64, p48, height);
        check (96000.0, 128, p96, height);
        check (88200.0, 128, p96, height);
    }
}

TEST_CASE ("Fixed release trajectory after a DC overload (section 9)", "[release]")
{
    const double fs = 48000.0;
    const std::size_t over = 24000;
    std::vector<double> x (over + 3000, 0.01);
    std::fill (x.begin(), x.begin() + std::ptrdiff_t (over), 1.0);
    const auto g = dcGain (x, fs, make (-12.0, 0.0, 10.0));
    const std::pair<int, double> pts[] = { { 0, 0.25120 },  { 16, 0.25509 },  { 32, 0.27488 },
                                           { 64, 0.36408 }, { 128, 0.53219 }, { 240, 0.72664 },
                                           { 480, 0.91356 }, { 960, 0.99136 }, { 1920, 0.99991 } };
    for (const auto& [k, v] : pts)
    {
        INFO ("k " << k);
        CHECK (g[over + std::size_t (k)] == Approx (v).margin (6e-6));
    }
}

TEST_CASE ("Click pair: release targets the hold value (section 4.4 / 9)", "[release]")
{
    const double fs = 48000.0;
    std::vector<double> x (2000, 0.01);
    const std::size_t k0 = 400;
    x[k0] = 2.0;
    x[k0 + 64] = 1.0;
    const auto g = dcGain (x, fs, make (-12.0, 0.0, 10.0));
    // k = 0: the reference reads 0.12560 because it does not clamp (decision D1); we give r[0] = 0.125594.
    CHECK (g[k0] == Approx (dbToLin (-12.0) / 2.0).epsilon (1e-12));
    CHECK (g[k0] == Approx (0.12560).margin (1.5e-5));
    const std::pair<int, double> pts[] = { { 32, 0.12925 }, { 63, 0.14350 }, { 64, 0.14402 }, { 96, 0.18108 },
                                           { 128, 0.28218 }, { 200, 0.49182 }, { 480, 0.86736 } };
    for (const auto& [k, v] : pts)
    {
        INFO ("k " << k);
        CHECK (g[k0 + std::size_t (k)] == Approx (v).margin (6e-6));
    }
}

TEST_CASE ("Engine matches the section 4 model to -100 dB", "[model]")
{
    const double fs = GENERATE (44100.0, 48000.0, 88200.0, 96000.0, 192000.0);
    const bool stereo = GENERATE (true, false);
    std::mt19937 rng (unsigned (fs) + (stereo ? 1u : 0u));
    std::uniform_real_distribution<double> u (0.0, 1.0);
    for (int trial = 0; trial < 6; ++trial)
    {
        Settings s;
        s.thresholdDb = param::Threshold::fromNorm (u (rng));
        s.ceilingDb = param::Ceiling::fromNorm (u (rng));
        s.releaseMs = param::Release::fromNorm (u (rng));
        const auto in = programme (std::size_t (fs * 0.5), stereo, unsigned (trial * 31 + 5), fs);
        const int L = LimiterEngine::latencyForSampleRate (fs);
        const auto out = aligned (runEngine (in, fs, s, -1, 97 + trial * 50), L, in.size());
        const auto g = referenceGain (in, fs, s.thresholdDb, s.releaseMs);
        const double makeup = dbToLin (s.ceilingDb - s.thresholdDb);

        double peak = 0.0, err = 0.0;
        for (std::size_t i = 0; i < in.size(); ++i)
        {
            const double yl = in.l[i] * g[i] * makeup;
            peak = std::max (peak, std::abs (yl));
            err = std::max (err, std::abs (out.l[i] - yl));
            if (stereo)
            {
                const double yr = in.r[i] * g[i] * makeup;
                peak = std::max (peak, std::abs (yr));
                err = std::max (err, std::abs (out.r[i] - yr));
            }
        }
        INFO ("fs " << fs << " thr " << s.thresholdDb << " ceil " << s.ceilingDb << " rel " << s.releaseMs);
        CHECK (linToDb (err / peak) < -100.0);
    }
}

TEST_CASE ("Ceiling is never exceeded (section 8)", "[ceiling]")
{
    const double fs = GENERATE (44100.0, 48000.0, 96000.0, 192000.0);
    for (double rel : { 0.01, 0.1, 1.0, 10.0, 316.23 })
        for (bool autoRel : { false, true })
            for (const auto& tc : { std::pair<double, double> { 0.0, 0.0 }, { -12.0, 0.0 }, { -30.0, -0.1 }, { -6.0, -12.0 },
                                    { -0.1, -30.0 } })
            {
                const auto in = programme (std::size_t (fs * 0.3), true, unsigned (rel * 100) + (autoRel ? 7u : 0u), fs);
                const auto out = runEngine (in, fs, make (tc.first, tc.second, rel, autoRel));
                const double ceiling = dbToLin (tc.second);
                double peak = 0.0;
                for (std::size_t i = 0; i < out.size(); ++i)
                    peak = std::max ({ peak, std::abs (out.l[i]), std::abs (out.r[i]) });
                INFO ("fs " << fs << " rel " << rel << " auto " << autoRel << " thr " << tc.first << " ceil " << tc.second);
                CHECK (peak <= ceiling * (1.0 + 1e-6));
            }
}

TEST_CASE ("Stereo link: a click on one channel reduces both identically", "[link]")
{
    const double fs = 48000.0;
    Signal in;
    in.l.assign (2000, 0.01);
    in.r.assign (2000, 0.01);
    in.l[500] = 3.0;
    const auto out = aligned (runEngine (in, fs, make (-6.0, -6.0, 10.0)), 64, in.size());
    bool reduced = false;
    for (std::size_t i = 0; i < in.size(); ++i)
    {
        if (i == 500)
            continue;
        REQUIRE (out.l[i] == out.r[i]);
        reduced = reduced || out.r[i] < 0.01 * 0.999;
    }
    CHECK (reduced);
    CHECK (std::abs (out.l[500]) == Approx (dbToLin (-6.0)).epsilon (1e-12));
}

TEST_CASE ("Mono runs the same algorithm on one channel", "[mono]")
{
    const double fs = 48000.0;
    const auto st = programme (24000, true, 11, fs);
    Signal mono;
    mono.l = st.l;
    Signal dual;
    dual.l = st.l;
    dual.r = st.l;
    const auto a = runEngine (mono, fs, make (-9.0, -1.0, 5.0));
    const auto b = runEngine (dual, fs, make (-9.0, -1.0, 5.0));
    for (std::size_t i = 0; i < a.size(); ++i)
        REQUIRE (a.l[i] == b.l[i]);
}

TEST_CASE ("Threshold / Ceiling changes are click-free (section 6)", "[smoothing]")
{
    const double fs = 48000.0;
    LimiterEngine e;
    e.prepare (fs, 2);
    e.setSettings (make (0.0, 0.0, 10.0));
    e.reset();
    // constant DC below threshold -> output = gain * makeup; jump the parameters
    std::vector<double> l (48000, 0.1), r (48000, 0.1);
    double* ptrs[2] = { l.data(), r.data() };
    e.process (ptrs, 4800);
    e.setSettings (make (-24.0, -3.0, 10.0)); // static gain +21 dB, and 0.1 is now 4 dB over threshold
    double* ptrs2[2] = { l.data() + 4800, r.data() + 4800 };
    e.process (ptrs2, 48000 - 4800);

    double maxStep = 0.0;
    for (std::size_t i = 4801; i < l.size(); ++i)
        maxStep = std::max (maxStep, std::abs (l[i] - l[i - 1]));
    // final level: the ceiling; the change takes ~20 ms and has no jump larger than a small fraction
    CHECK (l.back() == Approx (dbToLin (-3.0)).epsilon (1e-9));
    CHECK (maxStep < 0.01);
    // ~20 ms: settled to within 1 % after 25 ms
    const std::size_t at25ms = 4800 + 64 + std::size_t (0.025 * fs);
    CHECK (std::abs (l[at25ms] / dbToLin (-3.0) - 1.0) < 0.01);
}

TEST_CASE ("Bypass: delayed dry signal after a short crossfade", "[bypass]")
{
    const double fs = 48000.0;
    LimiterEngine e;
    e.prepare (fs, 2);
    e.setSettings (make (-20.0, -10.0, 10.0));
    e.setBypassed (true);
    e.reset();
    const auto in = programme (9600, true, 2, fs);
    auto l = in.l, r = in.r;
    double* ptrs[2] = { l.data(), r.data() };
    e.process (ptrs, int (l.size()));
    for (std::size_t i = 64; i < l.size(); ++i)
    {
        REQUIRE (l[i] == in.l[i - 64]);
        REQUIRE (r[i] == in.r[i - 64]);
    }
}
