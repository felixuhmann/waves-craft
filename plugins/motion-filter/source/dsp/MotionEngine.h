// Craft Motion Filter - the complete per-sample processor (spec r1 sections 2-10). Host-independent.
//
// Per channel:
//   x = in * Input
//   Route pre:      line <- x + fb tap;  p = dDry x + dWet tap;  wet = Chain(p);  y = mixDry x + mixWet wet
//   Route feedback: wet = Chain(x);  line <- Chain'(x + fb tap);  y = dDry (mixDry x + mixWet wet) + dWet tap
//   out = OutputTone(y * Output)
// Chain = drive/saturator -> VCF -> crush -> wet high-pass (+0.35 dB). Chain' is a second instance of the
// same chain inside the delay feedback loop (each repeat filtered again), so the delay mix at -100 % still
// yields the plain filtered signal in feedback route.
#pragma once

#include <array>
#include <cmath>

#include "Delay.h"
#include "Modulators.h"
#include "Vcf.h"

namespace craft::mf
{

class MotionEngine
{
public:
    static constexpr int latencySamples = 0;
    static constexpr double knobSmoothingSeconds = 0.010; // spec §10: click-free (>= 5 ms) ramps

    struct Telemetry
    {
        double cutoffHz[2] { 18000.0, 18000.0 };
        double combDelayMs[2] { 28.0, 28.0 };
        double resonance[2] {};
        double lfo = 0.0;
        int step = 0;
        double follower[2] {};
        double delayMs[2] {};
    };

    // withOutputTone = false when a caller applies the section-4 output response itself (oversampled use).
    void prepare (double sampleRate, bool withOutputTone = true)
    {
        fs = sampleRate;
        for (auto& c : ch)
        {
            c.vcf.prepare (fs);
            c.vcfLoop.prepare (fs);
            designWetHighPass (c.hp, fs);
            designWetHighPass (c.hpLoop, fs);
            if (withOutputTone)
                designOutputTone (c.tone, fs);
            else
                c.tone.setIdentity();
            c.follower.prepare (fs);
            c.delay.prepare (fs);
        }
        for (auto* s : knobSmoothers())
            s->setTimeConstant (knobSmoothingSeconds, fs);
        for (auto& row : amountSmoothers)
            for (auto& s : row)
                s.setTimeConstant (knobSmoothingSeconds, fs);
        bypassFade.setTimeConstant (0.005, fs);
        prepared = true;
        reset();
    }

    void reset()
    {
        for (auto& c : ch)
        {
            c.vcf.setType (settings.type);
            c.vcf.setSlope24 (settings.slope24);
            c.vcf.reset();
            c.vcfLoop.setType (settings.type);
            c.vcfLoop.setSlope24 (settings.slope24);
            c.vcfLoop.reset();
            c.crusher.reset();
            c.crusherLoop.reset();
            c.hp.reset();
            c.hpLoop.reset();
            c.tone.reset();
            c.follower.reset();
            c.delay.reset();
            for (auto& m : c.mod)
                m.reset (0.0);
        }
        lfo.reset();
        seq.reset();
        wasPlaying = false;
        smoothForCoefficients = -1.0;
        snapSmoothers();
    }

    void setSettings (const Settings& s) noexcept
    {
        settings = s;
        for (auto& c : ch)
        {
            c.vcf.setType (s.type);
            c.vcf.setSlope24 (s.slope24);
            c.vcfLoop.setType (s.type);
            c.vcfLoop.setSlope24 (s.slope24);
            c.delay.setAnalog (s.delayAnalog);
        }
        lfo.setShape (s.lfoShape);
    }
    const Settings& getSettings() const noexcept { return settings; }

    // Keyboard tracking (§6.4): the most recent MIDI note; -1 = none (no offset).
    void setNote (int note) noexcept { currentNote = note; }

    // Test hook: disable the level-dependent resonance limiting (exactly linear SVF).
    void setResonanceLimiting (double kappa) noexcept
    {
        for (auto& c : ch)
        {
            c.vcf.setResonanceLimiting (kappa);
            c.vcfLoop.setResonanceLimiting (kappa);
        }
    }

    const Telemetry& getTelemetry() const noexcept { return telemetry; }
    double getSampleRate() const noexcept { return fs; }

