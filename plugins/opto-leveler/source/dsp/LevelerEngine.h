// Craft Opto Leveler - complete processing chain (spec 3). Host independent, zero latency.
//
//  in -+------------------------------------------------------------------------------------ dry -+
//      +-> x g(t) -> make-up (T2) -> colour (T7) -> response (T8) -> Mix -> Trim -> + analog noise -> out
//                ^
//  sidechain: in -> Hi Freq weighting -> quadrature envelope -> averaging -> max over channels -> static curve (T1)
//             -> optical cell (4.3) -> g ; rectifier ripple -> waveform-following modulation of g
#pragma once

#include <algorithm>
#include <array>
#include <cmath>

#include "Detector.h"
#include "Laws.h"
#include "OptoCell.h"
#include "OutputStage.h"

namespace craft::opto_leveler::dsp
{

struct Settings
{
    double gain = 40.0;          // display units 0 ... 100 (T2)
    double peakReduction = 46.0; // display units 0 ... 100 (T1)
    Mode mode = Mode::Compress;
    double hiFreq = 50.0;        // 0 ... 100, 100 = flat
    AnalogNoise::Type analog = AnalogNoise::Type::Off;
    double mixPercent = 100.0;
    double trimDb = 0.0;
};

struct EngineTuning
{
    DetectorTuning detector;
    double rippleDepth = 0.026;     // waveform-following depth ...
    double rippleExponent = 0.737;  // ... times slope * (1/g - 1)^exponent
    double rippleDelay = 16.3e-6;   // s, delay of the modulation relative to the audio
};

class LevelerEngine
{
public:
    static constexpr int maxChannels = 2;
    static constexpr double smoothingSeconds = 0.02; // D4
    static constexpr double floorLevel = 1.0e-7;     // -140 dBFS: detector floor

    explicit LevelerEngine (EngineTuning t = defaultTuning()) : tuning (t) {}

    static EngineTuning defaultTuning()
    {
        EngineTuning t;
        return t;
    }

    void setSettings (const Settings& s)
    {
        settings = s;
        if (! prepared)
            return;
        prRamp.setTarget (std::clamp (s.peakReduction, 0.0, 100.0));
        modeRamp.setTarget (s.mode == Mode::Limit ? 1.0 : 0.0);
        makeupRamp.setTarget (makeupDbFor (s.gain));
        hiFreqRamp.setTarget (std::clamp (s.hiFreq, 0.0, 100.0));
        mixRamp.setTarget (std::clamp (s.mixPercent, 0.0, 100.0) / 100.0);
        trimRamp.setTarget (std::pow (10.0, std::clamp (s.trimDb, -18.0, 18.0) / 20.0));
        noise.setType (s.analog);
    }
    const Settings& getSettings() const noexcept { return settings; }

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        for (auto& sc : sidechain)
            sc.prepare (fs, tuning.detector);
        cell.prepare (fs);
        for (auto& c : colour)
            c.prepare (fs);
        for (auto& r : response)
            r.prepare (fs);
        noise.prepare (fs);
        // T2 is the path gain at 1 kHz small signal; the response filter contributes its own 1 kHz gain
        response1kDb = 20.0 * std::log10 (response[0].magnitude (1000.0, fs) * colour[0].magnitude (1000.0, fs));
        for (auto* r : { &prRamp, &modeRamp, &makeupRamp, &hiFreqRamp, &mixRamp, &trimRamp })
            r->prepare (fs, smoothingSeconds);
        const double d = std::clamp (tuning.rippleDelay * fs, 0.0, double (ripMask - 1));
        ripDelayInt = unsigned (d);
        ripDelayFrac = d - double (ripDelayInt);
        prepared = true;
        reset();
    }

