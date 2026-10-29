// Craft Dynamic Distortion - host-independent DSP core (spec r1 section 4 signal flow).
//
// Per channel (channels are fully independent):
//   x -> x Input -+--------------------------------------------------------------> delay -> x Direct ---+
//                 +-> [4x up] -> peak follower -> expansion gain E                                      |
//                 |            -> tone EQ -> x Drive -> x E -> pre-emphasis -> ADAA clip -> de-emphasis   + -> x Output
//                 |               -> HP -> LP -> [4x down] -> x Distortion Level ------------------------+
//
// The distortion path runs at 4x the host rate (review decision D1). The direct path is delayed by the
// oversampler latency (37 samples at every host rate), which is reported to the host.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

#include "Clipper.h"
#include "Expansion.h"
#include "Filters.h"
#include "Oversampling.h"
#include "Parameters.h"

namespace craft::dd
{

// Physical parameter values (section 3 units).
struct Settings
{
    double inputDb = param::Input::defaultValue;
    double directPercent = param::DirectLevel::defaultValue;
    double drivePercent = param::Drive::defaultValue;
    double distPercent = param::DistLevel::defaultValue;
    double attackMs = param::Attack::defaultValue;
    double releaseMs = param::Release::defaultValue;
    double bass = param::Bass::defaultValue;
    double mid = param::Mid::defaultValue;
    double midFreq = param::MidFreq::defaultValue;
    double treble = param::Treble::defaultValue;
    double lowPassHz = param::LowPass::defaultValue;
    double outputDb = param::Output::defaultValue;
    bool bypass = false; // crossfades to the (latency-aligned) unprocessed input
};

// Neutral settings used by the spec's measurements: EQ flat, LP 21 kHz, Input/Output 0 dB.
inline Settings neutralSettings()
{
    Settings s;
    s.bass = s.mid = s.treble = 0.0;
    s.lowPassHz = 21000.0;
    return s;
}

class DynamicDistortion
{
public:
    static constexpr int maxChannels = 2;
    static constexpr int oversampling = 4;
    // Clip ceiling of the (pre-emphasised) clipper. The low-frequency ceiling, i.e. the level long clipped
    // half-cycles settle at after de-emphasis, is clipCeiling * zero / pole = 0.310 (section 4.4).
    static constexpr double clipCeiling = 0.4461;
    static constexpr double detectorHoldMs = 2.0;  // peak hold before the release phase starts
    static constexpr double smoothingMs = 4.0;   // parameter smoothing time constant (D2: settled < 20 ms)
    static constexpr int coefficientInterval = 16; // base samples between filter updates while smoothing

    void setSettings (const Settings& s) noexcept { target = s; }
    const Settings& getSettings() const noexcept { return target; }

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        fsOs = fs * oversampling;
        latency = 0;
        for (auto& ch : channels)
        {
            ch.os.setFactor (oversampling);
            latency = ch.os.latency();
        }
        delayLength = std::max (latency, 1);
        for (auto& ch : channels)
            ch.delay.assign (size_t (delayLength), 0.0);
        smoothCoef = std::exp (-1.0 / (smoothingMs * 1.0e-3 * fs));
        snapToTarget();
        reset();
    }

    void reset() noexcept
    {
        for (auto& ch : channels)
        {
            ch.os.reset();
            ch.follower.reset();
            for (auto* b : { &ch.bass, &ch.mid, &ch.treble, &ch.pre, &ch.post, &ch.hp2, &ch.hp1, &ch.lpA, &ch.lpB })
                b->reset();
            ch.clip.reset();
            std::fill (ch.delay.begin(), ch.delay.end(), 0.0);
            ch.delayPos = 0;
        }
        snapToTarget();
    }

    // Development aid (calibration): replaces the expansion curve. The curve must outlive the engine.
    void setCurve (const expansion::Curve& c) noexcept { curve = &c; }
    // Development aid (calibration): detector hold time in ms (call before prepare()).
    void setDetectorHold (double ms) noexcept { holdMs = ms; }

    int getLatencySamples() const noexcept { return latency; }
    double getSampleRate() const noexcept { return fs; }

