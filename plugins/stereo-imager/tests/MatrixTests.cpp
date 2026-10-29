// Spec r1 section 9: steady-state LR matrices, measured with impulses through the engine.
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "TestHelpers.h"

using namespace testutil;
using Catch::Matchers::WithinAbs;

namespace
{
struct MatrixVector
{
    const char* name;
    Settings s;
    Matrix2 expected;
};


std::vector<MatrixVector> vectors()
{
    using P = Settings;
    auto mk = [] (auto f) { P s; f (s); return s; };
    namespace pm = craft::dsp::param;
    return {
        { "neutral", P {}, { 1, 0, 0, 1 } },
        { "Width 0", mk ([] (P& s) { s.width = pm::Width::fromNorm (0.0); }), { 0.5, 0.5, 0.5, 0.5 } },
        { "Width 0.48 (r=0.3)", mk ([] (P& s) { s.width = pm::Width::fromNorm (0.3); }), { 0.74, 0.26, 0.26, 0.74 } },
        { "Width 1.32 (r=0.6)", mk ([] (P& s) { s.width = pm::Width::fromNorm (0.6); }), { 1.0633, -0.1467, -0.1467, 1.0633 } },
        { "Width 2.08 (r=0.8)", mk ([] (P& s) { s.width = pm::Width::fromNorm (0.8); }), { 1.2992, -0.4555, -0.4555, 1.2992 } },
        { "Width 3.00", mk ([] (P& s) { s.width = pm::Width::fromNorm (1.0); }), { 1.6330, -0.8165, -0.8165, 1.6330 } },
        { "Rotation +45", mk ([] (P& s) { s.rotationDeg = 45; }), { 0.7071, -0.7071, 0.7071, 0.7071 } },
        { "Rotation -18", mk ([] (P& s) { s.rotationDeg = -18; }), { 0.9511, 0.3090, -0.3090, 0.9511 } },
        { "Asymmetry +36", mk ([] (P& s) { s.asymmetryDeg = 36; }), { 0.6106, 0.3894, -0.1984, 1.1984 } },
        { "Asymmetry -90", mk ([] (P& s) { s.asymmetryDeg = -90; }), { 1, 0, 1, 0 } },
        { "Asymmetry +90", mk ([] (P& s) { s.asymmetryDeg = 90; }), { 0, 1, 0, 1 } },
        { "Gain -6.0", mk ([] (P& s) { s.gainDb = pm::Gain::fromNorm (0.75); }), { 0.5012, 0, 0, 0.5012 } },
        { "Swap", mk ([] (P& s) { s.swap = true; }), { 0, 1, 1, 0 } },
        { "Polarity 1", mk ([] (P& s) { s.polarity1 = true; }), { -1, 0, 0, 1 } },
        { "MS input", mk ([] (P& s) { s.msInput = true; }), { 1, 1, 1, -1 } },
        { "MS input + Swap", mk ([] (P& s) { s.msInput = true; s.swap = true; }), { 1, 1, -1, 1 } },
        { "MS input + Width 2.08", mk ([] (P& s) { s.msInput = true; s.width = 2.08; }), { 0.8436, 1.7547, 0.8436, -1.7547 } },
        { "Width 2.08 + Rotation +22.5", mk ([] (P& s) { s.width = 2.08; s.rotationDeg = 22.5; }), { 1.3746, -0.9180, 0.0763, 1.0259 } },
        { "Rotation +22.5 + Asymmetry +45", mk ([] (P& s) { s.rotationDeg = 22.5; s.asymmetryDeg = 45; }), { 0.5412, 0, 0, 1.3066 } },
        { "Width 0.59 + Asymmetry +45", mk ([] (P& s) { s.width = 0.59; s.asymmetryDeg = 45; }), { 0.5, 0.5, 0.0828, 0.9172 } },
    };
}
} // namespace

TEST_CASE ("Section 9 LR matrices (48 kHz, Full and Imager variants)", "[matrix][spec9]")
{
    for (auto variant : { Variant::Full, Variant::Imager })
    {
        for (const auto& v : vectors())
        {
            INFO (v.name);
            auto e = makeEngine (variant, v.s, 48000.0);
            const auto a = measureMatrix (e);
            // table values are given to 4 decimals
            const double tol = 0.6e-4;
            CHECK_THAT (a.a00, WithinAbs (v.expected.a00, tol));
            CHECK_THAT (a.a01, WithinAbs (v.expected.a01, tol));
            CHECK_THAT (a.a10, WithinAbs (v.expected.a10, tol));
            CHECK_THAT (a.a11, WithinAbs (v.expected.a11, tol));

            // also against the analytic matrix used by the vector display
            const auto lr = broadbandLR (v.s, variant);
            CHECK_THAT (a.a00, WithinAbs (lr.a00, 1e-12));
            CHECK_THAT (a.a11, WithinAbs (lr.a11, 1e-12));
        }
    }
}

TEST_CASE ("Width continuity and endpoint gain", "[matrix]")
{
    double gM, gS;
    widthGains (1.0, gM, gS);
    CHECK (gM == 1.0);
    CHECK (gS == 1.0);
    widthGains (1.0 + 1e-9, gM, gS);
    CHECK_THAT (gM, WithinAbs (1.0, 1e-8));
    widthGains (3.0, gM, gS);
    CHECK_THAT (gM, WithinAbs (std::sqrt (2.0 / 3.0), 1e-15));
}

TEST_CASE ("MS matrix variant", "[matrix][variant]")
{
    Settings s;
    s.width = 2.5; // ignored by this variant
    s.gainDb = -10;
    auto e = makeEngine (Variant::MsMatrix, s, 48000.0);
    const auto a = measureMatrix (e);
    CHECK (a.a00 == 0.5);
    CHECK (a.a01 == 0.5);
    CHECK (a.a10 == 0.5);
    CHECK (a.a11 == -0.5);
}

TEST_CASE ("Balance behaviour of asymmetry + rotation: no crosstalk", "[matrix]")
{
    Settings s;
    s.rotationDeg = 22.5;
    s.asymmetryDeg = 45;
    const auto a = broadbandLR (s, Variant::Full);
    CHECK_THAT (a.a01, WithinAbs (0.0, 1e-15));
    CHECK_THAT (a.a10, WithinAbs (0.0, 1e-15));
}
