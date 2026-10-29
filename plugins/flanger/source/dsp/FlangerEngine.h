// Craft Flanger - the complete per-sample processor (spec r1 sections 1, 3, 4, 5, 6, 7, 9).
// Host-independent, header-only, double precision internally.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

#include "FirFilter.h"
#include "Lfo.h"
#include "Parameters.h"
#include "Smoothing.h"

namespace craft::flanger::dsp
{

// Spec section 1 variants.
enum class Variant
{
    Stereo = 0,       // 2 in / 2 out, two channel processors
    Mono = 1,         // 1 in / 1 out, the "left" channel processor
    MonoToStereo = 2  // 1 in / 2 out, input 1 feeds both channel processors
};

inline int numInputChannels (Variant v) noexcept { return v == Variant::Stereo ? 2 : 1; }
inline int numOutputChannels (Variant v) noexcept { return v == Variant::Mono ? 1 : 2; }

// Parameter values as displayed (already quantised by the parameter mapping; the engine re-derives the
// integer quantities from them so that unquantised inputs cannot produce off-grid internal values).
struct Settings
{
    double delayMs = param::Delay::defaultValue;
    double depthPct = param::Depth::defaultValue;
    double rateHz = param::Rate::defaultValue;  // LFO cycles per beat when tempoSync is on
    double mixPct = param::Mix::defaultValue;
    double feedbackPct = param::Feedback::defaultValue;
    double gainDb = param::Gain::defaultValue;
    double stereoDeg = param::Stereo::defaultValue;
    double filterHz = param::FilterFreq::defaultValue;
    bool filterHighPass = true;
    bool filterOn = false;
    bool triangle = false;       // waveform: false = sine
    bool wetInvert = false;      // Wet Polarity "-"
    bool feedbackInvert = false; // Feedback Polarity "-"
    bool tape = false;
    bool stop = false;
    bool tempoSync = false;
    double bpm = 120.0;          // host tempo, used when tempoSync is on
};

// ---------------------------------------------------------------------------------------------
// Closed-form quantities of the spec (also used by the tests).
namespace model
{
    inline long delayTenths (double delayMs) noexcept { return std::clamp (std::lround (delayMs * 10.0), 1L, 500L); }
    inline long percent (double pct) noexcept { return std::clamp (std::lround (pct), 0L, 100L); }

    // 4: D = floor(Delay_ms * fs / 1000), integer samples.
    inline int baseDelaySamples (double delayMs, double fs) noexcept
    {
        return std::max (1, int (std::floor (double (delayTenths (delayMs)) * fs / 10000.0 + 1.0e-9)));
    }

    // 4 (Tape): c = floor(depth * D).
    inline int tapeCentre (double depthPct, int D) noexcept { return int ((percent (depthPct) * long (D)) / 100L); }

    // 3: g_wet = 1 - F^(20 Delay_ms) / 2.
    inline double wetCompensation (double feedbackPct, double delayMs) noexcept
    {
        const double F = double (percent (feedbackPct)) / 100.0;
        return 1.0 - std::pow (F, 2.0 * double (delayTenths (delayMs))) / 2.0;
    }

    // 3: F_loop = min(F, 0.999).
    inline double feedbackLoop (double feedbackPct) noexcept
    {
        return std::min (double (percent (feedbackPct)) / 100.0, 0.999);
    }

    // 5.2 / 9: LFO frequency in Hz (Rate below 0.01 = static; Tempo Sync: Rate cycles per beat).
    inline double lfoHz (const Settings& s) noexcept
    {
        double r = std::clamp (s.rateHz, 0.0, param::Rate::maxValue);
        if (r < 0.01 - 1.0e-9)
            r = 0.0;
        if (s.tempoSync)
            r *= (s.bpm > 0.0 ? s.bpm : 120.0) / 60.0;
        return r;
    }

    // 4 (Normal): d = D + 0.95 depth D L
    inline double normalDelay (double D, double depth, double L) noexcept { return D + 0.95 * depth * D * L; }
    // 4 (Tape): d = max(1, c + (c - 1) L)
    inline double tapeDelay (double c, double L) noexcept { return std::max (1.0, c + (c - 1.0) * L); }
} // namespace model

// ---------------------------------------------------------------------------------------------
class FlangerEngine
{
public:
    // Smoothing / crossfade times (spec 7: continuous parameters 10-50 ms, toggles <= 20 ms).
    static constexpr double smoothingTau = 0.015;   // one-pole time constant, s
    static constexpr double delayGlideRise = 0.050; // 10-90 % rise of the read-position glide, s
    static constexpr double toggleFade = 0.015;     // linear crossfade of toggles, s

