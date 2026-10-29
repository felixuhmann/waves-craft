// Craft Hybrid Delay - "Analog" colour modes (spec r1 section 9).
//
// Each mode is a small fixed chain applied to the loop signal before the saturation (so every repeat is
// coloured again - placement is open in the spec, 12.3; see IMPLEMENTATION_NOTES.md):
//
//   gain -> [low shelf] -> [high shelf] -> [peak] -> [band dead-zone] -> [asymmetry]
//        -> expansion u + c3 u|u| -> slow envelope compressor u / (1 + kc env) -> DC blocker (5 Hz)
//
// The band dead-zone (mode 3) removes a band around ~100 Hz and re-adds it through a soft, asymmetric
// dead zone b - t tanh(b / t): low-level content in that band is "starved", loud content passes.
// The asymmetry term c2 u^2 / sqrt(u^2 + d^2) adds even harmonics at a level-independent dBc.
// The expansion + envelope compressor pair lowers the odd harmonics of the loop saturation while
// keeping its level compression (the spec's harmonic table is ~6 dB below what the section 4
// saturation alone produces).
// All constants were fitted offline to the section 9 tables (response, fundamental, H2/H3) at 48 kHz;
// filter corners are defined in Hz. Output noise (wet path, silent input) is generated separately by
// AnalogNoise.
#pragma once

#include <array>
#include <cmath>
#include <cstdint>

#include "Biquad.h"

namespace craft::hdelay
{

struct AnalogModeSpec
{
    double gainDb = 0.0;
    double lowShelfHz = 0.0, lowShelfQ = 0.6, lowShelfDb = 0.0;     // 0 Hz: unused
    double highShelfHz = 0.0, highShelfQ = 0.5, highShelfDb = 0.0;
    double peakHz = 0.0, peakQ = 0.7, peakDb = 0.0;
    double bandHz = 0.0, bandQ = 1.0, bandAmount = 0.0, deadPos = 0.01, deadNeg = 0.01;
    double asym = 0.0, asymKnee = 1.0e-3;
    double expand = 0.0, compress = 0.0;
    double noiseDbfs = -200.0; // RMS of the wet-path noise
};

// Index = mode (0 = Off).
inline const std::array<AnalogModeSpec, 5>& analogModeSpecs()
{
    static const std::array<AnalogModeSpec, 5> specs = [] {
        std::array<AnalogModeSpec, 5> s {};
        // Mode 1: low-frequency loss (low shelf), even + odd harmonics
        s[1].gainDb = -0.37175;
        s[1].lowShelfHz = 86.13268; s[1].lowShelfQ = 0.6; s[1].lowShelfDb = -3.03717;
        s[1].highShelfHz = 8000.0; s[1].highShelfQ = 0.5; s[1].highShelfDb = -0.13911;
        s[1].asym = 0.08609; s[1].asymKnee = 0.15636;
        s[1].expand = 0.13339; s[1].compress = 0.0;
        s[1].noiseDbfs = -103.0;
        // Mode 2: gentle high-frequency tilt, constant low-level second harmonic
        s[2].gainDb = -0.79992;
        s[2].highShelfHz = 1410.903; s[2].highShelfQ = 0.6; s[2].highShelfDb = -0.92592;
        s[2].asym = 0.0054275; s[2].asymKnee = 1.0e-3;
        s[2].expand = 0.63117; s[2].compress = 0.56390;
        s[2].noiseDbfs = -85.5;
        // Mode 3: "starved" - band-limited asymmetric dead zone around 100 Hz
        s[3].gainDb = 0.85976;
        s[3].highShelfHz = 3000.0; s[3].highShelfQ = 0.5; s[3].highShelfDb = -1.43829;
        s[3].peakHz = 1272.93; s[3].peakQ = 0.7; s[3].peakDb = -1.62443;
        s[3].bandHz = 105.874; s[3].bandQ = 0.250787; s[3].bandAmount = 1.0;
        s[3].deadPos = 0.0251831; s[3].deadNeg = 0.0466534;
        s[3].asym = 0.0705175; s[3].asymKnee = 0.0471764;
        s[3].expand = 0.274828; s[3].compress = 0.608726;
        s[3].noiseDbfs = -106.0;
        // Mode 4: nearly transparent
        s[4].gainDb = -0.26763;
        s[4].expand = 0.48024; s[4].compress = 0.48628;
        s[4].noiseDbfs = -109.0;
        return s;
    }();
    return specs;
}

class AnalogStage
{
public:
    static constexpr double envelopeSeconds = 0.02, dcBlockHz = 5.0;

