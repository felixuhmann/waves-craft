// Band split, latency, neutrality: spec r1 sections 2, 3, 4 at 44.1 / 48 / 96 kHz.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "SpecTables.h"
#include "TestHelpers.h"

using namespace helpers;

namespace
{
// Band b response magnitude measured through solo at frequency f (steady tone well below the threshold).
double soloedBandAtTone (Variant v, double fs, int band, double f)
{
    auto s = neutral();
    s.wordLength = 24;
    if (band >= 0)
        s.solo[size_t (band)] = true;
    Rig rig (v, fs, s);
    const int n = int (fs * 0.6);
    const auto y = rig.run (tones ({ { f, -40.0 } }, fs, n));
    const int len = int (fs * 0.25);
    return toneAmplitude (y, f, fs, n - len, len) / dbToLin (-40.0);
}
} // namespace

TEST_CASE ("LP kernels match the normative low-pass table", "[crossover][lp]")
{
    const double fs = GENERATE (44100.0, 48000.0, 96000.0);
    const int numColumns = 8;
    for (int c = 0; c < numColumns; ++c)
    {
        const double fc = spec_tables::xoColumns[c];
        LinearPhaseSplitter sp;
        sp.setCrossovers ({ fc, fc, fc, fc });
        sp.prepare (fs);

        double worst = 0.0, worstOffset = 0.0;
        for (const auto& row : spec_tables::xoLinear)
        {
            const double expected = row[c + 1], f = fc * std::pow (2.0, row[0]);
            if (std::isnan (expected) || f > 0.45 * fs || f < 8.0)
                continue;
            const double err = std::abs (sp.realisedLowpass (0, f) - expected);
            if (err > worst)
            {
                worst = err;
                worstOffset = row[0];
            }
        }
        INFO ("fs " << fs << " fc " << fc << " worst at octave " << worstOffset << " = " << worst);
        CHECK (worst <= 0.02);
    }
}

TEST_CASE ("LL band magnitudes match the normative table", "[crossover][ll]")
{
    const double fs = GENERATE (44100.0, 48000.0, 96000.0);
    int rowIndex = 0;
    for (const auto& row : spec_tables::xoLowLatency)
    {
        if (rowIndex++ % 4 != 0 || row[0] > 0.45 * fs)
            continue;
        for (int b = 0; b < numBands; ++b)
        {
            const double m = soloedBandAtTone (Variant::LowLatencyMulti, fs, b, row[0]);
            INFO ("fs " << fs << " f " << row[0] << " band " << b + 1);
            CHECK (std::abs (m - row[b + 1]) <= 0.03);
        }
    }
}

TEST_CASE ("LL magnitudes scale with the crossovers in octaves; sum stays flat", "[crossover][ll]")
{
    const double fs = GENERATE (44100.0, 48000.0, 96000.0);
    auto s = neutral();
    s.crossoverHz = { 160.0, 640.0, 2556.0, 10226.0 };
    for (double f : { 200.0, 640.0, 2556.0, 10226.0 })
    {
        if (f > 0.45 * fs)
            continue;
        s.solo = { false, true, false, false, false };
        Rig solo (Variant::LowLatencyMulti, fs, s);
        const int n = int (fs * 0.5), len = int (fs * 0.2);
        const auto yb = solo.run (tones ({ { f, -40.0 } }, fs, n));
        const double m = toneAmplitude (yb, f, fs, n - len, len) / dbToLin (-40.0);

        auto base = neutral();
        base.solo = s.solo;
        Rig ref (Variant::LowLatencyMulti, fs, base);
        const auto yr = ref.run (tones ({ { f / 2.0, -40.0 } }, fs, n));
        const double mr = toneAmplitude (yr, f / 2.0, fs, n - len, len) / dbToLin (-40.0);
        INFO ("f " << f);
        CHECK (std::abs (m - mr) <= 0.03);
    }

    // sum flat within 0.05 dB (no solo)
    for (double f : { 25.0, 80.0, 320.0, 700.0, 1278.0, 3000.0, 5113.0, 12000.0 })
    {
        const double a = soloedBandAtTone (Variant::LowLatencyMulti, fs, -1, f);
        INFO ("f " << f << " dB " << linToDb (a));
        CHECK (std::abs (linToDb (a)) <= 0.05);
    }
}

TEST_CASE ("LL group delay is close to the informative measurement", "[crossover][ll]")
{
    // Phase slope of the common allpass at 48 kHz: informative only, so a wide factor.
    const double fs = 48000.0;
    auto s = neutral();
    Rig rig (Variant::LowLatencyMulti, fs, s);
    std::vector<double> x (size_t (fs), 0.0);
    x[0] = 0.5;
    const auto y = rig.run (x);
    int pk = 0;
    for (int i = 0; i < int (y.size()); ++i)
        if (std::abs (y[size_t (i)]) > std::abs (y[size_t (pk)]))
            pk = i;
    CHECK (pk >= BandLimiterEngine::llLatency);
    CHECK (pk < BandLimiterEngine::llLatency + 40);
}

