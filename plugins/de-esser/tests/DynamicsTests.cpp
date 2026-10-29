// Spec r1 section 5 and test vectors 9.2 - 9.4: static curve, noise vs sine, envelopes, sample-rate independence.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

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

void checkVector (const std::vector<double>& got, const std::vector<double>& want, double tol, int firstChecked = 0)
{
    for (size_t i = size_t (firstChecked); i < want.size(); ++i)
    {
        CAPTURE (offsets[i], got[i], want[i]);
        CHECK (got[i] == Catch::Approx (want[i]).margin (tol));
    }
}
} // namespace

TEST_CASE ("Static curve formula (section 5.2)", "[dynamics]")
{
    CHECK (GainComputer::reductionDb (-30.0, -30.0) == Catch::Approx (0.08 * 6.0 / 7.0));
    CHECK (GainComputer::reductionDb (-30.09, -30.0) == 0.0);
    CHECK (GainComputer::reductionDb (-20.0, -30.0) == Catch::Approx (10.08 * 6.0 / 7.0));
    CHECK (GainComputer::reductionDb (0.0, -80.0) == Catch::Approx (25.575).margin (0.001));
    CHECK (std::pow (10.0, -GainComputer::reductionDb (0.0, -80.0) / 20.0) == Catch::Approx (1.0 / 19.0));
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

TEST_CASE ("9.3 high-passed noise vs sine of equal RMS", "[dynamics][9.3]")
{
    const double fs = 48000.0;
    const size_t n = size_t (fs * 3.0);
    const auto noise = highPassedNoise (n, 4000.0, fs);
    const double levels[] = { -42, -36, -30, -24, -18, -12, -6 };
    const double want[] = { -0.11, -3.01, -7.99, -13.18, -18.42, -23.44, -25.55 };
    for (int i = 0; i < 7; ++i)
    {
        CAPTURE (levels[i]);
        auto e = makeEngine (fs, -36.0, 2000.0);
        Stereo in (n);
        const double scale = dbToGain (levels[i]) / std::sqrt (2.0); // RMS of a sine with this peak
        for (size_t j = 0; j < n; ++j)
            in.l[j] = in.r[j] = noise[j] * scale;
        const auto out = runStereo (e, in);
        const size_t from = size_t (fs * 0.5);
        CHECK (gainToDb (rms (out.l, from, n) / rms (in.l, from, n)) == Catch::Approx (want[i]).margin (0.3));
    }
}

TEST_CASE ("9.4 envelope vectors at 48 kHz", "[dynamics][9.4]")
{
    const double fs = 48000.0;
    SECTION ("A attack")
    checkVector (envelope (fs, -40, { { -60, 0.1 }, { -10, 0.2 } }, 0.1, offsets),
                 { -1.71, -3.48, -6.69, -14.99, -23.16, -25.39, -25.49, -25.46, -25.42, -25.41 }, 1.0);
    SECTION ("B release after sustained reduction")
    checkVector (envelope (fs, -40, { { -10, 1.0 }, { -60, 0.2 } }, 1.0, offsets),
                 { -25.40, -25.41, -25.42, -25.25, -23.55, -19.41, -12.29, -7.23, -2.33, -0.55 }, 1.0);
    SECTION ("C attack small")
    checkVector (envelope (fs, -40, { { -60, 0.1 }, { -30, 0.2 } }, 0.1, offsets),
                 { -0.96, -1.95, -3.54, -6.39, -7.81, -8.14, -8.27, -8.28, -8.28, -8.28 }, 1.0);
    SECTION ("C' release small")
    checkVector (envelope (fs, -40, { { -30, 1.0 }, { -60, 0.2 } }, 1.0, offsets),
                 { -8.26, -8.23, -8.15, -7.92, -7.54, -6.84, -5.23, -3.49, -1.20, -0.23 }, 1.0);
    SECTION ("D release after a 5 ms burst (offsets >= 192)")
    checkVector (envelope (fs, -40, { { -60, 0.1 }, { -10, 0.005 }, { -60, 0.2 } }, 0.105, offsets),
                 { -28.6, -34.8, -34.5, -24.0, -23.9, -24.0, -10.35, -5.83, -1.91, -0.49 }, 4.0, 5);
    SECTION ("E attack, threshold -25")
    checkVector (envelope (fs, -25, { { -60, 0.1 }, { -10, 0.2 } }, 0.1, offsets),
                 { -1.29, -2.62, -4.85, -9.35, -11.97, -12.50, -12.64, -12.64, -12.64, -12.63 }, 1.0);
}

TEST_CASE ("Program-dependent release: a short burst recovers faster than sustained reduction", "[dynamics]")
{
    const double fs = 48000.0;
    const auto burst = envelope (fs, -40, { { -60, 0.1 }, { -10, 0.005 }, { -60, 0.2 } }, 0.105, offsets);
    const auto held = envelope (fs, -40, { { -10, 1.0 }, { -60, 0.2 } }, 1.0, offsets);
    for (size_t i = 6; i < offsets.size(); ++i) // from +480 on
    {
        CAPTURE (offsets[i]);
        CHECK (burst[i] > held[i]);
    }
}

TEST_CASE ("Time constants are in seconds: vectors hold at 44.1 and 96 kHz", "[dynamics][fs]")
{
    for (double fs : { 44100.0, 96000.0 })
    {
        CAPTURE (fs);
        checkVector (envelope (fs, -40, { { -60, 0.1 }, { -10, 0.2 } }, 0.1, offsets),
                     { -1.71, -3.48, -6.69, -14.99, -23.16, -25.39, -25.49, -25.46, -25.42, -25.41 }, 1.0, 3);
        checkVector (envelope (fs, -40, { { -10, 1.0 }, { -60, 0.2 } }, 1.0, offsets),
                     { -25.40, -25.41, -25.42, -25.25, -23.55, -19.41, -12.29, -7.23, -2.33, -0.55 }, 1.0, 3);
        checkVector (envelope (fs, -40, { { -30, 1.0 }, { -60, 0.2 } }, 1.0, offsets),
                     { -8.26, -8.23, -8.15, -7.92, -7.54, -6.84, -5.23, -3.49, -1.20, -0.23 }, 1.0, 3);
        CHECK (steadyGainDb (-36.0, -24.0, true, fs) == Catch::Approx (-9.98).margin (0.3));
        CHECK (steadyGainDb (-36.0, -12.0, true, fs) == Catch::Approx (-20.26).margin (0.3));
    }
}
