// Craft Punch Compressor - static curve with the measured correction table and the automatic makeup
// (spec r1 sections 4 and 5). Host-independent, header only.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>

namespace craft::punch::curve
{

// Columns of the correction table: d = level above threshold (dB).
inline constexpr std::array<double, 11> dGrid { -2.0, -1.0, 0.0, 1.0, 2.0, 3.0, 6.0, 9.0, 12.0, 18.0, 24.0 };

// Ratio-4 rows of the section 4 table at T = -6, -12, -24, -36, -48 dB. Cells the spec leaves empty ("—") are
// filled as documented in IMPLEMENTATION_NOTES.md (assumption A1):
//  - T -6,  d 9 ... 24: the next row's values (-12 row for 9/12, -24 row for 18/24); the -6 row ends at 0 dBFS.
//  - T -12, d -2/-1: 0 (as both neighbouring rows); d 1/2: linear in T between the -6 and -24 rows;
//           d 18/24: the -24 row.
//  - T -36, d -2/-1: the T-interpolated knee shape scaled to the row's own d = 0 value (+0.16);
//           d 1/2: linear in d between the row's d = 0 and d = 3 cells.
inline constexpr std::array<double, 5> tRows { -6.0, -12.0, -24.0, -36.0, -48.0 };
inline constexpr std::array<std::array<double, 11>, 5> delta4 { {
    { -0.00, 0.00, 0.00, -0.32, -0.42, -0.46, -0.52, -0.54, -0.55, -0.56, -0.57 },
    { 0.00, 0.00, 0.00, -0.30, -0.40, -0.46, -0.52, -0.54, -0.55, -0.56, -0.57 },
    { 0.00, 0.00, 0.01, -0.26, -0.35, -0.41, -0.48, -0.52, -0.54, -0.56, -0.57 },
    { 0.014, 0.078, 0.16, 0.04, -0.08, -0.20, -0.33, -0.41, -0.46, -0.52, -0.55 },
    { 0.09, 0.51, 1.03, 0.85, 0.69, 0.56, 0.23, 0.00, -0.17, -0.37, -0.48 },
} };

// T = -24 rows for low ratios, indexed by u = 1 - 1/R (R = 1.25, 1.5, 2).
inline constexpr std::array<double, 3> lowRatioU { 0.2, 1.0 / 3.0, 0.5 };
inline constexpr std::array<std::array<double, 11>, 3> delta24Low { {
    { -0.00, -0.00, 0.00, -0.14, -0.22, -0.29, -0.39, -0.44, -0.47, -0.51, -0.53 },
    { -0.00, 0.00, 0.00, -0.19, -0.29, -0.35, -0.44, -0.49, -0.51, -0.54, -0.56 },
    { 0.00, -0.00, 0.01, -0.23, -0.33, -0.39, -0.47, -0.51, -0.53, -0.56, -0.57 },
} };
inline constexpr int row24 = 2; // index of T = -24 in tRows

// Linear interpolation of a row at d (clamped to the grid ends; callers handle d < -2).
inline double rowAt (const std::array<double, 11>& row, double d) noexcept
{
    if (d <= dGrid.front()) return row.front();
    if (d >= dGrid.back()) return row.back();
    size_t k = 0;
    while (k + 2 < dGrid.size() && dGrid[k + 1] <= d)
        ++k;
    const double f = (d - dGrid[k]) / (dGrid[k + 1] - dGrid[k]);
    return row[k] + f * (row[k + 1] - row[k]);
}

// Delta_4(T, d): the Ratio-4 rows, linear in T (clamped to -6 ... -48) and d.
inline double delta4At (double T, double d) noexcept
{
    const double t = std::clamp (T, -48.0, -6.0);
    size_t k = 0;
    while (k + 2 < tRows.size() && tRows[k + 1] >= t)
        ++k;
    const double f = (tRows[k] - t) / (tRows[k] - tRows[k + 1]);
    const double a = rowAt (delta4[k], d), b = rowAt (delta4[k + 1], d);
    return a + f * (b - a);
}

// Delta_24(d, R) with u = 1 - 1/R: R >= 2 -> the Ratio-4 row; R < 2 -> the 1.25 / 1.5 / 2 rows, linear in u;
// below R = 1.25 linear towards 0 at R = 1 (assumption A2: neutral at Ratio 1).
inline double delta24At (double d, double u) noexcept
{
    if (u >= 0.5) return rowAt (delta4[row24], d);
    if (u <= lowRatioU[0]) return u <= 0.0 ? 0.0 : (u / lowRatioU[0]) * rowAt (delta24Low[0], d);
    const size_t k = u < lowRatioU[1] ? 0 : 1;
    const double f = (u - lowRatioU[k]) / (lowRatioU[k + 1] - lowRatioU[k]);
    const double a = rowAt (delta24Low[k], d), b = rowAt (delta24Low[k + 1], d);
    return a + f * (b - a);
}

// Combination rule (normative): Delta(T,d,R) = Delta24(d,R) + [Delta4(T,d) - Delta4(-24,d)] * (1 - 1/R) / 0.75;
// Delta = 0 below d = -2, the d = 24 value above d = 24.
inline double delta (double T, double d, double u) noexcept
{
    if (d < -2.0 || u <= 0.0)
        return 0.0;
    return delta24At (d, u) + (delta4At (T, d) - delta4At (-24.0, d)) * u / 0.75;
}

// Target gain reduction (dB, >= 0) for detector level D (dBFS), threshold T and u = 1 - 1/R.
// d <= 0: the knee correction only; d > 0: d (1 - 1/R) + Delta.
inline double gainReduction (double D, double T, double u) noexcept
{
    const double d = D - T;
    if (d < -2.0 || u <= 0.0)
        return 0.0;
    const double gr = (d > 0.0 ? d * u : 0.0) + delta (T, d, u);
    return gr > 0.0 ? gr : 0.0;
}

// --- automatic makeup (section 5) -------------------------------------------------------------------------
inline constexpr std::array<double, 9> kU { 0.0, 0.2, 1.0 / 3.0, 0.5, 2.0 / 3.0, 0.75, 0.875, 0.95, 0.98 };
inline constexpr std::array<double, 9> kV { 0.0, 0.0108, 0.1056, 0.2166, 0.3191, 0.3670, 0.4351, 0.4736, 0.4886 };
inline constexpr std::array<double, 7> sRel { 3.0, 10.0, 30.0, 100.0, 300.0, 1000.0, 3000.0 };
inline constexpr std::array<double, 7> sVal { 0.2476, 0.6596, 0.8730, 0.9628, 0.9902, 1.0000, 1.0029 };

template <size_t N>
inline double interp (const std::array<double, N>& xs, const std::array<double, N>& ys, double x) noexcept
{
    if (x <= xs.front()) return ys.front();
    if (x >= xs.back()) return ys.back();
    size_t k = 0;
    while (k + 2 < N && xs[k + 1] <= x)
        ++k;
    const double f = (x - xs[k]) / (xs[k + 1] - xs[k]);
    return ys[k] + f * (ys[k + 1] - ys[k]);
}

inline double makeupK (double u) noexcept { return u <= 0.0 ? 0.0 : interp (kU, kV, u); }

// s(Rel): linear in log10 Rel, clamped outside 3 ... 3000 ms.
inline double makeupS (double relMs) noexcept
{
    const double l = std::log10 (std::max (relMs, 1.0e-3));
    if (relMs <= sRel.front()) return sVal.front();
    if (relMs >= sRel.back()) return sVal.back();
    size_t k = 0;
    while (k + 2 < sRel.size() && sRel[k + 1] <= relMs)
        ++k;
    const double l0 = std::log10 (sRel[k]), l1 = std::log10 (sRel[k + 1]);
    return sVal[k] + (l - l0) / (l1 - l0) * (sVal[k + 1] - sVal[k]);
}

// Mk(T, R, Rel) = k(R) * max(0, -T - 9) * s(Rel)  [dB]
inline double makeupDb (double T, double u, double relMs) noexcept
{
    const double span = -T - 9.0;
    if (span <= 0.0 || u <= 0.0)
        return 0.0;
    return makeupK (u) * span * makeupS (relMs);
}

inline double ratioToU (double R) noexcept { return R <= 1.0 ? 0.0 : 1.0 - 1.0 / R; }

} // namespace craft::punch::curve