TEST_CASE ("Latency is the specified value and neutral LP output is the delayed input", "[latency][lp]")
{
    const double fs = GENERATE (44100.0, 48000.0, 96000.0);
    const auto variant = GENERATE (Variant::LinearMulti, Variant::LinearProfile);
    auto s = neutral();
    Rig rig (variant, fs, s);
    REQUIRE (rig.engine.getLatencySamples() == 3528);

    // White-ish 24-bit grid signal: bit exact after the delay.
    std::vector<double> x (40000, 0.0);
    craft::mastering_limiter::dsp::Random rng (7);
    for (auto& v : x)
        v = std::round (rng.uniformCentred() * 0.9 * 8388608.0) / 8388608.0;
    x[5000] = 8388607.0 / 8388608.0;
    const auto y = rig.run (x);
    for (size_t i = 3528; i < x.size(); ++i)
        if (y[i] != x[i - 3528])
        {
            INFO ("first difference at " << i << ": " << y[i] << " vs " << x[i - 3528]);
            FAIL();
        }
}

TEST_CASE ("LL variants report 64 samples", "[latency][ll]")
{
    const auto variant = GENERATE (Variant::LowLatencyMulti, Variant::LowLatencyProfile);
    const double fs = GENERATE (44100.0, 48000.0, 96000.0);
    Rig rig (variant, fs, neutral());
    CHECK (rig.engine.getLatencySamples() == 64);
    // Above about 10 kHz the common allpass delay is below 2 samples, so a 15 kHz burst arrives at the reported delay.
    std::vector<double> x (20000, 0.0);
    x[1000] = 0.5;
    const auto y = rig.run (x);
    double energyBefore = 0.0;
    for (int i = 0; i < 1000 + 64 - 2; ++i)
        energyBefore += y[size_t (i)] * y[size_t (i)];
    CHECK (energyBefore < 1.0e-9); // causal w.r.t. the reported delay
}

TEST_CASE ("Crossover mapping and clamps", "[params]")
{
    CHECK (param::crossoverFromNorm (0, 0.0) == 40.0);
    CHECK (param::crossoverFromNorm (0, 1.0) == 350.0);
    CHECK (param::crossoverFromNorm (1, 0.0) == 150.0);
    CHECK (param::crossoverFromNorm (2, 1.0) == 4757.0);
    CHECK (param::crossoverFromNorm (3, 1.0) == 16000.0);
    CHECK (param::crossoverRawFromNorm (1.0) == Catch::Approx (16.0 * 1334.8));
    CHECK (param::releaseFromNorm (1.0) == Catch::Approx (2500.0));
    CHECK (param::releaseToNorm (2500.0) == Catch::Approx (1.0));
    CHECK (param::multiplierFromNorm (0.5) == Catch::Approx (1.0));
    CHECK (param::levelFromNorm (0.6) == Catch::Approx (-12.0));
    CHECK (param::bandDbFromNorm (0.75) == Catch::Approx (6.0));
}

TEST_CASE ("Out-of-order crossovers stay stable and sum flat", "[crossover]")
{
    const double fs = 48000.0;
    const auto variant = GENERATE (Variant::LinearMulti, Variant::LowLatencyMulti);
    auto s = neutral();
    s.crossoverHz = { 350.0, 150.0, 4757.0, 4000.0 };
    Rig rig (variant, fs, s);
    const int n = 48000, len = 12000;
    const auto y = rig.run (tones ({ { 700.0, -20.0 } }, fs, n));
    CHECK (std::abs (linToDb (toneAmplitude (y, 700.0, fs, n - len, len)) + 20.0) < 0.05);
}

TEST_CASE ("Solo outputs only the soloed bands", "[solo]")
{
    const auto variant = GENERATE (Variant::LinearMulti, Variant::LowLatencyMulti);
    auto s = neutral();
    s.solo = { false, false, false, false, true }; // band 5 only
    Rig rig (variant, 48000.0, s);
    const int n = 48000, len = 12000;
    const auto y = rig.run (tones ({ { 100.0, -20.0 }, { 12000.0, -20.0 } }, 48000.0, n));
    CHECK (toneAmplitude (y, 100.0, 48000.0, n - len, len) < dbToLin (-70.0));
    CHECK (linToDb (toneAmplitude (y, 12000.0, 48000.0, n - len, len)) > -21.0);
}
