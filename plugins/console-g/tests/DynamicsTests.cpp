// Spec sections 5, 6, 9.2, 9.3, 10: compressor and expander / gate.
#include <catch2/catch_test_macros.hpp>

#include <cstdio>
#include <random>
#include <sstream>

#include "TestHelpers.h"

using namespace testing;
using cg::TaperId;

namespace
{
constexpr double fsTest = 48000.0;

// Tolerance of spec 10: 0.5 dB, 1.0 dB within +-3 dB of the knee (threshold reference T - 18 dBFS sine RMS,
// i.e. about T - 15 dBFS peak; both conventions are covered by a slightly wider window).
double compTolerance (double T, double level)
{
    return std::abs (level - (T - 16.5)) <= 4.5 ? 1.0 : 0.5;
}

std::string row (const std::vector<double>& v)
{
    std::ostringstream o;
    o.precision (3);
    for (double x : v)
        o << x << " ";
    return o.str();
}
} // namespace

TEST_CASE ("compressor: static curves match comp_static.csv (all thresholds x ratios)", "[dynamics][static]")
{
    const auto& tab = cg::CompressorTable::get();
    double worst = 0.0, sumSq = 0.0;
    int n = 0;
    for (size_t ti = 0; ti < tab.thresholdGrid().size(); ++ti)
        for (size_t ri = 0; ri < tab.ratioGrid().size(); ++ri)
        {
            const double T = tab.thresholdGrid()[ti], R = tab.ratioGrid()[ri];
            const auto g = staircase (comp (T, R), fsTest);
            for (int k = 0; k < cg::CompressorTable::numLevels; ++k)
            {
                const double L = -54.0 + 2.0 * k, ref = tab.tableValue (ti, ri, k);
                const double err = std::abs (g[size_t (k)] - ref);
                worst = std::max (worst, err);
                sumSq += err * err;
                ++n;
                INFO ("T " << T << " R " << R << " in " << L << ": " << g[size_t (k)] << " vs " << ref);
                CHECK (err <= compTolerance (T, L));
            }
        }
    WARN ("compressor static: worst " << worst << " dB, rms " << std::sqrt (sumSq / n) << " dB over " << n << " points");
}

TEST_CASE ("compressor: spec 9.2 timing-dependent rows (fast attack, release 4 s) and input trim", "[dynamics][static]")
{
    const double levels[] = { -54, -30, -22, -14, -6, 0 };
    struct Vec { const char* name; cg::Settings s; double db[6]; };
    auto trim = comp (-10, 4);
    trim.inputTrim = rOf (TaperId::inputTrim, 6.3);
    Vec vecs[] = {
        { "fast", comp (-10, 4, true), { 2.98, 1.84, -2.15, -7.10, -12.37, -16.51 } },
        { "release 4 s", comp (-10, 4, false, 4.0), { 2.98, 2.14, -1.56, -6.32, -11.61, -15.66 } },
        { "trim +6.3", trim, { 9.28, 6.17, 1.98, -2.93, -8.08, -12.16 } },
    };
    for (auto& v : vecs)
    {
        const auto g = staircase (v.s, fsTest);
        WARN (v.name << ": " << row (g));
        for (int k = 0; k < 6; ++k)
        {
            const double L = levels[k];
            const double got = g[size_t ((L + 54) / 2)];
            INFO (v.name << " at " << L << ": " << got << " vs " << v.db[k]);
            CHECK (std::abs (got - v.db[k]) <= compTolerance (-10, L) + (v.s.inputTrim != comp (-10, 4).inputTrim ? 0.0 : 0.0));
        }
    }
}