    void prepare (int newMode, double fs) noexcept
    {
        mode = newMode;
        const auto& s = analogModeSpecs()[size_t (mode)];
        spec = s;
        gain = std::pow (10.0, s.gainDb / 20.0);
        useLs = s.lowShelfHz > 0.0;
        useHs = s.highShelfHz > 0.0;
        usePk = s.peakHz > 0.0;
        useBand = s.bandHz > 0.0 && s.bandAmount != 0.0;
        if (useLs) ls.setCoeffs (BiquadCoeffs::lowShelf (BiquadCoeffs::cornerStandard (s.lowShelfHz, fs), s.lowShelfQ, s.lowShelfDb));
        if (useHs) hs.setCoeffs (BiquadCoeffs::highShelf (BiquadCoeffs::cornerStandard (s.highShelfHz, fs), s.highShelfQ, s.highShelfDb));
        if (usePk) pk.setCoeffs (peaking (BiquadCoeffs::cornerStandard (s.peakHz, fs), s.peakQ, s.peakDb));
        if (useBand) band.setCoeffs (BiquadCoeffs::bandPass (BiquadCoeffs::cornerStandard (s.bandHz, fs), s.bandQ));
        envCoeff = std::exp (-1.0 / (envelopeSeconds * fs));
        dcCoeff = std::exp (-2.0 * pi * dcBlockHz / fs);
        reset();
    }

    void reset() noexcept
    {
        ls.reset(); hs.reset(); pk.reset(); band.reset();
        env = 0.0;
        dcX = dcY = 0.0;
    }

    int getMode() const noexcept { return mode; }

    double process (double x) noexcept
    {
        if (mode == 0)
            return x;
        double u = x * gain;
        if (useLs) u = ls.process (u);
        if (useHs) u = hs.process (u);
        if (usePk) u = pk.process (u);
        if (useBand)
        {
            const double b = spec.bandAmount * band.process (u);
            const double t = b > 0.0 ? spec.deadPos : spec.deadNeg;
            u = u - t * std::tanh (b / t); // = u - b + (b - t tanh(b/t))
        }
        if (spec.asym != 0.0)
            u += spec.asym * u * u / std::sqrt (u * u + spec.asymKnee * spec.asymKnee);
        u += spec.expand * u * std::abs (u);
        env = std::abs (u) + envCoeff * (env - std::abs (u));
        u /= (1.0 + spec.compress * env);
        // DC blocker (first-order high-pass): the asymmetric terms create DC that would build up in the loop
        const double y = 0.5 * (1.0 + dcCoeff) * (u - dcX) + dcCoeff * dcY;
        dcX = u;
        dcY = y;
        return y;
    }

    static BiquadCoeffs peaking (double w0, double q, double gainDb) noexcept
    {
        const double A = std::pow (10.0, gainDb / 40.0), c = std::cos (w0), al = std::sin (w0) / (2.0 * q);
        return BiquadCoeffs::normalised (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
    }

private:
    int mode = 0;
    AnalogModeSpec spec;
    double gain = 1.0, envCoeff = 0.0, env = 0.0, dcCoeff = 0.0, dcX = 0.0, dcY = 0.0;
    bool useLs = false, useHs = false, usePk = false, useBand = false;
    Biquad ls, hs, pk, band;
};

// Wet-path noise of the Analog modes (silent input): a band of noise around 10 Hz plus two weak
// low-frequency tones (46.6 Hz and 91.8 Hz), total RMS = the mode's noiseDbfs.
class AnalogNoise
{
public:
    static constexpr double bandHz = 10.0, bandQ = 0.7, tone1Hz = 46.6, tone2Hz = 91.8;
    static constexpr double bandShare = 0.6, tone1Share = 0.25, tone2Share = 0.15; // power shares

    void prepare (double fs, std::uint32_t seed) noexcept
    {
        sampleRate = fs;
        bp.setCoeffs (BiquadCoeffs::bandPass (BiquadCoeffs::cornerStandard (bandHz, fs), bandQ));
        // unit-variance white noise through a 0 dB-peak band-pass has variance pi f0 / (Q fs)
        bandNorm = 1.0 / std::sqrt (pi * bandHz / (bandQ * fs));
        state = seed != 0 ? seed : 0x9e3779b9u;
        ph1 = 0.0;
        ph2 = 0.37;
        bp.reset();
    }

    void reset() noexcept { bp.reset(); }

    // rms: linear RMS of the whole noise signal
    double next (double rms) noexcept
    {
        const double white = (uniform() * 2.0 - 1.0) * std::sqrt (3.0);
        const double nb = bp.process (white) * bandNorm;
        ph1 += tone1Hz / sampleRate;
        ph1 -= std::floor (ph1);
        ph2 += tone2Hz / sampleRate;
        ph2 -= std::floor (ph2);
        const double t1 = std::sqrt (2.0) * std::sin (2.0 * pi * ph1);
        const double t2 = std::sqrt (2.0) * std::sin (2.0 * pi * ph2);
        return rms * (std::sqrt (bandShare) * nb + std::sqrt (tone1Share) * t1 + std::sqrt (tone2Share) * t2);
    }

private:
    double uniform() noexcept
    {
        // xorshift32
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return double (state) / 4294967296.0;
    }

    Biquad bp;
    double sampleRate = 48000.0, bandNorm = 1.0, ph1 = 0.0, ph2 = 0.0;
    std::uint32_t state = 1;
};

} // namespace craft::hdelay
