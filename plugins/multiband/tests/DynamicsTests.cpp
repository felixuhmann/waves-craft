// Spec r2 sections 5 and 6: attack / release / behaviour / auto envelope tables, peak-detector noise vector,
// stereo link, sample-rate independence.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "SpecTables.h"
#include "TestHelpers.h"

using namespace craft::multiband;
using Catch::Approx;

namespace
{
const double rates[] = { 44100.0, 48000.0, 96000.0 };

enum class Stimulus { attack, release, burst };

// Band-2 gain (dB) at the table times after the event, plus the value just before it.
struct Trace
{
    double start = 0.0;
    std::vector<double> at;
};

Trace runRow (const spec::EnvelopeRow& row, Stimulus kind, double fs)
{
    std::vector<std::pair<double, double>> segs;
    double eventS = 0.0;
    const std::vector<double>* times = nullptr;
    switch (kind)
    {
        case Stimulus::attack:  segs = { { 0.3, -50 }, { 0.55, -10 } }; eventS = 0.3; times = &spec::attackTimesMs; break;
        case Stimulus::release: segs = { { 0.3, -50 }, { 1.0, -10 }, { 2.05, -50 } }; eventS = 1.3; times = &spec::releaseTimesMs; break;
        // r2 5.2: the burst follows 2.5 s at -50 dBFS after a 1 s overload
        case Stimulus::burst:   segs = { { 0.3, -50 }, { 1.0, -10 }, { 2.5, -50 }, { 0.03, -10 }, { 0.55, -50 } }; eventS = 3.83; times = &spec::burstTimesMs; break;
    }
    const auto s = test::band2Settings (row.rangeDb, row.knee, row.attackMs, row.releaseMs, row.opto, row.autoRelease);
    const auto g = test::runBandGain (s, fs, test::toneSegments (fs, 1000.0, segs), 1);
    const auto ev = size_t (std::lround (eventS * fs));
    Trace t;
    t.start = g[ev - 1];
    for (double ms : *times)
        t.at.push_back (g[ev + size_t (std::lround (ms * fs / 1000.0))]);
    return t;
}

void checkRows (const std::vector<spec::EnvelopeRow>& rows, Stimulus kind, double fs, double toleranceScale = 1.0)
{
    for (const auto& row : rows)
    {
        const auto t = runRow (row, kind, fs);
        for (size_t i = 0; i < row.values.size(); ++i)
        {
            INFO (row.name << (row.autoRelease ? " (Auto)" : " (Manual)") << " at index " << i << ", fs " << fs);
            CHECK (t.at[i] == Approx (row.values[i]).margin (toleranceScale * spec::envelopeTolerance (row.values[i], t.start)));
        }
    }
}
} // namespace

TEST_CASE ("Attack envelope table (section 5)", "[dynamics]")
{
    for (double fs : rates)
        checkRows (spec::attackRows, Stimulus::attack, fs);
}

TEST_CASE ("Release envelope table after a sustained overload, Manual (section 5)", "[dynamics]")
{
    for (double fs : rates)
        checkRows (spec::releaseRows, Stimulus::release, fs);
}

TEST_CASE ("After a sustained overload Auto is identical to Manual", "[dynamics]")
{
    for (auto row : spec::releaseRows)
    {
        const auto manual = runRow (row, Stimulus::release, 48000.0);
        row.autoRelease = true;
        const auto autoR = runRow (row, Stimulus::release, 48000.0);
        for (size_t i = 0; i < row.values.size(); ++i)
        {
            INFO (row.name << " at index " << i);
            CHECK (autoR.at[i] == Approx (row.values[i]).margin (spec::envelopeTolerance (row.values[i], autoR.start)));
            CHECK (autoR.at[i] == Approx (manual.at[i]).margin (0.6)); // Opto at +20 ms: up to 0.53 dB
        }
    }
}

TEST_CASE ("Release after a 30 ms burst, Manual and Auto (section 5)", "[dynamics]")
{
    for (double fs : rates)
        checkRows (spec::burstRows, Stimulus::burst, fs);

    // Auto releases faster than Manual after a short overload; at Release 20 ms the difference is <= 0.1 dB.
    for (const auto& row : spec::burstRows)
        if (row.autoRelease)
        {
            auto manualRow = row;
            manualRow.autoRelease = false;
            const auto a = runRow (row, Stimulus::burst, 48000.0), m = runRow (manualRow, Stimulus::burst, 48000.0);
            INFO (row.name);
            CHECK (a.at[0] >= m.at[0] - 1e-9);
        }
    for (bool opto : { false, true })
    {
        spec::EnvelopeRow r { "Rel 20", 5, 20, 0.5, -12, opto, false, {} };
        const auto m = runRow (r, Stimulus::burst, 48000.0);
        r.autoRelease = true;
        const auto a = runRow (r, Stimulus::burst, 48000.0);
        for (size_t i = 0; i < a.at.size(); ++i)
            CHECK (std::abs (a.at[i] - m.at[i]) <= 0.1);
    }
}

