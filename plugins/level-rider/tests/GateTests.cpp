// Spec r1 section 7 / tables T4 (idle behaviour, +-1.0 dB) and T5 (activity gate state), start-up, Spill.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "TestHelpers.h"

using namespace craft::levelrider::test;
using Catch::Approx;

namespace
{
struct T4Row
{
    double idle;
    bool fast;
    double burst;
    double gain[10];
};

const double t4Times[10] = { 0.1, 0.25, 0.5, 0.75, 1, 1.5, 2, 2.5, 3, 4 };
const T4Row t4[] = {
    { 0, false, -9, { -11.91, -11.91, -11.91, -10.97, -9.92, -7.56, -4.89, -1.96, +0.00, +0.00 } },
    { 0, false, -33, { +6.69, +6.69, +6.69, +5.02, +3.35, +0.13, +0.00, +0.00, +0.00, +0.00 } },
    { 0, false, -21, { -3.70, -3.70, -3.70, -2.24, -0.70, +0.00, +0.00, +0.00, +0.00, +0.00 } },
    { 6, false, -9, { -11.91, -11.91, -11.91, -10.97, -9.92, -7.56, -4.89, -1.96, +1.16, +6.00 } },
    { 6, false, -33, { +6.69, +6.69, +6.69, +6.00, +6.00, +6.00, +6.00, +6.00, +6.00, +6.00 } },
    { 6, false, -21, { -3.70, -3.70, -3.70, -2.24, -0.70, +2.49, +5.81, +6.00, +6.00, +6.00 } },
    { -6, false, -9, { -11.91, -11.91, -11.91, -10.97, -9.92, -7.56, -6.00, -6.00, -6.00, -6.00 } },
    { -6, false, -33, { +6.69, +6.69, +6.69, +5.02, +3.35, +0.13, -2.94, -5.79, -6.00, -6.00 } },
    { -6, false, -21, { -3.70, -3.70, -3.70, -5.11, -6.00, -6.00, -6.00, -6.00, -6.00, -6.00 } },
    { 0, true, -9, { -11.83, -11.26, -10.24, -9.13, -7.93, -5.30, -2.41, +0.00, +0.00, +0.00 } },
    { 0, true, -33, { +4.37, +3.41, +1.78, +0.18, +0.00, +0.00, +0.00, +0.00, +0.00, +0.00 } },
    { 0, true, -21, { -5.27, -4.43, -2.97, -1.47, +0.00, +0.00, +0.00, +0.00, +0.00, +0.00 } },
};

const double t5Levels[20] = { -80, -21, -27, -33, -39, -45, -51, -57, -63, -69, -75, -69, -63, -57, -51, -45, -39, -33, -27, -21 };
struct T5Row
{
    double vocalSens;
    bool fast;
    double gain[20];
};
// Row -12 is informative only (decision D2) and not checked.
const T5Row t5[] = {
    { -6, false, { +0.0, -4.4, +0.6, +6.1, +11.2, +12.0, +12.0, +12.0, +0.0, +0.0, +0.0, +0.0, +0.0, +0.2, +3.9, +12.0, +11.3, +6.0, +0.5, -4.5 } },
    { 0, false, { +0.0, -3.9, +1.1, +6.7, +11.8, +12.0, +12.0, +12.0, +12.0, +0.0, +0.0, +0.0, +0.4, +6.3, +12.0, +12.0, +11.9, +6.6, +1.0, -4.0 } },
    { 6, false, { +0.0, -3.7, +1.4, +7.0, +12.0, +12.0, +12.0, +12.0, +12.0, +12.0, +0.0, +0.4, +7.1, +12.0, +12.0, +12.0, +12.0, +6.8, +1.3, -3.8 } },
    { 12, false, { +0.0, -3.6, +1.5, +7.1, +12.0, +12.0, +12.0, +12.0, +12.0, +12.0, +12.0, +12.0, +12.0, +12.0, +12.0, +12.0, +12.0, +7.0, +1.5, -3.7 } },
    { 0, true, { +0.0, -5.6, -0.7, +4.4, +9.9, +12.0, +12.0, +12.0, +12.0, +0.0, +0.0, +0.0, +0.4, +5.9, +12.0, +12.0, +10.0, +4.6, -1.0, -5.9 } },
};
} // namespace

