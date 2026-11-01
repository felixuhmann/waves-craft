// Craft Enhancer - level-dependent, time-varying gain of the processed band (spec r1 sections 5.1-5.3).
//
// Detectors run on the linked (max over channels) analytic envelope e of the band signal:
//   F  fast detector, one-pole in the dB domain (attack tauFA, release tauFR)
//   S  slow detector, one-pole in the linear amplitude domain (attack tauSA, release tauSR ~ 0.4-0.5 s)
// Gain law (dB, in "measured gain" units = processed output / input at the band centre with 24 dB band gain):
//   g      = settled curve of tables/settled_curves.csv at the input-referred level F - |band(fc)| (linear in dB
//            between the tabulated levels, flat above -3 dBFS, blending to gSilence over -70..-90 dBFS)
//   boost  = beta * (softplus_w(r - kappa) - softplus_w(-kappa)),  r = max(0, S_dB - F_dB)
//   target = max(g, softmin(g + boost, ceiling))
// The target gain (linear) passes two cascaded asymmetric one-pole smoothers (up / down time constants).
// All constants are fitted per band at Punch 0, 3 and 5 to tables/band_steps.csv (tools/fit/dyn_fit2.py); other
// Punch values interpolate the constants (time constants geometrically). Everything is defined in seconds and dB,
// so the behaviour is the same at every sample rate (decision D1).
#pragma once

#include <algorithm>
#include <array>
#include <cmath>

#include "Parameters.h"

namespace craft::enhancer::dsp
{

inline constexpr int numCurveLevels = 7;
inline constexpr double curveLevels[numCurveLevels] = { -70.0, -50.0, -40.0, -30.0, -20.0, -10.0, -3.0 };
inline constexpr double punchAnchors[3] = { 0.0, 3.0, 5.0 };

// |band b(f_centre)| in dB at the table crossovers 88/494/2576/9285 Hz (centres 45/200/1100/5000/14000 Hz).
inline constexpr double bandCentreDb[numBands] = { -1.563, -1.499, -1.781, -2.165, -0.697 };
// Band gain at which the tables were measured.
inline constexpr double tableBandGainDb = 24.0;

struct DynamicsShape
{
    double tauFA, tauFR, tauSA, tauSR, beta, kappa, width, tauUp1, tauDown1, tauUp2, tauDown2;
};

struct DynamicsProfile
{
    DynamicsShape shape {};
    double gSilence = 0, ceiling = 0;
    std::array<double, numCurveLevels> curve {};
};

// Fitted constants (tools/fit/dyn_fit2.py), [punch anchor] for the shared shape, [band][punch anchor] for the rest.
#include "DynamicsFit.inc"

// Settled curve of band b at Punch anchor i, read from the embedded table.
inline std::array<double, numCurveLevels> settledCurve (int band, int anchor)
{
    namespace t = tables;
    std::array<double, numCurveLevels> c {};
    for (std::size_t i = 0; i < t::settled_curves_rows; ++i)
    {
        const auto& r = t::settled_curves[i];
        if ((int) r[0] == band && r[2] == punchAnchors[anchor])
            for (int k = 0; k < numCurveLevels; ++k)
                if (r[3] == curveLevels[k])
                    c[(size_t) k] = r[4];
    }
    return c;
}

inline DynamicsProfile makeProfile (int band, double punch)
{
    punch = std::clamp (punch, 0.0, 5.0);
    const int i = punch <= 3.0 ? 0 : 1;
    const double a = (punch - punchAnchors[i]) / (punchAnchors[i + 1] - punchAnchors[i]);
    auto lin = [a] (double x0, double x1) { return x0 + a * (x1 - x0); };
    auto geo = [a] (double x0, double x1) {
        x0 = std::max (x0, 1.0e-5);
        x1 = std::max (x1, 1.0e-5);
        return std::exp (std::log (x0) + a * (std::log (x1) - std::log (x0)));
    };
    const auto& s0 = fit::shapes[band][i];
    const auto& s1 = fit::shapes[band][i + 1];
    DynamicsProfile p;
    p.shape = { geo (s0.tauFA, s1.tauFA), geo (s0.tauFR, s1.tauFR), geo (s0.tauSA, s1.tauSA), geo (s0.tauSR, s1.tauSR),
                lin (s0.beta, s1.beta),   lin (s0.kappa, s1.kappa), lin (s0.width, s1.width), geo (s0.tauUp1, s1.tauUp1),
                geo (s0.tauDown1, s1.tauDown1), geo (s0.tauUp2, s1.tauUp2), geo (s0.tauDown2, s1.tauDown2) };
    p.gSilence = lin (fit::gSilence[band][i], fit::gSilence[band][i + 1]);
    p.ceiling = lin (fit::ceiling[band][i], fit::ceiling[band][i + 1]);
    const auto c0 = settledCurve (band, i), c1 = settledCurve (band, i + 1);
    for (int k = 0; k < numCurveLevels; ++k)
        p.curve[(size_t) k] = lin (c0[(size_t) k], c1[(size_t) k]);
    return p;
}

// Gain computer of one band (linked channels).
class BandDynamics
{
public:
    void prepare (int bandIndex, double sampleRate) noexcept
    {
        band = bandIndex;
        fs = sampleRate;
        setPunch (3.0);
        reset();
    }

