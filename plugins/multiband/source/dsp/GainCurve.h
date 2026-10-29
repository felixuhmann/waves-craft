// Craft Multiband - static gain curve f(x; Knee, Range) (spec r2 section 4, review decision D6).
// Host-independent: no JUCE dependency.
//
// Band gain (dB) = Gain + Range * f(x), x = band level (sine-peak dBFS) - Threshold.
// f is built from the normative tables on the grid x = -36, -32, ..., +28 dB:
//   node(x) = KneeTable(x; Knee)                         (linear in Knee between the table rows)
//           + (1 - w) * [R05(x; Range) - R05(x; -12)]     (Range correction measured at Knee 0.50)
//           +      w  * [RH (x; Range) - RH (x; -12)]     (Hard-knee rows, w = (Knee - 0.50) / 0.25 in [0, 1])
// Range rows are interpolated linearly in Range. Between grid points the nodes are joined by a monotone cubic
// (Fritsch-Carlson PCHIP); outside the grid f decays to 0 (below) / rises smoothly to 1 (above).
#pragma once

#include <algorithm>
#include <array>
#include <cmath>

namespace craft::multiband
{

namespace curve
{
    inline constexpr int numX = 17;
    inline constexpr double xMin = -36.0, xStep = 4.0, xMax = 28.0;

    inline constexpr int numKneeRows = 6;
    inline constexpr std::array<double, numKneeRows> kneeValues { 0.00, 0.15, 0.30, 0.50, 0.71, 0.75 };
    // f(x) at Range -12 for each Knee row.
    inline constexpr double kneeTable[numKneeRows][numX] = {
        { .009, .013, .021, .033, .051, .078, .118, .174, .247, .339, .444, .555, .660, .752, .826, .882, .921 }, // Soft
        { .007, .010, .017, .026, .042, .066, .102, .157, .232, .328, .441, .558, .668, .762, .835, .889, .927 }, // 0.15
        { .004, .007, .011, .018, .031, .051, .085, .138, .216, .318, .437, .561, .675, .770, .843, .895, .932 }, // 0.30
        { .000, .001, .003, .006, .014, .030, .060, .113, .195, .304, .433, .564, .683, .779, .851, .902, .937 }, // 0.50
        { .000, .000, .000, .000, .000, .005, .033, .087, .173, .291, .429, .567, .690, .788, .859, .908, .941 }, // 0.71
        { .000, .000, .000, .000, .000, .001, .028, .082, .169, .289, .428, .568, .691, .789, .860, .909, .941 }, // Hard
    };

    // Range dependence at Knee 0.50.
    inline constexpr int numRangeRows = 8;
    inline constexpr std::array<double, numRangeRows> rangeValues { -24.0, -12.0, -8.0, -3.0, 3.0, 6.0, 12.0, 18.0 };
    inline constexpr double rangeTable[numRangeRows][numX] = {
        { .001, .002, .005, .011, .023, .046, .086, .146, .229, .331, .443, .555, .660, .750, .823, .879, .919 }, // -24
        { .000, .001, .003, .006, .014, .030, .060, .113, .195, .304, .433, .564, .683, .779, .851, .902, .937 }, // -12
        { .000, .001, .002, .005, .012, .026, .054, .105, .185, .297, .430, .567, .689, .787, .858, .908, .941 }, // -8
        { .000, .001, .002, .004, .010, .023, .048, .096, .176, .289, .427, .569, .696, .794, .865, .913, .945 }, // -3
        { .000, .001, .002, .004, .009, .020, .043, .089, .167, .283, .425, .572, .701, .800, .871, .917, .948 }, // +3
        { .000, .001, .001, .003, .008, .019, .042, .087, .165, .280, .424, .572, .702, .802, .872, .919, .949 }, // +6
        { .000, .001, .001, .003, .007, .018, .039, .084, .161, .278, .423, .572, .703, .803, .873, .920, .949 }, // +12
        { .000, .000, .001, .003, .007, .017, .039, .083, .162, .279, .424, .572, .701, .801, .872, .919, .949 }, // +18
    };

    // Hard-knee Range rows (section 4.1).
    inline constexpr int numHardRows = 3;
    inline constexpr std::array<double, numHardRows> hardRangeValues { -24.0, -12.0, 18.0 };
    inline constexpr double hardTable[numHardRows][numX] = {
        { .000, .000, .000, .000, .003, .023, .063, .126, .213, .321, .440, .558, .665, .757, .829, .884, .923 }, // -24
        { .000, .000, .000, .000, .000, .001, .028, .082, .169, .289, .428, .568, .691, .789, .860, .909, .941 }, // -12
        { .000, .000, .000, .000, .000, .000, .000, .000, .050, .214, .406, .584, .724, .824, .890, .932, .958 }, // +18
    };

