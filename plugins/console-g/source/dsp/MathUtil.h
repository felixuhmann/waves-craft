// Craft Console G - interpolation helpers (linear, monotone cubic / PCHIP) and dB conversions.
#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

namespace craft::consoleg::dsp
{

inline constexpr double pi = 3.14159265358979323846;

inline double dbToGain (double db) noexcept { return std::pow (10.0, db / 20.0); }
inline double gainToDb (double g) noexcept { return 20.0 * std::log10 (std::max (g, 1.0e-30)); }

// Index i with xs[i] <= x < xs[i+1] (clamped to [0, n-2]); xs ascending.
inline size_t segmentIndex (const std::vector<double>& xs, double x) noexcept
{
    if (xs.size() < 2 || x <= xs.front())
        return 0;
    if (x >= xs[xs.size() - 2])
        return xs.size() - 2;
    const auto it = std::upper_bound (xs.begin(), xs.end(), x);
    return size_t (it - xs.begin()) - 1;
}

// Piecewise-linear interpolation, clamped to the end values outside the range.
inline double interpLinear (const std::vector<double>& xs, const std::vector<double>& ys, double x) noexcept
{
    if (xs.empty())
        return 0.0;
    if (xs.size() == 1 || x <= xs.front())
        return ys.front();
    if (x >= xs.back())
        return ys.back();
    const size_t i = segmentIndex (xs, x);
    const double t = (x - xs[i]) / (xs[i + 1] - xs[i]);
    return ys[i] + t * (ys[i + 1] - ys[i]);
}

// Monotone piecewise cubic Hermite interpolation (Fritsch & Carlson 1980, the "PCHIP" end/interior slope rule of
// Moler, Numerical Computing with MATLAB, ch. 3). Clamped to the end values outside the range.
class Pchip
{
public:
    Pchip() = default;
    Pchip (std::vector<double> x, std::vector<double> y) { set (std::move (x), std::move (y)); }

    void set (std::vector<double> x, std::vector<double> y)
    {
        xs = std::move (x);
        ys = std::move (y);
        const size_t n = xs.size();
        d.assign (n, 0.0);
        if (n < 2)
            return;
        std::vector<double> h (n - 1), delta (n - 1);
        for (size_t k = 0; k + 1 < n; ++k)
        {
            h[k] = xs[k + 1] - xs[k];
            delta[k] = (ys[k + 1] - ys[k]) / h[k];
        }
        if (n == 2)
        {
            d[0] = d[1] = delta[0];
            return;
        }
        for (size_t k = 1; k + 1 < n; ++k)
        {
            if (delta[k - 1] * delta[k] <= 0.0)
                d[k] = 0.0;
            else
            {
                const double w1 = 2.0 * h[k] + h[k - 1], w2 = h[k] + 2.0 * h[k - 1];
                d[k] = (w1 + w2) / (w1 / delta[k - 1] + w2 / delta[k]);
            }
        }
        d[0] = endSlope (h[0], h[1], delta[0], delta[1]);
        d[n - 1] = endSlope (h[n - 2], h[n - 3], delta[n - 2], delta[n - 3]);
    }

    double operator() (double x) const noexcept
    {
        const size_t n = xs.size();
        if (n == 0)
            return 0.0;
        if (n == 1 || x <= xs.front())
            return ys.front();
        if (x >= xs.back())
            return ys.back();
        const size_t k = segmentIndex (xs, x);
        const double h = xs[k + 1] - xs[k], t = (x - xs[k]) / h;
        const double t2 = t * t, t3 = t2 * t;
        const double h00 = 2 * t3 - 3 * t2 + 1, h10 = t3 - 2 * t2 + t, h01 = -2 * t3 + 3 * t2, h11 = t3 - t2;
        return h00 * ys[k] + h10 * h * d[k] + h01 * ys[k + 1] + h11 * h * d[k + 1];
    }

    bool empty() const noexcept { return xs.empty(); }

private:
    static double endSlope (double h0, double h1, double del0, double del1) noexcept
    {
        double dd = ((2.0 * h0 + h1) * del0 - h0 * del1) / (h0 + h1);
        if (dd * del0 <= 0.0)
            dd = 0.0;
        else if (del0 * del1 <= 0.0 && std::abs (dd) > std::abs (3.0 * del0))
            dd = 3.0 * del0;
        return dd;
    }

    std::vector<double> xs, ys, d;
};

} // namespace craft::consoleg::dsp
