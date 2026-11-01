// Spec r2 section 6.1 static tone tables (fundamental gain and third harmonic), with the section 6.1 tolerances,
// plus sample-rate independence (section 9: +-0.2 dB on section 6.1 at 44.1 / 96 kHz).
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "SpecTables.h"
#include "TestHelpers.h"

using namespace ddtest;
using Catch::Approx;

namespace
{
ToneResult measureCell (const ddspec::ToneTable& t, int fi, int li, double fs)
{
    return measureTone (distortionOnly (t.drive, t.attack, t.release), fs, ddspec::toneFreqs[size_t (fi)],
                        ddspec::toneLevels[size_t (li)], 0.8, 0.25);
}

} // namespace

TEST_CASE ("Static tone tables match section 6.1", "[tones]")
{
    const int ti = GENERATE (0, 1, 2, 3, 4);
    const auto& t = ddspec::toneTables[size_t (ti)];
    CAPTURE (t.name);
    for (int fi = 0; fi < 4; ++fi)
        for (int li = 0; li < 6; ++li)
        {
            const auto r = measureCell (t, fi, li, 48000.0);
            const double gainTable = t.cells[fi][li][0], h3Table = t.cells[fi][li][1];
            CAPTURE (ddspec::toneFreqs[size_t (fi)], ddspec::toneLevels[size_t (li)], r.gainDb, gainTable, r.h3Dbc, h3Table);
            CHECK (r.gainDb == Approx (gainTable).margin (ddspec::fundamentalTolerance (t, fi, li)));
            CHECK (ddspec::h3Ok (t, li, h3Table, r.h3Dbc));
        }
}

namespace
{
void checkSampleRateIndependence (double fs)
{
    const int ti = GENERATE (0, 1, 2, 3, 4);
    const auto& t = ddspec::toneTables[size_t (ti)];
    CAPTURE (t.name, fs);
    for (int fi = 0; fi < 4; ++fi)
        for (int li = 0; li < 6; ++li)
        {
            CAPTURE (ddspec::toneFreqs[size_t (fi)], ddspec::toneLevels[size_t (li)]);
            CHECK (measureCell (t, fi, li, fs).gainDb == Approx (measureCell (t, fi, li, 48000.0).gainDb).margin (0.2));
        }
}
} // namespace

TEST_CASE ("Section 6.1 fundamentals at 44.1 kHz equal 48 kHz within 0.2 dB", "[tones][samplerate]")
{
    checkSampleRateIndependence (44100.0);
}

TEST_CASE ("Section 6.1 fundamentals at 96 kHz equal 48 kHz within 0.2 dB", "[tones][samplerate]")
{
    checkSampleRateIndependence (96000.0);
}

TEST_CASE ("Clipper: low-frequency ceiling and fully clipped harmonics (section 4.4)", "[clipper]")
{
    const double fs = 48000.0;
    // plain memoryless limit at c = 0.304 +- 0.01
    CHECK (DynamicDistortion::clipCeiling == Approx (0.304).margin (0.01));
    AdaaClipper clip;
    clip.setCeiling (DynamicDistortion::clipCeiling);
    double y = 0.0;
    for (int i = 0; i < 16; ++i)
        y = clip.process (10.0);
    CHECK (y == Approx (DynamicDistortion::clipCeiling).margin (1e-12));
    // a fully clipped tone at Distortion Level 100 %: -10.1 +- 0.3 dBFS RMS
    {
        const double a = sinePeak (0.0), f0 = 500.0;
        const size_t st = size_t (0.3 * fs), m = wholePeriods (f0, fs, 0.25);
        auto o = render (distortionOnly (100.0), fs, st + m, [&] (size_t i) { return a * std::sin (2.0 * pi * f0 * double (i) / fs); });
        CHECK (db (rms (o, st, m)) == Approx (-10.1).margin (0.3));
    }

    // fully clipped tone: odd harmonics close to a square wave (h3 ~ -9.5 dBc, h5 ~ -14 dBc)
    const double amp = sinePeak (0.0), f = 1000.0;
    const size_t settle = size_t (0.3 * fs), n = wholePeriods (f, fs, 0.25);
    auto out = render (distortionOnly (100.0), fs, settle + n, [&] (size_t i) { return amp * std::sin (2.0 * pi * f * double (i) / fs); });
    const double a1 = toneAmplitude (out, settle, n, f, fs);
    CHECK (db (toneAmplitude (out, settle, n, 3.0 * f, fs) / a1) == Approx (-9.5).margin (1.0));
    CHECK (db (toneAmplitude (out, settle, n, 5.0 * f, fs) / a1) == Approx (-14.0).margin (1.0));
    // symmetric: no even harmonics
    CHECK (db (toneAmplitude (out, settle, n, 2.0 * f, fs) / a1) < -80.0);
}
