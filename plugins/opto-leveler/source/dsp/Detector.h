// Craft Opto Leveler - sidechain: Hi Freq weighting (spec 5) and level detector (spec 6). Host independent.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>

#include "Filters.h"
#include "Weighting.h"

namespace craft::opto_leveler::dsp
{

// 90-degree allpass pair (design: tools/design_hilbert.py). Gives the quadrature envelope sqrt(I^2 + Q^2), which is
// exactly the peak level for a steady sine (no rectifier ripple) and sigma*sqrt(pi/2) for Gaussian noise - the
// same sine/noise ratio as a rectified-average detector calibrated to sine peak (spec 6).
class QuadratureEnvelope
{
public:
    static constexpr int numSections = 6;
    static constexpr std::array<double, numSections> coefI { 0.352758177150, 0.803970336922, 0.952314804368,
                                                              0.989107138613, 0.997561898560, 0.999511441759 };
    static constexpr std::array<double, numSections> coefQ { 0.628134996332, 0.901947905514, 0.977130385444,
                                                              0.994832880435, 0.998868628097, 0.999864342315 };

    void reset() noexcept
    {
        for (auto& s : stI)
            s = {};
        for (auto& s : stQ)
            s = {};
        qDelay = 0.0;
    }

    // Returns the instantaneous envelope of x.
    double process (double x) noexcept
    {
        double i = x, q = x;
        for (int k = 0; k < numSections; ++k)
        {
            i = section (stI[size_t (k)], coefI[size_t (k)], i);
            q = section (stQ[size_t (k)], coefQ[size_t (k)], q);
        }
        const double qd = qDelay;
        qDelay = q;
        return std::sqrt (i * i + qd * qd);
    }

private:
    struct State
    {
        double x1 = 0, x2 = 0, y1 = 0, y2 = 0;
    };
    // y[n] = a^2 (x[n] + y[n-2]) - x[n-2]
    static double section (State& s, double a, double x) noexcept
    {
        const double a2 = a * a;
        const double y = a2 * (x + s.y2) - s.x2;
        s.x2 = s.x1;
        s.x1 = x;
        s.y2 = s.y1;
        s.y1 = y;
        return y;
    }

    std::array<State, numSections> stI {}, stQ {};
    double qDelay = 0.0;
};

// Energy-separation amplitude estimate (discrete energy separation algorithm, "DESA-2", after Maragos, Kaiser &
// Quatieri, IEEE Trans. SP 1993). For a sinusoid it returns the exact amplitude from 5 consecutive samples at any
// frequency, so burst onsets and ends are acquired within ~0.1 ms. Delay: 2 samples (sidechain only).
class EnergyEnvelope
{
public:
    void reset() noexcept
    {
        for (auto& v : x)
            v = 0.0;
        last = 0.0;
    }
    double process (double in) noexcept
    {
        x[0] = x[1];
        x[1] = x[2];
        x[2] = x[3];
        x[3] = x[4];
        x[4] = in;
        const double px = x[2] * x[2] - x[1] * x[3];
        const double ym = x[2] - x[0], y0 = x[3] - x[1], yp = x[4] - x[2];
        const double py = y0 * y0 - ym * yp;
        if (px > 0.0 && py > 1.0e-30)
            last = 2.0 * px / std::sqrt (py);
        else if (px <= 0.0 && py <= 1.0e-30)
            last = std::abs (x[2]);
        return last;
    }

private:
    double x[5] {};
    double last = 0.0;
};

// Tunable detector constants (calibrated against T1/T3/T6/T7 - see IMPLEMENTATION_NOTES.md).
struct DetectorTuning
{
    double envelopeTau = 1.5e-3;  // s, averaging of the envelope in the compressed domain (programme / noise)
    double fastTau = 0.05e-3;     // s, averaging while an onset is being acquired
    double onsetDb = 15.0;        // envelope this far above the average starts onset acquisition ...
    double onsetHold = 2.0e-3;    // s, ... for this long (keeps burst attacks close to an ideal level step)
    double fallDb = 8.0;          // envelope this far below the average ...
    double fallPersist = 0.3e-3;  // s, ... for this long also switches to fast tracking (end of a burst)
    double compressKnee = 0.2;    // averaging domain F(e) = k ln(1 + e/k): linear below k, logarithmic above
    double satLevel = 0.2;        // > 0: above this envelope level F saturates further ...
    double satNepers = 0.5;       // ... approaching F(satLevel) + satNepers * k
    double rippleCornerHz = 110.0; // shelving low-pass applied to the waveform ripple (fast modulation path)
    double rippleFloor = 0.03;     // high-frequency floor of that shelf (relative)
    int estimator = 1;            // transient tracking: 0 = quadrature envelope, 1 = energy separation
};

// One channel of the sidechain: weighting -> quadrature envelope -> averaging in a compressed domain, plus the
// normalised rectifier ripple used for the waveform-following part of the gain (spec 6). For a steady sine the
// envelope is constant, so the averaging is exact (T1 calibration); for noise, averaging a concave function of the
// fluctuating envelope reads lower than for a sine of equal RMS (T6), increasingly so at high levels.
class SidechainChannel
{
public:
    void prepare (double sampleRate, const DetectorTuning& t)
    {
        fs = sampleRate;
        tuning = t;
        slowCoef = 1.0 - std::exp (-1.0 / (t.envelopeTau * fs));
        fastCoef = 1.0 - std::exp (-1.0 / (t.fastTau * fs));
        onsetRatio = std::pow (10.0, t.onsetDb / 20.0);
        holdSamples = std::max (1, int (std::lround (t.onsetHold * fs)));
        fallRatio = std::pow (10.0, -t.fallDb / 20.0);
        fallSamples = std::max (1, int (std::lround (t.fallPersist * fs)));
        satF = 0.0;
        if (t.satLevel > 0.0 && t.compressKnee > 0.0)
        {
            satF = t.compressKnee * std::log1p (t.satLevel / t.compressKnee);
            satS = t.satNepers * t.compressKnee;
        }
        // ripple shelf: H(s) = (floor * s + w) / (s + w)
        const double k = std::tan (pi * t.rippleCornerHz / fs);
        ripple.b0 = (t.rippleFloor + k) / (1.0 + k);
        ripple.b1 = (k - t.rippleFloor) / (1.0 + k);
        ripple.a1 = (k - 1.0) / (k + 1.0);
        weighting.prepare (fs);
        reset();
    }

