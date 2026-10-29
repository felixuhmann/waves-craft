// Craft Console E - compressor/limiter and expander/gate (spec r1 sections 6 and 7).
//
// One linked detector for all channels: each channel is high-frequency-emphasised (section 6.4), squared and
// averaged; the largest channel mean square sets the level (dBFS, calibrated so that a 1 kHz sine reads its
// peak level). The compressor applies the Appendix B static curve to that level and smooths the gain
// reduction in dB with an attack stage and a two-component release; the expander/gate drives a level
// envelope that falls at a constant dB rate set by its release control.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>

#include "MatchedBiquad.h"
#include "StaticCurves.h"

namespace craft::consolee::dsp
{

struct DynamicsSettings
{
    // compressor
    double compThresholdDb = 0.0;
    double compInvRatio = 1.0; // 1/R (1 = off, 0 = infinity)
    double compReleaseS = 0.4;
    bool compFastAttack = false;
    // expander / gate
    double expThresholdDb = -30.0;
    double expRangeDb = 0.0;
    double expReleaseS = 0.2;
    bool expFastAttack = false;
    bool gate = false;
};

namespace timing
{
// Detector mean-square averaging time constants (normal / fast-attack sections).
inline constexpr double detectorTau = 0.004;
inline constexpr double detectorFastTau = 0.0012;

// Compressor attack: one-pole on the gain reduction (dB) plus a short smoothing stage.
inline constexpr double detectorDelay = 0.00085; // response time of the gain computer (s)
inline constexpr double smoothSlowTau = 0.0005;
inline constexpr double smoothFastTau = 0.0003;

// Compressor release: two components, r(t) = w e^{-t/tau1} + (1 - w) e^{-t/tau2}; the two states also have their
// own attack time constants (att1, att2), so the attack lengthens with the release (spec r2 section 6.6).
// Tabulated at the release values of the spec tables and interpolated log-linearly in the release time.
struct ReleasePoint
{
    double releaseS, w, tau1, tau2, att1, att2;
};
inline constexpr std::array<ReleasePoint, 4> releaseSlow { { { 0.1, 0.85, 0.065, 0.160, 0.040, 0.040 },
                                                             { 0.4, 0.80, 0.095, 0.300, 0.040, 0.040 },
                                                             { 1.0, 0.70, 0.200, 1.20, 0.040, 0.040 },
                                                             { 4.0, 1.0, 1.60, 1.60, 0.056, 0.056 } } };
inline constexpr std::array<ReleasePoint, 4> releaseFast { { { 0.1, 0.85, 0.022, 0.060, 0.0014, 0.0028 },
                                                             { 0.4, 0.75, 0.035, 0.300, 0.0010, 0.0032 },
                                                             { 1.0, 0.70, 0.170, 1.25, 0.0012, 0.0040 },
                                                             { 4.0, 1.0, 1.60, 1.60, 0.0034, 0.0034 } } };

// Steady-state reduction vs attack mode and release (spec r2 section 6.5): gain change (dB, negative = more
// reduction) relative to slow attack / 0.4 s as a function of the Appendix B reduction |GR| (dB). The |GR| axis
// holds the reductions of the 6.5 table levels (ratio 4, threshold -10, -24 ... 0 dBFS); the values are the
// table values corrected for the engine's own ballistic bias (tools/TimingProbe.cpp).
inline constexpr std::array<double, 5> steadyGrAxis { 2.94, 6.00, 9.49, 13.32, 17.23 };
struct SteadyPoint
{
    double releaseS;
    std::array<double, 5> dG;
};
inline constexpr std::array<SteadyPoint, 4> steadySlow { { { 0.1, { 0.30, 0.42, 0.41, 0.50, 0.37 } },
                                                           { 0.4, { 0.0, 0.0, 0.0, 0.0, 0.0 } },
                                                           { 1.0, { -0.21, -0.34, -0.53, -0.47, -0.58 } },
                                                           { 4.0, { -0.54, -0.86, -1.16, -1.29, -1.58 } } } };
inline constexpr std::array<SteadyPoint, 4> steadyFast { { { 0.1, { -0.80, -1.09, -1.50, -1.56, -1.82 } },
                                                           { 0.4, { -0.91, -1.33, -1.75, -1.86, -2.17 } },
                                                           { 1.0, { -1.01, -1.47, -1.93, -2.14, -2.46 } },
                                                           { 4.0, { -1.09, -1.62, -2.14, -2.39, -2.76 } } } };

// Expander/gate: the level envelope falls at a constant rate (dB/s) set by the release control.
struct EnvelopePoint
{
    double releaseS, rateDbPerS, gateTau;
};
inline constexpr std::array<EnvelopePoint, 4> envelope { { { 0.1, 389.0, 0.141 },
                                                           { 0.4, 157.0, 0.352 },
                                                           { 1.0, 71.4, 0.771 },
                                                           { 4.0, 19.3, 1.70 } } };
inline constexpr double openSlowTau = 0.0012, openFastTau = 0.00015;
inline constexpr double expanderFollowTau = 0.0005;

template <typename Point, typename Get>
inline double interpLog (const std::array<Point, 4>& t, double releaseS, Get get) noexcept
{
    const double x = std::log (std::clamp (releaseS, t[0].releaseS, t[3].releaseS));
    int i = 0;
    while (i < 2 && std::log (t[size_t (i + 1)].releaseS) < x)
        ++i;
    const double x0 = std::log (t[size_t (i)].releaseS), x1 = std::log (t[size_t (i + 1)].releaseS);
    const double w = (x - x0) / (x1 - x0);
    return std::exp ((1 - w) * std::log (get (t[size_t (i)])) + w * std::log (get (t[size_t (i + 1)])));
}
// linear interpolation over log(release) (values may be zero or negative)
template <typename Point, typename Get>
inline double interpLinLog (const std::array<Point, 4>& t, double releaseS, Get get) noexcept
{
    const double x = std::log (std::clamp (releaseS, t[0].releaseS, t[3].releaseS));
    int i = 0;
    while (i < 2 && std::log (t[size_t (i + 1)].releaseS) < x)
        ++i;
    const double x0 = std::log (t[size_t (i)].releaseS), x1 = std::log (t[size_t (i + 1)].releaseS);
    const double w = (x - x0) / (x1 - x0);
    return (1 - w) * get (t[size_t (i)]) + w * get (t[size_t (i + 1)]);
}
} // namespace timing

inline double coeffForTau (double tau, double fs) noexcept { return 1.0 - std::exp (-1.0 / (std::max (tau, 1e-7) * fs)); }

// High-frequency emphasis of the detector, normalised to 0 dB at 1 kHz (fit of the section 6.4 table).
inline BiquadCoeffs detectorEmphasis (double fs) noexcept
{
    constexpr double fz = 1419.3, fp = 1727.4, q = 0.4137;
    const double wz = 2 * pi * fz, wp = 2 * pi * fp;
    AnalogSection s;
    s.b2 = 1; s.b1 = wz / q; s.b0 = wz * wz;
    s.a1 = wp / q; s.a0 = wp * wp;
    const double norm = 1.0 / std::sqrt (s.magnitudeSquared (2 * pi * 1000.0));
    s.b2 *= norm; s.b1 *= norm; s.b0 *= norm;
    return designMatched (s, fs, 2 * pi * 1000.0);
}

class Dynamics
{
public:
    static constexpr int maxChannels = 2;

