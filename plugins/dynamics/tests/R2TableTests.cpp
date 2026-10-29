// Spec r2 normative tables: 10.5 timing, 10.5.1 compressor trajectories, 10.5.3 expander trajectory,
// 10.8 detector type, 10.9 PeakRef near full scale. Every table is checked at 44.1, 48 and 96 kHz.
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <cstdio>
#include <cstdlib>
#include <string>

#include "TestHelpers.h"

using namespace testutil;
using Catch::Approx;

namespace
{
const double rates[] = { 44100.0, 48000.0, 96000.0 };

// 10.5.1 stimulus: 1 kHz sine, -40 dBFS, with -10 dBFS sections; returns the signal and the edge times (s).
struct Stimulus
{
    std::vector<double> x;
    double edgeA, edgeR, edgeR30, edgeR3;
};

Stimulus trajectoryStimulus (double fs)
{
    // 0.5 s lead-in, then: -40 1 s, -10 1 s, -40 2 s, -40 1 s, -10 30 ms, -40 2 s, -40 1 s, -10 3 ms, -40 2 s
    Stimulus s;
    s.edgeA = 1.5;
    s.edgeR = 2.5;
    s.edgeR30 = 5.5 + 0.030;
    s.edgeR3 = 5.53 + 2.0 + 1.0 + 0.003;
    const double end = s.edgeR3 + 2.0;
    const int n = int (std::ceil (end * fs));
    auto loud = [&] (double t) {
        return (t >= s.edgeA && t < s.edgeR) || (t >= 5.5 && t < s.edgeR30) || (t >= s.edgeR3 - 0.003 && t < s.edgeR3);
    };
    const double lo = dbToGain (-40.0), hi = dbToGain (-10.0);
    s.x = sine (n, fs, 1000.0, [&] (int i) { return loud (double (i) / fs) ? hi : lo; });
    return s;
}

// In-phase gain (dB) of the 1 ms block ending k ms after `edge`.
double blockGainDb (const std::vector<double>& in, const std::vector<double>& out, double fs, double edge, double k)
{
    const long b = long (std::llround ((edge + (k - 1.0) * 0.001) * fs)), e = long (std::llround ((edge + k * 0.001) * fs));
    double xy = 0.0, xx = 0.0;
    for (long i = b; i < e; ++i)
        xy += in[size_t (i)] * out[size_t (i)], xx += in[size_t (i)] * in[size_t (i)];
    return 20.0 * std::log10 (std::abs (xy / xx));
}

const double offsets[12] = { 1, 2, 3, 5, 10, 20, 50, 100, 200, 500, 1000, 1900 };

struct TrajRow
{
    double att, rel, pdr;
    char edge; // 'A', 'R', '3' (R30), '0' (R3)
    double tol;
    double g[12]; // NAN = not listed
};

constexpr double NA = 1.0e9;

const TrajRow trajectories[] = {
    { 0.1, 50, 0, 'A', 3.5, { -14.88, -19.34, -19.54, -19.59, -19.59, -19.59, -19.59, -19.59, -19.59, -19.59, NA, NA } },
    { 0.1, 50, 0, 'R', 1.5, { -18.87, -17.63, -16.56, -14.81, -11.72, -8.11, -3.54, -1.16, -0.27, -0.27, -0.27, -0.27 } },
    { 0.1, 50, 0, '3', 1.5, { -18.87, -17.63, -16.56, -14.81, -11.72, -8.11, -3.54, -1.16, -0.27, -0.27, -0.27, -0.27 } },
    { 0.1, 50, 0, '0', 1.5, { -18.85, -17.61, -16.55, -14.80, -11.71, -8.11, -3.54, -1.16, -0.27, -0.27, -0.27, -0.27 } },
    { 1, 50, 0, 'A', 1.5, { -3.19, -9.10, -12.65, -15.96, -17.90, -18.29, -18.32, -18.32, -18.32, -18.32, NA, NA } },
    { 1, 50, 0, 'R', 1.5, { -17.85, -16.75, -15.79, -14.18, -11.30, -7.85, -3.41, -1.08, -0.14, -0.14, -0.14, -0.14 } },
    { 1, 50, 0, '3', 1.5, { -17.85, -16.75, -15.79, -14.18, -11.30, -7.85, -3.41, -1.08, -0.14, -0.14, -0.14, -0.14 } },
    { 1, 50, 0, '0', 1.5, { -13.76, -13.12, -12.53, -11.49, -9.46, -6.79, -3.05, -0.98, -0.14, -0.14, -0.14, -0.14 } },
    { 10, 50, 0, 'A', 1.5, { -0.18, -0.88, -1.63, -3.13, -6.55, -11.58, -15.72, -16.13, -16.16, -16.16, NA, NA } },
    { 10, 50, 0, 'R', 1.5, { -16.12, -15.82, -15.23, -13.80, -11.00, -7.63, -3.28, -1.01, -0.07, +0.04, +0.04, +0.04 } },
    { 10, 50, 0, '3', 1.5, { -14.30, -14.09, -13.63, -12.47, -10.11, -7.13, -3.11, -0.96, -0.07, +0.04, +0.04, +0.04 } },
    { 10, 50, 0, '0', 2.0, { -2.34, -2.74, -2.78, -2.67, -2.37, -1.87, -0.93, -0.29, +0.01, +0.04, +0.04, +0.04 } },
    { 100, 50, 0, 'A', 2.0, { +0.05, +0.01, -0.04, -0.17, -0.53, -1.27, -3.43, -6.66, -11.48, -15.68, NA, NA } },
    { 100, 50, 0, 'R', 4.0, { -15.89, -15.85, -15.76, -15.40, -13.59, -9.26, -3.57, -0.99, -0.05, +0.06, +0.06, +0.06 } },
    { 100, 50, 0, '3', 2.5, { -2.08, -2.14, -2.21, -2.31, -2.44, -2.13, -1.02, -0.29, +0.02, +0.06, +0.06, +0.06 } },
    { 100, 50, 0, '0', 1.5, { -0.10, -0.16, -0.21, -0.30, -0.40, -0.38, -0.17, -0.02, +0.05, +0.06, +0.06, +0.06 } },
    { 1, 10, 0, 'A', 1.5, { -3.16, -9.07, -12.63, -15.81, -17.36, -17.51, -17.51, -17.51, -17.51, -17.51, NA, NA } },
    { 1, 10, 0, 'R', 1.5, { -15.59, -12.18, -9.94, -7.06, -3.54, -1.11, -0.10, -0.10, -0.10, -0.10, -0.10, -0.10 } },
    { 1, 10, 0, '3', 1.5, { -15.59, -12.18, -9.94, -7.06, -3.54, -1.11, -0.10, -0.10, -0.10, -0.10, -0.10, -0.10 } },
    { 1, 10, 0, '0', 1.5, { -12.76, -10.35, -8.61, -6.26, -3.21, -1.02, -0.10, -0.10, -0.10, -0.10, -0.10, -0.10 } },
    { 1, 200, 0, 'A', 1.5, { -3.21, -9.10, -12.66, -15.99, -18.03, -18.55, -18.62, -18.62, -18.62, -18.62, NA, NA } },
    { 1, 200, 0, 'R', 1.5, { -18.50, -18.18, -17.88, -17.30, -16.04, -14.02, -10.14, -6.67, -3.40, -0.63, -0.16, -0.16 } },
    { 1, 200, 0, '3', 1.5, { -18.48, -18.17, -17.86, -17.29, -16.03, -14.01, -10.14, -6.67, -3.40, -0.63, -0.16, -0.16 } },
    { 1, 200, 0, '0', 1.5, { -13.97, -13.80, -13.63, -13.31, -12.56, -11.28, -8.53, -5.79, -3.02, -0.57, -0.16, -0.16 } },
    { 1, 1000, 0, 'A', 1.5, { -3.21, -9.11, -12.66, -16.00, -18.06, -18.62, -18.75, -18.76, -18.76, -18.76, NA, NA } },
    { 1, 1000, 0, 'R', 1.5, { -18.73, -18.66, -18.60, -18.47, -18.15, -17.56, -16.02, -14.00, -11.18, -6.66, -3.39, -1.21 } },
    { 1, 1000, 0, '3', 1.5, { -18.70, -18.63, -18.57, -18.44, -18.13, -17.53, -15.99, -13.99, -11.17, -6.65, -3.39, -1.20 } },
    { 1, 1000, 0, '0', 1.5, { -14.08, -14.04, -14.01, -13.94, -13.76, -13.43, -12.53, -11.25, -9.29, -5.77, -3.01, -1.08 } },
    { 1, 1000, 5, 'A', 1.5, { -3.21, -9.11, -12.65, -15.89, -17.82, -18.49, -18.75, -18.76, -18.76, -18.76, NA, NA } },
    { 1, 1000, 5, 'R', 1.5, { -18.73, -18.66, -18.60, -18.47, -18.15, -17.56, -16.02, -14.00, -11.18, -6.66, -3.39, -1.21 } },
    { 1, 1000, 5, '3', 1.5, { -18.63, -18.55, -18.48, -18.35, -18.04, -17.45, -15.93, -13.94, -11.14, -6.64, -3.38, -1.20 } },
    { 1, 1000, 5, '0', 2.0, { -13.32, -11.93, -11.06, -10.06, -9.18, -8.85, -8.40, -7.73, -6.62, -4.35, -2.36, -0.86 } },
    { 1, 1000, 22, 'A', 1.5, { -3.21, -9.11, -12.65, -15.84, -17.49, -17.90, -18.39, -18.69, -18.76, -18.76, NA, NA } },
    { 1, 1000, 22, 'R', 1.5, { -18.73, -18.66, -18.60, -18.47, -18.15, -17.56, -16.02, -14.00, -11.18, -6.66, -3.39, -1.21 } },
    { 1, 1000, 22, '3', 3.0, { -17.48, -16.15, -15.16, -13.82, -12.15, -11.09, -10.37, -9.44, -7.95, -5.09, -2.70, -0.98 } },
    { 1, 1000, 22, '0', 4.5, { -12.98, -10.97, -9.62, -7.91, -5.92, -4.63, -3.99, -3.75, -3.31, -2.31, -1.32, -0.49 } },
    { 1, 1000, 102, 'A', 1.5, { -3.21, -9.11, -12.65, -15.82, -17.36, -17.57, -17.75, -18.02, -18.37, -18.72, NA, NA } },
    { 1, 1000, 102, 'R', 1.5, { -18.73, -18.66, -18.60, -18.47, -18.15, -17.56, -16.01, -14.00, -11.18, -6.66, -3.39, -1.21 } },
    { 1, 1000, 102, '3', 6.0, { -16.11, -13.35, -11.55, -9.31, -6.66, -4.77, -3.52, -3.22, -2.85, -2.01, -1.15, -0.43 } },
    { 1, 1000, 102, '0', 5.0, { -12.84, -10.65, -9.16, -7.24, -4.90, -3.17, -1.89, -1.51, -1.33, -0.96, -0.56, -0.21 } },
};

// White noise in the audio band at the given RMS: above 48 kHz the noise is low-passed at 20 kHz (8th order),
// so that every rate sees the same signal (the reference's "white noise" is an audio-band signal).
std::vector<double> audioBandNoise (int n, double fs, double rmsLinear)
{
    auto v = noise (n, 11u, 1.0);
    if (fs > 50000.0)
    {
        BiquadCoeffs c[4];
        const double qs[4] = { 0.5098, 0.6013, 0.9000, 2.5629 }; // 8th-order Butterworth sections
        for (int k = 0; k < 4; ++k)
        {
            const double w0 = 2.0 * pi * 20000.0 / fs, alpha = std::sin (w0) / (2.0 * qs[k]), cw = std::cos (w0), a0 = 1.0 + alpha;
            c[k] = { (1.0 - cw) / 2.0 / a0, (1.0 - cw) / a0, (1.0 - cw) / 2.0 / a0, -2.0 * cw / a0, (1.0 - alpha) / a0 };
        }
        Biquad st[4];
        for (auto& x : v)
            for (int k = 0; k < 4; ++k)
                x = st[k].process (c[k], x);
    }
    const double r = rms (v, 0, n);
    for (auto& x : v)
        x *= rmsLinear / r;
    return v;
}

double edgeOf (const Stimulus& s, char e)
{
    return e == 'A' ? s.edgeA : e == 'R' ? s.edgeR : e == '3' ? s.edgeR30 : s.edgeR3;
}
} // namespace