namespace
{
// Burst -40 -> -4 -> -40 dBFS (1 kHz); times (ms) to 50/63/90 % of the gain change after each edge.
struct Timing { double att[3], rel[3]; };
Timing burstTiming (const cg::Settings& s)
{
    auto e = makeEngine (fsTest, s);
    const size_t seg = size_t (fsTest * 2.0);
    std::vector<double> x (3 * seg);
    for (size_t i = 0; i < x.size(); ++i)
    {
        const double a = (i >= seg && i < 2 * seg) ? cg::dbToGain (-4.0) : cg::dbToGain (-40.0);
        x[i] = a * std::sin (2.0 * cg::pi * 1000.0 * double (i) / fsTest);
    }
    auto y = x;
    run (e, y);
    // gain envelope per 1 ms window (one period)
    const size_t w = size_t (fsTest / 1000.0);
    std::vector<double> g;
    for (size_t i = 0; i + w <= x.size(); i += w)
        g.push_back (cg::gainToDb (rms (y, i, i + w) / rms (x, i, i + w)));
    Timing t {};
    auto measure = [&] (size_t start, double* out) {
        const size_t s0 = start / w;
        const double g0 = g[s0 - 2], g1 = g[s0 + seg / w - 3];
        const double fr[3] = { 0.5, 0.63, 0.9 };
        for (int k = 0; k < 3; ++k)
        {
            const double lvl = g0 + fr[k] * (g1 - g0);
            size_t j = s0;
            while (j < g.size() && ((g1 < g0) ? g[j] > lvl : g[j] < lvl))
                ++j;
            out[k] = double (j - s0) + 0.5;
        }
    };
    measure (seg, t.att);
    measure (2 * seg, t.rel);
    return t;
}
} // namespace

TEST_CASE ("compressor: attack / release timing (spec 5 table) within 25 %", "[dynamics][timing]")
{
    struct Ref { bool fast; double release; double att[3], rel[3]; } refs[] = {
        { false, 0.4, { 31, 45, 85 }, { 87, 126, 334 } },
        { false, 0.1, { 34, 48, 94 }, { 61, 87, 220 } },
        { false, 1.2, { 34, 47, 93 }, { 201, 345, 1197 } },
        { true, 0.4, { 1.5, 2.5, 4.5 }, { 43, 66, 268 } },
        { true, 0.1, { 1.5, 2.5, 4.5 }, { 22, 30, 72 } },
    };
    for (const auto& r : refs)
    {
        const auto t = burstTiming (comp (-10, 4, r.fast, r.release));
        WARN ((r.fast ? "fast" : "auto") << " rel " << r.release << ": attack " << t.att[0] << "/" << t.att[1] << "/" << t.att[2]
                                          << " release " << t.rel[0] << "/" << t.rel[1] << "/" << t.rel[2]);
        for (int k = 0; k < 3; ++k)
        {
            INFO ((r.fast ? "fast" : "auto") << " release " << r.release << " k " << k);
            // attack of the fast setting is resolved to the 1 ms analysis window
            const double attTol = r.fast ? 1.5 : 0.25 * r.att[k];
            CHECK (std::abs (t.att[k] - r.att[k]) <= attTol);
            CHECK (std::abs (t.rel[k] - r.rel[k]) <= 0.25 * r.rel[k]);
        }
    }
}

