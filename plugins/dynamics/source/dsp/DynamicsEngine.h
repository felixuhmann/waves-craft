// Craft Dynamics - host-independent engine shared by the four products (spec r1 sections 1-8).
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

#include "Filters.h"
#include "Parameters.h"
#include "Smoothing.h"
#include "StaticCurves.h"

namespace craft::dynamics::dsp
{

// spec 1: one engine, four variants
enum class Variant
{
    Full = 0,        // compressor + gate + filter module + key/monitor/lookahead/input gains
    Compressor = 1,  // compressor only, wideband
    Gate = 2,        // gate/expander only, wideband
    CompressorFilter = 3 // compressor + filter module + key/monitor/lookahead/input gains (no gate)
};

struct VariantInfo
{
    bool comp, gate, filter; // filter module, key mode, monitor, lookahead and input gains come together
};

constexpr VariantInfo variantInfo (Variant v) noexcept
{
    switch (v)
    {
        case Variant::Full:             return { true, true, true };
        case Variant::Compressor:       return { true, false, false };
        case Variant::Gate:             return { false, true, false };
        case Variant::CompressorFilter: return { true, false, true };
    }
    return { true, true, true };
}

enum class EqMode { Wideband = 0, Sidechain = 1, Split = 2 };
enum class Monitor { Audio = 0, Sidechain = 1, Passive = 2 };
enum class KeyMode { Stereo = 0, LeftToRight = 1, RightToLeft = 2 };

// Physical settings (spec 2). Gate thresholds use -inf for the "-inf" setting.
struct Settings
{
    // compressor / expander
    double compThresholdDb = 0.0;
    double ratioPos = ratio::neutralPos;
    double compAttackMs = 2.0, compReleaseMs = 50.0, pdrMs = 0.0, makeupDb = 0.0;
    bool peakRef = false, compBypass = false;
    EqMode compEq = EqMode::Wideband;

    // gate / expander
    bool expander = false;
    double gateOpenDb = minusInfinityDb, gateCloseDb = minusInfinityDb;
    double gateAttackMs = 2.0, gateReleaseMs = 30.0, holdMs = 0.01;
    double floorValue = floorvalue::offValue; // physical floor parameter (see floorvalue)
    bool gateBypass = false;
    EqMode gateEq = EqMode::Wideband;

    // filter module
    FilterType filterType = FilterType::LowPass;
    double filterFreqHz = 7179.0, filterQ = 0.6;

    // global
    Monitor monitor = Monitor::Audio;
    KeyMode key = KeyMode::Stereo;
    bool lookahead = false;
    double inputGainLDb = 0.0, inputGainRDb = 0.0, outputGainDb = 0.0;
};

class DynamicsEngine
{
public:
    static constexpr int lookaheadSamples = 340;       // spec 3.5 / D6: fixed at every sample rate
    // spec r2 4.3 constants (normative)
    static constexpr double rectTauMs = 0.127;         // RECT_TAU
    static constexpr double kCalDb = 3.32;             // K_CAL
    static constexpr double detRel = 0.900;            // DET_REL
    static constexpr double pdrFastReleaseMs = 41.5;   // PDR_FAST_REL
    static constexpr double slowRel = 2.907;           // SLOW_REL
    static constexpr double slowWeight = 0.093;        // SLOW_W
    static constexpr double expDetRel = 0.166;         // EXP_DET_REL
    static constexpr double expGainAtt = 0.384;        // EXP_GAIN_ATT
    static constexpr double expGainRel = 0.842;        // EXP_GAIN_REL
    static constexpr double gateDetectorMaxAttackMs = 2.0;
    static constexpr double paramSmoothingMs = 4.0;    // one-pole time constant, settles well within 20 ms
    static constexpr double crossfadeMs = 10.0;        // switched settings (spec 8: <= 20 ms)

    explicit DynamicsEngine (Variant v = Variant::Full) : variant (v), info (variantInfo (v)) {}

    Variant getVariant() const noexcept { return variant; }

