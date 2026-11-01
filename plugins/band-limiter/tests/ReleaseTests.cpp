// Release: spec r1 sections 7, 8, 9, 12 (tolerance: times +-25 % or +-20 ms, whichever is larger; points +-1.5 dB).
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <string>

#include "SpecTables.h"
#include "TestHelpers.h"

using namespace helpers;

namespace
{
struct Recovery
{
    double depth, t63, t90, t99;
};

// Short-time level of the carrier around sample i: amplitude over a window of several periods, in dB against the
// unlimited output level.
std::vector<double> grCurve (const std::vector<double>& y, double f, double fs, double reference, int start, int count, int stepSamples)
{
    std::vector<double> gr;
    const int len = std::max (int (fs * 4.0 / f), int (fs * 0.002));
    for (int k = 0; k < count; ++k)
    {
        const int centre = start + k * stepSamples;
        gr.push_back (linToDb (toneAmplitude (y, f, fs, centre - len / 2, len) / reference));
    }
    return gr;
}

// Tone step -24 -> 0 -> -24 dBFS, T -12 (spec 7).
Recovery stepRecovery (Variant v, double fs, Settings s, double f)
{
    s.thresholdDb = -12.0;
    Rig rig (v, fs, s);
    const int settle = int (fs * 0.2), over = int (fs * 0.5), tail = int (fs * 2.6);
    const int n = settle + over + tail;
    std::vector<double> x (size_t (n), 0.0);
    for (int i = 0; i < n; ++i)
    {
        const double a = (i >= settle && i < settle + over) ? 1.0 : dbToLin (-24.0);
        x[size_t (i)] = a * std::sin (twoPi * f * double (i) / fs);
    }
    const auto y = rig.run (x);
    const int latency = rig.engine.getLatencySamples();
    const double reference = dbToLin (-24.0 + 12.0);
    const int stepMs = int (fs * 0.001);
    const int end = settle + over + latency;

    // Output level during the over minus the unlimited output (+12 dB) is the gain reduction.
    const double depth = linToDb (toneAmplitude (y, f, fs, end - int (fs * 0.05), int (fs * 0.04))) - 12.0;
    Recovery r { depth, -1.0, -1.0, -1.0 };
    const auto gr = grCurve (y, f, fs, reference, end, tail / stepMs - 40, stepMs);
    const double from = depth;
    // Skip the windows that still overlap the loud part.
    const size_t first = size_t (std::max (4.0 / f, 0.002) * 500.0) + 1;
    for (size_t k = first; k < gr.size(); ++k)
    {
        if (r.t63 < 0.0 && gr[k] >= 0.37 * from)
            r.t63 = double (k);
        if (r.t90 < 0.0 && gr[k] >= 0.10 * from)
            r.t90 = double (k);
        if (r.t99 < 0.0 && gr[k] >= 0.01 * from)
            r.t99 = double (k);
    }
    return r;
}

bool timeOk (double measured, double expected)
{
    return std::abs (measured - expected) <= std::max (0.25 * expected, 20.0);
}

struct StepCase
{
    const char* name;
    double f;
    Behaviour b;
    double r3;
};
} // namespace

