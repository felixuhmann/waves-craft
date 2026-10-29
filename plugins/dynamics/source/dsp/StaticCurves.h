// Craft Dynamics - static gain laws (spec r1 sections 5 and 6, Appendix A/B). Host-independent.
#pragma once

#include <algorithm>
#include <cmath>

#include "CurveTables.h"
#include "Parameters.h"

namespace craft::dynamics::dsp::curves
{

// Detector reading of a steady 1 kHz sine with Comp Attack 2 ms / Release 50 ms relative to its peak level
// (Appendix A: x_det = sine peak - T + 1.277 dB). Used to estimate the output peak for the expansion limit.
inline constexpr double sinePeakToDetectorDb = 1.277;

// Gate open/close comparisons are made against threshold + 1.85 dB (spec 6).
inline constexpr double gateThresholdOffsetDb = 1.85;

// Reference x for PeakRef: the curve value for a sine whose peak is 2.43 dBFS (spec 5).
inline constexpr double peakRefXOffsetDb = 3.71;
inline constexpr double peakRefCapDb = 40.0;

// Positions below/above this distance from 1.00:1 are expansion/compression; within it the gain is exactly 0 dB.
inline constexpr double neutralTolerance = 5.0e-4;

inline bool isNeutral (double r) noexcept { return std::abs (r - ratio::neutralPos) <= neutralTolerance; }
inline bool isExpansion (double r) noexcept { return r < ratio::neutralPos - neutralTolerance; }

// One Appendix A column, linear in x; flat beyond the table, except for the limiter-like columns (20.62:1,
// 50:1) which continue with slope -1.
inline double columnDb (int col, double x) noexcept
{
    using namespace tables;
    const double fi = (x - curveX0) * 0.5;
    if (! (fi > 0.0))
        return curveDb[0][col];
    constexpr int last = numCurveRows - 1;
    if (fi >= double (last))
    {
        double v = curveDb[last][col];
        if (col == 10 || col == 11)
            v -= x - (curveX0 + 2.0 * last);
        return v;
    }
    const int i = int (fi);
    const double t = fi - double (i);
    return curveDb[i][col] + t * (curveDb[i + 1][col] - curveDb[i][col]);
}

// f_R(x): unlimited static curve for ratio position r (linear interpolation in r between the 14 tabulated
// positions, with an exact 0 dB curve at 1.00:1).
inline double compressorCurveDb (double r, double x) noexcept
{
    using namespace tables;
    r = clamp01 (r);
    if (isNeutral (r))
        return 0.0;
    const double n = ratio::neutralPos;
    // neighbours: positions are curvePositions[] plus the neutral point between 0.20 (col 3) and 0.25 (col 4)
    if (r < n && r > curvePositions[3])
        return columnDb (3, x) * (n - r) / (n - curvePositions[3]);
    if (r > n && r < curvePositions[4])
        return columnDb (4, x) * (r - n) / (curvePositions[4] - n);
    int k = 0;
    while (k < numCurveCols - 2 && r > curvePositions[k + 1])
        ++k;
    const double t = (r - curvePositions[k]) / (curvePositions[k + 1] - curvePositions[k]);
    const double a = columnDb (k, x);
    return t <= 0.0 ? a : a + t * (columnDb (k + 1, x) - a);
}

// Expansion output limit (spec 5 / 10.3): maps the estimated unlimited output peak (dBFS) to the limited one.
inline double outputLimitDb (double y) noexcept
{
    using namespace tables;
    if (y <= 0.0)
        return y;
    if (y >= double (numLimitPoints - 1))
        return std::min (limitCeilingDb, limitOutDb[numLimitPoints - 1] + (y - (numLimitPoints - 1)) * 0.001);
    const int i = int (y);
    const double t = y - double (i);
    return limitOutDb[i] + t * (limitOutDb[i + 1] - limitOutDb[i]);
}

// Output limit on the module's final output (spec r2 5 / 10.3 / 10.9): a positive gain G (dB, after PeakRef and
// Makeup) is reduced so that the estimated output peak e - 1.277 + G follows the limit law; never below 0 dB.
inline double limitFinalGainDb (double gainDb, double e, double instantPeakDb = -400.0) noexcept
{
    if (! (gainDb > 0.0))
        return gainDb;
    // output peak estimate: the detector's sine-peak reading, or the held instantaneous peak when that is higher
    // (onsets from silence, before the detector has caught up)
    const double y = std::max (e - sinePeakToDetectorDb, instantPeakDb) + gainDb;
    if (y <= 0.0)
        return gainDb;
    return gainDb - std::min (gainDb, y - outputLimitDb (y));
}

// Static compressor/expander gain in dB for detector level e: curve + PeakRef offset + Makeup, then the output limit.
inline double staticGainDb (double r, double e, double thresholdDb, bool peakRef, double makeupDb) noexcept;

// Compressor/expander curve gain in dB for detector level e and threshold T (LowRef, unlimited, no makeup).
inline double compressorGainDb (double r, double e, double thresholdDb) noexcept
{
    return compressorCurveDb (r, e - thresholdDb);
}

// PeakRef offset O(T, R) (spec 5, 10.4, review decision D1). Cancellation ratios: the -5.00:1 offsets are the
// measured table of 10.4 (interpolated in T); positions between -15.90:1 and -5.00:1 blend from the curve rule.
inline double peakRefOffsetDb (double r, double thresholdDb) noexcept
{
    r = clamp01 (r);
    auto rule = [thresholdDb] (double pos) {
        if (isNeutral (pos))
            return 0.0;
        return std::min (peakRefCapDb, -compressorCurveDb (pos, peakRefXOffsetDb - thresholdDb));
    };
    if (r <= 0.9)
        return rule (r);
    static constexpr double tT[4] = { -80.0, -60.0, -20.0, -10.0 };
    static constexpr double tO[4] = { 10.46, 10.44, 8.60, 5.77 };
    double table;
    const double T = thresholdDb;
    if (T <= tT[0])
        table = tO[0];
    else if (T >= tT[3])
        table = std::max (0.0, tO[3] + (T - tT[3]) * (tO[3] - tO[2]) / (tT[3] - tT[2]));
    else
    {
        int i = 0;
        while (T > tT[i + 1])
            ++i;
        table = tO[i] + (T - tT[i]) * (tO[i + 1] - tO[i]) / (tT[i + 1] - tT[i]);
    }
    const double t = (r - 0.9) / 0.1;
    return std::min (peakRefCapDb, rule (0.9) + t * (table - rule (0.9)));
}

inline double staticGainDb (double r, double e, double thresholdDb, bool peakRef, double makeupDb) noexcept
{
    double g = compressorCurveDb (r, e - thresholdDb) + makeupDb;
    if (peakRef)
        g += peakRefOffsetDb (r, thresholdDb);
    return limitFinalGainDb (g, e);
}

// Appendix B: expander unit gain h(x), x = e - GateOpen. Below the table: 1:2 expansion (slope 1); above: last value.
inline double expanderGainDb (double x) noexcept
{
    using namespace tables;
    if (x <= expanderX[0])
        return expanderDb[0] + (x - expanderX[0]);
    if (x >= expanderX[numExpanderPoints - 1])
        return expanderDb[numExpanderPoints - 1];
    int i = 0;
    while (x > expanderX[i + 1])
        ++i;
    const double t = (x - expanderX[i]) / (expanderX[i + 1] - expanderX[i]);
    return expanderDb[i] + t * (expanderDb[i + 1] - expanderDb[i]);
}

// Gate/expander module gain G = F + (1 - F) g, written so that g = 1 gives exactly 1.
inline double applyFloor (double g, double floorLinear) noexcept { return g + floorLinear * (1.0 - g); }

} // namespace craft::dynamics::dsp::curves