    void prepare (double sampleRate)
    {
        fs = sampleRate > 0.0 ? sampleRate : 48000.0;
        for (auto* s : { &smThreshold, &smRatio, &smMakeup, &smPeakRef, &smInL, &smInR, &smOut, &smFloor, &smLogFreq, &smQ })
            s->setTimeConstant (paramSmoothingMs * 0.001, fs);
        smThreshold.setEpsilon (1.0e-6);
        smRatio.setEpsilon (1.0e-7);
        smMakeup.setEpsilon (1.0e-6);
        smPeakRef.setEpsilon (1.0e-7);
        smLogFreq.setEpsilon (1.0e-6);
        smQ.setEpsilon (1.0e-7);
        fade.setRampTime (crossfadeMs * 0.001, fs);
        rectCoeff = std::exp (-1.0 / (rectTauMs * 0.001 * fs));
        calFs = -1.0; // recompute the expander calibration at the new rate
        updateBlockConstants();
        reset();
    }

    // Clears all state and jumps to the current settings without smoothing or crossfades.
    void reset()
    {
        smThreshold.snapToTarget();
        smRatio.snapToTarget();
        smMakeup.snapToTarget();
        smPeakRef.snapToTarget();
        smInL.snapToTarget();
        smInR.snapToTarget();
        smOut.snapToTarget();
        smFloor.snapToTarget();
        smLogFreq.snapToTarget();
        smQ.snapToTarget();
        cores[0] = Core {};
        cores[0].cfg = discreteFromSettings();
        cores[1] = cores[0];
        active = 0;
        fading = false;
        fade.setTarget (0.0);
        fade.snapToTarget();
        updateDesigns();
    }

    void setSettings (const Settings& in)
    {
        settings = in;
        auto& s = settings;
        if (! info.comp)
        {
            s.compBypass = true;
            s.compEq = EqMode::Wideband;
        }
        if (! info.gate)
        {
            s.gateBypass = true;
            s.gateEq = EqMode::Wideband;
        }
        if (! info.filter)
        {
            s.compEq = s.gateEq = EqMode::Wideband;
            s.monitor = Monitor::Audio;
            s.key = KeyMode::Stereo;
            s.lookahead = false;
            s.inputGainLDb = s.inputGainRDb = 0.0;
        }
        smThreshold.setTarget (s.compThresholdDb);
        smRatio.setTarget (clamp01 (s.ratioPos));
        smMakeup.setTarget (s.makeupDb);
        smPeakRef.setTarget (s.peakRef ? 1.0 : 0.0);
        smInL.setTarget (dbToGain (s.inputGainLDb));
        smInR.setTarget (dbToGain (s.inputGainRDb));
        smOut.setTarget (dbToGain (s.outputGainDb));
        smFloor.setTarget (floorvalue::linear (s.floorValue, s.expander));
        smLogFreq.setTarget (std::log2 (clampTo (s.filterFreqHz, 1.0, 1.0e6)));
        smQ.setTarget (clampTo (s.filterQ, 0.05, 2.0));
        updateBlockConstants();
    }

    const Settings& getSettings() const noexcept { return settings; }

    // Latency reported to the host for the current settings (spec 3.5).
    int getLatencySamples() const noexcept { return settings.lookahead && info.filter ? lookaheadSamples : 0; }

    // Process in place; `right` may be null for a single channel (mono variants run the same engine, D7).
    template <typename Sample>
    void process (Sample* left, Sample* right, int numSamples)
    {
        const int nch = right != nullptr ? 2 : 1;
        const Discrete wanted = discreteFromSettings();
        for (int i = 0; i < numSamples; ++i)
        {
            if (! fading && ! (cores[size_t (active)].cfg == wanted))
                startCrossfade (wanted);

            Frame fr;
            fr.threshold = smThreshold.next();
            fr.ratio = smRatio.next();
            fr.makeup = smMakeup.next();
            fr.peakRefAmount = smPeakRef.next();
            fr.floorLinear = smFloor.next();
            const double gl = smInL.next(), gr = smInR.next();
            fr.outGain = smOut.next();
            smLogFreq.next();
            smQ.next();
            if (info.filter && (--designCountdown <= 0))
            {
                designCountdown = 16;
                if (! (designFreq == smLogFreq.getCurrent() && designQ == smQ.getCurrent()))
                    updateDesigns();
            }

            double x[2] = { double (left[i]) * gl, nch > 1 ? double (right[i]) * gr : 0.0 };
            double y[2];
            cores[size_t (active)].tick (x, y, nch, fr, *this);
            if (fading)
            {
                double y2[2];
                cores[size_t (1 - active)].tick (x, y2, nch, fr, *this);
                const double w = fade.next();
                for (int c = 0; c < nch; ++c)
                    y[c] = y[c] + w * (y2[c] - y[c]);
                if (fade.isSettled())
                {
                    active = 1 - active;
                    fading = false;
                }
            }
            left[i] = Sample (y[0]);
            if (nch > 1)
                right[i] = Sample (y[1]);
        }
    }

