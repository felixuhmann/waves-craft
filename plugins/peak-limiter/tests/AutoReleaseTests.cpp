// Spec r1 section 5 (Auto Release) and its tolerance in section 8 (+-20 %, monotonic recovery).
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "TestHelpers.h"

using namespace testutil;

namespace
{
struct Row
{
    double releaseMs, overloadMs, overDb, t63, t90, t99;
};

const Row table[] = {
    { 31.62, 5, 12, 2.75, 4.58, 6.25 },        { 31.62, 20, 12, 2.90, 5.40, 7.60 },
    { 31.62, 100, 12, 4.65, 12.12, 14.38 },    { 31.62, 500, 12, 26.60, 34.33, 36.60 },
    { 31.62, 2000, 12, 34.12, 41.88, 44.12 },  { 31.62, 500, 6, 13.98, 18.75, 20.29 },
    { 31.62, 500, 24, 44.81, 55.08, 57.92 },   { 316.23, 5, 12, 22.90, 48.21, 70.27 },
    { 316.23, 20, 12, 24.69, 61.08, 83.31 },   { 316.23, 100, 12, 49.44, 126.75, 148.98 },
    { 316.23, 500, 12, 263.58, 340.90, 363.12 }, { 316.23, 2000, 12, 335.54, 412.85, 435.08 },
    { 316.23, 500, 6, 134.98, 182.79, 197.35 }, { 316.23, 500, 24, 453.85, 556.46, 584.60 },
    { 1.00, 500, 12, 1.17, 1.65, 2.04 },
};

struct Recovery
{
    double t63, t90, t99;
    bool monotonic;
};

// DC step `overDb` above a -12 dB threshold for `overloadMs`, then a 0.01 carrier; recovery of the linear-gain
// deficit measured from the end of the overload (aligned time), like the section 5 measurement.
Recovery measure (const Row& row, double fs, bool autoRelease = true)
{
    const double thr = -12.0;
    const std::size_t over = std::size_t (std::llround (row.overloadMs * 1e-3 * fs));
    const std::size_t tail = std::size_t ((3.0 * row.releaseMs + 100.0) * 1e-3 * fs);
    std::vector<double> x (over + tail, 0.01);
    std::fill (x.begin(), x.begin() + std::ptrdiff_t (over), dbToLin (thr + row.overDb));

    Settings s;
    s.thresholdDb = thr;
    s.ceilingDb = thr; // makeup 1: the output / input ratio is the gain
    s.releaseMs = row.releaseMs;
    s.autoRelease = autoRelease;
    const auto g = dcGain (x, fs, s);
    const double gmin = dbToLin (-row.overDb);

    Recovery rec { -1.0, -1.0, -1.0, true };
    double prev = 0.0;
    for (std::size_t i = over; i < x.size(); ++i)
    {
        const double deficit = (1.0 - g[i]) / (1.0 - gmin);
        if (g[i] < prev - 1e-12)
            rec.monotonic = false;
        prev = g[i];
        const double t = double (i - over) / fs * 1e3; // first sample at or below the level
        if (rec.t63 < 0 && deficit <= 0.37) rec.t63 = t;
        if (rec.t90 < 0 && deficit <= 0.10) rec.t90 = t;
        if (rec.t99 < 0 && deficit <= 0.01) rec.t99 = t;
    }
    return rec;
}
} // namespace

TEST_CASE ("Auto Release recovery table within +-20 % (section 5/8)", "[auto]")
{
    double worst = 0.0;
    for (const auto& row : table)
    {
        const auto rec = measure (row, 48000.0);
        INFO ("Release " << row.releaseMs << " ms, overload " << row.overloadMs << " ms, " << row.overDb << " dB over: t63 "
                         << rec.t63 << " / " << row.t63 << ", t90 " << rec.t90 << " / " << row.t90 << ", t99 " << rec.t99
                         << " / " << row.t99);
        CHECK (rec.monotonic);
        for (auto [got, want] : { std::pair { rec.t63, row.t63 }, { rec.t90, row.t90 }, { rec.t99, row.t99 } })
        {
            REQUIRE (got > 0.0);
            CHECK (std::abs (got / want - 1.0) <= 0.20);
            worst = std::max (worst, std::abs (got / want - 1.0));
        }
    }
    WARN ("worst Auto Release table deviation: " << worst * 100.0 << " %");
}

TEST_CASE ("Auto Release behaves the same at 44.1 / 96 kHz", "[auto]")
{
    for (double fs : { 44100.0, 96000.0 })
        for (const auto& row : table)
        {
            if (row.releaseMs < 2.0)
                continue; // at 1 ms the (rate-dependent) look-ahead smoothing dominates
            const auto rec = measure (row, fs);
            INFO ("fs " << fs << " Release " << row.releaseMs << " overload " << row.overloadMs << " over " << row.overDb);
            CHECK (rec.monotonic);
            CHECK (std::abs (rec.t63 / row.t63 - 1.0) <= 0.25);
            CHECK (std::abs (rec.t90 / row.t90 - 1.0) <= 0.25);
            CHECK (std::abs (rec.t99 / row.t99 - 1.0) <= 0.25);
        }
}

TEST_CASE ("Auto Release: program dependence and Release scaling", "[auto]")
{
    // longer overloads recover more slowly; a longer Release recovers more slowly
    const auto shortOver = measure ({ 100.0, 5, 12, 0, 0, 0 }, 48000.0);
    const auto longOver = measure ({ 100.0, 1000, 12, 0, 0, 0 }, 48000.0);
    CHECK (longOver.t90 > 3.0 * shortOver.t90);
    const auto slowRel = measure ({ 300.0, 1000, 12, 0, 0, 0 }, 48000.0);
    CHECK (slowRel.t90 > 2.5 * longOver.t90);

    // attack and look-ahead are identical to the fixed mode
    std::vector<double> x (2000, 0.01);
    x[800] = 2.0;
    Settings s;
    s.thresholdDb = -12.0;
    s.ceilingDb = -12.0;
    s.releaseMs = 50.0;
    const auto fixedG = dcGain (x, 48000.0, s);
    s.autoRelease = true;
    const auto autoG = dcGain (x, 48000.0, s);
    for (std::size_t i = 0; i <= 800; ++i)
        REQUIRE (autoG[i] == fixedG[i]);
}

TEST_CASE ("Auto Release is click-free on programme material", "[auto]")
{
    const double fs = 48000.0;
    const auto in = programme (48000, true, 21, fs);
    Settings s;
    s.thresholdDb = -10.0;
    s.ceilingDb = -10.0;
    s.releaseMs = 31.62;
    s.autoRelease = true;
    // observe the gain through a tiny constant probe on the right channel (never triggers; linked gain)
    Signal probe;
    probe.l = in.l;
    probe.r.assign (in.size(), 1e-6);
    const auto out = runEngine (probe, fs, s);
    double maxStep = 0.0;
    for (std::size_t i = 64 + 1; i < out.size(); ++i) // the first L output samples are the silent pre-roll
        maxStep = std::max (maxStep, std::abs (out.r[i] - out.r[i - 1]) / 1e-6);
    // the gain never jumps by more than a few percent per sample (it is twice boxcar-smoothed)
    CHECK (maxStep < 0.05);
}
