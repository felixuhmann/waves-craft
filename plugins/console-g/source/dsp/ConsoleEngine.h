// Craft Console G - complete channel strip (spec section 1 signal flow), host independent.
#pragma once

#include <array>
#include <cmath>
#include <cstdint>

#include "Biquad.h"
#include "DynamicsSection.h"
#include "Equaliser.h"
#include "Filters.h"
#include "Tapers.h"

namespace craft::consoleg::dsp
{

// Host values: every continuous parameter is the spec's normalised r in [0, 1] (section 2).
struct Settings
{
    double compThreshold = 0, compRatio = 0, compRelease = 0, expThreshold = 0, expRange = 0, expRelease = 0;
    bool compFast = false, expGate = false, expFast = false;
    bool dynBypass = false, dynPostEq = false, filtersSidechain = false, filtersSplit = false, eqBypass = false;
    double hpFreq = 0, lpFreq = 0, lfGain = 0, lfFreq = 0, lmfGain = 0, lmfFreq = 0, lmfQ = 0, hmfGain = 0, hmfFreq = 0,
           hmfQ = 0, hfGain = 0, hfFreq = 0;
    bool lmfDiv3 = false, hmfX3 = false;
    double inputTrim = 0, outputGain = 0;
    bool analog = false, polarity = false; // polarity true = inverted

    // Spec section 2 defaults, expressed as host values.
    static Settings defaults()
    {
        const auto& t = Tapers::get();
        Settings s;
        s.compThreshold = t[TaperId::compThreshold].rForValue (0.0);
        s.compRatio = 0.0;
        s.compRelease = t[TaperId::compRelease].rForValue (0.4);
        s.expThreshold = t[TaperId::expThreshold].rForValue (-30.0);
        s.expRange = 0.0;
        s.expRelease = t[TaperId::expRelease].rForValue (0.2);
        s.hpFreq = 0.0;
        s.lpFreq = 0.0;
        s.lfGain = t[TaperId::lfGain].rForValue (0.0);
        s.lfFreq = t[TaperId::lfFreq].rForValue (60.0);
        s.hfGain = t[TaperId::hfGain].rForValue (0.0);
        s.hfFreq = t[TaperId::hfFreq].rForValue (8000.0);
        s.lmfGain = t[TaperId::lmfGain].rForValue (0.0);
        s.lmfFreq = t[TaperId::lmfFreq].rForValue (500.0);
        s.lmfQ = t[TaperId::lmfQ].rForValue (2.5);
        s.hmfGain = t[TaperId::hmfGain].rForValue (0.0);
        s.hmfFreq = t[TaperId::hmfFreq].rForValue (3500.0);
        s.hmfQ = t[TaperId::hmfQ].rForValue (1.5);
        s.inputTrim = t[TaperId::inputTrim].rForValue (0.0);
        s.outputGain = t[TaperId::outputGain].rForValue (0.0);
        return s;
    }
};

class ConsoleEngine
{
public:
    static constexpr int maxChannels = 2;
    static constexpr double baselineCubic = 0.00316;        // spec 1.1
    static constexpr double analogNoiseDbfs = -110.8;       // spec 1.2 (RMS, per channel)
    static constexpr double smoothingMs = 4.0;              // D6: ~99 % after 18 ms
    static constexpr double rampMs = 10.0;                  // switch cross-fades
    static constexpr double routeDuckMs = 3.0;              // routing change: fade out, switch, fade in
    static constexpr int controlInterval = 16;

    static int getLatencySamples() noexcept { return 0; }

    // Output safety (not in the spec): with every gain at maximum (trim +18 dB, EQ boosts, make-up, output +12 dB)
    // a full-scale input can exceed +40 dBFS. Levels above +24 dBFS are bent smoothly towards +30 dBFS; nothing below
    // +24 dBFS is affected (all spec behaviour lies far below).
    static double safetyClip (double v) noexcept
    {
        constexpr double knee = 16.0, span = 16.0; // +24 dBFS, limit +30 dBFS
        const double a = std::abs (v);
        if (a <= knee)
            return v;
        const double y = knee + span * std::tanh ((a - knee) / span);
        return v < 0.0 ? -y : y;
    }

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        lowPass.prepare (fs);
        dynamics.prepare (fs);
        smoothCoeff = std::exp (-1000.0 / (smoothingMs * fs));
        rampStep = 1000.0 / (rampMs * fs);
        duckStep = 1000.0 / (routeDuckMs * fs);
        reset();
    }