TEST_CASE ("Electro releases slowly at large gain change, Opto fast (shape, section 5)", "[dynamics]")
{
    // Same Release: at 20 ms Opto has recovered more than Electro; near the end Electro has caught up.
    spec::EnvelopeRow e { "e", 5, 500, 0.5, -12, false, false, {} }, o { "o", 5, 500, 0.5, -12, true, false, {} };
    const auto te = runRow (e, Stimulus::release, 48000.0), to = runRow (o, Stimulus::release, 48000.0);
    CHECK (to.at[0] > te.at[0] + 1.0);  // 20 ms
    CHECK (te.at[5] > to.at[5] - 0.25); // 1 s
}

TEST_CASE ("Peak-detector noise vector (section 6)", "[dynamics]")
{
    const double fs = 48000.0;
    // white noise, scaled so that its band-2 component has the requested RMS
    const auto w = test::whiteNoise (size_t (3 * fs), 11);
    std::array<CrossoverSection, 3> secs;
    for (int k = 0; k < 3; ++k)
        secs[size_t (k)] = CrossoverSection::design (param::defaultCrossover[size_t (k)], 0.6, fs);
    FourBandSplitter sp;
    sp.setSections (secs);
    std::vector<double> band (w.size());
    std::array<double, 4> b {};
    double e = 0.0;
    for (size_t i = 0; i < w.size(); ++i)
    {
        sp.process (w[i], b);
        band[i] = b[1];
        e += b[1] * b[1];
    }
    const double rms = std::sqrt (e / double (w.size()));

    std::vector<double> results[2];
    int set = 0;
    for (auto [att, rel] : { std::pair<double, double> { 0.5, 5.0 }, { 5.0, 50.0 } })
    {
        for (const auto& p : spec::noisePoints)
        {
            const double scale = test::dbToLin (p.bandRmsDb) / rms;
            std::vector<double> x (w.size());
            for (size_t i = 0; i < w.size(); ++i)
                x[i] = w[i] * scale;
            const auto s = test::band2Settings (-12.0, 0.5, att, rel, false, false);
            const auto g = test::runBandGain (s, fs, x, 1);
            // energy-weighted band gain over the last 2.5 s
            double ein = 0.0, eout = 0.0;
            for (size_t i = size_t (0.5 * fs); i < x.size(); ++i)
            {
                const double v = band[i] * scale;
                ein += v * v;
                eout += v * v * test::dbToLin (2.0 * g[i]);
            }
            INFO ("band RMS " << p.bandRmsDb << " dB, Attack " << att << ", Release " << rel);
            const double gain = 10.0 * std::log10 (eout / ein);
            CHECK (gain == Approx (p.gainDb).margin (0.75));
            results[set].push_back (gain);
        }
        ++set;
    }
    // the reference gives the same reduction for both timings
    for (size_t k = 0; k < results[0].size(); ++k)
        CHECK (std::abs (results[0][k] - results[1][k]) <= 0.3);
}

TEST_CASE ("Stereo link: the louder channel sets the common band gain (section 6)", "[dynamics]")
{
    const double fs = 48000.0;
    for (double range : { -12.0, 12.0 })
    {
        const auto s = test::band2Settings (range, 0.5, 5.0, 50.0, false, false);
        const auto loud = test::toneSegments (fs, 1000.0, { { 1.0, -10.0 } });
        const auto soft = test::toneSegments (fs, 1000.0, { { 1.0, -40.0 } });
        const double expect = range < 0 ? -10.17 : 10.43;

        // engine output per channel: both channels get the same gain
        for (int swap = 0; swap < 2; ++swap)
        {
            MultibandEngine e;
            e.setSettings (s);
            e.prepare (fs, 2);
            auto out = test::runStereo (e, swap ? soft : loud, swap ? loud : soft);
            const double g = e.getBandGainDb (1);
            CHECK (g == Approx (expect).margin (0.3));

            // measure the actual channel gains of the 1 kHz tone over the last 0.2 s (band 2 dominates at 1 kHz)
            auto rmsTail = [&] (const std::vector<double>& v) {
                double acc = 0.0;
                for (size_t i = v.size() - 9600; i < v.size(); ++i) acc += v[i] * v[i];
                return std::sqrt (acc / 9600.0);
            };
            const double gl = test::linToDb (rmsTail (out.first) / rmsTail (swap ? soft : loud));
            const double gr = test::linToDb (rmsTail (out.second) / rmsTail (swap ? loud : soft));
            CHECK (gl == Approx (gr).margin (0.01));
        }
    }
}