    // Process a stereo block in place. sideL/sideR may be null (follower then uses the input).
    template <typename Sample>
    void process (Sample* left, Sample* right, const Sample* sideL, const Sample* sideR, int numSamples,
                  const Transport& transport = {}) noexcept
    {
        if (! prepared || numSamples <= 0)
            return;
        beginBlock (transport);

        const auto& s = settings;
        const bool useSidechain = s.followSidechain && sideL != nullptr;
        const double combMax = ch[0].vcf.combMaxDelay();
        const double crushRate = s.crush > 0.0 ? law::crushRateHz (s.crush) : 0.0;
        if (s.smooth != smoothForCoefficients)
        {
            smoothForCoefficients = s.smooth;
            for (auto& c : ch)
                for (auto& m : c.mod)
                    m.setTimeConstant (s.smooth * 0.001, fs); // Smooth % x 1 ms
        }
        // block-rate targets of the smoothed values (the smoothers ramp between blocks)
        const double tIn = law::dbToGain (s.inputDb), tOut = law::dbToGain (s.outputDb);
        const double tMixDry = law::mixDry (s.mix), tMixWet = law::mixWet (s.mix);
        const double tFb = s.delayFeedback / 100.0;
        const double tDDry = law::delayMixDry (s.delayMix), tDWet = law::delayMixWet (s.delayMix);
        const double tDriveG = law::driveGain (s.drive), tDriveC = law::driveCeiling (s.drive);
        double tAmount[3][3]; // LFO / sequencer: tapered T(a); follower: T(a)/T(0.5)
        bool anyMod = false;
        for (int d = 0; d < 3; ++d)
        {
            tAmount[modLfo][d] = law::taper (s.mod[modLfo][d] / 100.0, law::taperLfo);
            tAmount[modSeq][d] = law::taper (s.mod[modSeq][d] / 100.0, law::taperSeqFollower);
            tAmount[modFollower][d] = law::taper (s.mod[modFollower][d] / 100.0, law::taperSeqFollower)
                                      / law::taper (0.5, law::taperSeqFollower);
            for (int src = 0; src < 3; ++src)
                anyMod = anyMod || tAmount[src][d] != 0.0 || amountSmoothers[size_t (src)][size_t (d)].y != 0.0;
        }
        const double baseDelayMs = s.delaySync
            ? delaySyncChoices[size_t (clampIndex (s.delaySyncIndex, int (delaySyncChoices.size())))].beats() * 60000.0 / bpm
            : s.delayMs;
        const double ratio = delayRatioChoices[size_t (clampIndex (s.delayRatioIndex, 3))];
        const double kbdOct = (s.keyboardTracking && currentNote >= 0) ? (currentNote - 60) / 12.0 : 0.0;
        const double maxFc = std::min (law::maxCutoffHz, 0.45 * fs);
        const int lastStep = std::max (1, std::min (16, s.lastStep));

        for (int i = 0; i < numSamples; ++i)
        {
            // knob smoothing
            const double r = sFreq.process (s.freqNorm);
            const double resKnob = sRes.process (s.resonance);
            const double spread = sSpread.process (s.spread);
            const double driveG = sDriveG.process (tDriveG), driveC = sDriveC.process (tDriveC);
            const double inGain = sIn.process (tIn);
            const double outGain = sOut.process (tOut);
            const double mixDry = sMixDry.process (tMixDry);
            const double mixWet = sMixWet.process (tMixWet);
            const double fb = sFeedback.process (tFb);
            const double dDry = sDelayDry.process (tDDry);
            const double dWet = sDelayWet.process (tDWet);
            const double bypassWeight = bypassFade.process (s.bypass ? 1.0 : 0.0);
            double amount[3][3] {};
            if (anyMod)
                for (int src = 0; src < 3; ++src)
                    for (int d = 0; d < 3; ++d)
                        amount[src][d] = amountSmoothers[size_t (src)][size_t (d)].process (tAmount[src][d]);

            // shared sources: LFO and sequencer (same phase on both channels)
            const double l = lfo.value();
            lfo.advance();
            const int step = seq.currentStep (lastStep, s.swing);
            const double v = clampd (s.steps[size_t (step)] / 100.0, -1.0, 1.0);
            seq.advance();
            double lT[3], sT[3];
            for (int d = 0; d < 3; ++d)
            {
                lT[d] = l * amount[modLfo][d];
                sT[d] = v * amount[modSeq][d];
            }

            Sample* io[2] = { left, right };
            for (int c = 0; c < 2; ++c)
            {
                auto& st = ch[size_t (c)];
                const double in = double (io[c][i]);
                const double x = in * inGain;

                // envelope follower (input or sidechain)
                const double det = useSidechain ? double ((c == 0 || sideR == nullptr) ? sideL[i] : sideR[i]) : x;
                const double env = st.follower.process (det);
                double fOct[3];
                for (int d = 0; d < 3; ++d)
                    fOct[d] = amount[modFollower][d] != 0.0 ? law::followerOctavesScaled (env, amount[modFollower][d]) : 0.0;

                // destination sums, slewed by Smooth (§6)
                const double freqOct = st.mod[0].process (law::seqFreqOct * sT[0] + law::lfoFreqOct * lT[0] + fOct[0]);
                const double ampM = st.mod[1].process (sT[0] + law::lfoAmpScale * lT[0] + fOct[0] / law::seqFreqOct);
                const double resPts = st.mod[2].process (law::resPointsPerUnit
                                                         * (sT[1] + law::lfoAmpScale * lT[1] + fOct[1] / law::seqFreqOct));
                const double timeNorm = st.mod[3].process ((law::seqFreqOct * sT[2] + law::lfoFreqOct * lT[2] + fOct[2])
                                                           / law::knobRangeOct);
                const double kbd = st.mod[4].process (kbdOct);

                // filter control
                const double spreadOct = law::spreadOctaves (spread, c);
                const double fc = clampd (30.0 * std::exp2 (r * law::knobRangeOct + freqOct + kbd + spreadOct),
                                          law::minCutoffHz, maxFc);
                const double res = clampd (resKnob + resPts, 0.0, 100.0);
                VcfControl vc;
                vc.g = std::tan (law::pi * fc / fs);
                vc.k = 1.0 / law::qFromResonance (res);
                const double combMs = param::combDelayMs (clampd (r + freqOct / law::knobRangeOct, 0.0, 1.0))
                                      * std::exp2 (-kbd - spreadOct);
                vc.combDelaySamples = clampd (combMs * 0.001 * fs, 3.0, combMax);
                vc.combGain = law::combGain (res);
                if (s.type == FilterType::Amp || st.vcf.typeWeight (FilterType::Amp) > 1.0e-9)
                {
                    const double knobAmp = r <= 0.001 ? 0.0 : law::dbToGain (param::ampGainDb (r)); // -80 dB = mute
                    vc.ampGain = law::modulatedAmpGain (knobAmp, ampM);
                }

                // delay time (right channel x ratio), modulated in units of the free-time knob range
                const double dMs = baseDelayMs * (c == 1 ? ratio : 1.0) * (timeNorm != 0.0 ? std::pow (20.0, timeNorm) : 1.0);
                st.delay.setTarget (dMs * 0.001 * fs);

                // audio
                const double tap = st.delay.read();
                double y;
                if (! s.delayRouteFeedback)
                {
                    st.delay.write (feedbackGuard (x + fb * tap));
                    const double pre = dDry * x + dWet * tap;
                    const double wet = chain (st.vcf, st.crusher, st.hp, pre, vc, driveG, driveC, crushRate);
                    y = mixDry * x + mixWet * wet;
                }
                else
                {
                    const double wet = chain (st.vcf, st.crusher, st.hp, x, vc, driveG, driveC, crushRate);
                    const double loop = chain (st.vcfLoop, st.crusherLoop, st.hpLoop, x + fb * tap, vc, driveG, driveC, crushRate);
                    st.delay.write (feedbackGuard (loop));
                    y = dDry * (mixDry * x + mixWet * wet) + dWet * tap;
                }

                const double out = st.tone.process (y * outGain);
                io[c][i] = Sample (bypassWeight * in + (1.0 - bypassWeight) * out);

                if (i == numSamples - 1)
                {
                    telemetry.cutoffHz[c] = fc;
                    telemetry.combDelayMs[c] = vc.combDelaySamples * 1000.0 / fs;
                    telemetry.resonance[c] = res;
                    telemetry.follower[c] = env;
                    telemetry.delayMs[c] = st.delay.currentDelay() * 1000.0 / fs;
                }
            }
            if (i == numSamples - 1)
            {
                telemetry.lfo = l;
                telemetry.step = step;
            }
        }
    }

private:
    struct Channel
    {
        Vcf vcf, vcfLoop;
        Crusher crusher, crusherLoop;
        Biquad hp, hpLoop, tone;
        EnvelopeFollower follower;
        DelayChannel delay;
        std::array<OnePole, 5> mod; // freq oct, amp m, res points, time norm, keyboard oct
    };

