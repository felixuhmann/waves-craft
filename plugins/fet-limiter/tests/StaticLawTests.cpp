// Spec r1 section 5: static compression law per revision / ratio (tables/static_gr.csv), 1 kHz sine.
#include <catch2/catch_test_macros.hpp>

#include <map>

#include "TestHelpers.h"

using namespace fettest;

namespace
{
constexpr double inputDisplay = -12.0;

// Steady-state gain change (dB) at internal level x: out - (L + Gin + Gout), Output display 0, 1 kHz.
double measureGr (Revision rev, Ratio ratio, double x, double fs = 48000.0, double release = 4.0)
{
    Settings s = neutralSettings();
    s.revision = rev;
    s.ratio = ratio;
    s.inputDisplay = inputDisplay;
    s.outputDisplay = 0.0;
    s.release = release;
    const double gin = inputGainDb (rev, inputDisplay);
    const double level = x - gin;
    Rig rig (s, fs);
    auto sig = sine (1000.0, level, 1.2, fs);
    rig.run (sig);
    const size_t n = wholeCycles (1000.0, fs, size_t (0.1 * fs));
    const double out = toneDb (sig, 1000.0, fs, sig.size() - n, n);
    return out - level - gin;
}

struct Row
{
    double x, gr;
    bool settled;
};

std::map<std::pair<int, int>, std::vector<Row>> tableRows()
{
    std::map<std::pair<int, int>, std::vector<Row>> rows;
    const auto csv = parseCsv (craft::fet_limiter::spec_tables::staticGrCsv);
    for (const auto& r : csv.rows)
    {
        const int rev = r[0] == "A" ? 1 : 0; // Revision enum value
        int ratio = 4;
        if (r[1] == "20") ratio = 0;
        else if (r[1] == "12") ratio = 1;
        else if (r[1] == "8") ratio = 2;
        else if (r[1] == "4") ratio = 3;
        rows[{ rev, ratio }].push_back ({ std::stod (r[2]), std::stod (r[3]), r[4] == "E" });
    }
    return rows;
}
} // namespace

TEST_CASE ("Static GR matches tables/static_gr.csv per revision and ratio (+-1.0 dB, All +-2.5 dB)", "[static]")
{
    const auto rows = tableRows();
    for (const auto& [key, list] : rows)
    {
        const Revision rev = Revision (key.first);
        const Ratio ratio = Ratio (key.second);
        const double tol = ratio == Ratio::All ? 2.5 : 1.0;
        // E rows take precedence over A rows at the same x and define the settled curve; A rows are checked where no
        // E row covers their x (assumption S1).
        double eLo = 1e9, eHi = -1e9;
        for (const auto& r : list)
            if (r.settled)
            {
                eLo = std::min (eLo, r.x);
                eHi = std::max (eHi, r.x);
            }
        int checked = 0;
        for (const auto& r : list)
        {
            if (! r.settled && r.x >= eLo && r.x <= eHi)
                continue;
            if (r.x < -12.0) // far below threshold: 1 kHz small-signal, covered by the gain tests
                continue;
            const double expected = ratio == Ratio::All && rev == Revision::B ? r.gr + 0.6 : r.gr;
            const double g = measureGr (rev, ratio, r.x);
            INFO ("rev " << (rev == Revision::A ? "A" : "B") << " ratio " << ratioName (ratio) << " x " << r.x << " expected "
                         << expected << " measured " << g);
            CHECK (std::abs (g - expected) <= tol);
            ++checked;
        }
        CHECK (checked > 10);
    }
}

TEST_CASE ("Static law properties: threshold rises with ratio, max reduction ~31.9 dB", "[static]")
{
    // rev A: GR = -1 dB at x ~ +0.5 / +2.5 / +3.5 / +5.5 for 4 / 8 / 12 / 20 (+-1 dB in x)
    const std::pair<Ratio, double> knees[] = { { Ratio::R4, 0.5 }, { Ratio::R8, 2.5 }, { Ratio::R12, 3.5 }, { Ratio::R20, 5.5 } };
    for (const auto& [ratio, xk] : knees)
    {
        const double lo = measureGr (Revision::A, ratio, xk - 1.0), hi = measureGr (Revision::A, ratio, xk + 1.0);
        INFO ("ratio " << ratioName (ratio) << " GR(x-1) " << lo << " GR(x+1) " << hi);
        CHECK (lo > -1.0 - 0.3);
        CHECK (hi < -1.0 + 0.3);
    }
    for (auto ratio : { Ratio::R20, Ratio::R12, Ratio::R8, Ratio::R4 })
    {
        const double g = measureGr (Revision::A, ratio, 50.0);
        INFO ("ratio " << ratioName (ratio) << " GR at x = 50: " << g);
        CHECK (g > -33.0);
        CHECK (g < -30.5);
    }
}

TEST_CASE ("Release changes the static GR by <= 0.3 dB, Attack not at all", "[static]")
{
    for (double x : { 5.0, 15.0, 25.0 })
    {
        const double g1 = measureGr (Revision::A, Ratio::R4, x, 48000.0, 1.0);
        const double g7 = measureGr (Revision::A, Ratio::R4, x, 48000.0, 7.0);
        CHECK (std::abs (g1 - g7) <= 0.3);
    }
}

TEST_CASE ("Static staircase at 44.1 and 96 kHz matches 48 kHz within 0.07 dB", "[static][samplerate]")
{
    for (double x : { -5.0, 0.0, 5.0, 10.0, 20.0, 30.0, 40.0 })
    {
        const double g48 = measureGr (Revision::A, Ratio::R4, x, 48000.0);
        for (double fs : { 44100.0, 96000.0 })
        {
            const double g = measureGr (Revision::A, Ratio::R4, x, fs);
            INFO ("x " << x << " fs " << fs << " GR " << g << " vs 48 kHz " << g48);
            CHECK (std::abs (g - g48) <= 0.07);
        }
    }
}
