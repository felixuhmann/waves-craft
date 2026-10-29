// Craft Colour Delay - distortion colour (spec r2 section 7.2, [target]).
//
// Own design, per channel (no channel mixing, spec r3 section 7); fed after the compressor (spec r3 section 3.4):
//   drive D(Send) -> gate (on the post-drive peak level of that channel) -> low-Send voicing blend -> band-pass voicing (pre) -> symmetric
//   saturator (linear below |x| = knee, rational soft limit above; odd harmonics only; first-order antiderivative
//   anti-aliasing) -> post voicing (presence peak
//   + low-pass) -> make-up M(Send) -> latency delay -> width all-pass.
// The spec r3 tables are end-to-end (plugin input -> isolated effect output), i.e. they include the compressor's gain
// reduction; the stage targets are the table values minus the compressor gain at that level, at a stage input level
// reduced by the same amount. The pre / post voicing, the knee and D / M at Send 50 / 75 / 100 % were fitted offline
// (least squares on a simulation of this chain at 48 kHz) to those stage targets: small-signal tables (Send 50 / 75 /
// 100 %), 1 kHz gain-vs-level (Send 50 / 100 %) and the odd-harmonic profile. At Send <= 25 % the stage is linear
// (stage input stays below the knee): D / M give -1.7 dB (25 %) and -9.85 dB (10 %), and the gate thresholds of
// Send 0 / 10 / 25 % land as specified (gate -32 dBFS post-drive). Below Send 50 % an extra voicing (1st-order HP 242 Hz, LP 3.86 kHz Q 0.2,
// peak 9.24 kHz +8.7 dB; 0 dB at 1 kHz) is blended in (fully at <= 25 %) for the narrower Send 25 % response.
// Width: the left channel ends in a first-order all-pass with coefficient +c, the right one with -c, c = Width * cMax;
// cMax gives a 53 degree phase difference at 1 kHz (correlation 0.6 for an identical 1 kHz tone). All-passes keep
// both channels at equal level and never mix them; at Width 0 both channels are processed identically.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <vector>

#include "Filters.h"
#include "Smoothing.h"

namespace craft::cdl
{

class Distortion
{
public:
    static constexpr double kneeLevel = 0.4649;      // saturator linear up to here
    static constexpr double gateThresholdDb = -32.0; // post-drive peak level (dBFS) below which the channel is gated
    static constexpr double gateHysteresisDb = 2.0;
    static constexpr double latencySeconds = 0.0065; // plus ~0.3 ms voicing group delay = ~6.8 ms
    static constexpr double widthCorrelation = 0.6;  // identical 1 kHz tone at Width 100 %

    // drive D (dB at 1 kHz into the saturator) and make-up M (dB) over Send (%)
    static double driveDb (double send) noexcept
    {
        static constexpr double s[] = { 0.0, 10.0, 25.0, 50.0, 75.0, 100.0 };
        static constexpr double d[] = { -6.0, -2.0, 10.0, 24.3556, 38.0874, 51.6177 };
        return interp (s, d, 6, send);
    }
    static double makeupDb (double send) noexcept
    {
        static constexpr double s[] = { 0.0, 10.0, 25.0, 50.0, 75.0, 100.0 };
        static constexpr double m[] = { -7.85, -7.85, -11.7, -12.6438, -12.7245, -12.3998 };
        return interp (s, m, 6, send);
    }

    static double saturate (double x) noexcept
    {
        const double a = std::abs (x);
        if (a <= kneeLevel)
            return x;
        const double u = (a - kneeLevel) / (1.0 - kneeLevel);
        const double y = kneeLevel + (1.0 - kneeLevel) * u / (1.0 + u);
        return x < 0.0 ? -y : y;
    }

    // antiderivative of saturate() (even function), for first-order anti-derivative anti-aliasing (ADAA)
    static double saturateIntegral (double x) noexcept
    {
        const double a = std::abs (x);
        if (a <= kneeLevel)
            return 0.5 * a * a;
        const double u = (a - kneeLevel) / (1.0 - kneeLevel);
        return 0.5 * kneeLevel * kneeLevel + kneeLevel * (a - kneeLevel)
             + (1.0 - kneeLevel) * (1.0 - kneeLevel) * (u - std::log1p (u));
    }