    double chain (Vcf& vcf, Crusher& crusher, Biquad& hp, double u, const VcfControl& vc, double driveG, double driveC,
                  double crushRate) noexcept
    {
        double y = saturate (u, driveG, driveC);
        y = vcf.process (y, vc);
        if (crushRate > 0.0)
            y = crusher.process (y, crushRate, fs);
        return hp.process (y);
    }

    static int clampIndex (int i, int n) noexcept { return i < 0 ? 0 : (i >= n ? n - 1 : i); }

    void beginBlock (const Transport& t) noexcept
    {
        const auto& s = settings;
        bpm = (t.valid && t.bpm > 1.0) ? clampd (t.bpm, 10.0, 999.0) : 120.0;
        const bool playing = t.valid && t.playing;
        const bool started = playing && ! wasPlaying;
        wasPlaying = playing;

        // LFO
        if (s.lfoSync)
        {
            const double cycleBeats = lfoSyncChoices[size_t (clampIndex (s.lfoSyncIndex, int (lfoSyncChoices.size())))].beats();
            lfo.clock.inc = bpm / 60.0 / cycleBeats / fs;
            if (playing)
            {
                if (started) lfo.clock.pos = t.ppq / cycleBeats;
                else lfo.clock.follow (t.ppq / cycleBeats);
            }
        }
        else
        {
            lfo.clock.inc = clampd (s.lfoRateHz, 0.01, 1000.0) / fs;
            if (started)
                lfo.clock.pos = 0.0;
        }

        // sequencer
        if (s.seqSync)
        {
            const double stepBeats = seqSyncChoices[size_t (clampIndex (s.seqSyncIndex, int (seqSyncChoices.size())))].beats();
            seq.clock.inc = bpm / 60.0 / stepBeats / fs;
            if (playing)
            {
                if (started) seq.clock.pos = t.ppq / stepBeats;
                else seq.clock.follow (t.ppq / stepBeats);
            }
        }
        else
        {
            seq.clock.inc = clampd (s.seqRate, 0.01, 5000.0) / fs;
            if (started)
                seq.clock.pos = 0.0;
        }
    }

