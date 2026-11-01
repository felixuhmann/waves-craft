// Development aid (not shipped): prints the dynamics timing and static measurements next to the spec r3 targets.
// Build: g++ -O2 -std=c++17 TimingProbe.cpp -o timing_probe   (from this directory)
#include <cstdio>
#include <vector>

#include "../source/dsp/Dynamics.h"

using namespace craft::consolee::dsp;

struct Times
{
    double t10, t63, t90;
};

static Times stepRun (DynamicsSettings s, double a, double b, double fs, double pre, double post, bool expander)
{
    Dynamics d;
    d.setSettings (s);
    d.prepare (fs);
    const int npre = int (pre * fs), npost = int (post * fs);
    double ph = 0, g0 = 0;
    const double dph = 2 * pi * 1000.0 / fs;
    std::vector<double> g (static_cast<size_t> (npost));
    for (int i = 0; i < npre + npost; ++i)
    {
        const double x = std::pow (10.0, (i < npre ? a : b) / 20.0) * std::sin (ph);
        ph += dph;
        double sc[2] = { x, x };
        d.process (sc, 2);
        const double gd = expander ? d.getExpanderGainDb() : d.getCompressorReductionDb();
        if (i == npre - 1) g0 = gd;
        if (i >= npre) g[size_t (i - npre)] = gd;
    }
    const double g1 = g.back();
    Times t { -1, -1, -1 };
    for (int i = 0; i < npost; ++i)
    {
        const double fr = (g[size_t (i)] - g0) / (g1 - g0);
        if (t.t10 < 0 && fr >= 0.1) t.t10 = i / fs * 1000;
        if (t.t63 < 0 && fr >= 0.632) t.t63 = i / fs * 1000;
        if (t.t90 < 0 && fr >= 0.9) t.t90 = i / fs * 1000;
    }
    return t;
}

static double steadyGain (DynamicsSettings s, double level, double fs, double f = 1000.0)
{
    Dynamics d;
    d.setSettings (s);
    d.prepare (fs);
    double ph = 0, acc = 0;
    int n = 0;
    for (int i = 0; i < int (4 * fs); ++i)
    {
        const double x = std::pow (10.0, level / 20.0) * std::sin (ph);
        ph += 2 * pi * f / fs;
        double sc[2] = { x, x };
        const double gd = d.process (sc, 2);
        if (i > 3 * fs) { acc += std::pow (10.0, gd / 20.0); ++n; }
    }
    return 20 * std::log10 (acc / n);
}

int main()
{
    const double fs = 48000;
    DynamicsSettings s;
    s.compInvRatio = 0.25;
    s.compThresholdDb = -10;
    const char* att[2][4] = { { "6.5/46/89", "6.0/45/88", "6.5/45.5/94", "7.5/60/123.5" }, { "1.5/3.0/5.5", "1.5/3.0/6.0", "1.5/3.0/7.0", "1.5/5.0/11" } };
    const char* rel[2][4] = { { "87/217", "127/339", "334/1371", "1659/3431" }, { "31/73", "66/278", "298/1404", "1662/3433" } };
    const double rels[4] = { 0.1, 0.4, 1.0, 4.0 };
    for (int fast = 0; fast < 2; ++fast)
        for (int k = 0; k < 4; ++k)
        {
            s.compFastAttack = fast;
            s.compReleaseS = rels[k];
            auto a = stepRun (s, -36.5, -6.5, fs, 1.0, 1.0, false);
            auto r = stepRun (s, -6.5, -36.5, fs, 2.0, 12.0, false);
            std::printf ("%s rel %.1f: attack %.2f/%.2f/%.2f (%s)  release %.0f/%.0f (%s)\n", fast ? "fast" : "slow", rels[k], a.t10, a.t63,
                         a.t90, att[fast][k], r.t63, r.t90, rel[fast][k]);
        }

    // section 6.5: gain change vs slow / 0.4 s
    const double levels[5] = { -24, -18, -12, -6, 0 };
    double base[5];
    s.compFastAttack = false;
    s.compReleaseS = 0.4;
    for (int i = 0; i < 5; ++i)
        base[i] = steadyGain (s, levels[i], fs);
    std::printf ("base (slow/0.4): ");
    for (double b : base) std::printf ("%.2f ", b);
    std::printf ("\n");
    for (int fast = 0; fast < 2; ++fast)
        for (int k = 0; k < 4; ++k)
        {
            s.compFastAttack = fast;
            s.compReleaseS = rels[k];
            std::printf ("dG %s %.1f: ", fast ? "fast" : "slow", rels[k]);
            for (int i = 0; i < 5; ++i)
                std::printf ("%+.2f ", steadyGain (s, levels[i], fs) - base[i]);
            std::printf ("\n");
        }

    DynamicsSettings e;
    e.expThresholdDb = -20;
    e.expRangeDb = 40;
    e.compInvRatio = 1.0;
    for (int gate = 0; gate < 2; ++gate)
    {
        e.gate = gate;
        for (double rr : { 0.1, 0.4, 1.0, 4.0 })
        {
            e.expReleaseS = rr;
            e.expFastAttack = rr == 0.4;
            auto r = stepRun (e, -25, -60, fs, 1.0, 12.0, true);
            std::printf ("%s rel %.1f: %.0f / %.0f / %.0f\n", gate ? "gate" : "exp ", rr, r.t10, r.t63, r.t90);
        }
    }
    const double r72[][5] = { { -30, 40, 52, 111, 146 }, { -20, 40, 58, 166, 237 }, { -30, 20, 110, 270, 394 }, { -20, 20, 123, 427, 875 }, { -30, 10, 238, 542, 989 },
                              { -20, 10, 237, 542, 988 }, { -10, 5, 776, 1078, 1515 }, { -10, 15, 155, 460, 906 }, { -10, 30, 88, 367, 714 } };
    e.gate = false; e.expFastAttack = false; e.expReleaseS = 0.4;
    for (const auto& q : r72)
    {
        e.expThresholdDb = q[0]; e.expRangeDb = q[1];
        auto r = stepRun (e, -25, -60, fs, 1.0, 6.0, true);
        std::printf ("7.2 thr %.0f range %.0f: %.0f/%.0f/%.0f (%.0f/%.0f/%.0f)\n", q[0], q[1], r.t10, r.t63, r.t90, q[2], q[3], q[4]);
    }
    std::printf ("targets exp: 24/68/96 58/166/236 128/364/520 470/1344/1922; gate 36/156/332 86/390/830 188/856/1826 626/2260/3700\n");
}
