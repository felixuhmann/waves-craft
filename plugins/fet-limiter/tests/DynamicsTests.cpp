// Spec r1 section 6: attack / release timing (tables/timing.csv) and release trajectories
// (tables/release_trajectories.csv), stereo link, sample-rate independence.
#include <catch2/catch_test_macros.hpp>

#include <map>

#include "TestHelpers.h"

using namespace fettest;

namespace
{
struct TimingRow
{
    std::string config;
    Revision rev;
    Ratio ratio;
    double attack, release, hold, grEnd, attT63, relT63, relT90;
    bool approx;
};

Ratio ratioFromText (const std::string& t)
{
    if (t == "20") return Ratio::R20;
    if (t == "12") return Ratio::R12;
    if (t == "8") return Ratio::R8;
    if (t == "4") return Ratio::R4;
    return Ratio::All;
}

std::vector<TimingRow> timingRows()
{
    std::vector<TimingRow> rows;
    const auto& t = specTables().timing;
    for (const auto& r : t.rows)
    {
        TimingRow row;
        row.config = r[0];
        row.rev = r[1] == "A" ? Revision::A : Revision::B;
        row.ratio = ratioFromText (r[2]);
        row.attack = std::stod (r[3]);
        row.release = std::stod (r[4]);
        row.hold = std::stod (r[5]);
        row.grEnd = std::stod (r[6]);
        row.attT63 = parseValue (r[7]);
        row.relT63 = parseValue (r[8]);
        row.relT90 = parseValue (r[9]);
        row.approx = r[10].find ("approx") != std::string::npos;
        rows.push_back (row);
    }
    return rows;
}

struct Trajectory
{
    std::vector<double> gr;
    bool approx;
};

std::vector<double> trajectoryTimes()
{
    std::vector<double> times;
    const auto& h = specTables().trajectories.header;
    for (size_t i = 2; i + 1 < h.size(); ++i)
    {
        const auto at = h[i].find ("_at_");
        times.push_back (std::stod (h[i].substr (at + 4)));
    }
    return times;
}

std::map<std::pair<std::string, double>, Trajectory> trajectories()
{
    std::map<std::pair<std::string, double>, Trajectory> out;
    for (const auto& r : specTables().trajectories.rows)
    {
        Trajectory t;
        for (size_t i = 2; i + 1 < r.size(); ++i)
            t.gr.push_back (std::stod (r[i]));
        t.approx = r.back() != "yes";
        out[{ r[0], std::stod (r[1]) }] = t;
    }
    return out;
}

// Burst level: the settled reduction of the configuration equals the long-hold gr_end (approx rows: the regular
// ~21 dB burst of the other rows, see IMPLEMENTATION_NOTES D3).
double burstInternalLevel (Revision rev, Ratio ratio, double settledReduction)
{
    StaticLaw law;
    law.set (rev, ratio);
    double lo = -20.0, hi = 60.0;
    for (int i = 0; i < 60; ++i)
    {
        const double mid = 0.5 * (lo + hi);
        (law.reduction (mid) < settledReduction ? lo : hi) = mid;
    }
    return 0.5 * (lo + hi);
}

struct BurstResult
{
    std::vector<double> gr; // per sample from the burst start
    size_t burstSamples;
    double fs;
};

// +30 dB step on a 4 kHz carrier (Input -12), hold, back to the base level; gain reduction read per sample.
BurstResult runBurst (const TimingRow& row, double settled, double fs = 48000.0, double postMs = 5200.0)
{
    Settings s = neutralSettings();
    s.revision = row.rev;
    s.ratio = row.ratio;
    s.attack = row.attack;
    s.release = row.release;
    s.inputDisplay = -12.0;
    s.outputDisplay = -18.0;
    const double gin = inputGainDb (row.rev, s.inputDisplay);
    const double xBurst = burstInternalLevel (row.rev, row.ratio, settled);
    const double aHi = dbToAmp (xBurst - gin), aLo = dbToAmp (xBurst - 30.0 - gin);
    FetEngine e;
    e.setSettings (s);
    e.prepare (fs);
    const size_t pre = size_t (0.3 * fs), burst = size_t (row.hold * 0.001 * fs), post = size_t (postMs * 0.001 * fs);
    BurstResult res { {}, burst, fs };
    res.gr.reserve (burst + post);
    for (size_t i = 0; i < pre + burst + post; ++i)
    {
        const bool on = i >= pre && i < pre + burst;
        double v = (on ? aHi : aLo) * std::cos (twoPi * 4000.0 * double (i) / fs);
        double* p = &v;
        e.process (&p, 1, 1);
        if (i >= pre)
            res.gr.push_back (e.getGainReductionDb());
    }
    return res;
}

double attackT63Ms (const BurstResult& b)
{
    const double end = b.gr[b.burstSamples - 1];
    for (size_t i = 0; i < b.burstSamples; ++i)
        if (b.gr[i] >= 0.63 * end)
            return std::max (double (i), 0.5) / b.fs * 1000.0;
    return 1e9;
}

double releaseCrossMs (const BurstResult& b, double fraction)
{
    const double end = b.gr[b.burstSamples - 1];
    for (size_t i = b.burstSamples - 1; i < b.gr.size(); ++i)
        if (b.gr[i] <= end * (1.0 - fraction))
            return double (i - (b.burstSamples - 1)) / b.fs * 1000.0;
    return 1e9;
}

double grAfterMs (const BurstResult& b, double ms)
{
    const size_t i = std::min (b.gr.size() - 1, b.burstSamples - 1 + size_t (ms * 0.001 * b.fs));
    return b.gr[i];
}

double settledReductionOf (const std::vector<TimingRow>& rows, const TimingRow& row)
{
    if (row.approx)
        return row.rev == Revision::A ? 21.3 : 22.7;
    for (const auto& r : rows)
        if (r.config == row.config && r.hold == 2000.0)
            return -r.grEnd;
    return 21.0;
}

bool within (double measured, double expected, double relTol) { return std::abs (measured / expected - 1.0) <= relTol; }
} // namespace

