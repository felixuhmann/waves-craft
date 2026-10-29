// Spec sections 2, 3, 4, 8, 9.1, 10: tapers, filters and equaliser responses.
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cstdio>

#include "TestHelpers.h"

using namespace testing;
using cg::TaperId;

namespace
{
std::string normaliseCell (std::string cell)
{
    if (cell.rfind ("Out", 0) == 0)
        return "Off";
    const auto k = cell.find ("KHz");
    if (k != std::string::npos)
        cell.replace (k, 3, "kHz");
    return cell;
}

double analogDb (std::complex<double> h) { return cg::gainToDb (std::abs (h)); }

// Bilinear without prewarping maps the digital frequency f to the analog frequency (fs/pi) tan(pi f / fs).
double warped (double f, double fs) { return fs / cg::pi * std::tan (cg::pi * f / fs); }

const double rates[] = { 44100.0, 48000.0, 96000.0 };
} // namespace

TEST_CASE ("tapers reproduce every point of param_tapers.csv exactly", "[tapers]")
{
    const auto t = cg::parseCsv (cgt::param_tapers, cgt::param_tapers_count);
    int checked = 0;
    for (const auto& row : t.rows)
    {
        const cg::Taper* taper = nullptr;
        for (int k = 0; k < int (TaperId::count); ++k)
            if (tapers()[TaperId (k)].name == row[0])
                taper = &tapers()[TaperId (k)];
        REQUIRE (taper != nullptr);
        for (int i = 0; i < cg::Taper::points; ++i)
        {
            const double r = i / 40.0;
            INFO (row[0] << " r=" << r);
            CHECK (taper->format (r) == normaliseCell (row[size_t (i + 1)]));
            ++checked;
        }
    }
    CHECK (checked == 20 * 41);
}

TEST_CASE ("tapers interpolate linearly between points and invert", "[tapers]")
{
    const auto& thr = tapers()[TaperId::compThreshold];
    CHECK_THAT (thr.valueAt (0.0125), Catch::Matchers::WithinAbs (9.6, 1e-12));
    CHECK_THAT (thr.rForValue (-10.0), Catch::Matchers::WithinAbs (0.65 + 0.025 * 0.5 / 0.8, 1e-9));
    const auto& hp = tapers()[TaperId::hpFreq];
    CHECK (hp.isOff (0.0));
    CHECK (! hp.isOff (0.025));
    CHECK (std::isinf (tapers()[TaperId::compRatio].valueAt (1.0)));
    CHECK (tapers()[TaperId::compRatio].format (1.0) == "+Inf");
    // defaults of spec section 2
    const auto d = cg::Settings::defaults();
    CHECK_THAT (tapers()[TaperId::compThreshold].valueAt (d.compThreshold), Catch::Matchers::WithinAbs (0.0, 1e-9));
    CHECK_THAT (tapers()[TaperId::lfFreq].valueAt (d.lfFreq), Catch::Matchers::WithinAbs (60.0, 1e-9));
    CHECK_THAT (tapers()[TaperId::lmfFreq].valueAt (d.lmfFreq), Catch::Matchers::WithinAbs (500.0, 1e-9));
    CHECK_THAT (tapers()[TaperId::lmfQ].valueAt (d.lmfQ), Catch::Matchers::WithinAbs (2.5, 1e-9));
    CHECK_THAT (tapers()[TaperId::hmfQ].valueAt (d.hmfQ), Catch::Matchers::WithinAbs (1.5, 1e-9));
    CHECK_THAT (tapers()[TaperId::outputGain].valueAt (d.outputGain), Catch::Matchers::WithinAbs (0.0, 1e-9));
    CHECK_THAT (tapers()[TaperId::compRelease].valueAt (d.compRelease), Catch::Matchers::WithinAbs (0.4, 1e-9));
    CHECK_THAT (tapers()[TaperId::expRelease].valueAt (d.expRelease), Catch::Matchers::WithinAbs (0.2, 1e-9));
}

TEST_CASE ("high-pass: factored prototypes match hp.csv and the bilinear rule holds at 44.1/48/96 kHz", "[filters]")
{
    for (const auto& row : cg::HighPassTable::get().rows())
    {
        const auto p = cg::HighPassTable::get().prototype (row.display);
        for (double f = 10.0; f < 20000.0; f *= 1.2)
        {
            const std::complex<double> s (0.0, 2.0 * cg::pi * f);
            const auto ref = row.B3 * s * s * s / (((s + row.A2) * s + row.A1) * s + row.A0);
            INFO ("display " << row.display << " f " << f);
            CHECK (std::abs (analogDb (p.response (f)) - analogDb (ref)) < 0.1);
        }
        for (double fs : rates)
        {
            cg::BiquadCoeffs a, b;
            p.digitise (fs, a, b);
            for (double f = 20.0; f < 0.45 * fs; f *= 1.3)
            {
                const double d = cg::gainToDb (std::abs (a.response (f, fs) * b.response (f, fs)));
                CHECK (std::abs (d - analogDb (p.response (warped (f, fs)))) < 1e-6);
            }
        }
    }
}

