// Requantiser stage in the complete engine: spec r1 section 10.
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "TestHelpers.h"

using namespace helpers;

namespace
{
struct Total
{
    Dither d;
    Shaping s;
    double expected;
};
} // namespace

TEST_CASE ("16-bit total error matches the measured values", "[requantiser]")
{
    const double fs = GENERATE (44100.0, 48000.0, 96000.0);
    const Total totals[] = {
        { Dither::Type1, Shaping::None, 0.50 },     { Dither::Type1, Shaping::Moderate, 1.01 },
        { Dither::Type1, Shaping::Normal, 1.98 },   { Dither::Type1, Shaping::Ultra, 3.93 },
        { Dither::Type2, Shaping::None, 0.41 },     { Dither::Type2, Shaping::Moderate, 0.65 },
        { Dither::Type2, Shaping::Normal, 1.18 },   { Dither::Type2, Shaping::Ultra, 2.28 },
        { Dither::None, Shaping::None, 0.29 },      { Dither::None, Shaping::Moderate, 0.58 },
        { Dither::None, Shaping::Normal, 1.15 },    { Dither::None, Shaping::Ultra, 2.26 },
    };
    for (const auto& t : totals)
    {
        // The shaped totals are specified at 48 kHz; the NTF is designed per rate, its total at 96 kHz is lower by design.
        if (fs > 50000.0 && t.s != Shaping::None)
            continue;
        Settings s;
        s.wordLength = 16;
        s.dither = t.d;
        s.shaping = t.s;
        Rig rig (Variant::LowLatencyMulti, fs, s);
        // -60 dBFS moving signal; the allpass only moves its phase, so compare against the same engine at 32-bit-like
        // resolution: the reference is the 24-bit-free path, obtained with a parallel engine at maximum word length.
        Settings hi = neutral();
        hi.wordLength = 24;
        Rig ref (Variant::LowLatencyMulti, fs, hi);
        const int n = 1 << 17;
        const auto x = tones ({ { 997.0, -60.0 } }, fs, n);
        const auto y = rig.run (x);
        const auto r = ref.run (x);
        double sum = 0.0;
        int count = 0;
        for (int i = 8192; i < n; ++i, ++count)
        {
            const double e = (y[size_t (i)] - r[size_t (i)]) * 32768.0;
            sum += e * e;
        }
        const double rms = std::sqrt (sum / double (count));
        INFO ("fs " << fs << " dither " << int (t.d) << " shaping " << int (t.s) << ": " << rms << " expected " << t.expected);
        // Spec: +-0.05 LSB without shaping, +-10 % with shaping (the 24-bit reference adds a negligible error).
        CHECK (std::abs (rms - t.expected) <= (t.s == Shaping::None ? 0.05 : 0.10 * t.expected));
    }
}

TEST_CASE ("Digital silence: Type 1 dithers, Type 2 and None output exact zero", "[requantiser]")
{
    const auto variant = GENERATE (Variant::LinearMulti, Variant::LowLatencyMulti);
    auto run = [variant] (Dither d) {
        Settings s;
        s.wordLength = 16;
        s.dither = d;
        s.shaping = Shaping::Normal;
        Rig rig (variant, 48000.0, s);
        // Signal first: filter residue must also end up as exact digital silence.
        auto x = tones ({ { 440.0, -6.0 } }, 48000.0, 24000);
        x.resize (240000, 0.0);
        return rig.run (x);
    };
    const auto t1 = run (Dither::Type1);
    const auto t2 = run (Dither::Type2);
    const auto none = run (Dither::None);
    CHECK (peak (t1, 200000, 40000) > 0.0);
    CHECK (peak (t2, 200000, 40000) == 0.0);
    CHECK (peak (none, 200000, 40000) == 0.0);
}

TEST_CASE ("Word lengths quantise to their grid", "[requantiser]")
{
    const int bits = GENERATE (24, 22, 20, 18, 16);
    auto s = neutral();
    s.wordLength = bits;
    Rig rig (Variant::LowLatencyMulti, 48000.0, s);
    const auto y = rig.run (tones ({ { 997.0, -10.0 } }, 48000.0, 8000));
    const double scale = std::ldexp (1.0, bits - 1);
    for (double v : y)
        CHECK (v * scale == std::round (v * scale));
}