TEST_CASE ("T4: idle behaviour after a burst falls to -80 dBFS noise", "[T4][gate]")
{
    for (const auto& row : t4)
    {
        auto e = makeEngine (48000.0, -12, 12, row.fast, -21, row.idle);
        const auto tr = runSegments (e, { { row.burst, 12.0 }, { -80.0, 5.0 } }); // 12 s bursts like the T2 holds
        for (size_t j = 0; j < 10; ++j)
        {
            const double g = tr.at (12.0 + t4Times[j]);
            INFO ("Idle " << row.idle << (row.fast ? " Fast" : " Slow") << ", burst " << row.burst << ", " << t4Times[j]
                          << " s: " << g << " vs " << row.gain[j]);
            CHECK (std::abs (g - row.gain[j]) <= 1.0);
        }
        // the fader stops exactly at Idle
        CHECK (tr.at (17.0) == Approx (row.idle).margin (1e-9));
    }
}

TEST_CASE ("T5: activity gate state at the end of each 4 s step", "[T5][gate]")
{
    for (const auto& row : t5)
    {
        auto e = makeEngine (48000.0, -12, 12, row.fast, -21, 0.0, row.vocalSens);
        BandNoise noise (48000.0, 11);
        const int win = 240;
        std::vector<double> l (static_cast<size_t> (win)), r (static_cast<size_t> (win));
        bool refRide[20], ride[20];
        for (size_t s = 0; s < 20; ++s)
        {
            const double a = dbToGain (t5Levels[s]);
            for (int w = 0; w < 4 * 200; ++w)
            {
                for (int i = 0; i < win; ++i)
                    l[size_t (i)] = r[size_t (i)] = a * noise.next();
                double* ch[2] = { l.data(), r.data() };
                e.process (ch, 2, (const double* const*) nullptr, 0, win);
            }
            ride[s] = e.isRiding();
            refRide[s] = std::abs (row.gain[s]) > 0.05; // the reference shows exactly +0.0 while idle
        }
        for (size_t s = 0; s < 20; ++s)
        {
            const bool adjacent = (s > 0 && refRide[s - 1] != refRide[s]) || (s + 1 < 20 && refRide[s + 1] != refRide[s]);
            INFO ("Vocal Sensitivity " << row.vocalSens << (row.fast ? " Fast" : " Slow") << ", step " << s + 1 << " ("
                                       << t5Levels[s] << " dBFS): riding " << ride[s] << " vs " << refRide[s]);
            if (! adjacent)
                CHECK (ride[s] == refRide[s]);
        }
    }
}

TEST_CASE ("T5: absolute threshold -62 - Vocal Sensitivity with 4 dB hysteresis (+-3 dB)", "[T5][gate]")
{
    for (double vs : { -12.0, -6.0, 0.0, 6.0, 12.0 })
    {
        const double thetaOn = -62.0 - vs;
        // steady band noise 3 dB above theta_on (+ weighting margin) rides, 3 dB below theta_off stays idle
        {
            auto e = makeEngine (48000.0, -12, 12, false, -21, 0.0, vs);
            runSegments (e, { { thetaOn + 4.0, 3.0 } });
            INFO ("VS " << vs << " above theta_on");
            CHECK (e.isRiding());
        }
        {
            auto e = makeEngine (48000.0, -12, 12, false, -21, 0.0, vs);
            runSegments (e, { { thetaOn - 4.0 - 3.0, 3.0 } });
            INFO ("VS " << vs << " below theta_off");
            CHECK_FALSE (e.isRiding());
        }
        {
            // hysteresis: after riding, a level between theta_off and theta_on keeps riding
            auto e = makeEngine (48000.0, -12, 12, false, -21, 0.0, vs);
            runSegments (e, { { thetaOn + 4.0, 3.0 }, { thetaOn - 1.0, 3.0 } });
            INFO ("VS " << vs << " inside the hysteresis band");
            CHECK (e.isRiding());
        }
    }
}

TEST_CASE ("Start-up: g = Idle before any signal", "[gate][startup]")
{
    for (double idle : { -6.0, 0.0, 6.0 })
    {
        auto e = makeEngine (48000.0, -12, 12, false, -21, idle);
        CHECK (e.getGainDb() == Approx (idle));
        const auto tr = runSegments (e, { { -300.0, 1.0 } });
        CHECK (tr.at (1.0) == Approx (idle).margin (1e-9));
        CHECK_FALSE (e.isRiding());
    }
}