TEST_CASE ("Attack and release timing match tables/timing.csv (+-25 %) and trajectories (+-1.5 dB / approx +-3 dB)",
           "[dynamics]")
{
    const auto rows = timingRows();
    const auto traj = trajectories();
    const auto times = trajectoryTimes();
    for (const auto& row : rows)
    {
        const auto b = runBurst (row, settledReductionOf (rows, row));
        const double end = b.gr[b.burstSamples - 1];
        INFO (row.config << " hold " << row.hold << " ms: GR at end " << -end << " dB (table " << row.grEnd << ")");

        if (std::isfinite (row.attT63) && row.attT63 > 0.0 && row.hold == 50.0)
        {
            const double t = attackT63Ms (b);
            INFO ("attack t63 " << t << " ms (table " << row.attT63 << ")");
            CHECK (within (t, row.attT63, 0.25));
        }
        if (std::isfinite (row.relT63))
        {
            const double t = releaseCrossMs (b, 0.63);
            INFO ("release t63 " << t << " ms (table " << row.relT63 << ")");
            CHECK (within (t, row.relT63, 0.25));
        }
        if (std::isfinite (row.relT90))
        {
            const double t = releaseCrossMs (b, 0.90);
            INFO ("release t90 " << t << " ms (table " << row.relT90 << ")");
            CHECK (within (t, row.relT90, 0.25));
        }
        const auto it = traj.find ({ row.config, row.hold });
        REQUIRE (it != traj.end());
        const auto& tr = it->second;
        for (size_t k = 0; k < times.size(); ++k)
        {
            const double g = grAfterMs (b, times[k]);
            if (tr.approx)
            {
                // approx rows: the absolute level contradicts spec 5 (see notes D3); compare the recovery shape
                // normalised to the burst-end GR, scaled to the 21 dB burst, with the +-3 dB tolerance
                const double shape = (g / end - tr.gr[k] / tr.gr[0]) * 21.0;
                INFO ("t " << times[k] << " ms: shape error " << shape << " dB");
                CHECK (std::abs (shape) <= 3.0);
            }
            else
            {
                INFO ("t " << times[k] << " ms: GR " << -g << " (table " << tr.gr[k] << ")");
                CHECK (std::abs (-g - tr.gr[k]) <= 1.5);
            }
        }
    }
}

