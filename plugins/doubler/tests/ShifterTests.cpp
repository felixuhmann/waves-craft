// Spec section 11.3: statistical acceptance of the pitch shifter
// (voice 1 solo, panned left, delay 20 ms, Range 80 Hz unless noted).
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "TestHelpers.h"

using namespace testutil;
using Catch::Approx;

namespace
{
Settings shifterSettings (double detune, bool octave = false, bool range20 = false)
{
    auto s = soloSettings();
    s.voices[0].panDeg = -45.0;
    s.voices[0].delayMs = 20.0;
    s.voices[0].detuneCents = detune;
    s.voices[0].octave = octave;
    s.range20 = range20;
    return s;
}

std::vector<double> shift (const Settings& s, const std::vector<double>& x, double fs)
{
    auto e = makeEngine (s, fs);
    return run (e, x, x).l;
}

double ms (double samples, double fs) { return samples / fs * 1000.0; }
} // namespace

TEST_CASE ("Pitch of a 1 kHz sine (phase slope over 3 s)", "[shifter]")
{
    const double fs = GENERATE (44100.0, 48000.0);
    const auto x = sine (int (3.3 * fs), 1000.0, fs);
    const size_t from = size_t (0.2 * fs), to = size_t (3.2 * fs);

    for (double c : { 1.0, -1.0, 10.0, -10.0, 50.0, -50.0, 100.0, -100.0 })
    {
        const auto y = shift (shifterSettings (c), x, fs);
        const double fRef = 1000.0 * std::exp2 (c / 1200.0);
        const double measured = c + pitchCents (y, from, to, fRef, fs);
        INFO ("fs " << fs << " detune " << c << " measured " << measured);
        WARN ("pitch fs " << fs << " detune " << c << ": " << measured);
        CHECK (measured == Approx (c).margin (0.5));
    }
    for (double c : { 0.0, 50.0 })
    {
        const auto y = shift (shifterSettings (c, true), x, fs);
        const double expected = c - 1200.0;
        const double fRef = 1000.0 * std::exp2 (expected / 1200.0);
        const double measured = expected + pitchCents (y, from, to, fRef, fs);
        INFO ("fs " << fs << " octave + " << c << " measured " << measured);
        WARN ("pitch fs " << fs << " octave + " << c << ": " << measured);
        CHECK (measured == Approx (expected).margin (6.0));
    }
}

TEST_CASE ("Sine envelope stays smooth across splices (Hilbert, 1 % percentile)", "[shifter]")
{
    const double fs = 48000.0;
    const size_t n = size_t (1) << 17;
    const size_t from = size_t (0.1 * fs);
    struct Case
    {
        double f, cents, limit;
    };
    std::vector<Case> cases;
    for (double f : { 1000.0, 100.0 })
        for (double c : { 10.0, -10.0, 50.0, -50.0, 100.0, -100.0 })
            cases.push_back ({ f, c, 0.97 });
    cases.push_back ({ 5000.0, 50.0, 0.90 });

    for (const auto& c : cases)
    {
        const auto x = sine (int (from + n + 1000), c.f, fs, 0.5);
        const auto y = shift (shifterSettings (c.cents), x, fs);
        const auto a = analytic (y, from, n);
        std::vector<double> env;
        for (size_t i = 4000; i < n - 4000; ++i)
            env.push_back (std::abs (a[i]) / 0.5);
        const double p1 = percentile (env, 0.01);
        INFO ("f " << c.f << " cents " << c.cents << " 1% envelope " << p1);
        WARN ("envelope " << c.f << " Hz " << c.cents << " cents: 1% percentile " << p1);
        CHECK (p1 >= c.limit);
    }
}