TEST_CASE ("Step release table", "[release]")
{
    static const StepCase cases[] = {
        { "tstep1k_Adaptive", 1000, Behaviour::Adaptive, 0 },
        { "tstep1k_Aggressive", 1000, Behaviour::Aggressive, 0 },
        { "tstep1k_Warm", 1000, Behaviour::Warm, 0 },
        { "tstep1k_Scaled", 1000, Behaviour::Scaled, 0 },
        { "tstep1k_Manual", 1000, Behaviour::Manual, 0 },
        { "tstep100_Adaptive", 100, Behaviour::Adaptive, 0 },
        { "tstep100_Manual", 100, Behaviour::Manual, 0 },
        { "tstep100_Warm", 100, Behaviour::Warm, 0 },
        { "tstep100_Scaled", 100, Behaviour::Scaled, 0 },
        { "tstep100_Aggressive", 100, Behaviour::Aggressive, 0 },
        { "tstep8k_Adaptive", 8000, Behaviour::Adaptive, 0 },
        { "tstep8k_Aggressive", 8000, Behaviour::Aggressive, 0 },
        { "tstep8k_Warm", 8000, Behaviour::Warm, 0 },
        { "tstep8k_Scaled", 8000, Behaviour::Scaled, 0 },
        { "tstep8k_Manual", 8000, Behaviour::Manual, 0 },
        { "tstep1k_Manual_r3_10", 1000, Behaviour::Manual, 10 },
        { "tstep1k_Manual_r3_100", 1000, Behaviour::Manual, 100 },
        { "tstep1k_Manual_r3_1000", 1000, Behaviour::Manual, 1000 },
    };
    for (const auto& c : cases)
    {
        const spec_tables::ReleaseStep* row = nullptr;
        for (const auto& t : spec_tables::releaseSteps)
            if (std::string (t.name) == c.name)
                row = &t;
        REQUIRE (row != nullptr);

        auto s = neutral();
        s.behaviour = c.b;
        if (c.r3 > 0.0)
            s.releaseMs[2] = c.r3;
        const auto r = stepRecovery (Variant::LinearMulti, 48000.0, s, c.f);
        INFO (c.name << ": depth " << r.depth << " t " << r.t63 << "/" << r.t90 << "/" << r.t99 << " expected " << row->grDuring
                     << " " << row->t63 << "/" << row->t90 << "/" << row->t99);
        CHECK (std::abs (r.depth - row->grDuring) <= 1.5);
        CHECK (timeOk (r.t63, row->t63));
        CHECK (timeOk (r.t90, row->t90));
        CHECK (timeOk (r.t99, row->t99));
    }
}

TEST_CASE ("Release times are the same at every sample rate (spec 12)", "[release]")
{
    const double fs = GENERATE (44100.0, 96000.0);
    auto s = neutral();
    auto r = stepRecovery (Variant::LinearMulti, fs, s, 1000.0);
    INFO ("adaptive " << r.t63 << "/" << r.t90 << "/" << r.t99);
    CHECK (timeOk (r.t63, 231));
    CHECK (timeOk (r.t90, 409));
    CHECK (timeOk (r.t99, 495));
    s.behaviour = Behaviour::Manual;
    r = stepRecovery (Variant::LinearMulti, fs, s, 1000.0);
    INFO ("manual " << r.t63 << "/" << r.t90 << "/" << r.t99);
    CHECK (timeOk (r.t63, 307));
    CHECK (timeOk (r.t90, 499));
    CHECK (timeOk (r.t99, 642));
}

TEST_CASE ("Burst release trajectories", "[release]")
{
    const double fs = 48000.0;
    for (const auto& row : spec_tables::releaseBursts)
    {
        const std::string name = row.name;
        auto s = neutral();
        s.thresholdDb = -12.0;
        s.behaviour = name.find ("Manual") != std::string::npos ? Behaviour::Manual : Behaviour::Adaptive;
        const double burst = name.find ("_200") != std::string::npos ? 0.2 : 0.02;
        Rig rig (Variant::LinearMulti, fs, s);
        const int start = int (fs * 0.3), stop = start + int (burst * fs), n = stop + int (fs * 1.2);
        std::vector<double> x (size_t (n), 0.0);
        for (int i = 0; i < n; ++i)
        {
            const double a = (i >= start && i < stop) ? 1.0 : dbToLin (-24.0);
            x[size_t (i)] = a * std::sin (twoPi * 1000.0 * double (i) / fs);
        }
        const auto y = rig.run (x);
        const int end = stop + rig.engine.getLatencySamples();
        const double reference = dbToLin (-12.0);
        for (size_t k = 0; k < std::size (spec_tables::burstTimesMs); ++k)
        {
            const double tms = spec_tables::burstTimesMs[k];
            const int centre = end + int (tms * 0.001 * fs);
            const int len = int (fs * 0.004);
            const double gr = linToDb (toneAmplitude (y, 1000.0, fs, centre - len / 2, len) / reference);
            INFO (name << " at " << tms << " ms: " << gr << " expected " << row.gr[k]);
            CHECK (std::abs (gr - row.gr[k]) <= 1.5);
        }
    }
}

