// Craft Colour Delay - complete host-independent processor (spec r1 section 3).
//
//   xi = x * Input
//   wet path (per channel): feedback delay -> HP -> LP -> stereo-linked compressor -> wet gain
//   colour effects (reverb, distortion, doubler, phaser) take the post-wet-gain signal, run in parallel
//   y = dry * xi + wet + sum(effects);  y *= Output;  Monitor (L / L+R / R)
#pragma once

#include <algorithm>
#include <array>
#include <cmath>

#include "Compressor.h"
#include "Distortion.h"
#include "Doubler.h"
#include "FeedbackDelay.h"
#include "Filters.h"
#include "Parameters.h"
#include "Phaser.h"
#include "Reverb.h"
#include "Smoothing.h"

namespace craft::cdl
{

enum class Monitor { Left = 0, Stereo = 1, Right = 2 };

struct Settings
{
    double inputDb = 0.0, outputDb = 0.0;
    bool hostSync = true;
    std::array<double, 2> delayMs { 500.0, 500.0 };
    std::array<int, 2> noteIndex { param::noteDefaultIndex, param::noteDefaultIndex };
    std::array<double, 2> feedbackPct { param::feedbackDefault, param::feedbackDefault };
    bool delayLink = true;
    double dryWetPct = param::dryWetDefault;
    double hpHz = param::FilterHz::minValue;  // minValue = off
    double lpHz = param::FilterHz::maxValue;  // maxValue = off
    bool verbOn = false, distOn = false, doublerOn = false, phaserOn = false;
    double verbSize = param::VerbSize::defaultValue, verbLevel = param::levelDefault;
    double distLevel = param::levelDefault, distSend = param::distSendDefault, distWidth = param::distWidthDefault;
    double doublerLevel = param::levelDefault, doublerCents = param::Detune::defaultValue;
    double phaserLevel = param::levelDefault, phaserRateHz = param::PhaserRate::defaultValue;
    Monitor monitor = Monitor::Stereo;
    bool bypass = false;
};

class ColourDelayEngine
{
public:
    static constexpr double smoothingSeconds = 0.020;

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        const int maxDelay = (int) std::floor (2.0 * fs) + 1;
        for (auto& d : delays)
            d.prepare (fs, maxDelay);
        compressor.prepare (fs);
        reverb.prepare (fs);
        distortion.prepare (fs);
        doubler.prepare (fs);
        phaser.prepare (fs);

        for (auto* s : { &inGain, &outGain, &dryG, &wetG, &hpMix, &lpMix, &hpLogF, &lpLogF, &monL, &monR, &bypassMix })
            s->setTimeConstant (smoothingSeconds, fs);
        for (auto& s : feedback)
            s.setTimeConstant (smoothingSeconds, fs);
        for (auto& s : fxLevel)
            s.setTimeConstant (smoothingSeconds, fs);
        for (auto& s : fxOn)
            s.setTimeConstant (smoothingSeconds, fs);
        hpLogF.setEpsilon (1.0e-7);
        lpLogF.setEpsilon (1.0e-7);
        for (auto& s : fxOn)
            s.setEpsilon (1.0e-6);
        bypassMix.setEpsilon (1.0e-6);

        applySettings();
        reset();
    }

    // Clears all state and snaps every smoothed control to its target (used after prepare and by tests).
    void reset() noexcept
    {
        for (auto& d : delays)
        {
            d.reset();
            d.snapDelay();
        }
        for (auto& f : hp) f.reset();
        for (auto& f : lp) f.reset();
        compressor.reset();
        for (auto* s : { &inGain, &outGain, &dryG, &wetG, &hpMix, &lpMix, &hpLogF, &lpLogF, &monL, &monR, &bypassMix })
            s->snapToTarget();
        for (auto& s : feedback) s.snapToTarget();
        for (auto& s : fxLevel) s.snapToTarget();
        for (auto& s : fxOn) s.snapToTarget();
        reverb.snapParameters();
        distortion.snapParameters();
        doubler.snapParameters();
        reverb.reset();
        distortion.reset();
        doubler.reset();
        phaser.reset();
        for (auto& a : fxActive) a = false;
        updateFilters (true);
    }

