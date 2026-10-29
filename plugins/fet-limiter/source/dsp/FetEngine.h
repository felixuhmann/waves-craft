// Craft FET Limiter - complete processing chain (spec r1 sections 1, 3-9). Host independent.
//
//  x -> Input gain -> [gain cell GR(t), linked detector] -> saturation -> response filters -> Output gain
//    -> (+ Analog noise / hum) = wet;   y = Trim (Mix wet + (1 - Mix) x)
//
// Zero latency. Stereo: one detector fed by max(|L|, |R|) (spec 6.4), one gain for both channels.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>

#include "Colouration.h"
#include "Dynamics.h"
#include "DynamicsMap.h"
#include "Filters.h"
#include "GainLaws.h"
#include "Noise.h"
#include "Parameters.h"

namespace craft::fet_limiter::dsp
{

// One-pole parameter smoother that lands exactly on its target (spec D3: click-free, <= 20 ms).
class Smoother
{
public:
    void prepare (double fs, double tauMs) noexcept { coeff = onePoleCoeff (tauMs, fs); }
    void setTarget (double t) noexcept { target = t; }
    void snap() noexcept { value = target; }
    double next() noexcept
    {
        if (value != target)
        {
            value += (target - value) * coeff;
            if (std::abs (target - value) <= 1.0e-9 * std::max (1.0, std::abs (target)))
                value = target;
        }
        return value;
    }
    double get() const noexcept { return value; }
    bool isSettled() const noexcept { return value == target; }

private:
    double coeff = 1.0, value = 0.0, target = 0.0;
};

class FetEngine
{
public:
    static constexpr int maxChannels = 2;
    static constexpr double smoothingMs = 3.0; // 99 % after ~14 ms

    FetEngine()
    {
        for (int rev = 0; rev < 2; ++rev)
            for (int r = 0; r < 5; ++r)
                laws[size_t (rev)][size_t (r)].set (rev == 0 ? Revision::A : Revision::B, Ratio (r));
    }

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        detector.prepare (fs);
        cell.prepare (fs);
        noise.prepare (fs);
        for (auto* s : { &inGain, &outGain, &trim, &mix, &compOn, &makeup })
            s->prepare (fs, smoothingMs);
        appliedRevision = settings.revision;
        for (auto& f : filters)
            f.prepare (settings.revision, fs);
        prepared = true;
        applySettings (true);
        reset();
    }

    void reset()
    {
        detector.reset();
        cell.reset();
        noise.reset();
        for (auto& f : filters)
            f.reset();
        for (auto* s : { &inGain, &outGain, &trim, &mix, &compOn, &makeup })
            s->snap();
        lastEnv = -1.0;
        target = 0.0;
        grDb = 0.0;
    }

    void setSettings (const Settings& s)
    {
        settings = s;
        if (prepared)
            applySettings (false);
    }

    const Settings& getSettings() const noexcept { return settings; }

    // In-place processing of numChannels (1 or 2) channels.
    template <typename Sample>
    void process (Sample* const* channels, int numChannels, int numSamples) noexcept
    {
        numChannels = std::clamp (numChannels, 1, maxChannels);
        const SaturatorParams& sat = saturatorParams (appliedRevision);
        const StaticLaw& law = laws[size_t (revIndex (settings.revision))][size_t (settings.ratio)];
        const bool analogOn = noise.isActive();

        for (int i = 0; i < numSamples; ++i)
        {
            const double gIn = inGain.next(), gOut = outGain.next(), tr = trim.next(), mx = mix.next();
            const double on = compOn.next(), mk = makeup.next();

            double u[maxChannels] {}, dry[maxChannels] {};
            double peak = 0.0;
            for (int c = 0; c < numChannels; ++c)
            {
                dry[c] = double (channels[c][i]);
                u[c] = dry[c] * gIn;
                peak = std::max (peak, std::abs (u[c]));
                inPeak = std::max (inPeak, std::abs (dry[c]));
            }

            // linked detector + static law + gain-cell dynamics
            const double env = detector.process (peak);
            if (env != lastEnv)
            {
                lastEnv = env;
                target = env > 1.0e-12 ? law.reduction (20.0 * std::log10 (env)) : 0.0;
            }
            const double g = cell.process (target);
            const double applied = g * on;
            grDb = applied;
            maxGrDb = std::max (maxGrDb, applied);
            const double cellGain = std::exp (-applied * (std::log (10.0) / 20.0)) * mk;

            if (analogOn)
                noise.advanceFrame();
            for (int c = 0; c < numChannels; ++c)
            {
                double w = saturate (sat, u[c] * cellGain);
                w = filters[size_t (c)].process (w) * gOut;
                if (analogOn)
                    w += noise.sample (c);
                const double y = tr * (mx * w + (1.0 - mx) * dry[c]);
                outPeak = std::max (outPeak, std::abs (y));
                channels[c][i] = Sample (y);
            }
        }
    }

    // Meters (reset by the caller)
    void resetMeters() noexcept
    {
        inPeak = outPeak = 0.0;
        maxGrDb = 0.0;
    }
    double getInputPeak() const noexcept { return inPeak; }
    double getOutputPeak() const noexcept { return outPeak; }
    double getMaxGainReductionDb() const noexcept { return maxGrDb; }
    double getGainReductionDb() const noexcept { return grDb; }

    int getLatencySamples() const noexcept { return 0; }

private:
    void applySettings (bool snapAll)
    {
        const Revision rev = settings.revision;
        inGain.setTarget (inputGain (rev, settings.inputDisplay));
        outGain.setTarget (outputGain (rev, settings.outputDisplay));
        trim.setTarget (dbToGain (settings.trimDb));
        mix.setTarget (settings.mixPercent / 100.0);
        compOn.setTarget (settings.compOff ? 0.0 : 1.0);
        makeup.setTarget (settings.ratio == Ratio::All && ! settings.compOff ? dbToGain (allMakeupDb (rev)) : 1.0);
        cell.setParams (dynamicsFor (rev, settings.ratio, settings.attack, settings.release));
        noise.setMode (rev, settings.analog);
        if (rev != appliedRevision)
        {
            appliedRevision = rev;
            for (auto& f : filters)
                f.setRevision (rev, fs);
        }
        if (snapAll)
            for (auto* s : { &inGain, &outGain, &trim, &mix, &compOn, &makeup })
                s->snap();
    }

    Settings settings;
    Revision appliedRevision = Revision::A;
    double fs = 48000.0;
    bool prepared = false;

    std::array<std::array<StaticLaw, 5>, 2> laws;
    PeakDetector detector;
    GainCell cell;
    NoiseSource noise;
    std::array<ResponseFilter, maxChannels> filters;
    Smoother inGain, outGain, trim, mix, compOn, makeup;

    double lastEnv = -1.0, target = 0.0, grDb = 0.0;
    double inPeak = 0.0, outPeak = 0.0, maxGrDb = 0.0;
};

} // namespace craft::fet_limiter::dsp