    struct Meters
    {
        double compLevelDb = -200.0; // compressor detector level e
        double compGainDb = 0.0;     // compressor gain (incl. PeakRef offset and makeup); 0 when bypassed
        double gateLevelDb = -200.0; // gate/expander detector level e
        double gateGainDb = 0.0;     // gate/expander module gain in dB (20 log10 |G|); 0 when bypassed
        bool gateOpen = true;
    };

    Meters getMeters() const noexcept
    {
        const auto& c = cores[size_t (fading ? 1 - active : active)];
        Meters m;
        m.compLevelDb = c.compE;
        m.compGainDb = c.cfg.compOn ? c.compGainDb : 0.0;
        m.gateLevelDb = c.gateE;
        m.gateGainDb = c.cfg.gateOn ? gainToDb (std::abs (c.gateGain)) : 0.0;
        m.gateOpen = c.gateIsOpen;
        return m;
    }

    // Static compressor gain (dB, incl. PeakRef offset and makeup) for a detector level - used by the GUI graph.
    static double staticCompressorGainDb (const Settings& s, double detectorDb) noexcept
    {
        return curves::staticGainDb (s.ratioPos, detectorDb, s.compThresholdDb, s.peakRef, s.makeupDb);
    }

    // Static gate/expander gain (linear, signed) for a detector level - used by the GUI graph.
    static double staticGateGain (const Settings& s, double detectorDb) noexcept
    {
        const double F = floorvalue::linear (s.floorValue, s.expander);
        if (s.gateOpenDb == minusInfinityDb)
            return 1.0;
        double g;
        if (s.expander)
            g = dbToGain (curves::expanderGainDb (detectorDb - s.gateOpenDb));
        else
            g = detectorDb > s.gateOpenDb + curves::gateThresholdOffsetDb ? 1.0 : 0.0;
        return curves::applyFloor (g, F);
    }

private:
    struct Discrete
    {
        bool compOn = false, gateOn = false, expander = false, lookahead = false, monitorFiltered = false;
        EqMode compEq = EqMode::Wideband, gateEq = EqMode::Wideband;
        FilterType type = FilterType::LowPass;
        Monitor monitor = Monitor::Audio;
        KeyMode key = KeyMode::Stereo;

        bool operator== (const Discrete& o) const noexcept
        {
            return compOn == o.compOn && gateOn == o.gateOn && expander == o.expander && lookahead == o.lookahead
                && monitorFiltered == o.monitorFiltered && compEq == o.compEq && gateEq == o.gateEq && type == o.type
                && monitor == o.monitor && key == o.key;
        }
    };

    struct Frame
    {
        double threshold = 0.0, ratio = ratio::neutralPos, makeup = 0.0, peakRefAmount = 0.0, floorLinear = 0.0, outGain = 1.0;
    };

    struct Core
    {
        Discrete cfg;
        FilterDesign design;
        FilterChannel filt[2];

        double compM[2] = { 0.0, 0.0 }, gateM[2] = { 0.0, 0.0 }; // rectifier states
        double compE = -120.0, compESlow = -120.0, compGainDb = 0.0, slowMemory = 0.0;
        double gateE = -120.0, gateG = 1.0, gateGain = 1.0, expV = 0.0;
        bool gateIsOpen = true;
        long holdLeft = 0;

        std::array<double, lookaheadSamples> delayX[2] {}, delaySc[2] {};
        int delayPos = 0;