    void setPunch (double punch) noexcept
    {
        profile = makeProfile (band, punch);
        const auto& s = profile.shape;
        auto c = [this] (double tau) { return tau <= 1.0e-5 ? 0.0 : std::exp (-1.0 / (tau * fs)); };
        cFA = c (s.tauFA);
        cFR = c (s.tauFR);
        cSA = c (s.tauSA);
        cSR = c (s.tauSR);
        cU1 = c (s.tauUp1);
        cD1 = c (s.tauDown1);
        cU2 = c (s.tauUp2);
        cD2 = c (s.tauDown2);
        h0 = s.width * std::log1p (std::exp (-s.kappa / s.width));
        // internal gain offset: measured gain (dB) - 24 dB band gain - band centre magnitude
        internalOffsetDb = -tableBandGainDb - bandCentreDb[band];
    }

    void reset() noexcept
    {
        fastDb = floorDb;
        slow = 0.0;
        g1 = g2 = -1.0;
    }

    // e: linked analytic envelope of the band (linear). Returns the processed-path gain (linear, for 0 dB band gain).
    inline double process (double e) noexcept
    {
        const double xDb = e > floorLin ? 20.0 * std::log10 (e) : floorDb;
        fastDb = xDb > fastDb ? xDb + (fastDb - xDb) * cFA : xDb + (fastDb - xDb) * cFR;
        const double ec = std::max (e, floorLin);
        slow = ec > slow ? ec + (slow - ec) * cSA : ec + (slow - ec) * cSR;

        const auto& s = profile.shape;
        const double slowDb = 20.0 * std::log10 (std::max (slow, floorLin));
        const double r = std::max (0.0, slowDb - fastDb);
        const double boost = r > 0.0 ? s.beta * (s.width * std::log1p (std::exp ((r - s.kappa) / s.width)) - h0) : 0.0;
        const double g = staticGainDb (fastDb - bandCentreDb[band]);
        double target = g;
        if (boost > 0.0)
            target = std::max (g, -std::log (std::exp (-(g + boost)) + std::exp (-profile.ceiling)));
        const double gl = std::pow (10.0, (target + internalOffsetDb) / 20.0);
        if (g1 < 0.0)
            g1 = g2 = gl;
        g1 = gl > g1 ? gl + (g1 - gl) * cU1 : gl + (g1 - gl) * cD1;
        g2 = g1 > g2 ? g1 + (g2 - g1) * cU2 : g1 + (g2 - g1) * cD2;
        return g2;
    }

    // Settled curve with the silence extension (input-referred level in dBFS -> measured gain in dB).
    double staticGainDb (double levelDb) const noexcept
    {
        const auto& c = profile.curve;
        if (levelDb < curveLevels[0])
        {
            const double a = std::min (1.0, (curveLevels[0] - levelDb) / 20.0);
            return c[0] + (profile.gSilence - c[0]) * a;
        }
        for (int k = 1; k < numCurveLevels; ++k)
            if (levelDb <= curveLevels[k])
                return c[(size_t) k - 1] + (c[(size_t) k] - c[(size_t) k - 1]) * (levelDb - curveLevels[k - 1]) / (curveLevels[k] - curveLevels[k - 1]);
        return c[numCurveLevels - 1];
    }

    const DynamicsProfile& getProfile() const noexcept { return profile; }
    double getCurrentGain() const noexcept { return g2 < 0.0 ? 0.0 : g2; }

private:
    static constexpr double floorDb = -120.0;
    static constexpr double floorLin = 1.0e-6;

    int band = 0;
    double fs = 48000.0;
    DynamicsProfile profile;
    double cFA = 0, cFR = 0, cSA = 0, cSR = 0, cU1 = 0, cD1 = 0, cU2 = 0, cD2 = 0, h0 = 0, internalOffsetDb = 0;
    double fastDb = floorDb, slow = 0.0, g1 = -1.0, g2 = -1.0;
};

} // namespace craft::enhancer::dsp
