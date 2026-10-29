// Craft Console E - static dynamics curves (spec r1 sections 6, 7 and Appendix B).
#pragma once

#include <algorithm>
#include <array>
#include <cmath>

namespace craft::consolee::dsp
{

// ---- compressor: Appendix B.1 gain reduction GR_R(u), u = L - 0.864 T --------------------------------
namespace comp
{
inline constexpr double thresholdScale = 0.864;
inline constexpr int numU = 31;           // u = -40 ... +20 in 2 dB steps
inline constexpr double uMin = -40.0, uStep = 2.0;
inline constexpr int numRatios = 8;

// 1 - 1/R of the tabulated ratios 1.5, 2, 3, 4, 8, 12, 20, inf
inline constexpr std::array<double, numRatios> ratioX { 1.0 - 1.0 / 1.5, 0.5, 1.0 - 1.0 / 3.0, 0.75, 0.875, 1.0 - 1.0 / 12.0, 0.95, 1.0 };

// NaN-free table: the entries the appendix leaves open (u > +10 for R = 1.5, 3, 12, 20) are extrapolated
// linearly with the slope of the last two tabulated points.
inline constexpr double gr[numRatios][numU] = {
    { 0, 0, 0, 0, 0, 0, 0, 0, -0.00, -0.00, -0.02, -0.06, -0.13, -0.22, -0.34, -0.48, -0.64, -0.82, -1.04, -1.27, -1.51, -1.75, -2.05, -2.35, -2.66, -2.98,
      -3.30, -3.62, -3.94, -4.26, -4.58 },
    { 0, 0, 0, 0, 0, 0, 0, 0, -0.01, -0.06, -0.21, -0.43, -0.76, -1.15, -1.60, -2.09, -2.66, -3.29, -3.93, -4.56, -5.23, -5.96, -6.77, -7.54, -8.31, -9.07,
      -9.89, -10.73, -11.59, -12.44, -13.25 },
    { 0, 0, 0, 0, 0, 0, 0, -0.01, -0.06, -0.34, -0.77, -1.31, -2.03, -2.78, -3.57, -4.55, -5.51, -6.45, -7.45, -8.50, -9.59, -10.77, -11.95, -13.06, -14.17, -15.37,
      -16.57, -17.77, -18.97, -20.17, -21.37 },
    { 0, 0, 0, 0, 0, 0, 0, -0.04, -0.15, -0.54, -1.10, -1.79, -2.65, -3.56, -4.51, -5.65, -6.75, -7.84, -9.10, -10.32, -11.57, -12.90, -14.22, -15.47, -16.79, -18.18,
      -19.62, -20.94, -22.24, -23.58, -25.02 },
    { 0, 0, 0, 0, 0, 0, -0.00, -0.12, -0.40, -1.03, -1.84, -2.76, -3.85, -5.03, -6.29, -7.64, -8.97, -10.32, -11.84, -13.37, -14.90, -16.45, -17.92, -19.45, -21.11, -22.77,
      -24.36, -25.90, -27.50, -29.15, -30.87 },
    { 0, 0, 0, 0, 0, -0.00, -0.01, -0.18, -0.53, -1.26, -2.13, -3.12, -4.28, -5.56, -6.93, -8.32, -9.72, -11.14, -12.75, -14.40, -16.06, -17.67, -19.20, -20.81, -22.59, -24.34,
      -26.09, -27.84, -29.59, -31.34, -33.09 },
    { 0, 0, 0, 0, 0, -0.00, -0.01, -0.24, -0.65, -1.45, -2.42, -3.51, -4.71, -6.08, -7.55, -9.00, -10.48, -12.00, -13.67, -15.40, -17.17, -18.84, -20.42, -22.13, -23.99, -25.80,
      -27.61, -29.42, -31.23, -33.04, -34.85 },
    { 0, 0, 0, 0, 0, -0.00, -0.04, -0.38, -1.01, -2.04, -3.30, -4.71, -6.16, -7.77, -9.54, -11.25, -13.03, -14.91, -16.78, -18.74, -20.72, -22.57, -24.32, -26.33, -28.43, -30.38,
      -31.87, -33.78, -35.84, -37.93, -40.02 },
};

// Appendix B.2 make-up gain at T = +10, 0, -10, -20 (entries left open in the appendix are filled as described in
// IMPLEMENTATION_NOTES.md: the -20 dB column scales the -10 dB column by a factor interpolated in 1 - 1/R).
inline constexpr std::array<double, 4> makeupT { 10.0, 0.0, -10.0, -20.0 };
inline constexpr double makeup[numRatios][4] = {
    { 0, 0.55, 1.10, 2.558 }, // 1.5
    { 0, 0.93, 1.87, 4.35 },  // 2
    { 0, 1.34, 2.67, 5.714 }, // 3
    { 0, 1.49, 2.98, 6.10 },  // 4
    { 0, 1.85, 3.70, 7.08 },  // 8
    { 0, 1.92, 3.83, 7.228 }, // 12
    { 0, 2.05, 4.10, 7.648 }, // 20
    { 0, 2.38, 4.77, 8.74 },  // inf
};

// Interpolation weight over the ratio axis (x = 1 - 1/R); x below the first row blends with "no reduction".
inline void ratioWeights (double x, int& i, double& w, bool& belowFirst) noexcept
{
    belowFirst = x < ratioX[0];
    if (belowFirst) { i = 0; w = x / ratioX[0]; return; }
    i = 0;
    while (i < numRatios - 2 && ratioX[size_t (i + 1)] <= x)
        ++i;
    w = std::clamp ((x - ratioX[size_t (i)]) / (ratioX[size_t (i + 1)] - ratioX[size_t (i)]), 0.0, 1.0);
}

inline double grRow (int row, double u) noexcept
{
    const double p = (u - uMin) / uStep;
    if (p <= 0.0) return gr[row][0];
    if (p >= numU - 1)
    {
        const double slope = (gr[row][numU - 1] - gr[row][numU - 2]) / uStep;
        return gr[row][numU - 1] + slope * (u - (uMin + uStep * (numU - 1)));
    }
    const int k = int (p);
    const double t = p - k;
    return gr[row][k] + t * (gr[row][k + 1] - gr[row][k]);
}

inline double makeupRow (int row, double T) noexcept
{
    T = std::clamp (T, -20.0, 10.0);
    int k = 0;
    while (k < 2 && T < makeupT[size_t (k + 1)])
        ++k;
    const double t = (makeupT[size_t (k)] - T) / (makeupT[size_t (k)] - makeupT[size_t (k + 1)]);
    return makeup[row][k] + t * (makeup[row][k + 1] - makeup[row][k]);
}

// Static compressor gain in dB (make-up included) for a detector level L (dBFS, 1 kHz sine calibrated),
// threshold control T (dB) and inverse ratio 1/R.
inline double staticGainDb (double levelDb, double thresholdDb, double invRatio) noexcept
{
    const double x = 1.0 - std::clamp (invRatio, 0.0, 1.0);
    if (x <= 0.0)
        return 0.0;
    const double u = levelDb - thresholdScale * thresholdDb;
    int i;
    double w;
    bool below;
    ratioWeights (x, i, w, below);
    if (below)
        return w * (grRow (0, u) + makeupRow (0, thresholdDb));
    return (1 - w) * (grRow (i, u) + makeupRow (i, thresholdDb)) + w * (grRow (i + 1, u) + makeupRow (i + 1, thresholdDb));
}

inline double makeupDb (double thresholdDb, double invRatio) noexcept
{
    return staticGainDb (-1000.0, thresholdDb, invRatio);
}
} // namespace comp

// ---- expander / gate (section 7) -------------------------------------------------------------------
namespace expander
{
// Spec r2 section 7.1: expander gain (dB) at range 40 for thresholds -30, -20, -10, 0 and 1 kHz sine levels
// -57 ... -21 dBFS (3 dB steps); 0 dB above the table. Interpolated linearly in level and threshold.
inline constexpr std::array<double, 4> curveThresholds { -30.0, -20.0, -10.0, 0.0 };
inline constexpr double curveLevel0 = -57.0, curveStep = 3.0;
inline constexpr int curveN = 13;
inline constexpr double curve40[4][curveN] = {
    { -11.0, -7.6, -4.1, -0.5, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { -20.3, -16.9, -13.4, -9.9, -6.3, -2.6, 0, 0, 0, 0, 0, 0, 0 },
    { -30.2, -26.8, -23.3, -19.8, -16.1, -12.5, -8.7, -4.9, -1.1, 0, 0, 0, 0 },
    { -40.5, -37.1, -33.6, -30.1, -26.5, -22.8, -19.0, -15.2, -11.4, -7.6, -4.0, -0.4, 0 },
};
// level shift of the curve per dB of threshold outside -30 ... 0 (slope of the knee between the rows)
inline constexpr double kneeShiftPerDb = 0.8;
// knees (level where the expansion starts) of the four rows, dBFS
inline constexpr std::array<double, 4> curveKnees { -47.58, -39.89, -32.13, -23.67 };
// smaller ranges start the expansion slightly later: gain offset (dB) by range (rows thr -20 / -10, ranges 5 ... 30)
inline constexpr std::array<double, 7> offsetRange { 0, 5, 10, 15, 20, 30, 40 };
inline constexpr std::array<double, 7> offsetDb { 0.8, 0.8, 0.5, 0.43, 0.27, 0.1, 0.0 };
// the gate opens slightly above the expander knee (thr -20: between -39 and -36; thr -10: between -33 and -30 dBFS)
inline constexpr double gateAboveKnee = 1.5;

namespace detail
{
inline double rowGain (int row, double level) noexcept
{
    const double p = (level - curveLevel0) / curveStep;
    if (p >= curveN - 1) return 0.0;
    if (p <= 0.0) return curve40[row][0] + (curve40[row][1] - curve40[row][0]) * p; // extrapolate the slope
    const int k = int (p);
    return curve40[row][k] + (p - k) * (curve40[row][k + 1] - curve40[row][k]);
}
} // namespace detail

// range-40 curve for any threshold (interpolated between rows, level-shifted outside -30 ... 0)
inline double curveGain (double levelDb, double thresholdDb) noexcept
{
    if (thresholdDb > 0.0)
        return detail::rowGain (3, levelDb - kneeShiftPerDb * thresholdDb);
    if (thresholdDb < -30.0)
        return detail::rowGain (0, levelDb - kneeShiftPerDb * (thresholdDb + 30.0));
    const int i = std::min (int ((thresholdDb + 30.0) / 10.0), 2);
    const double w = (thresholdDb - curveThresholds[size_t (i)]) / 10.0;
    return (1 - w) * detail::rowGain (i, levelDb) + w * detail::rowGain (i + 1, levelDb);
}

inline double kneeDbfs (double thresholdDb) noexcept
{
    if (thresholdDb > 0.0) return curveKnees[3] + kneeShiftPerDb * thresholdDb;
    if (thresholdDb < -30.0) return curveKnees[0] + kneeShiftPerDb * (thresholdDb + 30.0);
    const int i = std::min (int ((thresholdDb + 30.0) / 10.0), 2);
    const double w = (thresholdDb - curveThresholds[size_t (i)]) / 10.0;
    return (1 - w) * curveKnees[size_t (i)] + w * curveKnees[size_t (i + 1)];
}

inline double gateSwitchDbfs (double thresholdDb) noexcept { return kneeDbfs (thresholdDb) + gateAboveKnee; }

// Expander floor (dB of attenuation) for a range control value: measured 5 -> 0.8, 10 -> 7.7, 15 -> 14.5,
// 20 -> 21.4, then +1.38 dB per dB (specification choice).
inline double expanderFloor (double range) noexcept
{
    static constexpr double r[] = { 0, 5, 10, 15, 20 };
    static constexpr double f[] = { 0, 0.8, 7.7, 14.5, 21.4 };
    range = std::max (0.0, range);
    if (range >= 20.0) return 21.4 + 1.38 * (range - 20.0);
    int i = std::min (int (range / 5.0), 3);
    return f[i] + (f[i + 1] - f[i]) * (range - r[i]) / 5.0;
}

// Gate floor: 0 -> 0, 20 -> 25.4, 40 -> 52.7 dB (linear in dB between).
inline double gateFloor (double range) noexcept
{
    range = std::clamp (range, 0.0, 40.0);
    return range <= 20.0 ? 25.4 * range / 20.0 : 25.4 + (52.7 - 25.4) * (range - 20.0) / 20.0;
}

// Static expander gain (dB, <= 0) for envelope level env (dBFS).
inline double staticGainDb (double envDb, double thresholdDb, double rangeDb) noexcept
{
    double offset = offsetDb.back();
    for (size_t i = 0; i + 1 < offsetRange.size(); ++i)
        if (rangeDb <= offsetRange[i + 1])
        {
            offset = offsetDb[i] + (offsetDb[i + 1] - offsetDb[i]) * (rangeDb - offsetRange[i]) / (offsetRange[i + 1] - offsetRange[i]);
            break;
        }
    const double g = curveGain (envDb, thresholdDb);
    if (g >= 0.0)
        return 0.0;
    return std::max (-expanderFloor (rangeDb), std::min (0.0, g + offset));
}
} // namespace expander

} // namespace craft::consolee::dsp
