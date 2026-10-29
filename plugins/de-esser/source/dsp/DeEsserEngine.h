// Craft De-Esser - complete per-sample processor (spec r1 sections 2-6, decisions D2/D3).
// Host-independent: no JUCE dependency.
#pragma once

#include <algorithm>
#include <cmath>

#include "Dynamics.h"
#include "Filters.h"
#include "Parameters.h"
#include "Smoothing.h"

namespace craft::deesser::dsp
{

class DeEsserEngine
{
public:
    static constexpr int maxChannels = 2;
    static constexpr double parameterRampSeconds = 0.010; // D2: Threshold / Freq glide (<= 20 ms)
    static constexpr double crossfadeSeconds = 0.010;     // D2: Audio / SideChain / Monitor crossfades (<= 20 ms)
    static constexpr double monitorScale = 0.70710678118654752440; // section 6: monitor = d / sqrt 2

    static constexpr int getLatencySamples() noexcept { return 0; }

    // numChannels: 2 = stereo (linked, d = SC((L+R)/2)), 1 = mono (d = SC(x), decision D3).
    explicit DeEsserEngine (int numChannels = 2) noexcept : channels (std::clamp (numChannels, 1, maxChannels)) {}

    int getNumChannels() const noexcept { return channels; }

    void prepare (double sampleRate) noexcept
    {
        fs = sampleRate;
        reducer.prepare (fs);
        threshold.setRampTime (parameterRampSeconds, fs);
        frequency.setRampTime (parameterRampSeconds, fs);
        for (auto* r : { &bandPassAmount, &widebandAmount, &monitorAmount, &bypassAmount })
            r->setRampTime (crossfadeSeconds, fs);
        applySettings (true);
        reset();
    }

    void reset() noexcept
    {
        sidechain.reset();
        for (auto& s : split)
            s.reset();
        reducer.reset();
        threshold.snapToTarget();
        frequency.snapToTarget();
        for (auto* r : { &bandPassAmount, &widebandAmount, &monitorAmount, &bypassAmount })
            r->snapToTarget();
        updateCoefficients (frequency.next());
        resetMeters();
    }

    void setSettings (const Settings& s) noexcept
    {
        settings = s;
        applySettings (false);
    }
    const Settings& getSettings() const noexcept { return settings; }

    void setBypassed (bool b) noexcept { bypassAmount.setTarget (b ? 1.0 : 0.0); }

    // In-place processing of numChannels() channels.
    template <typename Sample>
    void process (Sample* const* data, int numSamples) noexcept
    {
        Sample* c0 = data[0];
        Sample* c1 = channels > 1 ? data[1] : nullptr;

        for (int i = 0; i < numSamples; ++i)
        {
            const double thr = threshold.next();
            if (! frequency.isSettled())
                updateCoefficients (frequency.next());
            const double bp = bandPassAmount.next();
            const double wb = widebandAmount.next();
            const double mon = monitorAmount.next();
            const double byp = bypassAmount.next();

            const double x0 = double (c0[i]);
            const double x1 = c1 != nullptr ? double (c1[i]) : x0;

            // section 2: linked sidechain d = SC((L + R) / 2); mono: d = SC(x)
            const double scIn = c1 != nullptr ? 0.5 * (x0 + x1) : x0;
            const auto f = sidechain.process (scCoeffs, scIn);
            const double d = bp == 0.0 ? f.highPass : (bp == 1.0 ? f.bandPass : f.highPass + (f.bandPass - f.highPass) * bp);

            // section 5
            const double g = reducer.process (d, thr);
            meterEnvelope = std::max (meterEnvelope, reducer.getEnvelope());
            meterMinGain = std::min (meterMinGain, g);

            // section 6
            const double monitorOut = d * monitorScale;
            c0[i] = Sample (channelOut (0, x0, g, wb, mon, byp, monitorOut));
            if (c1 != nullptr)
                c1[i] = Sample (channelOut (1, x1, g, wb, mon, byp, monitorOut));
        }
    }

    // Display support: largest detector envelope and smallest gain since the last call.
    double takeMeterEnvelope() noexcept { const double v = meterEnvelope; meterEnvelope = 0.0; return v; }
    double takeMeterMinGain() noexcept { const double v = meterMinGain; meterMinGain = 1.0; return v; }
    void resetMeters() noexcept { meterEnvelope = 0.0; meterMinGain = 1.0; }

    double getCurrentGain() const noexcept { return reducer.getGain(); }

private:
    double channelOut (int ch, double x, double g, double wb, double mon, double byp, double monitorOut) noexcept
    {
        const double high = split[ch].process (splitCoeffs, x);

        // Split: y = LP1(x) + g HP1(x) = x + (g - 1) HP1(x) (exact identity for g = 1); Wideband: y = g x.
        double y;
        if (wb == 1.0)
            y = g * x;
        else if (wb == 0.0)
            y = x + (g - 1.0) * high;
        else
            y = x + (g - 1.0) * (high + (x - high) * wb);

        if (mon != 0.0)
            y = mon == 1.0 ? monitorOut : y + (monitorOut - y) * mon;
        if (byp != 0.0)
            y = byp == 1.0 ? x : y + (x - y) * byp;
        return y;
    }

    void applySettings (bool snap) noexcept
    {
        threshold.setTarget (settings.thresholdDb);
        frequency.setTarget (std::clamp (settings.frequencyHz, param::Frequency::minValue, param::Frequency::maxValue));
        bandPassAmount.setTarget (settings.sidechain == SidechainMode::BandPass ? 1.0 : 0.0);
        widebandAmount.setTarget (settings.audio == AudioMode::Wideband ? 1.0 : 0.0);
        monitorAmount.setTarget (settings.monitor == MonitorMode::Sidechain ? 1.0 : 0.0);
        if (snap)
        {
            threshold.snapToTarget();
            frequency.snapToTarget();
            bandPassAmount.snapToTarget();
            widebandAmount.snapToTarget();
            monitorAmount.snapToTarget();
            updateCoefficients (frequency.getTarget());
        }
    }

    void updateCoefficients (double hz) noexcept
    {
        scCoeffs = SidechainCoeffs::design (hz, fs);
        splitCoeffs = SplitCoeffs::design (hz, fs);
    }

    const int channels;
    double fs = 48000.0;
    Settings settings;

    LinearRamp threshold;
    LogGlide frequency;
    LinearRamp bandPassAmount, widebandAmount, monitorAmount, bypassAmount;

    SidechainCoeffs scCoeffs;
    SplitCoeffs splitCoeffs;
    SidechainFilter sidechain;
    SplitHighBand split[maxChannels];
    GainReducer reducer;

    double meterEnvelope = 0.0, meterMinGain = 1.0;
};

} // namespace craft::deesser::dsp