    void setSettings (const Settings& s) noexcept
    {
        settings = s;
        applySettings();
    }
    const Settings& getSettings() const noexcept { return settings; }

    void setTempo (double bpm) noexcept
    {
        if (bpm > 0.0 && bpm != tempo)
        {
            tempo = bpm;
            applyDelayTimes();
        }
    }

    // Effective delay-line length per channel (after Link and Host sync).
    int getDelaySamples (int channel) const noexcept { return targetDelay[(size_t) channel]; }

    template <typename Sample>
    void process (Sample* left, Sample* right, int numSamples) noexcept
    {
        for (int n = 0; n < numSamples; ++n)
        {
            double l = (double) left[n], r = (double) right[n];
            processSample (l, r);
            left[n] = (Sample) l;
            right[n] = (Sample) r;
        }
    }

    double getGainReductionDb() const noexcept { return compressor.getGainReductionDb(); }

    // component access (tests)
    Reverb& getReverb() noexcept { return reverb; }
    Distortion& getDistortion() noexcept { return distortion; }
    Doubler& getDoubler() noexcept { return doubler; }
    Phaser& getPhaser() noexcept { return phaser; }

private:
    enum Fx { Verb = 0, Dist, Dbl, Pha, NumFx };

    void applyDelayTimes() noexcept
    {
        for (int c = 0; c < 2; ++c)
        {
            const int src = settings.delayLink ? 0 : c;
            targetDelay[(size_t) c] = settings.hostSync ? param::delaySamplesNote (settings.noteIndex[(size_t) src], tempo, fs)
                                                        : param::delaySamplesMs (settings.delayMs[(size_t) src], fs);
            delays[(size_t) c].setDelay (targetDelay[(size_t) c]);
        }
    }

    void applySettings() noexcept
    {
        const auto& s = settings;
        applyDelayTimes();
        for (int c = 0; c < 2; ++c)
            feedback[(size_t) c].setTarget (std::clamp (s.feedbackPct[(size_t) (s.delayLink ? 0 : c)], 0.0, 100.0) / 100.0);

        inGain.setTarget (dbToGain (s.inputDb));
        outGain.setTarget (dbToGain (s.outputDb));
        dryG.setTarget (param::dryGain (s.dryWetPct));
        wetG.setTarget (param::wetGain (s.dryWetPct));

        const bool hpOn = s.hpHz > param::FilterHz::minValue;
        const bool lpOn = s.lpHz < param::FilterHz::maxValue;
        hpMix.setTarget (hpOn ? 1.0 : 0.0);
        lpMix.setTarget (lpOn ? 1.0 : 0.0);
        hpLogF.setTarget (std::log (std::clamp (s.hpHz, 16.0, 21357.0)));
        lpLogF.setTarget (std::log (std::clamp (s.lpHz, 16.0, 21357.0)));

        fxOn[Verb].setTarget (s.verbOn ? 1.0 : 0.0);
        fxOn[Dist].setTarget (s.distOn ? 1.0 : 0.0);
        fxOn[Dbl].setTarget (s.doublerOn ? 1.0 : 0.0);
        fxOn[Pha].setTarget (s.phaserOn ? 1.0 : 0.0);
        fxLevel[Verb].setTarget (param::reverbLevelGain (s.verbLevel));
        fxLevel[Dist].setTarget (param::effectLevelGain (s.distLevel));
        fxLevel[Dbl].setTarget (param::effectLevelGain (s.doublerLevel));
        fxLevel[Pha].setTarget (param::effectLevelGain (s.phaserLevel));

        reverb.setSize (s.verbSize);
        distortion.setSend (s.distSend);
        distortion.setWidth (s.distWidth);
        doubler.setDetune (s.doublerCents);
        phaser.setRate (s.phaserRateHz);

        monL.setTarget (s.monitor == Monitor::Right ? 0.0 : 1.0);
        monR.setTarget (s.monitor == Monitor::Left ? 0.0 : 1.0);
        bypassMix.setTarget (s.bypass ? 1.0 : 0.0);
    }