TEST_CASE ("compressor: peak-sensitive detector (noise vs sine) and stereo link (spec 9.3)", "[dynamics]")
{
    auto measure = [] (const cg::Settings& s, bool noise) {
        auto e = makeEngine (fsTest, s);
        std::mt19937 rng (3);
        std::normal_distribution<double> nd (0.0, 0.1); // -20 dBFS RMS
        std::vector<double> x (size_t (fsTest * 3.0));
        for (size_t i = 0; i < x.size(); ++i)
            x[i] = noise ? nd (rng) : 0.1 * std::sqrt (2.0) * std::sin (2.0 * cg::pi * 1000.0 * double (i) / fsTest);
        auto y = x;
        run (e, y);
        return cg::gainToDb (rms (y, x.size() / 2, x.size()) / rms (x, x.size() / 2, x.size()));
    };
    struct Ref { cg::Settings s; double noise, sine; const char* name; } refs[] = {
        { comp (-10, 4), -4.28, -3.62, "auto" },
        { comp (-10, 4, true), -6.15, -5.20, "fast" },
        { comp (-20, std::numeric_limits<double>::infinity()), -13.18, -11.82, "T-20 Inf" },
    };
    for (const auto& r : refs)
    {
        const double n = measure (r.s, true), s = measure (r.s, false);
        WARN (r.name << ": noise " << n << " sine " << s);
        CHECK (std::abs (s - r.sine) < 0.5);
        CHECK (std::abs (n - r.noise) < 0.6);
        CHECK (n < s); // noise receives more reduction than a sine of equal RMS
    }

    // link: L bursts to -4 dBFS, R steady 2.5 kHz at -40 dBFS -> R receives the same reduction (-11.57 dB)
    auto e = makeEngine (fsTest, comp (-10, 4));
    const size_t n = size_t (fsTest * 2.0);
    std::vector<double> l (n), r (n);
    for (size_t i = 0; i < n; ++i)
    {
        l[i] = cg::dbToGain (-4.0) * std::sin (2.0 * cg::pi * 1000.0 * double (i) / fsTest);
        r[i] = cg::dbToGain (-40.0) * std::sin (2.0 * cg::pi * 2500.0 * double (i) / fsTest);
    }
    auto yl = l, yr = r;
    run (e, yl, &yr);
    const double gl = cg::gainToDb (rms (yl, n / 2, n) / rms (l, n / 2, n));
    const double gr = cg::gainToDb (rms (yr, n / 2, n) / rms (r, n / 2, n));
    WARN ("link: L " << gl << " R " << gr);
    CHECK (std::abs (gl - gr) < 0.05);
    CHECK (std::abs (gr - -11.57) < 1.0);
}

TEST_CASE ("expander / gate: static staircases match exp_static.csv (up and down)", "[dynamics][static]")
{
    const auto t = cg::parseCsv (cgt::exp_static, cgt::exp_static_count);
    double worst = 0.0;
    int n = 0, nBad = 0;
    for (const auto& r : t.rows)
    {
        const bool gate = r[0] == "gate", down = r[3] == "down";
        const double T = cg::parseCell (r[1]), range = cg::parseCell (r[2]);
        const auto g = staircase (expander (cg::Settings::defaults(), T, range, gate), fsTest, -54.0, 0.0, down);
        const double onset = cg::ExpanderModel::get().onsetDb (T);
        std::vector<double> ref;
        for (size_t k = 4; k < r.size(); ++k)
            ref.push_back (cg::parseCell (r[k]));
        for (size_t k = 0; k < ref.size(); ++k)
        {
            const double L = -54.0 + 2.0 * double (k);
            const double err = std::abs (g[k] - ref[k]);
            // knee: within 3 dB of the onset / gate thresholds, of the floor corner, and the first step after silence
            const double floorDb = gate ? cg::ExpanderModel::get().gateFloorDb (range)
                                        : std::max (0.0, cg::ExpanderModel::get().expanderDepthMax (range)
                                                             - cg::ExpanderModel::get().expanderOffset());
            const bool nearKnee = std::abs (L - onset) <= 4.0 || std::abs (-ref[k] - floorDb) <= 4.0 || (! down && k <= 1)
                                  || (gate && std::abs (ref[k]) > 1.0 && std::abs (ref[k]) < floorDb - 1.0);
            const double tol = nearKnee ? 1.0 : 0.5;
            worst = std::max (worst, err);
            ++n;
            if (err > tol)
                ++nBad;
            INFO (r[0] << " T " << T << " range " << range << " " << r[3] << " at " << L << ": " << g[k] << " vs " << ref[k]
                        << "\n got " << row (g) << "\n ref " << row (ref));
            CHECK (err <= tol);
        }
    }
    WARN ("expander static: worst " << worst << " dB; " << nBad << " of " << n << " points outside tolerance");
}