    void reset()
    {
        const uint32_t seeds[maxChannels] = { 0x9e3779b9u, 0x85ebca6bu }; // independent noise per channel (spec 1.2)
        for (size_t c = 0; c < chans.size(); ++c)
        {
            chans[c] = Channel {};
            chans[c].rng = seeds[c];
        }
        dynamics.reset();
        snapToTargets();
        designAll (true);
    }

    void setSettings (const Settings& s) noexcept { target = s; }
    const Settings& getSettings() const noexcept { return target; }

    // In-place processing of 1 or 2 channels (stereo: linked dynamics).
    template <typename Sample>
    void process (Sample* const* data, int numChannels, int numSamples) noexcept
    {
        numChannels = std::min (numChannels, maxChannels);
        blockInPeak = blockOutPeak = 0.0;
        blockMinGrDb = 0.0;
        for (int i = 0; i < numSamples; ++i)
        {
            if (controlCounter-- <= 0)
            {
                controlCounter = controlInterval - 1;
                updateControl();
            }
            stepSmoothers();

            double x[maxChannels] {}, det[maxChannels] {};
            for (int c = 0; c < numChannels; ++c)
            {
                double v = double (data[c][i]);
                if (! std::isfinite (v))
                    v = 0.0;
                v *= polaritySm * trimSm;
                v += baselineCubic * v * v * v;
                blockInPeak = std::max (blockInPeak, std::abs (v));
                x[c] = v;
            }

            // routing (spec 1)
            double peak = 0.0;
            switch (route)
            {
                case Route::sidechainPre:
                    for (int c = 0; c < numChannels; ++c) { det[c] = filters (c, x[c]); peak = std::max (peak, std::abs (det[c])); }
                    applyDynamics (x, numChannels, peak);
                    for (int c = 0; c < numChannels; ++c) x[c] = eq (c, x[c]);
                    break;
                case Route::sidechainPost:
                    for (int c = 0; c < numChannels; ++c) { x[c] = eq (c, x[c]); det[c] = filters (c, x[c]); peak = std::max (peak, std::abs (det[c])); }
                    applyDynamics (x, numChannels, peak);
                    break;
                case Route::postEq:
                    for (int c = 0; c < numChannels; ++c) { x[c] = eq (c, filters (c, x[c])); peak = std::max (peak, std::abs (x[c])); }
                    applyDynamics (x, numChannels, peak);
                    break;
                case Route::split:
                    for (int c = 0; c < numChannels; ++c) { x[c] = filters (c, x[c]); peak = std::max (peak, std::abs (x[c])); }
                    applyDynamics (x, numChannels, peak);
                    for (int c = 0; c < numChannels; ++c) x[c] = eq (c, x[c]);
                    break;
                case Route::normal:
                default:
                    for (int c = 0; c < numChannels; ++c) peak = std::max (peak, std::abs (x[c]));
                    applyDynamics (x, numChannels, peak);
                    for (int c = 0; c < numChannels; ++c) x[c] = eq (c, filters (c, x[c]));
                    break;
            }

            for (int c = 0; c < numChannels; ++c)
            {
                double v = x[c];
                if (analogMix > 0.0)
                    v += analogMix * noiseAmp * chans[size_t (c)].noise();
                v *= outSm * duck;
                if (! std::isfinite (v))
                    v = 0.0;
                v = safetyClip (v);
                blockOutPeak = std::max (blockOutPeak, std::abs (v));
                data[c][i] = Sample (v);
            }
        }
        sanitise (numChannels);
    }

    // Block meters (linear peaks; gain reduction in dB, <= 0, excludes make-up).
    double getInputPeak() const noexcept { return blockInPeak; }
    double getOutputPeak() const noexcept { return blockOutPeak; }
    double getGainReductionDb() const noexcept { return blockMinGrDb; }