TEST_CASE ("Manual ignores burst length; Adaptive recovers faster after short overs", "[release]")
{
    const double fs = 48000.0;
    auto levelAfter = [fs] (Behaviour b, double burst, double afterMs) {
        auto s = neutral();
        s.thresholdDb = -12.0;
        s.behaviour = b;
        Rig rig (Variant::LinearMulti, fs, s);
        const int start = int (fs * 0.3), stop = start + int (burst * fs), n = stop + int (fs * 0.8);
        std::vector<double> x (size_t (n), 0.0);
        for (int i = 0; i < n; ++i)
            x[size_t (i)] = ((i >= start && i < stop) ? 1.0 : dbToLin (-24.0)) * std::sin (twoPi * 1000.0 * double (i) / fs);
        const auto y = rig.run (x);
        const int centre = stop + rig.engine.getLatencySamples() + int (afterMs * 0.001 * fs);
        return linToDb (toneAmplitude (y, 1000.0, fs, centre - 96, 192) / dbToLin (-12.0));
    };
    CHECK (std::abs (levelAfter (Behaviour::Manual, 0.02, 100.0) - levelAfter (Behaviour::Manual, 0.2, 100.0)) < 1.5); // spec 7 trajectory tolerance
    CHECK (levelAfter (Behaviour::Adaptive, 0.02, 100.0) > levelAfter (Behaviour::Adaptive, 0.2, 100.0) + 3.0);
}

TEST_CASE ("Profile steps", "[profiles][release]")
{
    for (const auto& row : spec_tables::profiles)
    {
        auto s = neutral();
        applyProfile (s, row.index, 1.0);
        const auto r = stepRecovery (Variant::LinearProfile, 48000.0, s, 1000.0);
        INFO ("profile " << row.index << ": depth " << r.depth << " t " << r.t63 << "/" << r.t90 << "/" << r.t99 << " expected "
                         << row.grDuring << " " << row.t63 << "/" << row.t90 << "/" << row.t99);
        CHECK (std::abs (r.depth - row.grDuring) <= 1.5);
        CHECK (timeOk (r.t63, row.t63));
        CHECK (timeOk (r.t90, row.t90));
        CHECK (timeOk (r.t99, row.t99));
    }
}

TEST_CASE ("Release multiplier scales the profile releases", "[profiles]")
{
    Settings a, b;
    applyProfile (a, 2, 1.0);
    applyProfile (b, 2, 2.0);
    for (int i = 0; i < numBands; ++i)
        CHECK (b.releaseMs[size_t (i)] == 2.0 * a.releaseMs[size_t (i)]);
}

// The low-latency rows of release_steps.csv are not reproduced, see IMPLEMENTATION_NOTES: t63 = t99 = 1 ms whatever
// the band release, which contradicts spec sections 7, 8 and 12. The LL engine follows the same release law as LP.
TEST_CASE ("LL release follows the same law as LP", "[release][ll]")
{
    auto s = neutral();
    const auto r = stepRecovery (Variant::LowLatencyMulti, 48000.0, s, 1000.0);
    INFO (r.t63 << "/" << r.t90 << "/" << r.t99);
    CHECK (std::abs (r.depth + 12.0) <= 1.5);
    CHECK (timeOk (r.t63, 233));
    CHECK (timeOk (r.t90, 415));
    CHECK (timeOk (r.t99, 499));
}