    explicit FlangerEngine (Variant v = Variant::Stereo) : variant (v) {}

    Variant getVariant() const noexcept { return variant; }
    static constexpr int getLatencySamples() noexcept { return 0; }

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        // Longest read: 1.95 D (normal) or 2c - 1 (tape) at 50 ms, plus slack for the glide and the FIR.
        const int maxD = model::baseDelaySamples (param::Delay::maxValue, fs);
        int size = 1;
        while (size < 2 * maxD + 16)
            size <<= 1;
        mask = size - 1;
        maxRead = double (size - 4);
        for (auto& c : channels)
        {
            c.wet.assign (size_t (size), 0.0);
            c.dry.assign (size_t (size), 0.0);
        }

        for (auto* s : { &gIn, &gDry, &gWet, &fbMag, &depthS, &rateS, &offsetS, &filterHzS })
            s->setTimeConstant (smoothingTau, fs);
        gIn.setEpsilon (1.0e-10);
        gDry.setEpsilon (1.0e-10);
        gWet.setEpsilon (1.0e-10);
        fbMag.setEpsilon (1.0e-10);
        depthS.setEpsilon (1.0e-10);
        rateS.setEpsilon (1.0e-9);
        offsetS.setEpsilon (1.0e-12);
        filterHzS.setEpsilon (1.0e-6);
        for (auto* s : { &baseD, &tapeC })
        {
            s->setRiseTime (delayGlideRise, fs);
            s->setEpsilon (1.0e-9);
        }
        for (auto* r : { &tapeFade, &triFade, &filterOnFade, &highPassFade, &wetInvFade, &fbInvFade, &runFade, &bypassFade })
            r->setRampTime (toggleFade, fs);

        window = kaiserWindow (4.0);
        setSettings (settings);
        reset();
    }

    // Clears all state, restarts the LFO at theta = 0 and settles every smoother on its target.
    void reset() noexcept
    {
        for (auto& c : channels)
        {
            std::fill (c.wet.begin(), c.wet.end(), 0.0);
            std::fill (c.dry.begin(), c.dry.end(), 0.0);
            c.hist.fill (0.0);
            c.lastDelay = 0.0;
            c.lastLfo = 0.0;
        }
        writePos = 0;
        histPos = 0;
        phase = 0.0;
        for (auto* s : { &gIn, &gDry, &gWet, &fbMag, &depthS, &rateS, &offsetS, &filterHzS })
            s->snapToTarget();
        baseD.snapToTarget();
        tapeC.snapToTarget();
        for (auto* r : { &tapeFade, &triFade, &filterOnFade, &highPassFade, &wetInvFade, &fbInvFade, &runFade, &bypassFade })
            r->snapToTarget();
        kernelHz = -1.0;
        updateKernel (filterHzS.getCurrent(), highPassFade.getCurrent());
        for (int ch = 0; ch < 2; ++ch)
            channels[size_t (ch)].lastDelay = currentDelayFor (ch);
    }

    // Restart the LFO phase (theta = 0) without touching the audio state: start of playback (spec 9).
    void restartLfo() noexcept { phase = 0.0; }