    const LowPassDesigner& getLowPassDesigner() const noexcept { return lowPass; }
    double getSampleRate() const noexcept { return fs; }

private:
    enum class Route { normal, split, postEq, sidechainPre, sidechainPost };

    struct Channel
    {
        Biquad hp2, hp1, lp, lf, lmf, hmf, hf;
        uint32_t rng = 0x9e3779b9u;
        // white noise, uniform, unit RMS
        double noise() noexcept
        {
            rng ^= rng << 13;
            rng ^= rng >> 17;
            rng ^= rng << 5;
            return (double (rng) / 4294967295.0 * 2.0 - 1.0) * 1.7320508075688772;
        }
    };

    struct Smoothed
    {
        double value = 0.0, targetValue = 0.0;
        void snap() noexcept { value = targetValue; }
        bool step (double coeff) noexcept
        {
            if (value == targetValue)
                return false;
            value = targetValue + coeff * (value - targetValue);
            if (std::abs (value - targetValue) < 1.0e-7)
                value = targetValue;
            return true;
        }
    };

    struct Ramp
    {
        double value = 0.0, targetValue = 0.0;
        void step (double s) noexcept
        {
            if (value < targetValue)
                value = std::min (targetValue, value + s);
            else if (value > targetValue)
                value = std::max (targetValue, value - s);
        }
    };

    enum Sm { smHp, smLp, smLfG, smLfF, smLmfG, smLmfF, smLmfQ, smHmfG, smHmfF, smHmfQ, smHfG, smHfF, smThr, smRatio, smCount };

    double filters (int c, double x) noexcept
    {
        auto& ch = chans[size_t (c)];
        const double h = ch.hp1.process (ch.hp2.process (x));
        double y = x + hpMix.value * (h - x);
        const double l = ch.lp.process (y);
        return y + lpMix.value * (l - y);
    }

    double eq (int c, double x) noexcept
    {
        auto& ch = chans[size_t (c)];
        const double y = ch.hf.process (ch.hmf.process (ch.lmf.process (ch.lf.process (x))));
        return x + eqMix.value * (y - x);
    }

    void applyDynamics (double* x, int numChannels, double peak) noexcept
    {
        const double gDb = dynamics.process (peak);
        const double g = 1.0 + dynMix.value * (dbToGain (gDb) - 1.0);
        for (int c = 0; c < numChannels; ++c)
            x[c] *= g;
        const double gr = dynMix.value * (gDb - dynamics.makeup());
        blockMinGrDb = std::min (blockMinGrDb, gr);
    }

    Route routeFor (const Settings& s) const noexcept
    {
        if (s.filtersSidechain)
            return s.dynPostEq ? Route::sidechainPost : Route::sidechainPre;
        if (s.dynPostEq)
            return Route::postEq;
        return s.filtersSplit ? Route::split : Route::normal;
    }

    void setTargets() noexcept
    {
        const Settings& s = target;
        const double rs[smCount] = { s.hpFreq, s.lpFreq, s.lfGain, s.lfFreq, s.lmfGain, s.lmfFreq, s.lmfQ, s.hmfGain,
                                     s.hmfFreq, s.hmfQ, s.hfGain, s.hfFreq, s.compThreshold, s.compRatio };
        for (int k = 0; k < smCount; ++k)
            sm[k].targetValue = std::min (1.0, std::max (0.0, rs[k]));
        const auto& t = Tapers::get();
        const bool hpOn = ! t[TaperId::hpFreq].isOff (s.hpFreq), lpOn = ! t[TaperId::lpFreq].isOff (s.lpFreq);
        // a filter that is switched on starts at its new frequency (no sweep from the "Off" position)
        if (hpOn && hpMix.value == 0.0 && hpMix.targetValue == 0.0)
            sm[smHp].snap();
        if (lpOn && lpMix.value == 0.0 && lpMix.targetValue == 0.0)
            sm[smLp].snap();
        if (! hpOn)
            sm[smHp].value = sm[smHp].targetValue = hpLastOn;
        else
            hpLastOn = sm[smHp].targetValue;
        if (! lpOn)
            sm[smLp].value = sm[smLp].targetValue = lpLastOn;
        else
            lpLastOn = sm[smLp].targetValue;
        hpMix.targetValue = hpOn ? 1.0 : 0.0;
        lpMix.targetValue = lpOn ? 1.0 : 0.0;
        eqMix.targetValue = s.eqBypass ? 0.0 : 1.0;
        dynMix.targetValue = s.dynBypass ? 0.0 : 1.0;
        analogTarget = s.analog ? 1.0 : 0.0;
        polarityTarget = s.polarity ? -1.0 : 1.0;
        trimTarget = dbToGain (t[TaperId::inputTrim].valueAt (s.inputTrim));
        outTarget = dbToGain (t[TaperId::outputGain].valueAt (s.outputGain));
    }