    // ADAA: y = (F(x) - F(x1)) / (x - x1); suppresses the aliasing of the heavily driven saturator
    static double saturateAdaa (double x, double x1) noexcept
    {
        const double d = x - x1;
        if (std::abs (d) < 1.0e-7)
            return saturate (0.5 * (x + x1));
        return (saturateIntegral (x) - saturateIntegral (x1)) / d;
    }

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        const double nyq = 0.45 * fs;
        for (auto& ch : chans)
        {
            ch.pre[0].setHighPass (96.8899, 0.8741, fs);
            ch.pre[1].setHighPass (96.8899, 0.8741, fs);
            ch.pre[2].setLowPass (std::min (1288.4486, nyq), 0.300, fs);
            ch.pre[3].setPeak (749.6161, 0.8698, 3.9373, fs);
            ch.pre[4].setLowPass (std::min (13337.2266, nyq), 1.4057, fs);
            ch.post[0].setPeak (std::min (2264.7365, nyq), 0.593, 6.0098, fs);
            ch.post[1].setLowPass (std::min (7158.3958, nyq), 1.2683, fs);
            // low-Send voicing
            {
                const double k = std::tan (pi * 241.7 / fs);
                ch.lowHp.b0 = 1.0 / (1.0 + k);
                ch.lowHp.b1 = -1.0 / (1.0 + k);
                ch.lowHp.a1 = (k - 1.0) / (1.0 + k);
            }
            ch.lowLp.setLowPass (std::min (3860.2, nyq), 0.2, fs);
            ch.lowPk.setPeak (std::min (9238.8, nyq), 0.7, 8.6732, fs);
            // The voicing was fitted with bilinear filters at 48 kHz, whose zeros at Nyquist add high-frequency droop;
            // above 60 kHz that droop is missing, so a 9 kHz Q 0.6 low-pass restores the 48 kHz response (4 / 8 kHz).
            ch.rateComp = Biquad();
            if (fs > 60000.0)
                ch.rateComp.setLowPass (9000.0, 0.6, fs);
            ch.delay.assign ((size_t) std::lround (latencySeconds * fs) + 1, 0.0);
        }

        // normalise pre and post to 0 dB at 1 kHz
        const double w = 2.0 * pi * 1000.0 / fs;
        std::complex<double> h = 1.0, g = 1.0;
        for (auto& b : chans[0].pre) h *= b.response (w);
        for (auto& b : chans[0].post) g *= b.response (w);
        g *= chans[0].rateComp.response (w);
        preNorm = 1.0 / std::abs (h);
        postNorm = 1.0 / std::abs (g);
        lowNorm = 1.0 / std::abs (chans[0].lowHp.response (w) * chans[0].lowLp.response (w) * chans[0].lowPk.response (w));

        // all-pass coefficient for the width target (bisection on the 1 kHz phase difference)
        const double target = std::acos (widthCorrelation);
        auto phaseDiff = [w] (double c) {
            const auto z1 = std::polar (1.0, -w);
            return std::arg ((c + z1) / (1.0 + c * z1)) - std::arg ((-c + z1) / (1.0 - c * z1));
        };
        double lo = 0.0, hi = 0.999;
        for (int i = 0; i < 60; ++i)
        {
            const double mid = 0.5 * (lo + hi);
            (std::abs (phaseDiff (mid)) < target ? lo : hi) = mid;
        }
        cMax = 0.5 * (lo + hi);

        envRelease = std::exp (-1.0 / (0.050 * fs));
        gateAttack = std::exp (-1.0 / (0.001 * fs));
        gateRelease = std::exp (-1.0 / (0.020 * fs));
        driveSmoother.setTimeConstant (0.02, fs);
        makeupSmoother.setTimeConstant (0.02, fs);
        lowSmoother.setTimeConstant (0.02, fs);
        widthSmoother.setTimeConstant (0.02, fs);
        reset();
    }

    void reset() noexcept
    {
        for (auto& ch : chans)
        {
            for (auto& b : ch.pre) b.reset();
            for (auto& b : ch.post) b.reset();
            ch.lowHp.reset();
            ch.lowLp.reset();
            ch.lowPk.reset();
            ch.rateComp.reset();
            std::fill (ch.delay.begin(), ch.delay.end(), 0.0);
            ch.apX = ch.apY = ch.env = ch.gate = ch.satX = 0.0;
            ch.gateOpen = false;
        }
        delayPos = 0;
        driveSmoother.snapToTarget();
        makeupSmoother.snapToTarget();
        widthSmoother.snapToTarget();
    }