TEST_CASE ("r2 10.5.1 compressor gain trajectories (all cells, 44.1/48/96 kHz)", "[r2][envelope]")
{
    int cells = 0, failures = 0;
    for (double fs : rates)
    {
        const auto stim = trajectoryStimulus (fs);
        double lastAtt = -1, lastRel = -1, lastPdr = -1;
        std::vector<double> out;
        double sumSq = 0.0, worst = 0.0;
        int n = 0;
        for (const auto& row : trajectories)
        {
            if (row.att != lastAtt || row.rel != lastRel || row.pdr != lastPdr)
            {
                Settings s;
                s.compThresholdDb = -30.0;
                s.ratioPos = 0.8;
                s.compAttackMs = row.att;
                s.compReleaseMs = row.rel;
                s.pdrMs = row.pdr;
                auto e = makeEngine (s, fs);
                out = runStereo (e, stim.x, stim.x, 512).l;
                lastAtt = row.att, lastRel = row.rel, lastPdr = row.pdr;
            }
            const double edge = edgeOf (stim, row.edge);
            for (int k = 0; k < 12; ++k)
            {
                if (row.g[k] == NA)
                    continue;
                const double got = blockGainDb (stim.x, out, fs, edge, offsets[k]);
                const double err = got - row.g[k];
                sumSq += err * err, worst = std::max (worst, std::abs (err)), ++n;
                if (std::getenv ("DYN_VERBOSE") != nullptr && std::abs (err) > 1.0)
                    std::printf ("  cell fs %.0f att %g rel %g pdr %g %c +%g: %.2f vs %.2f (tol %.1f)\n", fs, row.att, row.rel, row.pdr, row.edge, offsets[k], got, row.g[k], row.tol);
                ++cells;
                if (std::abs (err) > row.tol)
                {
                    ++failures;
                    FAIL_CHECK ("fs " << fs << " att " << row.att << " rel " << row.rel << " pdr " << row.pdr << " edge " << row.edge
                                      << " +" << offsets[k] << " ms: " << got << " vs " << row.g[k] << " (tol " << row.tol << ")");
                }
            }
        }
        std::printf ("10.5.1 @ %.0f Hz: %d cells, RMS error %.2f dB, max %.2f dB\n", fs, n, std::sqrt (sumSq / n), worst);
    }
    CHECK (cells == 3 * 460);
    CHECK (failures == 0);
}

