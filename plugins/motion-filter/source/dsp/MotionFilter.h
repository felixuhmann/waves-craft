// Craft Motion Filter - top-level processor: the engine, oversampled so that it runs at >= 176.4 kHz.
//
// Oversampling (4x at 44.1/48 kHz, 2x at 88.2/96 kHz, none at >= 120 kHz) keeps the filter responses on their
// analog prototypes up to 16 kHz (no bilinear cramping) and reduces saturator aliasing. Dry and wet run through
// the same up/down filters, so they stay aligned; the latency (37 / 31 / 0 samples) is reported to the host
// (spec §1: <= 64 samples, compensated). The section-4 output HF response is applied at the base rate.
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
    static constexpr int chunk = 256;

    static int factorFor (double rate) noexcept { return rate < 60000.0 ? 4 : (rate < 120000.0 ? 2 : 1); }

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        factor = factorFor (fs);
        oversampled = factor > 1;
        for (auto& o : os)
            o.setFactor (factor);
        engine.setSettings (settings);
        engine.prepare (fs * factor, ! oversampled);
        for (auto& t : tone)
        {
            designOutputTone (t, fs);
            if (! oversampled)
                t.setIdentity(); // the engine applies it (flat at these rates anyway)
        }
        for (auto& b : hi)
            b.assign (size_t (4 * chunk), 0.0);
        reset();
    }

    void reset()
    {
        engine.reset();
        for (auto& o : os) o.reset();
        for (auto& t : tone) t.reset();
    }

    void setSettings (const Settings& s) noexcept
    {
        settings = s;
        engine.setSettings (s);
    }
    const Settings& getSettings() const noexcept { return settings; }
    void setNote (int note) noexcept { engine.setNote (note); }

    int getLatencySamples() const noexcept { return os[0].latency(); }
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
            const int f = factor;
            for (int i = 0; i < n; ++i)
            {
                os[0].up (double (left[start + i]), hl + f * i);
                os[1].up (double (right[start + i]), hr + f * i);
                if (sideL != nullptr)
                {
                    os[2].up (double (sideL[start + i]), sl + f * i);
                    os[3].up (double (sideR != nullptr ? sideR[start + i] : sideL[start + i]), sr + f * i);
                }
            }
            engine.process (hl, hr, sideL != nullptr ? sl : nullptr, sideL != nullptr ? sr : nullptr, f * n, t);
            for (int i = 0; i < n; ++i)
            {
                left[start + i] = Sample (tone[0].process (os[0].down (hl + f * i)));
                right[start + i] = Sample (tone[1].process (os[1].down (hr + f * i)));
            }
            t.ppq += ppqPerSample * n;
        }
    }

private:
    double fs = 48000.0;
    bool oversampled = true;
    int factor = 4;
    Settings settings;
    MotionEngine engine;
    std::array<Oversampler, 4> os; // L, R, sidechain L, sidechain R
    std::array<Biquad, 2> tone;
    std::array<std::vector<double>, 4> hi;
};

} // namespace craft::mf