    std::array<OnePole*, 12> knobSmoothers() noexcept
    {
        return { &sFreq, &sRes, &sSpread, &sDriveG, &sDriveC, &sIn, &sOut, &sMixDry, &sMixWet, &sFeedback, &sDelayDry, &sDelayWet };
    }

    void snapSmoothers() noexcept
    {
        const auto& s = settings;
        sFreq.reset (s.freqNorm);
        sRes.reset (s.resonance);
        sSpread.reset (s.spread);
        sDriveG.reset (law::driveGain (s.drive));
        sDriveC.reset (law::driveCeiling (s.drive));
        sIn.reset (law::dbToGain (s.inputDb));
        sOut.reset (law::dbToGain (s.outputDb));
        sMixDry.reset (law::mixDry (s.mix));
        sMixWet.reset (law::mixWet (s.mix));
        sFeedback.reset (s.delayFeedback / 100.0);
        sDelayDry.reset (law::delayMixDry (s.delayMix));
        sDelayWet.reset (law::delayMixWet (s.delayMix));
        for (int d = 0; d < 3; ++d)
        {
            amountSmoothers[modLfo][size_t (d)].reset (law::taper (s.mod[modLfo][d] / 100.0, law::taperLfo));
            amountSmoothers[modSeq][size_t (d)].reset (law::taper (s.mod[modSeq][d] / 100.0, law::taperSeqFollower));
            amountSmoothers[modFollower][size_t (d)].reset (law::taper (s.mod[modFollower][d] / 100.0, law::taperSeqFollower)
                                                            / law::taper (0.5, law::taperSeqFollower));
        }
        bypassFade.reset (s.bypass ? 1.0 : 0.0);
    }

    double fs = 48000.0, bpm = 120.0;
    bool prepared = false, wasPlaying = false;
    int currentNote = -1;
    Settings settings;
    std::array<Channel, 2> ch;
    Lfo lfo;
    StepSequencer seq;
    double smoothForCoefficients = -1.0;
    OnePole sFreq, sRes, sSpread, sDriveG, sDriveC, sIn, sOut, sMixDry, sMixWet, sFeedback, sDelayDry, sDelayWet, bypassFade;
    std::array<std::array<OnePole, 3>, 3> amountSmoothers;
    Telemetry telemetry;
};

} // namespace craft::mf
