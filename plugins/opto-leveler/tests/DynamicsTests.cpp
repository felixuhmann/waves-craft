// Gain-cell dynamics (T3): the spec 4.3 cell model on its own (ideal level steps, as in the spec) and the complete
// engine fed with the T3 burst signals; sample-rate independence of the trajectories.
#include <catch2/catch_test_macros.hpp>

#include <cstdio>
#include <string>

#include "SpecMeasure.h"

using namespace optotest;

namespace
{
struct Row
{
    dsp::Mode mode;
    double pr, level, seconds;
};

Row rowOf (int r)
{
    const std::string id = spec::T3.textAt (r, 0);
    return { id[0] == 'L' ? dsp::Mode::Limit : dsp::Mode::Compress, std::atof (id.c_str() + 2), spec::T3.at (r, 1),
             spec::T3.at (r, 2) };
}

std::vector<double> times (const spec::Table& t)
{
    std::vector<double> v;
    for (int c = 3; c < t.numCols; ++c)
        v.push_back (t.headerValue[c]);
    return v;
}

struct Score
{
    double sq = 0.0, worst = 0.0;
    int n = 0;
    void add (double e)
    {
        sq += e * e;
        worst = std::max (worst, std::abs (e));
        ++n;
    }
    double rms() const { return std::sqrt (sq / std::max (1, n)); }
};

void score (Score& s, const spec::Table& t, int r, const std::vector<double>& got, const char* what)
{
    for (size_t k = 0; k < got.size(); ++k)
    {
        const double want = t.at (r, 3 + int (k));
        if (std::isnan (want))
            continue;
        const double e = got[k] - want;
        INFO (spec::T3.textAt (r, 0) << " " << spec::T3.at (r, 1) << " dBFS " << spec::T3.at (r, 2) << " s, " << what << " "
                                     << t.headerValue[3 + int (k)] << " s: want " << want << " got " << got[k]);
        CHECK (std::abs (e) <= 1.0); // spec 11: +-1 dB per point
        s.add (e);
    }
}
} // namespace

TEST_CASE ("T3 with the spec 4.3 cell model driven by ideal level steps", "[dynamics][T3]")
{
    // Literal spec procedure: the target attenuation jumps to the static value of the burst at onset and back to
    // the bed's (0 dB) at its end.
    const double fs = 48000.0;
    const auto at = times (spec::T3), rt = times (spec::T3_1);
    Score s;
    for (int r = 0; r < spec::T3.numRows; ++r)
    {
        const auto row = rowOf (r);
        const double gr = dsp::staticCurve (row.mode).gainReductionDb (row.pr, row.level);
        const double bedGr = dsp::staticCurve (row.mode).gainReductionDb (row.pr, -50.0);
        dsp::OptoCell cell;
        cell.prepare (fs);
        const int burst = int (std::round (row.seconds * fs));
        std::vector<double> attack (at.size(), std::nan ("")), release (rt.size(), std::nan (""));
        for (int i = 0; i < burst; ++i)
        {
            const double g = cell.process (1.0 - dbToLin (-gr));
            for (size_t k = 0; k < at.size(); ++k)
                if (int (std::round (at[k] * fs)) - 1 == i)
                    attack[k] = -linToDb (g);
        }
        for (int i = 0; i < int (19.5 * fs); ++i)
        {
            const double g = cell.process (1.0 - dbToLin (-bedGr));
            for (size_t k = 0; k < rt.size(); ++k)
                if (int (std::round (rt[k] * fs)) - 1 == i)
                    release[k] = -linToDb (g);
        }
        score (s, spec::T3, r, attack, "attack");
        score (s, spec::T3_1, r, release, "release");
    }
    std::printf ("[opto-leveler] T3 cell model: %d points, rms %.3f dB, max %.3f dB\n", s.n, s.rms(), s.worst);
    CHECK (s.n == 600);
    CHECK (s.rms() <= 0.4);
}

TEST_CASE ("T3 trajectories of the complete engine (1 kHz bursts on a -50 dBFS bed)", "[dynamics][T3]")
{
    const auto at = times (spec::T3), rt = times (spec::T3_1);
    Score s;
    for (int r = 0; r < spec::T3.numRows; ++r)
    {
        const auto row = rowOf (r);
        const auto tr = measureBurst (makeSettings (row.pr, row.mode), row.level, row.seconds, at, rt);
        score (s, spec::T3, r, tr.attack, "attack");
        score (s, spec::T3_1, r, tr.release, "release");
    }
    std::printf ("[opto-leveler] T3 engine: %d points, rms %.3f dB, max %.3f dB\n", s.n, s.rms(), s.worst);
    CHECK (s.n == 600);
    CHECK (s.rms() <= 0.4);
}

TEST_CASE ("gain-reduction trajectories are sample-rate independent", "[dynamics][samplerate]")
{
    const std::vector<double> at { 0.001, 0.002, 0.005, 0.01, 0.02, 0.05, 0.1, 0.2 };
    const std::vector<double> rt { 0.005, 0.02, 0.1, 0.5, 1.0, 2.0, 5.0 };
    double worst = 0.0;
    for (const auto& row : { Row { dsp::Mode::Compress, 50, -10, 0.2 }, Row { dsp::Mode::Limit, 50, -20, 0.2 },
                             Row { dsp::Mode::Compress, 70, -10, 0.2 } })
    {
        const auto s = makeSettings (row.pr, row.mode);
        const auto ref = measureBurst (s, row.level, row.seconds, at, rt, 48000.0);
        for (double fs : { 44100.0, 96000.0 })
        {
            const auto tr = measureBurst (s, row.level, row.seconds, at, rt, fs);
            for (size_t k = 0; k < at.size(); ++k)
            {
                // the attack points sample a steep curve: compare at the exact time, allowing for the
                // sub-sample offset of the rounded sample position
                INFO ("fs " << fs << " attack " << at[k] << ": " << tr.attack[k] << " vs " << ref.attack[k]);
                CHECK (std::abs (tr.attack[k] - ref.attack[k]) <= 0.1);
                worst = std::max (worst, std::abs (tr.attack[k] - ref.attack[k]));
            }
            for (size_t k = 0; k < rt.size(); ++k)
            {
                INFO ("fs " << fs << " release " << rt[k] << ": " << tr.release[k] << " vs " << ref.release[k]);
                CHECK (std::abs (tr.release[k] - ref.release[k]) <= 0.05);
                worst = std::max (worst, std::abs (tr.release[k] - ref.release[k]));
            }
        }
    }
    std::printf ("[opto-leveler] trajectory sample-rate dependence: worst %.4f dB\n", worst);
}

TEST_CASE ("release is programme dependent (spec 4.2)", "[dynamics]")
{
    const std::vector<double> at { 0.01 }, rt { 1.0 };
    const auto s = makeSettings (50.0);
    const auto shortBurst = measureBurst (s, -10.0, 0.02, at, rt);
    const auto longBurst = measureBurst (s, -10.0, 2.0, at, rt);
    // after a 20 ms burst the 1 s residual is ~0.5 dB, after >= 2 s ~1.45 dB
    CHECK (std::abs (shortBurst.release[0] - 0.52) <= 0.3);
    CHECK (std::abs (longBurst.release[0] - 1.45) <= 0.3);
    CHECK (longBurst.release[0] > 1.5 * shortBurst.release[0]);
}
