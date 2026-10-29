// Sections 4 and 5: static curve, correction table, combination rule and automatic makeup; steady-state levels
// through the complete engine (tolerance 13: +-0.2 dB).
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "TestHelpers.h"

using namespace testutil;
using Catch::Approx;

namespace
{
const double dCols[11] = { -2, -1, 0, 1, 2, 3, 6, 9, 12, 18, 24 };
constexpr double na = 99.0; // "—" in the spec table
const double table4[5][11] = {
    { -0.00, 0.00, 0.00, -0.32, -0.42, -0.46, -0.52, na, na, na, na },
    { na, na, 0.00, na, na, -0.46, -0.52, -0.54, -0.55, na, na },
    { 0.00, 0.00, 0.01, -0.26, -0.35, -0.41, -0.48, -0.52, -0.54, -0.56, -0.57 },
    { na, na, 0.16, na, na, -0.20, -0.33, -0.41, -0.46, -0.52, -0.55 },
    { 0.09, 0.51, 1.03, 0.85, 0.69, 0.56, 0.23, 0.00, -0.17, -0.37, -0.48 },
};
const double tRows[5] = { -6, -12, -24, -36, -48 };
const double ratioRows[4] = { 1.25, 1.5, 2.0, 50.0 };
const double table24[4][11] = {
    { -0.00, -0.00, 0.00, -0.14, -0.22, -0.29, -0.39, -0.44, -0.47, -0.51, -0.53 },
    { -0.00, 0.00, 0.00, -0.19, -0.29, -0.35, -0.44, -0.49, -0.51, -0.54, -0.56 },
    { 0.00, -0.00, 0.01, -0.23, -0.33, -0.39, -0.47, -0.51, -0.53, -0.56, -0.57 },
    { -0.00, 0.00, 0.02, -0.27, -0.36, -0.41, -0.48, -0.52, -0.54, -0.56, -0.57 },
};

// Static gain (dB, output re input) for a level D, as specified: -GR + makeup.
double specGain (double D, double T, double R, double rel)
{
    const double u = curve::ratioToU (R);
    return -curve::gainReduction (D, T, u) + curve::makeupDb (T, u, rel);
}
} // namespace

TEST_CASE ("Correction table: Ratio-4 rows reproduce every tabulated cell", "[static]")
{
    const double u4 = 0.75;
    for (int t = 0; t < 5; ++t)
        for (int c = 0; c < 11; ++c)
            if (table4[t][c] != na)
                CHECK (curve::delta (tRows[t], dCols[c], u4) == Approx (table4[t][c]).margin (1.0e-9));
}

TEST_CASE ("Correction table: low-ratio rows at T -24 and the R >= 2 rule", "[static]")
{
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 11; ++c)
            CHECK (curve::delta (-24.0, dCols[c], curve::ratioToU (ratioRows[r])) == Approx (table24[r][c]).margin (1.0e-9));
    // R = 50 row: within +-0.05 dB of the R = 4 row (the spec's ratio independence for R >= 2)
    for (int c = 0; c < 11; ++c)
        CHECK (curve::delta (-24.0, dCols[c], curve::ratioToU (50.0)) == Approx (table24[3][c]).margin (0.05));
}

TEST_CASE ("Combination rule, clamps and knee", "[static]")
{
    // Delta = Delta24(d,R) + [Delta4(T,d) - Delta4(-24,d)] (1 - 1/R) / 0.75
    const double u = curve::ratioToU (8.0);
    for (double d : { -1.5, 0.0, 0.5, 4.0, 10.0, 20.0 })
        for (double T : { -10.0, -30.0, -40.0, -47.0 })
            CHECK (curve::delta (T, d, u)
                   == Approx (curve::delta24At (d, u) + (curve::delta4At (T, d) - curve::delta4At (-24.0, d)) * u / 0.75));
    // beyond the table
    CHECK (curve::delta (-24.0, -2.5, 0.75) == 0.0);
    CHECK (curve::delta (-24.0, 40.0, 0.75) == Approx (-0.57));
    CHECK (curve::delta4At (-3.0, 3.0) == Approx (curve::delta4At (-6.0, 3.0)));
    CHECK (curve::delta4At (-60.0, 3.0) == Approx (0.56));
    // the low-threshold knee: positive reduction below threshold, scaling with 1 - 1/R
    CHECK (curve::gainReduction (-49.0, -48.0, 0.75) == Approx (0.51));
    CHECK (curve::gainReduction (-49.0, -48.0, curve::ratioToU (2.0))
           == Approx (0.0 + (0.51 - 0.0) * 0.5 / 0.75).margin (1.0e-9));
    CHECK (curve::gainReduction (-50.5, -24.0, 0.75) == 0.0);
    // monotone static curve (GR non-decreasing with level) for all thresholds / ratios
    for (double T = 0.0; T >= -48.0; T -= 4.0)
        for (double R : { 1.12, 1.5, 2.0, 3.0, 4.0, 10.0, 50.0 })
        {
            double prev = 0.0;
            for (double D = -60.0; D <= 6.0; D += 0.05)
            {
                const double gr = curve::gainReduction (D, T, curve::ratioToU (R));
                CHECK (gr >= prev - 1.0e-9);
                prev = gr;
            }
        }
    // Ratio 1: no reduction at all
    for (double D = -60.0; D <= 6.0; D += 0.5)
        CHECK (curve::gainReduction (D, -48.0, 0.0) == 0.0);
}