    void updateFilters (bool force) noexcept
    {
        const double maxF = 0.49 * fs;
        if (force || ! hpLogF.isSettled())
        {
            const double f = std::min (std::exp (hpLogF.next()), maxF);
            for (auto& h : hp) h.design (true, f, fs);
        }
        if (force || ! lpLogF.isSettled())
        {
            const double f = std::min (std::exp (lpLogF.next()), maxF);
            for (auto& h : lp) h.design (false, f, fs);
        }
    }

    void processSample (double& l, double& r) noexcept
    {
        const double xl = l, xr = r;
        const double gi = inGain.next();
        const double il = xl * gi, ir = xr * gi;

        // feedback delays (no cross-feed)
        double wl = delays[0].process (il, feedback[0].next());
        double wr = delays[1].process (ir, feedback[1].next());

        // HP then LP (outside the loop); crossfaded in/out at the off positions
        updateFilters (false);
        const double hm = hpMix.next(), lm = lpMix.next();
        if (hm > 0.0)
        {
            const double fl = hp[0].process (wl), fr = hp[1].process (wr);
            wl += hm * (fl - wl);
            wr += hm * (fr - wr);
        }
        else
        {
            hp[0].reset(); hp[1].reset();
        }
        if (lm > 0.0)
        {
            const double fl = lp[0].process (wl), fr = lp[1].process (wr);
            wl += lm * (fl - wl);
            wr += lm * (fr - wr);
        }
        else
        {
            lp[0].reset(); lp[1].reset();
        }

        // compressor (stereo-linked) and wet gain
        const double gc = compressor.process (wl, wr);
        const double gw = wetG.next();
        const double el = wl * gc * gw, er = wr * gc * gw;

        // colour effects on the post-wet-gain signal
        double fxl = 0.0, fxr = 0.0;
        for (int k = 0; k < NumFx; ++k)
        {
            const double on = fxOn[(size_t) k].next();
            const double lvl = fxLevel[(size_t) k].next();
            if (on <= 0.0)
            {
                if (fxActive[(size_t) k])
                {
                    resetEffect (k);
                    fxActive[(size_t) k] = false;
                }
                continue;
            }
            fxActive[(size_t) k] = true;
            double ol = 0.0, orr = 0.0;
            switch (k)
            {
                case Verb: reverb.process (el, er, ol, orr); break;
                case Dist: distortion.process (el, er, ol, orr); break;
                case Dbl:  doubler.process (el, er, ol, orr); break;
                default:   phaser.process (el, er, ol, orr); break;
            }
            const double g = on * lvl;
            fxl += g * ol;
            fxr += g * orr;
        }

        const double gd = dryG.next(), go = outGain.next();
        double yl = (gd * il + el + fxl) * go;
        double yr = (gd * ir + er + fxr) * go;
        yl *= monL.next();
        yr *= monR.next();

        const double b = bypassMix.next();
        if (b >= 1.0)
        {
            yl = xl;
            yr = xr;
        }
        else if (b > 0.0)
        {
            yl += b * (xl - yl);
            yr += b * (xr - yr);
        }
        l = safetyClip (yl);
        r = safetyClip (yr);
    }

    void resetEffect (int k) noexcept
    {
        switch (k)
        {
            case Verb: reverb.reset(); break;
            case Dist: distortion.reset(); break;
            case Dbl:  doubler.reset(); break;
            default:   phaser.reset(); break;
        }
    }

    double fs = 48000.0, tempo = 120.0;
    Settings settings;
    std::array<int, 2> targetDelay { 1, 1 };
    std::array<FeedbackDelay, 2> delays;
    std::array<CrossoverFilter, 2> hp, lp;
    Compressor compressor;
    Reverb reverb;
    Distortion distortion;
    Doubler doubler;
    Phaser phaser;

    OnePoleSmoother inGain, outGain, dryG, wetG, hpMix, lpMix, hpLogF, lpLogF, monL, monR, bypassMix;
    std::array<OnePoleSmoother, 2> feedback;
    std::array<OnePoleSmoother, NumFx> fxLevel, fxOn;
    std::array<bool, NumFx> fxActive {};
};

} // namespace craft::cdl
