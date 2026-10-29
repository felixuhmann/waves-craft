// Sections 6, 7 and 12: detector test vectors, attack / release / punch / sync trajectories, sample-rate
// independence. Tolerances of section 13.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cstdio>

#include "TestHelpers.h"

using namespace testutil;
using Catch::Approx;

namespace
{
const double attackMs[12] = { 0, 1, 2, 3, 5, 8, 12, 20, 30, 50, 80, 150 };
const double releaseMsPts[11] = { 0, 2, 5, 10, 20, 50, 100, 200, 400, 800, 1300 };

struct Row
{
    const char* name;
    Settings s;
    std::vector<double> ref;
};

Settings with (std::initializer_list<std::pair<const char*, double>> kv)
{
    Settings s = trajectorySettings();
    for (const auto& [k, v] : kv)
    {
        const std::string key (k);
        if (key == "A") s.attackMs = v;
        else if (key == "Rel") s.releaseMs = v;
        else if (key == "P") s.punch = v;
        else if (key == "HostNote")
        {
            s.sync = param::Sync::Host;
            s.hostBpm = 120.0;
            s.note = int (v);
        }
    }
    return s;
}

// Linear interpolation of a tabulated trajectory to every millisecond (for the RMS criterion).
double tableAt (const double* t, const std::vector<double>& v, double ms)
{
    size_t k = 0;
    while (k + 2 < v.size() && t[k + 1] <= ms)
        ++k;
    const double f = std::clamp ((ms - t[k]) / (t[k + 1] - t[k]), 0.0, 1.0);
    return v[k] + f * (v[k + 1] - v[k]);
}

void checkTrajectory (const Row& row, bool attack, double fs = 48000.0, const std::vector<double>* reference = nullptr,
                      double tol = 1.5, double earlyTol = 3.0, double rmsTol = 0.8)
{
    const auto tr = runTrajectory (fs, row.s);
    const double* times = attack ? attackMs : releaseMsPts;
    const size_t step = attack ? tr.up : tr.down;
    const auto& ref = reference != nullptr ? *reference : row.ref;
    std::string line;
    for (size_t i = 0; i < ref.size(); ++i)
    {
        const double v = tr.gainAt (step, times[i]);
        char buf[48];
        std::snprintf (buf, sizeof (buf), " %+.2f(%+.2f)", v, v - ref[i]);
        line += buf;
        INFO (row.name << (attack ? " attack" : " release") << " t=" << times[i] << " ms @ " << fs);
        CHECK (v == Approx (ref[i]).margin (times[i] <= 5.0 ? earlyTol : tol));
    }
    // RMS over the per-ms trajectory against the linearly interpolated table
    const double end = attack ? 150.0 : 1300.0;
    double sum = 0.0;
    int n = 0;
    for (double ms = 0.0; ms <= end; ms += 1.0, ++n)
    {
        const double e = tr.gainAt (step, ms) - tableAt (times, ref, ms);
        sum += e * e;
    }
    INFO (row.name << ":" << line);
    CHECK (std::sqrt (sum / n) <= rmsTol);
}
} // namespace

TEST_CASE ("Attack trajectories (section 7.4)", "[dynamics][attack]")
{
    const std::vector<Row> rows = {
        { "A0.5", with ({ { "A", 0.5 } }), { -4.10, -7.08, -7.56, -7.63, -7.64, -7.64, -7.64, -7.64, -7.64, -7.64, -7.64, -7.64 } },
        { "A2", with ({ { "A", 2.0 } }), { 1.91, -0.74, -2.85, -4.44, -6.32, -7.33, -7.59, -7.64, -7.64, -7.64, -7.64, -7.64 } },
        { "R4_T-24", with ({}), { 4.25, 3.34, 2.47, 1.64, 0.09, -1.91, -3.96, -6.29, -7.29, -7.62, -7.64, -7.64 } },
        { "A30", with ({ { "A", 30.0 } }), { 5.05, 4.83, 4.61, 4.39, 3.96, 3.33, 2.51, 0.99, -0.72, -3.42, -5.84, -7.45 } },
        { "A100", with ({ { "A", 100.0 } }), { 5.22, 5.16, 5.09, 5.02, 4.89, 4.69, 4.43, 3.92, 3.28, 2.08, 0.41, -2.74 } },
        { "P5", with ({ { "P", 5.0 } }), { 5.30, 5.30, 5.30, 5.30, 4.25, 1.64, -1.29, -5.07, -6.95, -7.60, -7.64, -7.64 } },
        { "P10", with ({ { "P", 10.0 } }), { 5.30, 5.30, 5.30, 5.30, 5.30, 5.30, 2.47, -3.02, -6.29, -7.55, -7.64, -7.64 } },
        { "P20", with ({ { "P", 20.0 } }), { 5.30, 5.30, 5.30, 5.30, 5.30, 5.30, 5.30, 4.25, -3.02, -7.29, -7.63, -7.64 } },
        { "P30", with ({ { "P", 30.0 } }), { 5.30, 5.30, 5.30, 5.30, 5.30, 5.30, 5.30, 5.30, 4.25, -6.29, -7.62, -7.64 } },
    };
    for (const auto& r : rows)
        checkTrajectory (r, true);
}

