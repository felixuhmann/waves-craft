// Development aid (not shipped): prints the dynamics timing and static measurements next to the spec targets.
// Build: g++ -O2 -std=c++17 TimingProbe.cpp -o timing_probe   (from this directory)
#include <cstdio>
#include <vector>

#include "../source/dsp/Dynamics.h"

using namespace craft::consolee::dsp;

struct Times
{
    double t10, t63, t90;
};

// sine at level a dBFS, then b dBFS; records the compressor reduction or the expander gain (dB) after the step
static Times stepRun (DynamicsSettings s, double a, double b, double f, double fs, double pre, double post, bool expander)
{
    Dynamics d;
    d.prepare (fs);
    d.setSettings (s);
    const int npre = int (pre * fs), npost = int (post * fs);
    double ph = 0, g0 = 0;
    const double dph = 2 * pi * f / fs;
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

int main()
{
    const double fs = 48000;
    DynamicsSettings s;
    s.compInvRatio = 0.25;
    s.compThresholdDb = -10;
    for (int fast = 0; fast < 2; ++fast)
    {
        s.compFastAttack = fast;
        s.compReleaseS = 0.4;
        auto a = stepRun (s, -36.5, -6.5, 1000, fs, 1.0, 1.0, false);
        std::printf ("attack %s: %.2f / %.2f / %.2f   (target %s)\n", fast ? "fast" : "slow", a.t10, a.t63, a.t90, fast ? "1/2/5" : "6/46/90");
        for (double rel : { 0.1, 0.4, 1.0, 4.0 })
        {
            s.compReleaseS = rel;
            auto r = stepRun (s, -6.5, -36.5, 1000, fs, 2.0, 12.0, false);
            std::printf ("  release %.1f: t63 %.0f t90 %.0f\n", rel, r.t63, r.t90);
        }
    }
    std::printf ("targets slow: 87/217 127/339 334/1371 1659/3431; fast: 31/73 66/278 298/1404 1662/3433\n");

    s.compFastAttack = false;
    s.compReleaseS = 0.4;
    for (double f : { 50., 100., 200., 500., 1000., 2000., 5000., 10000. })
    {
        Dynamics d;
        d.prepare (fs);
        d.setSettings (s);
        double ph = 0, acc = 0;
        int n = 0;
        for (int i = 0; i < int (3 * fs); ++i)
        {
            const double x = std::pow (10.0, -6.0 / 20.0) * std::sin (ph);
            ph += 2 * pi * f / fs;
            double sc[2] = { x, x };
            const double gd = d.process (sc, 2);
            if (i > 2 * fs) { acc += gd; ++n; }
        }
        std::printf ("%6.0f Hz: %.2f dB\n", f, acc / n);
    }
    std::printf ("target: -9.57 -9.55 -9.66 -9.84 -10.33 -10.84 -11.50 -11.67\n");

    DynamicsSettings e;
    e.expThresholdDb = -20;
    e.expRangeDb = 40;
    e.compInvRatio = 1.0;
    const double knee = expander::kneeDbfs (-20);
    for (int gate = 0; gate < 2; ++gate)
    {
        e.gate = gate;
        for (double rel : { 0.1, 0.4, 1.0, 4.0 })
        {
            e.expReleaseS = rel;
            e.expFastAttack = rel == 0.4;
            auto r = stepRun (e, knee + 6, knee - 35, 1000, fs, 1.0, 10.0, true);
            std::printf ("%s rel %.1f: %.0f / %.0f / %.0f\n", gate ? "gate" : "exp ", rel, r.t10, r.t63, r.t90);
        }
        e.expReleaseS = 0.4;
        e.expFastAttack = false;
        auto o = stepRun (e, knee - 35, knee + 6, 1000, fs, 3.0, 1.0, true);
        e.expFastAttack = true;
        auto o2 = stepRun (e, knee - 35, knee + 6, 1000, fs, 3.0, 1.0, true);
        std::printf ("  opening t90 slow %.2f ms fast %.2f ms\n", o.t90, o2.t90);
    }
    std::printf ("targets exp: 24/68/96 58/166/236 128/364/520 470/1344/1922; gate 36/156/332 86/390/830 188/856/1826 626/2260/3700\n");
}
