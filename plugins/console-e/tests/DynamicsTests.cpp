// Spec r1 sections 6, 7 and Appendix B: compressor static tables, make-up, frequency dependence, linking,
// attack/release; expander/gate static curve, floors and timing (test vectors 3 and 4).
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <map>

#include "TestHelpers.h"

using namespace craft::consolee::test;

namespace
{
constexpr double fs = 48000.0;

// ---- Appendix B straight from the spec --------------------------------------------------------------
struct AppendixB
{
    std::map<double, std::vector<double>> gr;   // ratio -> GR at u = -40..+20 step 2 (NaN where open)
    std::map<double, std::vector<double>> makeup; // ratio -> M at T = +10, 0, -10, -20 (NaN where open)
};

const AppendixB& appendixB()
{
    static const AppendixB b = [] {
        AppendixB out;
        for (const auto& r : appendixRows())
        {
            if (r.section != "B.1" && r.section != "B.2")
                continue;
            const double ratio = r.id == "+Inf" ? INFINITY : std::stod (r.id);
            (r.section == "B.1" ? out.gr : out.makeup)[ratio] = r.values;
        }
        return out;
    }();
    return b;
}

// expected static gain (dB) or NaN where the appendix leaves the value open
double expectedGain (double ratio, double thresholdDb, double levelDb)
{
    const auto& b = appendixB();
    const auto& m = b.makeup.at (ratio);
    const int ti = thresholdDb == 10 ? 0 : thresholdDb == 0 ? 1 : thresholdDb == -10 ? 2 : 3;
    double M = m[size_t (ti)];
    if (std::isnan (M) && ti == 0) M = 0.0; // "zero at threshold +10"
    const double u = levelDb - 0.864 * thresholdDb;
    if (u < -40.0) return M;
    if (u > 20.0) return std::nan ("");
    const auto& g = b.gr.at (ratio);
    const double p = (u + 40.0) / 2.0;
    const size_t k = std::min (size_t (p), size_t (29));
    const double t = p - double (k);
    const double v = g[k] + t * (g[k + 1] - g[k]);
    return M + v;
}

Settings compSettings (double ratio, double thresholdDb, double releaseS = 0.4, bool fast = false)
{
    Settings s = withCompressor (ratio, thresholdDb);
    s.compRelease = param::compRelease.toNorm (releaseS);
    s.compFastAttack = fast;
    return s;
}

// Steady-state gain of a sine (engine output vs input amplitude at the tone frequency).
double steadyGainDb (ConsoleEngine& e, double freq, double levelDb, double seconds = 0.6)
{
    const size_t n = size_t (seconds * fs);
    const auto x = sine (freq, fs, levelDb, n);
    const auto y = run (e, x);
    const size_t len = wholeCycles (freq, fs, 0.1);
    return gainToDb (toneAmplitude (y[0], freq, fs, n - len, len) / toneAmplitude (x, freq, fs, n - len, len));
}

struct Times
{
    double t10 = -1, t63 = -1, t90 = -1;
};

Times crossings (const std::vector<double>& g, double g0, double g1)
{
    Times t;
    for (size_t i = 0; i < g.size(); ++i)
    {
        const double f = (g[i] - g0) / (g1 - g0);
        if (t.t10 < 0 && f >= 0.1) t.t10 = double (i) / fs * 1000.0;
        if (t.t63 < 0 && f >= 0.632) t.t63 = double (i) / fs * 1000.0;
        if (t.t90 < 0 && f >= 0.9) t.t90 = double (i) / fs * 1000.0;
    }
    return t;
}

// Steps a 1 kHz sine from level a to level b and records a dynamics gain trace (dB) after the step.
template <typename Probe>
std::vector<double> stepTrace (const Settings& s, double a, double b, double pre, double post, Probe probe, double& before)
{
    auto e = makeEngine (s);
    const size_t npre = size_t (pre * fs), npost = size_t (post * fs);
    std::vector<double> trace (npost);
    for (size_t i = 0; i < npre + npost; ++i)
    {
        double v = dbToGain (i < npre ? a : b) * std::sin (2.0 * pi * 1000.0 * double (i) / fs);
        double w = v;
        double* ptr[2] = { &v, &w };
        e.process (ptr, 1);
        const double g = probe (e);
        if (i == npre - 1) before = g;
        if (i >= npre) trace[i - npre] = g;
    }
    return trace;
}

bool within (double measured, double target, double tol = 0.25)
{
    return std::abs (measured - target) <= tol * target;
}
} // namespace