    void setSettings (const Settings& s) noexcept
    {
        settings = s;
        const double m = double (model::percent (s.mixPct)) / 100.0;
        const int D = model::baseDelaySamples (s.delayMs, fs);

        gIn.setTarget (std::pow (10.0, std::round (s.gainDb * 10.0) / 200.0));
        gDry.setTarget (std::sqrt (1.0 - m));
        gWet.setTarget (std::sqrt (m) * model::wetCompensation (s.feedbackPct, s.delayMs));
        fbMag.setTarget (model::feedbackLoop (s.feedbackPct));
        depthS.setTarget (double (model::percent (s.depthPct)) / 100.0);
        rateS.setTarget (model::lfoHz (s));
        offsetS.setTarget (stereoOffsetCycles (s.stereoDeg));
        filterHzS.setTarget (std::clamp (s.filterHz, param::FilterFreq::minValue, std::min (param::FilterFreq::maxValue, 0.49 * fs)));
        baseD.setTarget (double (D));
        tapeC.setTarget (double (model::tapeCentre (s.depthPct, D)));

        tapeFade.setTarget (s.tape ? 1.0 : 0.0);
        triFade.setTarget (s.triangle ? 1.0 : 0.0);
        filterOnFade.setTarget (s.filterOn ? 1.0 : 0.0);
        highPassFade.setTarget (s.filterHighPass ? 1.0 : 0.0);
        wetInvFade.setTarget (s.wetInvert ? 1.0 : 0.0);
        fbInvFade.setTarget (s.feedbackInvert ? 1.0 : 0.0);
        runFade.setTarget (s.stop ? 0.0 : 1.0);
    }

    const Settings& getSettings() const noexcept { return settings; }

    void setBypassed (bool b) noexcept { bypassFade.setTarget (b ? 1.0 : 0.0); }

    // In-place processing. Stereo: ch0/ch1 = L/R. Mono: ch0 only (ch1 may be null).
    // MonoToStereo: ch0 is the input; outputs are written to ch0 and ch1 (input channel 2 is ignored).
    template <typename Sample>
    void process (Sample* ch0, Sample* ch1, int numSamples) noexcept
    {
        const int numCh = numOutputChannels (variant);
        if ((numCh == 2 && ch1 == nullptr) || ch0 == nullptr || channels[0].wet.empty())
            return;

        for (int i = 0; i < numSamples; ++i)
        {
            // shared per-sample control values
            const double gi = gIn.next();
            const double gd = gDry.next();
            const double gw = gWet.next() * (1.0 - 2.0 * wetInvFade.next());
            const double fb = fbMag.next() * (1.0 - 2.0 * fbInvFade.next());
            const double depth = depthS.next();
            const double Ds = baseD.next();
            const double cs = tapeC.next();
            const double tape = tapeFade.next();
            const double tri = triFade.next();
            const double fOn = filterOnFade.next();
            const double hp = highPassFade.next();
            const double fHz = filterHzS.next();
            const double offR = offsetS.next();
            const double byp = bypassFade.next();
            const double inc = rateS.next() * runFade.next() / fs;

            if (fOn > 0.0 && (fHz != kernelHz || hp != kernelHp))
                updateKernel (fHz, hp);

            const double in0 = double (ch0[i]);
            const double in1 = variant == Variant::Stereo ? double (ch1[i]) : in0;

            double out[2] = { 0.0, 0.0 };
            for (int ch = 0; ch < numCh; ++ch)
            {
                auto& c = channels[size_t (ch)];
                const double x = (ch == 0 ? in0 : in1);
                const double xg = gi * x;

                // 5: LFO value (right channel leads by the stereo offset)
                const double p = wrapPhase (phase + (ch == 1 ? offR : 0.0));
                double L = tri <= 0.0 ? lfoSine (p) : (tri >= 1.0 ? lfoTriangle (p) : (1.0 - tri) * lfoSine (p) + tri * lfoTriangle (p));

                // 4: delay trajectory and wet read (2-point linear interpolation); tape crossfades the taps
                double w;
                if (tape <= 0.0)
                {
                    const double d = clampDelay (model::normalDelay (Ds, depth, L));
                    w = readFrac (c.wet, d);
                    c.lastDelay = d;
                }
                else if (tape >= 1.0)
                {
                    const double d = clampDelay (model::tapeDelay (cs, L));
                    w = readFrac (c.wet, d);
                    c.lastDelay = d;
                }
                else
                {
                    const double dN = clampDelay (model::normalDelay (Ds, depth, L));
                    const double dT = clampDelay (model::tapeDelay (cs, L));
                    w = (1.0 - tape) * readFrac (c.wet, dN) + tape * readFrac (c.wet, dT);
                    c.lastDelay = (1.0 - tape) * dN + tape * dT;
                }
                c.lastLfo = L;

                // 3: v(n) = x_in(n) + p_fb F_loop w(n)   (feedback taken before the filter)
                c.wet[size_t (writePos)] = xg + fb * w;

                // dry path: undelayed (normal) or delayed by max(1, c) samples (tape)
                double dry = xg;
                c.dry[size_t (writePos)] = xg;
                if (tape > 0.0)
                    dry = (1.0 - tape) * xg + tape * readFrac (c.dry, std::max (1.0, cs));

                // 6: 9-tap FIR on the wet output only
                c.hist[size_t (histPos)] = w;
                double wf = w;
                if (fOn > 0.0)
                {
                    double acc = 0.0;
                    for (int k = 0; k < firTaps; ++k)
                        acc += kernel[size_t (k)] * c.hist[size_t ((histPos - k) & histMask)];
                    wf = fOn >= 1.0 ? acc : (1.0 - fOn) * w + fOn * acc;
                }

                const double y = gd * dry + gw * wf;
                out[ch] = byp <= 0.0 ? y : (1.0 - byp) * y + byp * x;
            }

            ch0[i] = Sample (out[0]);
            if (numCh == 2)
                ch1[i] = Sample (out[1]);

            writePos = (writePos + 1) & mask;
            histPos = (histPos + 1) & histMask;
            phase += inc;
            if (phase >= 1.0)
                phase -= 1.0;
        }
    }