TEST_CASE ("Attack: higher ratios attack faster; 1 = slowest, 7 = fastest", "[dynamics]")
{
    const auto rows = timingRows();
    auto t63 = [&] (Ratio ratio, double attack) {
        TimingRow r { "x", Revision::A, ratio, attack, 4.0, 50.0, -21.0, 0, 0, 0, false };
        return attackT63Ms (runBurst (r, 21.3, 48000.0, 10.0));
    };
    double prev = 1e9;
    for (double a : { 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0 })
    {
        const double t = t63 (Ratio::R4, a);
        CHECK (t < prev);
        prev = t;
    }
    // geometric interpolation between the anchors: Attack 2 ~ sqrt(1.55 * 0.73)
    CHECK (within (t63 (Ratio::R4, 2.0), std::sqrt (1.55 * 0.73), 0.25));
    CHECK (t63 (Ratio::R20, 3.0) < t63 (Ratio::R8, 3.0));
    CHECK (t63 (Ratio::R12, 3.0) < t63 (Ratio::R8, 3.0));
    CHECK (t63 (Ratio::R8, 3.0) < t63 (Ratio::R4, 3.0));
    (void) rows;
}

TEST_CASE ("Release: 1 = slowest, 7 = fastest (long holds)", "[dynamics]")
{
    double prev = 1e9;
    for (double r : { 1.0, 2.5, 3.0, 4.0, 5.0, 6.0, 7.0 })
    {
        TimingRow row { "x", Revision::A, Ratio::R4, 3.0, r, 2000.0, -21.0, 0, 0, 0, false };
        const double t = releaseCrossMs (runBurst (row, 21.3, 48000.0, 9000.0), 0.63);
        INFO ("release " << r << " t63 " << t);
        CHECK (t < prev * 1.05); // the approx rows (Release 1 - 2) are within their tolerance of Release 3

        prev = t;
    }
}

TEST_CASE ("Stereo link: GR follows max(|L|, |R|)", "[dynamics][stereo]")
{
    Settings s = neutralSettings();
    s.inputDisplay = -12.0;
    auto run = [&] (bool loudLeft) {
        FetEngine e;
        e.setSettings (s);
        e.prepare (48000.0);
        auto loud = sine (1000.0, -10.0, 0.3, 48000.0);
        auto quiet = sine (1000.0, -50.0, 0.3, 48000.0);
        auto& l = loudLeft ? loud : quiet;
        auto& r = loudLeft ? quiet : loud;
        double* p[2] = { l.data(), r.data() };
        e.process (p, 2, int (l.size()));
        return e.getGainReductionDb();
    };
    const double a = run (true), b = run (false);
    CHECK (a > 10.0);
    CHECK (std::abs (a - b) < 1e-9);
}

TEST_CASE ("Attack / release times at 96 kHz match 48 kHz within 2 %", "[dynamics][samplerate]")
{
    for (double a : { 3.0, 7.0 })
        for (double r : { 4.0, 7.0 })
        {
            TimingRow row { "x", Revision::A, Ratio::R4, a, r, 50.0, -21.0, 0, 0, 0, false };
            const auto b48 = runBurst (row, 21.3, 48000.0, 4000.0);
            const auto b96 = runBurst (row, 21.3, 96000.0, 4000.0);
            INFO ("attack " << a << " release " << r);
            CHECK (within (releaseCrossMs (b96, 0.63), releaseCrossMs (b48, 0.63), 0.02));
            CHECK (within (releaseCrossMs (b96, 0.90), releaseCrossMs (b48, 0.90), 0.02));
            // attack t63 resolution is one sample at 48 kHz
            CHECK (std::abs (attackT63Ms (b96) - attackT63Ms (b48)) <= std::max (0.02 * attackT63Ms (b48), 1000.0 / 48000.0));
        }
}