    // Below the grid f decays by this factor per dB; above, the remaining distance to 1 shrinks per dB
    // (f(+32) = 0.95 at Range -12 / Knee 0.50, spec section 4).
    inline constexpr double lowDecayPerDb = 0.09;
    inline constexpr double highDecayPerDb = 0.0578;

    // Linear interpolation of a table column set at position v among sorted row keys.
    template <size_t N>
    inline void rowWeights (const std::array<double, N>& keys, double v, int& i0, double& t) noexcept
    {
        if (v <= keys[0]) { i0 = 0; t = 0.0; return; }
        if (v >= keys[N - 1]) { i0 = int (N) - 2; t = 1.0; return; }
        int i = 0;
        while (i < int (N) - 2 && v > keys[size_t (i + 1)])
            ++i;
        i0 = i;
        t = (v - keys[size_t (i)]) / (keys[size_t (i + 1)] - keys[size_t (i)]);
    }
} // namespace curve

class GainCurve
{
public:
    GainCurve() { set (0.5, -12.0); }

    // Knee: 0 (Soft) ... 0.75 (Hard); Range in dB. Recomputes the nodes only when a value changed.
    void set (double knee, double rangeDb) noexcept
    {
        if (knee == cachedKnee && rangeDb == cachedRange)
            return;
        cachedKnee = knee;
        cachedRange = rangeDb;
        using namespace curve;

        int ki; double kt;
        rowWeights (kneeValues, std::clamp (knee, 0.0, 0.75), ki, kt);
        int ri; double rt;
        rowWeights (rangeValues, rangeDb, ri, rt);
        int hi; double ht;
        rowWeights (hardRangeValues, rangeDb, hi, ht);
        const double w = std::clamp ((knee - 0.50) / 0.25, 0.0, 1.0);

        double prev = 0.0;
        for (int j = 0; j < numX; ++j)
        {
            const double base = kneeTable[ki][j] + kt * (kneeTable[ki + 1][j] - kneeTable[ki][j]);
            const double r05 = rangeTable[ri][j] + rt * (rangeTable[ri + 1][j] - rangeTable[ri][j]);
            const double rh = hardTable[hi][j] + ht * (hardTable[hi + 1][j] - hardTable[hi][j]);
            const double corr = (1.0 - w) * (r05 - rangeTable[1][j]) + w * (rh - hardTable[1][j]);
            double v = std::clamp (base + corr, 0.0, 1.0);
            v = std::max (v, prev); // keep the curve monotonic
            nodes[size_t (j)] = v;
            prev = v;
        }
        computeSlopes();
    }

    // f(x), x in dB relative to the threshold.
    double eval (double x) const noexcept
    {
        using namespace curve;
        if (x <= xMin)
            return nodes[0] * std::exp (lowDecayPerDb * (x - xMin));
        if (x >= xMax)
            return 1.0 - (1.0 - nodes[numX - 1]) * std::exp (-highDecayPerDb * (x - xMax));
        const double p = (x - xMin) / xStep;
        int i = int (p);
        if (i > numX - 2)
            i = numX - 2;
        const double t = p - double (i);
        const double y0 = nodes[size_t (i)], y1 = nodes[size_t (i + 1)];
        const double m0 = slopes[size_t (i)], m1 = slopes[size_t (i + 1)]; // per grid step
        const double t2 = t * t, t3 = t2 * t;
        return (2.0 * t3 - 3.0 * t2 + 1.0) * y0 + (t3 - 2.0 * t2 + t) * m0 + (-2.0 * t3 + 3.0 * t2) * y1 + (t3 - t2) * m1;
    }

    const std::array<double, curve::numX>& getNodes() const noexcept { return nodes; }

private:
    void computeSlopes() noexcept
    {
        using namespace curve;
        std::array<double, numX - 1> d {};
        for (int j = 0; j < numX - 1; ++j)
            d[size_t (j)] = nodes[size_t (j + 1)] - nodes[size_t (j)];
        slopes[0] = d[0];
        slopes[numX - 1] = d[numX - 2];
        for (int j = 1; j < numX - 1; ++j)
        {
            const double a = d[size_t (j - 1)], b = d[size_t (j)];
            slopes[size_t (j)] = (a * b <= 0.0) ? 0.0 : 2.0 * a * b / (a + b);
        }
    }

    std::array<double, curve::numX> nodes {}, slopes {};
    double cachedKnee = -1.0, cachedRange = 1.0e9;
};

} // namespace craft::multiband