TEST_CASE ("Mono engine equals the stereo engine with identical channels", "[dynamics][engine]")
{
    const double fs = 48000.0;
    Settings s = Settings::defaults();
    const auto x = test::whiteNoise (size_t (fs), 21, 0.2);
    MultibandEngine st, mo;
    st.setSettings (s);
    st.prepare (fs, 2);
    mo.setSettings (s);
    mo.prepare (fs, 1);
    auto a = test::runStereo (st, x, x);
    std::vector<double> m = x;
    double* io[1] = { m.data() };
    mo.process (io, int (m.size()));
    double maxErr = 0.0;
    for (size_t i = 0; i < x.size(); ++i)
        maxErr = std::max (maxErr, std::abs (a.first[i] - m[i]));
    CHECK (maxErr < 1e-12);
}

TEST_CASE ("Sample-rate independence of static curve and envelopes (44.1 / 48 / 96 kHz)", "[dynamics]")
{
    const std::vector<spec::EnvelopeRow> rows { spec::attackRows[1], spec::attackRows[4], spec::attackRows[8] };
    for (const auto& row : rows)
    {
        const auto ref = runRow (row, Stimulus::attack, 48000.0);
        for (double fs : { 44100.0, 96000.0 })
        {
            const auto t = runRow (row, Stimulus::attack, fs);
            for (size_t i = 0; i < t.at.size(); ++i)
            {
                INFO (row.name << " at index " << i << ", fs " << fs);
                // within 0.05 dB except at 1-2 ms where one sample of timing is a larger share
                CHECK (t.at[i] == Approx (ref.at[i]).margin (i < 2 ? 0.3 : 0.05));
            }
        }
    }
    for (const auto& row : { spec::releaseRows[2], spec::releaseRows[9], spec::releaseRows[5] })
    {
        const auto ref = runRow (row, Stimulus::release, 48000.0);
        for (double fs : { 44100.0, 96000.0 })
        {
            const auto t = runRow (row, Stimulus::release, fs);
            for (size_t i = 0; i < t.at.size(); ++i)
            {
                INFO (row.name << " at index " << i << ", fs " << fs);
                CHECK (t.at[i] == Approx (ref.at[i]).margin (0.05));
            }
        }
    }
}

TEST_CASE ("Release after overloads of different duration and level (r2 section 5.3)", "[dynamics]")
{
    for (double fs : rates)
    {
        std::vector<std::pair<double, double>> segs;
        std::vector<size_t> ends;
        size_t n = 0;
        for (const auto& sg : spec::overloadStimulus)
        {
            segs.push_back ({ sg.seconds, sg.levelDb });
            n += size_t (std::lround (sg.seconds * fs));
            ends.push_back (n);
        }
        const auto x = test::toneSegments (fs, 1000.0, segs);
        for (const auto& row : spec::overloadRows)
        {
            auto s = test::band2Settings (row.rangeDb, row.knee, row.attackMs, row.releaseMs, row.opto, row.autoRelease);
            s.bands[1].thresholdDb = row.thresholdDb;
            const auto g = test::runBandGain (s, fs, x, 1);
            for (size_t o = 0; o < 5; ++o)
            {
                const size_t end = ends[size_t (spec::overloadSegments[o])];
                // the table's start column (band gain at the end of the overload) sets the tolerance; it is not a
                // checked point itself (250 checked points = 10 settings x 5 overloads x 5 times)
                const double start = row.values[o][0];
                for (size_t i = 0; i < 5; ++i)
                {
                    const double v = g[end + size_t (std::lround (spec::overloadTimesMs[i] * fs / 1000.0))];
                    INFO (row.name << ", overload " << o << ", +" << spec::overloadTimesMs[i] << " ms, fs " << fs);
                    CHECK (v == Approx (row.values[o][i + 1]).margin (spec::envelopeTolerance (row.values[o][i + 1], start)));
                }
            }
        }
    }
}
