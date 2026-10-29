// Craft Audio doubler - complete stereo engine (spec r1 section 4). Host-independent, header only.
//
//   (Lo, Ro) = Master * (Direct + EQ(sum of voices))
//
// One engine parameterised by the number of voices N (4 or 2); the 2-voice product is the 4-voice engine
// with voices 3 and 4 absent. No reported latency.
#pragma once

#include <array>
#include <cmath>
#include <vector>

#include "Parameters.h"
#include "Shelf.h"
#include "Smoothing.h"
#include "Voice.h"

namespace craft::doubler
{

class DoublerEngine
{
public:
    static constexpr int latencySamples = 0;

    explicit DoublerEngine (int numberOfVoices = 4) : nVoices (numberOfVoices >= 4 ? 4 : 2) { settings = defaultSettings (nVoices); }

    int getNumVoices() const noexcept { return nVoices; }
    double getSampleRate() const noexcept { return fs; }
    const Voice& getVoice (int i) const noexcept { return voices[size_t (i)]; }
    const Settings& getSettings() const noexcept { return settings; }

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        for (auto& v : voices)
            v.prepare (fs);
        const double tau = 1.0e-3; // gains, pans: settle within 5 ms (spec section 11.5)
        for (auto& c : voiceCoeff)
            for (auto& s : c)
                s.setTimeConstant (tau, fs);
        for (auto& s : directCoeff)
            s.setTimeConstant (tau, fs);
        master.setTimeConstant (tau, fs);
        for (auto& sh : shelves)
        {
            sh.gain.setTimeConstant (2.0e-3, fs);
            sh.logFreq.setTimeConstant (2.0e-3, fs);
            sh.gain.setEpsilon (1.0e-6);
            sh.logFreq.setEpsilon (1.0e-7);
            sh.enable.setRampTime (5.0e-3, fs);
        }
        bypassMix.setRampTime (5.0e-3, fs);
        directXfStep = 1.0 / std::max (1.0, 5.0e-3 * fs);

        int size = 1;
        while (size < RangeConfig::make (true, fs).latency + 4)
            size <<= 1;
        for (auto& d : directDelay)
            d.assign (size_t (size), 0.0);
        directMask = size - 1;

        applySettings();
    }

    void setSettings (const Settings& s)
    {
        const bool resetEdge = s.modReset && ! settings.modReset;
        settings = s;
        if (resetEdge)
            for (auto& v : voices)
                v.resetLfo(); // all LFOs restart at phase 0 simultaneously (spec section 7)
        applySettings();
    }

    void reset()
    {
        applySettings();
        for (auto& v : voices)
            v.reset();
        for (auto& c : voiceCoeff)
            for (auto& s : c)
                s.snap();
        for (auto& s : directCoeff)
            s.snap();
        master.snap();
        for (auto& sh : shelves)
        {
            sh.gain.snap();
            sh.logFreq.snap();
            sh.enable.snap();
            sh.updateCoeffs (fs);
            for (auto& f : sh.filter)
                f.reset();
        }
        bypassMix.snap();
        for (auto& d : directDelay)
            std::fill (d.begin(), d.end(), 0.0);
        directPos = 0;
        directDelayCur = directDelayNext = directDelayTarget;
        directXfActive = false;
    }

    // In-place stereo processing.
    template <typename Sample>
    void process (Sample* left, Sample* right, int numSamples) noexcept
    {
        for (int i = 0; i < numSamples; ++i)
        {
            const double L = double (left[i]), R = double (right[i]);
            double outL, outR;
            processSample (L, R, outL, outR);
            left[i] = Sample (outL);
            right[i] = Sample (outR);
        }
    }

    void processSample (double L, double R, double& outL, double& outR) noexcept
    {
        // --- direct path (rotation; optionally delayed by Lr)
        directPos = (directPos + 1) & directMask;
        directDelay[0][size_t (directPos)] = L;
        directDelay[1][size_t (directPos)] = R;
        double dl, dr;
        directTap (directDelayCur, dl, dr);
        if (directXfActive)
        {
            directXf += directXfStep;
            if (directXf >= 1.0)
            {
                directXfActive = false;
                directDelayCur = directDelayNext;
                directTap (directDelayCur, dl, dr);
            }
            else
            {
                double nl, nr;
                directTap (directDelayNext, nl, nr);
                dl = (1.0 - directXf) * dl + directXf * nl;
                dr = (1.0 - directXf) * dr + directXf * nr;
            }
        }
        else if (directDelayTarget != directDelayCur)
        {
            directDelayNext = directDelayTarget;
            directXfActive = true;
            directXf = 0.0;
        }
        const double cLL = directCoeff[0].next(), cLR = directCoeff[1].next(), cRL = directCoeff[2].next(),
                     cRR = directCoeff[3].next();
        const double directL = cLL * dl + cLR * dr;
        const double directR = cRL * dl + cRR * dr;

        // --- voices
        double sumL = 0.0, sumR = 0.0;
        const double srcA = routedA (L, R), srcB = routedB (L, R);
        for (int n = 0; n < nVoices; ++n)
        {
            const double y = voices[size_t (n)].process ((n % 2) == 0 ? srcA : srcB);
            sumL += voiceCoeff[size_t (n)][0].next() * y;
            sumR += voiceCoeff[size_t (n)][1].next() * y;
        }

        // --- voice EQ: low shelf, then high shelf (spec section 8)
        for (auto& sh : shelves)
            sh.process (sumL, sumR, fs);

        const double m = master.next();
        double oL = m * (directL + sumL), oR = m * (directR + sumR);

        const double b = bypassMix.next();
        if (b > 0.0)
        {
            oL = (1.0 - b) * oL + b * L;
            oR = (1.0 - b) * oR + b * R;
        }
        outL = oL;
        outR = oR;
    }