TEST_CASE ("Compressor: 1 kHz staircases match Appendix B (test vector 3)", "[dynamics][compressor][appendix]")
{
    int points = 0;
    double worst = 0;
    std::string where;
    for (double ratio : { 1.5, 2.0, 3.0, 4.0, 8.0, 12.0, 20.0, double (INFINITY) })
        for (double thr : { 10.0, 0.0, -10.0, -20.0 })
        {
            auto e = makeEngine (compSettings (ratio, thr));
            for (double level = -51.0; level <= 3.0; level += 3.0)
            {
                const double expected = expectedGain (ratio, thr, level);
                const double measured = steadyGainDb (e, 1000.0, level, 0.5);
                if (std::isnan (expected))
                    continue;
                const double err = std::abs (measured - expected);
                if (err > worst)
                {
                    worst = err;
                    where = "R " + std::to_string (ratio) + " T " + std::to_string (thr) + " L " + std::to_string (level);
                }
                ++points;
            }
        }
    INFO ("worst " << worst << " dB at " << where << " (" << points << " points)");
    CHECK (points > 400);
    CHECK (worst <= 0.5);
}

TEST_CASE ("Compressor: automatic make-up gain (Appendix B.2)", "[dynamics][compressor][appendix]")
{
    for (const auto& [ratio, row] : appendixB().makeup)
        for (int ti = 0; ti < 4; ++ti)
        {
            if (std::isnan (row[size_t (ti)]))
                continue;
            const double thr = 10.0 - 10.0 * ti;
            auto e = makeEngine (compSettings (ratio, thr));
            INFO ("ratio " << ratio << " threshold " << thr);
            CHECK (std::abs (steadyGainDb (e, 1000.0, -70.0, 0.3) - row[size_t (ti)]) <= 0.5);
        }
}

TEST_CASE ("Compressor: frequency dependence of the detector (section 6.4)", "[dynamics][compressor]")
{
    const double f[] = { 50, 100, 200, 500, 1000, 2000, 5000, 10000 };
    const double target[] = { -9.57, -9.55, -9.66, -9.84, -10.33, -10.84, -11.50, -11.67 };
    for (int i = 0; i < 8; ++i)
    {
        auto e = makeEngine (compSettings (4.0, -10.0));
        const double g = steadyGainDb (e, f[i], -6.0, 1.0);
        INFO (f[i] << " Hz: " << g << " dB (target " << target[i] << ")");
        CHECK (std::abs (g - target[i]) <= 0.5);
    }
}

TEST_CASE ("Compressor: one linked detector for both channels", "[dynamics][compressor]")
{
    auto e = makeEngine (compSettings (4.0, -20.0));
    const size_t n = size_t (0.6 * fs);
    const auto l = sine (1000.0, fs, -10.0, n);
    const auto r = sine (1000.0, fs, -40.0, n);
    const auto y = run (e, l, r);
    const size_t len = wholeCycles (1000.0, fs, 0.1);
    const double gl = gainToDb (toneAmplitude (y[0], 1000, fs, n - len, len) / toneAmplitude (l, 1000, fs, n - len, len));
    const double gr = gainToDb (toneAmplitude (y[1], 1000, fs, n - len, len) / toneAmplitude (r, 1000, fs, n - len, len));
    CHECK (std::abs (gl - gr) < 0.01);
    CHECK (gl < expectedGain (4.0, -20.0, -40.0) - 5.0); // the quiet channel gets the loud channel's reduction
}

