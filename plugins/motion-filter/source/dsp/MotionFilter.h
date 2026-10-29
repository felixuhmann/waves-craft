// Craft Motion Filter - top-level processor: the engine, 2x oversampled at base rates below 60 kHz.
//
// Oversampling keeps the filter responses on their analog prototypes up to high frequencies (no
// bilinear cramping near Nyquist at 44.1/48 kHz) and reduces saturator aliasing. Dry and wet run through
// the same up/down filters, so they stay aligned; the latency (31 samples) is reported to the host (spec §1:
// <= 64 samples, compensated). At 60 kHz and above the engine runs at the host rate with zero latency.
// The section-4 output HF response is applied at the base rate after down-sampling.
#pragma once

#include <array>
#include <vector>

#include "MotionEngine.h"
#include "Oversampling.h"

namespace craft::mf
{

class MotionFilter
{
public:
    static constexpr double oversamplingBelowHz = 60000.0;
    static constexpr int chunk = 256;

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        oversampled = fs < oversamplingBelowHz;
        engine.setSettings (settings);
        engine.prepare (oversampled ? 2.0 * fs : fs, ! oversampled);
        for (auto& t : tone)
        {
            designOutputTone (t, fs);
            if (! oversampled)
                t.setIdentity(); // the engine applies it (flat at these rates anyway)
        }
        for (auto& b : hi)
            b.assign (size_t (2 * chunk), 0.0);
        reset();
    }

    void reset()
    {
        engine.reset();
        for (auto& u : up) u.reset();
        for (auto& u : upSide) u.reset();
        for (auto& d : down) d.reset();
        for (auto& t : tone) t.reset();
    }

    void setSettings (const Settings& s) noexcept
    {
        settings = s;
        engine.setSettings (s);
    }
    const Settings& getSettings() const noexcept { return settings; }
    void setNote (int note) noexcept { engine.setNote (note); }
    void setResonanceLimiting (double kappa) noexcept { engine.setResonanceLimiting (kappa); }

    int getLatencySamples() const noexcept { return oversampled ? oversamplingLatency : 0; }
    double getSampleRate() const noexcept { return fs; }
    const MotionEngine::Telemetry& getTelemetry() const noexcept { return engine.getTelemetry(); }

    template <typename Sample>
    void process (Sample* left, Sample* right, const Sample* sideL, const Sample* sideR, int numSamples,
                  const Transport& transport = {}) noexcept
    {
        if (! oversampled)
        {
            engine.process (left, right, sideL, sideR, numSamples, transport);
            return;
        }

        Transport t = transport;
        const double ppqPerSample = t.bpm / 60.0 / fs;
        for (int start = 0; start < numSamples; start += chunk)
        {
            const int n = std::min (chunk, numSamples - start);
            double* hl = hi[0].data();
            double* hr = hi[1].data();
            double* sl = hi[2].data();
            double* sr = hi[3].data();
            for (int i = 0; i < n; ++i)
            {
                up[0].process (double (left[start + i]), hl[2 * i], hl[2 * i + 1]);
                up[1].process (double (right[start + i]), hr[2 * i], hr[2 * i + 1]);
                if (sideL != nullptr)
                {
                    upSide[0].process (double (sideL[start + i]), sl[2 * i], sl[2 * i + 1]);
                    upSide[1].process (double (sideR != nullptr ? sideR[start + i] : sideL[start + i]), sr[2 * i], sr[2 * i + 1]);
                }
            }
            engine.process (hl, hr, sideL != nullptr ? sl : nullptr, sideL != nullptr ? sr : nullptr, 2 * n, t);
            for (int i = 0; i < n; ++i)
            {
                left[start + i] = Sample (tone[0].process (down[0].process (hl[2 * i], hl[2 * i + 1])));
                right[start + i] = Sample (tone[1].process (down[1].process (hr[2 * i], hr[2 * i + 1])));
            }
            t.ppq += ppqPerSample * n;
        }
    }

private:
    double fs = 48000.0;
    bool oversampled = true;
    Settings settings;
    MotionEngine engine;
    std::array<Upsampler2x, 2> up, upSide;
    std::array<Downsampler2x, 2> down;
    std::array<Biquad, 2> tone;
    std::array<std::vector<double>, 4> hi;
};

} // namespace craft::mf
