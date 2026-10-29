// Spec r1 section 5: filter responses per type / slope / resonance, cutoff accuracy at any sample rate,
// resonance -> Q, spread, comb, amp. Tolerances from section 12.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "TestHelpers.h"

using namespace craft::mf;
using namespace mftest;
using Catch::Approx;

namespace
{
// Small-signal gain of the filter stage alone (no section-4 chain) at f.
double vcfGain (FilterType type, bool slope24, double fc, double res, double f, double fs, double amplitude = 0.0316)
{
    Vcf v;
    v.setType (type);
    v.setSlope24 (slope24);
    v.prepare (fs);
    VcfControl c;
    c.g = std::tan (pi * fc / fs);
    c.k = 1.0 / law::qFromResonance (res);
    return sineGain ([&] (double x) { return v.process (x, c); }, f, fs, amplitude);
}

const char* typeName (FilterType t)
{
    switch (t)
    {
        case FilterType::LowPass: return "LP";
        case FilterType::HighPass: return "HP";
        case FilterType::BandPass: return "BP";
        case FilterType::BandReject: return "BR";
        default: return "?";
    }
}
} // namespace

TEST_CASE ("Resonance to Q law", "[filter]")
{
    CHECK (law::qFromResonance (0.0) == Approx (1.0));
    CHECK (law::qFromResonance (50.0) == Approx (1.0 / (1.0 - 0.293)).epsilon (1e-6));
    CHECK (law::qFromResonance (50.0) == Approx (1.41).margin (0.01));
    CHECK (law::qFromResonance (90.0) == Approx (2.12).margin (0.02));
    CHECK (law::qFromResonance (100.0) == Approx (4.6).margin (0.01));
    // monotonic, rising steeply above 90
    double prev = 0.0;
    for (double r = 0.0; r <= 100.0; r += 0.5)
    {
        const double q = law::qFromResonance (r);
        CHECK (q > prev);
        prev = q;
    }
    CHECK (law::qFromResonance (95.0) - law::qFromResonance (90.0) > law::qFromResonance (90.0) - law::qFromResonance (80.0));
}

namespace
{
// Small-signal response of the filter in the complete processor (oversampling, chain), with the section-4
// chain removed by dividing by the response of a transparent filter (Amp type at 0 dB): "after removing
// the fixed chain" as in spec section 5. Impulse responses at -30 dBFS.
struct ChainNormalised
{
    double fs;
    std::vector<double> ref;
    explicit ChainNormalised (double rate) : fs (rate) { ref = impulseResponse (transparentSettings(), fs, 0.6); }
    std::vector<double> response (FilterType type, bool slope24, double fc, double res) const
    {
        Settings s;
        s.type = type;
        s.slope24 = slope24;
        s.freqNorm = freqKnob (fc);
        s.resonance = res;
        return impulseResponse (s, fs, 0.6);
    }
    double gainDb (const std::vector<double>& h, double f) const { return db (dftMagnitude (h, f, fs) / dftMagnitude (ref, f, fs)); }
};
} // namespace

TEST_CASE ("Small-signal magnitude matches the section-5 prototypes", "[filter]")
{
    const double freqs[] = { 30, 50, 80, 125, 200, 315, 500, 800, 1250, 2000, 3150, 5000, 6300, 8000, 10000, 12500, 16000 };
    for (double fs : { 44100.0, 48000.0, 96000.0 })
    {
        const ChainNormalised chain (fs);
        for (auto type : { FilterType::LowPass, FilterType::HighPass, FilterType::BandPass, FilterType::BandReject })
            for (bool slope24 : { false, true })
                for (double fc : { 100.0, 1000.0, 5000.0 })
                    for (double res : { 0.0, 50.0, 90.0 })
                    {
                        const double q = law::qFromResonance (res);
                        const bool loose = slope24 || type == FilterType::BandPass;
                        const double tol = loose ? 1.5 : 1.0;
                        const auto h = chain.response (type, slope24, fc, res);
                        for (double f : freqs)
                        {
                            const double expectDb = db (prototypeMagnitude (type, slope24, f, fc, q));
                            if (expectDb < -20.0)
                                continue;
                            const double gotDb = chain.gainDb (h, f);
                            INFO (typeName (type) << (slope24 ? " 24" : " 12") << " fc " << fc << " res " << res << " fs " << fs
                                                  << " f " << f << " expect " << expectDb << " got " << gotDb);
                            CHECK (std::abs (gotDb - expectDb) <= tol);
                        }
                    }
    }
}