    void prepare (double sampleRate) noexcept
    {
        fs = sampleRate;
        const auto e = detectorEmphasis (fs);
        for (auto& f : emphasis)
            f.c = e;
        msCoeff = coeffForTau (timing::detectorTau, fs);
        msFastCoeff = coeffForTau (timing::detectorFastTau, fs);
        delayLength = std::clamp (int (std::lround (timing::detectorDelay * fs)) + 1, 1, maxDelay);
        reset();
        updateCoefficients();
    }

    void reset() noexcept
    {
        for (auto& f : emphasis) f.reset();
        ms.fill (0.0);
        msFast.fill (0.0);
        delayLine.fill (0.0);
        delayPos = 0;
        s1 = s2 = smooth = 0.0;
        env = -200.0;
        expGain = 0.0;
        if (fs > 0) // settle the gate/expander closed for silence
            expGain = -currentFloor();
    }

    void setSettings (const DynamicsSettings& s) noexcept
    {
        settings = s;
        updateCoefficients();
    }

    // Feed one frame of detector input (numCh channels); returns the total gain in dB to apply.
    double process (const double* sc, int numCh) noexcept
    {
        double peakMs = 0.0, peakFast = 0.0;
        for (int c = 0; c < numCh; ++c)
        {
            const double x = emphasis[size_t (c)].process (sc[c]);
            ms[size_t (c)] += (x * x - ms[size_t (c)]) * msCoeff;
            msFast[size_t (c)] += (x * x - msFast[size_t (c)]) * msFastCoeff;
            peakMs = std::max (peakMs, ms[size_t (c)]);
            peakFast = std::max (peakFast, msFast[size_t (c)]);
        }
        level = 10.0 * std::log10 (2.0 * peakMs + 1e-30);
        levelFast = 10.0 * std::log10 (2.0 * peakFast + 1e-30);
        const double compLevel = settings.compFastAttack ? levelFast : level;
        const double expLevel = settings.expFastAttack ? levelFast : level;

        // ---- compressor
        double compDb = 0.0;
        if (settings.compInvRatio < 1.0)
        {
            const double staticGr = comp::staticGainDb (compLevel, settings.compThresholdDb, settings.compInvRatio) - makeup;
            delayLine[size_t (delayPos)] = std::min (0.0, staticGr + steadyShift (-staticGr));
            delayPos = (delayPos + 1) % delayLength;
            const double target = delayLine[size_t (delayPos)];
            s1 += (target - s1) * (target < s1 ? attackCoeff : rel1Coeff);
            s2 += (target - s2) * (target < s2 ? attack2Coeff : rel2Coeff);
            const double gr = relWeight * s1 + (1.0 - relWeight) * s2;
            smooth += (gr - smooth) * smoothCoeff;
            compDb = smooth + makeup;
        }
        else
        {
            s1 = s2 = smooth = 0.0;
        }
        reduction = std::min (0.0, compDb - makeup);

        // ---- expander / gate
        if (expLevel > env)
            env = expLevel;
        else
            env = std::max (expLevel, env - envFall);
        double expDb = 0.0;
        if (settings.expRangeDb > 0.0)
        {
            double target;
            const double floorDb = currentFloor();
            if (settings.gate)
                target = env >= expander::gateSwitchDbfs (settings.expThresholdDb) ? 0.0 : -floorDb;
            else
                target = expander::staticGainDb (env, settings.expThresholdDb, settings.expRangeDb);
            const double k = target > expGain ? openCoeff : (settings.gate ? gateCoeff : followCoeff);
            expGain += (target - expGain) * k;
            expDb = expGain;
        }
        else
        {
            expGain = 0.0;
        }
        expanderGain = expDb;
        return compDb + expDb;
    }

