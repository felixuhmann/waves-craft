// Craft Hybrid Delay - complete per-sample processor (spec r1 sections 3-9).
// Host-independent, header-only, double precision internally.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

#include "Analog.h"
#include "Biquad.h"
#include "DelayLine.h"
#include "DelayTime.h"
#include "LoFi.h"
#include "Modulation.h"
#include "Parameters.h"
#include "Smoothing.h"

namespace craft::hdelay
{

struct Settings
{
    param::Sync sync = param::Sync::Host;
    double bpm = 120.0;      // manual tempo (BPM sync)
    double hostBpm = 120.0;  // host tempo (Host sync)
    double delayMs = 375.0, delayRMs = 375.0;
    int delayNote = param::defaultNote, delayRNote = param::defaultNote;
    bool dual = false;
    double feedbackPct = 60.0;
    bool pingPong = false;
    bool pingPongStartRight = false;
    bool phaseL = false, phaseR = false; // true = inverted wet
    double mixPct = 100.0;
    double outputDb = 0.0;
    int analog = param::defaultAnalog;   // 0 = Off, 1..4
    bool lofi = false;
    double modRateHz = 0.1, modDepthPct = 0.0;
    double hiPassHz = 0.0, loPassHz = 0.0; // 0 = Off
};

// Stereo: 2 in -> 2 out. MonoToStereo: the left input feeds both lines (Ping Pong available,
// Phase R follows Phase L).
enum class InputMode { Stereo, MonoToStereo };

// Delay time in ms for channel 0 (L) or 1 (R) - Delay R only in Dual mode.
inline double resolvedDelayMs (const Settings& s, int channel) noexcept
{
    const bool useR = channel == 1 && s.dual;
    return resolveDelayMs (s.sync, useR ? s.delayRMs : s.delayMs, useR ? s.delayRNote : s.delayNote, s.bpm, s.hostBpm);
}

class HybridDelayEngine
{
public:
    static constexpr double glideSeconds = 0.2;      // section 9: tau of delay-time changes
    static constexpr double hiPassQ = 0.80, loPassQ = 0.95;
    static constexpr double hiPassEdgeHz = 10.0;      // HP fades in/out from here when toggled
    static constexpr double mixSeconds = 0.004, gainSeconds = 0.004, feedbackSeconds = 0.01; // mix/gain: linear ramps
    static constexpr double filterSeconds = 0.03, depthSeconds = 0.02;
    static constexpr double fadeSeconds = 0.02;       // Analog / LoFi / Ping Pong / phase crossfades
    static constexpr double bypassSeconds = 0.01;

    void prepare (double fs)
    {
        sampleRate = fs;
        const int maxDelay = int (std::ceil (maxDelayMs * fs / 1000.0 * 1.5)) + 64;
        std::uint32_t seed = 0x1234567u;
        for (auto& c : ch)
        {
            c.line.prepare (maxDelay);
            c.lofi.prepare (fs);
            c.noise.prepare (fs, seed);
            seed = seed * 747796405u + 2891336453u;
            c.mod.prepare (fs);
            c.glide.setTimeConstant (glideSeconds, fs);
            c.glide.setEpsilon (1.0e-6);
            c.phaseGain.setRampTime (fadeSeconds, fs);
            c.noiseLevel.setTimeConstant (fadeSeconds, fs);
            c.noiseLevel.setEpsilon (1.0e-12);
        }
        lfo.prepare (fs);
        feedback.setTimeConstant (feedbackSeconds, fs);
        feedback.setEpsilon (1.0e-9);
        mix.setRampTime (mixSeconds, fs);
        outGain.setRampTime (gainSeconds, fs);
        depth.setTimeConstant (depthSeconds, fs);
        depth.setEpsilon (1.0e-7);
        analogFade.setRampTime (fadeSeconds, fs);
        lofiFade.setRampTime (fadeSeconds, fs);
        pingPongFade.setRampTime (fadeSeconds, fs);
        bypassFade.setRampTime (bypassSeconds, fs);
        for (auto* f : { &hp, &lp })
        {
            f->logHz.setTimeConstant (filterSeconds, fs);
            f->logHz.setEpsilon (1.0e-6);
        }
        hp.edgeHz = hiPassEdgeHz;
        lp.edgeHz = 0.45 * fs;
        prepared = true;
        applySettings (true);
        reset();
    }