    // Introspection (GUI indicator and tests): values used for the most recent sample.
    double getDelaySamples (int ch) const noexcept { return channels[size_t (ch & 1)].lastDelay; }
    double getLfoValue (int ch) const noexcept { return channels[size_t (ch & 1)].lastLfo; }
    double getPhase() const noexcept { return phase; }
    double getRightPhaseOffset() const noexcept { return offsetS.getCurrent(); }
    const FirKernel& getKernel() const noexcept { return kernel; }

private:
    struct Channel
    {
        std::vector<double> wet, dry; // v(n) delay line, input delay line (tape dry path)
        std::array<double, 16> hist {}; // w history for the FIR
        double lastDelay = 0.0, lastLfo = 0.0;
    };
    static constexpr int histMask = 15;

    double clampDelay (double d) const noexcept { return std::clamp (d, 1.0, maxRead); }

    // w = (1 - a) v(n - k) + a v(n - k - 1), k = floor(d), a = d - k; v(n) itself is not yet written.
    double readFrac (const std::vector<double>& buf, double d) const noexcept
    {
        const double fl = std::floor (d);
        const int k = int (fl);
        const double a = d - fl;
        const double v0 = buf[size_t ((writePos - k) & mask)];
        if (a == 0.0)
            return v0;
        const double v1 = buf[size_t ((writePos - k - 1) & mask)];
        return (1.0 - a) * v0 + a * v1;
    }

    double currentDelayFor (int ch) const noexcept
    {
        const double p = wrapPhase (phase + (ch == 1 ? offsetS.getCurrent() : 0.0));
        const double L = settings.triangle ? lfoTriangle (p) : lfoSine (p);
        return settings.tape ? clampDelay (model::tapeDelay (tapeC.getCurrent(), L))
                             : clampDelay (model::normalDelay (baseD.getCurrent(), depthS.getCurrent(), L));
    }

    void updateKernel (double fHz, double hp) noexcept
    {
        kernelHz = fHz;
        kernelHp = hp;
        if (hp <= 0.0)
            kernel = lowPassKernel (fHz, fs, window);
        else if (hp >= 1.0)
            kernel = highPassKernel (fHz, fs, window);
        else
        {
            const auto lp = lowPassKernel (fHz, fs, window), hpk = highPassKernel (fHz, fs, window);
            for (size_t k = 0; k < kernel.size(); ++k)
                kernel[k] = (1.0 - hp) * lp[k] + hp * hpk[k];
        }
    }

    const Variant variant;
    double fs = 48000.0;
    Settings settings;

    std::array<Channel, 2> channels;
    int mask = 0, writePos = 0, histPos = 0;
    double maxRead = 1.0;
    double phase = 0.0; // LFO phase in cycles, theta = 2 pi phase

    OnePoleSmoother gIn, gDry, gWet, fbMag, depthS, rateS, offsetS, filterHzS;
    SecondOrderSmoother baseD, tapeC;
    LinearRamp tapeFade, triFade, filterOnFade, highPassFade, wetInvFade, fbInvFade, runFade, bypassFade;

    FirKernel window {}, kernel {};
    double kernelHz = -1.0, kernelHp = -1.0;
};

} // namespace craft::flanger::dsp
