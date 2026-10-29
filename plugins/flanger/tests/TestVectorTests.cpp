// Spec r1 section 10: impulse-response test vectors (48 kHz, Depth 0, Filter off unless stated).
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "TestHelpers.h"

using namespace testutil;
using Catch::Matchers::WithinAbs;

namespace
{
constexpr double fs = 48000.0;
constexpr int N = 4096;

Settings staticBase()
{
    Settings s;
    s.depthPct = 0.0; // static LFO
    return s;
}

std::vector<double> ir (const Settings& s, int n = N)
{
    return render (s, fs, impulse (n), impulse (n)).l;
}

// Every sample not in `taps` must be (numerically) zero.
void requireOnlyTaps (const std::vector<double>& y, std::initializer_list<int> taps, double tol = 1e-9)
{
    for (int i = 0; i < int (y.size()); ++i)
    {
        bool isTap = false;
        for (int t : taps)
            isTap = isTap || t == i;
        if (! isTap)
        {
            INFO ("sample " << i);
            REQUIRE (std::abs (y[size_t (i)]) <= tol);
        }
    }
}
} // namespace

TEST_CASE ("10: Mix 50, Fb 50, Delay 2.2 -> echoes every 105 samples halving", "[vectors]")
{
    auto s = staticBase();
    const auto y = ir (s);
    CHECK_THAT (y[0], WithinAbs (0.70711, 1e-5));
    CHECK_THAT (y[105], WithinAbs (0.70711, 1e-5));
    CHECK_THAT (y[210], WithinAbs (0.35355, 1e-5));
    CHECK_THAT (y[315], WithinAbs (0.17678, 1e-5));
    for (int k = 1; k < 30; ++k)
        CHECK_THAT (y[size_t (105 * (k + 1))], WithinAbs (y[size_t (105 * k)] * 0.5, 1e-12));
    for (int i = 1; i < 105; ++i)
        REQUIRE (y[size_t (i)] == 0.0);
}

TEST_CASE ("10: Mix 25 / 75 with Fb 0 -> equal-power dry/wet gains", "[vectors]")
{
    auto s = staticBase();
    s.feedbackPct = 0;
    s.mixPct = 25;
    auto y = ir (s);
    CHECK_THAT (y[0], WithinAbs (0.86603, 1e-5));
    CHECK_THAT (y[105], WithinAbs (0.5, 1e-12));
    requireOnlyTaps (y, { 0, 105 });

    s.mixPct = 75;
    y = ir (s);
    CHECK_THAT (y[0], WithinAbs (0.5, 1e-12));
    CHECK_THAT (y[105], WithinAbs (0.86603, 1e-5));
    requireOnlyTaps (y, { 0, 105 });
}

TEST_CASE ("10: Mix 100, Fb 0 -> single tap at floor(Delay * fs)", "[vectors]")
{
    auto s = staticBase();
    s.feedbackPct = 0;
    s.mixPct = 100;
    const std::pair<double, int> cases[] = { { 0.1, 4 }, { 0.5, 24 }, { 1.0, 48 }, { 50.0, 2400 } };
    for (auto [ms, tap] : cases)
    {
        s.delayMs = ms;
        const auto y = ir (s);
        INFO ("Delay " << ms << " ms");
        CHECK (y[size_t (tap)] == 1.0);
        requireOnlyTaps (y, { tap }, 0.0);
    }
}

TEST_CASE ("10: feedback decay and wet gain compensation", "[vectors]")
{
    auto s = staticBase();
    s.mixPct = 100;

    SECTION ("Fb 75, Delay 1.0")
    {
        s.feedbackPct = 75;
        s.delayMs = 1.0;
        const auto y = ir (s);
        CHECK_THAT (y[48], WithinAbs (0.998414, 1e-6));
        for (int k = 1; k < 20; ++k)
            CHECK_THAT (y[size_t (48 * (k + 1))], WithinAbs (y[size_t (48 * k)] * 0.75, 1e-12));
    }
    SECTION ("Fb 100, Delay 1.0 (loop gain limited to 0.999)")
    {
        s.feedbackPct = 100;
        s.delayMs = 1.0;
        const auto y = ir (s);
        CHECK_THAT (y[48], WithinAbs (0.5, 1e-12));
        for (int k = 1; k < 80; ++k)
            CHECK_THAT (y[size_t (48 * (k + 1))], WithinAbs (y[size_t (48 * k)] * 0.999, 1e-12));
    }
    SECTION ("Fb 95, Delay 0.4")
    {
        s.feedbackPct = 95;
        s.delayMs = 0.4;
        const auto y = ir (s);
        CHECK_THAT (y[19], WithinAbs (0.66829, 1e-5));
        for (int k = 1; k < 50; ++k)
            CHECK_THAT (y[size_t (19 * (k + 1))], WithinAbs (y[size_t (19 * k)] * 0.95, 1e-12));
    }
}

TEST_CASE ("3: wet gain compensation examples", "[vectors]")
{
    CHECK_THAT (model::wetCompensation (95, 0.2), WithinAbs (0.5928, 1e-4));
    CHECK_THAT (model::wetCompensation (95, 0.4), WithinAbs (0.6683, 1e-4));
    CHECK_THAT (model::wetCompensation (95, 1.0), WithinAbs (0.8208, 1e-4));
    CHECK_THAT (model::wetCompensation (95, 5.0), WithinAbs (0.9970, 1e-4));
    CHECK (model::wetCompensation (100, 3.0) == 0.5);
}