    void setSettings (const Settings& s) noexcept
    {
        settings = s;
        if (prepared)
            applySettings (false);
    }
    const Settings& getSettings() const noexcept { return settings; }

    void setInputMode (InputMode m) noexcept
    {
        if (m == inputMode)
            return;
        inputMode = m;
        if (prepared)
            applySettings (false);
    }
    InputMode getInputMode() const noexcept { return inputMode; }

    void setBypassed (bool b) noexcept { bypassFade.setTarget (b ? 1.0 : 0.0); }

    // Clears the delay lines and snaps every smoother to its target.
    void reset() noexcept
    {
        for (auto& c : ch)
        {
            c.line.reset();
            c.lofi.reset();
            c.analogCur.reset();
            c.analogPrev.reset();
            c.noise.reset();
            c.mod.reset();
            c.glide.snap();
            c.hpf.reset();
            c.lpf.reset();
            c.phaseGain.snapTo (c.phaseGain.getTarget());
            c.noiseLevel.snap();
        }
        lfo.reset();
        feedback.snap();
        mix.snapTo (mix.getTarget());
        outGain.snapTo (outGain.getTarget());
        depth.snap();
        analogFade.snapTo (1.0);
        lofiFade.snapTo (lofiFade.getTarget());
        pingPongFade.snapTo (pingPongFade.getTarget());
        bypassFade.snapTo (bypassFade.getTarget());
        for (auto* f : { &hp, &lp })
        {
            f->logHz.snap();
            f->active = ! f->targetOff;
        }
        updateFilterCoeffs();
    }

    double getSampleRate() const noexcept { return sampleRate; }
    double getTargetDelaySamples (int c) const noexcept { return ch[size_t (c)].glide.getTarget(); }
    double getCurrentDelaySamples (int c) const noexcept { return ch[size_t (c)].glide.getCurrent(); }
    static constexpr int getLatencySamples() noexcept { return 0; }

    // In-place processing. right may equal nullptr only in MonoToStereo mode with a mono output.
    template <typename Sample>
    void process (Sample* left, Sample* right, int numSamples) noexcept
    {
        for (int i = 0; i < numSamples; ++i)
        {
            const double xl = double (left[i]);
            const double xr = inputMode == InputMode::MonoToStereo || right == nullptr ? xl : double (right[i]);
            double yl, yr;
            processSample (xl, xr, yl, yr);
            left[i] = Sample (yl);
            if (right != nullptr)
                right[i] = Sample (yr);
        }
    }