TEST_CASE ("r2 10.5 timing table: t63 / t90 within 2 ms + 25 %", "[r2][envelope]")
{
    struct Row
    {
        double att, rel, pdr;
        char edge;
        double t63, t90;
    };
    // (i) cells are informative and omitted
    const Row rows[] = {
        { 1, 50, 0, 'A', 3.5, 6.5 },     { 1, 50, 0, 'R', 25.5, 74.5 },   { 10, 50, 0, 'A', 16.5, 32.5 },
        { 10, 50, 0, 'R', 28.5, 80.5 },  { 100, 50, 0, 'A', 164.5, 312.5 }, { 1, 10, 0, 'A', 2.5, 5.5 },
        { 1, 10, 0, 'R', 6.5, 16.5 },    { 1, 200, 0, 'A', 3.5, 6.5 },     { 1, 200, 0, 'R', 95.5, 290.5 },
        { 1, 1000, 0, 'A', 3.5, 6.5 },   { 1, 1000, 0, 'R', 417.5, 1144.5 },
    };
    for (double fs : rates)
    {
        const auto stim = trajectoryStimulus (fs);
        for (const auto& r : rows)
        {
            Settings s;
            s.compThresholdDb = -30.0;
            s.ratioPos = 0.8;
            s.compAttackMs = r.att;
            s.compReleaseMs = r.rel;
            auto e = makeEngine (s, fs);
            const auto out = runStereo (e, stim.x, stim.x, 512).l;
            const double edge = r.edge == 'A' ? stim.edgeA : stim.edgeR;
            std::vector<double> g;
            const int span = r.edge == 'A' ? 990 : 1990; // the loud section after edge A lasts 1 s
            for (int k = 1; k <= span; ++k)
                g.push_back (blockGainDb (stim.x, out, fs, edge, k));
            const double before = blockGainDb (stim.x, out, fs, edge, -20.0), after = g.back();
            auto cross = [&] (double f) {
                const double level = before + f * (after - before);
                for (size_t k = 0; k < g.size(); ++k)
                    if ((after < before && g[k] <= level) || (after > before && g[k] >= level))
                        return double (k) + 0.5;
                return 1.0e9;
            };
            const double t63 = cross (0.63), t90 = cross (0.9);
            INFO ("fs " << fs << " att " << r.att << " rel " << r.rel << " edge " << r.edge << ": t63 " << t63 << " t90 " << t90);
            CHECK (std::abs (t63 - r.t63) <= 2.0 + 0.25 * r.t63);
            CHECK (std::abs (t90 - r.t90) <= 2.0 + 0.25 * r.t90);
        }
    }
}

