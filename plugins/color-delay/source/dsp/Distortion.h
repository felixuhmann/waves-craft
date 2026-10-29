// Craft Colour Delay - distortion colour (spec r1 section 7.2, [target]).
//
// Own design, fitted to the spec's measured targets:
//   mono sum (L + R) -> drive P(Send) -> gate (on the post-drive level) -> band-pass voicing (pre) -> symmetric
//   saturator (linear below |x| = 0.4, rational soft limit above; odd harmonics only) -> presence peak (post)
//   -> make-up Q(Send) -> latency delay -> width network (0 % identical L/R, 100 % decorrelated L/R, equal level).
// Voicing = pre * post is fitted to the small-signal response table (Send 50 %); the presence peak after the
// saturator raises the 3rd harmonic re the fundamental (odd-harmonic profile); P and Q are interpolated over Send
// from a table fitted to the drive, harmonic and gate data.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

#include "Filters.h"
#include "Smoothing.h"

namespace craft::cdl
{

class Distortion
{
public:
    static constexpr double kneeLevel = 0.4;     // saturator linear up to here
    static constexpr double gateThresholdDb = -30.0; // post-drive peak level (dBFS) below which the output is gated
    static constexpr double gateHysteresisDb = 2.0;
    static constexpr double latencySeconds = 0.0065; // plus ~0.3 ms voicing group delay = ~6.8 ms
    static constexpr double decorrelatorMs[4] = { 3.1, 4.7, 7.3, 11.3 };

    // drive P (dB at 1 kHz into the saturator) and make-up Q (dB) over Send (%)
    static double driveDb (double send) noexcept
    {
        static constexpr double s[] = { 0.0, 10.0, 25.0, 50.0, 100.0 };
        static constexpr double p[] = { -3.0, 1.0, 8.5, 22.0, 56.0 };
        return interp (s, p, 5, send);
    }
    static double makeupDb (double send) noexcept
    {
        static constexpr double s[] = { 0.0, 50.0, 100.0 };
        static constexpr double q[] = { -10.3, -10.3, -25.3 };
        return interp (s, q, 3, send);
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

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        // voicing (fitted at 48 kHz, designed at the actual rate)
        pre[0].setHighPass (96.75, 0.892, fs);
        pre[1].setHighPass (96.75, 0.892, fs);
        pre[2].setLowPass (std::min (4301.0, 0.45 * fs), 0.300, fs);
        pre[3].setLowPass (std::min (4301.0, 0.45 * fs), 0.300, fs);
        pre[4].setPeak (704.4, 0.989, 1.458, fs);
        pre[5].setLowPass (std::min (8715.0, 0.45 * fs), 1.214, fs);
        post.setPeak (std::min (3000.0, 0.4 * fs), 0.9, 3.0, fs);

        // normalise pre and post to 0 dB at 1 kHz
        const double w = 2.0 * pi * 1000.0 / fs;
        std::complex<double> h = 1.0;
        for (auto& b : pre)
            h *= b.response (w);
        preNorm = 1.0 / std::abs (h);
        postNorm = 1.0 / std::abs (post.response (w));

        delayBuf.assign ((size_t) std::lround (latencySeconds * fs) + 1, 0.0);
        for (int i = 0; i < 4; ++i)
            decorr[(size_t) i].buf.assign ((size_t) std::max (2, (int) std::lround (decorrelatorMs[i] * 0.001 * fs)), 0.0);

        envRelease = std::exp (-1.0 / (0.050 * fs));
        gateAttack = std::exp (-1.0 / (0.001 * fs));
        gateRelease = std::exp (-1.0 / (0.020 * fs));
        driveSmoother.setTimeConstant (0.02, fs);
        makeupSmoother.setTimeConstant (0.02, fs);
        widthSmoother.setTimeConstant (0.02, fs);
        reset();
    }

