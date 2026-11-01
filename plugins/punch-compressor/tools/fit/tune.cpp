// Tuning harness for ReleaseLaw (development aid, not built by CMake). Build against a copy of source/dsp in which
// "static constexpr double fastMs" is made "static inline double fastMs", with -DTABLE_DIR-independent args:
//   tune <csv> fastMs mChargeMs mMaxMs mPerRel sChargeMs gateDecay holdScale
// Prints: B <release> <row errors>, T (7.4 release), N (noise vs sine), L (LF H3), F (100 Hz level).
#include "../../tests/TestHelpers.h"
#include "../../tests/BurstTable.h"
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <thread>
using namespace testutil;

int main (int argc, char** argv)
{
    if (argc > 8)
    {
        ReleaseLaw::fastMs = atof (argv[2]); ReleaseLaw::mChargeMs = atof (argv[3]); ReleaseLaw::mMaxMs = atof (argv[4]);
        ReleaseLaw::mPerRel = atof (argv[5]); ReleaseLaw::sChargeMs = atof (argv[6]); ReleaseLaw::gateDecay = atof (argv[7]);
        ReleaseLaw::holdScale = atof (argv[8]);
        if (argc > 9) ReleaseLaw::sLeakMs = atof (argv[9]);
    }
    const double fs = 48000.0;
    const auto table = burst::loadTable (argv[1]);
    std::vector<std::function<void()>> jobs;
    std::vector<std::string> out (9);
    const double rels[6] = { 10, 30, 100, 300, 1000, 3000 };
    for (int r = 0; r < 6; ++r)
        jobs.push_back ([&, r] {
            const auto rows = burst::rowsFor (table, rels[r]);
            const auto res = burst::run (rows, rels[r], fs);
            double worst = 0, worstLate = 0, sum = 0; int n = 0; std::string detail;
            for (size_t i = 0; i < rows.size(); ++i)
                for (size_t k = 0; k < rows[i].values.size(); ++k)
                {
                    const double e = res[i][k] - rows[i].values[k];
                    sum += e * e; ++n;
                    if (burst::times[k] > 5.0) worstLate = std::max (worstLate, std::abs (e)); else worst = std::max (worst, std::abs (e));
                    if (std::abs (e) > (burst::times[k] <= 5.0 ? 3.0 : 1.5))
                    {
                        char b[64]; snprintf (b, 64, " [%g/%g t%g %+.1f]", rows[i].burstMs, rows[i].burstDb, burst::times[k], e); detail += b;
                    }
                }
            char b[96]; snprintf (b, 96, "B %g early %.2f late %.2f rms %.2f", rels[r], worst, worstLate, std::sqrt (sum / n));
            out[size_t (r)] = b + detail;
        });
    jobs.push_back ([&] {
        const double t[11] = { 0, 2, 5, 10, 20, 50, 100, 200, 400, 800, 1300 };
        struct R { double rel; double v[11]; };
        const R rows[] = { { 3, { -10.38, -4.76, -1.41, 0.49, 1.27, 1.36, 1.36, 1.36, 1.36, 1.36, 1.36 } },
                           { 30, { -7.98, -6.92, -5.56, -3.77, -1.24, 3.09, 4.81, 4.81, 4.81, 4.81, 4.81 } },
                           { 100, { -7.58, -7.16, -6.66, -5.92, -4.64, -1.84, 1.40, 5.30, 5.30, 5.30, 5.30 } },
                           { 300, { -7.45, -7.21, -6.96, -6.68, -6.17, -4.82, -3.05, -0.46, 4.21, 5.45, 5.45 } },
                           { 1000, { -7.42, -7.35, -7.30, -7.21, -7.04, -6.54, -5.79, -4.50, -2.49, 0.37, 3.69 } },
                           { 3000, { -7.42, -7.41, -7.39, -7.36, -7.30, -7.12, -6.84, -6.31, -5.36, -3.78, -2.21 } } };
        std::string l = "T";
        for (const auto& r : rows)
        {
            auto s = trajectorySettings(); s.releaseMs = r.rel;
            const auto tr = runTrajectory (fs, s);
            double w = 0;
            for (int i = 0; i < 11; ++i) w = std::max (w, std::abs (tr.gainAt (tr.down, t[i]) - r.v[i]));
            char b[32]; snprintf (b, 32, " %.2f", w); l += b;
        }
        out[6] = l;
    });
    jobs.push_back ([&] {
        const size_t n = size_t (4.0 * fs);
        const double level = dbToLin (-15.0);
        const auto sn = sine (fs, 1000.0, level * std::sqrt (2.0), n);
        const auto nz = gaussianNoise (n, level, 12345u);
        struct C { bool isA; double v, ref; };
        const C cs[] = { { true, 0.5, -4.51 }, { true, 2, -4.11 }, { true, 7, -3.62 }, { true, 30, -3.02 }, { true, 100, -2.55 }, { false, 3, -2.72 }, { false, 30, -3.23 }, { false, 300, -3.94 }, { false, 1000, -4.32 } };
        std::string l = "N";
        const size_t a = size_t (fs);
        for (const auto& c : cs)
        {
            auto s = trajectorySettings();
            if (c.isA) s.attackMs = c.v; else s.releaseMs = c.v;
            auto ys = runAligned (stereo (sn), fs, s).l; auto yn = runAligned (stereo (nz), fs, s).l;
            char b[32]; snprintf (b, 32, " %.2f", linToDb (rms (yn, a, n) / rms (nz, a, n)) - linToDb (rms (ys, a, n) / rms (sn, a, n)) - c.ref); l += b;
        }
        out[7] = l;
    });
    jobs.push_back ([&] {
        struct C { double rel, h3[3]; };
        const C cs[] = { { 3, { -23.8, -32.2, -44.3 } }, { 30, { -28.5, -37.6, -50.0 } }, { 100, { -30.9, -42.2, -55.1 } }, { 300, { -32.2, -46.6, -61.5 } } };
        const double fr[3] = { 50, 100, 200 };
        std::string l = "L";
        for (const auto& c : cs)
            for (int i = 0; i < 3; ++i)
            {
                auto s = trajectorySettings(); s.releaseMs = c.rel;
                const auto r = measureTone (fs, s, fr[i], dbToLin (-6.0), 1.0, 1.0);
                char b[32]; snprintf (b, 32, " %.1f", r.h3Dbc - c.h3[i]); l += b;
            }
        l += " F";
        const double a[3] = { 0.1, 0.3, 0.95 }, g[3] = { 4.86, -3.57, -10.23 }, h[3] = { -35.7, -45.2, -38.8 };
        for (int i = 0; i < 3; ++i)
        {
            const auto r = measureTone (fs, trajectorySettings(), 100.0, a[i], 1.0, 1.0);
            char b[40]; snprintf (b, 40, " %.2f/%.1f", r.gainDb - g[i], r.h3Dbc - h[i]); l += b;
        }
        out[8] = l;
    });
    std::vector<std::thread> th;
    for (auto& j : jobs) th.emplace_back (j);
    for (auto& t : th) t.join();
    for (const auto& l : out) printf ("%s\n", l.c_str());
}