    void processSample (double xl, double xr, double& yl, double& yr) noexcept
    {
        // ---- controls
        const double g = feedback.next();
        tickFilter (hp);
        tickFilter (lp);
        if (--filterCoeffCountdown <= 0)
        {
            filterCoeffCountdown = 16;
            if (coeffsDirty || (hp.active && hp.logHz.getCurrent() != hp.usedLogHz)
                || (lp.active && lp.logHz.getCurrent() != lp.usedLogHz))
                updateFilterCoeffs();
        }
        const double aAnalog = analogFade.next();
        const double aLofi = lofiFade.next();
        const double aPing = pingPongFade.next();
        const double dep = depth.next();
        const double tri = lfo.next();

        // ---- read the lines
        std::array<double, 2> fb {}, wetRaw {};
        for (size_t c = 0; c < 2; ++c)
        {
            auto& k = ch[c];
            const double dg = k.glide.next();
            const double m = k.mod.process (dg * dep * tri * 0.5);
            const double d = std::clamp (dg + m, double (SincTable::halfTaps + 1), maxReadDelay());
            wetRaw[c] = k.line.read (d);
            fb[c] = k.line.read (d + 1.0);
        }

        // ---- loop inputs
        const bool mono = inputMode == InputMode::MonoToStereo;
        const std::array<double, 2> x { xl, mono ? xl : xr };
        std::array<double, 2> v {};
        if (aPing == 0.0)
        {
            v[0] = x[0] + g * fb[0];
            v[1] = x[1] + g * fb[1];
        }
        else
        {
            const double xm = mono ? xl : (xl + xr) * 0.70710678118654752440;
            const double injL = settings.pingPongStartRight ? 0.0 : 1.0;
            const double vpL = injL * xm + g * g * fb[1];
            const double vpR = (1.0 - injL) * xm + fb[0];
            if (aPing == 1.0)
            {
                v[0] = vpL;
                v[1] = vpR;
            }
            else
            {
                v[0] = x[0] + g * fb[0] + aPing * (vpL - (x[0] + g * fb[0]));
                v[1] = x[1] + g * fb[1] + aPing * (vpR - (x[1] + g * fb[1]));
            }
        }

        // ---- loop processing: Analog -> saturation -> LoFi -> LoPass -> HiPass
        for (size_t c = 0; c < 2; ++c)
        {
            auto& k = ch[c];
            double u = aAnalog >= 1.0 ? k.analogCur.process (v[c])
                                      : k.analogPrev.process (v[c]) * (1.0 - aAnalog) + k.analogCur.process (v[c]) * aAnalog;
            double w = u / (1.0 + std::abs (u));
            if (aLofi > 0.0)
            {
                const double lf = k.lofi.process (w);
                w = aLofi >= 1.0 ? lf : w + aLofi * (lf - w);
            }
            if (lp.active) w = k.lpf.process (w);
            if (hp.active) w = k.hpf.process (w);
            k.line.push (w);
        }

        // ---- output stage
        const double m = mix.next();
        double cm, sm;
        if (mix.isSettled() && m == 1.0) { cm = 0.0; sm = 1.0; }
        else if (mix.isSettled() && m == 0.0) { cm = 1.0; sm = 0.0; }
        else { cm = std::cos (m * 0.5 * pi); sm = std::sin (m * 0.5 * pi); }
        const double G = outGain.next();
        const double b = bypassFade.next();

        std::array<double, 2> y {};
        for (size_t c = 0; c < 2; ++c)
        {
            auto& k = ch[c];
            double wet = wetRaw[c];
            const double nl = k.noiseLevel.next();
            if (nl != 0.0)
                wet += k.noise.next (nl);
            wet *= k.phaseGain.next();
            y[c] = G * (cm * x[c] + sm * wet);
            if (b != 0.0)
                y[c] = b >= 1.0 ? x[c] : y[c] + b * (x[c] - y[c]);
        }
        yl = y[0];
        yr = y[1];
    }

private:
    struct FilterCtl
    {
        OnePole logHz;          // log(frequency)
        double edgeHz = 10.0;   // frequency the filter fades from / to when toggled
        bool active = false;    // biquads in circuit
        bool targetOff = true;
        double usedLogHz = 0.0; // value the current coefficients were computed for
    };

    struct Channel
    {
        DelayLine line;
        AnalogStage analogCur, analogPrev;
        LoFiFilter lofi;
        Biquad hpf, lpf;
        AnalogNoise noise;
        ModulationSmoother mod;
        OnePole glide, noiseLevel;
        LinearRamp phaseGain;
    };

    double maxReadDelay() const noexcept { return maxDelayMs * sampleRate / 1000.0 * 1.5; }

