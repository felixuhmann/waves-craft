// Craft Motion Filter - DSP building blocks (host-independent, double precision).
//
// Public references: V. Zavalishin, "The Art of VA Filter Design" (TPT / zero-delay-feedback state-variable
// filter, ch. 3-4); U. Zoelzer (ed.), "DAFX" (delay lines, comb filters, fractional-delay interpolation,
// sample-rate reduction); R. Bristow-Johnson, "Audio EQ Cookbook" (bilinear biquads).
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include "Laws.h"

namespace craft::mf
{

// One-pole smoother: y += (x - y)(1 - a), a = e^(-1/(tau fs)); tau <= 0 means no smoothing.
struct OnePole
{
    double a = 0.0, y = 0.0;
    void setTimeConstant (double tauSeconds, double fs) noexcept
    {
        a = (tauSeconds <= 0.0 || fs <= 0.0) ? 0.0 : std::exp (-1.0 / (tauSeconds * fs));
    }
    void reset (double v) noexcept { y = v; }
    double process (double x) noexcept
    {
        y = x + a * (y - x);
        return y;
    }
};

// Direct-form-I biquad (b0 b1 b2 / 1 a1 a2).
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    double x1 = 0, x2 = 0, y1 = 0, y2 = 0;

    void reset() noexcept { x1 = x2 = y1 = y2 = 0.0; }
    void setIdentity() noexcept { b0 = 1; b1 = b2 = a1 = a2 = 0; }
    double process (double x) noexcept
    {
        const double y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
        x2 = x1; x1 = x;
        y2 = y1; y1 = y;
        return y;
    }

    // Bilinear (prewarped) 2nd-order high-pass, Q = 1/sqrt(2) gives Butterworth.
    void setHighPass (double fc, double q, double fs) noexcept
    {
        const double w = 2.0 * law::pi * fc / fs, cw = std::cos (w), alpha = std::sin (w) / (2.0 * q);
        const double a0 = 1.0 + alpha;
        b0 = (1.0 + cw) / 2.0 / a0; b1 = -(1.0 + cw) / a0; b2 = b0;
        a1 = -2.0 * cw / a0; a2 = (1.0 - alpha) / a0;
    }
    void setLowPass (double fc, double q, double fs) noexcept
    {
        const double w = 2.0 * law::pi * fc / fs, cw = std::cos (w), alpha = std::sin (w) / (2.0 * q);
        const double a0 = 1.0 + alpha;
        b0 = (1.0 - cw) / 2.0 / a0; b1 = (1.0 - cw) / a0; b2 = b0;
        a1 = -2.0 * cw / a0; a2 = (1.0 - alpha) / a0;
    }
    void scale (double g) noexcept { b0 *= g; b1 *= g; b2 *= g; }

    // Magnitude at frequency f (for tests and displays).
    double magnitude (double f, double fs) const noexcept
    {
        const double w = 2.0 * law::pi * f / fs;
        const double cr = b0 + b1 * std::cos (w) + b2 * std::cos (2 * w), ci = -b1 * std::sin (w) - b2 * std::sin (2 * w);
        const double dr = 1.0 + a1 * std::cos (w) + a2 * std::cos (2 * w), di = -a1 * std::sin (w) - a2 * std::sin (2 * w);
        return std::sqrt ((cr * cr + ci * ci) / (dr * dr + di * di));
    }
};

// §4 output HF response. At 44.1/48 kHz the specified roll-off (-0.3 dB @ 10 kHz ... -4.8 dB @ 22 kHz) is
// reproduced by a biquad least-squares fitted (in dB, offline) to those points for each base rate (zeros
// reflected into the unit circle: minimum phase, same magnitude). Other
// rates in 40-60 kHz use the nearest fit; below 40 kHz and at 60 kHz and above the output is flat (review
// decision (5): no HF ripple at high rates).
inline void designOutputTone (Biquad& bq, double fs) noexcept
{
    bq.setIdentity();
    if (fs < 40000.0 || fs >= 60000.0)
        return;
    if (std::abs (fs - 44100.0) < std::abs (fs - 48000.0))
    {
        bq.b0 = 0.8889103266599879; bq.b1 = 1.2045256212824154; bq.b2 = 0.38143876816770167;
        bq.a1 = 1.179696154386514;  bq.a2 = 0.29412772800578146;
    }
    else
    {
        bq.b0 = 0.8524288441240261; bq.b1 = 1.1407260291161454; bq.b2 = 0.3735673855183175;
        bq.a1 = 1.0972052403144525; bq.a2 = 0.2678879341073828;
    }
}

// §4 wet-chain: 2nd-order Butterworth high-pass at 21.3 Hz with +0.35 dB broadband gain.
inline void designWetHighPass (Biquad& bq, double fs) noexcept
{
    bq.setHighPass (law::wetHighPassHz, 1.0 / std::sqrt (2.0), fs);
    bq.scale (law::dbToGain (law::wetGainDb));
}

