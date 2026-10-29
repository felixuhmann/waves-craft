// Filter module (spec 7, 10.6).
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "TestHelpers.h"

using namespace testutil;
using Catch::Approx;

namespace
{
double magDb (const BiquadCoeffs& c, double hz, double fs, int power = 1)
{
    return double (power) * 20.0 * std::log10 (std::abs (c.response (2.0 * pi * hz / fs)));
}

// Analog reference |H(j w)| of spec 7.
double analogMagDb (FilterType t, double freq, double q, double hz)
{
    const auto a = filterdesign::analog (t, freq, q);
    const std::complex<double> s (0.0, 2.0 * pi * hz);
    return 20.0 * std::log10 (std::abs ((a.n2 * s * s + a.n1 * s + a.n0) / (a.d2 * s * s + a.d1 * s + a.d0)));
}
} // namespace

TEST_CASE ("Filter shapes: LP/HP half power at Freq, BP/BR centred, bilinear prewarped", "[filter]")
{
    for (double fs : { 44100.0, 48000.0, 96000.0 })
        for (double q : { 0.1, 0.35, 0.6 })
            for (double f : { 16.0, 100.0, 1000.0, 7179.0, 15000.0 })
            {
                const auto lp = filterdesign::design (FilterType::LowPass, f, q, fs);
                const auto hp = filterdesign::design (FilterType::HighPass, f, q, fs);
                const auto bp = filterdesign::design (FilterType::BandPass, f, q, fs);
                const auto br = filterdesign::design (FilterType::BandReject, f, q, fs);
                CHECK (magDb (lp.h, f, fs) == Approx (-3.0103).margin (1.0e-6));
                CHECK (magDb (hp.h, f, fs) == Approx (-3.0103).margin (1.0e-6));
                CHECK (magDb (bp.h, f, fs) == Approx (0.0).margin (1.0e-6));
                CHECK (magDb (br.h, f, fs) < -100.0);
                // close to the analog prototype well below Nyquist
                if (f <= 1000.0)
                    for (double hz : { f / 2.0, f * 1.5, f * 2.0 })
                    {
                        CHECK (magDb (lp.h, hz, fs) == Approx (analogMagDb (FilterType::LowPass, f, q, hz)).margin (0.1));
                        CHECK (magDb (bp.h, hz, fs) == Approx (analogMagDb (FilterType::BandPass, f, q, hz)).margin (0.1));
                    }
            }
}

TEST_CASE ("10.6 sidechain |H|^3 and passive 2P magnitudes (Freq 1 kHz, Q 0.6)", "[filter]")
{
    const double fs = 48000.0;
    const auto lp = filterdesign::design (FilterType::LowPass, 1000.0, 0.6, fs);
    CHECK (magDb (lp.h, 500.0, fs, 3) == Approx (-2.0).margin (0.1));
    CHECK (magDb (lp.h, 1000.0, fs, 3) == Approx (-9.0).margin (0.1));
    CHECK (magDb (lp.h, 2000.0, fs, 3) == Approx (-31.0).margin (0.1));
    CHECK (magDb (lp.h, 3000.0, fs, 3) == Approx (-49.8).margin (0.15));
    const auto bp = filterdesign::design (FilterType::BandPass, 1000.0, 0.6, fs);
    CHECK (magDb (bp.h, 500.0, fs, 3) == Approx (-7.8).margin (0.1));
    CHECK (magDb (bp.h, 1000.0, fs, 3) == Approx (0.0).margin (0.1));
    CHECK (magDb (bp.h, 2000.0, fs, 3) == Approx (-7.8).margin (0.1));
    const auto hp = filterdesign::design (FilterType::HighPass, 1000.0, 0.6, fs);
    CHECK (magDb (hp.h, 2000.0, fs, 3) == Approx (-2.0).margin (0.1));
    CHECK (magDb (hp.h, 1000.0, fs, 3) == Approx (-9.0).margin (0.1));
    CHECK (magDb (hp.h, 500.0, fs, 3) == Approx (-31.0).margin (0.25)); // "mirror of LP": exact only in the analog domain

    auto passive = [&] (const FilterDesign& d, double hz) {
        const double w = 2.0 * pi * hz / fs;
        const auto h = d.h.response (w);
        return 20.0 * std::log10 (std::abs (2.0 * (d.ap.response (w) - d.activeSign * h * h)));
    };
    CHECK (passive (lp, 500.0) == Approx (-11.1).margin (0.1));
    CHECK (passive (lp, 1000.0) == Approx (0.0).margin (0.1));
    CHECK (passive (lp, 2000.0) == Approx (5.2).margin (0.1));
    CHECK (passive (lp, 5000.0) == Approx (6.0).margin (0.1));
}

