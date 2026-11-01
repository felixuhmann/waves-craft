// Craft Enhancer - complete host-independent processing (spec r1 section 2).
//
//   x_in    = x * inputGain
//   bands   = Crossover(x_in)                                       (five phase-aligned bands, sum = all-pass)
//   direct  = sum_b W_b( band_b ) * directGain
//   process = sum_b W_b( bandGain_b * g_b(t) * (band_b + harmonics_b) )   (only if the band gain is on, not bypassed)
//   y       = (direct + process) * outputGain
// W_b is the per-band width matrix (spec 4), applied to the sum of both parts of the band (it is linear).
// Solo: only soloed bands are heard (direct and processed). Zero latency, no clipping.
#pragma once

#include <array>
#include <cmath>
#include <vector>

#include "Analytic.h"
#include "BandDynamics.h"
#include "Crossover.h"
#include "Harmonics.h"
#include "Parameters.h"
#include "Smoothing.h"

namespace craft::enhancer::dsp
{

// Width gains (spec 4): returns (gM, gS). Same law as the Craft stereo imager.
inline void widthGains (double w, double& gM, double& gS) noexcept
{
    if (w <= 1.0)
    {
        gM = 1.0;
        gS = w;
    }
    else
    {
        const double g = std::sqrt ((5.0 + 3.0 / (w * w)) / 8.0);
        gM = g;
        gS = g * w;
    }
}

class EnhancerEngine
{
public:
    static constexpr int maxChannels = 2;
    static int getLatencySamples() noexcept { return 0; }

    void prepare (double sampleRate, int numChannelsToUse)
    {
        fs = sampleRate;
        numChannels = std::clamp (numChannelsToUse, 1, maxChannels);
        for (auto* s : { &inGain, &outGain, &directGain })
        {
            s->setTimeConstant (smoothingTau, fs);
            s->setEpsilon (1.0e-9);
        }
        for (int b = 0; b < numBands; ++b)
        {
            bandGain[(size_t) b].setTimeConstant (smoothingTau, fs);
            bandGain[(size_t) b].setEpsilon (1.0e-9);
            widthM[(size_t) b].setTimeConstant (smoothingTau, fs);
            widthS[(size_t) b].setTimeConstant (smoothingTau, fs);
            widthM[(size_t) b].setEpsilon (1.0e-9);
            widthS[(size_t) b].setEpsilon (1.0e-9);
            audible[(size_t) b].setRampTime (0.02, fs);
            procOn[(size_t) b].setRampTime (0.02, fs);
            dynamics[(size_t) b].prepare (b, fs);
            for (int c = 0; c < maxChannels; ++c)
            {
                analytic[(size_t) c][(size_t) b].prepare (fs);
                harmonics[(size_t) c][(size_t) b].prepare (b, fs);
            }
        }
        for (auto& s : logCrossover)
        {
            s.setTimeConstant (smoothingTau, fs);
            s.setEpsilon (1.0e-7);
        }
        punchSmoother.setTimeConstant (smoothingTau, fs);
        punchSmoother.setEpsilon (1.0e-4);
        (void) HarmonicCurves::get();
        setSettings (settings);
        snapSmoothers();
        reset();
    }

    void reset() noexcept
    {
        for (auto& x : crossovers)
            x.reset();
        for (int b = 0; b < numBands; ++b)
        {
            dynamics[(size_t) b].reset();
            for (int c = 0; c < maxChannels; ++c)
            {
                analytic[(size_t) c][(size_t) b].reset();
                harmonics[(size_t) c][(size_t) b].reset();
            }
        }
    }

    void setSettings (const Settings& s) noexcept
    {
        settings = s;
        inGain.setTarget (dbToGain (s.inputDb));
        outGain.setTarget (dbToGain (s.outputDb));
        directGain.setTarget (param::Direct::isOff (s.directDb) ? 0.0 : dbToGain (s.directDb));
        bool anySolo = false;
        for (bool v : s.solo)
            anySolo = anySolo || v;
        for (int b = 0; b < numBands; ++b)
        {
            const auto i = (size_t) b;
            const bool off = param::BandGain::isOff (s.bandDb[i]);
            bandGain[i].setTarget (off ? 0.0 : dbToGain (s.bandDb[i]));
            procOn[i].setTarget (off || s.bypass[i] ? 0.0 : 1.0);
            audible[i].setTarget (! anySolo || s.solo[i] ? 1.0 : 0.0);
            double gM, gS;
            widthGains (numChannels > 1 ? s.width[i] : 1.0, gM, gS);
            widthM[i].setTarget (gM);
            widthS[i].setTarget (gS);
        }
        const auto f = orderedCrossovers (s.crossoverHz);
        for (int k = 0; k < numCrossovers; ++k)
            logCrossover[(size_t) k].setTarget (std::log (std::clamp (f[(size_t) k], 1.0, maxCrossoverRatio * fs)));
        punchSmoother.setTarget (s.punch);
    }

