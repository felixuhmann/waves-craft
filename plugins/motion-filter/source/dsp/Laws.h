// Craft Motion Filter - the measured laws of spec r1 (sections 4-9) as plain functions.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>

#include "Parameters.h"

namespace craft::mf::law
{

constexpr double pi = 3.14159265358979323846;

inline double dbToGain (double db) noexcept { return std::pow (10.0, db / 20.0); }
inline double gainToDb (double g) noexcept { return 20.0 * std::log10 (g); }

// ---------------------------------------------------------------------------------------------
// §5 filter
constexpr double minCutoffHz = 30.0, maxCutoffHz = 18600.0;
const double knobRangeOct = std::log2 (600.0); // 9.23 octaves: one full Freq knob turn

// Resonance (0..100 %) -> Q (spec r2 §5). 1/Q = 1 - 0.00586 Res up to 90 (Q 1.0 / 1.41 / 2.12), then Q rises
// linearly to 2.40 at 100. Level-independent.
constexpr double qAtResonance100 = 2.40;
inline double qFromResonance (double res) noexcept
{
    res = clampd (res, 0.0, 100.0);
    const double q = 1.0 / (1.0 - 0.00586 * std::min (res, 90.0));
    if (res <= 90.0)
        return q;
    return q + (qAtResonance100 - q) * (res - 90.0) / 10.0;
}

// Spread: left cutoff x 2^(+0.0144 S), right x 2^(-0.0144 S); channel 0 = left.
inline double spreadOctaves (double spreadPercent, int channel) noexcept
{
    return (channel == 0 ? 1.0 : -1.0) * 0.0144 * spreadPercent;
}

// Comb feedback gain g = 0.50 + 0.0029 * Feedback %.
inline double combGain (double feedbackPercent) noexcept { return 0.50 + 0.0029 * clampd (feedbackPercent, 0.0, 100.0); }


// ---------------------------------------------------------------------------------------------
// §6 modulation
constexpr double taperSeqFollower = 3.04, taperLfo = 3.57;

// T_k(|a|) sign(a), T_k(x) = (e^(kx) - 1) / (e^k - 1).
inline double taper (double a, double k) noexcept
{
    const double x = std::min (std::abs (a), 1.0);
    const double t = (std::exp (k * x) - 1.0) / (std::exp (k) - 1.0);
    return a < 0.0 ? -t : t;
}

constexpr double seqFreqOct = 5.17;  // octaves per unit of v T(a)
constexpr double lfoFreqOct = 9.23;  // octaves per unit of l T(a)
constexpr double lfoAmpScale = 1.43; // LFO m = 1.43 l T(a) for the gain (and resonance) destinations
constexpr double resPointsPerUnit = 100.0;

// Envelope follower -> octaves (spec r2 §6.3 target table): raw = 6.4 A T_f(|a|)/T_f(0.5) octaves with the
// follower taper k_f = 4.26 (so +100 % gives ~9.4x the +50 % slope: +0.60 oct at A = 0.01). Positive offsets
// saturate softly at +1.82 oct (raw / (1 + (raw/1.82)^4)^(1/4)); negative offsets grow slightly faster than
// linear above ~2.5 oct (x (1 + 0.25 (raw/3.26)^4)), reaching -4.1 oct at A = 0.51 for -50 %.
constexpr double followerOctPerUnit = 6.4, followerPositiveLimitOct = 1.82, taperFollower = 4.26;
// scaledAmount = sign(a) T_f(|a|)/T_f(0.5) (precomputed per block in the engine)
inline double followerOctavesScaled (double peak, double scaledAmount) noexcept
{
    const double raw = followerOctPerUnit * std::max (peak, 0.0) * std::abs (scaledAmount);
    if (scaledAmount >= 0.0)
    {
        const double r4 = std::pow (raw / followerPositiveLimitOct, 4.0);
        return raw / std::pow (1.0 + r4, 0.25);
    }
    const double r = raw / 3.26;
    return -raw * (1.0 + 0.25 * r * r * r * r);
}
inline double followerAmountScale (double amount) noexcept
{
    return taper (amount, taperFollower) / taper (0.5, taperFollower);
}
inline double followerOctaves (double peak, double amount) noexcept
{
    return followerOctavesScaled (peak, followerAmountScale (amount));
}

// Amp-type gain under modulation: g = max(0, g_knob + 0.0647 (e^(1.502 m) - 1)), m in sequencer scale.
inline double modulatedAmpGain (double knobGain, double m) noexcept
{
    const double g = knobGain + 0.0647 * (std::exp (1.502 * m) - 1.0);
    return clampd (g, 0.0, 4.0);
}

// ---------------------------------------------------------------------------------------------
// §7 mixer: dry = (1 - m)/2, wet = (1 + m)/2 with m = Mix/100.
inline double mixDry (double mixPercent) noexcept { return (1.0 - mixPercent / 100.0) * 0.5; }
inline double mixWet (double mixPercent) noexcept { return (1.0 + mixPercent / 100.0) * 0.5; }

// ---------------------------------------------------------------------------------------------
// §8 delay mix table (dry, first tap) at m = -100, -75, ..., +100, interpolated with a Catmull-Rom spline.
inline constexpr std::array<double, 9> delayMixDryTable { { 1.000, 0.971, 0.888, 0.761, 0.603, 0.432, 0.264, 0.116, 0.000 } };
inline constexpr std::array<double, 9> delayMixWetTable { { 0.000, 0.193, 0.368, 0.509, 0.603, 0.646, 0.639, 0.586, 0.500 } };

inline double interpolateTable (const std::array<double, 9>& t, double mixPercent) noexcept
{
    const double pos = clampd ((mixPercent + 100.0) / 25.0, 0.0, 8.0);
    const int i = std::min (int (pos), 7);
    const double f = pos - i;
    auto at = [&t] (int k) {
        if (k < 0) return 2.0 * t[0] - t[1];
        if (k > 8) return 2.0 * t[8] - t[7];
        return t[size_t (k)];
    };
    const double p0 = at (i - 1), p1 = at (i), p2 = at (i + 1), p3 = at (i + 2);
    return p1 + 0.5 * f * (p2 - p0 + f * (2.0 * p0 - 5.0 * p1 + 4.0 * p2 - p3 + f * (3.0 * (p1 - p2) + p3 - p0)));
}
inline double delayMixDry (double mixPercent) noexcept { return std::max (0.0, interpolateTable (delayMixDryTable, mixPercent)); }
inline double delayMixWet (double mixPercent) noexcept { return std::max (0.0, interpolateTable (delayMixWetTable, mixPercent)); }

// ---------------------------------------------------------------------------------------------
// §9 drive: small-signal gain +0.073 dB per %, into c tanh(u / c). The spec's 0.2 / 0.4 / 0.8 / 2.7 dB at
// -3 dBFS are swept-sine readings (~0.65x a steady sine); with a steady -3 dBFS 1 kHz sine the reference
// compresses 0.31 / 0.61 / 1.17 / 4.18 dB at 0 / 25 / 50 / 100 % (verifier). The ceiling
// c = 1.880 e^(-0.006678 d) gives 0.30 / 0.62 / 1.23 / 4.09 dB steady.
inline double driveGain (double drivePercent) noexcept { return dbToGain (0.073 * drivePercent); }
inline double driveCeiling (double drivePercent) noexcept { return 1.880 * std::exp (-0.006678 * drivePercent); }

// Crush (spec r2 §9): hold length N = 1 + 64 (Crush/100)^2 samples at 48 kHz, i.e. a hold time of N / 48000 s
// at any host rate (rate f_h = 48000 / N); 0 = off.
inline double crushHoldSamples48k (double crush) noexcept
{
    const double c = clampd (crush, 0.0, 100.0) / 100.0;
    return 1.0 + 64.0 * c * c;
}
inline double crushRateHz (double crush) noexcept { return 48000.0 / crushHoldSamples48k (crush); }

// ---------------------------------------------------------------------------------------------
// §4 fixed chain
constexpr double wetHighPassHz = 21.3;    // 2nd-order Butterworth: -3.6 dB at 20 Hz, -0.14 dB at 50 Hz
constexpr double wetGainDb = 0.35;

} // namespace craft::mf::law
