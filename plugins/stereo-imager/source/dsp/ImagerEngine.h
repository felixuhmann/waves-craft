// Craft Audio stereo imager - complete host-independent processing engine.
//
// Per sample:  [M; S] = B * [in1; in2]          (steps 1-6 and gain, B smoothed, tau 40 ms)
//              M, S   -> shuffler sections        (step 7, optional, crossfaded in/out)
//              [L; R] = [M + S; M - S]            (step 9)
// When the shuffler is fully out of circuit the encode matrix is folded into B, so neutral
// settings give exact identity coefficients and a bit-exact passthrough.
#pragma once

#include "Matrix.h"
#include "Shuffler.h"
#include "Smoothing.h"

namespace craft::dsp
{

class ImagerEngine
{
public:
    // Smoothing constants (spec section 6)
    static constexpr double broadbandTimeConstant = 0.040; // s, first order
    static constexpr double shuffleRiseTime       = 0.160; // s, 10-90 %, second order
    static constexpr double shufflerFadeTime      = 0.050; // s, in/out crossfade of the shuffler stage
    static constexpr double bypassFadeTime        = 0.020; // s

    explicit ImagerEngine (Variant v = Variant::Full) noexcept : variant (v)
    {
        prepare (sampleRate);
        setSettings (Settings {});
        reset();
    }

    Variant getVariant() const noexcept { return variant; }

    // Always 0 (spec section 1).
    static constexpr int getLatencySamples() noexcept { return 0; }

    void prepare (double newSampleRate) noexcept
    {
        sampleRate = newSampleRate;
        for (auto& c : coeff)
        {
            c.setTimeConstant (broadbandTimeConstant, sampleRate);
            c.setEpsilon (1.0e-9);
        }
        for (auto* s : { &shuffleSm, &trimSm, &logFreqSm })
        {
            s->setRiseTime (shuffleRiseTime, sampleRate);
            s->setEpsilon (1.0e-7);
        }
        shufflerMix.setRampTime (shufflerFadeTime, sampleRate);
        bypassMix.setRampTime (bypassFadeTime, sampleRate);
        reset();
    }

    // Jump all smoothers to their targets and clear the filter state.
    void reset() noexcept
    {
        for (auto& c : coeff)
            c.snapToTarget();
        shuffleSm.snapToTarget();
        trimSm.snapToTarget();
        logFreqSm.snapToTarget();
        updateShufflerMixTarget();
        shufflerMix.snapToTarget();
        bypassMix.snapToTarget();
        secM.reset();
        secS.reset();
        updateShufflerCoefficients();
    }

    void setSettings (const Settings& p) noexcept
    {
        Settings s = p;
        if (variant != Variant::Full)
        {
            s.shuffle = 1.0;
            s.bassTrimDb = 0.0;
        }
        settings = s;

        const Matrix2 b = broadbandToMS (s, variant);
        coeff[0].setTarget (b.a00);
        coeff[1].setTarget (b.a01);
        coeff[2].setTarget (b.a10);
        coeff[3].setTarget (b.a11);

        targetActive = variant == Variant::Full && shufflerActive (s.shuffle, s.bassTrimDb);
        // While inactive, the shuffle/trim targets are neutral; frequency follows the setting.
        shuffleSm.setTarget (targetActive ? s.shuffle : 1.0);
        trimSm.setTarget (targetActive ? s.bassTrimDb : 0.0);
        logFreqSm.setTarget (std::log2 (s.frequencyHz));
        updateShufflerMixTarget();
    }

    const Settings& getSettings() const noexcept { return settings; }

    void setBypassed (bool shouldBypass) noexcept { bypassMix.setTarget (shouldBypass ? 1.0 : 0.0); }

    // True once every smoother has reached its target (useful for tests).
    bool isSettled() const noexcept
    {
        for (auto& c : coeff)
            if (! c.isSettled())
                return false;
        return shuffleSm.isSettled() && trimSm.isSettled() && logFreqSm.isSettled()
            && shufflerMix.isSettled() && bypassMix.isSettled();
    }

    bool isShufflerInCircuit() const noexcept { return shufflerMix.getCurrent() > 0.0 || shufflerMix.getTarget() > 0.0; }

    // Process a stereo block in place (ch1/ch2 may alias inputs; float or double).
    template <typename Sample>
    void process (Sample* ch1, Sample* ch2, int numSamples) noexcept
    {
        for (int i = 0; i < numSamples; ++i)
        {
            const double x1 = static_cast<double> (ch1[i]);
            const double x2 = static_cast<double> (ch2[i]);

            const double b00 = coeff[0].next(), b01 = coeff[1].next(), b10 = coeff[2].next(), b11 = coeff[3].next();

            if (! (shuffleSm.isSettled() && trimSm.isSettled() && logFreqSm.isSettled()))
            {
                shuffleSm.next();
                trimSm.next();
                logFreqSm.next();
                updateShufflerCoefficients();
                updateShufflerMixTarget();
            }
            const double mix = shufflerMix.next();

            double y1, y2;
            if (mix == 0.0)
            {
                // Shuffler out of circuit: fold encode into the matrix.
                y1 = (b00 + b10) * x1 + (b01 + b11) * x2;
                y2 = (b00 - b10) * x1 + (b01 - b11) * x2;
                // keep the filter state running so a later fade-in is smooth
                secM.process (b00 * x1 + b01 * x2);
                secS.process (b10 * x1 + b11 * x2);
            }
            else
            {
                double m = b00 * x1 + b01 * x2;
                double s = b10 * x1 + b11 * x2;
                const double mf = secM.process (m);
                const double sf = secS.process (s);
                if (mix == 1.0)
                {
                    m = mf;
                    s = sf;
                }
                else
                {
                    m += mix * (mf - m);
                    s += mix * (sf - s);
                }
                y1 = m + s;
                y2 = m - s;
            }

            const double bp = bypassMix.next();
            if (bp == 0.0)
            {
                ch1[i] = static_cast<Sample> (y1);
                ch2[i] = static_cast<Sample> (y2);
            }
            else if (bp == 1.0)
            {
                ch1[i] = static_cast<Sample> (x1);
                ch2[i] = static_cast<Sample> (x2);
            }
            else
            {
                ch1[i] = static_cast<Sample> (y1 + bp * (x1 - y1));
                ch2[i] = static_cast<Sample> (y2 + bp * (x2 - y2));
            }
        }
    }

private:
    void updateShufflerCoefficients() noexcept
    {
        double gM, gS;
        shufflerGains (shuffleSm.getCurrent(), trimSm.getCurrent(), gM, gS);
        const double f = std::exp2 (logFreqSm.getCurrent());
        secM.setCoefficients (shufflerSectionCoeffs (gM, f, sampleRate));
        secS.setCoefficients (shufflerSectionCoeffs (gS, f, sampleRate));
    }

    // The stage fades in as soon as it is requested, and fades out only once the shuffle and
    // trim values have glided back to neutral (so the S-shaped transition stays audible).
    void updateShufflerMixTarget() noexcept
    {
        const bool keep = targetActive || ! (shuffleSm.isSettled() && trimSm.isSettled());
        shufflerMix.setTarget (keep ? 1.0 : 0.0);
    }

    Variant variant;
    Settings settings;
    double sampleRate = 48000.0;
    bool targetActive = false;

    OnePoleSmoother coeff[4];
    SecondOrderSmoother shuffleSm, trimSm, logFreqSm;
    LinearRamp shufflerMix, bypassMix;
    FirstOrderSection secM, secS;
};

} // namespace craft::dsp
