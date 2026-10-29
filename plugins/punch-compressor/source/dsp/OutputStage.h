// Craft Punch Compressor - output peak stage (spec r1 section 11, decisions D2 / D3).
//
// A look-ahead peak limiter with ceiling 1.0 that shares the compressor's H-sample look-ahead. Its detector runs
// on a *predicted* post-chain signal: the newest input sample through the current mix / output gain and a
// second (noise-free) instance of the analog stage (see PunchEngine). From that it derives the required gain
// q = min(1, 1 / |y|) per sample (linked across channels), which is
//
//   delayed by H - m  ->  held (minimum over m + 1 samples)  ->  released (one-pole, linear gain)
//   ->  two cascaded moving averages of A and B samples (A + B - 1 = m + 1)
//
// and applied to the actual (H-delayed) chain output. The two moving averages give the S-shaped ramp of
// section 11 (fitted: A = 45, B = 14 at 44.1 / 48 kHz, i.e. the gain starts falling 57 samples before the peak
// and reaches the required gain at the peak); they scale with the look-ahead at higher rates.
// A final clamp to +-1.0 guarantees D2 (never above 1.0) where the prediction is short (e.g. analog noise).
// Limiter and Clipper are the same stage (D3).
#pragma once

#include <algorithm>
#include <cmath>

#include "Primitives.h"

namespace craft::punch
{

class OutputStage
{
public:
    static constexpr double ceiling = 1.0;
    static constexpr double releaseSeconds = 0.050; // implementation choice (spec open question 3)

    void prepare (double fs, int lookahead)
    {
        const int scale = std::max (1, lookahead / 64);
        boxALength = 45 * scale;
        boxBLength = 14 * scale;
        rampLength = boxALength + boxBLength - 1; // 58 / 116 / 232
        const int m = rampLength - 1;
        qDelay.prepare (std::max (0, lookahead - m));
        hold.prepare (m + 1);
        boxA.prepare (boxALength);
        boxB.prepare (boxBLength);
        releaseCoeff = std::exp (-1.0 / (releaseSeconds * fs));
        reset();
    }

    void reset() noexcept
    {
        qDelay.reset (1.0);
        hold.reset (1.0);
        boxA.reset (1.0);
        boxB.reset (1.0);
        released = 1.0;
    }

    // Required gain for the newest (predicted) sample peak `p` (linear, linked over channels).
    static double requiredGain (double p) noexcept { return p > ceiling ? ceiling / p : 1.0; }

    // Push the newest required gain; returns the gain for the sample leaving the look-ahead now.
    double process (double q) noexcept
    {
        const double h = hold.push (qDelay.push (q));
        if (h <= released)
            released = h;
        else
        {
            released = h + releaseCoeff * (released - h);
            if (h - released < 1.0e-12)
                released = h;
        }
        return boxB.push (boxA.push (released));
    }

    static double clampToCeiling (double y) noexcept { return std::clamp (y, -ceiling, ceiling); }

    int getRampLength() const noexcept { return rampLength; }

private:
    int boxALength = 45, boxBLength = 14, rampLength = 58;
    DelayLine<double> qDelay;
    SlidingMin hold;
    Boxcar boxA, boxB;
    double released = 1.0, releaseCoeff = 0.0;
};

} // namespace craft::punch
