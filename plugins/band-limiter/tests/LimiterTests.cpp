// Static gain, ceiling and attenuation distribution: spec r1 sections 5, 6, 9, 12.
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <string>

#include "SpecTables.h"
#include "TestHelpers.h"

using namespace helpers;

namespace
{
constexpr double lsb24 = 1.0 / 8388608.0;

struct Case
{
    const char* name;
    double t;
    std::array<double, 5> priority;
    std::array<double, 5> gain;
    double sep;
    Behaviour behaviour;
    double l1, l2; // tone levels
    double f2;
};

const Case twoToneCases[] = {
    { "tt_T-12", -12, {}, {}, 1, Behaviour::Adaptive, -6, -6, 2000 },
    { "tt_T-6", -6, {}, {}, 1, Behaviour::Adaptive, -6, -6, 2000 },
    { "tt_T-18", -18, {}, {}, 1, Behaviour::Adaptive, -6, -6, 2000 },
    { "tt_T-12_Manual", -12, {}, {}, 1, Behaviour::Manual, -6, -6, 2000 },
    { "tt_T-12_p2_-12", -12, { 0, -12 }, {}, 1, Behaviour::Adaptive, -6, -6, 2000 },
    { "tt_T-12_p2_-6", -12, { 0, -6 }, {}, 1, Behaviour::Adaptive, -6, -6, 2000 },
    { "tt_T-12_p2_3", -12, { 0, 3 }, {}, 1, Behaviour::Adaptive, -6, -6, 2000 },
    { "tt_T-12_p2p6", -12, { 0, 6 }, {}, 1, Behaviour::Adaptive, -6, -6, 2000 },
    { "tt_T-12_p2_12", -12, { 0, 12 }, {}, 1, Behaviour::Adaptive, -6, -6, 2000 },
    { "tt_T-12_p4p6", -12, { 0, 0, 0, 6 }, {}, 1, Behaviour::Adaptive, -6, -6, 2000 },
    { "tt_T-12_p2p12_p4m12", -12, { 0, 12, 0, -12 }, {}, 1, Behaviour::Adaptive, -6, -6, 2000 },
    { "tt_T-12_p1p12", -12, { 12 }, {}, 1, Behaviour::Adaptive, -6, -6, 2000 },
    { "tt_T-12_sep0.0", -12, {}, {}, 0.0, Behaviour::Adaptive, -6, -6, 2000 },
    { "tt_T-12_sep0.25", -12, {}, {}, 0.25, Behaviour::Adaptive, -6, -6, 2000 },
    { "tt_T-12_sep0.5", -12, {}, {}, 0.5, Behaviour::Adaptive, -6, -6, 2000 },
    { "tt_T-12_sep0.75", -12, {}, {}, 0.75, Behaviour::Adaptive, -6, -6, 2000 },
    { "tt_T-12_g2p6", -12, {}, { 0, 6 }, 1, Behaviour::Adaptive, -6, -6, 2000 },
    { "tt_T-12_g4_-6", -12, {}, { 0, 0, 0, -6 }, 1, Behaviour::Adaptive, -6, -6, 2000 },
    { "tt_T-12_g4_6", -12, {}, { 0, 0, 0, 6 }, 1, Behaviour::Adaptive, -6, -6, 2000 },
    { "ttL_T-12", -12, {}, {}, 1, Behaviour::Adaptive, -6, -12, 2000 },
    { "ttL2_T-12", -12, {}, {}, 1, Behaviour::Adaptive, -12, -6, 2000 },
    { "tt_same_T-12", -12, {}, {}, 1, Behaviour::Adaptive, -6, -6, 200 },
};

const spec_tables::TwoTone& lookup (const std::string& name)
{
    for (const auto& row : spec_tables::twoTone)
        if (name == row.name)
            return row;
    FAIL ("missing table row " << name);
    return spec_tables::twoTone[0];
}

Settings fromCase (const Case& c)
{
    auto s = neutral();
    s.thresholdDb = c.t;
    s.priority = c.priority;
    s.gainDb = c.gain;
    s.separation = c.sep;
    s.behaviour = c.behaviour;
    return s;
}

struct Levels
{
    double a, b, pk;
};

Levels measureTwoTone (Variant v, double fs, const Settings& s, double f1, double l1, double f2, double l2)
{
    Rig rig (v, fs, s);
    const int n = int (fs * 3.0), len = int (fs * 0.5);
    const auto y = rig.run (tones ({ { f1, l1 }, { f2, l2 } }, fs, n));
    return { linToDb (toneAmplitude (y, f1, fs, n - len, len)), linToDb (toneAmplitude (y, f2, fs, n - len, len)),
             linToDb (peak (y, n - len, len)) };
}
} // namespace

