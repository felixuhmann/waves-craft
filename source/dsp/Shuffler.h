// Craft Audio stereo imager - Blumlein-style shuffler section (spec r1 section 5).
//
// Each of M and S passes a first-order section with DC gain G and HF gain -1, designed in
// continuous time as H(s) = (sqrt(G) wb - s) / (wb / sqrt(G) + s) and discretised with the
// bilinear transform at the actual sample rate, with the corner pre-warped so that the
// -90 degree / |H| = sqrt(G) point lies exactly at the Frequency parameter.
#pragma once

#include <cmath>
#include "Matrix.h"

namespace craft::dsp
{

// Shuffler active iff Shuffle != 1.0 or Bass Trim != 0.0 dB (after quantisation).
inline bool shufflerActive (double shuffle, double bassTrimDb) noexcept
{
    return std::lround (shuffle * 10.0) != 10 || std::lround (bassTrimDb * 10.0) != 0;
}

// Low-frequency gains: G_M = T sqrt((1 + 1/s^2)/2), G_S = s G_M
inline void shufflerGains (double s, double bassTrimDb, double& gM, double& gS) noexcept
{
    const double t = dbToGain (bassTrimDb);
    gM = t * std::sqrt ((1.0 + 1.0 / (s * s)) / 2.0);
    gS = s * gM;
}

struct FirstOrderCoeffs
{
    double b0 = 1.0, b1 = 0.0, a1 = 0.0; // y = b0 x + b1 x[n-1] - a1 y[n-1]
};

// H(z) = [g k (1 + z^-1) - (1 - z^-1)] / [(k/g)(1 + z^-1) + (1 - z^-1)], k = tan(pi f / fs), g = sqrt(G)
inline FirstOrderCoeffs shufflerSectionCoeffs (double G, double frequencyHz, double sampleRate) noexcept
{
    const double k = std::tan (pi * frequencyHz / sampleRate);
    const double g = std::sqrt (G);
    const double gk = g * k, kg = k / g;
    const double a0 = kg + 1.0;
    return { (gk - 1.0) / a0, (gk + 1.0) / a0, (kg - 1.0) / a0 };
}

// Transposed direct form II first-order section (double precision).
class FirstOrderSection
{
public:
    void setCoefficients (const FirstOrderCoeffs& c) noexcept { coeffs = c; }
    void reset() noexcept { z1 = 0.0; }

    double process (double x) noexcept
    {
        const double y = coeffs.b0 * x + z1;
        z1 = coeffs.b1 * x - coeffs.a1 * y;
        return y;
    }

private:
    FirstOrderCoeffs coeffs;
    double z1 = 0.0;
};

} // namespace craft::dsp
