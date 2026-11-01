// Craft Hybrid Delay - "Analog" colour modes (spec r2 section 9).
//
// Each mode is a small fixed chain applied to the wet signal read from the delay line (placement: see
// IMPLEMENTATION_NOTES.md, r2 assumption 1):
//
//   gain -> [low shelf] -> [high shelf] -> [peak] -> [band dead-zone] -> [crossover] -> [asymmetry]
//        -> cubic u + k3 u^3 -> expansion u + c3 u|u|   (memoryless terms: optionally on a low-passed band only)
//        -> DC blocker (1 Hz)   [a slow envelope compressor stage exists but is unused since r3: it let
//                                instantaneous expansion through on clicks, spec r3 9.2.1]
//
// The band dead-zone (mode 3) removes a band around ~100 Hz and re-adds it through a soft, asymmetric
// dead zone b - t tanh(b / t): low-level content in that band is "starved", loud content passes.
// The crossover term u - xo tanh(u / xt) adds low-level odd harmonics (mode 4).
// The asymmetry term c2 u^2 / sqrt(u^2 + d^2) adds even harmonics.
// All constants were fitted offline (tools/fit/analog_fit_r2.py) to the section 9.2 tables, measured after
// the section 4.1 saturation. The noise (section 9.3) is injected inside the loop by the engine.
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
    double highShelfHz = 0.0, highShelfQ = 0.6, highShelfDb = 0.0;
    double peakHz = 0.0, peakQ = 0.7, peakDb = 0.0;
    double bandHz = 0.0, bandQ = 1.0, bandAmount = 0.0, deadPos = 0.01, deadNeg = 0.01;
    double nlBandHz = 0.0;              // > 0: memoryless terms act on a low-passed band only
    double crossover = 0.0, crossoverKnee = 0.01;
    double asym = 0.0, asymKnee = 1.0e-3;
    double cubic = 0.0;                 // u + k3 u^3
    double expand = 0.0, compress = 0.0;
    double noiseDbfs = -200.0;       // target output RMS with silent input, Feedback 0 (section 9.3)
    double noiseInjectDbfs = -200.0; // RMS injected into the loop (calibrated to give noiseDbfs at the output)
};

// Index = mode (0 = Off).
inline const std::array<AnalogModeSpec, 5>& analogModeSpecs()
{
    static const std::array<AnalogModeSpec, 5> specs = [] {
        std::array<AnalogModeSpec, 5> s {};
        // Mode 1: low-frequency loss (low shelf) and low-frequency (transformer-like) distortion on a band
        // below ~180 Hz: even + odd harmonics on bass tones, transients almost unchanged
        s[1].gainDb = -0.24454;
        s[1].lowShelfHz = 96.8488; s[1].lowShelfDb = -3.08397;
        s[1].nlBandHz = 178.33;
        s[1].crossover = 0.000763535; s[1].crossoverKnee = 0.021866;
        s[1].asym = 0.067038; s[1].asymKnee = 0.0923346;
        s[1].cubic = -0.0932447; s[1].expand = 1.00965; s[1].compress = 0.0;
        // Mode 2: gentle high-frequency tilt, weak level-dependent second harmonic
        s[2].gainDb = -0.574866;
        s[2].highShelfHz = 570.093; s[2].highShelfDb = -0.377451;
        s[2].asym = 0.00910324; s[2].asymKnee = 0.383885;
        s[2].cubic = -0.172506; s[2].expand = 0.133371; s[2].compress = 0.0;
        // Mode 3: "starved" - band-limited asymmetric dead zone around 100 Hz
        s[3].gainDb = 0.720138;
        s[3].highShelfHz = 1439.69; s[3].highShelfDb = -1.49923;
        s[3].peakHz = 785.765; s[3].peakDb = -2.038;
        s[3].bandHz = 92.9519; s[3].bandQ = 0.23439; s[3].bandAmount = 1.0;
        s[3].deadPos = 0.0191578; s[3].deadNeg = 0.0325663;
        s[3].asym = 0.105295; s[3].asymKnee = 0.0086392;
        s[3].cubic = -0.266403; s[3].expand = -0.00721763; s[3].compress = 0.0;
        // Mode 4: nearly transparent; a small low-level expansion (below about -66 dBFS) supplies the low-level
        // 3rd harmonic of the table (-36 dBc at a = 0.03) in phase with the saturation's
        s[4].gainDb = -0.406355;
        s[4].crossover = -0.000900391; s[4].crossoverKnee = 0.0005;
        s[4].cubic = -0.425064; s[4].expand = 0.280488; s[4].compress = 0.0;
        // Noise (section 9.3): output RMS targets at Feedback 0; injection levels calibrated through each
        // mode's chain (tests/AnalogTests.cpp checks the output levels at 0 ... 100 % feedback)
        s[1].noiseDbfs = -102.7; s[2].noiseDbfs = -85.5; s[3].noiseDbfs = -106.0; s[4].noiseDbfs = -108.7;
        s[1].noiseInjectDbfs = -99.87; s[2].noiseInjectDbfs = -85.07; s[3].noiseInjectDbfs = -102.67; s[4].noiseInjectDbfs = -117.39;
        return s;
    }();
    return specs;
}