TEST_CASE ("Compressor: attack times (section 6)", "[dynamics][compressor][timing]")
{
    auto probe = [] (const ConsoleEngine& e) { return e.getDynamics().getCompressorReductionDb(); };
    double before = 0;
    // ratio 4, threshold -10: +30 dB step from -36.5 to -6.5 dBFS (~13 dB reduction)
    {
        const auto tr = stepTrace (compSettings (4.0, -10.0), -36.5, -6.5, 1.0, 1.0, probe, before);
        const auto t = crossings (tr, before, tr.back());
        INFO ("slow attack " << t.t10 << " / " << t.t63 << " / " << t.t90 << " ms");
        CHECK (std::abs (tr.back() - before) > 11.0);
        CHECK (within (t.t10, 6));
        CHECK (within (t.t63, 46));
        CHECK (within (t.t90, 90));
    }
    {
        const auto tr = stepTrace (compSettings (4.0, -10.0, 0.4, true), -36.5, -6.5, 1.0, 0.5, probe, before);
        const auto t = crossings (tr, before, tr.back());
        INFO ("fast attack " << t.t10 << " / " << t.t63 << " / " << t.t90 << " ms");
        CHECK (within (t.t10, 1));
        CHECK (within (t.t63, 2));
        CHECK (within (t.t90, 5));
    }
}

TEST_CASE ("Compressor: release times (section 6)", "[dynamics][compressor][timing]")
{
    auto probe = [] (const ConsoleEngine& e) { return e.getDynamics().getCompressorReductionDb(); };
    struct Row
    {
        double release, slow63, slow90, fast63, fast90;
    };
    const Row rows[] = { { 0.1, 87, 217, 31, 73 }, { 0.4, 127, 339, 66, 278 }, { 1.0, 334, 1371, 298, 1404 }, { 4.0, 1659, 3431, 1662, 3433 } };
    for (const auto& r : rows)
        for (bool fast : { false, true })
        {
            double before = 0;
            const auto tr = stepTrace (compSettings (4.0, -10.0, r.release, fast), -6.5, -36.5, 2.0, 12.0, probe, before);
            const auto t = crossings (tr, before, tr.back());
            INFO ("release " << r.release << (fast ? " fast" : " slow") << ": " << t.t63 << " / " << t.t90 << " ms");
            CHECK (before < -11.0);
            CHECK (within (t.t63, fast ? r.fast63 : r.slow63));
            CHECK (within (t.t90, fast ? r.fast90 : r.slow90));
        }
}

TEST_CASE ("Compressor: time constants are defined in seconds (96 kHz)", "[dynamics][timing][samplerate]")
{
    auto probe = [] (const ConsoleEngine& e) { return e.getDynamics().getCompressorReductionDb(); };
    // re-run the slow attack at 96 kHz by building the engine directly
    ConsoleEngine e (2);
    e.setSettings (compSettings (4.0, -10.0));
    e.prepare (96000.0);
    const double f96 = 96000.0;
    std::vector<double> tr;
    double before = 0;
    for (size_t i = 0; i < size_t (2.0 * f96); ++i)
    {
        double v = dbToGain (i < size_t (f96) ? -36.5 : -6.5) * std::sin (2.0 * pi * 1000.0 * double (i) / f96);
        double w = v;
        double* ptr[2] = { &v, &w };
        e.process (ptr, 1);
        if (i == size_t (f96) - 1) before = probe (e);
        if (i >= size_t (f96)) tr.push_back (probe (e));
    }
    double t63 = -1;
    for (size_t i = 0; i < tr.size(); ++i)
        if ((tr[i] - before) / (tr.back() - before) >= 0.632) { t63 = double (i) / f96 * 1000.0; break; }
    CHECK (within (t63, 46));
}