    void snapToTargets() noexcept
    {
        hpMix = lpMix = eqMix = dynMix = Ramp {};
        setTargets();
        for (auto& v : sm)
            v.snap();
        hpMix.value = hpMix.targetValue;
        lpMix.value = lpMix.targetValue;
        eqMix.value = eqMix.targetValue;
        dynMix.value = dynMix.targetValue;
        analogMix = analogTarget;
        polaritySm = polarityTarget;
        trimSm = trimTarget;
        outSm = outTarget;
        route = routeFor (target);
        duck = 1.0;
        ducking = false;
        noiseAmp = dbToGain (analogNoiseDbfs);
        controlCounter = 0;
    }

    void stepSmoothers() noexcept
    {
        hpMix.step (rampStep);
        lpMix.step (rampStep);
        eqMix.step (rampStep);
        dynMix.step (rampStep);
        auto toward = [this] (double& v, double t) { v = t + smoothCoeff * (v - t); if (std::abs (v - t) < 1e-9) v = t; };
        toward (trimSm, trimTarget);
        toward (outSm, outTarget);
        analogMix = analogTarget > analogMix ? std::min (analogTarget, analogMix + rampStep)
                                             : std::max (analogTarget, analogMix - rampStep);
        polaritySm = polarityTarget > polaritySm ? std::min (polarityTarget, polaritySm + 2.0 * rampStep)
                                                 : std::max (polarityTarget, polaritySm - 2.0 * rampStep);
        // routing change: duck to zero, switch, come back
        const Route wanted = routeFor (target);
        if (wanted != route)
        {
            duck = std::max (0.0, duck - duckStep);
            if (duck <= 0.0)
                route = wanted;
        }
        else if (duck < 1.0)
            duck = std::min (1.0, duck + duckStep);
    }

    void updateControl() noexcept
    {
        setTargets();
        bool changed = false;
        for (int k = 0; k < smCount; ++k)
        {
            // advance by controlInterval samples at once
            const double c = std::pow (smoothCoeff, controlInterval);
            changed |= sm[k].step (c);
        }
        designAll (false);
        (void) changed;
    }

