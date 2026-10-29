// Tuning harness for the fast-stage / charge constants of GainSmoother.h (development aid, not built by CMake).
// Build against a copy of source/dsp in which ReleaseLaw's "static constexpr double fastF0 = ..." line is made
// "static inline double ..." (so it can be set at run time), e.g.
//   g++ -O2 -std=c++17 -pthread -I<copy containing dsp/> tune.cpp -o tune
// Usage: tune f0 pf kf c0 pc   -> per-row release-table errors, noise-vs-sine errors (N), LF-ripple H3 errors (L)
#include "../../tests/TestHelpers.h"
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <functional>
using namespace testutil;

static Settings with (const char* k, double v)
{
    Settings s = trajectorySettings();
    std::string key (k);
    if (key == "A") s.attackMs = v;
    else if (key == "Rel") s.releaseMs = v;
    else if (key == "Host") { s.sync = param::Sync::Host; s.hostBpm = 120.0; s.note = int (v); }
    return s;
}

int main (int argc, char** argv)
{
    if (argc > 5)
    {
        ReleaseLaw::fastF0 = atof (argv[1]); ReleaseLaw::fastPf = atof (argv[2]); ReleaseLaw::fastKf = atof (argv[3]);
        ReleaseLaw::chargeC0 = atof (argv[4]); ReleaseLaw::chargePc = atof (argv[5]);
    }
    const double fs = 48000.0;
    std::vector<std::function<void()>> jobs;
    // release rows
    const double times[11] = { 0, 2, 5, 10, 20, 50, 100, 200, 400, 800, 1300 };
    struct RRow { const char* name; Settings s; std::vector<double> ref; };
    std::vector<RRow> rows = {
        { "Rel3", with ("Rel", 3), { -10.38, -4.76, -1.41, 0.49, 1.27, 1.36, 1.36, 1.36, 1.36, 1.36, 1.36 } },
        { "Rel30", with ("Rel", 30), { -7.98, -6.92, -5.56, -3.77, -1.24, 3.09, 4.81, 4.81, 4.81, 4.81, 4.81 } },
        { "Rel100", with ("Rel", 100), { -7.58, -7.16, -6.66, -5.92, -4.64, -1.84, 1.40, 5.30, 5.30, 5.30, 5.30 } },
        { "Rel300", with ("Rel", 300), { -7.45, -7.21, -6.96, -6.68, -6.17, -4.82, -3.05, -0.46, 4.21, 5.45, 5.45 } },
        { "Rel1000", with ("Rel", 1000), { -7.42, -7.35, -7.30, -7.21, -7.04, -6.54, -5.79, -4.50, -2.49, 0.37, 3.69 } },
        { "Rel3000", with ("Rel", 3000), { -7.42, -7.41, -7.39, -7.36, -7.30, -7.12, -6.84, -6.31, -5.36, -3.78, -2.21 } },
        { "H18", with ("Host", 20), { -7.46, -7.21, -6.94, -6.61, -6.00, -4.45, -2.47, 0.36, 5.44, 5.44, 5.44 } },
        { "H14", with ("Host", 27), { -7.43, -7.22, -7.01, -6.82, -6.50, -5.61, -4.36, -2.39, 0.44, 5.48, 5.48 } },
    };
    std::vector<std::string> out (rows.size() + 2);
    for (size_t r = 0; r < rows.size(); ++r)
        jobs.push_back ([&, r] {
            auto tr = runTrajectory (fs, rows[r].s);
            std::string l = std::string ("T ") + rows[r].name;
            for (int i = 0; i < 11; ++i) { char b[32]; snprintf (b, 32, " %.3f", tr.gainAt (tr.down, times[i]) - rows[r].ref[size_t (i)]); l += b; }
            // rms vs interpolated
            double sum = 0; int n = 0;
            for (double ms = 0; ms <= 1300; ms += 1, ++n)
            {
                size_t k = 0; while (k + 2 < 11 && times[k + 1] <= ms) ++k;
                double f = std::clamp ((ms - times[k]) / (times[k + 1] - times[k]), 0.0, 1.0);
                double ref = rows[r].ref[k] + f * (rows[r].ref[k + 1] - rows[r].ref[k]);
                double e = tr.gainAt (tr.down, ms) - ref; sum += e * e;
            }
            char b[32]; snprintf (b, 32, " rms %.3f", std::sqrt (sum / n)); l += b;
            out[r] = l;
        });
    // noise
    jobs.push_back ([&] {
        const size_t n = size_t (4.0 * fs);
        const double level = dbToLin (-15.0);
        const auto sn = sine (fs, 1000.0, level * std::sqrt (2.0), n);
        const auto nz = gaussianNoise (n, level, 12345u);
        struct C { const char* k; double v; double ref; };
        C cs[] = { { "A", 0.5, -4.51 }, { "A", 2, -4.11 }, { "A", 7, -3.62 }, { "A", 30, -3.02 }, { "A", 100, -2.55 }, { "Rel", 3, -2.72 }, { "Rel", 30, -3.23 }, { "Rel", 300, -3.94 }, { "Rel", 1000, -4.32 } };
        std::string l = "N";
        const size_t a = size_t (fs);
        for (auto& c : cs)
        {
            auto s = with (c.k, c.v);
            auto ys = runAligned (stereo (sn), fs, s).l; auto yn = runAligned (stereo (nz), fs, s).l;
            double d = linToDb (rms (yn, a, n) / rms (nz, a, n)) - linToDb (rms (ys, a, n) / rms (sn, a, n)) - c.ref;
            char b[32]; snprintf (b, 32, " %.3f", d); l += b;
        }
        out[rows.size()] = l;
    });
    jobs.push_back ([&] {
        struct C { double rel; double h3[3]; };
        C cs[] = { { 3, { -23.8, -32.2, -44.3 } }, { 30, { -28.5, -37.6, -50.0 } }, { 100, { -30.9, -42.2, -55.1 } }, { 300, { -32.2, -46.6, -61.5 } } };
        double fr[3] = { 50, 100, 200 };
        std::string l = "L";
        for (auto& c : cs)
            for (int i = 0; i < 3; ++i)
            {
                auto s = with ("Rel", c.rel);
                auto r = measureTone (fs, s, fr[i], dbToLin (-6.0), 0.6, 1.0);
                char b[32]; snprintf (b, 32, " %.2f", r.h3Dbc - c.h3[i]); l += b;
            }
        out[rows.size() + 1] = l;
    });
    std::vector<std::thread> th;
    for (auto& j : jobs) th.emplace_back (j);
    for (auto& t : th) t.join();
    for (auto& l : out) printf ("%s\n", l.c_str());
}
