// Craft Opto Leveler - optical gain cell dynamics: the spec section 4.3 reference model, implemented literally.
// Host independent.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>

namespace craft::opto_leveler::dsp
{

// State: three attenuation components c_i >= 0, total attenuation a = sum c_i, cell gain g = 1 - a.
// Per sample, each component moves towards w_i * a_t with an attack time constant that shrinks for deeper targets
// (tau_a_i * (0.6 / a_t)^beta) or with its own release time constant. The slow components only charge while the
// reduction is held, which gives the programme-dependent two-stage release (spec 4.2).
class OptoCell
{
public:
    static constexpr int numComponents = 3;
    static constexpr std::array<double, 3> weight { 0.7805, 0.1381, 0.0814 };
    static constexpr std::array<double, 3> tauAttack { 4.90e-3, 23.1e-3, 110.0e-3 };
    static constexpr std::array<double, 3> tauRelease { 0.264, 2.75, 5.38 };
    static constexpr double beta = 1.26;
    static constexpr double minTarget = 1.0e-9; // keeps (0.6 / a_t)^beta finite; such targets never charge anyway

    void prepare (double sampleRate)
    {
        dt = 1.0 / sampleRate;
        for (int i = 0; i < numComponents; ++i)
            releaseCoef[size_t (i)] = 1.0 - std::exp (-dt / tauRelease[size_t (i)]);
        reset();
    }

    void reset() noexcept { c.fill (0.0); }

    // One sample: a_t = target attenuation (1 - 10^(-GR_static / 20)). Returns the cell gain g.
    double process (double targetAttenuation) noexcept
    {
        const double at = std::clamp (targetAttenuation, 0.0, 0.999999);
        double scale = -1.0; // (0.6 / a_t)^beta, computed on demand
        for (int i = 0; i < numComponents; ++i)
        {
            const size_t k = size_t (i);
            const double target = weight[k] * at;
            if (target > c[k])
            {
                if (scale < 0.0)
                    scale = std::pow (0.6 / std::max (at, minTarget), beta);
                const double tau = tauAttack[k] * scale;
                c[k] += (target - c[k]) * (1.0 - std::exp (-dt / tau));
            }
            else
            {
                c[k] += (target - c[k]) * releaseCoef[k];
            }
        }
        return gain();
    }

    double attenuation() const noexcept { return c[0] + c[1] + c[2]; }
    double gain() const noexcept { return 1.0 - attenuation(); }
    const std::array<double, 3>& components() const noexcept { return c; }

private:
    double dt = 1.0 / 48000.0;
    std::array<double, 3> releaseCoef {};
    std::array<double, 3> c {};
};

} // namespace craft::opto_leveler::dsp