class AnalogStage
{
public:
    static constexpr double envelopeSeconds = 0.02, dcBlockHz = 1.0;

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
        useNlBand = s.nlBandHz > 0.0;
        if (useNlBand) nlBand.setCoeffs (BiquadCoeffs::lowPass (BiquadCoeffs::cornerStandard (s.nlBandHz, fs), 0.707));
        envCoeff = std::exp (-1.0 / (envelopeSeconds * fs));
        dcCoeff = std::exp (-2.0 * pi * dcBlockHz / fs);
        reset();
    }

    void reset() noexcept
    {
        ls.reset(); hs.reset(); pk.reset(); band.reset(); nlBand.reset();
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
        // memoryless terms; with nlBandHz > 0 they act only on a low-passed band b and the difference is
        // added back (low-frequency, transformer-like distortion that leaves broadband clicks almost untouched)
        const double b = useNlBand ? nlBand.process (u) : u;
        double y = b;
        if (spec.crossover != 0.0)
            y -= spec.crossover * std::tanh (y / spec.crossoverKnee);
        if (spec.asym != 0.0)
            y += spec.asym * y * y / std::sqrt (y * y + spec.asymKnee * spec.asymKnee);
        y += spec.cubic * y * y * y;
        y += spec.expand * y * std::abs (y);
        u += y - b;
        env = std::abs (u) + envCoeff * (env - std::abs (u));
        u /= (1.0 + spec.compress * env);
        // DC blocker (first-order high-pass, 1 Hz): removes the DC of the asymmetric terms
        const double out = 0.5 * (1.0 + dcCoeff) * (u - dcX) + dcCoeff * dcY;
        dcX = u;
        dcY = out;
        return out;
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
    bool useLs = false, useHs = false, usePk = false, useBand = false, useNlBand = false;
    Biquad ls, hs, pk, band, nlBand;
};

// Noise of the Analog modes (section 9.3): injected into the loop signal before the saturation on every
// pass, so it accumulates with feedback. A band of noise around 10 Hz plus two weak low-frequency tones
// (46.6 Hz and 91.8 Hz).
class AnalogNoise
{
public:
    // three noise bands: (centre Hz, Q, power share). The two narrow bands sound like weak hum components
    // but are random, so they accumulate in the loop like the rest of the noise.
    static constexpr double bandHz[3] = { 10.0, 46.6, 91.8 };
    static constexpr double bandQ[3] = { 0.7, 15.0, 15.0 };
    static constexpr double share[3] = { 0.6, 0.25, 0.15 };

    void prepare (double fs, std::uint32_t seed) noexcept
    {
        for (int i = 0; i < 3; ++i)
        {
            bp[i].setCoeffs (BiquadCoeffs::bandPass (BiquadCoeffs::cornerStandard (bandHz[i], fs), bandQ[i]));
            // unit-variance white noise through a 0 dB-peak band-pass has variance pi f0 / (Q fs)
            gain[i] = std::sqrt (share[i]) / std::sqrt (pi * bandHz[i] / (bandQ[i] * fs));
        }
        state = seed != 0 ? seed : 0x9e3779b9u;
        reset();
    }

    void reset() noexcept
    {
        for (auto& b : bp)
            b.reset();
    }

    // rms: linear RMS of the whole noise signal
    double next (double rms) noexcept
    {
        double sum = 0.0;
        for (int i = 0; i < 3; ++i)
            sum += gain[i] * bp[i].process ((uniform() * 2.0 - 1.0) * std::sqrt (3.0));
        return rms * sum;
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

    Biquad bp[3];
    double gain[3] {};
    std::uint32_t state = 1;
};

} // namespace craft::hdelay