    // Jump every smoother to its target (after prepare / state restore).
    void snapSmoothers() noexcept
    {
        for (auto* s : { &inGain, &outGain, &directGain })
            s->snapToTarget();
        for (int b = 0; b < numBands; ++b)
        {
            bandGain[(size_t) b].snapToTarget();
            widthM[(size_t) b].snapToTarget();
            widthS[(size_t) b].snapToTarget();
            audible[(size_t) b].snapToTarget();
            procOn[(size_t) b].snapToTarget();
        }
        for (auto& s : logCrossover)
            s.snapToTarget();
        punchSmoother.snapToTarget();
        updateCrossoverCoefficients();
        updatePunch();
    }

    // In-place processing of 1 or 2 channels.
    template <typename Sample>
    void process (Sample* const* channels, int numSamples) noexcept
    {
        for (int n = 0; n < numSamples; ++n)
        {
            // control-rate updates of the crossover coefficients and the punch profile while they move
            if (--controlCounter <= 0)
            {
                controlCounter = controlInterval;
                bool moving = false;
                for (auto& s : logCrossover)
                    moving = moving || ! s.isSettled();
                if (moving)
                {
                    for (auto& s : logCrossover)
                        for (int i = 0; i < controlInterval; ++i)
                            s.next();
                    updateCrossoverCoefficients();
                }
                if (! punchSmoother.isSettled())
                {
                    for (int i = 0; i < controlInterval; ++i)
                        punchSmoother.next();
                    updatePunch();
                }
            }

            const double gin = inGain.next();
            const double gout = outGain.next();
            const double gdir = directGain.next();

            double bands[maxChannels][numBands];
            double env[maxChannels][numBands];
            double re[maxChannels][numBands], im[maxChannels][numBands];
            for (int c = 0; c < numChannels; ++c)
            {
                crossovers[(size_t) c].process (double (channels[c][n]) * gin, bands[c]);
                for (int b = 0; b < numBands; ++b)
                {
                    analytic[(size_t) c][(size_t) b].process (bands[c][b], re[c][b], im[c][b]);
                    env[c][b] = std::sqrt (re[c][b] * re[c][b] + im[c][b] * im[c][b]);
                }
            }

            double out[maxChannels] = { 0.0, 0.0 };
            for (int b = 0; b < numBands; ++b)
            {
                const auto i = (size_t) b;
                double e = env[0][b];
                if (numChannels > 1)
                    e = std::max (e, env[1][b]);
                const double g = dynamics[i].process (e);
                const double bg = bandGain[i].next();
                const double on = procOn[i].next();
                const double aud = audible[i].next();
                const double gM = widthM[i].next();
                const double gS = widthS[i].next();
                const double procGain = g * bg * on;

                double v[maxChannels];
                for (int c = 0; c < numChannels; ++c)
                {
                    const double h = harmonics[(size_t) c][i].process (bands[c][b], re[c][b], im[c][b], env[c][b]);
                    const double proc = procGain != 0.0 ? procGain * (bands[c][b] + h) : 0.0;
                    v[c] = aud * (gdir * bands[c][b] + proc);
                }
                if (numChannels > 1)
                {
                    const double m = 0.5 * (v[0] + v[1]), s = 0.5 * (v[0] - v[1]);
                    out[0] += gM * m + gS * s;
                    out[1] += gM * m - gS * s;
                }
                else
                    out[0] += v[0];
            }
            for (int c = 0; c < numChannels; ++c)
                channels[c][n] = Sample (out[c] * gout);
        }
    }

    const Settings& getSettings() const noexcept { return settings; }
    double getBandGain (int b) const noexcept { return dynamics[(size_t) b].getCurrentGain(); }
    double getSampleRate() const noexcept { return fs; }

private:
    void updateCrossoverCoefficients() noexcept
    {
        std::array<SplitCoeffs, numCrossovers> c;
        for (int k = 0; k < numCrossovers; ++k)
            c[(size_t) k] = designSplit (k, std::exp (logCrossover[(size_t) k].getCurrent()), fs);
        for (auto& x : crossovers)
            x.setCoefficients (c);
    }

    void updatePunch() noexcept
    {
        for (auto& d : dynamics)
            d.setPunch (punchSmoother.getCurrent());
    }

    static constexpr int controlInterval = 16;

    double fs = 48000.0;
    int numChannels = 2;
    Settings settings;
    std::array<Crossover, maxChannels> crossovers;
    std::array<std::array<AnalyticFilter, numBands>, maxChannels> analytic;
    std::array<std::array<HarmonicGenerator, numBands>, maxChannels> harmonics;
    std::array<BandDynamics, numBands> dynamics;
    OnePoleSmoother inGain, outGain, directGain, punchSmoother;
    std::array<OnePoleSmoother, numBands> bandGain, widthM, widthS;
    std::array<OnePoleSmoother, numCrossovers> logCrossover;
    std::array<LinearRamp, numBands> audible, procOn;
    int controlCounter = 0;
};

} // namespace craft::enhancer::dsp