TEST_CASE ("Cutoff equals the knob at any sample rate (review decision 1)", "[filter]")
{
    // LP and HP magnitudes cross exactly at fc (both = Q there): check the crossing within +-3 %.
    for (double fs : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        const ChainNormalised chain (fs);
        for (double fc : { 100.0, 1000.0, 5000.0, 10000.0 })
        {
            INFO ("fs " << fs << " fc " << fc);
            const auto lp = chain.response (FilterType::LowPass, false, fc, 50);
            const auto hp = chain.response (FilterType::HighPass, false, fc, 50);
            const double below = fc / 1.03, above = fc * 1.03;
            CHECK (chain.gainDb (lp, below) > chain.gainDb (hp, below));
            CHECK (chain.gainDb (lp, above) < chain.gainDb (hp, above));
            // and |LP(fc)| = Q within 10 %
            CHECK (fromDb (chain.gainDb (lp, fc)) == Approx (law::qFromResonance (50)).epsilon (0.10));
        }
    }
}

TEST_CASE ("Band-pass peak is at fc with Q-defined bandwidth", "[filter]")
{
    const double fs = 96000.0; // the filter stage's internal rate for 48 kHz hosts
    for (double res : { 0.0, 50.0, 90.0 })
    {
        const double q = law::qFromResonance (res);
        CHECK (vcfGain (FilterType::BandPass, false, 1000.0, res, 1000.0, fs) == Approx (1.0).margin (0.06));
        // -3 dB points at fc (sqrt(1 + 1/(4Q^2)) +- 1/(2Q))
        const double hi = 1000.0 * (std::sqrt (1.0 + 1.0 / (4 * q * q)) + 1.0 / (2 * q));
        CHECK (db (vcfGain (FilterType::BandPass, false, 1000.0, res, hi, fs)) == Approx (-3.01).margin (0.3));
        // 24 dB: two identical sections -> -6 dB at the same points
        CHECK (db (vcfGain (FilterType::BandPass, true, 1000.0, res, hi, fs)) == Approx (-6.02).margin (0.5));
    }
}

TEST_CASE ("Res 100 is resonant small-signal and limited at high level", "[filter]")
{
    const double fs = 96000.0;
    CHECK (vcfGain (FilterType::LowPass, false, 1000.0, 100.0, 1000.0, fs, 0.0316) == Approx (4.6).epsilon (0.10));
    const double big = vcfGain (FilterType::LowPass, false, 1000.0, 100.0, 1000.0, fs, fromDb (-3.0));
    CHECK (big == Approx (2.5).epsilon (0.15));
    // no self-oscillation without input
    Vcf v;
    v.setType (FilterType::LowPass);
    v.prepare (fs);
    VcfControl c;
    c.g = std::tan (pi * 1000.0 / fs);
    c.k = 1.0 / law::qFromResonance (100.0);
    double peak = 0.0;
    v.process (1.0, c);
    for (int i = 0; i < int (fs); ++i)
        peak = std::abs (v.process (0.0, c));
    CHECK (peak < 1.0e-6);
}

TEST_CASE ("Spread offsets left and right cutoffs by +-0.0144 oct per %", "[filter]")
{
    CHECK (law::spreadOctaves (50.0, 0) == Approx (0.72));
    CHECK (law::spreadOctaves (50.0, 1) == Approx (-0.72));
    Settings s;
    s.freqNorm = freqKnob (1000.0);
    s.spread = 50.0;
    auto e = makeEngine (s, 48000.0);
    traceTelemetry (e, 16, [] (const auto& t) { return t.cutoffHz[0]; });
    CHECK (octavesFrom (e.getTelemetry().cutoffHz[0], 1000.0) == Approx (0.72).margin (0.01));
    CHECK (octavesFrom (e.getTelemetry().cutoffHz[1], 1000.0) == Approx (-0.72).margin (0.01));
}

TEST_CASE ("Cutoff clamps to 30 Hz .. 18.6 kHz", "[filter]")
{
    Settings s;
    s.freqNorm = 1.0;
    s.mod[modSeq][destFreq] = 100.0;
    s.steps.fill (100.0);
    s.smooth = 0.0;
    auto e = makeEngine (s, 96000.0);
    traceTelemetry (e, 8, [] (const auto& t) { return t.cutoffHz[0]; });
    CHECK (e.getTelemetry().cutoffHz[0] == Approx (18600.0));
    s.freqNorm = 0.0;
    s.steps.fill (-100.0);
    auto e2 = makeEngine (s, 96000.0);
    traceTelemetry (e2, 8, [] (const auto& t) { return t.cutoffHz[0]; });
    CHECK (e2.getTelemetry().cutoffHz[0] == Approx (30.0));
}

