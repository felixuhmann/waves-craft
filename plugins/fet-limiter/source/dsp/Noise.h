// Craft FET Limiter - Analog noise and hum (spec 7.3, D4). Host independent.
//
// Added at the output of the wet path, independent of the Input setting.
//   rev A: low-frequency noise (white noise through a 1 Hz high-pass and two 6 Hz low-pass poles), -71.8 dBFS rms.
//   rev B: white noise plus a mains line (50 / 60 Hz, -89 dBFS peak) and its 3rd harmonic (-95.5 dBFS peak);
//          total -76.1 dBFS rms.
// Stereo channels get independent noise and the same hum. Analog Off adds nothing.
#pragma once

#include <cmath>
#include <cstdint>

#include "Filters.h"
#include "Parameters.h"

namespace craft::fet_limiter::dsp
{

class NoiseSource
{
public:
    static constexpr double revARmsDb = -71.8;
    static constexpr double revBRmsDb = -76.1;
    static constexpr double humDb = -89.0;
    static constexpr double hum3Db = -95.5;

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        for (auto& f : hpf)
            f.setCoeffs (highPass1 (1.0, fs));
        lpCoeff = 1.0 - std::exp (-2.0 * pi * lpHz / fs);
        // Power gain of HP(1 Hz) * LP(6 Hz)^2 for unit-variance white noise (continuous-time approximation):
        // integral of |H(f)|^2 over 0..fs/2 times the one-sided density 2 / fs of unit white noise.
        double integral = 0.0;
        const double df = 0.01;
        for (double f = df * 0.5; f < 2000.0; f += df)
        {
            const double hp2 = (f * f) / (f * f + 1.0);
            const double lp2v = 1.0 / (1.0 + (f / lpHz) * (f / lpHz));
            integral += hp2 * lp2v * lp2v * df;
        }
        scaleA = std::pow (10.0, revARmsDb / 20.0) / std::sqrt (integral * 2.0 / fs);
        humAmp = std::pow (10.0, humDb / 20.0);
        hum3Amp = std::pow (10.0, hum3Db / 20.0);
        noiseB = revBNoiseRms();
        reset();
    }

    void reset()
    {
        for (int c = 0; c < 2; ++c)
        {
            state[c] = 0x9E3779B97F4A7C15ull ^ (0x632BE59BD9B4E019ull * std::uint64_t (c + 1));
            lp1[c] = lp2[c] = 0.0;
            hpf[c].reset();
        }
        phase = 0.0;
    }

    void setMode (Revision r, Analog a) noexcept
    {
        rev = r;
        analog = a;
    }

    bool isActive() const noexcept { return analog != Analog::Off; }

    // Advances the hum oscillator once per sample frame (call before the channel samples of the frame).
    void advanceFrame() noexcept
    {
        const double f = analog == Analog::Hz50 ? 50.0 : 60.0;
        phase += f / fs;
        if (phase >= 1.0)
            phase -= 1.0;
    }

    double sample (int channel) noexcept
    {
        const double w = gaussian (channel);
        if (rev == Revision::A)
        {
            // 1 Hz high-pass, two 6 Hz low-pass poles: almost all energy below ~10 Hz
            double v = hpf[channel].process (w);
            lp1[channel] += (v - lp1[channel]) * lpCoeff;
            lp2[channel] += (lp1[channel] - lp2[channel]) * lpCoeff;
            return lp2[channel] * scaleA;
        }
        const double ph = 2.0 * pi * phase;
        return w * noiseB + humAmp * std::sin (ph) + hum3Amp * std::sin (3.0 * ph);
    }

    // rms of the white part of rev B so that the total (noise + hum) is revBRmsDb
    static double revBNoiseRms() noexcept
    {
        const double total = std::pow (10.0, revBRmsDb / 20.0);
        const double h1 = std::pow (10.0, humDb / 20.0), h3 = std::pow (10.0, hum3Db / 20.0);
        return std::sqrt (total * total - 0.5 * (h1 * h1 + h3 * h3));
    }

private:
    double uniform (int c) noexcept
    {
        auto& s = state[c];
        s ^= s << 13;
        s ^= s >> 7;
        s ^= s << 17;
        return (double (s >> 11) + 0.5) * (1.0 / 9007199254740992.0);
    }

    double gaussian (int c) noexcept
    {
        // Box-Muller (one value per call)
        const double u1 = uniform (c), u2 = uniform (c);
        return std::sqrt (-2.0 * std::log (u1)) * std::cos (2.0 * pi * u2);
    }

    static constexpr double lpHz = 6.0;
    double fs = 48000.0, lpCoeff = 0.0, phase = 0.0;
    double scaleA = 1.0, humAmp = 0.0, hum3Amp = 0.0, noiseB = 0.0;
    Revision rev = Revision::A;
    Analog analog = Analog::Off;
    std::uint64_t state[2] {};
    double lp1[2] {}, lp2[2] {};
    Biquad hpf[2];
};

} // namespace craft::fet_limiter::dsp