TEST_CASE ("Short gaps (< 0.5 s, Slow) do not move the fader; Fast moves at once", "[gate][idle]")
{
    auto slow = makeEngine (48000.0, -12, 12, false);
    auto tr = runSegments (slow, { { -9.0, 8.0 }, { -80.0, 0.4 }, { -9.0, 0.2 } });
    const double before = tr.at (8.0);
    for (double t = 8.0; t <= 8.4; t += 0.05)
        CHECK (std::abs (tr.at (t) - before) < 0.05);

    auto fast = makeEngine (48000.0, -12, 12, true);
    tr = runSegments (fast, { { -9.0, 8.0 }, { -80.0, 0.4 } });
    CHECK (tr.at (8.4) - tr.at (8.0) > 1.0);
}

TEST_CASE ("Relative threshold: a 30 dB drop goes idle within about 3 s and resumes after 4 - 10 s", "[gate][relative]")
{
    for (bool fast : { false, true })
    {
        auto e = makeEngine (48000.0, -12, 12, fast);
        BandNoise noise (48000.0, 5);
        std::vector<double> l (48), r (48);
        auto run = [&] (double level, double seconds, auto&& onBlock) {
            for (int b = 0; b < int (seconds * 1000.0); ++b)
            {
                for (size_t i = 0; i < 48; ++i)
                    l[i] = r[i] = dbToGain (level) * noise.next();
                double* ch[2] = { l.data(), r.data() };
                e.process (ch, 2, (const double* const*) nullptr, 0, 48);
                onBlock (b);
            }
        };
        run (-3.0, 12.0, [] (int) {});
        REQUIRE (e.isRiding());
        int idleAt = -1, resumeAt = -1;
        run (-33.0, 12.0, [&] (int b) {
            if (idleAt < 0 && ! e.isRiding())
                idleAt = b;
            if (idleAt >= 0 && resumeAt < 0 && e.isRiding() && b - idleAt > 500)
                resumeAt = b;
        });
        INFO ((fast ? "Fast" : "Slow") << ": idle at " << idleAt << " ms, resumed at " << resumeAt << " ms");
        CHECK (idleAt >= 0);
        CHECK (idleAt < 3000);
        CHECK (resumeAt >= 4000);
        CHECK (resumeAt <= 10000);
    }
}

TEST_CASE ("Relative threshold: drops of 12 dB or less never go idle", "[gate][relative]")
{
    for (bool fast : { false, true })
    {
        auto e = makeEngine (48000.0, -12, 12, fast);
        BandNoise noise (48000.0, 9);
        std::vector<double> l (48), r (48);
        int idleBlocks = 0;
        const double levels[] = { -9.0, -21.0, -9.0, -21.0, -15.0, -27.0, -33.0 };
        for (size_t s = 0; s < std::size (levels); ++s)
            for (int b = 0; b < 8000; ++b)
            {
                for (size_t i = 0; i < 48; ++i)
                    l[i] = r[i] = dbToGain (levels[s]) * noise.next();
                double* ch[2] = { l.data(), r.data() };
                e.process (ch, 2, (const double* const*) nullptr, 0, 48);
                if (s > 0 && ! e.isRiding())
                    ++idleBlocks;
            }
        INFO ((fast ? "Fast" : "Slow") << ": " << idleBlocks << " idle blocks");
        CHECK (idleBlocks == 0);
    }
}

TEST_CASE ("Live: Spill gate (section 7.4)", "[gate][spill]")
{
    auto make = [] (double spill) {
        RiderEngine e;
        Settings s;
        s.rangeMinDb = -12.0;
        s.rangeMaxDb = 12.0;
        s.spillEnabled = true;
        s.spillDb = spill;
        e.setSettings (s);
        e.prepare (48000.0);
        return e;
    };
    {
        // default -48: vocal-level material rides as in the standard product
        auto live = make (-48.0);
        auto standard = makeEngine (48000.0, -12, 12);
        const auto a = runSegments (live, { { -21.0, 6.0 }, { -33.0, 6.0 } });
        const auto b = runSegments (standard, { { -21.0, 6.0 }, { -33.0, 6.0 } });
        CHECK (a.gainDb == b.gainDb);
    }
    {
        // material below Spill does not ride
        auto live = make (-30.0);
        runSegments (live, { { -36.0, 4.0 } });
        CHECK_FALSE (live.isRiding());
        runSegments (live, { { -24.0, 4.0 } });
        CHECK (live.isRiding());
    }
}