TEST_CASE ("10: polarity switches", "[vectors]")
{
    auto s = staticBase();
    SECTION ("Mix 50, Wet Polarity -")
    {
        s.wetInvert = true;
        const auto y = ir (s);
        CHECK_THAT (y[0], WithinAbs (0.70711, 1e-5));
        CHECK_THAT (y[105], WithinAbs (-0.70711, 1e-5));
    }
    SECTION ("Mix 100, Fb 50, Feedback Polarity -")
    {
        s.mixPct = 100;
        s.delayMs = 1.0;
        s.feedbackInvert = true;
        const auto y = ir (s);
        CHECK_THAT (y[48], WithinAbs (1.0, 1e-5));
        CHECK_THAT (y[96], WithinAbs (-0.5, 1e-5));
        CHECK_THAT (y[144], WithinAbs (0.25, 1e-5));
        CHECK_THAT (y[192], WithinAbs (-0.125, 1e-5));
    }
}

TEST_CASE ("10: output gain -12 / +12 dB with Mix 0", "[vectors]")
{
    auto s = staticBase();
    s.mixPct = 0;
    s.gainDb = -12;
    auto y = ir (s);
    CHECK_THAT (y[0], WithinAbs (0.25119, 1e-5));
    requireOnlyTaps (y, { 0 }, 0.0);
    s.gainDb = 12;
    y = ir (s);
    CHECK_THAT (y[0], WithinAbs (3.98107, 1e-5));
    requireOnlyTaps (y, { 0 }, 0.0);
}

TEST_CASE ("10: tape mode impulse responses", "[vectors]")
{
    auto s = staticBase();
    s.tape = true;
    SECTION ("Tape, Mix 50, Depth 0 -> wet and dry both at 1 sample")
    {
        const auto y = ir (s);
        CHECK (y[0] == 0.0);
        CHECK_THAT (y[1], WithinAbs (1.41421, 1e-5));
    }
    SECTION ("Tape, Mix 100, Depth 50, Rate 0 -> tap at c = floor(0.5 * 105) = 52")
    {
        s.mixPct = 100;
        s.depthPct = 50;
        s.rateHz = 0;
        const auto y = ir (s);
        CHECK_THAT (y[52], WithinAbs (1.0, 1e-9));
        for (int i = 0; i < 52; ++i)
            REQUIRE (y[size_t (i)] == 0.0);
    }
}

TEST_CASE ("10: wet FIR filter taps", "[vectors][filter]")
{
    auto s = staticBase();
    s.mixPct = 100;
    s.feedbackPct = 0;
    s.delayMs = 1.0;
    s.filterOn = true;

    auto check = [&] (double fc, bool hp, std::initializer_list<double> half, double tol) {
        s.filterHz = fc;
        s.filterHighPass = hp;
        const auto y = ir (s);
        const auto model = ReferenceModel::kernel (fc, fs, hp);
        int i = 0;
        for (double v : half)
        {
            INFO ("fc " << fc << " hp " << hp << " tap " << 48 + i);
            CHECK_THAT (y[size_t (48 + i)], WithinAbs (v, tol));       // measured vector
            CHECK_THAT (y[size_t (56 - i)], WithinAbs (v, tol));       // symmetric
            CHECK_THAT (y[size_t (48 + i)], WithinAbs (model[size_t (i)], 1e-12)); // spec formula
            ++i;
        }
        requireOnlyTaps (y, { 48, 49, 50, 51, 52, 53, 54, 55, 56 }, 0.0);
    };

    check (1000.0, false, { 0.0174425, 0.0655388, 0.1292758, 0.1845239, 0.2064381 }, 1e-6);
    // Spec 6: measured taps match the formula to <= 2e-3 (reference parameter quantisation).
    check (1000.0, true, { -0.0035202, -0.0132267, -0.0260898, -0.0372397, 0.9582871 }, 2e-3);
    check (10000.0, true, { 0.0061158, 0.0245142, -0.0505566, -0.2764258, 0.5850583 }, 2e-3);
}

TEST_CASE ("6: high-pass DC gain at 1 kHz is -1.96 dB (not renormalised)", "[filter]")
{
    const auto h = highPassKernel (1000.0, fs, kaiserWindow (4.0));
    double sum = 0.0;
    for (double v : h)
        sum += v;
    CHECK_THAT (20.0 * std::log10 (sum), WithinAbs (-1.96, 0.01));
    double lp = 0.0;
    for (double v : lowPassKernel (3000.0, fs, kaiserWindow (4.0)))
        lp += v;
    CHECK_THAT (lp, WithinAbs (1.0, 1e-14));
}

TEST_CASE ("10: Mix 100, Fb 50, LP 1 kHz -> each echo filtered exactly once", "[vectors][filter]")
{
    auto s = staticBase();
    s.mixPct = 100;
    s.feedbackPct = 50;
    s.delayMs = 1.0;
    s.filterOn = true;
    s.filterHighPass = false;
    const auto y = ir (s);
    const auto h = ReferenceModel::kernel (1000.0, fs, false);
    const double g = model::wetCompensation (50, 1.0);
    for (int k = 0; k < 8; ++k)
        for (int j = 0; j < 9; ++j)
        {
            const int idx = 52 + 48 * k + (j - 4);
            // neighbouring echoes overlap only where the kernels' supports meet (none at 48-sample spacing)
            INFO ("echo " << k << " tap " << j);
            CHECK_THAT (y[size_t (idx)], WithinAbs (g * std::pow (0.5, k) * h[size_t (j)], 1e-12));
        }
}