TEST_CASE ("Release trajectories incl. host sync (section 7.4)", "[dynamics][release]")
{
    const std::vector<Row> rows = {
        { "Rel3", with ({ { "Rel", 3.0 } }), { -10.38, -4.76, -1.41, 0.49, 1.27, 1.36, 1.36, 1.36, 1.36, 1.36, 1.36 } },
        { "Rel30", with ({ { "Rel", 30.0 } }), { -7.98, -6.92, -5.56, -3.77, -1.24, 3.09, 4.81, 4.81, 4.81, 4.81, 4.81 } },
        { "R4_T-24", with ({}), { -7.58, -7.16, -6.66, -5.92, -4.64, -1.84, 1.40, 5.30, 5.30, 5.30, 5.30 } },
        { "Rel300", with ({ { "Rel", 300.0 } }), { -7.45, -7.21, -6.96, -6.68, -6.17, -4.82, -3.05, -0.46, 4.21, 5.45, 5.45 } },
        { "Rel1000", with ({ { "Rel", 1000.0 } }), { -7.42, -7.35, -7.30, -7.21, -7.04, -6.54, -5.79, -4.50, -2.49, 0.37, 3.69 } },
        { "Rel3000", with ({ { "Rel", 3000.0 } }), { -7.42, -7.41, -7.39, -7.36, -7.30, -7.12, -6.84, -6.31, -5.36, -3.78, -2.21 } },
        { "Host_1-8", with ({ { "HostNote", 20 } }), { -7.46, -7.21, -6.94, -6.61, -6.00, -4.45, -2.47, 0.36, 5.44, 5.44, 5.44 } },
        { "Host_1-4", with ({ { "HostNote", 27 } }), { -7.43, -7.22, -7.01, -6.82, -6.50, -5.61, -4.36, -2.39, 0.44, 5.48, 5.48 } },
    };
    for (const auto& r : rows)
        checkTrajectory (r, false);
}

TEST_CASE ("Punch delays both the gain-reduction onset and the release start by P ms (+-0.5 ms)", "[dynamics][punch]")
{
    const double fs = 48000.0;
    // onset / release start: first sample where the gain-reduction gain departs from its resting value
    auto firstChange = [&] (const Settings& s, bool attack) {
        const size_t n = size_t (1.0 * fs);
        std::vector<double> x (n);
        const size_t step = size_t (0.3 * fs), stop = size_t (0.6 * fs);
        for (size_t i = 0; i < n; ++i)
            x[i] = ((i >= step && i < stop) ? 0.5 : 0.01) * std::sin (2.0 * pi * 1000.0 * double (i) / fs);
        PunchEngine e;
        e.prepare (fs, 1);
        e.setSettings (s);
        e.reset();
        const int L = e.getLatencySamples();
        // track the applied gain via the meter: process sample by sample
        double prev = 1.0;
        size_t found = 0;
        for (size_t i = 0; i < n + size_t (L); ++i)
        {
            double v = i < n ? x[i] : 0.0;
            double* p = &v;
            e.resetMeters();
            e.process (&p, 1);
            const double g = e.getMinGain();
            const size_t aligned = i >= size_t (L) ? i - size_t (L) : 0;
            if (attack && aligned < stop && g < 0.999 && found == 0 && i >= size_t (L))
                found = aligned;
            if (! attack && aligned > stop && g > prev + 1.0e-6 && found == 0)
                found = aligned;
            prev = g;
        }
        return (double (found) - double (attack ? step : stop)) * 1000.0 / fs;
    };
    Settings base = trajectorySettings();
    base.attackMs = 0.5;
    const double onset0 = firstChange (base, true), rel0 = firstChange (base, false);
    for (double P : { 5.0, 10.0, 20.0, 30.0 })
    {
        Settings s = base;
        s.punch = P;
        INFO ("Punch " << P);
        CHECK (firstChange (s, true) - onset0 == Approx (P).margin (0.5));
        CHECK (firstChange (s, false) - rel0 == Approx (P).margin (0.5));
    }
    // the look-ahead: with Punch 0 the reduction starts before the step (about H samples early)
    CHECK (onset0 < 0.0);
    CHECK (onset0 > -1.5);
}