        void tick (const double* x, double* y, int nch, const Frame& fr, const DynamicsEngine& e) noexcept
        {
            const bool hasFilter = e.info.filter;
            // detection signals (spec 3.2)
            double sc[2] = { x[0], x[1] };
            if (hasFilter)
                for (int c = 0; c < nch; ++c)
                    sc[c] = filt[c].sidechain (design, x[c]);
            const double* dComp = cfg.compEq != EqMode::Wideband ? sc : x;
            const double* dGate = cfg.gateEq != EqMode::Wideband ? sc : x;

            int c0 = 0, c1 = nch - 1; // keyed channels
            if (nch > 1 && cfg.key == KeyMode::LeftToRight)
                c1 = 0;
            else if (nch > 1 && cfg.key == KeyMode::RightToLeft)
                c0 = 1;

            const double a = e.rectCoeff;
            // compressor detector (spec 4)
            if (e.info.comp)
            {
                double m = 0.0;
                for (int c = c0; c <= c1; ++c)
                {
                    compM[c] = a * compM[c] + (1.0 - a) * std::abs (dComp[c]);
                    m = std::max (m, compM[c]);
                }
                const double l = levelDb (m);
                if (e.pdrOn)
                {
                    double k = l > compE ? e.cCompAtt : e.cPdrFastRel;
                    compE = k * compE + (1.0 - k) * l;
                    k = l > compESlow ? e.cPdrSlowAtt : e.cCompRel;
                    compESlow = k * compESlow + (1.0 - k) * l;
                }
                else
                {
                    const double k = l > compE ? e.cCompAtt : e.cCompRel;
                    compE = k * compE + (1.0 - k) * l;
                    compESlow = compE;
                }
                const double ee = std::max (compE, compESlow);
                // spec r2 5: target reduction t, slow memory s, R = (1 - W) t + W s
                const double t = -curves::compressorCurveDb (fr.ratio, ee - fr.threshold);
                if (t > slowMemory)
                    slowMemory = t;
                else
                    slowMemory = e.cSlow * slowMemory + (1.0 - e.cSlow) * t;
                double g = -((1.0 - slowWeight) * t + slowWeight * slowMemory) + fr.makeup;
                if (fr.peakRefAmount > 0.0)
                    g += fr.peakRefAmount * curves::peakRefOffsetDb (fr.ratio, fr.threshold);
                compGainDb = curves::limitFinalGainDb (g, ee); // output limit on the final output (r2 10.9)
            }

            // gate / expander detector and gain (spec 4, 6)
            if (e.info.gate)
            {
                double m = 0.0;
                for (int c = c0; c <= c1; ++c)
                {
                    gateM[c] = a * gateM[c] + (1.0 - a) * std::abs (dGate[c]);
                    m = std::max (m, gateM[c]);
                }
                const double l = levelDb (m);
                const bool enabled = e.gateOpenDb != minusInfinityDb;
                if (cfg.expander)
                {
                    const double k = l > gateE ? e.cGateAtt : e.cExpDetRel;
                    gateE = k * gateE + (1.0 - k) * l;
                    if (enabled)
                    {
                        // spec r2 6: dB-domain smoothing of the expander gain
                        const double u = curves::expanderGainDb (gateE + e.expCalibrationDb - e.gateOpenDb);
                        const double kv = u > expV ? e.cExpGainAtt : e.cExpGainRel;
                        expV = kv * expV + (1.0 - kv) * u;
                        gateG = dbToGain (expV);
                    }
                    else
                    {
                        expV = 0.0;
                        gateG = 1.0;
                    }
                    gateIsOpen = ! enabled || gateE > e.gateOpenDb + curves::gateThresholdOffsetDb;
                    holdLeft = 0;
                }
                else
                {
                    if (l > gateE)
                        gateE = e.cGateDetAtt * gateE + (1.0 - e.cGateDetAtt) * l;
                    else if (holdLeft <= 0)
                        gateE = e.cGateRel * gateE + (1.0 - e.cGateRel) * l;
                    if (! gateIsOpen)
                    {
                        if (! enabled || gateE > e.gateOpenDb + curves::gateThresholdOffsetDb)
                        {
                            gateIsOpen = true;
                            holdLeft = e.holdSamples;
                        }
                    }
                    else if (enabled && holdLeft <= 0 && gateE < e.gateCloseDb + curves::gateThresholdOffsetDb)
                        gateIsOpen = false;
                    if (holdLeft > 0)
                        --holdLeft;
                    if (gateIsOpen)
                    {
                        gateG = 1.0 + e.cGateAtt * (gateG - 1.0);
                        if (1.0 - gateG < 1.0e-12)
                            gateG = 1.0;
                    }
                    else
                        gateG *= e.cGateRel;
                }
                gateGain = curves::applyFloor (gateG, fr.floorLinear);
            }

            // audio path: lookahead delay (spec 3.5); the detectors above saw the undelayed signal
            double xd[2], scd[2];
            for (int c = 0; c < nch; ++c)
            {
                auto& dx = delayX[c];
                auto& ds = delaySc[c];
                const double oldX = dx[size_t (delayPos)], oldSc = ds[size_t (delayPos)];
                dx[size_t (delayPos)] = x[c];
                ds[size_t (delayPos)] = sc[c];
                xd[c] = cfg.lookahead ? oldX : x[c];
                scd[c] = cfg.lookahead ? oldSc : sc[c];
            }
            if (++delayPos == lookaheadSamples)
                delayPos = 0;

            const double gc = cfg.compOn ? dbToGain (compGainDb) : 1.0;
            const double gg = cfg.gateOn ? gateGain : 1.0;
            const bool compSplit = cfg.compOn && cfg.compEq == EqMode::Split;
            const bool gateSplit = cfg.gateOn && cfg.gateEq == EqMode::Split;
            const double gBand = (compSplit ? gc : 1.0) * (gateSplit ? gg : 1.0);
            const double gWide = (compSplit ? 1.0 : gc) * (gateSplit ? 1.0 : gg);

            for (int c = 0; c < nch; ++c)
            {
                double active = 0.0, allpass = xd[c];
                if (hasFilter)
                    active = filt[c].bands (design, xd[c], allpass);
                double out;
                if (cfg.monitor == Monitor::Sidechain)
                    out = cfg.monitorFiltered ? scd[c] : xd[c];
                else if (cfg.monitor == Monitor::Passive)
                    out = 2.0 * (allpass - active);
                else if (compSplit || gateSplit)
                    out = (allpass + (gBand - 1.0) * active) * gWide * fr.outGain;
                else
                    out = xd[c] * gWide * fr.outGain;
                y[c] = out;
            }

            // key modes: only the keyed channel is output (spec 3.6); the sidechain monitor shows the key
            if (nch > 1 && cfg.key == KeyMode::LeftToRight)
            {
                if (cfg.monitor == Monitor::Sidechain)
                    y[1] = y[0];
                y[0] = 0.0;
            }
            else if (nch > 1 && cfg.key == KeyMode::RightToLeft)
            {
                if (cfg.monitor == Monitor::Sidechain)
                    y[0] = y[1];
                y[1] = 0.0;
            }
        }