TEST_CASE ("Two-tone distribution at 48 kHz (table +-1 dB)", "[distribution]")
{
    for (const auto& c : twoToneCases)
    {
        const auto& row = lookup (c.name);
        const auto r = measureTwoTone (Variant::LinearMulti, 48000.0, fromCase (c), 100.0, c.l1, c.f2, c.l2);
        INFO (c.name << ": " << r.a << " / " << r.b << " peak " << r.pk << " expected " << row.out1 << " / " << row.out2 << " peak " << row.peak);
        CHECK (std::abs (r.a - row.out1) <= 1.0);
        CHECK (std::abs (r.b - row.out2) <= 1.0);
        CHECK (std::abs (r.pk - row.peak) <= 0.35);
    }
}

TEST_CASE ("Two-tone distribution elsewhere (spec 12: +-1.5 dB)", "[distribution]")
{
    const double fs = GENERATE (44100.0, 96000.0);
    for (const char* name : { "tt_T-12", "tt_T-12_p2p6", "tt_T-12_sep0.5" })
        for (const auto& c : twoToneCases)
            if (std::string (c.name) == name)
            {
                const auto& row = lookup (c.name);
                const auto r = measureTwoTone (Variant::LinearMulti, fs, fromCase (c), 100.0, c.l1, c.f2, c.l2);
                INFO (c.name << " fs " << fs << ": " << r.a << " / " << r.b);
                CHECK (std::abs (r.a - row.out1) <= 1.5);
                CHECK (std::abs (r.b - row.out2) <= 1.5);
            }
}

TEST_CASE ("Low-latency two-tone rows", "[distribution][ll]")
{
    // Table rows flagged LOW-LATENCY are the last three of the CSV.
    const auto& base = spec_tables::twoTone[std::size (spec_tables::twoTone) - 3];
    const auto& prio = spec_tables::twoTone[std::size (spec_tables::twoTone) - 2];
    const auto& sep = spec_tables::twoTone[std::size (spec_tables::twoTone) - 1];

    auto s = neutral();
    s.thresholdDb = -12.0;
    auto r = measureTwoTone (Variant::LowLatencyMulti, 48000.0, s, 100.0, -6.0, 2000.0, -6.0);
    CHECK (std::abs (r.a - base.out1) <= 1.0);
    CHECK (std::abs (r.b - base.out2) <= 1.0);
    CHECK (std::abs (r.pk - base.peak) <= 0.35);

    s.priority[1] = 6.0;
    r = measureTwoTone (Variant::LowLatencyMulti, 48000.0, s, 100.0, -6.0, 2000.0, -6.0);
    INFO ("prio " << r.a << " / " << r.b);
    CHECK (std::abs (r.a - prio.out1) <= 1.0);
    CHECK (std::abs (r.b - prio.out2) <= 1.0);

    s.priority[1] = 0.0;
    s.separation = 0.0;
    r = measureTwoTone (Variant::LowLatencyMulti, 48000.0, s, 100.0, -6.0, 2000.0, -6.0);
    INFO ("sep " << r.a << " / " << r.b);
    CHECK (std::abs (r.a - sep.out1) <= 1.0);
    CHECK (std::abs (r.b - sep.out2) <= 1.0);
}

TEST_CASE ("Five-tone distribution (table +-1 dB)", "[distribution]")
{
    const double fs = 48000.0;
    const double freqs[5] = { 40, 160, 640, 2560, 10240 };
    for (const auto& row : spec_tables::multiTone)
    {
        auto s = neutral();
        s.thresholdDb = -12.0;
        if (std::string (row.name).find ("p3p12") != std::string::npos)
            s.priority[2] = 12.0;
        if (std::string (row.name).find ("sep0") != std::string::npos)
            s.separation = 0.0;
        Rig rig (Variant::LinearMulti, fs, s);
        const int n = int (fs * 3.0), len = int (fs * 1.0);
        const auto y = rig.run (tones ({ { 40, -12 }, { 160, -12 }, { 640, -12 }, { 2560, -12 }, { 10240, -12 } }, fs, n));
        for (int i = 0; i < 5; ++i)
        {
            const double l = linToDb (toneAmplitude (y, freqs[i], fs, n - len, len));
            INFO (row.name << " tone " << freqs[i] << ": " << l << " expected " << row.out[i]);
            CHECK (std::abs (l - row.out[i]) <= 1.0);
        }
        CHECK (peak (y, n - len, len) <= 1.0 + lsb24);
    }
}