    void reset()
    {
        weighting.reset();
        quad.reset();
        energy.reset();
        ripple.reset();
        env = envF = 0.0;
        rip = 0.0;
        hold = below = 0;
    }

    void setHiFreq (double hiFreq) { weighting.setHiFreq (hiFreq); }

    // averaging domain and its inverse
    double toDomain (double e) const noexcept
    {
        const double k = tuning.compressKnee;
        double f = k > 0.0 ? k * std::log1p (e / k) : e;
        if (satF > 0.0 && f > satF)
        {
            const double d = f - satF;
            f = satF + d / (1.0 + d / satS);
        }
        return f;
    }
    double fromDomain (double f) const noexcept
    {
        const double k = tuning.compressKnee;
        if (satF > 0.0 && f > satF)
        {
            const double d = std::min (f - satF, 0.999 * satS);
            f = satF + d / (1.0 - d / satS);
        }
        return k > 0.0 ? k * std::expm1 (std::min (f / k, 600.0)) : f;
    }

    // Processes one input sample; level() / rippleValue() are valid afterwards.
    void process (double x) noexcept
    {
        const double s = weighting.process (x);
        const double eq = quad.process (s);
        const double ed = energy.process (s);
        const double e = eq;
        if (e > env * onsetRatio && e > 1.0e-6)
            hold = holdSamples;
        if (e < env * fallRatio)
        {
            if (++below >= fallSamples)
                hold = holdSamples;
        }
        else
            below = 0;
        double coef = slowCoef;
        if (hold > 0)
        {
            --hold;
            coef = fastCoef;
        }
        // while a transient is being acquired, track the exact energy-separation amplitude
        const double tracked = (hold > 0 && tuning.estimator == 1) ? ed : e;
        envF += (toDomain (tracked) - envF) * coef;
        env = fromDomain (envF);
        // instantaneous waveform level relative to the envelope, in nepers: ln(|s| / e) + ln 2 is zero-mean for a
        // steady sine (and for Gaussian noise), positive near the waveform peaks
        const double r = e > 1.0e-9 ? std::log (std::max (std::abs (s) / e, 0.01)) + 0.69314718055994531 : 0.0;
        rip = ripple.process (r);
    }

    double level() const noexcept { return env; }        // linear, sine-peak calibrated
    double rippleValue() const noexcept { return rip; }  // dimensionless, ~0 mean
    const Weighting& getWeighting() const noexcept { return weighting; }

    static void flush (double& v) noexcept
    {
        if (std::abs (v) < 1.0e-30)
            v = 0.0;
    }
    void flushDenormals() noexcept
    {
        flush (env);
        flush (envF);
        flush (rip);
        flush (ripple.s);
    }

private:
    double fs = 48000.0;
    DetectorTuning tuning;
    Weighting weighting;
    QuadratureEnvelope quad;
    EnergyEnvelope energy;
    FirstOrder ripple;
    double slowCoef = 0.0, fastCoef = 0.0, onsetRatio = 1.0, env = 0.0, envF = 0.0, rip = 0.0;
    double satF = 0.0, satS = 1.0;
    double fallRatio = 0.0;
    int holdSamples = 1, hold = 0, fallSamples = 1, below = 0;
};

} // namespace craft::opto_leveler::dsp