        // spec r2 4.3: level of the rectified average, calibrated by K_CAL
        static double levelDb (double m) noexcept { return 20.0 * std::log10 (m + 1.0e-15) + kCalDb; }
    };

    Discrete discreteFromSettings() const noexcept
    {
        const auto& s = settings;
        Discrete d;
        d.compOn = info.comp && ! s.compBypass;
        d.gateOn = info.gate && ! s.gateBypass;
        d.expander = s.expander;
        d.lookahead = info.filter && s.lookahead;
        d.compEq = info.filter && info.comp ? s.compEq : EqMode::Wideband;
        d.gateEq = info.filter && info.gate ? s.gateEq : EqMode::Wideband;
        d.type = s.filterType;
        d.monitor = info.filter ? s.monitor : Monitor::Audio;
        d.key = info.filter ? s.key : KeyMode::Stereo;
        d.monitorFiltered = d.compEq != EqMode::Wideband; // r2 3.7: the compressor's EQ mode only
        return d;
    }

    void startCrossfade (const Discrete& wanted)
    {
        const int next = 1 - active;
        cores[size_t (next)] = cores[size_t (active)];
        cores[size_t (next)].cfg = wanted;
        cores[size_t (next)].design = designFor (cores[size_t (next)].cfg.type);
        fade.setTarget (0.0);
        fade.snapToTarget();
        fade.setTarget (1.0);
        fading = true;
    }

    FilterDesign designFor (FilterType t) const noexcept
    {
        return filterdesign::design (t, std::exp2 (smLogFreq.getCurrent()), smQ.getCurrent(), fs);
    }

    void updateDesigns() noexcept
    {
        designFreq = smLogFreq.getCurrent();
        designQ = smQ.getCurrent();
        for (auto& c : cores)
            c.design = designFor (c.cfg.type);
    }

