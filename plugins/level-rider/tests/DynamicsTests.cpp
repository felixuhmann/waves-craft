// Spec r1 section 6 / tables T2 (band-noise equilibria, +-0.6 dB) and T3 (step trajectories, +-1.5 dB or
// reached within +-30 % of the stated time).
#include <map>

#include <catch2/catch_test_macros.hpp>

#include "TestHelpers.h"

using namespace craft::levelrider::test;

namespace
{
struct T2Row
{
    const char* name;
    double minDb, maxDb;
    bool fast;
    double targetDb;
    double gain[18];
};

const T2Row t2[] = {
    { "+-12", -12, 12, false, -21, { -3.9, -11.8, -3.8, -8.4, -3.9, +1.2, -4.0, +6.6, +6.4, +1.1, -12.0, -8.5, -11.9, -12.0, -4.6, +11.9, -4.1, -4.0 } },
    { "default +-6", -6, 6, false, -21, { -2.4, -6.0, -2.3, -6.0, -2.4, +2.1, -2.4, +6.0, +6.0, +2.0, -6.0, -6.0, -6.0, -6.0, -3.0, +6.0, -2.5, -2.4 } },
    { "+-3", -3, 3, false, -21, { -1.3, -3.0, -1.2, -3.0, -1.3, +2.8, -1.3, +3.0, +3.0, +2.7, -3.0, -3.0, -3.0, -3.0, -0.6, +3.0, -1.4, -1.3 } },
    { "+-12 Fast", -12, 12, true, -21, { -5.6, -12.0, -5.5, -10.0, -5.6, -0.6, -5.7, +4.7, +4.4, -0.7, -12.0, -10.2, -12.0, -12.0, -6.9, +10.0, -5.8, -5.8 } },
    { "+-6 Fast", -6, 6, true, -21, { -3.8, -6.0, -3.7, -6.0, -3.8, +0.5, -3.8, +5.3, +5.0, +0.4, -6.0, -6.0, -6.0, -6.0, -4.8, +6.0, -3.9, -3.9 } },
    { "+-12, Target -12", -12, 12, false, -12, { +3.9, -6.1, +4.0, -1.5, +3.9, +9.6, +3.8, +12.0, +12.0, +9.4, -6.3, -1.7, -6.3, -10.2, +2.1, +12.0, +3.7, +3.8 } },
    { "+-12, Target -33", -12, 12, false, -33, { -11.9, -12.0, -11.8, -12.0, -11.9, -8.3, -12.0, -4.0, -4.2, -8.4, -12.0, -12.0, -12.0, -12.0, -9.9, +0.7, -12.0, -11.9 } },
};

const double t3Times[13] = { 0.01, 0.03, 0.06, 0.1, 0.2, 0.3, 0.5, 1, 1.5, 2, 3, 4, 6 };
const int t3Step[10] = { 1, 2, 3, 4, 5, 6, 7, 9, 15, 16 }; // index of the T2 segment the step goes into

const double t3Slow[10][13] = {
    { -4.6, -5.9, -7.5, -8.6, -10.3, -11.1, -11.5, -11.8, -12.0, -12.0, -11.8, -11.9, -11.9 },
    { -11.9, -11.9, -11.9, -11.8, -11.8, -11.7, -11.6, -11.2, -10.5, -9.5, -6.2, -4.0, -4.0 },
    { -4.0, -4.7, -5.6, -6.1, -7.2, -7.6, -8.0, -8.3, -8.3, -8.3, -8.4, -8.2, -8.4 },
    { -8.3, -8.2, -8.2, -8.1, -7.8, -7.5, -6.9, -5.0, -4.1, -4.0, -3.9, -4.0, -3.8 },
    { -3.9, -3.9, -3.8, -3.7, -3.3, -3.0, -2.3, -0.1, +0.9, +1.1, +1.1, +1.1, +1.1 },
    { +1.0, +0.3, -0.5, -1.3, -2.3, -2.8, -3.6, -3.7, -3.9, -4.0, -3.9, -3.9, -4.0 },
    { -4.1, -4.0, -4.0, -4.0, -3.9, -3.8, -3.7, -3.0, -2.2, -0.8, +3.4, +6.4, +6.4 },
    { +6.0, +5.3, +4.6, +4.0, +2.8, +1.9, +1.7, +1.1, +1.1, +1.3, +1.1, +1.0, +1.1 },
    { -4.2, -4.1, -4.1, -4.1, -4.0, -3.8, -3.6, -2.7, -1.5, +0.2, +5.0, +10.4, +11.9 },
    { +10.9, +8.5, +5.5, +2.6, -0.9, -2.3, -3.3, -4.0, -3.9, -3.8, -3.8, -3.9, -3.9 } };

const double t3Fast[10][13] = {
    { -7.7, -10.1, -11.2, -11.8, -12.0, -12.0, -12.0, -12.0, -12.0, -12.0, -12.0, -12.0, -12.0 },
    { -12.0, -12.0, -12.0, -12.0, -12.0, -12.0, -12.0, -11.9, -11.7, -11.2, -9.4, -6.1, -5.8 },
    { -5.9, -7.1, -8.1, -8.7, -9.3, -9.4, -9.5, -9.8, -9.8, -9.9, -10.0, -9.6, -9.8 },
    { -9.7, -9.7, -9.6, -9.6, -9.4, -9.2, -8.8, -7.5, -6.1, -5.7, -5.8, -5.7, -5.6 },
    { -5.7, -5.7, -5.6, -5.5, -5.3, -5.1, -4.6, -3.0, -1.0, -0.8, -0.9, -0.7, -0.8 },
    { -1.5, -2.8, -3.9, -4.4, -4.9, -5.1, -5.6, -5.5, -5.6, -5.8, -5.7, -5.7, -5.7 },
    { -5.8, -5.8, -5.7, -5.7, -5.7, -5.7, -5.6, -5.4, -5.1, -4.5, -2.3, +1.5, +4.6 },
    { +3.4, +2.3, +1.3, +0.9, -0.0, -0.7, -0.6, -1.2, -1.1, -0.9, -0.8, -0.8, -0.8 },
    { -6.7, -6.7, -6.7, -6.7, -6.7, -6.7, -6.7, -6.6, -6.5, -6.3, -5.4, -3.3, +5.2 },
    { +6.0, +0.3, -2.7, -3.8, -4.9, -5.5, -5.9, -5.7, -5.7, -5.6, -5.6, -5.4, -5.7 } };

// One T2 run per configuration (12 s segments), shared by the T2 and T3 tests.
const GainTrace& t2Run (size_t row, double fs = 48000.0)
{
    static std::map<std::pair<size_t, int>, GainTrace> cache;
    const auto key = std::make_pair (row, int (fs));
    auto it = cache.find (key);
    if (it == cache.end())
    {
        const auto& r = t2[row];
        auto e = makeEngine (fs, r.minDb, r.maxDb, r.fast, r.targetDb);
        std::vector<Segment> segs;
        for (double l : t2Levels())
            segs.push_back ({ l, 12.0 });
        it = cache.emplace (key, runSegments (e, segs)).first;
    }
    return it->second;
}

// Section 11 T3 rule: the value is within +-1.5 dB, or the trajectory reaches it within +-30 % of the time.
bool t3Matches (const GainTrace& tr, double stepTime, double t, double ref)
{
    if (std::abs (tr.at (stepTime + t) - ref) <= 1.5)
        return true;
    double lo = 1e9, hi = -1e9;
    for (double tt = stepTime + 0.7 * t; tt <= stepTime + 1.3 * t + 1e-9; tt += tr.windowSeconds)
    {
        lo = std::min (lo, tr.at (tt));
        hi = std::max (hi, tr.at (tt));
    }
    return ref >= lo && ref <= hi;
}

void checkT2Row (size_t row)
{
    const auto& tr = t2Run (row);
    for (size_t i = 0; i < 18; ++i)
    {
        const double end = tr.segmentEnd[i];
        const double g = tr.mean (end - 0.3, end);
        INFO (t2[row].name << ", segment " << i + 1 << " (" << t2Levels()[i] << " dBFS): " << g << " vs " << t2[row].gain[i]);
        CHECK (std::abs (g - t2[row].gain[i]) <= 0.6);
    }
}

void checkT3 (bool fast)
{
    const auto& tr = t2Run (fast ? 3 : 0);
    const auto& table = fast ? t3Fast : t3Slow;
    for (size_t s = 0; s < 10; ++s)
    {
        const double stepTime = tr.segmentEnd[size_t (t3Step[s] - 1)];
        for (size_t j = 0; j < 13; ++j)
        {
            INFO ((fast ? "Fast " : "Slow ") << t2Levels()[size_t (t3Step[s] - 1)] << " -> " << t2Levels()[size_t (t3Step[s])]
                                             << " at " << t3Times[j] << " s: " << tr.at (stepTime + t3Times[j]) << " vs "
                                             << table[s][j]);
            CHECK (t3Matches (tr, stepTime, t3Times[j], table[s][j]));
        }
    }
}
} // namespace