    // In-place processing of up to two channels.
    template <typename Sample>
    void process (Sample* const* data, int numChannels, int numSamples) noexcept
    {
        numChannels = std::min (numChannels, maxChannels);
        for (int i = 0; i < numSamples; ++i)
        {
            if (stepSmoothing())
                updateFilters();
            for (int c = 0; c < numChannels; ++c)
                data[c][i] = Sample (processSample (channels[size_t (c)], double (data[c][i])));
        }
    }

    // Convenience for tests: one channel.
    double processSample (int channel, double x) noexcept
    {
        if (stepSmoothing())
            updateFilters();
        return processSample (channels[size_t (channel)], x);
    }

    // Detector level (linear, peak) and current expansion gain (dB) of a channel, for metering.
    double getEnvelope (int channel) const noexcept { return channels[size_t (channel)].follower.value(); }
    double getExpansionDb (int channel) const noexcept
    {
        const double e = channels[size_t (channel)].follower.value();
        return e > 0.001 ? curve->gainDb (20.0 * std::log10 (e)) : 0.0;
    }

    // Small-signal gain of the drive stage (linear), including the correction that makes the 1 kHz
    // small-signal gain of the whole distortion path equal to the section 4.2 table at LP 21 kHz.
    static double driveGain (double percent) noexcept
    {
        static const double correctionDb = -gainToDb (post::hpMagnitude (1000.0) * post::lpMagnitude (1000.0, 21000.0));
        return dbToGain (laws::driveDb (percent) + correctionDb);
    }

private:
    struct Channel
    {
        Oversampler os;
        PeakFollower follower;
        Biquad bass, mid, treble, pre, post, hp2, hp1, lpA, lpB;
        AdaaClipper clip;
        std::vector<double> delay;
        int delayPos = 0;
    };

    // Smoothed control values.
    struct Controls
    {
        double inGain = 1, directGain = 1, distGain = 1, outGain = 1, driveGain = 1;
        double bass = 0, mid = 0, treble = 0, logMidFreq = 0, logLp = 0;
        double attackMs = 1, releaseMs = 31.8;
        double bypassMix = 0;
    };

    Controls targetControls() const noexcept
    {
        Controls c;
        c.inGain = dbToGain (clampd (target.inputDb, param::Input::minValue, param::Input::maxValue));
        c.outGain = dbToGain (clampd (target.outputDb, param::Output::minValue, param::Output::maxValue));
        c.directGain = laws::directGain (target.directPercent);
        c.distGain = laws::distortionGain (target.distPercent);
        c.driveGain = driveGain (target.drivePercent);
        c.bass = clampd (target.bass, -50.0, 50.0);
        c.mid = clampd (target.mid, -50.0, 50.0);
        c.treble = clampd (target.treble, -50.0, 50.0);
        c.logMidFreq = std::log (clampd (target.midFreq, param::MidFreq::minValue, param::MidFreq::maxValue));
        c.logLp = std::log (clampd (target.lowPassHz, param::LowPass::minValue, param::LowPass::maxValue));
        c.attackMs = clampd (target.attackMs, param::Attack::minValue, param::Attack::maxValue);
        c.releaseMs = clampd (target.releaseMs, param::Release::minValue, param::Release::maxValue);
        c.bypassMix = target.bypass ? 1.0 : 0.0;
        return c;
    }

    void snapToTarget() noexcept
    {
        cur = targetControls();
        filterControls = cur;
        countdown = 0;
        designFilters();
        for (auto& ch : channels)
        {
            ch.follower.setTimes (cur.attackMs, cur.releaseMs, holdMs, fsOs);
            ch.clip.setCeiling (clipCeiling);
        }
    }