TEST_CASE ("Automatic makeup: tables and examples", "[static][makeup]")
{
    CHECK (curve::makeupDb (-24.0, 0.75, 100.0) == Approx (5.30).margin (0.005));
    CHECK (curve::makeupDb (-48.0, curve::ratioToU (50.0), 1000.0) == Approx (19.05).margin (0.01));
    CHECK (curve::makeupDb (-9.0, 0.75, 100.0) == 0.0);
    CHECK (curve::makeupDb (-5.0, 0.98, 3000.0) == 0.0);
    CHECK (curve::makeupDb (-48.0, 0.0, 100.0) == 0.0);
    const double R[8] = { 1.25, 1.5, 2, 3, 4, 8, 20, 50 };
    const double k[8] = { 0.0108, 0.1056, 0.2166, 0.3191, 0.3670, 0.4351, 0.4736, 0.4886 };
    for (int i = 0; i < 8; ++i)
        CHECK (curve::makeupK (curve::ratioToU (R[i])) == Approx (k[i]).margin (1.0e-9));
    const double rel[7] = { 3, 10, 30, 100, 300, 1000, 3000 };
    const double s[7] = { 0.2476, 0.6596, 0.8730, 0.9628, 0.9902, 1.0000, 1.0029 };
    for (int i = 0; i < 7; ++i)
        CHECK (curve::makeupS (rel[i]) == Approx (s[i]).margin (1.0e-9));
    CHECK (curve::makeupS (1.0) == Approx (0.2476));
    CHECK (curve::makeupS (10000.0) == Approx (1.0029));
    // log interpolation: Rel 250 between 100 and 300
    CHECK (curve::makeupS (250.0) == Approx (0.9628 + (0.9902 - 0.9628) * std::log10 (2.5) / std::log10 (3.0)));
}

TEST_CASE ("Steady 1 kHz sines through the engine match static curve + makeup (+-0.2 dB)", "[static][engine]")
{
    const double fs = 48000.0;
    struct Case
    {
        double T, R, rel, level;
    };
    const Case cases[] = {
        { -24, 4, 100, -6 },  { -24, 4, 100, -40 }, { -24, 4, 100, -22 }, { -24, 4, 100, -25 }, { -6, 4, 100, -2 },
        { -12, 2, 30, 0 },    { -36, 8, 300, -20 }, { -48, 50, 1000, -10 }, { -48, 4, 100, -49 }, { -48, 4, 100, -47 },
        { -36, 1.5, 3, -30 }, { -18, 3, 100, -12 }, { -30, 20, 3000, -3 }, { -42, 1.25, 10, -20 }, { 0, 4, 100, -1 },
    };
    for (const auto& c : cases)
    {
        Settings s = neutralSettings();
        s.thresholdDb = c.T;
        s.ratio = c.R;
        s.releaseMs = c.rel;
        const auto r = measureTone (fs, s, 1000.0, dbToLin (c.level), 1.5, 0.2);
        INFO ("T " << c.T << " R " << c.R << " Rel " << c.rel << " level " << c.level);
        CHECK (r.gainDb == Approx (specGain (c.level, c.T, c.R, c.rel)).margin (0.2));
        CHECK (r.h3Dbc < -120.0); // section 6: no gain ripple on a steady 1 kHz sine
    }
}

TEST_CASE ("Steady state is sample-rate independent (44.1 / 48 / 96 kHz within 0.03 dB)", "[static][engine][rates]")
{
    for (const double level : { -6.0, -20.0, -40.0 })
    {
        Settings s = trajectorySettings();
        const double ref = measureTone (48000.0, s, 1000.0, dbToLin (level), 1.0, 0.2).gainDb;
        for (double fs : { 44100.0, 96000.0 })
            CHECK (measureTone (fs, s, 1000.0, dbToLin (level), 1.0, 0.2).gainDb == Approx (ref).margin (0.03));
    }
}
