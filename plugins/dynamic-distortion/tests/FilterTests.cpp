// Spec r1 sections 4.5 (tone EQ) and 4.6 (post-clipper HP / LP), measured through the complete processor.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "SpecTables.h"
#include "TestHelpers.h"

using namespace ddtest;
using Catch::Approx;

namespace
{
// Distortion-path settings of the section 4.5 measurement: Drive 0 %, LP 21 kHz, Direct 0 %.
Settings eqSettings (double bass, double mid, double midFreq, double treble)
{
    Settings s = distortionOnly (0.0);
    s.bass = bass;
    s.mid = mid;
    s.midFreq = midFreq;
    s.treble = treble;
    return s;
}
} // namespace

TEST_CASE ("Tone EQ responses match the section 4.5 table", "[eq]")
{
    const double fs = GENERATE (44100.0, 48000.0, 96000.0);
    CAPTURE (fs);
    const auto neutral = impulseResponse (eqSettings (0, 0, 1252, 0), fs);
    for (const auto& row : ddspec::eqRows)
    {
        const auto ir = impulseResponse (eqSettings (row.bass, row.mid, row.midFreq, row.treble), fs);
        for (size_t i = 0; i < ddspec::responseFreqs.size(); ++i)
        {
            const double f = ddspec::responseFreqs[i];
            if (f >= 0.5 * fs)
                continue;
            const double got = responseDb (ir, f, fs) - responseDb (neutral, f, fs);
            CAPTURE (row.name, f);
            CHECK (got == Approx (row.db[i]).margin (f > 15000.0 ? 1.0 : 0.5));
        }
    }
}

TEST_CASE ("Tone EQ: boost is the exact mirror of the cut plus the broadband level drop", "[eq]")
{
    const double fs = 48000.0;
    for (double v : { 5.0, 17.5, 25.0, 40.0, 50.0 })
    {
        for (int band = 0; band < 3; ++band)
        {
            const double dropDb = (band == 1 ? -9.0 : -6.0) * v / 50.0;
            auto settings = [&] (double value) {
                return eqSettings (band == 0 ? value : 0.0, band == 1 ? value : 0.0, 1252.0, band == 2 ? value : 0.0);
            };
            const auto cut = impulseResponse (settings (-v), fs, 0.5);
            const auto boost = impulseResponse (settings (v), fs, 0.5);
            const auto neutral = impulseResponse (settings (0.0), fs, 0.5);
            for (double f : { 30.0, 100.0, 300.0, 1252.0, 3000.0, 8000.0, 16000.0 })
            {
                const double c = responseDb (cut, f, fs) - responseDb (neutral, f, fs);
                const double b = responseDb (boost, f, fs) - responseDb (neutral, f, fs);
                CAPTURE (band, v, f);
                CHECK (b + c == Approx (dropDb).margin (0.01));
            }
        }
    }
    // the cut depth is linear in the value: 18 dB at -50 (centre of the bells, DC of the shelf)
    for (double v : { 10.0, 30.0, 50.0 })
    {
        CHECK (gainToDb (std::abs (eq::bass (-v).at (1.0e-6))) == Approx (-18.0 * v / 50.0).margin (1e-6));
        CHECK (gainToDb (std::abs (eq::treble (-v).at (1.0))) == Approx (-18.0 * v / 50.0).margin (1e-6));
        CHECK (gainToDb (std::abs (eq::mid (-v, 1.2).at (1.0))) == Approx (-18.0 * v / 50.0).margin (1e-6));
        // boost peak: +12 dB (Bass / Treble), +9 dB (Mid) at +50
        CHECK (gainToDb (std::abs (eq::bass (v).at (1.0e-6))) == Approx (12.0 * v / 50.0).margin (1e-6));
        CHECK (gainToDb (std::abs (eq::treble (v).at (1.0))) == Approx (12.0 * v / 50.0).margin (1e-6));
        CHECK (gainToDb (std::abs (eq::mid (v, 1.2).at (1.0))) == Approx (9.0 * v / 50.0).margin (1e-6));
    }
}