TEST_CASE ("dynamics: spec 9.2 rows (compressor + expander sum, expander/gate vectors)", "[dynamics][static]")
{
    struct Vec { const char* name; cg::Settings s; double db[6]; };
    Vec vecs[] = {
        { "expander T-20 range 40", expander (cg::Settings::defaults(), -20, 40), { -17.04, 0.00, 0.00, 0.00, 0.01, 0.02 } },
        { "expander T0 range 10", expander (cg::Settings::defaults(), 0, 10), { -3.51, -6.94, 0.00, 0.00, 0.01, 0.02 } },
        { "gate T-20 range 40", expander (cg::Settings::defaults(), -20, 40, true), { -33.90, 0.00, 0.00, 0.00, 0.01, 0.02 } },
        { "comp + expander", expander (comp (-10, 4), -20, 20), { -10.05, 2.29, -0.90, -5.25, -10.36, -14.17 } },
    };
    const double levels[] = { -54, -30, -22, -14, -6, 0 };
    for (auto& v : vecs)
    {
        const auto g = staircase (v.s, fsTest);
        WARN (v.name << ": " << row (g));
        for (int k = 0; k < 6; ++k)
        {
            const double got = g[size_t ((levels[k] + 54) / 2)];
            INFO (v.name << " at " << levels[k] << ": " << got << " vs " << v.db[k]);
            CHECK (std::abs (got - v.db[k]) <= 1.0);
        }
    }
}

TEST_CASE ("expander floor is always finite; the strip never latches into silence (D7)", "[dynamics][d7]")
{
    auto e = makeEngine (fsTest);
    std::mt19937 rng (11);
    std::uniform_real_distribution<double> u (0.0, 1.0);
    const size_t block = 512;
    std::vector<double> l (block), r (block);
    size_t t = 0;
    for (int setting = 0; setting < 300; ++setting)
    {
        auto s = cg::Settings::defaults();
        s.expThreshold = u (rng);
        s.expRange = u (rng);
        s.expRelease = u (rng);
        s.expGate = u (rng) > 0.5;
        s.expFast = u (rng) > 0.5;
        s.compThreshold = u (rng);
        s.compRatio = u (rng);
        s.compFast = u (rng) > 0.5;
        s.dynPostEq = u (rng) > 0.7;
        s.filtersSidechain = u (rng) > 0.7;
        e.setSettings (s);
        const int blocks = 1 + int (u (rng) * 20);
        for (int b = 0; b < blocks; ++b)
        {
            const double kind = u (rng);
            for (size_t i = 0; i < block; ++i, ++t)
            {
                double v = 0.0;
                if (kind < 0.3) v = 0.0;                                         // digital silence
                else if (kind < 0.6) v = 1.0e-4 * std::sin (0.05 * double (t)); // very low level
                else v = (u (rng) * 2.0 - 1.0) * (kind < 0.8 ? 1.0 : 1.0e-30);
                l[i] = v;
                r[i] = -v;
            }
            double* p[2] = { l.data(), r.data() };
            e.process (p, 2, int (block));
            for (size_t i = 0; i < block; ++i)
                REQUIRE (std::isfinite (l[i]));
        }
    }
    // back to neutral settings: a -20 dBFS tone must come through at unity
    e.setSettings (cg::Settings::defaults());
    std::vector<double> x (static_cast<size_t> (fsTest)), y;
    for (size_t i = 0; i < x.size(); ++i)
        x[i] = 0.1 * std::sin (2.0 * cg::pi * 1000.0 * double (i) / fsTest);
    y = x;
    auto y2 = x;
    run (e, y, &y2);
    const double g = cg::gainToDb (rms (y, x.size() / 2, x.size()) / rms (x, x.size() / 2, x.size()));
    CHECK (std::abs (g) < 0.05);

    // deepest settings: the attenuation stays finite (gate range 40 floor -52.7 dB, expander range 40 ~ -49 dB)
    auto s = expander (cg::Settings::defaults(), 10, 40, true);
    auto gs = staircase (s, fsTest, -54.0, -40.0, true);
    for (double gdb : gs)
    {
        CHECK (std::isfinite (gdb));
        CHECK (gdb > -60.0);
    }
}
