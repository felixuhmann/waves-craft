// Adaptive Release: spec r1 section 5 table (tolerance spec 10: +-25 %, t99 of the 5 ms row +-35 %).
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "TestHelpers.h"

using namespace mltest;
using Catch::Approx;

namespace
{
struct Row
{
    double overloadMs, depthDb, t63, t90, t99;
};

constexpr Row table[] = {
    { 5.0, 12.0, 0.96, 1.35, 377.0 },    { 20.0, 12.0, 1.10, 126.0, 626.0 }, { 100.0, 12.0, 111.0, 392.0, 892.0 },
    { 500.0, 12.0, 217.0, 498.0, 998.0 }, { 2000.0, 12.0, 219.0, 501.0, 1000.0 }, { 500.0, 6.0, 216.0, 498.0, 998.0 },
    { 500.0, 24.0, 222.0, 498.0, 998.0 },
};

struct Recovery
{
    double t63, t90, t99;
    bool monotonic;
};

Recovery measure (double fs, const Row& row, double releaseMs)
{
    const double thresholdDb = -12.0, T = dbToLin (thresholdDb);
    const size_t pre = size_t (0.3 * fs), over = size_t (std::lround (row.overloadMs * 1e-3 * fs)), post = size_t (2.0 * fs);
    std::vector<double> peaks (pre + over + post, 0.05);
    for (size_t i = pre; i < pre + over; ++i)
        peaks[i] = T * dbToLin (row.depthDb);
    const auto g = gainTrajectory (fs, peaks, thresholdDb, releaseMs, true);
    const size_t end = pre + over;
    const double d0 = 1.0 - dbToLin (-row.depthDb);
    Recovery r {};
    r.t63 = recoveryTime (g, end, d0, std::exp (-1.0), fs) * 1e3;
    r.t90 = recoveryTime (g, end, d0, 0.1, fs) * 1e3;
    r.t99 = recoveryTime (g, end, d0, 0.01, fs) * 1e3;
    r.monotonic = true;
    for (size_t i = end + 1; i < g.size(); ++i)
        if (g[i] < g[i - 1] - 1e-15)
            r.monotonic = false;
    return r;
}
} // namespace

TEST_CASE ("Adaptive Release recovery table (spec 5)")
{
    const double fs = GENERATE (44100.0, 48000.0, 96000.0);
    for (const auto& row : table)
    {
        const auto r = measure (fs, row, 1.0);
        CAPTURE (fs, row.overloadMs, row.depthDb, r.t63, r.t90, r.t99);
        CHECK (r.t63 == Approx (row.t63).epsilon (0.25));
        CHECK (r.t90 == Approx (row.t90).epsilon (0.25));
        CHECK (r.t99 == Approx (row.t99).epsilon (row.overloadMs == 5.0 ? 0.35 : 0.25));
        CHECK (r.monotonic);
    }
}

TEST_CASE ("Adaptive Release ignores the Release control (spec 2 note, D2)")
{
    const double fs = 48000.0;
    std::vector<double> x (48000);
    for (size_t i = 0; i < x.size(); ++i)
        x[i] = (0.2 + 0.8 * ((i / 3000) % 2)) * std::sin (0.07 * double (i));
    std::vector<double> ref;
    for (double rel : { 3.16, 31.6, 316.0 })
    {
        const auto y = runMono (fs, plainSettings (-10.0, -0.5, rel, true), x);
        if (ref.empty())
            ref = y;
        else
            for (size_t i = 0; i < y.size(); ++i)
                REQUIRE (y[i] == ref[i]);
    }
    // ... while the fixed-release mode does depend on it
    const auto a = runMono (fs, plainSettings (-10.0, -0.5, 3.16, false), x);
    const auto b = runMono (fs, plainSettings (-10.0, -0.5, 316.0, false), x);
    CHECK (a != b);
}

TEST_CASE ("Adaptive Release: attack, look-ahead and ceiling enforcement unchanged (spec 5)")
{
    const double fs = 48000.0;
    const size_t n0 = 5000;
    std::vector<double> peaks (8000, 0.01);
    peaks[n0] = 1.0;
    const auto fixed = gainTrajectory (fs, peaks, -12.0, 10.0, false);
    const auto adaptive = gainTrajectory (fs, peaks, -12.0, 10.0, true);
    for (size_t i = 0; i < n0; ++i)
        REQUIRE (adaptive[i] == Approx (fixed[i]).margin (1e-12));
    CHECK (adaptive[n0] == Approx (dbToLin (-12.0)).epsilon (1e-12));
}