TEST_CASE ("Mid band: centre follows Mid Freq between the tabulated anchors", "[eq]")
{
    const double fs = 48000.0;
    const auto neutral = impulseResponse (eqSettings (0, 0, 1252, 0), fs);
    for (double fc : { 150.0, 300.0, 600.0, 1252.0, 2500.0, 5000.0 })
    {
        const auto ir = impulseResponse (eqSettings (0, -50, fc, 0), fs);
        // the deepest point of the cut lies within 5 % of the set frequency and is ~18 dB deep
        double best = 1e9, bestF = 0;
        for (double f = fc * 0.8; f <= fc * 1.25; f *= 1.005)
        {
            const double d = responseDb (ir, f, fs) - responseDb (neutral, f, fs);
            if (d < best)
            {
                best = d;
                bestF = f;
            }
        }
        CAPTURE (fc);
        CHECK (bestF == Approx (fc).epsilon (0.05));
        CHECK (best == Approx (-18.0).margin (0.3));
    }
}

TEST_CASE ("LP responses match section 4.6", "[lp]")
{
    const double fs = GENERATE (44100.0, 48000.0, 96000.0);
    CAPTURE (fs);
    // the LP's own response: ratio to the LP at 21 kHz times the analog 21 kHz response
    const auto ref = impulseResponse (distortionOnly (0.0), fs);
    for (const auto& row : ddspec::lpRows)
    {
        auto s = distortionOnly (0.0);
        s.lowPassHz = row.hz;
        const auto ir = impulseResponse (s, fs);
        for (size_t i = 0; i < ddspec::responseFreqs.size(); ++i)
        {
            const double f = ddspec::responseFreqs[i];
            if (f > 0.45 * fs)
                continue; // section 4.6: sample-rate independent up to 0.45 fs
            const double got = responseDb (ir, f, fs) - responseDb (ref, f, fs) + gainToDb (post::lpMagnitude (f, 21000.0));
            const double literal = gainToDb (post::lpMagnitude (f, row.hz)); // two first-order sections
            CAPTURE (row.hz, f);
            // implementation = normative text (two identical first-order sections, -6.02 dB at the set frequency)
            CHECK (got == Approx (literal).margin (f > 15000.0 ? 0.35 : 0.2));
            if ((row.hz == 500.0 || row.hz == 2000.0) && f == 20000.0)
            {
                // Documented deviation (IMPLEMENTATION_NOTES.md): the table's 20 kHz cells of LP 500 / 2000
                // (-61.34 / -38.82 dB) lie 2.75 / 1.27 dB above the response of the normative structure
                // (-64.09 / -40.09 dB); the implementation follows the normative text.
                CHECK (std::abs (got - row.db[i]) < 3.0);
                continue;
            }
            CHECK (got == Approx (row.db[i]).margin (f > 15000.0 ? 1.0 : 0.5));
        }
        // -6.02 dB at the set frequency
        const double atFc = responseDb (ir, row.hz, fs) - responseDb (ref, row.hz, fs) + gainToDb (post::lpMagnitude (row.hz, 21000.0));
        CHECK (atFc == Approx (-6.02).margin (0.1));
    }
}

TEST_CASE ("HP response matches section 4.6", "[hp]")
{
    const double fs = GENERATE (44100.0, 48000.0, 96000.0);
    CAPTURE (fs);
    const auto ir = impulseResponse (distortionOnly (0.0), fs, 2.0);
    const double ref = responseDb (ir, 1000.0, fs);
    for (size_t i = 0; i < ddspec::hpFreqs.size(); ++i)
    {
        CAPTURE (ddspec::hpFreqs[i]);
        CHECK (responseDb (ir, ddspec::hpFreqs[i], fs) - ref == Approx (ddspec::hpDb[i]).margin (0.5));
    }
    // the HP is on the distortion path only: the direct path is flat
    Settings s = neutralSettings();
    s.distPercent = 0.0;
    const auto dir = impulseResponse (s, fs, 0.5);
    for (double f : { 20.0, 50.0, 1000.0, 15000.0 })
        CHECK (responseDb (dir, f, fs) == Approx (0.0).margin (1e-6));
}

TEST_CASE ("Clip emphasis pair is transparent for small signals", "[clipper]")
{
    // pre * post = 1 for all frequencies
    const auto pre = emphasis::preFilter(), postF = emphasis::postFilter();
    for (double f : { 1.0, 30.0, 134.48, 1000.0, 20000.0 })
        CHECK (std::abs (pre.at (f / emphasis::poleHz) * postF.at (f / emphasis::poleHz)) == Approx (1.0).margin (1e-12));
}
