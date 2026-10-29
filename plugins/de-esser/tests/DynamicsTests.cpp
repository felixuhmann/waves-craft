// Spec r3 section 5 and test vectors 9.2 - 9.4, 9.6: static curve, noise vs sine, envelopes, release after bursts.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <limits>

#include "TestHelpers.h"

using namespace craft::deesser::test;

namespace
{
constexpr double tone = 10000.0;

// Steady gain (dB) of a 10 kHz sine at `peakDb` in L (and R if `both`): output RMS / input RMS.
double steadyGainDb (double thresholdDb, double peakDb, bool both, double fs = 48000.0)
{
    auto e = makeEngine (fs, thresholdDb, 2000.0);
    const size_t n = size_t (fs * 0.6);
    Stereo in (n);
    in.l = steppedSine (tone, fs, { { peakDb, n } });
    if (both)
        in.r = in.l;
    const auto out = runStereo (e, in);
    return gainToDb (rms (out.l, n / 2, n) / rms (in.l, n / 2, n));
}

// Gain (dB) at the given offsets (in 48 kHz samples, scaled to fs) after the level change.
std::vector<double> envelope (double fs, double thresholdDb, const std::vector<std::pair<double, double>>& segmentsSeconds,
                              double changeAtSeconds, const std::vector<int>& offsets48k)
{
    std::vector<std::pair<double, size_t>> seg;
    for (const auto& s : segmentsSeconds)
        seg.push_back ({ s.first, size_t (std::llround (s.second * fs)) });
    auto e = makeEngine (fs, thresholdDb, 2000.0);
    Stereo in;
    in.l = steppedSine (tone, fs, seg);
    in.r = in.l;
    const auto g = recordGain (e, in);
    const size_t change = size_t (std::llround (changeAtSeconds * fs));
    std::vector<double> out;
    for (int o : offsets48k)
        out.push_back (gainToDb (g[change + size_t (std::llround (o * fs / 48000.0))]));
    return out;
}

const std::vector<int> offsets { 5, 10, 20, 48, 96, 192, 480, 960, 2400, 4800 };
} // namespace

TEST_CASE ("Static curve formula (r3 section 5 step 2)", "[dynamics]")
{
    CHECK (GainComputer::reductionDb (-30.0, -30.0) == Catch::Approx (0.0793 * 0.85648));
    CHECK (GainComputer::reductionDb (-30.08, -30.0) == 0.0);
    CHECK (GainComputer::reductionDb (-20.0, -30.0) == Catch::Approx (10.0793 * 0.85648));
    CHECK (GainComputer::reductionDb (0.0, -80.0) == 25.570);
}

TEST_CASE ("9.2 static curve, 10 kHz sine in L and R", "[dynamics][9.2]")
{
    const double levels[] = { -48, -42, -36, -30, -24, -18, -12, -6, 0 };
    struct Row
    {
        double thr;
        double db[9];
    };
    const Row rows[] = {
        { -12, { 0, 0, 0, 0, 0, 0, 0.00, -4.98, -10.12 } },
        { -24, { 0, 0, 0, 0, 0.00, -4.93, -10.08, -15.22, -20.36 } },
        { -36, { 0, 0, 0.00, -4.85, -9.98, -15.12, -20.26, -25.41, -25.57 } },
        { -60, { -9.98, -15.12, -20.26, -25.40, -25.57, -25.57, -25.57, -25.57, -25.57 } },
    };
    for (const auto& row : rows)
        for (int i = 0; i < 9; ++i)
        {
            CAPTURE (row.thr, levels[i]);
            CHECK (steadyGainDb (row.thr, levels[i], true) == Catch::Approx (row.db[i]).margin (0.3));
        }
}

TEST_CASE ("9.2 static curve, sine in L only (linked sidechain)", "[dynamics][9.2]")
{
    const double levels[] = { -24, -18, -12, -6, 0 };
    const double want[] = { -4.79, -9.95, -15.10, -20.24, -25.39 };
    for (int i = 0; i < 5; ++i)
    {
        CAPTURE (levels[i]);
        CHECK (steadyGainDb (-36.0, levels[i], false) == Catch::Approx (want[i]).margin (0.3));
    }
}

double noiseGainDb (double peakDb)
{
    const double fs = 48000.0;
    const size_t n = size_t (fs * 3.0);
    static const auto noise = highPassedNoise (n, 4000.0, fs);
    auto e = makeEngine (fs, -36.0, 2000.0);
    Stereo in (n);
    const double scale = dbToGain (peakDb) / std::sqrt (2.0); // RMS of a sine with this peak
    for (size_t j = 0; j < n; ++j)
        in.l[j] = in.r[j] = noise[j] * scale;
    const auto out = runStereo (e, in);
    const size_t from = size_t (fs * 0.5);
    return gainToDb (rms (out.l, from, n) / rms (in.l, from, n));
}