    double getLevelDb() const noexcept { return level; }
    double getCompressorReductionDb() const noexcept { return reduction; } // <= 0, without make-up
    double getExpanderGainDb() const noexcept { return expanderGain; }      // <= 0
    double getMakeupDb() const noexcept { return makeup; }

private:
    // section 6.5 shift for a static reduction of `gr` dB (>= 0)
    double steadyShift (double gr) const noexcept
    {
        const auto& ax = timing::steadyGrAxis;
        if (gr <= 0.0)
            return 0.0;
        if (gr <= ax[0])
            return steadyDg[0] * gr / ax[0];
        for (size_t k = 1; k < ax.size(); ++k)
            if (gr <= ax[k])
                return steadyDg[k - 1] + (steadyDg[k] - steadyDg[k - 1]) * (gr - ax[k - 1]) / (ax[k] - ax[k - 1]);
        const size_t n = ax.size() - 1;
        return steadyDg[n] + (steadyDg[n] - steadyDg[n - 1]) / (ax[n] - ax[n - 1]) * (gr - ax[n]);
    }

    double currentFloor() const noexcept
    {
        return settings.gate ? expander::gateFloor (settings.expRangeDb) : expander::expanderFloor (settings.expRangeDb);
    }

    void updateCoefficients() noexcept
    {
        if (fs <= 0)
            return;
        using namespace timing;
        const auto& at = settings.compFastAttack ? releaseFast : releaseSlow;
        attackCoeff = coeffForTau (interpLog (at, settings.compReleaseS, [] (const ReleasePoint& p) { return p.att1; }), fs);
        attack2Coeff = coeffForTau (interpLog (at, settings.compReleaseS, [] (const ReleasePoint& p) { return p.att2; }), fs);
        const auto& st = settings.compFastAttack ? steadyFast : steadySlow;
        for (size_t k = 0; k < steadyDg.size(); ++k)
            steadyDg[k] = interpLinLog (st, settings.compReleaseS, [k] (const SteadyPoint& p) { return p.dG[k]; });
        smoothCoeff = coeffForTau (settings.compFastAttack ? smoothFastTau : smoothSlowTau, fs);
        const auto& rt = settings.compFastAttack ? releaseFast : releaseSlow;
        relWeight = interpLog (rt, settings.compReleaseS, [] (const ReleasePoint& p) { return p.w; });
        rel1Coeff = coeffForTau (interpLog (rt, settings.compReleaseS, [] (const ReleasePoint& p) { return p.tau1; }), fs);
        rel2Coeff = coeffForTau (interpLog (rt, settings.compReleaseS, [] (const ReleasePoint& p) { return p.tau2; }), fs);
        makeup = comp::makeupDb (settings.compThresholdDb, settings.compInvRatio);

        envFall = interpLog (envelope, settings.expReleaseS, [] (const EnvelopePoint& p) { return p.rateDbPerS; }) / fs;
        gateCoeff = coeffForTau (interpLog (envelope, settings.expReleaseS, [] (const EnvelopePoint& p) { return p.gateTau; }), fs);
        openCoeff = coeffForTau (settings.expFastAttack ? openFastTau : openSlowTau, fs);
        followCoeff = coeffForTau (expanderFollowTau, fs);
    }

    double fs = 0.0;
    DynamicsSettings settings;
    std::array<Biquad, maxChannels> emphasis;
    std::array<double, maxChannels> ms {}, msFast {};
    double msCoeff = 0.0, msFastCoeff = 0.0;
    double level = -200.0, levelFast = -200.0;

    double attackCoeff = 0, attack2Coeff = 0, smoothCoeff = 0, rel1Coeff = 0, rel2Coeff = 0, relWeight = 1, makeup = 0;
    double s1 = 0, s2 = 0, smooth = 0, reduction = 0;
    std::array<double, 5> steadyDg {};
    static constexpr int maxDelay = 512;
    std::array<double, maxDelay> delayLine {};
    int delayLength = 1, delayPos = 0;

    double envFall = 0, gateCoeff = 0, openCoeff = 0, followCoeff = 0;
    double env = -200.0, expGain = 0.0, expanderGain = 0.0;
};

} // namespace craft::consolee::dsp
