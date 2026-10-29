// Spec r1 section 5 / table T1: the static law, exact on steady 1 kHz tones (+-0.25 dB), at 44.1 / 48 / 96 kHz.
#include <catch2/catch_test_macros.hpp>

#include "TestHelpers.h"

using namespace craft::levelrider::test;

namespace
{
const double t1Levels[12] = { -45, -41, -37, -33, -29, -25, -21, -17, -13, -9, -5, -1 };

struct T1Row
{
    const char* name;
    double minDb, maxDb;
    bool fast;
    double gain[12];
};

const T1Row t1[] = {
    { "+-12", -12, 12, false, { 12.00, 12.00, 12.00, 9.47, 5.84, 2.22, -1.27, -4.55, -7.53, -10.13, -12.00, -12.00 } },
    { "default -6/+6", -6, 6, false, { 6.00, 6.00, 6.00, 6.00, 6.00, 3.03, -0.10, -2.89, -5.27, -6.00, -6.00, -6.00 } },
    { "-3/+3", -3, 3, false, { 3.00, 3.00, 3.00, 3.00, 3.00, 3.00, 0.77, -1.72, -3.00, -3.00, -3.00, -3.00 } },
    { "Min -2, Max +10", -2, 10, false, { 10.00, 10.00, 10.00, 10.00, 6.97, 3.88, 1.10, -1.27, -2.00, -2.00, -2.00, -2.00 } },
    { "+-12 Fast", -12, 12, true, { 12.00, 12.00, 12.00, 9.40, 5.78, 2.16, -1.33, -4.61, -7.58, -10.18, -12.00, -12.00 } },
};
} // namespace

TEST_CASE ("T1: the section 5 formula reproduces the static law table", "[T1][static]")
{
    for (const auto& row : t1)
        for (int i = 0; i < 12; ++i)
        {
            INFO (row.name << " at " << t1Levels[i] << " dBFS");
            const double g = StaticLaw::gainDb (t1Levels[i], -21.0, row.minDb, row.maxDb);
            CHECK (std::abs (g - row.gain[i]) <= 0.25);
        }
}

TEST_CASE ("T1: steady 1 kHz sine steps through the engine (8 s steps) at 44.1 / 48 / 96 kHz", "[T1][static][engine]")
{
    for (double fs : { 44100.0, 48000.0, 96000.0 })
        for (const auto& row : t1)
        {
            auto e = makeEngine (fs, row.minDb, row.maxDb, row.fast);
            std::vector<Segment> segs;
            for (double l : t1Levels)
                segs.push_back ({ l, 8.0 });
            const auto tr = runSegments (e, segs, Stimulus::Sine, 1000.0);
            for (int i = 0; i < 12; ++i)
            {
                INFO (row.name << " at " << t1Levels[i] << " dBFS, fs " << fs);
                const double g = tr.mean (tr.segmentEnd[size_t (i)] - 0.1, tr.segmentEnd[size_t (i)]);
                CHECK (std::abs (g - row.gain[i]) <= 0.25);
            }
        }
}

TEST_CASE ("T1: a steady sine settles to the same gain at every sample rate", "[T1][static][rates]")
{
    // section 11: results identical in dB at 44.1, 48 and 96 kHz
    for (double level : { -33.0, -21.0, -13.0 })
    {
        double g[3];
        int k = 0;
        for (double fs : { 44100.0, 48000.0, 96000.0 })
        {
            auto e = makeEngine (fs, -12, 12);
            const auto tr = runSegments (e, { { level, 6.0 } }, Stimulus::Sine, 1000.0);
            g[k++] = tr.mean (5.9, 6.0);
        }
        INFO ("level " << level);
        CHECK (std::abs (g[0] - g[1]) <= 0.05);
        CHECK (std::abs (g[2] - g[1]) <= 0.05);
    }
}