    // Advances the smoothing by one base sample; true when the filters need a redesign.
    bool stepSmoothing() noexcept
    {
        const Controls t = targetControls();
        const double k = smoothCoef;
        auto glide = [k] (double& v, double to) {
            v = to + k * (v - to);
            if (std::abs (v - to) < 1.0e-9)
                v = to;
        };
        glide (cur.inGain, t.inGain);
        glide (cur.directGain, t.directGain);
        glide (cur.distGain, t.distGain);
        glide (cur.outGain, t.outGain);
        glide (cur.driveGain, t.driveGain);
        glide (cur.bass, t.bass);
        glide (cur.mid, t.mid);
        glide (cur.treble, t.treble);
        glide (cur.logMidFreq, t.logMidFreq);
        glide (cur.logLp, t.logLp);
        glide (cur.bypassMix, t.bypassMix);
        if (t.attackMs != cur.attackMs || t.releaseMs != cur.releaseMs)
        {
            cur.attackMs = t.attackMs;
            cur.releaseMs = t.releaseMs;
            for (auto& ch : channels)
                ch.follower.setTimes (cur.attackMs, cur.releaseMs, holdMs, fsOs);
        }
        if (--countdown > 0)
            return false;
        countdown = coefficientInterval;
        const bool changed = cur.bass != filterControls.bass || cur.mid != filterControls.mid
                             || cur.treble != filterControls.treble || cur.logMidFreq != filterControls.logMidFreq
                             || cur.logLp != filterControls.logLp;
        return changed;
    }

    void updateFilters() noexcept
    {
        filterControls = cur;
        designFilters();
    }

    void designFilters() noexcept
    {
        const auto& c = filterControls;
        const double midFreq = std::exp (c.logMidFreq), lp = std::exp (c.logLp);
        const auto midShape = eq::midShape (midFreq);
        Biquad bass, mid, treble, pre, postF, hp2, hp1, lpS;
        bass.design (eq::bass (c.bass), eq::bassF0, fsOs);
        mid.design (eq::mid (c.mid, midShape.qn), midShape.centreHz, fsOs);
        treble.design (eq::treble (c.treble), eq::trebleF0, fsOs);
        pre.design (emphasis::preFilter(), emphasis::poleHz, fsOs);
        postF.design (emphasis::postFilter(), emphasis::poleHz, fsOs);
        hp2.design (post::hp2(), post::hp2F0, fsOs);
        hp1.design (post::hp1(), post::hp1F0, fsOs);
        lpS.designMatchedLowpass1 (lp, fsOs);
        for (auto& ch : channels)
        {
            ch.bass.copyCoefficients (bass);
            ch.mid.copyCoefficients (mid);
            ch.treble.copyCoefficients (treble);
            ch.pre.copyCoefficients (pre);
            ch.post.copyCoefficients (postF);
            ch.hp2.copyCoefficients (hp2);
            ch.hp1.copyCoefficients (hp1);
            ch.lpA.copyCoefficients (lpS);
            ch.lpB.copyCoefficients (lpS);
        }
    }

    double processSample (Channel& ch, double x) noexcept
    {
        const double in = x * cur.inGain;

        // direct path: integer delay matching the oversampler (a bit-exact copy of the input; Input gain is
        // applied at the output of the delay so that bypass can use the same line)
        double delayed = x;
        if (latency > 0)
        {
            delayed = ch.delay[size_t (ch.delayPos)];
            ch.delay[size_t (ch.delayPos)] = x;
            if (++ch.delayPos >= delayLength)
                ch.delayPos = 0;
        }

        // distortion path at the oversampled rate
        double up[oversampling];
        ch.os.up (in, up);
        for (int k = 0; k < oversampling; ++k)
        {
            const double u = up[k];
            const double e = ch.follower.process (u);
            const double gain = cur.driveGain * curve->gain (e);
            double v = ch.treble.process (ch.mid.process (ch.bass.process (u)));
            v = ch.pre.process (v * gain);
            v = ch.post.process (ch.clip.process (v));
            v = ch.hp1.process (ch.hp2.process (v));
            up[k] = ch.lpB.process (ch.lpA.process (v));
        }
        const double dist = ch.os.down (up);

        const double wet = cur.outGain * (cur.directGain * (delayed * cur.inGain) + cur.distGain * dist);
        if (cur.bypassMix <= 0.0)
            return wet;
        if (cur.bypassMix >= 1.0)
            return delayed;
        return wet + cur.bypassMix * (delayed - wet);
    }

    Settings target;
    const expansion::Curve* curve = &expansion::defaultCurve();
    Controls cur, filterControls;
    std::array<Channel, maxChannels> channels;
    double fs = 48000.0, fsOs = 192000.0, smoothCoef = 0.0, holdMs = detectorHoldMs;
    int latency = 0, delayLength = 1, countdown = 0;
};

} // namespace craft::dd