TEST_CASE ("Expander: static curve (section 7.1) and floors", "[dynamics][expander]")
{
    Settings s;
    s.expThreshold = param::expThreshold.toNorm (-20.0);
    s.expRange = param::expRange.toNorm (40.0);
    const double level[] = { -45, -42, -39, -36, -33, -30, -24 };
    const double gain[] = { -16.9, -13.4, -9.9, -6.3, -2.6, 0.0, 0.0 };
    for (int i = 0; i < 7; ++i)
    {
        auto e = makeEngine (s);
        const double g = steadyGainDb (e, 1000.0, level[i], 1.0);
        INFO (level[i] << " dBFS: " << g << " dB (target " << gain[i] << ")");
        CHECK (std::abs (g - gain[i]) <= 0.5);
    }
    // floors far below the threshold
    const double range[] = { 5, 10, 15, 20 };
    const double floorDb[] = { 0.8, 7.7, 14.5, 21.4 };
    for (int i = 0; i < 4; ++i)
    {
        s.expRange = param::expRange.toNorm (range[i]);
        auto e = makeEngine (s);
        const double g = steadyGainDb (e, 1000.0, -80.0, 1.0);
        INFO ("range " << range[i] << ": " << g);
        CHECK (std::abs (g + floorDb[i]) <= 0.5);
    }
}

TEST_CASE ("Gate: hard switching and floors (section 7)", "[dynamics][gate]")
{
    Settings s;
    s.gate = true;
    s.expThreshold = param::expThreshold.toNorm (-20.0);
    for (auto [range, floorDb] : { std::pair { 20.0, 25.4 }, std::pair { 40.0, 52.7 }, std::pair { 10.0, 12.7 } })
    {
        s.expRange = param::expRange.toNorm (range);
        auto e = makeEngine (s);
        INFO ("range " << range);
        CHECK (std::abs (steadyGainDb (e, 1000.0, -33.0, 1.0) + floorDb) <= 0.5); // just below the knee
        auto e2 = makeEngine (s);
        CHECK (std::abs (steadyGainDb (e2, 1000.0, -29.0, 1.0)) <= 0.1);           // just above
    }
}

TEST_CASE ("Expander and gate: closing and opening times (section 7)", "[dynamics][expander][gate][timing]")
{
    auto probe = [] (const ConsoleEngine& e) { return e.getDynamics().getExpanderGainDb(); };
    struct Row
    {
        double release;
        bool fast;
        double e10, e63, e90, g10, g63, g90;
    };
    const Row rows[] = { { 0.1, false, 24, 68, 96, 36, 156, 332 },
                         { 0.4, true, 58, 166, 236, 86, 390, 830 },
                         { 1.0, false, 128, 364, 520, 188, 856, 1826 },
                         { 4.0, false, 470, 1344, 1922, 626, 2260, 3700 } };
    const double knee = expander::kneeDbfs (-20.0);
    for (const auto& r : rows)
        for (bool gate : { false, true })
        {
            Settings s;
            s.gate = gate;
            s.expThreshold = param::expThreshold.toNorm (-20.0);
            s.expRange = param::expRange.toNorm (40.0);
            s.expRelease = param::expRelease.toNorm (r.release);
            s.expFastAttack = r.fast;
            double before = 0;
            // the signal falls from 6 dB above to 35 dB below the threshold
            const auto tr = stepTrace (s, knee + 6.0, knee - 35.0, 1.0, 9.0, probe, before);
            const auto t = crossings (tr, before, tr.back());
            INFO ((gate ? "gate" : "expander") << " release " << r.release << ": " << t.t10 << " / " << t.t63 << " / " << t.t90);
            CHECK (within (t.t10, gate ? r.g10 : r.e10));
            CHECK (within (t.t63, gate ? r.g63 : r.e63));
            CHECK (within (t.t90, gate ? r.g90 : r.e90));
        }
    // opening: within ~4 ms (fast attack < 1 ms)
    for (bool gate : { false, true })
        for (bool fast : { false, true })
        {
            Settings s;
            s.gate = gate;
            s.expThreshold = param::expThreshold.toNorm (-20.0);
            s.expRange = param::expRange.toNorm (40.0);
            s.expFastAttack = fast;
            double before = 0;
            const auto tr = stepTrace (s, knee - 35.0, knee + 6.0, 2.0, 0.5, probe, before);
            const auto t = crossings (tr, before, tr.back());
            INFO ((gate ? "gate" : "expander") << (fast ? " fast" : " slow") << " opening t90 " << t.t90 << " ms");
            CHECK (t.t90 <= (fast ? 1.0 : 5.0));
        }
}