TEST_CASE ("high-pass: -3 dB points and spec 9.1 vectors", "[filters]")
{
    struct Point { double display, f3; } pts[] = { { 16, 15.9 }, { 20, 17.9 }, { 70, 32.0 }, { 120, 103 }, { 200, 147 }, { 350, 415 } };
    for (const auto& pt : pts)
    {
        const auto p = cg::HighPassTable::get().prototype (pt.display);
        cg::BiquadCoeffs a, b;
        p.digitise (48000.0, a, b);
        // find the -3 dB point
        double lo = 1.0, hi = 5000.0;
        for (int i = 0; i < 100; ++i)
        {
            const double mid = std::sqrt (lo * hi);
            const double d = cg::gainToDb (std::abs (a.response (mid, 48000.0) * b.response (mid, 48000.0)));
            (d < -3.0103 ? lo : hi) = mid;
        }
        INFO ("display " << pt.display);
        CHECK (std::abs (lo / pt.f3 - 1.0) < 0.03);
    }

    struct Vec { double display; double db[13]; } vecs[] = {
        { 70, { -3.89, -0.10, -0.00, -0.00, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
        { 120, { -32.11, -14.26, -3.41, -0.08, -0.00, 0, 0, 0, 0, 0, 0, 0, 0 } },
        { 350, { -68.42, -50.41, -37.09, -19.07, -1.23, -0.02, -0.00, 0, 0, 0, 0, 0, 0 } },
    };
    for (const auto& v : vecs)
    {
        for (size_t k = 0; k < vectorFreqs().size(); ++k)
        {
            const double f = vectorFreqs()[k];
            const auto p = cg::HighPassTable::get().prototype (v.display);
            cg::BiquadCoeffs a, b;
            p.digitise (48000.0, a, b);
            const double d = cg::gainToDb (std::abs (a.response (f, 48000.0) * b.response (f, 48000.0)));
            INFO ("HP " << v.display << " at " << f << " Hz: " << d << " vs " << v.db[k]);
            CHECK (std::abs (d - v.db[k]) < 0.1);
        }
    }
}

TEST_CASE ("low-pass: fitted rows meet lp_target.csv at 44.1/48/96 kHz", "[filters]")
{
    const auto& target = cg::LowPassTarget::get();
    for (double fs : rates)
    {
        cg::LowPassDesigner d;
        d.prepare (fs);
        double worst = 0.0;
        for (size_t k = 0; k < target.rowR().size(); ++k)
        {
            const auto& c = d.rows()[k];
            for (double f : target.freqs())
            {
                if (f > 0.45 * fs)
                    continue;
                const double err = std::abs (c.magnitudeDb (f, fs) - target.rowDb (k, f));
                worst = std::max (worst, err);
                INFO ("fs " << fs << " r " << target.rowR()[k] << " f " << f);
                CHECK (err < (f <= 15000.0 ? 0.5 : 1.5));
            }
            // 20 kHz (beyond the last tabulated point: extrapolated target)
            if (20000.0 < 0.45 * fs)
                CHECK (std::abs (c.magnitudeDb (20000.0, fs) - target.rowDb (k, 20000.0)) < 1.5);
        }
        WARN ("low-pass worst table error at " << fs << " Hz: " << worst << " dB");
        // off-grid: halfway between rows (interpolated coefficients vs interpolated target)
        for (double r = 0.0375; r < 1.0; r += 0.05)
        {
            const auto c = d.coeffs (r);
            for (double f : target.freqs())
                if (f <= std::min (15000.0, 0.45 * fs))
                {
                    INFO ("off-grid fs " << fs << " r " << r << " f " << f);
                    CHECK (std::abs (c.magnitudeDb (f, fs) - target.targetDb (r, f)) < 0.5);
                }
        }
    }
}

// The 9.1 LP vectors were measured at 48 kHz, where the reference's low-pass deviates from its 96 kHz target
// (lp_target.csv) by up to 0.7 dB below 10 kHz and ~3 dB at 20 kHz (cramping towards Nyquist). Spec 8 / 10 make the
// rate-independent target normative for LP, so the vectors are checked with the observed spread (see notes).
TEST_CASE ("low-pass: spec 9.1 vectors at 48 kHz (informative, target is normative)", "[filters]")
{
    struct Vec { double display; double db[13]; } vecs[] = {
        { 3200, { 0, 0, 0, 0, -0.03, -0.14, -0.79, -3.49, -7.55, -15.46, -24.06, -30.81, -34.15 } },
        { 8000, { 0, 0, 0, 0, 0, -0.02, -0.07, -0.23, -0.50, -1.54, -4.47, -9.28, -13.94 } },
        { 12000, { 0, 0, 0, 0, 0, 0, 0, -0.01, -0.02, -0.07, -0.21, -0.62, -1.98 } },
    };
    cg::LowPassDesigner d;
    d.prepare (48000.0);
    for (const auto& v : vecs)
    {
        const double r = rOf (TaperId::lpFreq, v.display);
        const auto c = d.coeffs (r);
        for (size_t k = 0; k < vectorFreqs().size(); ++k)
        {
            const double f = vectorFreqs()[k];
            INFO ("LP " << v.display << " at " << f << ": " << c.magnitudeDb (f, 48000.0) << " vs " << v.db[k]);
            CHECK (std::abs (c.magnitudeDb (f, 48000.0) - v.db[k]) < (f <= 12000.0 ? 1.5 : 3.5));
        }
    }
}

TEST_CASE ("shelves: grid points reproduce the table prototypes; 0 dB is flat; bilinear rule", "[eq]")
{
    for (const auto* table : { &cg::ShelfTable::lf(), &cg::ShelfTable::hf() })
    {
        for (const auto& pt : table->points())
        {
            if (pt.freq == 40.0)
                continue; // single point, not a row (see notes)
            const auto p = table->prototype (pt.gain, pt.freq);
            for (double fs : rates)
            {
                const auto c = p.digitise (fs), ref = pt.proto.digitise (fs);
                for (double f = 20.0; f < 0.45 * fs; f *= 1.25)
                {
                    INFO ("gain " << pt.gain << " freq " << pt.freq << " fs " << fs << " f " << f);
                    CHECK (std::abs (c.magnitudeDb (f, fs) - ref.magnitudeDb (f, fs)) < 0.1);
                    CHECK (std::abs (c.magnitudeDb (f, fs) - analogDb (p.response (warped (f, fs)))) < 1e-6);
                }
            }
        }
        for (double f : { 30.0, 60.0, 450.0, 1500.0, 8000.0, 16000.0 })
        {
            const auto c = table->prototype (0.0, f).digitise (48000.0);
            CHECK (c.b0 == 1.0);
            CHECK (c.b1 == c.a1);
            CHECK (c.b2 == c.a2);
        }
    }
}

namespace
{
struct EqVector
{
    const char* name;
    std::function<void (cg::Settings&)> set;
    double db[13];
};

std::vector<EqVector> eqVectors()
{
    using T = TaperId;
    auto lf = [] (double g, double f) { return [=] (cg::Settings& s) { s.lfGain = rOf (T::lfGain, g); s.lfFreq = rOf (T::lfFreq, f); }; };
    auto hf = [] (double g, double f) { return [=] (cg::Settings& s) { s.hfGain = rOf (T::hfGain, g); s.hfFreq = rOf (T::hfFreq, f); }; };
    auto lmf = [] (double g, double q, double f, bool d3) {
        return [=] (cg::Settings& s) { s.lmfGain = rOf (T::lmfGain, g); s.lmfQ = rOf (T::lmfQ, q); s.lmfFreq = rOf (T::lmfFreq, f); s.lmfDiv3 = d3; };
    };
    auto hmf = [] (double g, double q, double f, bool x3) {
        return [=] (cg::Settings& s) { s.hmfGain = rOf (T::hmfGain, g); s.hmfQ = rOf (T::hmfQ, q); s.hmfFreq = rOf (T::hmfFreq, f); s.hmfX3 = x3; };
    };
    return {
        { "LF +15.0 @ 60", lf (15.0, 60), { 16.32, 14.43, 11.85, 7.30, 1.95, -0.07, -0.59, -0.47, -0.31, -0.13, -0.05, -0.02, -0.00 } },
        { "LF -15.0 @ 60", lf (-15.0, 60), { -12.98, -11.62, -9.57, -5.65, -1.19, 0.25, 0.51, 0.35, 0.21, 0.09, 0.03, 0.01, 0.00 } },
        { "LF +11.9 @ 200", lf (11.9, 200), { 13.82, 13.62, 13.17, 11.53, 6.70, 2.23, -0.66, -1.00, -0.71, -0.32, -0.12, -0.04, -0.01 } },
        { "LF -11.9 @ 450", lf (-11.9, 450), { -8.62, -8.56, -8.43, -7.87, -5.43, -2.41, -0.15, 0.51, 0.52, 0.32, 0.14, 0.05, 0.01 } },
        { "HF +15.0 @ 8k", hf (15.0, 8000), { -0.01, -0.03, -0.09, -0.33, -1.48, -1.18, 4.03, 8.85, 11.62, 14.66, 16.54, 17.40, 17.78 } },
        { "HF -15.0 @ 8k", hf (-15.0, 8000), { 0.01, 0.03, 0.08, 0.31, 1.32, 0.74, -4.09, -8.18, -10.20, -12.00, -12.86, -13.18, -13.32 } },
        { "HF +11.9 @ 1.5k", hf (11.9, 1500), { -0.21, -0.74, -1.55, -0.80, 6.67, 11.26, 13.69, 14.47, 14.69, 14.82, 14.86, 14.88, 14.88 } },
        { "HF -11.9 @ 16k", hf (-11.9, 16000), { 0.00, 0.01, 0.02, 0.06, 0.36, 0.98, 0.20, -2.99, -4.84, -6.39, -7.06, -7.31, -7.40 } },
        { "LMF +12 Q1.0 0.5k", lmf (12, 1.0, 500, false), { 0.93, 2.92, 5.64, 10.62, 15.27, 11.74, 6.61, 3.34, 1.93, 0.82, 0.36, 0.19, 0.12 } },
        { "LMF -12 Q3.5 0.5k", lmf (-12, 3.5, 500, false), { -0.11, -0.43, -1.15, -3.84, -10.22, -4.87, -1.51, -0.52, -0.26, -0.10, -0.04, -0.02, -0.01 } },
        { "LMF +20 Q1.5 0.5k", lmf (20, 1.5, 500, false), { 0.61, 2.06, 4.40, 9.76, 21.11, 13.32, 6.57, 3.14, 1.77, 0.74, 0.32, 0.17, 0.11 } },
        { "LMF +12 Q1.5 2.5k", lmf (12, 1.5, 2500, false), { 0.03, 0.12, 0.31, 1.15, 4.82, 10.45, 14.42, 9.61, 6.51, 3.42, 1.69, 0.96, 0.63 } },
        { "LMF +12 Q1.5 0.5k /3", lmf (12, 1.5, 500, true), { 4.30, 9.72, 14.68, 11.39, 3.97, 1.32, 0.36, 0.12, 0.06, 0.02, 0.01, 0.00, 0.00 } },
        { "HMF +12 Q1.0 3.5k", hmf (12, 1.0, 3500, false), { 0.02, 0.08, 0.21, 0.80, 3.53, 7.77, 12.73, 14.48, 13.13, 9.71, 6.49, 4.42, 3.23 } },
        { "HMF -12 Q0.5 3.5k", hmf (-12, 0.5, 3500, false), { -0.02, -0.08, -0.22, -0.82, -3.46, -7.03, -10.09, -10.83, -10.28, -8.39, -6.01, -4.23, -3.12 } },
        { "HMF +12 Q1.5 7k", hmf (12, 1.5, 7000, false), { 0.00, 0.01, 0.04, 0.14, 0.83, 2.69, 6.72, 11.38, 13.72, 12.00, 8.13, 5.54, 4.02 } },
        { "HMF +12 Q0.1 3.5k", hmf (12, 0.1, 3500, false), { 0.05, 0.21, 0.55, 1.87, 6.22, 10.67, 14.10, 14.90, 14.28, 12.17, 9.41, 7.17, 5.65 } },
        { "HMF +12 Q1.5 0.6k x3", hmf (12, 1.5, 600, true), { 0.03, 0.11, 0.29, 1.07, 4.56, 10.00, 14.32, 9.88, 6.75, 3.58, 1.79, 1.01, 0.67 } },
        { "combination",
          [] (cg::Settings& s) {
              s.lfGain = rOf (T::lfGain, 6.0); s.lfFreq = rOf (T::lfFreq, 60.0);
              s.hfGain = rOf (T::hfGain, -6.0); s.hfFreq = rOf (T::hfFreq, 8000.0);
              s.lmfGain = rOf (T::lmfGain, -8.0); s.lmfFreq = rOf (T::lmfFreq, 300.0); s.lmfQ = rOf (T::lmfQ, 0.75);
              s.hmfGain = rOf (T::hmfGain, 5.0); s.hmfFreq = rOf (T::hmfFreq, 3000.0); s.hmfQ = rOf (T::hmfQ, 2.0);
              s.hpFreq = rOf (T::hpFreq, 70.0); s.lpFreq = rOf (T::lpFreq, 12000.0);
          },
          { -1.30, 0.36, -2.10, -5.49, -5.54, -2.29, 0.75, 1.48, 0.64, -0.93, -2.06, -2.91, -4.45 } },
    };
}
} // namespace

TEST_CASE ("equaliser and filters: spec 9.1 vectors through the whole strip at 48 kHz (-30 dBFS)", "[eq][vectors]")
{
    double worst = 0.0;
    for (const auto& v : eqVectors())
    {
        auto s = cg::Settings::defaults();
        v.set (s);
        for (size_t k = 0; k < vectorFreqs().size(); ++k)
        {
            const double f = vectorFreqs()[k];
            const double d = stripMagnitudeDb (s, 48000.0, f);
            worst = std::max (worst, std::abs (d - v.db[k]));
            INFO (v.name << " at " << f << " Hz: " << d << " vs " << v.db[k]);
            CHECK (std::abs (d - v.db[k]) < 1.0);
        }
    }
    WARN ("spec 9.1 worst deviation: " << worst << " dB");
}

TEST_CASE ("LMF bell: table points are reproduced exactly (bilinear of the effective bell)", "[eq]")
{
    const auto& tab = cg::BellTable::lmf();
    for (const auto& pt : tab.points())
    {
        const auto p = tab.params (pt.gain, rOf (TaperId::lmfQ, pt.q), pt.freq, pt.div3);
        INFO ("LMF g " << pt.gain << " Q " << pt.q << " f " << pt.freq << " /3 " << pt.div3);
        CHECK_THAT (p.f0, Catch::Matchers::WithinRel (pt.p.f0, 1e-6));
        CHECK_THAT (p.q, Catch::Matchers::WithinRel (pt.p.q, 1e-6));
        CHECK_THAT (p.gainDb, Catch::Matchers::WithinRel (pt.p.gainDb, 1e-6));
    }
}

TEST_CASE ("HMF bell: table points reproduced; magnitude-matched digital bell meets the target at 44.1/48/96 kHz", "[eq]")
{
    const auto& tab = cg::BellTable::hmf();
    for (const auto& pt : tab.points())
    {
        const auto p = tab.params (pt.gain, rOf (TaperId::hmfQ, pt.q), pt.freq);
        INFO ("HMF g " << pt.gain << " Q " << pt.q << " f " << pt.freq);
        CHECK_THAT (p.f0, Catch::Matchers::WithinRel (pt.p.f0, 1e-6));
        CHECK_THAT (p.q, Catch::Matchers::WithinRel (pt.p.q, 1e-6));
        CHECK_THAT (p.gainDb, Catch::Matchers::WithinRel (pt.p.gainDb, 1e-6));
        for (double fs : rates)
        {
            const auto c = p.matched (fs);
            for (double f = 20.0; f <= 20000.0; f *= 1.15)
            {
                INFO ("fs " << fs << " f " << f);
                const double err = std::abs (c.magnitudeDb (f, fs) - analogDb (p.response (f)));
                CHECK (err < (f <= 15000.0 ? 0.5 : 1.5));
            }
        }
    }
    // x3 at the top of the range stays within tolerance at 44.1 kHz
    auto p = tab.params (12.0, rOf (TaperId::hmfQ, 1.5), 7000.0);
    p.f0 *= 3.0;
    const auto c = p.matched (44100.0);
    for (double f = 20.0; f <= 15000.0; f *= 1.15)
        CHECK (std::abs (c.magnitudeDb (f, 44100.0) - analogDb (p.response (f))) < 0.5);
}

TEST_CASE ("sample-rate rule: HMF and LP are nearly rate independent, shelves follow the bilinear rule", "[eq]")
{
    auto s = cg::Settings::defaults();
    s.hmfGain = rOf (TaperId::hmfGain, 12.0);
    s.lpFreq = rOf (TaperId::lpFreq, 8000.0);
    for (double f : { 100.0, 1000.0, 3500.0, 10000.0, 15000.0 })
    {
        const double a = stripMagnitudeDb (s, 44100.0, f), b = stripMagnitudeDb (s, 96000.0, f);
        INFO ("f " << f << ": " << a << " vs " << b);
        CHECK (std::abs (a - b) < 0.6);
    }
}