TEST_CASE ("Equal-RMS noise vs sine: extra reduction for noise (section 6, +-0.5 dB)", "[dynamics][detector]")
{
    const double fs = 48000.0;
    const size_t n = size_t (6.0 * fs);
    const double level = dbToLin (-15.0);
    const auto sineSig = sine (fs, 1000.0, level * std::sqrt (2.0), n);
    const auto noiseSig = gaussianNoise (n, level, 12345u);
    struct Case
    {
        const char* name;
        Settings s;
        double ref;
    };
    const Case cases[] = {
        { "A0.5", with ({ { "A", 0.5 } }), -4.51 },   { "A2", with ({ { "A", 2.0 } }), -4.11 },
        { "R4_T-24", with ({}), -3.62 },               { "A30", with ({ { "A", 30.0 } }), -3.02 },
        { "A100", with ({ { "A", 100.0 } }), -2.55 },  { "Rel3", with ({ { "Rel", 3.0 } }), -2.72 },
        { "Rel30", with ({ { "Rel", 30.0 } }), -3.23 }, { "Rel300", with ({ { "Rel", 300.0 } }), -3.94 },
        { "Rel1000", with ({ { "Rel", 1000.0 } }), -4.32 },
    };
    const size_t a = size_t (1.0 * fs);
    for (const auto& c : cases)
    {
        const auto ys = runAligned (stereo (sineSig), fs, c.s).l;
        const auto yn = runAligned (stereo (noiseSig), fs, c.s).l;
        const double gs = linToDb (rms (ys, a, n) / rms (sineSig, a, n));
        const double gn = linToDb (rms (yn, a, n) / rms (noiseSig, a, n));
        INFO (c.name << ": noise - sine = " << gn - gs);
        CHECK (gn - gs == Approx (c.ref).margin (0.5));
    }
}

TEST_CASE ("Low-frequency ripple: H3 of a -6 dBFS sine (section 6, +-4 dB)", "[dynamics][detector]")
{
    const double fs = 48000.0;
    struct Case
    {
        double rel;
        double h3[3];
    };
    const Case cases[] = { { 3.0, { -23.8, -32.2, -44.3 } },
                           { 30.0, { -28.5, -37.6, -50.0 } },
                           { 100.0, { -30.9, -42.2, -55.1 } },
                           { 300.0, { -32.2, -46.6, -61.5 } } };
    const double freqs[3] = { 50.0, 100.0, 200.0 };
    for (const auto& c : cases)
    {
        Settings s = trajectorySettings();
        s.releaseMs = c.rel;
        for (int i = 0; i < 3; ++i)
        {
            const auto r = measureTone (fs, s, freqs[i], dbToLin (-6.0), 1.0, 1.0);
            INFO ("Release " << c.rel << " f " << freqs[i] << " H3 " << r.h3Dbc << " H2 " << r.h2Dbc);
            CHECK (r.h3Dbc == Approx (c.h3[i]).margin (4.0));
            CHECK (r.h2Dbc < -85.0);
        }
        const auto r500 = measureTone (fs, s, 500.0, dbToLin (-6.0), 1.0, 1.0);
        INFO ("Release " << c.rel << " 500 Hz H3 " << r500.h3Dbc);
        CHECK (r500.h3Dbc < -120.0);
    }
}

TEST_CASE ("Trajectories are sample-rate independent (44.1 / 96 kHz within 0.1 dB of 48 kHz)", "[dynamics][rates]")
{
    for (const auto& s : { with ({}), with ({ { "A", 2.0 } }), with ({ { "Rel", 30.0 } }), with ({ { "P", 10.0 } }) })
    {
        const auto ref = runTrajectory (48000.0, s);
        for (double fs : { 44100.0, 96000.0 })
        {
            const auto tr = runTrajectory (fs, s);
            // compare after the first 5 ms, where the look-ahead (64 vs 128 samples) is not yet involved
            for (double ms : { 8.0, 12.0, 20.0, 30.0, 50.0, 80.0, 150.0 })
            {
                INFO ("attack t=" << ms << " fs " << fs);
                CHECK (tr.gainAt (tr.up, ms) == Approx (ref.gainAt (ref.up, ms)).margin (0.1));
            }
            for (double ms : { 5.0, 10.0, 20.0, 50.0, 100.0, 200.0, 400.0, 800.0 })
            {
                INFO ("release t=" << ms << " fs " << fs);
                CHECK (tr.gainAt (tr.down, ms) == Approx (ref.gainAt (ref.down, ms)).margin (0.1));
            }
        }
    }
}