TEST_CASE ("r2 10.5.3 expander gain trajectory (linear gain, +-0.15)", "[r2][expander]")
{
    const double off[11] = { 1, 2, 3, 5, 10, 20, 50, 100, 200, 500, 990 };
    struct Row
    {
        char edge;
        double g[11];
    };
    const Row rows[] = {
        { 'A', { 0.538, 0.816, 0.924, 0.986, 0.999, 0.999, 0.999, 0.999, 0.999, 0.999, 0.999 } },
        { 'R', { 0.995, 0.987, 0.979, 0.963, 0.926, 0.855, 0.681, 0.486, 0.296, 0.277, 0.277 } },
        { '3', { 0.995, 0.987, 0.979, 0.963, 0.926, 0.855, 0.681, 0.486, 0.296, 0.277, 0.277 } },
        { '0', { 0.947, 0.940, 0.932, 0.918, 0.882, 0.815, 0.652, 0.468, 0.289, 0.277, 0.277 } },
    };
    for (double fs : rates)
    {
        const auto stim = trajectoryStimulus (fs);
        Settings s;
        s.compBypass = true;
        s.expander = true;
        s.gateOpenDb = -30.0;
        s.gateAttackMs = 1.0;
        s.gateReleaseMs = 100.0;
        auto e = makeEngine (s, fs);
        const auto out = runStereo (e, stim.x, stim.x, 512).l;
        double worst = 0.0;
        for (const auto& r : rows)
            for (int k = 0; k < 11; ++k)
            {
                const double got = dbToGain (blockGainDb (stim.x, out, fs, edgeOf (stim, r.edge), off[k]));
                worst = std::max (worst, std::abs (got - r.g[k]));
                INFO ("fs " << fs << " edge " << r.edge << " +" << off[k]);
                CHECK (got == Approx (r.g[k]).margin (0.15));
            }
        std::printf ("10.5.3 @ %.0f Hz: max error %.3f\n", fs, worst);
    }
}

