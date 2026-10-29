// Spec r1 sections 4, 5, 12 and Appendix A: filter and EQ magnitude responses (test vector 2).
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdio>

#include "TestHelpers.h"

using namespace craft::consolee::test;

namespace
{
bool isMagnitudeRow (const AppendixRow& r)
{
    return r.section.rfind ("A.", 0) == 0 && r.section != "A.17" && r.values.size() == 17;
}

bool isHfBandAtHighSetting (const RowSetting& s)
{
    return (s.band == RowBand::hfBell || s.band == RowBand::hfShelf) && param::hfFreq (s.freqR) >= 8.0 - 1e-9;
}

// Section 12 tolerance at frequency f for sample rate fs. At rates other than 48 kHz the 48 kHz tables are
// compared up to 12 kHz with +-0.5 dB (the reference itself agrees within 0.1 dB below 8 kHz) and with +-1.5 dB
// above (IMPLEMENTATION_NOTES.md).
double tolerance (const RowSetting& s, double f, double fs)
{
    if (fs != 48000.0 && isHfBandAtHighSetting (s))
        return 1.5;
    const double edge = std::min (0.25 * fs, 12000.0);
    if (f <= edge)
        return 0.5;
    return s.band == RowBand::lp ? 3.0 : 1.5;
}

struct Worst
{
    double excess = -1e9;
    std::string id;
    double f = 0, err = 0;
};

Worst checkAllRows (double fs, int& checked)
{
    Worst w;
    checked = 0;
    for (const auto& row : appendixRows())
    {
        if (! isMagnitudeRow (row))
            continue;
        const auto set = classifyRow (row.id);
        const auto coeffs = designEq (eqForRow (set), fs);
        const auto& freqs = appendixFrequencies();
        for (size_t i = 0; i < freqs.size(); ++i)
        {
            if (freqs[i] >= 0.5 * fs)
                continue;
            const double err = eqResponseDb (coeffs, freqs[i], fs) - row.values[i];
            const double excess = std::abs (err) - tolerance (set, freqs[i], fs);
            if (excess > w.excess)
                w = { excess, row.id, freqs[i], err };
        }
        ++checked;
    }
    return w;
}
} // namespace

TEST_CASE ("Appendix A: every magnitude row at 48 kHz is within the section 12 tolerance", "[eq][appendix]")
{
    int checked = 0;
    const auto w = checkAllRows (48000.0, checked);
    INFO ("worst row " << w.id << " at " << w.f << " Hz: error " << w.err << " dB");
    CHECK (checked == 275);
    CHECK (w.excess <= 0.0);
}

TEST_CASE ("Appendix A: rows hold at 44.1 kHz and 96 kHz (analogue-matched, no warping)", "[eq][appendix][samplerate]")
{
    for (double fs : { 44100.0, 88200.0, 96000.0, 192000.0 })
    {
        int checked = 0;
        const auto w = checkAllRows (fs, checked);
        INFO ("fs " << fs << ": worst row " << w.id << " at " << w.f << " Hz: error " << w.err << " dB");
        CHECK (w.excess <= 0.0);
    }
}

TEST_CASE ("High-pass: third-order Butterworth at the section 4 corners, Off is exact", "[eq][hp]")
{
    // section 4 table (fit error <= 0.5 dB): -3 dB at f_HP and 18 dB/oct below
    const double r[] = { 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0 };
    const double fc[] = { 16.9, 17.8, 28.2, 56.0, 102.9, 132.1, 174.6, 230.9, 322.9, 414.8 };
    for (int i = 0; i < 10; ++i)
    {
        const auto c = designHighPass (r[i], 48000.0);
        CHECK (std::abs (c.magnitudeDb (fc[i], 48000.0) + 3.01) < 0.5);
        // asymptote: one octave below the corner is ~ -18.1 dB for a third-order Butterworth
        CHECK (std::abs (c.magnitudeDb (fc[i] / 2.0, 48000.0) + 18.13) < 0.5);
    }
    const auto off = designHighPass (0.0, 48000.0);
    CHECK (off.count == 0);
}

TEST_CASE ("Low-pass: steeper with every position, Off is exact", "[eq][lp]")
{
    // section 4 lists pole frequencies of a 48 kHz two-pole fit; the normative magnitudes are Appendix A.2 (checked
    // above). Here: monotonic behaviour between the tabulated positions and an exact Off.
    double prev = 1.0;
    for (double r = 0.1; r <= 1.0001; r += 0.05)
    {
        const double db = designLowPass (r, 48000.0).magnitudeDb (5000.0, 48000.0);
        CHECK (db < prev + 1e-9);
        prev = db;
    }
    CHECK (designLowPass (0.0, 48000.0).count == 0);
    // r = 0.1: corner above 19 kHz, at most 2.7 dB loss at 20 kHz
    CHECK (designLowPass (0.1, 48000.0).magnitudeDb (20000.0, 48000.0) > -2.7);
}