const double noiseLevels[] = { -42, -36, -30, -24, -18, -12, -6 };
const double noiseWant[] = { -0.11, -3.01, -7.99, -13.18, -18.42, -23.44, -25.55 };

TEST_CASE ("9.3 high-passed noise vs sine of equal RMS", "[dynamics][9.3]")
{
    for (int i = 0; i < 7; ++i)
    {
        CAPTURE (noiseLevels[i]);
        CHECK (noiseGainDb (noiseLevels[i]) == Catch::Approx (noiseWant[i]).margin (0.3));
    }
}
namespace
{
const double sampleRates[] = { 44100.0, 48000.0, 96000.0 };

struct Vector94
{
    const char* name;
    double thr;
    std::vector<std::pair<double, double>> segments; // (dBFS, seconds)
    double change;                                   // seconds
    std::vector<double> want;
};

const std::vector<Vector94>& vectors94()
{
    static const std::vector<Vector94> v {
        { "A attack", -40, { { -60, 0.1 }, { -10, 0.2 } }, 0.1,
          { -1.34, -3.02, -6.61, -14.47, -22.69, -25.13, -25.38, -25.40, -25.40, -25.40 } },
        { "B release after 1 s", -40, { { -60, 0.1 }, { -10, 1.0 }, { -60, 0.2 } }, 1.1,
          { -25.39, -25.36, -25.26, -24.81, -21.88, -17.31, -10.94, -6.45, -1.99, -0.38 } },
        { "C attack small", -40, { { -60, 0.1 }, { -30, 0.2 } }, 0.1,
          { -0.73, -1.71, -3.51, -6.27, -7.76, -8.12, -8.27, -8.28, -8.28, -8.28 } },
        { "C' release small", -40, { { -60, 0.1 }, { -30, 1.0 }, { -60, 0.2 } }, 1.1,
          { -8.27, -8.24, -8.13, -7.91, -7.52, -6.82, -5.21, -3.48, -1.20, -0.24 } },
        { "D release after 5 ms burst", -40, { { -60, 0.1 }, { -10, 0.005 }, { -60, 0.2 } }, 0.105,
          { -25.23, -25.20, -25.04, -23.72, -17.31, -10.67, -6.76, -4.37, -1.46, -0.29 } },
        { "E attack, threshold -25", -25, { { -60, 0.1 }, { -10, 0.2 } }, 0.1,
          { -1.00, -2.29, -4.80, -9.13, -11.87, -12.46, -12.63, -12.63, -12.63, -12.63 } },
    };
    return v;
}

// 9.6 rows: a burst pattern (level, burst length, number of bursts, start-to-start period) followed by -60 dBFS.
struct Row96
{
    double level, burstSeconds;
    int count;
    double periodSeconds;
    std::vector<double> want; // NaN = not specified
};

constexpr double none = std::numeric_limits<double>::quiet_NaN();
const std::vector<int> offsets96 { 48, 96, 192, 480, 960, 2400, 4800, 9600 };

const std::vector<Row96>& rows96()
{
    static const std::vector<Row96> r {
        { -10, 0.001, 1, 0, { -17.35, -13.98, -9.24, -3.87, -1.93, -0.71, -0.14, -0.01 } },
        { -10, 0.002, 1, 0, { -21.96, -16.51, -10.35, -4.18, -2.69, -0.96, -0.19, -0.01 } },
        { -10, 0.005, 1, 0, { -23.72, -17.31, -10.67, -6.76, -4.37, -1.46, -0.29, -0.01 } },
        { -10, 0.010, 1, 0, { -23.83, -17.36, -13.92, -9.42, -5.75, -1.83, -0.35, -0.02 } },
        { -10, 0.020, 1, 0, { -23.85, -21.16, -16.91, -10.77, -6.37, -1.98, -0.38, -0.02 } },
        { -10, 0.050, 1, 0, { -24.76, -21.84, -17.29, -10.93, -6.44, -1.99, -0.38, -0.02 } },
        { -10, 0.100, 1, 0, { -24.81, -21.87, -17.31, -10.93, -6.45, -1.99, -0.38, -0.02 } },
        { -10, 0.200, 1, 0, { -24.81, -21.88, -17.31, -10.94, -6.45, -1.99, -0.38, -0.02 } },
        { -10, 0.500, 1, 0, { -24.81, -21.88, -17.31, -10.94, -6.45, -1.99, -0.38, -0.02 } },
        { -10, 1.000, 1, 0, { -24.81, -21.88, -17.31, -10.94, -6.45, -1.99, -0.38, -0.02 } },
        { -20, 0.005, 1, 0, { -15.19, -11.95, -8.22, -5.50, -3.65, -1.25, -0.25, -0.01 } },
        { -20, 0.020, 1, 0, { -15.78, -14.50, -12.49, -8.69, -5.39, -1.74, -0.34, -0.01 } },
        { -20, 0.200, 1, 0, { -16.24, -14.89, -12.77, -8.84, -5.47, -1.75, -0.34, -0.01 } },
        { -30, 0.005, 1, 0, { -7.29, -6.27, -4.74, -3.36, -2.33, -0.84, -0.17, -0.01 } },
        { -30, 0.020, 1, 0, { -7.67, -7.30, -6.63, -5.08, -3.40, -1.18, -0.23, -0.01 } },
        { -30, 0.200, 1, 0, { -7.91, -7.52, -6.82, -5.21, -3.48, -1.20, -0.24, -0.01 } },
        { -10, 0.005, 2, 0.020, { -23.73, -17.32, -11.57, -8.19, -5.14, -1.67, -0.32, -0.01 } },
        { -10, 0.005, 2, 0.050, { -23.72, -17.31, -10.67, -7.28, -4.66, -1.54, -0.30, -0.01 } },
        { -10, 0.005, 2, 0.100, { -23.72, -17.31, -10.67, -6.86, -4.43, -1.48, -0.29, -0.01 } },
        { -10, 0.005, 2, 0.300, { -23.72, -17.31, -10.67, -6.76, -4.37, -1.46, -0.29, -0.01 } },
        { -10, 0.005, 10, 0.060, { -23.72, -17.31, -10.67, -7.22, -4.63, -1.53, none, none } },
    };
    return r;
}

// Gain (dB) at offsets96 after the end of the last burst of the row's pattern.
std::vector<double> runRow96 (double fs, const Row96& row)
{
    std::vector<std::pair<double, double>> seg { { -60, 0.1 } };
    for (int b = 0; b < row.count; ++b)
    {
        seg.push_back ({ row.level, row.burstSeconds });
        if (b + 1 < row.count)
            seg.push_back ({ -60, row.periodSeconds - row.burstSeconds });
    }
    double end = 0.0;
    for (const auto& s : seg)
        end += double (std::llround (s.second * fs)) / fs; // the stimulus rounds each segment to whole samples
    seg.push_back ({ -60, 0.25 });
    return envelope (fs, -40, seg, end, offsets96);
}

std::vector<double> runVector94 (double fs, const Vector94& v)
{
    return envelope (fs, v.thr, v.segments, v.change, offsets);
}
} // namespace

