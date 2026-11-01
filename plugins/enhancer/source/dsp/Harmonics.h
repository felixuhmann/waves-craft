// Craft Enhancer - harmonic generation of the processed band (spec r1 section 5.4).
//
// For a band signal x with analytic envelope e (Analytic.h), u = x / e is the cosine of the instantaneous phase, so the
// Chebyshev polynomials T_k(u) = cos(k phase) produce exactly the k-th harmonic. The harmonic levels (dBc, H2..H5) depend
// on the frequency of the band content, estimated from the analytic signal's phase increment, and are interpolated from
// tables/harmonics.csv (per band: the in-band rows plus the adjacent rows; log-frequency interpolation). Harmonics that
// would reach 0.45 fs are faded out (no aliasing). Being normalised by e, the levels do not depend on input level or
// band gain (spec 5.4).
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

#include "Parameters.h"

namespace craft::enhancer::dsp
{

inline constexpr int numHarmonics = 4; // H2 .. H5

// Per band and harmonic: (log f, dBc) support points.
struct HarmonicCurves
{
    std::array<std::array<std::vector<std::pair<double, double>>, numHarmonics>, numBands> points;

    HarmonicCurves()
    {
        namespace t = tables;
        for (int b = 0; b < numBands; ++b)
        {
            // rows of this band in ascending frequency
            std::vector<int> rows;
            for (int i = 0; i < (int) t::harmonics_rows; ++i)
                if ((int) t::harmonics[i][0] == b)
                    rows.push_back (i);
            int first = -1, last = -1;
            for (int j = 0; j < (int) rows.size(); ++j)
                if (t::harmonics[rows[(size_t) j]][6] > 0.5)
                {
                    if (first < 0)
                        first = j;
                    last = j;
                }
            const int lo = std::max (0, first - 1), hi = std::min ((int) rows.size() - 1, last + 1);
            for (int j = lo; j <= hi; ++j)
            {
                const auto& r = t::harmonics[rows[(size_t) j]];
                for (int k = 0; k < numHarmonics; ++k)
                {
                    const double v = r[2 + k];
                    if (std::isnan (v) || v < -90.0)
                        continue;
                    points[(size_t) b][(size_t) k].push_back ({ std::log (r[1]), v });
                }
            }
        }
    }

    // dBc of harmonic k (0 = H2) for band b at frequency f; -inf if no data
    double levelDb (int b, int k, double f) const noexcept
    {
        const auto& p = points[(size_t) b][(size_t) k];
        if (p.empty())
            return -300.0;
        const double lf = std::log (std::max (f, 1.0));
        if (lf <= p.front().first)
            return p.front().second;
        for (size_t i = 1; i < p.size(); ++i)
            if (lf <= p[i].first)
            {
                const double a = (lf - p[i - 1].first) / (p[i].first - p[i - 1].first);
                return p[i - 1].second + a * (p[i].second - p[i - 1].second);
            }
        return p.back().second;
    }

    static const HarmonicCurves& get()
    {
        static const HarmonicCurves curves;
        return curves;
    }
};

// Harmonic generator for one channel of one band.
class HarmonicGenerator
{
public:
    void prepare (int bandIndex, double sampleRate) noexcept
    {
        band = bandIndex;
        fs = sampleRate;
        // frequency estimate smoothing: 5 ms
        freqCoeff = std::exp (-1.0 / (0.005 * fs));
        reset();
    }

    void reset() noexcept
    {
        prevRe = prevIm = 0.0;
        accRe = accIm = 0.0;
        counter = 0;
        coeffs = {};
    }

    // x: band sample, (re, im): analytic signal of x, env = |re + j im|. Returns the harmonic signal to add to x.
    inline double process (double x, double re, double im, double env) noexcept
    {
        // phase increment z[n] conj(z[n-1]) (power weighted), smoothed
        const double pr = re * prevRe + im * prevIm;
        const double pi_ = im * prevRe - re * prevIm;
        prevRe = re;
        prevIm = im;
        accRe = pr + freqCoeff * (accRe - pr);
        accIm = pi_ + freqCoeff * (accIm - pi_);

        if (--counter <= 0)
        {
            counter = 16;
            updateCoefficients();
        }

        if (env < 1.0e-12)
            return 0.0;
        const double u = std::clamp (x / env, -1.0, 1.0);
        const double u2 = u * u;
        const double t2 = 2.0 * u2 - 1.0;
        const double t3 = u * (4.0 * u2 - 3.0);
        const double t4 = 8.0 * u2 * (u2 - 1.0) + 1.0;
        const double t5 = u * (16.0 * u2 * u2 - 20.0 * u2 + 5.0);
        return env * (coeffs[0] * t2 + coeffs[1] * t3 + coeffs[2] * t4 + coeffs[3] * t5);
    }

    double getFrequencyEstimate() const noexcept
    {
        if (accRe == 0.0 && accIm == 0.0)
            return 0.0;
        return std::abs (std::atan2 (accIm, accRe)) * fs / (2.0 * 3.14159265358979323846);
    }

private:
    void updateCoefficients() noexcept
    {
        const double f = getFrequencyEstimate();
        const auto& curves = HarmonicCurves::get();
        for (int k = 0; k < numHarmonics; ++k)
        {
            const double hf = double (k + 2) * f;
            const double fade = std::clamp ((0.45 * fs - hf) / (0.05 * fs), 0.0, 1.0);
            coeffs[(size_t) k] = f > 0.0 && fade > 0.0 ? fade * std::pow (10.0, curves.levelDb (band, k, f) / 20.0) : 0.0;
        }
    }

    int band = 0;
    double fs = 48000.0, freqCoeff = 0.0;
    double prevRe = 0, prevIm = 0, accRe = 0, accIm = 0;
    int counter = 0;
    std::array<double, numHarmonics> coeffs {};
};

} // namespace craft::enhancer::dsp