    void setSend (double sendPercent) noexcept
    {
        driveSmoother.setTarget (driveDb (sendPercent));
        makeupSmoother.setTarget (makeupDb (sendPercent));
        lowSmoother.setTarget (std::clamp ((50.0 - sendPercent) / 25.0, 0.0, 1.0));
    }
    void setWidth (double widthPercent) noexcept { widthSmoother.setTarget (std::clamp (widthPercent, 0.0, 100.0) / 100.0); }
    void snapParameters() noexcept
    {
        driveSmoother.snapToTarget();
        makeupSmoother.snapToTarget();
        widthSmoother.snapToTarget();
        lowSmoother.snapToTarget();
    }

    void process (double inLeft, double inRight, double& outLeft, double& outRight) noexcept
    {
        const double drive = dbToGain (driveSmoother.next());
        const double makeup = dbToGain (makeupSmoother.next());
        const double c = widthSmoother.next() * cMax;
        const double low = lowSmoother.next();
        const double gateOnDb = gateThresholdDb, gateOffDb = gateThresholdDb - gateHysteresisDb;

        const double in[2] = { inLeft, inRight };
        double out[2];
        const int readPos = delayPos;
        for (int k = 0; k < 2; ++k)
        {
            auto& ch = chans[(size_t) k];
            const double x = in[k] * drive;

            // gate on the post-drive peak envelope of this channel
            const double a = std::abs (x);
            ch.env = a > ch.env ? a : a + envRelease * (ch.env - a);
            const double envDb = gainToDb (ch.env);
            if (ch.gateOpen ? envDb < gateOffDb : envDb >= gateOnDb)
                ch.gateOpen = ! ch.gateOpen;
            const double gTarget = ch.gateOpen ? 1.0 : 0.0;
            ch.gate = gTarget + (gTarget > ch.gate ? gateAttack : gateRelease) * (ch.gate - gTarget);
            if (ch.gate < 1.0e-6 && ! ch.gateOpen)
                ch.gate = 0.0;

            double v = x * preNorm;
            if (low > 0.0)
            {
                const double e = ch.lowPk.process (ch.lowLp.process (ch.lowHp.process (v))) * lowNorm;
                v += low * (e - v);
            }
            else
            {
                ch.lowHp.reset();
                ch.lowLp.reset();
                ch.lowPk.reset();
            }
            for (auto& b : ch.pre)
                v = b.process (v);
            const double sat = saturateAdaa (v, ch.satX);
            ch.satX = v;
            v = sat;
            for (auto& b : ch.post)
                v = b.process (v);
            v = ch.rateComp.process (v);
            v *= postNorm * makeup * ch.gate;

            // latency
            const double d = ch.delay[(size_t) readPos];
            ch.delay[(size_t) readPos] = v;

            // width all-pass: H(z) = (ck + z^-1) / (1 + ck z^-1), ck = +c (left) / -c (right)
            const double ck = k == 0 ? c : -c;
            const double y = ck * d + ch.apX - ck * ch.apY;
            ch.apX = d;
            ch.apY = y;
            out[k] = y;
        }
        if (++delayPos == (int) chans[0].delay.size())
            delayPos = 0;
        outLeft = out[0];
        outRight = out[1];
    }

    int getLatencySamples() const noexcept { return (int) chans[0].delay.size(); }

private:
    static double interp (const double* xs, const double* ys, int n, double x) noexcept
    {
        if (x <= xs[0]) return ys[0];
        for (int i = 1; i < n; ++i)
            if (x <= xs[i])
                return ys[i - 1] + (x - xs[i - 1]) / (xs[i] - xs[i - 1]) * (ys[i] - ys[i - 1]);
        return ys[n - 1];
    }

    struct Channel
    {
        std::array<Biquad, 5> pre;
        std::array<Biquad, 2> post;
        FirstOrder lowHp;
        Biquad lowLp, lowPk, rateComp;
        std::vector<double> delay;
        double apX = 0.0, apY = 0.0, env = 0.0, gate = 0.0, satX = 0.0;
        bool gateOpen = false;
    };

    double fs = 48000.0, preNorm = 1.0, postNorm = 1.0, lowNorm = 1.0, cMax = 0.0;
    std::array<Channel, 2> chans;
    int delayPos = 0;
    double envRelease = 0.0, gateAttack = 0.0, gateRelease = 0.0;
    OnePoleSmoother driveSmoother, makeupSmoother, widthSmoother, lowSmoother;
};

} // namespace craft::cdl