    void reset() noexcept
    {
        for (auto& b : pre) b.reset();
        post.reset();
        std::fill (delayBuf.begin(), delayBuf.end(), 0.0);
        delayPos = 0;
        for (auto& d : decorr)
        {
            std::fill (d.buf.begin(), d.buf.end(), 0.0);
            d.pos = 0;
        }
        env = 0.0;
        gate = 0.0;
        gateOpen = false;
        driveSmoother.snapToTarget();
        makeupSmoother.snapToTarget();
        widthSmoother.snapToTarget();
    }

    void setSend (double sendPercent) noexcept
    {
        driveSmoother.setTarget (driveDb (sendPercent));
        makeupSmoother.setTarget (makeupDb (sendPercent));
    }
    void setWidth (double widthPercent) noexcept { widthSmoother.setTarget (std::clamp (widthPercent, 0.0, 100.0) / 100.0); }
    void snapParameters() noexcept
    {
        driveSmoother.snapToTarget();
        makeupSmoother.snapToTarget();
        widthSmoother.snapToTarget();
    }

    void process (double inLeft, double inRight, double& outLeft, double& outRight) noexcept
    {
        const double drive = dbToGain (driveSmoother.next());
        const double makeup = dbToGain (makeupSmoother.next());
        const double width = widthSmoother.next();

        const double x = (inLeft + inRight) * drive;

        // gate on the post-drive peak envelope
        const double a = std::abs (x);
        env = a > env ? a : a + envRelease * (env - a);
        const double envDb = gainToDb (env);
        if (gateOpen ? envDb < gateThresholdDb - gateHysteresisDb : envDb >= gateThresholdDb)
            gateOpen = ! gateOpen;
        const double gTarget = gateOpen ? 1.0 : 0.0;
        gate = gTarget + (gTarget > gate ? gateAttack : gateRelease) * (gate - gTarget);
        if (gate < 1.0e-6 && ! gateOpen)
            gate = 0.0;

        double v = x * preNorm;
        for (auto& b : pre)
            v = b.process (v);
        v = saturate (v);
        v = post.process (v) * postNorm * makeup * gate;

        // latency
        const double d = delayBuf[(size_t) delayPos];
        delayBuf[(size_t) delayPos] = v;
        if (++delayPos == (int) delayBuf.size())
            delayPos = 0;

        // width: L = cos(t) d + sin(t) u, R = cos(t) d - sin(t) u, u = decorrelated d, t = width * pi/4
        double u = d;
        for (auto& ap : decorr)
            u = ap.process (u, 0.5);
        const double t = width * (pi / 4.0);
        const double c = std::cos (t), s = std::sin (t);
        outLeft = c * d + s * u;
        outRight = c * d - s * u;
    }

    int getLatencySamples() const noexcept { return (int) delayBuf.size() - 1; }

private:
    static double interp (const double* xs, const double* ys, int n, double x) noexcept
    {
        if (x <= xs[0]) return ys[0];
        for (int i = 1; i < n; ++i)
            if (x <= xs[i])
                return ys[i - 1] + (x - xs[i - 1]) / (xs[i] - xs[i - 1]) * (ys[i] - ys[i - 1]);
        return ys[n - 1];
    }

    struct AllPass
    {
        std::vector<double> buf;
        int pos = 0;
        double process (double x, double g) noexcept
        {
            const double dd = buf[(size_t) pos];
            const double v = x + g * dd;
            buf[(size_t) pos] = v;
            if (++pos == (int) buf.size())
                pos = 0;
            return dd - g * v;
        }
    };

    double fs = 48000.0;
    std::array<Biquad, 6> pre;
    Biquad post;
    double preNorm = 1.0, postNorm = 1.0;
    std::vector<double> delayBuf;
    int delayPos = 0;
    std::array<AllPass, 4> decorr;
    double env = 0.0, gate = 0.0, envRelease = 0.0, gateAttack = 0.0, gateRelease = 0.0;
    bool gateOpen = false;
    OnePoleSmoother driveSmoother, makeupSmoother, widthSmoother;
};

} // namespace craft::cdl
