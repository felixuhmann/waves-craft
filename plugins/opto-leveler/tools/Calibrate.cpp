// Development aid (not shipped): prints how the engine meets the spec tables for a detector tuning.
// Usage: opto_calibrate [envTauMs rippleCornerHz rippleFloor rippleDepth] [sections...]
//   sections: t1 t3 t4 t5 t6 t7 (default all)
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "../tests/SpecMeasure.h"

using namespace optotest;

int main (int argc, char** argv)
{
    auto t = dsp::LevelerEngine::defaultTuning();
    int a = 1;
    // key=value overrides: tau fast onset hold knee sat rc rf depth
    for (; a < argc && std::strchr (argv[a], '=') != nullptr; ++a)
    {
        const std::string kv = argv[a];
        const auto key = kv.substr (0, kv.find ('='));
        const double v = std::atof (kv.c_str() + kv.find ('=') + 1);
        auto& d = t.detector;
        if (key == "tau") d.envelopeTau = v * 1e-3;
        else if (key == "fast") d.fastTau = v * 1e-3;
        else if (key == "onset") d.onsetDb = v;
        else if (key == "hold") d.onsetHold = v * 1e-3;
        else if (key == "est") d.estimator = int (v);
        else if (key == "fall") d.fallDb = v;
        else if (key == "persist") d.fallPersist = v * 1e-3;
        else if (key == "knee") d.compressKnee = v;
        else if (key == "sat") d.satLevel = v;
        else if (key == "satnp") d.satNepers = v;
        else if (key == "rc") d.rippleCornerHz = v;
        else if (key == "rf") d.rippleFloor = v;
        else if (key == "depth") t.rippleDepth = v;
        else if (key == "kappa") t.rippleExponent = v;
        else if (key == "rdelay") t.rippleDelay = v * 1e-6;
    }
    auto want = [&] (const char* s) {
        if (a >= argc)
            return true;
        for (int i = a; i < argc; ++i)
            if (std::strcmp (argv[i], s) == 0)
                return true;
        return false;
    };
    const double fs = 48000.0;

    if (want ("t1"))
    {
        for (int m = 0; m < 2; ++m)
        {
            const auto& tab = m == 0 ? spec::T1C : spec::T1L;
            double worst = 0.0, sq = 0.0;
            int n = 0;
            for (int r = 2; r < tab.numRows; ++r)
                for (int c = 1; c < tab.numCols; c += 1)
                {
                    const double pr = tab.at (r, 0), lev = tab.headerValue[c], want_ = tab.at (r, c);
                    if (lev < -48)
                        continue;
                    const double got = steadyGrSine (makeSettings (pr, dsp::Mode (m)), 1000.0, lev, fs, 5.0, t);
                    const double e = got - want_;
                    worst = std::max (worst, std::abs (e));
                    sq += e * e;
                    ++n;
                    if (std::abs (e) > 0.25)
                        std::printf ("  T1%c PR %3.0f L %4.0f: want %6.2f got %6.2f (%+.2f)\n", m ? 'L' : 'C', pr, lev, want_, got, e);
                }
            std::printf ("T1%c: max |err| %.3f dB, rms %.3f\n", m ? 'L' : 'C', worst, std::sqrt (sq / n));
        }
    }
    if (want ("t3"))
    {
        std::vector<double> at, rt;
        for (int c = 3; c < spec::T3.numCols; ++c)
            at.push_back (spec::T3.headerValue[c]);
        for (int c = 3; c < spec::T3_1.numCols; ++c)
            rt.push_back (spec::T3_1.headerValue[c]);
        double sq = 0.0, worst = 0.0;
        int n = 0;
        for (int r = 0; r < spec::T3.numRows; ++r)
        {
            const std::string id = spec::T3.textAt (r, 0);
            const auto mode = id[0] == 'L' ? dsp::Mode::Limit : dsp::Mode::Compress;
            const double pr = std::atof (id.c_str() + 2);
            const double lev = spec::T3.at (r, 1), dur = spec::T3.at (r, 2);
            const auto tr = measureBurst (makeSettings (pr, mode), lev, dur, at, rt, fs, t);
            std::printf ("  %s %4.0f %4.2f A:", id.c_str(), lev, dur);
            for (size_t k = 0; k < at.size(); ++k)
            {
                const double w_ = spec::T3.at (r, 3 + int (k));
                if (std::isnan (w_))
                    continue;
                const double e = tr.attack[k] - w_;
                sq += e * e;
                worst = std::max (worst, std::abs (e));
                ++n;
                std::printf (" %+.2f", e);
            }
            std::printf ("  R:");
            for (size_t k = 0; k < rt.size(); ++k)
            {
                const double w_ = spec::T3_1.at (r, 3 + int (k));
                if (std::isnan (w_))
                    continue;
                const double e = tr.release[k] - w_;
                sq += e * e;
                worst = std::max (worst, std::abs (e));
                ++n;
                std::printf (" %+.2f", e);
            }
            std::printf ("\n");
        }
        std::printf ("T3: %d points, rms %.3f dB, max %.3f dB\n", n, std::sqrt (sq / n), worst);
    }
    if (want ("t4"))
    {
        for (int r = 0; r < spec::T4.numRows; ++r)
        {
            const std::string id = spec::T4.textAt (r, 0);
            const bool lim = id.find ("Limit") != std::string::npos;
            const double hf = spec::T4.at (r, 0) == spec::T4.at (r, 0) ? spec::T4.at (r, 0) : 100.0;
            std::printf ("T4 %-12s", id.c_str());
            for (int c = 1; c < spec::T4.numCols; ++c)
            {
                const double f = spec::T4.headerValue[c];
                const double got = steadyGrSine (makeSettings (50, lim ? dsp::Mode::Limit : dsp::Mode::Compress, lim ? 100.0 : hf),
                                                 f, -10.0, fs, 5.0, t);
                std::printf (" %+6.2f", got - spec::T4.at (r, c));
            }
            std::printf ("\n");
        }
    }
    if (want ("t5"))
    {
        for (int r = 0; r < spec::T5.numRows; ++r)
        {
            std::printf ("T5 HF %3.0f", spec::T5.at (r, 0));
            for (int c = 1; c < spec::T5.numCols; ++c)
            {
                const double got = steadyGrSine (makeSettings (60, dsp::Mode::Compress, spec::T5.at (r, 0)), 1000.0,
                                                 spec::T5.headerValue[c], fs, 5.0, t);
                std::printf (" %+6.2f", got - spec::T5.at (r, c));
            }
            std::printf ("\n");
        }
    }
    if (want ("t6"))
    {
        for (int r = 0; r < spec::T6.numRows; ++r)
        {
            const bool lim = std::string (spec::T6.textAt (r, 0)) == "Limit";
            const auto s = makeSettings (50, lim ? dsp::Mode::Limit : dsp::Mode::Compress);
            const double rms = spec::T6.at (r, 1);
            const double gs = steadyGrRms (s, 0, rms, fs, 6.0, t), gn = steadyGrRms (s, 1, rms, fs, 6.0, t);
            std::printf ("T6 %s %4.0f: sine %6.2f (want %6.2f)  noise %6.2f (want %6.2f)\n", lim ? "L" : "C", rms, gs,
                         spec::T6.at (r, 2), gn, spec::T6.at (r, 3));
        }
    }
    if (want ("t7"))
    {
        for (int r = 0; r < spec::T7.numRows; ++r)
        {
            const double pr = spec::T7.at (r, 0), f = spec::T7.at (r, 1), lev = spec::T7.at (r, 2);
            const auto h = measureHarmonics (makeSettings (pr), f, lev, fs, pr > 0 ? 4.0 : 0.5, t);
            std::printf ("T7 PR %2.0f %5.0f Hz %4.0f: gain %+6.2f (%+6.2f)", pr, f, lev, h.gainDb, h.gainDb - spec::T7.at (r, 3));
            for (int k = 2; k <= 5; ++k)
                std::printf ("  H%d %7.1f (%+5.1f)", k, h.hDbc[k], h.hDbc[k] - spec::T7.at (r, 2 + k));
            std::printf ("\n");
        }
    }
    if (want ("t7q"))
    {
        // compressed rows only (PR 50, -18 ... 0 dBFS): worst harmonic error above -90 dBc
        double worst = 0.0;
        for (int r = 0; r < spec::T7.numRows; ++r)
        {
            const double pr = spec::T7.at (r, 0), f = spec::T7.at (r, 1), lev = spec::T7.at (r, 2);
            if (pr < 50 || lev < -18)
                continue;
            const auto h = measureHarmonics (makeSettings (pr), f, lev, fs, 2.5, t);
            for (int k = 2; k <= 5; ++k)
            {
                const double want_ = spec::T7.at (r, 2 + k);
                if (want_ < -90.0 || k * f >= 0.5 * fs)
                    continue;
                if (k <= 3 || f < 500.0) // H2/H3 everywhere, H4/H5 at 100 Hz
                    worst = std::max (worst, std::abs (h.hDbc[k] - want_));
                std::printf ("%+.1f ", h.hDbc[k] - want_);
            }
        }
        std::printf ("\nT7Q worst %.2f\n", worst);
    }
    return 0;
}