TEST_CASE ("White noise: level and 1/3-octave flatness", "[shifter]")
{
    const double fs = 48000.0;
    const auto x = noise (int (6.0 * fs), 77);
    const size_t from = size_t (0.2 * fs), to = x.size();
    const size_t nfft = 8192;
    const auto bandsIn = thirdOctaveBands (powerSpectrum (x, from, to, nfft), fs, nfft);

    for (double c : { 10.0, -10.0, 50.0, -50.0, 100.0, -100.0 })
    {
        const auto y = shift (shifterSettings (c), x, fs);
        // the output is the input delayed by ~20 ms; compare over the same span
        const double levelDb = 20.0 * std::log10 (rms (y, from, to) / rms (x, from, to));
        INFO ("cents " << c << " level " << levelDb << " dB");
        CHECK (levelDb == Approx (-1.8).margin (0.6));

        // band gain relative to the broadband gain (the colouration of the shifter)
        const auto bandsOut = thirdOctaveBands (powerSpectrum (y, from, to, nfft), fs, nfft);
        double worst = 0.0;
        size_t worstBand = 0;
        for (size_t b = 0; b < bandsIn.size(); ++b)
        {
            const double dev = 10.0 * std::log10 (bandsOut[b] / bandsIn[b]) - levelDb;
            if (std::abs (dev) > worst)
            {
                worst = std::abs (dev);
                worstBand = b;
            }
        }
        INFO ("worst 1/3-octave deviation " << worst << " dB in band " << worstBand);
        WARN ("noise " << c << " cents: level " << levelDb << " dB, worst band deviation " << worst << " dB");
        CHECK (worst <= 3.0);
    }
}

TEST_CASE ("Delay window, splice jumps and mean delay", "[shifter]")
{
    const double fs = GENERATE (44100.0, 48000.0);
    const auto x = noise (int (4.0 * fs), 99);

    struct Case
    {
        double cents;
        bool octave, range20;
    };
    const Case cases[] = { { 50, false, false },  { -50, false, false }, { 100, false, false }, { -100, false, false },
                           { 0, true, false },    { 50, false, true },   { -50, false, true },  { 0, true, true } };
    for (const auto& c : cases)
    {
        auto s = shifterSettings (c.cents, c.octave, c.range20);
        s.voices[0].delayMs = c.range20 ? 60.0 : 20.0;
        auto e = makeEngine (s, fs);
        const double dn = e.getVoice (0).getNominalDelay();
        const double wh = c.range20 ? 23.6 : 6.37, jmin = c.range20 ? 23.9 : 5.6, jmax = c.range20 ? 35.0 : 11.1;

        std::vector<double> traj;
        std::vector<double> jumps;
        double l, r;
        for (size_t i = 0; i < x.size(); ++i)
        {
            e.processSample (x[i], x[i], l, r);
            const double d = e.getVoice (0).getDominantDelay();
            if (! traj.empty() && std::abs (d - traj.back()) > 0.002 * fs)
                jumps.push_back (std::abs (d - traj.back()));
            traj.push_back (d);
        }
        // skip the first excursion from Dn (the shifter starts at the nominal delay)
        const size_t skip = size_t (0.3 * fs);
        double lo = 1e9, hi = -1e9, mean = 0.0;
        for (size_t i = skip; i < traj.size(); ++i)
        {
            lo = std::min (lo, traj[i]);
            hi = std::max (hi, traj[i]);
            mean += traj[i];
        }
        mean /= double (traj.size() - skip);

        INFO ("fs " << fs << " cents " << c.cents << " octave " << c.octave << " range20 " << c.range20 << ": window ["
                    << ms (lo - dn, fs) << ", " << ms (hi - dn, fs) << "] ms, mean " << ms (mean - dn, fs) << " ms, "
                    << jumps.size() << " splices, splices " << e.getVoice (0).getSpliceCount());
        double jMinSeen = 1e9, jMaxSeen = 0.0;
        for (auto j : jumps)
        {
            jMinSeen = std::min (jMinSeen, ms (j, fs));
            jMaxSeen = std::max (jMaxSeen, ms (j, fs));
        }
        WARN ("window fs " << fs << " cents " << c.cents << " oct " << c.octave << " r20 " << c.range20 << ": ["
                           << ms (lo - dn, fs) << ", " << ms (hi - dn, fs) << "] ms, mean " << ms (mean - dn, fs)
                           << " ms, jumps " << jMinSeen << " .. " << jMaxSeen << " ms (" << jumps.size() << ")");
        CHECK (ms (lo - dn, fs) >= -wh - 0.5);
        CHECK (ms (hi - dn, fs) <= wh + 0.5);
        REQUIRE (jumps.size() >= 3);
        for (auto j : jumps)
        {
            CHECK (ms (j, fs) >= jmin - 1.0);
            CHECK (ms (j, fs) <= jmax + 1.0);
        }
        if (! c.range20 && ! c.octave && std::abs (c.cents) == 50.0)
            CHECK (ms (mean - dn, fs) == Approx (c.cents > 0 ? -2.2 : 2.2).margin (0.7));
    }
}