// r3 section 8: every 9.4 and 9.6 table point within +-1.0 dB, RMS over all of them <= 0.3 dB, at each rate.
TEST_CASE ("9.4 + 9.6 envelope and release tables at 44.1 / 48 / 96 kHz", "[dynamics][9.4][9.6][fs]")
{
    for (double fs : sampleRates)
    {
        double sq = 0.0, worst = 0.0;
        int n = 0;
        auto add = [&] (double got, double want) {
            const double err = got - want;
            CHECK (got == Catch::Approx (want).margin (1.0));
            sq += err * err;
            worst = std::max (worst, std::abs (err));
            ++n;
        };
        for (const auto& v : vectors94())
        {
            const auto got = runVector94 (fs, v);
            for (size_t i = 0; i < offsets.size(); ++i)
            {
                CAPTURE (fs, v.name, offsets[i]);
                add (got[i], v.want[i]);
            }
        }
        for (const auto& row : rows96())
        {
            const auto got = runRow96 (fs, row);
            for (size_t i = 0; i < offsets96.size(); ++i)
            {
                if (std::isnan (row.want[i]))
                    continue;
                CAPTURE (fs, row.level, row.burstSeconds, row.count, row.periodSeconds, offsets96[i]);
                add (got[i], row.want[i]);
            }
        }
        CAPTURE (fs, worst, std::sqrt (sq / n));
        CHECK (n == 60 + 166);
        CHECK (std::sqrt (sq / n) <= 0.3);
    }
}

TEST_CASE ("Program-dependent release: short bursts recover faster than sustained reduction", "[dynamics]")
{
    const auto& rows = rows96();
    const auto burst = runRow96 (48000.0, rows[2]);   // 5 ms
    const auto held = runRow96 (48000.0, rows[9]);    // 1000 ms
    for (size_t i = 0; i < offsets96.size() - 1; ++i)
    {
        CAPTURE (offsets96[i]);
        CHECK (burst[i] > held[i]);
    }
}

TEST_CASE ("Static gain is sample-rate independent", "[dynamics][fs]")
{
    for (double fs : { 44100.0, 96000.0 })
    {
        CAPTURE (fs);
        CHECK (steadyGainDb (-36.0, -24.0, true, fs) == Catch::Approx (-9.98).margin (0.3));
        CHECK (steadyGainDb (-36.0, -12.0, true, fs) == Catch::Approx (-20.26).margin (0.3));
    }
}