TEST_CASE ("Split identity: A + P = AP (flat), |P| = 1 - |A|, A in phase with AP", "[filter][split]")
{
    const double fs = 48000.0;
    for (auto t : { FilterType::LowPass, FilterType::BandPass, FilterType::HighPass, FilterType::BandReject })
        for (double q : { 0.1, 0.6 })
            for (double hz : { 30.0, 200.0, 800.0, 1000.0, 1300.0, 5000.0, 15000.0 })
            {
                const auto d = filterdesign::design (t, 1000.0, q, fs);
                const double w = 2.0 * pi * hz / fs;
                const auto h = d.h.response (w), ap = d.ap.response (w);
                const auto a = d.activeSign * h * h, p = ap - a;
                CHECK (std::abs (ap) == Approx (1.0).margin (1.0e-9));
                CHECK (std::abs (p) == Approx (1.0 - std::abs (a)).margin (1.0e-9));
                if (std::abs (a) > 1.0e-6)
                    CHECK (std::abs (std::arg (a / ap)) < 1.0e-6);
            }
}

TEST_CASE ("r2 7: allpass D(-s)/D(s) for every type - AP(0) = +1, band-pass passive has the band-reject polarity", "[filter][split]")
{
    const double fs = 48000.0;
    for (auto t : { FilterType::LowPass, FilterType::BandPass, FilterType::HighPass, FilterType::BandReject })
    {
        const auto d = filterdesign::design (t, 1000.0, 0.6, fs);
        CHECK (d.ap.response (0.0).real() == Approx (1.0).margin (1.0e-9));
    }
    // passive monitor (2P) in-phase gain far below the band centre: band-pass passes it in phase (+2)
    for (auto t : { FilterType::BandPass, FilterType::BandReject })
    {
        Settings s;
        s.filterType = t;
        s.filterFreqHz = 4000.0;
        s.monitor = Monitor::Passive;
        auto e = makeEngine (s, fs);
        const int n = int (0.3 * fs);
        auto in = sine (n, fs, 40.0, -20.0);
        auto out = runStereo (e, in, in);
        double xy = 0.0, xx = 0.0;
        for (int i = n / 2; i < n; ++i)
            xy += in[size_t (i)] * out.l[size_t (i)], xx += in[size_t (i)] * in[size_t (i)];
        INFO ("type " << int (t) << " in-phase gain " << xy / xx);
        if (t == FilterType::BandPass)
            CHECK (xy / xx > 1.9);
        else
            CHECK (std::abs (xy / xx) < 0.05); // band-reject: 40 Hz is in its active band, passive ~ 0
    }
}

TEST_CASE ("Engine: sidechain and passive monitors, split with no gain change is flat", "[filter][monitor]")
{
    const double fs = 48000.0;
    Settings s;
    s.filterType = FilterType::LowPass;
    s.filterFreqHz = 1000.0;
    s.filterQ = 0.6;
    s.compEq = EqMode::Sidechain;
    s.monitor = Monitor::Sidechain;
    auto gainAt = [&] (const Settings& st, double hz) {
        auto e = makeEngine (st, fs);
        const int n = int (0.4 * fs);
        auto in = sine (n, fs, hz, -30.0);
        auto out = runStereo (e, in, in);
        return ratioDb (out.l, in, n / 2, n);
    };
    CHECK (gainAt (s, 500.0) == Approx (-2.0).margin (0.1));
    CHECK (gainAt (s, 1000.0) == Approx (-9.0).margin (0.1));
    CHECK (gainAt (s, 2000.0) == Approx (-31.0).margin (0.1));

    s.monitor = Monitor::Passive;
    CHECK (gainAt (s, 500.0) == Approx (-11.1).margin (0.1));
    CHECK (gainAt (s, 2000.0) == Approx (5.2).margin (0.1));

    // no module uses the filter: the sidechain monitor is the wideband signal
    s.compEq = EqMode::Wideband;
    s.monitor = Monitor::Sidechain;
    CHECK (gainAt (s, 2000.0) == Approx (0.0).margin (1.0e-9));
    // r2 3.7: only the gate filtered -> the monitor stays wideband (it follows the compressor's EQ mode)
    s.gateEq = EqMode::Sidechain;
    s.gateOpenDb = -60.0;
    CHECK (gainAt (s, 2000.0) == Approx (0.0).margin (1.0e-9));
    s.gateEq = EqMode::Wideband;
    s.gateOpenDb = minusInfinityDb;

    // Split with no gain change: 0.00 dB at every frequency (allpass)
    s.monitor = Monitor::Audio;
    s.compEq = EqMode::Split;
    for (auto t : { FilterType::LowPass, FilterType::BandPass, FilterType::HighPass, FilterType::BandReject })
    {
        s.filterType = t;
        for (double hz : { 60.0, 700.0, 1000.0, 3000.0, 12000.0 })
            CHECK (gainAt (s, hz) == Approx (0.0).margin (0.01));
    }
}

TEST_CASE ("Engine: split compression acts on the active band only", "[filter][split]")
{
    const double fs = 48000.0;
    Settings s;
    s.filterType = FilterType::HighPass;
    s.filterFreqHz = 4000.0;
    s.compEq = EqMode::Split;
    s.ratioPos = 0.8;
    s.compThresholdDb = -60.0;
    auto gainAt = [&] (double hz) {
        auto e = makeEngine (s, fs);
        const int n = int (0.4 * fs);
        auto in = sine (n, fs, hz, -10.0);
        auto out = runStereo (e, in, in);
        return ratioDb (out.l, in, n / 2, n);
    };
    CHECK (gainAt (200.0) == Approx (0.0).margin (0.05));  // passive band untouched
    CHECK (gainAt (12000.0) < -15.0);                      // active band compressed
}