TEST_CASE ("r2 10.8 detector type (rectified average): noise / 100 Hz / 10 kHz vs 1 kHz", "[r2][detector]")
{
    struct Row
    {
        double r;
        double noise, low, high;
    };
    const Row rows[] = { { 0.8, 0.86, -0.92, 1.19 }, { 0.5, 0.66, -0.72, 0.92 } };
    for (double fs : rates)
        for (const auto& row : rows)
        {
            Settings s;
            s.compThresholdDb = -40.0;
            s.ratioPos = row.r;
            const int n = int (0.5 * fs);
            double dn = 0.0, dl = 0.0, dh = 0.0;
            int count = 0;
            for (double peak = -30.0; peak <= -4.0 + 1e-9; peak += 2.0)
            {
                auto gainOf = [&] (std::vector<double> x) {
                    auto e = makeEngine (s, fs);
                    const auto in = x;
                    auto out = runStereo (e, std::move (x), in, 512);
                    return ratioDb (out.l, in, n / 2, n);
                };
                const double ref = gainOf (sine (n, fs, 1000.0, peak));
                dn += gainOf (audioBandNoise (n, fs, dbToGain (peak - 3.0103))) - ref;
                dl += gainOf (sine (n, fs, 100.0, peak)) - ref;
                dh += gainOf (sine (n, fs, 10000.0, peak)) - ref;
                ++count;
            }
            dn /= count, dl /= count, dh /= count;
            INFO ("fs " << fs << " r " << row.r << ": noise " << dn << " 100 Hz " << dl << " 10 kHz " << dh);
            CHECK (dn == Approx (row.noise).margin (0.4));
            CHECK (dl == Approx (row.low).margin (0.4));
            CHECK (dh == Approx (row.high).margin (0.4));
        }
}