    // Clears all state; parameters jump to their targets.
    void reset()
    {
        prRamp.setImmediate (std::clamp (settings.peakReduction, 0.0, 100.0));
        modeRamp.setImmediate (settings.mode == Mode::Limit ? 1.0 : 0.0);
        makeupRamp.setImmediate (makeupDbFor (settings.gain));
        hiFreqRamp.setImmediate (std::clamp (settings.hiFreq, 0.0, 100.0));
        mixRamp.setImmediate (std::clamp (settings.mixPercent, 0.0, 100.0) / 100.0);
        trimRamp.setImmediate (std::pow (10.0, std::clamp (settings.trimDb, -18.0, 18.0) / 20.0));
        noise.setType (settings.analog, true);
        noise.reset();
        for (auto& sc : sidechain)
        {
            sc.setHiFreq (hiFreqRamp.current());
            sc.reset();
        }
        cell.reset();
        for (auto& c : colour)
            c.reset();
        for (auto& r : response)
            r.reset();
        updateCurve();
        makeupLin = std::pow (10.0, makeupRamp.current() / 20.0);
        lastGain = 1.0;
        lastGrDb = 0.0;
        ripLine.fill (0.0);
        ripPos = 0;
        resetMeters();
    }

    double makeupDbFor (double gainDisplay) const { return gainLawDb (gainDisplay) - response1kDb; }

    template <typename Sample>
    void process (Sample* const* channels, int numChannels, int numSamples) noexcept
    {
        const int nch = std::clamp (numChannels, 1, maxChannels);
        for (int i = 0; i < numSamples; ++i)
        {
            // ---- parameter smoothing (D4)
            if (prRamp.isRamping() || modeRamp.isRamping())
            {
                prRamp.next();
                modeRamp.next();
                updateCurve();
            }
            if (makeupRamp.isRamping())
                makeupLin = std::pow (10.0, makeupRamp.next() / 20.0);
            if (hiFreqRamp.isRamping())
            {
                const double h = hiFreqRamp.next();
                if ((++hiFreqCounter & 15) == 0 || ! hiFreqRamp.isRamping())
                    for (auto& sc : sidechain)
                        sc.setHiFreq (h);
            }
            const double mix = mixRamp.next();
            const double trim = trimRamp.next();

            // ---- sidechain (feed-forward from the input, linked: max of the channel levels, D6)
            double level = 0.0, rip = 0.0;
            for (int c = 0; c < nch; ++c)
            {
                const double x = double (channels[c][i]);
                auto& sc = sidechain[size_t (c)];
                sc.process (x);
                if (sc.level() >= level)
                {
                    level = sc.level();
                    rip = sc.rippleValue();
                }
                inAbsSum[size_t (c)] += std::abs (x);
            }
            const double levelDb = 20.0 * std::log10 (std::max (level, floorLevel));
            double slope = 0.0;
            const double grDb = curveGr (levelDb, slope);
            const double target = 1.0 - std::pow (10.0, -grDb / 20.0);
            double g = cell.process (target);

            // waveform-following part (spec 6): the gain follows the rectified waveform in proportion to the
            // local slope of the static curve and the depth of the cell.
            ripLine[ripPos] = rip;
            const double ripDelayed = ripLine[(ripPos - ripDelayInt) & ripMask] * (1.0 - ripDelayFrac)
                                      + ripLine[(ripPos - ripDelayInt - 1) & ripMask] * ripDelayFrac;
            ripPos = (ripPos + 1) & ripMask;
            if (tuning.rippleDepth > 0.0 && slope > 0.0 && g < 1.0)
            {
                const double depth = tuning.rippleDepth * slope
                                     * std::pow (1.0 / std::max (g, 1.0e-3) - 1.0, tuning.rippleExponent);
                g *= std::exp (-std::clamp (depth * ripDelayed, -1.0, 1.0));
            }
            lastGain = g;
            lastGrDb = grDb;
            minGain = std::min (minGain, g);

            // ---- audio path
            for (int c = 0; c < nch; ++c)
            {
                const double x = double (channels[c][i]);
                auto& r = response[size_t (c)];
                double wet = colour[size_t (c)].process (x * g * makeupLin);
                wet = r.process (wet);
                double y = (mix * wet + (1.0 - mix) * x) * trim;
                outAbsSum[size_t (c)] += std::abs (y);
                if (noise.getType() != AnalogNoise::Type::Off)
                    y += (c == 0 ? (humSample = noise.nextHum()) : humSample) + noise.nextNoise (c);
                channels[c][i] = Sample (y);
            }
            ++meterSamples;
        }
        for (auto& sc : sidechain)
            sc.flushDenormals();
        for (auto& c : colour)
            c.flushDenormals();
        for (auto& r : response)
            r.flushDenormals();
    }