    // Nominal voice delay Dn in samples (spec section 5).
    static double nominalDelay (const VoiceSettings& v, bool align, const RangeConfig& c, double fs) noexcept
    {
        const double d = v.delayMs * fs / 1000.0;
        return align ? d + double (c.latency) : std::max (d, double (c.latency));
    }

private:
    double routedA (double L, double R) const noexcept
    {
        switch (settings.routingA)
        {
            case Routing::Sum:        return 0.5 * (L + R);
            case Routing::Difference: return 0.5 * (L - R);
            case Routing::Direct:     break;
        }
        return L;
    }
    double routedB (double L, double R) const noexcept
    {
        switch (settings.routingB)
        {
            case Routing::Sum:        return 0.5 * (R + L);
            case Routing::Difference: return 0.5 * (R - L);
            case Routing::Direct:     break;
        }
        return R;
    }

    void directTap (int delay, double& l, double& r) const noexcept
    {
        const size_t idx = size_t ((directPos - delay) & directMask);
        l = directDelay[0][idx];
        r = directDelay[1][idx];
    }

    void applySettings()
    {
        const auto& s = settings;
        const RangeConfig cfg = RangeConfig::make (s.range20, fs);

        // direct: gain g, rotation theta (spec section 4)
        const double gd = s.directOn ? std::pow (10.0, s.directGainDb / 20.0) : 0.0;
        const double th = s.directPanDeg * pi / 180.0;
        directCoeff[0].setTarget (gd * std::cos (th));
        directCoeff[1].setTarget (-gd * std::sin (th));
        directCoeff[2].setTarget (gd * std::sin (th));
        directCoeff[3].setTarget (gd * std::cos (th));
        directDelayTarget = s.alignDirect ? cfg.latency : 0;

        for (int n = 0; n < maxVoices; ++n)
        {
            const auto& v = s.voices[size_t (n)];
            const bool on = v.on && n < nVoices;
            const double g = on ? std::pow (10.0, v.gainDb / 20.0) : 0.0;
            const double phi = (v.panDeg + 45.0) * pi / 180.0; // constant-power pan
            voiceCoeff[size_t (n)][0].setTarget (g * std::cos (phi));
            voiceCoeff[size_t (n)][1].setTarget (g * std::sin (phi));

            const bool isStatic = v.detuneCents == 0.0 && v.depthCents == 0.0 && ! v.octave;
            voices[size_t (n)].setTargets (cfg, nominalDelay (v, s.alignDirect, cfg, fs),
                                           v.detuneCents + (v.octave ? -1200.0 : 0.0), v.depthCents, v.rateHz,
                                           v.feedbackPct / 100.0, s.fbHighPassHz, isStatic);
        }

        shelves[0].type = ShelfType::Low;
        shelves[0].setTargets (s.eqLowOn, s.eqLowGainDb, s.eqLowFreqHz);
        shelves[1].type = ShelfType::High;
        shelves[1].setTargets (s.eqHighOn, s.eqHighGainDb, s.eqHighFreqHz);

        master.setTarget (std::pow (10.0, s.masterDb / 20.0));
        bypassMix.setTarget (s.bypass ? 1.0 : 0.0);
    }

    struct Shelf
    {
        ShelfType type = ShelfType::Low;
        OnePole gain, logFreq;
        Ramp enable;
        Biquad filter[2];
        int countdown = 0;

        void setTargets (bool on, double gainDb, double freqHz)
        {
            enable.setTarget (on ? 1.0 : 0.0);
            gain.setTarget (gainDb);
            logFreq.setTarget (std::log2 (freqHz));
        }
        void updateCoeffs (double fs)
        {
            const auto c = designShelf (type, std::exp2 (logFreq.getCurrent()), gain.getCurrent(), fs);
            filter[0].c = c;
            filter[1].c = c;
        }
        void process (double& l, double& r, double fs) noexcept
        {
            // EQ parameter changes are smoothed (spec section 13.3); coefficients follow every 16 samples
            if (! gain.isSettled() || ! logFreq.isSettled())
            {
                gain.next();
                logFreq.next();
                if (--countdown <= 0 || (gain.isSettled() && logFreq.isSettled()))
                {
                    updateCoeffs (fs);
                    countdown = 16;
                }
            }
            const double e = enable.next();
            const double fl = filter[0].process (l), fr = filter[1].process (r);
            if (e >= 1.0)
            {
                l = fl;
                r = fr;
            }
            else if (e > 0.0)
            {
                l = e * fl + (1.0 - e) * l;
                r = e * fr + (1.0 - e) * r;
            }
        }
    };

    const int nVoices;
    double fs = 48000.0;
    Settings settings;

    std::array<Voice, maxVoices> voices;
    std::array<std::array<OnePole, 2>, maxVoices> voiceCoeff;
    std::array<OnePole, 4> directCoeff;
    OnePole master;
    std::array<Shelf, 2> shelves;
    Ramp bypassMix;

    std::array<std::vector<double>, 2> directDelay;
    int directMask = 0, directPos = 0;
    int directDelayTarget = 0, directDelayCur = 0, directDelayNext = 0;
    double directXf = 0.0, directXfStep = 0.01;
    bool directXfActive = false;
};

} // namespace craft::doubler