TEST_CASE ("The shifter trajectory is what a cross-correlation of the output sees", "[shifter]")
{
    // short-window cross-correlation between input and output at a small detune (little smearing):
    // the peak lag follows the dominant tap
    const double fs = 48000.0;
    const auto x = noise (int (4.0 * fs), 123);
    auto s = shifterSettings (10.0);
    auto e = makeEngine (s, fs);
    std::vector<double> y (x.size()), dom (x.size());
    double r;
    for (size_t i = 0; i < x.size(); ++i)
    {
        e.processSample (x[i], x[i], y[i], r);
        dom[i] = e.getVoice (0).getDominantDelay();
    }
    const int win = 1024;
    int agree = 0, total = 0;
    for (size_t c = size_t (0.3 * fs); c + size_t (win) < x.size(); c += 2400)
    {
        int bestLag = 0;
        double best = -1e9;
        for (int lag = int (0.005 * fs); lag <= int (0.035 * fs); ++lag)
        {
            double acc = 0.0;
            for (int i = 0; i < win; ++i)
                acc += y[c + size_t (i)] * x[c + size_t (i) - size_t (lag)];
            if (acc > best)
            {
                best = acc;
                bestLag = lag;
            }
        }
        ++total;
        if (std::abs (double (bestLag) - dom[c + size_t (win / 2)]) <= 0.25e-3 * fs)
            ++agree;
    }
    INFO (agree << " of " << total << " windows agree");
    CHECK (agree >= int (0.9 * total));
}

TEST_CASE ("Delay-parameter change: new delay within 10 ms, click-free", "[shifter]")
{
    const double fs = 48000.0;
    const double f = 230.0;
    const auto x = sine (int (0.6 * fs), f, fs, 0.5);
    const double maxSlope = 0.5 * 2.0 * pi * f / fs;

    for (double detune : { 0.0, 20.0 })
    {
        auto s = shifterSettings (detune);
        auto e = makeEngine (s, fs);
        std::vector<double> l = x, r = x;
        const int t0 = int (0.3 * fs);
        e.process (l.data(), r.data(), t0);
        s.voices[0].delayMs = 37.3;
        e.setSettings (s);
        e.process (l.data() + t0, r.data() + t0, int (x.size()) - t0);

        double maxStep = 0.0;
        for (size_t i = size_t (t0) - 100; i < size_t (t0 + 0.05 * fs); ++i)
            maxStep = std::max (maxStep, std::abs (l[i + 1] - l[i]));
        INFO ("detune " << detune << " max step " << maxStep << " vs sine slope " << maxSlope);
        CHECK (maxStep <= 1.3 * maxSlope);
        CHECK (e.getVoice (0).getNominalDelay() == Approx (37.3e-3 * fs));
        if (detune == 0.0)
        {
            // 10 ms after the change the output is exactly the input delayed by the new delay
            const double d = 37.3e-3 * fs, mu = d - std::floor (d);
            for (size_t i = size_t (t0 + 0.01 * fs); i < x.size(); ++i)
            {
                const double expect = (1.0 - mu) * x[i - size_t (d)] + mu * x[i - size_t (d) - 1];
                REQUIRE (l[i] == Approx (expect).margin (1e-9));
            }
        }
    }
}