TEST_CASE ("r2 10.9 PeakRef near full scale (output limit on the final output, +-0.3 dB)", "[r2][peakref]")
{
    struct Row
    {
        double ratio, T;
        double g[7];
    };
    const Row rows[] = {
        { 0.50, -10, { -11.42, -10.71, -9.64, -8.20, -6.45, -4.48, -2.40 } },
        { 0.50, -20, { -12.40, -10.32, -8.26, -6.28, -4.47, -2.84, -1.42 } },
        { 0.50, -60, { -0.20, -0.15, -0.11, -0.08, -0.05, -0.03, -0.01 } },
        { 0.50, -80, { -0.02, -0.01, -0.01, -0.01, -0.01, -0.00, -0.00 } },
        { 1.45, -10, { +4.32, +3.89, +3.38, +2.79, +2.15, +1.48, +0.53 } },
        { 1.45, -20, { +3.92, +3.24, +2.57, +1.95, +1.38, +0.88, +0.32 } },
        { 1.45, -60, { +0.07, +0.05, +0.04, +0.03, +0.02, +0.01, +0.00 } },
        { 1.45, -80, { +0.01, +0.01, +0.00, +0.00, +0.00, +0.00, +0.00 } },
        { 4.58, -10, { +8.39, +7.70, +6.85, +5.84, +4.46, +2.79, +1.00 } },
        { 4.58, -20, { +11.73, +10.22, +8.49, +6.67, +4.81, +2.93, +1.05 } },
        { 4.58, -60, { +1.43, +1.09, +0.81, +0.58, +0.39, +0.24, +0.10 } },
        { 4.58, -80, { +0.16, +0.12, +0.09, +0.06, +0.04, +0.03, +0.01 } },
        { 50.0, -10, { +8.79, +8.08, +7.20, +6.13, +4.59, +2.86, +1.05 } },
        { 50.0, -20, { +12.66, +10.86, +9.00, +7.08, +5.12, +3.15, +1.17 } },
        { 50.0, -60, { -5.55, -7.55, -9.56, -11.56, -13.56, -15.56, -17.56 } },
        { 50.0, -80, { -25.57, -27.57, -29.57, -31.57, -33.57, -35.57, -37.57 } },
        { -5.0, -10, { +3.77, +2.78, +1.51, -0.11, -2.15, -4.74, -8.15 } },
        { -5.0, -20, { -1.91, -5.32, -10.26, -19.81, -22.86, -13.27, -9.48 } },
        { -5.0, -60, { -0.22, -0.18, -0.15, -0.12, -0.10, -0.08, -0.07 } },
        { -5.0, -80, { -0.02, -0.02, -0.01, -0.01, -0.01, -0.01, -0.01 } },
        { 4.58, -30, { +10.85, +9.16, +7.49, +5.88, +4.26, +2.57, +0.85 } },
        { 4.58, -50, { +3.64, +2.85, +2.17, +1.58, +1.09, +0.68, +0.26 } },
        { 4.58, -70, { +0.49, +0.37, +0.27, +0.19, +0.13, +0.08, +0.03 } },
        { 4.58, -100, { +0.02, +0.01, +0.01, +0.01, +0.00, +0.00, +0.00 } },
        { 0.65, -40, { -1.36, -1.01, -0.74, -0.52, -0.34, -0.21, -0.10 } },
        { 1.00, -40, { -0.00, -0.00, -0.00, -0.00, -0.00, -0.00, -0.00 } },
        { 13.69, -40, { +12.88, +10.90, +8.92, +6.95, +4.99, +3.04, +1.10 } },
    };
    const double levels[7] = { -12, -10, -8, -6, -4, -2, 0 };
    for (double fs : rates)
        for (const auto& row : rows)
        {
            Settings s;
            s.ratioPos = row.ratio == 1.0 ? ratio::neutralPos : ratio::toPosition (row.ratio);
            s.compThresholdDb = row.T;
            s.peakRef = true;
            for (int i = 0; i < 7; ++i)
            {
                INFO ("fs " << fs << " ratio " << row.ratio << " T " << row.T << " level " << levels[i]);
                CHECK (steadyGainDb (s, fs, levels[i], 1000.0, 1.2) == Approx (row.g[i]).margin (0.3));
            }
        }
}
