// Robustness: randomised automation of every engine setting while processing noise, impulses, silence,
// denormal-range and full-scale signals at several sample rates and block sizes. Every output sample must be
// finite and below +40 dBFS. (The plug-in level counterpart is tools/hoststress.)
#include <catch2/catch_test_macros.hpp>

#include <random>

#include "TestHelpers.h"

using namespace testutil;

namespace
{
Settings randomSettings (std::mt19937& rng, Settings s)
{
    std::uniform_real_distribution<double> u (0.0, 1.0);
    auto pick = [&] (double p) { return u (rng) < p; };
    auto extreme = [&] (double lo, double hi) {
        const double r = u (rng);
        return r < 0.2 ? lo : (r > 0.8 ? hi : lo + (hi - lo) * u (rng));
    };
    if (pick (0.3)) s.sync = static_cast<param::Sync> (int (u (rng) * 3.0) % 3);
    if (pick (0.3)) s.bpm = extreme (30.0, 300.0);
    if (pick (0.3)) s.hostBpm = pick (0.1) ? extreme (1.0, 1000.0) : extreme (30.0, 300.0);
    if (pick (0.3)) s.delayMs = param::DelayMs::fromNorm (u (rng));
    if (pick (0.3)) s.delayRMs = param::DelayMs::fromNorm (u (rng));
    if (pick (0.3)) s.delayNote = int (u (rng) * 20.0) % 20;
    if (pick (0.3)) s.delayRNote = int (u (rng) * 20.0) % 20;
    if (pick (0.2)) s.dual = ! s.dual;
    if (pick (0.3)) s.feedbackPct = extreme (0.0, 200.0);
    if (pick (0.2)) s.pingPong = ! s.pingPong;
    if (pick (0.2)) s.pingPongStartRight = ! s.pingPongStartRight;
    if (pick (0.2)) s.phaseL = ! s.phaseL;
    if (pick (0.2)) s.phaseR = ! s.phaseR;
    if (pick (0.3)) s.mixPct = extreme (0.0, 100.0);
    if (pick (0.3)) s.outputDb = extreme (-18.0, 18.0);
    if (pick (0.3)) s.analog = int (u (rng) * 5.0) % 5;
    if (pick (0.2)) s.lofi = ! s.lofi;
    if (pick (0.3)) s.modRateHz = param::ModRate::fromNorm (extreme (0.0, 1.0));
    if (pick (0.3)) s.modDepthPct = extreme (0.0, 100.0);
    if (pick (0.3)) s.hiPassHz = param::HiPass::fromNorm (extreme (0.0, 1.0));
    if (pick (0.3)) s.loPassHz = param::LoPass::fromNorm (extreme (0.0, 1.0));
    return s;
}

void fillSignal (std::mt19937& rng, int kind, std::vector<double>& l, std::vector<double>& r)
{
    std::uniform_real_distribution<double> u (-1.0, 1.0);
    for (size_t i = 0; i < l.size(); ++i)
    {
        double a = 0.0, b = 0.0;
        switch (kind)
        {
            case 0: a = u (rng); b = u (rng); break;                                       // full-scale noise
            case 1: a = (u (rng) > 0.995) ? 1.0 : 0.0; b = -a; break;                       // impulses
            case 2: break;                                                                   // silence
            case 3: a = 1.0e-39 * u (rng); b = 1.0e-310 * u (rng); break;                    // denormal range
            case 4: a = b = ((i / 37) % 2) ? 1.0 : -1.0; break;                              // full-scale square
            default: a = 1.0; b = -1.0; break;                                               // DC
        }
        l[i] = a;
        r[i] = b;
    }
}
} // namespace

TEST_CASE ("Engine stress: random automation, signals, rates and block sizes stay finite and bounded", "[stress]")
{
    std::mt19937 rng (12345);
    std::uniform_int_distribution<int> blockDist (1, 4096);
    for (double fs : { 44100.0, 48000.0, 96000.0, 192000.0 })
        for (auto mode : { InputMode::Stereo, InputMode::MonoToStereo })
        {
            HybridDelayEngine e;
            e.setInputMode (mode);
            Settings s;
            e.setSettings (s);
            e.prepare (fs);
            const auto total = size_t (fs * 20.0); // 20 s per configuration
            size_t done = 0;
            int kind = 0;
            std::vector<double> l, r;
            while (done < total)
            {
                const int n = blockDist (rng);
                if (rng() % 8 == 0)
                    s = randomSettings (rng, s);
                e.setSettings (s);
                if (rng() % 16 == 0)
                    kind = int (rng() % 6);
                if (rng() % 400 == 0)
                    e.setBypassed (rng() % 2 == 0);
                if (rng() % 1500 == 0)
                    e.prepare (fs); // re-prepare mid-stream
                l.resize (size_t (n));
                r.resize (size_t (n));
                fillSignal (rng, kind, l, r);
                e.process (l.data(), r.data(), n);
                for (int i = 0; i < n; ++i)
                {
                    const bool ok = std::isfinite (l[size_t (i)]) && std::isfinite (r[size_t (i)])
                                 && std::abs (l[size_t (i)]) < 100.0 && std::abs (r[size_t (i)]) < 100.0;
                    if (! ok)
                    {
                        INFO ("fs " << fs << " sample " << done + size_t (i) << ": " << l[size_t (i)] << ", " << r[size_t (i)]);
                        REQUIRE (ok);
                    }
                }
                done += size_t (n);
            }
        }
}