    double coeffMs (double ms) const noexcept { return std::exp (-1.0 / (std::max (ms, 1.0e-4) * 0.001 * fs)); }

public:
    // Mean steady-state level-smoother reading (dB re peak) of a 0 dBFS 1 kHz sine through the rectifier and a
    // level smoother with the given attack/release: the periodic fixed point e(T) = e(0) is found by bisection.
    double steadySineReadingDb (double attMs, double relMs) const noexcept
    {
        const int len = std::min (4096, int (std::lround (fs / 100.0))); // ten periods of 1 kHz
        const double pi2 = 2.0 * 3.14159265358979323846 * 1000.0 / fs;
        std::array<double, 4096> level {};
        double m = 0.0;
        for (int i = 0; i < 4 * len; ++i) // rectifier to steady state, keep the last ten periods
        {
            m = rectCoeff * m + (1.0 - rectCoeff) * std::abs (std::sin (pi2 * double (i)));
            if (i >= 3 * len)
                level[size_t (i - 3 * len)] = Core::levelDb (m);
        }
        const double ca = coeffMs (attMs), cr = coeffMs (relMs);
        auto run = [&] (double e0, double* mean) {
            double e = e0, sum = 0.0;
            for (int i = 0; i < len; ++i)
            {
                const double l = level[size_t (i)];
                const double c = l > e ? ca : cr;
                e = c * e + (1.0 - c) * l;
                sum += e;
            }
            if (mean != nullptr)
                *mean = sum / double (len);
            return e;
        };
        double lo = -40.0, hi = 20.0;
        for (int k = 0; k < 60; ++k)
        {
            const double mid = 0.5 * (lo + hi);
            (run (mid, nullptr) > mid ? lo : hi) = mid;
        }
        double mean = 0.0;
        run (0.5 * (lo + hi), &mean);
        return mean;
    }

private:
    void updateBlockConstants() noexcept
    {
        const auto& s = settings;
        cCompAtt = coeffMs (s.compAttackMs);
        cCompRel = coeffMs (detRel * s.compReleaseMs);
        cSlow = coeffMs (slowRel * s.compReleaseMs);
        cExpDetRel = coeffMs (expDetRel * s.gateReleaseMs);
        if (s.gateAttackMs != calAtt || s.gateReleaseMs != calRel || fs != calFs)
        {
            // keep Appendix B's static coordinates with the fast expander release (IMPLEMENTATION_NOTES.md)
            calAtt = s.gateAttackMs, calRel = s.gateReleaseMs, calFs = fs;
            expCalibrationDb = steadySineReadingDb (s.gateAttackMs, s.gateReleaseMs)
                             - steadySineReadingDb (s.gateAttackMs, expDetRel * s.gateReleaseMs);
        }
        cExpGainAtt = coeffMs (expGainAtt * s.gateAttackMs);
        cExpGainRel = coeffMs (expGainRel * s.gateReleaseMs);
        pdrOn = s.pdrMs > 0.0;
        cPdrFastRel = coeffMs (pdrFastReleaseMs);
        cPdrSlowAtt = coeffMs (0.5 * std::max (s.pdrMs, 1.0e-3));
        cGateAtt = coeffMs (s.gateAttackMs);
        cGateRel = coeffMs (s.gateReleaseMs);
        cGateDetAtt = coeffMs (std::min (s.gateAttackMs, gateDetectorMaxAttackMs));
        holdSamples = long (std::llround (s.holdMs * 0.001 * fs));
        gateOpenDb = s.gateOpenDb;
        gateCloseDb = (s.gateCloseDb == minusInfinityDb) ? s.gateOpenDb : std::min (s.gateCloseDb, s.gateOpenDb);
    }

    const Variant variant;
    const VariantInfo info;
    Settings settings;
    double fs = 48000.0;

    OnePoleSmoother smThreshold, smRatio, smMakeup, smPeakRef, smInL, smInR, smOut, smFloor, smLogFreq, smQ;
    double designFreq = 0.0, designQ = 0.0;
    int designCountdown = 0;

    double rectCoeff = 0.0, cSlow = 0.0, cExpDetRel = 0.0, cExpGainAtt = 0.0, cExpGainRel = 0.0;
    double expCalibrationDb = 0.0, calAtt = -1.0, calRel = -1.0, calFs = -1.0;
    double cCompAtt = 0.0, cCompRel = 0.0, cPdrFastRel = 0.0, cPdrSlowAtt = 0.0;
    double cGateAtt = 0.0, cGateRel = 0.0, cGateDetAtt = 0.0;
    bool pdrOn = false;
    long holdSamples = 0;
    double gateOpenDb = minusInfinityDb, gateCloseDb = minusInfinityDb;

    std::array<Core, 2> cores {};
    int active = 0;
    bool fading = false;
    LinearRamp fade;
};

} // namespace craft::dynamics::dsp