    void applySettings (bool initial) noexcept
    {
        const auto& s = settings;
        for (int c = 0; c < 2; ++c)
        {
            auto& k = ch[size_t (c)];
            k.glide.setTarget (delaySamples (resolvedDelayMs (s, c), sampleRate));
            const bool inv = (c == 1 && inputMode == InputMode::MonoToStereo) ? s.phaseL : (c == 0 ? s.phaseL : s.phaseR);
            k.phaseGain.setTarget (inv ? -1.0 : 1.0);
            k.noiseLevel.setTarget (s.analog > 0 ? std::pow (10.0, analogModeSpecs()[size_t (std::clamp (s.analog, 0, 4))].noiseDbfs / 20.0) : 0.0);
        }
        feedback.setTarget (s.feedbackPct / 100.0);
        mix.setTarget (std::clamp (s.mixPct, 0.0, 100.0) / 100.0);
        outGain.setTarget (std::pow (10.0, s.outputDb / 20.0));
        depth.setTarget (std::clamp (s.modDepthPct, 0.0, 100.0) / 100.0);
        lfo.setRate (s.modRateHz);
        lofiFade.setTarget (s.lofi ? 1.0 : 0.0);
        pingPongFade.setTarget (s.pingPong ? 1.0 : 0.0);

        const int mode = std::clamp (s.analog, 0, 4);
        if (initial)
        {
            for (auto& k : ch)
            {
                k.analogCur.prepare (mode, sampleRate);
                k.analogPrev.prepare (mode, sampleRate);
            }
            analogFade.snapTo (1.0);
        }
        else if (mode != ch[0].analogCur.getMode())
        {
            for (auto& k : ch)
            {
                k.analogPrev = k.analogCur;
                k.analogCur.prepare (mode, sampleRate);
            }
            analogFade.snapTo (0.0);
            analogFade.setTarget (1.0);
        }

        if (! initial && lofiFade.getCurrent() == 0.0 && s.lofi)
            for (auto& k : ch)
                k.lofi.reset();

        setFilterTarget (hp, s.hiPassHz, initial);
        setFilterTarget (lp, s.loPassHz, initial);
    }

    void setFilterTarget (FilterCtl& f, double hz, bool initial) noexcept
    {
        if (hz > 0.0)
        {
            if (! f.active)
            {
                f.active = true;
                f.logHz.snapTo (std::log (initial ? hz : f.edgeHz));
                for (auto& k : ch)
                    (&f == &hp ? k.hpf : k.lpf).reset();
                coeffsDirty = true;
                filterCoeffCountdown = 0; // new coefficients on the next sample
            }
            f.targetOff = false;
            f.logHz.setTarget (std::log (hz));
        }
        else
        {
            f.targetOff = true;
            if (initial)
                f.active = false;
            f.logHz.setTarget (std::log (f.edgeHz));
        }
        if (initial)
            f.logHz.snap();
    }

    void tickFilter (FilterCtl& f) noexcept
    {
        if (! f.active)
            return;
        f.logHz.next();
        if (f.targetOff && f.logHz.isSettled())
            f.active = false;
    }

    void updateFilterCoeffs() noexcept
    {
        coeffsDirty = false;
        hp.usedLogHz = hp.logHz.getCurrent();
        lp.usedLogHz = lp.logHz.getCurrent();
        if (hp.active)
        {
            const auto c = BiquadCoeffs::highPass (BiquadCoeffs::cornerSpec (std::exp (hp.logHz.getCurrent()), sampleRate), hiPassQ);
            for (auto& k : ch)
                k.hpf.setCoeffs (c);
        }
        if (lp.active)
        {
            const auto c = BiquadCoeffs::lowPass (BiquadCoeffs::cornerSpec (std::exp (lp.logHz.getCurrent()), sampleRate), loPassQ);
            for (auto& k : ch)
                k.lpf.setCoeffs (c);
        }
    }

    Settings settings;
    InputMode inputMode = InputMode::Stereo;
    double sampleRate = 48000.0;
    bool prepared = false, coeffsDirty = true;
    int filterCoeffCountdown = 0;

    std::array<Channel, 2> ch;
    TriangleLfo lfo;
    OnePole feedback, depth;
    TimedRamp mix, outGain;
    LinearRamp analogFade, lofiFade, pingPongFade, bypassFade;
    FilterCtl hp, lp;
};

} // namespace craft::hdelay