TEST_CASE ("T2: band-noise equilibria, +-12 Slow", "[T2][dynamics]") { checkT2Row (0); }
TEST_CASE ("T2: band-noise equilibria, default +-6 Slow", "[T2][dynamics]") { checkT2Row (1); }
TEST_CASE ("T2: band-noise equilibria, +-3 Slow", "[T2][dynamics]") { checkT2Row (2); }
TEST_CASE ("T2: band-noise equilibria, +-12 Fast", "[T2][dynamics]") { checkT2Row (3); }
TEST_CASE ("T2: band-noise equilibria, +-6 Fast", "[T2][dynamics]") { checkT2Row (4); }
TEST_CASE ("T2: band-noise equilibria, +-12 Target -12", "[T2][dynamics]") { checkT2Row (5); }
TEST_CASE ("T2: band-noise equilibria, +-12 Target -33", "[T2][dynamics]") { checkT2Row (6); }

TEST_CASE ("T3: step trajectories, Slow", "[T3][dynamics]") { checkT3 (false); }
TEST_CASE ("T3: step trajectories, Fast", "[T3][dynamics]") { checkT3 (true); }

TEST_CASE ("T2/T3 timing is the same in seconds at 44.1 and 96 kHz", "[T2][T3][rates]")
{
    // section 11: identical in dB and seconds at 44.1, 48 and 96 kHz. The first five segments (steady state and
    // 6 / 12 dB steps) are compared with the 48 kHz run.
    for (double fs : { 44100.0, 96000.0 })
    {
        auto e = makeEngine (fs, -12, 12, false, -21);
        std::vector<Segment> segs;
        for (size_t i = 0; i < 5; ++i)
            segs.push_back ({ t2Levels()[i], 12.0 });
        const auto tr = runSegments (e, segs);
        for (size_t i = 0; i < 5; ++i)
        {
            const double end = tr.segmentEnd[i];
            INFO ("fs " << fs << ", segment " << i + 1);
            CHECK (std::abs (tr.mean (end - 0.3, end) - t2[0].gain[i]) <= 0.6);
        }
        for (size_t s = 0; s < 4; ++s)
            for (size_t j = 0; j < 13; ++j)
            {
                const double stepTime = tr.segmentEnd[size_t (t3Step[s] - 1)];
                INFO ("fs " << fs << ", step " << s << " at " << t3Times[j] << " s");
                CHECK (t3Matches (tr, stepTime, t3Times[j], t3Slow[s][j]));
            }
    }
}