TEST_CASE ("Appendix A.17: derived bell peak gains", "[eq][appendix]")
{
    int n = 0;
    double worst = 0;
    std::string worstId;
    for (const auto& row : appendixRows())
    {
        if (row.section != "A.17")
            continue;
        const auto set = classifyRow (row.id);
        const auto c = designEq (eqForRow (set), 48000.0);
        // fine log sweep for the extremum
        double peak = 0, peakF = 0;
        for (double f = 20.0; f < 21000.0; f *= 1.002)
        {
            const double db = eqResponseDb (c, f, 48000.0);
            if (std::abs (db) > std::abs (peak)) { peak = db; peakF = f; }
        }
        const double err = std::abs (peak - row.values[0]);
        if (err > worst) { worst = err; worstId = row.id; }
        // peak frequency within 10 % where the peak lies inside the measured band
        if (row.values[1] < 18000.0)
            CHECK (std::abs (std::log (peakF / row.values[1])) < std::log (1.10));
        ++n;
    }
    INFO ("worst peak gain error " << worst << " dB at " << worstId);
    CHECK (n == 192);
    CHECK (worst <= 0.5);
}

TEST_CASE ("EQ: proportional-Q character and reciprocity", "[eq]")
{
    // peak gain above the control value at narrow Q (section 5) and narrower bandwidth with more gain
    EqSettings s;
    s.lmfFreq = 0.5;
    s.lmfQ = 0.0;
    s.lmfGain = 1.0; // +15
    const auto narrow = designEq (s, 48000.0);
    CHECK (narrow.lmf.magnitudeDb (850.0, 48000.0) > 17.0);
    s.lmfQ = 1.0;
    const auto wide = designEq (s, 48000.0);
    const double wideMax = wide.lmf.magnitudeDb (840.0, 48000.0);
    CHECK (wideMax > 14.5);
    CHECK (wideMax < 16.5);
    // cut mirrors boost within ~1 dB at the peak
    s.lmfQ = 0.5;
    s.lmfGain = 1.0;
    const double boost = designEq (s, 48000.0).lmf.magnitudeDb (800.0, 48000.0);
    s.lmfGain = 0.0;
    const double cut = designEq (s, 48000.0).lmf.magnitudeDb (800.0, 48000.0);
    CHECK (std::abs (boost + cut) < 1.0);
}

TEST_CASE ("EQ: neutral settings are an exact identity", "[eq]")
{
    const auto c = designEq (EqSettings {}, 48000.0);
    CHECK (c.lf.count == 0);
    CHECK (c.lmf.count == 0);
    CHECK (c.hmf.count == 0);
    CHECK (c.hf.count == 0);
    CHECK (c.hp.count == 0);
    CHECK (c.lp.count == 0);
}

TEST_CASE ("EQ: responses stay finite and stable across the whole control space", "[eq]")
{
    for (double fs : { 44100.0, 48000.0, 96000.0 })
        for (double fr = 0; fr <= 1.0001; fr += 0.125)
            for (double g = 0; g <= 1.0001; g += 0.125)
                for (double q = 0; q <= 1.0001; q += 0.25)
                {
                    EqSettings s;
                    s.lfFreq = s.lmfFreq = s.hmfFreq = s.hfFreq = fr;
                    s.lfGain = s.lmfGain = s.hmfGain = s.hfGain = g;
                    s.lmfQ = s.hmfQ = q;
                    s.lfBell = s.hfBell = q > 0.5;
                    s.hp = s.lp = fr;
                    const auto c = designEq (s, fs);
                    for (const auto* b : { &c.lf, &c.lmf, &c.hmf, &c.hf, &c.hp, &c.lp })
                        for (int i = 0; i < b->count; ++i)
                        {
                            const auto& k = b->s[size_t (i)];
                            // stability triangle of the denominator
                            CHECK (std::abs (k.a2) < 1.0);
                            CHECK (std::abs (k.a1) < 1.0 + k.a2 + 1e-12);
                            CHECK (std::isfinite (k.b0 + k.b1 + k.b2));
                        }
                    const double db = eqResponseDb (c, 1000.0, fs);
                    CHECK (std::isfinite (db));
                }
}