    void designAll (bool force) noexcept
    {
        const auto& t = Tapers::get();
        auto differs = [&] (int k) { return force || sm[k].value != designed[k]; };
        auto mark = [&] (int k) { designed[k] = sm[k].value; };

        if (differs (smHp))
        {
            const double disp = t[TaperId::hpFreq].valueAt (sm[smHp].value);
            const auto proto = HighPassTable::get().prototype (std::isnan (disp) ? 16.0 : disp);
            BiquadCoeffs second, first;
            proto.digitise (fs, second, first);
            for (auto& ch : chans) { ch.hp2.c = second; ch.hp1.c = first; }
            mark (smHp);
        }
        if (differs (smLp))
        {
            const auto c = lowPass.coeffs (std::max (sm[smLp].value, 0.025));
            for (auto& ch : chans) ch.lp.c = c;
            mark (smLp);
        }
        if (differs (smLfG) || differs (smLfF))
        {
            const auto c = ShelfTable::lf()
                               .prototype (t[TaperId::lfGain].valueAt (sm[smLfG].value), t[TaperId::lfFreq].valueAt (sm[smLfF].value))
                               .digitise (fs);
            for (auto& ch : chans) ch.lf.c = c;
            mark (smLfG); mark (smLfF);
        }
        if (differs (smHfG) || differs (smHfF))
        {
            const auto c = ShelfTable::hf()
                               .prototype (t[TaperId::hfGain].valueAt (sm[smHfG].value), t[TaperId::hfFreq].valueAt (sm[smHfF].value))
                               .digitise (fs);
            for (auto& ch : chans) ch.hf.c = c;
            mark (smHfG); mark (smHfF);
        }
        if (differs (smLmfG) || differs (smLmfF) || differs (smLmfQ) || target.lmfDiv3 != designedDiv3)
        {
            const auto p = BellTable::lmf().params (t[TaperId::lmfGain].valueAt (sm[smLmfG].value), sm[smLmfQ].value,
                                                    t[TaperId::lmfFreq].valueAt (sm[smLmfF].value), target.lmfDiv3);
            const auto c = p.bilinear (fs);
            for (auto& ch : chans) ch.lmf.c = c;
            mark (smLmfG); mark (smLmfF); mark (smLmfQ);
            designedDiv3 = target.lmfDiv3;
        }
        if (differs (smHmfG) || differs (smHmfF) || differs (smHmfQ) || target.hmfX3 != designedX3)
        {
            auto p = BellTable::hmf().params (t[TaperId::hmfGain].valueAt (sm[smHmfG].value), sm[smHmfQ].value,
                                              t[TaperId::hmfFreq].valueAt (sm[smHmfF].value));
            if (target.hmfX3)
                p.f0 *= 3.0;
            const auto c = p.matched (fs);
            for (auto& ch : chans) ch.hmf.c = c;
            mark (smHmfG); mark (smHmfF); mark (smHmfQ);
            designedX3 = target.hmfX3;
        }

        DynamicsSettings d;
        d.compThreshold = t[TaperId::compThreshold].valueAt (sm[smThr].value);
        d.compRatio = t[TaperId::compRatio].valueAt (sm[smRatio].value);
        d.compFast = target.compFast;
        d.compRelease = t[TaperId::compRelease].valueAt (target.compRelease);
        d.expThreshold = t[TaperId::expThreshold].valueAt (target.expThreshold);
        d.expRange = t[TaperId::expRange].valueAt (target.expRange);
        d.gate = target.expGate;
        d.expFast = target.expFast;
        d.expRelease = t[TaperId::expRelease].valueAt (target.expRelease);
        dynamics.setSettings (d, force);
    }

    void sanitise (int numChannels) noexcept
    {
        for (int c = 0; c < numChannels; ++c)
        {
            auto& ch = chans[size_t (c)];
            for (Biquad* b : { &ch.hp2, &ch.hp1, &ch.lp, &ch.lf, &ch.lmf, &ch.hmf, &ch.hf })
                b->sanitise();
        }
        dynamics.sanitise();
    }

    double fs = 48000.0;
    Settings target = Settings::defaults();
    std::array<Channel, maxChannels> chans {};
    LowPassDesigner lowPass;
    DynamicsSection dynamics;

    Smoothed sm[smCount];
    double designed[smCount] {};
    bool designedDiv3 = false, designedX3 = false;
    double hpLastOn = 0.025, lpLastOn = 0.025;
    Ramp hpMix, lpMix, eqMix, dynMix;
    double analogMix = 0.0, analogTarget = 0.0, polaritySm = 1.0, polarityTarget = 1.0;
    double trimSm = 1.0, trimTarget = 1.0, outSm = 1.0, outTarget = 1.0;
    double smoothCoeff = 0.0, rampStep = 0.0, duckStep = 0.0, noiseAmp = 0.0;
    double duck = 1.0;
    bool ducking = false;
    Route route = Route::normal;
    int controlCounter = 0;

    double blockInPeak = 0.0, blockOutPeak = 0.0, blockMinGrDb = 0.0;
};

} // namespace craft::consoleg::dsp