TEST_CASE ("Below the limiting point: y = x * Gain * (C - T) makeup", "[static]")
{
    const double fs = GENERATE (44100.0, 48000.0, 96000.0);
    const auto variant = GENERATE (Variant::LinearMulti, Variant::LowLatencyMulti);
    auto s = neutral();
    s.thresholdDb = -12.0;
    s.ceilingDb = -3.0;
    Rig rig (variant, fs, s);
    const int n = int (fs * 0.8), len = int (fs * 0.2);
    const auto y = rig.run (tones ({ { 700.0, -24.0 } }, fs, n));
    CHECK (std::abs (linToDb (toneAmplitude (y, 700.0, fs, n - len, len)) - (-24.0 + 12.0 - 3.0)) < 0.05);

    auto g = neutral();
    g.gainDb[4] = 6.0;
    Rig boosted (variant, fs, g);
    const auto y2 = boosted.run (tones ({ { 16000.0 / 44100.0 * fs * 0.5, -30.0 } }, fs, n));
    const double f = 16000.0 / 44100.0 * fs * 0.5;
    CHECK (std::abs (linToDb (toneAmplitude (y2, f, fs, n - len, len)) - (-30.0 + 6.0)) < 1.2);
}

TEST_CASE ("Ceiling is never exceeded", "[ceiling]")
{
    const double fs = GENERATE (44100.0, 48000.0, 96000.0);
    const auto variant = GENERATE (Variant::LinearMulti, Variant::LowLatencyMulti);
    const double ceiling = GENERATE (0.0, -3.0, -12.0);
    auto s = neutral();
    s.thresholdDb = -18.0;
    s.ceilingDb = ceiling;
    const double limit = dbToLin (ceiling) + 0.51 * lsb24;

    craft::mastering_limiter::dsp::Random rng (11);
    std::vector<double> x (size_t (fs * 2.0));
    for (size_t i = 0; i < x.size(); ++i)
    {
        const double t = double (i) / fs;
        double v = 0.0;
        if (t < 0.5)
            v = 2.0 * rng.uniformCentred();                   // full-scale noise
        else if (t < 1.0)
            v = (std::fmod (t, 0.1) < 0.002 ? 1.0 : 0.0);     // clicks
        else if (t < 1.5)
            v = (std::fmod (t, 0.2) < 0.1 ? 1.0 : 0.05) * std::sin (twoPi * 997.0 * t); // tone steps
        else
            v = 0.5 * std::sin (twoPi * 100.0 * t) + 0.5 * std::sin (twoPi * 2000.0 * t);
        x[i] = v;
    }
    Rig rig (variant, fs, s);
    const auto y = rig.run (x);
    CHECK (peak (y, 0, int (y.size())) <= limit);
}

TEST_CASE ("No true-peak processing: fs/4 sine reaches exactly 0 dBFS", "[ceiling]")
{
    const double fs = 48000.0;
    auto s = neutral();
    s.thresholdDb = -6.0;
    Rig rig (Variant::LinearMulti, fs, s);
    const int n = 96000;
    std::vector<double> x (size_t (n), 0.0);
    const double a = dbToLin (-3.0);
    for (int i = 0; i < n; ++i)
        x[size_t (i)] = a * std::sin (twoPi * 0.25 * double (i) + 0.7853981633974483) * std::sqrt (2.0);
    const auto y = rig.run (x);
    CHECK (std::abs (linToDb (peak (y, n / 2, n / 2))) < 0.05);
}

TEST_CASE ("Profile map and profile distribution", "[profiles]")
{
    CHECK (param::profileFromNorm (0.0) == 1);
    CHECK (param::profileFromNorm (0.0625) == 2);
    CHECK (param::profileFromNorm (0.1875) == 3);
    CHECK (param::profileFromNorm (0.3125) == 4);
    CHECK (param::profileFromNorm (0.4375) == 5);
    CHECK (param::profileFromNorm (0.5625) == 6);
    CHECK (param::profileFromNorm (0.6875) == 7);
    CHECK (param::profileFromNorm (0.8125) == 8);
    CHECK (param::profileFromNorm (0.9375) == 9);
    CHECK (param::profileFromNorm (1.0) == 9);

    for (const auto& row : spec_tables::profiles)
    {
        auto s = neutral();
        s.thresholdDb = -12.0;
        applyProfile (s, row.index, 1.0);
        const auto r = measureTwoTone (Variant::LinearProfile, 48000.0, s, 100.0, -6.0, 2000.0, -6.0);
        INFO ("profile " << row.index << ": " << r.a << " / " << r.b << " expected " << row.out100 << " / " << row.out2k);
        CHECK (std::abs (r.a - row.out100) <= 1.0);
        CHECK (std::abs (r.b - row.out2k) <= 1.0);
    }

    // Profile 1 equals the Multi defaults exactly.
    Settings p1, multi;
    applyProfile (p1, 1, 1.0);
    CHECK (p1.priority == multi.priority);
    CHECK (p1.separation == multi.separation);
    CHECK (p1.releaseMs == multi.releaseMs);
    CHECK (p1.crossoverHz == multi.crossoverHz);
    CHECK (p1.behaviour == multi.behaviour);
}