// Topology-preserving-transform state-variable filter section (Zavalishin). Normalised responses with
// s = j w / wc: LP 1/D, BP (s/Q)/D (unity peak), HP s^2/D, with D = 1 + s/Q + s^2, prewarped so the
// analog cutoff is hit exactly at any sample rate. The damping k = 1/Q grows with the band state's
// level (kappa v1^2, previous sample) which limits resonance at high levels like an analog saturator.
struct SvfSection
{
    double ic1 = 0.0, ic2 = 0.0, v1Prev = 0.0;
    double kappa = law::resonanceLimitKappa;

    struct Out
    {
        double lp, bp, hp;
    };

    void reset() noexcept { ic1 = ic2 = v1Prev = 0.0; }

    // g = tan(pi fc / fs), k0 = 1/Q
    Out process (double x, double g, double k0) noexcept
    {
        const double k = std::min (k0 + kappa * v1Prev * v1Prev, 20.0);
        const double a1 = 1.0 / (1.0 + g * (g + k)), a2 = g * a1, a3 = g * a2;
        const double v3 = x - ic2;
        const double v1 = a1 * ic1 + a2 * v3;
        const double v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0 * v1 - ic1;
        ic2 = 2.0 * v2 - ic2;
        v1Prev = v1;
        return { v2, k * v1, x - k * v1 - v2 };
    }
};

// Circular delay buffer with 4-point Hermite interpolated reads.
class DelayBuffer
{
public:
    void allocate (int maxDelaySamples)
    {
        size_t n = 1;
        while (n < size_t (maxDelaySamples) + 8)
            n <<= 1;
        buf.assign (n, 0.0);
        mask = n - 1;
        writePos = 0;
    }
    void clear() noexcept { std::fill (buf.begin(), buf.end(), 0.0); }
    double maxDelay() const noexcept { return double (buf.size()) - 8.0; }

    void write (double x) noexcept
    {
        buf[writePos] = x;
        writePos = (writePos + 1) & mask;
    }

    // Value written d samples ago (clamped to [3, max] so the cubic only uses samples already written).
    double read (double d) const noexcept
    {
        d = clampd (d, 3.0, maxDelay());
        const double pos = double (writePos) - d + double (buf.size());
        const auto i = size_t (pos);
        const double f = pos - double (i);
        const double xm1 = buf[(i - 1) & mask], x0 = buf[i & mask], x1 = buf[(i + 1) & mask], x2 = buf[(i + 2) & mask];
        const double c1 = 0.5 * (x1 - xm1);
        const double c2 = xm1 - 2.5 * x0 + 2.0 * x1 - 0.5 * x2;
        const double c3 = 0.5 * (x2 - xm1) + 1.5 * (x0 - x1);
        return ((c3 * f + c2) * f + c1) * f + x0;
    }

private:
    std::vector<double> buf;
    size_t mask = 0, writePos = 0;
};

// §5 feedback comb without direct term: y[n] = x[n-D] + g y[n-D].
class CombFilter
{
public:
    void prepare (double fs) { line.allocate (int (std::ceil (0.25 * fs)) + 4); } // up to 250 ms
    void reset() noexcept { line.clear(); }
    double process (double x, double delaySamples, double g) noexcept
    {
        const double y = line.read (delaySamples);
        line.write (x + g * y);
        return y;
    }
    double maxDelay() const noexcept { return line.maxDelay(); }

private:
    DelayBuffer line;
};

// §9 drive: gain G into c tanh(u/c) (odd-symmetric, low-order odd harmonics).
inline double saturate (double x, double gain, double ceiling) noexcept
{
    return ceiling * std::tanh (gain * x / ceiling);
}

// §9 crush: sample-and-hold of the filter output at f_h (host-rate independent); off at crush 0.
struct Crusher
{
    double phase = 0.0, held = 0.0;
    void reset() noexcept { phase = 1.0; held = 0.0; }
    double process (double x, double holdRateHz, double fs) noexcept
    {
        phase += holdRateHz / fs;
        if (phase >= 1.0)
        {
            phase -= std::floor (phase);
            held = x;
        }
        return held;
    }
};

// Soft guard for the delay feedback path: exactly linear up to |x| = 1, then saturating towards 2.
inline double feedbackGuard (double x) noexcept
{
    const double a = std::abs (x);
    if (a <= 1.0)
        return x;
    const double y = 1.0 + std::tanh (a - 1.0);
    return x < 0.0 ? -y : y;
}

// Small deterministic PRNG (xorshift32) for the sample-and-hold LFO.
struct Random
{
    uint32_t s = 0x2545F491u;
    double nextBipolar() noexcept
    {
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        return double (s) / 2147483647.5 - 1.0;
    }
};

} // namespace craft::mf