TEST_CASE ("Comb: y[n] = x[n-D] + g y[n-D] with D = displayed delay", "[filter][comb]")
{
    for (double fs : { 44100.0, 48000.0, 96000.0 })
        for (double r : { 0.0, 0.5, 1.0 })
            for (double fb : { 0.0, 50.0, 95.0 })
            {
                const double ms = param::combDelayMs (r);
                const double d = ms * 0.001 * fs;
                Vcf v;
                v.setType (FilterType::Comb);
                v.prepare (fs);
                VcfControl c;
                c.combDelaySamples = d;
                c.combGain = law::combGain (fb);
                std::vector<double> y (size_t (3 * d + 10));
                for (size_t i = 0; i < y.size(); ++i)
                    y[i] = v.process (i == 0 ? 1.0 : 0.0, c);
                // first and second echo positions (+-1 sample) and the feedback ratio
                auto peakNear = [&] (double pos) {
                    size_t best = 0;
                    for (size_t i = size_t (pos) - 2; i <= size_t (pos) + 2; ++i)
                        if (std::abs (y[i]) > std::abs (y[best]))
                            best = i;
                    return best;
                };
                const size_t p1 = peakNear (d), p2 = peakNear (2 * d);
                INFO ("fs " << fs << " ms " << ms << " fb " << fb);
                CHECK (std::abs (double (p1) - d) <= 1.0);
                CHECK (std::abs (double (p2) - 2 * d) <= 1.0);
                double e1 = 0, e2 = 0; // energy-based ratio is robust to the fractional interpolation
                for (size_t i = p1 - 3; i <= p1 + 3; ++i) e1 += y[i];
                for (size_t i = p2 - 3; i <= p2 + 3; ++i) e2 += y[i];
                CHECK (e1 == Approx (1.0).margin (0.02));
                CHECK (e2 / e1 == Approx (0.50 + 0.0029 * fb).margin (0.01));
                CHECK (y[0] == 0.0); // no direct term
            }
    CHECK (law::combGain (0.0) == Approx (0.50));
    CHECK (law::combGain (50.0) == Approx (0.645));
    CHECK (law::combGain (95.0) == Approx (0.7755));
}

TEST_CASE ("Amp type: gain = displayed dB, -80 dB mutes", "[filter]")
{
    for (double r : { 1.0, 0.925, 0.5 })
    {
        Settings s;
        s.type = FilterType::Amp;
        s.freqNorm = r;
        s.mix = 100.0;
        const double g = engineGain (s, 96000.0, 1000.0);
        CHECK (db (g) == Approx (param::ampGainDb (r) + law::wetGainDb).margin (0.1));
    }
    Settings s;
    s.type = FilterType::Amp;
    s.freqNorm = 0.0;
    CHECK (engineGain (s, 96000.0, 1000.0) < 1.0e-6);
}

TEST_CASE ("Type and slope changes crossfade without jumps", "[filter][smoothing]")
{
    Settings s;
    s.freqNorm = freqKnob (500.0);
    auto e = makeEngine (s, 48000.0);
    std::vector<double> x (48000);
    for (size_t i = 0; i < x.size(); ++i)
        x[i] = 0.5 * std::sin (2.0 * pi * 200.0 * double (i) / 48000.0);
    double maxStep = 0.0, prev = 0.0;
    for (size_t i = 0; i < x.size(); i += 64)
    {
        if (i == 12000) { s.type = FilterType::HighPass; e.setSettings (s); }
        if (i == 24000) { s.slope24 = true; e.setSettings (s); }
        if (i == 36000) { s.type = FilterType::Comb; e.setSettings (s); }
        double l[64], r[64];
        for (int k = 0; k < 64; ++k) l[k] = r[k] = x[i + size_t (k)];
        e.process (l, r, (const double*) nullptr, (const double*) nullptr, 64);
        for (int k = 0; k < 64; ++k)
        {
            if (i + size_t (k) > 2000)
                maxStep = std::max (maxStep, std::abs (l[k] - prev));
            prev = l[k];
        }
    }
    // a 200 Hz sine at 0.5 changes by at most ~0.013 per sample; allow a little more for the fades
    CHECK (maxStep < 0.05);
}
