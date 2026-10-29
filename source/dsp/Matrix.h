// Craft Audio stereo imager - broadband matrix stage (spec r1 section 4, steps 1-6 and 8).
#pragma once

#include <cmath>

namespace craft::dsp
{

enum class Variant
{
    Full,     // all controls, shuffler available
    Imager,   // Shuffle / Frequency / Bass Trim fixed at neutral
    MsMatrix  // fixed LR -> MS matrix, no controls
};

// Physical (already quantised) parameter values.
struct Settings
{
    double gainDb       = 0.0;
    double width        = 1.0;
    double rotationDeg  = 0.0;
    double asymmetryDeg = 0.0;
    double shuffle      = 1.0;
    double frequencyHz  = 650.0;
    double bassTrimDb   = 0.0;
    bool swap      = false;
    bool polarity1 = false; // true = channel 1 inverted
    bool polarity2 = false;
    bool msInput   = false; // false = LR input, true = MS input
};

// 2x2 matrix, rows = outputs, columns = inputs.
struct Matrix2
{
    double a00 = 1.0, a01 = 0.0, a10 = 0.0, a11 = 1.0;

    static Matrix2 identity() noexcept { return {}; }

    Matrix2 operator* (const Matrix2& o) const noexcept
    {
        return { a00 * o.a00 + a01 * o.a10, a00 * o.a01 + a01 * o.a11,
                 a10 * o.a00 + a11 * o.a10, a10 * o.a01 + a11 * o.a11 };
    }
};

constexpr double pi = 3.14159265358979323846;

inline double degToRad (double d) noexcept { return d * (pi / 180.0); }

// Exact sine/cosine for the multiples of 90 degrees so neutral and extreme settings
// produce exact 0 / +-1 coefficients (keeps the neutral path bit-exact).
inline void sinCosDeg (double deg, double& s, double& c) noexcept
{
    const double q = deg / 90.0;
    if (q == std::floor (q))
    {
        const int k = ((static_cast<int> (q) % 4) + 4) % 4;
        static constexpr double sv[] = { 0.0, 1.0, 0.0, -1.0 };
        static constexpr double cv[] = { 1.0, 0.0, -1.0, 0.0 };
        s = sv[k];
        c = cv[k];
        return;
    }
    s = std::sin (degToRad (deg));
    c = std::cos (degToRad (deg));
}

// Width gains (step 4): returns (gM, gS)
inline void widthGains (double w, double& gM, double& gS) noexcept
{
    if (w <= 1.0)
    {
        gM = 1.0;
        gS = w;
    }
    else
    {
        const double g = std::sqrt ((5.0 + 3.0 / (w * w)) / 8.0);
        gM = g;
        gS = g * w;
    }
}

inline double dbToGain (double dB) noexcept { return dB == 0.0 ? 1.0 : std::pow (10.0, dB / 20.0); }

// Broadband matrix B from the two input channels to (M, S) after steps 1-6 and the gain (step 8).
// (Gain is a scalar on both M and S and therefore commutes with the shuffler.)
inline Matrix2 broadbandToMS (const Settings& p, Variant v) noexcept
{
    if (v == Variant::MsMatrix)
        return { 0.5, 0.0, 0.0, 0.5 }; // E * B = [[.5 .5][.5 -.5]]

    // 1. polarity
    const Matrix2 pol { p.polarity1 ? -1.0 : 1.0, 0.0, 0.0, p.polarity2 ? -1.0 : 1.0 };
    // 2. swap
    const Matrix2 swp = p.swap ? Matrix2 { 0.0, 1.0, 1.0, 0.0 } : Matrix2::identity();
    // 3. decode
    const Matrix2 dec = p.msInput ? Matrix2::identity() : Matrix2 { 0.5, 0.5, 0.5, -0.5 };
    // 4. width
    double gM, gS;
    widthGains (p.width, gM, gS);
    const Matrix2 wid { gM, 0.0, 0.0, gS };
    // 5. asymmetry: M' = M - sin(a) S, S' = cos(a) S
    double sa, ca;
    sinCosDeg (p.asymmetryDeg, sa, ca);
    const Matrix2 asy { 1.0, -sa, 0.0, ca };
    // 6. rotation: M' = cos M + sin S, S' = -sin M + cos S
    double sr, cr;
    sinCosDeg (p.rotationDeg, sr, cr);
    const Matrix2 rot { cr, sr, -sr, cr };
    // 8. gain
    const double g = dbToGain (p.gainDb);
    const Matrix2 gain { g, 0.0, 0.0, g };

    return gain * (rot * (asy * (wid * (dec * (swp * pol)))));
}

// Encode (step 9): L = M + S, R = M - S
inline Matrix2 encodeMatrix() noexcept { return { 1.0, 1.0, 1.0, -1.0 }; }

// Full broadband LR -> LR matrix (shuffler excluded); used for the vector display and tests.
inline Matrix2 broadbandLR (const Settings& p, Variant v) noexcept
{
    return encodeMatrix() * broadbandToMS (p, v);
}

} // namespace craft::dsp
