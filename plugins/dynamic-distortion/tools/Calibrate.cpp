// Craft Dynamic Distortion - calibration and report tool (development aid, not shipped).
//
//   dynamic_distortion_calibrate fit [holdMs]             fits the expansion knots to spec section 4.3, prints them
//   dynamic_distortion_calibrate report [fs] [holdMs]     section 4.3 / 6.1 / 6.2 / 7 comparison (built-in curve
//                                                         unless holdMs is given, then the curve is refitted first)
//
// The fit runs the complete engine (Drive 0 %, Attack 1 ms, Release 31.8 ms, 1 kHz, 48 kHz) and moves each knot
// by the remaining error at its table level until the measured gain increase matches the table.
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "../tests/SpecTables.h"
#include "../tests/TestHelpers.h"

using namespace ddtest;

namespace
{
double expansionIncrease (double level, double fs = 48000.0)
{
    const auto s = distortionOnly (0.0);
    const double small = measureTone (s, fs, 1000.0, -50.0, 0.4, 0.2).gainDb;
    return measureTone (s, fs, 1000.0, level, 0.4, 0.2).gainDb - small;
}

expansion::Curve fitCurve (bool verbose)
{
    static expansion::Curve c;
    c = expansion::defaultCurve();
    tweaks().curve = &c;
    const auto& L = ddspec::expansionLevels;
    const auto& G = ddspec::expansionGains;
    // knots 1..14 sit at the table levels converted to dB peak; knot 0 is the onset 3 dB below
    for (size_t i = 0; i < L.size(); ++i)
        c.knots[i + 1] = { L[i] + 3.0103, G[i] };
    c.knots[0] = { c.knots[1].levelDb - 3.0, 0.0 };
    for (int it = 0; it < 15; ++it)
    {
        double worst = 0.0;
        for (size_t i = 1; i < L.size(); ++i) // level -40 (knot 1) stays 0
        {
            const double err = G[i] - expansionIncrease (L[i]);
            worst = std::max (worst, std::abs (err));
            c.knots[i + 1].gainDb += 0.9 * err;
        }
        if (verbose)
            std::printf ("iteration %d: worst error %.4f dB\n", it, worst);
        if (worst < 0.004)
            break;
    }
    return c;
}

void report (double fs)
{
    int fails = 0, checks = 0;
    std::printf ("== section 4.3 (fs %.0f)\n", fs);
    for (size_t i = 0; i < ddspec::expansionLevels.size(); ++i)
    {
        const double got = expansionIncrease (ddspec::expansionLevels[i], fs);
        const bool ok = std::abs (got - ddspec::expansionGains[i]) <= 0.4;
        fails += ok ? 0 : 1;
        ++checks;
        std::printf ("%6.0f: table %+5.2f  got %+5.2f%s\n", ddspec::expansionLevels[i], ddspec::expansionGains[i], got,
                     ok ? "" : "  FAIL");
    }

    for (const auto& t : ddspec::toneTables)
    {
        std::printf ("== %s (fs %.0f): gain err / h3 got (table)\n", t.name, fs);
        for (int fi = 0; fi < 4; ++fi)
        {
            std::printf ("%5.0f:", ddspec::toneFreqs[size_t (fi)]);
            for (int li = 0; li < 6; ++li)
            {
                const auto r = measureTone (distortionOnly (t.drive, t.attack, t.release), fs, ddspec::toneFreqs[size_t (fi)],
                                            ddspec::toneLevels[size_t (li)], 1.0, 0.25);
                const double ge = r.gainDb - t.cells[fi][li][0];
                const bool gOk = std::abs (ge) <= ddspec::fundamentalTolerance (t, fi, li);
                const bool hOk = ddspec::h3Ok (t, li, t.cells[fi][li][1], r.h3Dbc);
                fails += (gOk ? 0 : 1) + (hOk ? 0 : 1);
                checks += 2;
                std::printf (" %+5.1f%s %+4.0f%s(%+4.0f)", ge, gOk ? " " : "!", std::max (r.h3Dbc, -999.0), hOk ? " " : "!",
                             t.cells[fi][li][1]);
            }
            std::printf ("\n");
        }
    }

    std::printf ("== section 6.2 (fs %.0f): got (table)   [! = outside +-2.5 dB; down+10 informative]\n", fs);
    const std::vector<double> upMs (ddspec::stepUpMs.begin(), ddspec::stepUpMs.end());
    const std::vector<double> downMs (ddspec::stepDownMs.begin(), ddspec::stepDownMs.end());
    for (const auto& r : ddspec::stepRows)
    {
        const bool longRelease = r.release > 1000.0;
        const auto tr = stepResponse (distortionOnly (0.0, r.attack, r.release), fs, upMs, downMs, longRelease ? 6.0 : 1.0);
        std::printf ("A%5.1f R%6.1f:", r.attack, r.release);
        for (size_t i = 0; i < 7; ++i)
        {
            // Release 3000: the table's 'up' values reflect the reference's preceding programme; after >= 5 s at
            // -50 dBFS the attack phase equals the A 1 / R 31.8 row.
            const double target = longRelease ? ddspec::stepRows[1].v[i] : r.v[i];
            const bool ok = std::abs (tr.up[i] - target) <= ddspec::stepTolerance;
            fails += ok ? 0 : 1;
            ++checks;
            std::printf (" %+5.2f%s(%+5.2f)", tr.up[i], ok ? " " : "!", target);
        }
        std::printf (" |");
        for (size_t i = 0; i < 6; ++i)
        {
            const bool ok = i == 0 || std::abs (tr.down[i] - r.v[7 + i]) <= ddspec::stepTolerance;
            if (i > 0)
            {
                fails += ok ? 0 : 1;
                ++checks;
            }
            std::printf (" %+5.2f%s(%+5.2f)", tr.down[i], ok ? " " : "!", r.v[7 + i]);
        }
        std::printf ("\n");
    }

    // section 7 / D1: 15 kHz, -6 dBFS, Drive 50 %
    {
        auto s = distortionOnly (50.0);
        const double amp = sinePeak (-6.0), f = 15000.0;
        const size_t settle = size_t (0.3 * fs), n = size_t (0.5 * fs);
        auto y = render (s, fs, settle + n, [&] (size_t i) { return amp * std::sin (2.0 * pi * f * double (i) / fs); });
        const double fund = toneAmplitude (y, settle, n, f, fs);
        double worst = -300, worstF = 0;
        for (double g = 20.0; g < 0.5 * fs - 20; g += 10.0)
        {
            if (std::abs (std::fmod (g, f)) < 30.0 || std::abs (std::fmod (g, f) - f) < 30.0)
                continue; // harmonics of the tone
            const double a = db (toneAmplitude (y, settle, n, g, fs) / fund);
            if (a > worst)
            {
                worst = a;
                worstF = g;
            }
        }
        std::printf ("== aliasing (fs %.0f): 3 kHz %.1f dBc, worst %.1f dBc at %.0f Hz\n", fs,
                     db (toneAmplitude (y, settle, n, 3000.0, fs) / fund), worst, worstF);
    }
    std::printf ("== %d of %d checks outside tolerance\n", fails, checks);
}
} // namespace

int main (int argc, char** argv)
{
    if (argc > 1 && std::strcmp (argv[1], "fit") == 0)
    {
        if (argc > 2)
            tweaks().holdMs = std::atof (argv[2]);
        const auto c = fitCurve (true);
        for (const auto& k : c.knots)
            std::printf ("{ %.2f, %.3f },\n", k.levelDb, k.gainDb);
        return 0;
    }
    const double fs = argc > 2 ? std::atof (argv[2]) : 48000.0;
    if (argc > 3)
    {
        tweaks().holdMs = std::atof (argv[3]);
        fitCurve (false);
    }
    report (fs);
    return 0;
}