    // ---- state / metering
    double getCellGain() const noexcept { return lastGain; }           // last applied cell gain (linear)
    double getStaticGrDb() const noexcept { return lastGrDb; }         // last static-curve target (dB)
    double getDetectorLevel (int c) const noexcept { return sidechain[size_t (c)].level(); }
    const OptoCell& getCell() const noexcept { return cell; }
    double getMinGain() const noexcept { return minGain; }
    double getInputAverage (int c) const noexcept { return meterSamples > 0 ? inAbsSum[size_t (c)] / meterSamples : 0.0; }
    double getOutputAverage (int c) const noexcept { return meterSamples > 0 ? outAbsSum[size_t (c)] / meterSamples : 0.0; }
    void resetMeters() noexcept
    {
        minGain = 1.0;
        inAbsSum.fill (0.0);
        outAbsSum.fill (0.0);
        meterSamples = 0;
    }
    double getSampleRate() const noexcept { return fs; }
    static constexpr int getLatencySamples() noexcept { return 0; }

private:
    // Static curve for the current (smoothed) Peak Reduction and mode, resampled on the 4 dB level grid.
    void updateCurve()
    {
        const auto& c = staticCurve (Mode::Compress);
        const auto& l = staticCurve (Mode::Limit);
        const double pr = prRamp.current(), m = modeRamp.current();
        curveMin = c.minLevel();
        curveStep = c.levelAt (1) - c.levelAt (0);
        for (int k = 0; k < numCurvePoints; ++k)
        {
            const double lev = curveMin + curveStep * k;
            const double a = c.gainReductionDb (pr, lev);
            const double b = m > 0.0 ? l.gainReductionDb (pr, lev) : 0.0;
            curve[size_t (k)] = a + m * (b - a);
        }
        curveTopSlope = std::max (0.0, (curve[numTablePoints - 1] - curve[numTablePoints - 2]) / curveStep);
    }

    double curveGr (double levelDb, double& slope) const noexcept
    {
        const double pos = (levelDb - curveMin) / curveStep;
        if (pos <= 0.0)
        {
            slope = 0.0;
            return curve[0];
        }
        const double top = double (numTablePoints - 1);
        if (pos >= top)
        {
            const double over = std::min (levelDb - (curveMin + curveStep * top), StaticCurve::maxLevelDb);
            slope = levelDb - (curveMin + curveStep * top) < StaticCurve::maxLevelDb ? curveTopSlope : 0.0;
            return std::min (StaticCurve::maxGrDb, curve[numTablePoints - 1] + curveTopSlope * over);
        }
        const int k = int (pos);
        const double t = pos - double (k);
        slope = (curve[size_t (k + 1)] - curve[size_t (k)]) / curveStep;
        return curve[size_t (k)] + t * (curve[size_t (k + 1)] - curve[size_t (k)]);
    }

    static constexpr int numTablePoints = 16, numCurvePoints = 16;

    EngineTuning tuning;
    Settings settings;
    double fs = 48000.0;
    bool prepared = false;
    std::array<SidechainChannel, maxChannels> sidechain;
    OptoCell cell;
    std::array<Response, maxChannels> response;
    std::array<ColourStage, maxChannels> colour;
    AnalogNoise noise;
    Ramp prRamp, modeRamp, makeupRamp, hiFreqRamp, mixRamp, trimRamp;
    unsigned hiFreqCounter = 0;
    std::array<double, numCurvePoints> curve {};
    double curveMin = -60.0, curveStep = 4.0, curveTopSlope = 0.0;
    double makeupLin = 1.0, response1kDb = 0.0, humSample = 0.0;
    double lastGain = 1.0, lastGrDb = 0.0, minGain = 1.0;
    std::array<double, maxChannels> inAbsSum {}, outAbsSum {};
    double meterSamples = 0.0;
    static constexpr unsigned ripMask = 15;
    std::array<double, ripMask + 1> ripLine {};
    unsigned ripPos = 0, ripDelayInt = 0;
    double ripDelayFrac = 0.0;
};

} // namespace craft::opto_leveler::dsp
