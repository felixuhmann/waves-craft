// Craft Punch Compressor - "Analog" colour modes (spec r1 section 10).
//
// Each mode is one fixed chain per channel, applied after the Output gain:
//
//   gain -> [low shelf] -> [peaking] -> [high shelf]                                  (linear colour)
//        -> band split b = BP(u):  y = u - b + phi(b)                                  (level-dependent band)
//             phi(b) = b - a+ t+ tanh(b / t+)   (b >= 0)      soft, asymmetric "dead zone": the band is
//                      b - a- t- tanh(b / t-)   (b <  0)      attenuated at low level and opens up when loud
//                      + g3 b|b| + g2 b^2
//        -> y + b2 y^2 + b3 y|y|                                                       (2nd / 3rd harmonics)
//        -> y - xo tanh(y / xk)                                                        (crossover, constant H3)
//        -> DC blocker (first-order high-pass, 2 Hz)
//        -> + white noise (uncorrelated per channel)
//
// All constants were fitted offline (see IMPLEMENTATION_NOTES.md, "Analog fit") to the section 10 tables:
// the -30 dBFS magnitude response, the steady-tone fundamental / H2 / H3 table, the click peaks, the Mode 3
// level points and the noise floors. The filters are RBJ cookbook biquads (bilinear, pre-warped), so the
// response is the same at every sample rate below the top octave.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

#include "Primitives.h"

namespace craft::punch
{

struct AnalogModeParams
{
    double gainDb = 0.0;
    double lsHz = 100.0, lsQ = 0.7071, lsDb = 0.0;
    double pkHz = 1000.0, pkQ = 0.7, pkDb = 0.0;
    double hsHz = 5000.0, hsQ = 0.7071, hsDb = 0.0;
    double b2 = 0.0, b3 = 0.0;          // full-band 2nd / 3rd order terms
    double xo = 0.0, xk = 0.001;        // crossover
    bool band = false;                  // band split active
    double bandHz = 2000.0, bandQ = 0.5;
    double ap = 0.0, tp = 0.01, an = 0.0, tn = 0.01, g3 = 0.0, g2 = 0.0;
    double noiseDbfs = -400.0;          // output RMS with silent input
};

// Index = mode (0 = Off).
inline const std::array<AnalogModeParams, 5>& analogModeParams()
{
    static const std::array<AnalogModeParams, 5> p = [] {
        std::array<AnalogModeParams, 5> s {};
        // Mode 1: flat -0.25 dB with a slight low-frequency lift and top-end loss; level-proportional H2 / H3
        s[1].gainDb = -0.24924183325451157;
        s[1].lsHz = 53.96653378663355; s[1].lsDb = 0.18299644309011068;
        s[1].hsHz = 9748.680387080967; s[1].hsDb = -0.09163507108599538;
        s[1].b2 = 0.004373623908324571; s[1].b3 = 0.007900263116113282;
        s[1].noiseDbfs = -101.6;
        // Mode 2: broad presence dip (-0.8 dB around 1.5 kHz) whose band expands with level (3rd harmonic)
        s[2].gainDb = 0.057897702833486324;
        s[2].pkHz = 1630.846313682062; s[2].pkQ = 0.1595131610656987; s[2].pkDb = -0.8472867798943918;
        s[2].hsHz = 5619.7182667799725; s[2].hsDb = -0.11179591366002148;
        s[2].b2 = 0.00037308834547464676;
        s[2].band = true; s[2].bandHz = 1747.855936250817; s[2].bandQ = 0.15113660695942635;
        s[2].g3 = 0.03335335796887562; s[2].g2 = 0.001690237606849793;
        s[2].noiseDbfs = -82.7;
        // Mode 3: "starved" - a wide band around 2 kHz sits in an asymmetric soft dead zone: about -5 dB at low
        // level, opening towards 0 dB at full scale, with strong 2nd and 3rd harmonics; low-frequency lift
        s[3].gainDb = -0.8009342371171942;
        s[3].lsHz = 206.27727126527083; s[3].lsQ = 2.999357700908922; s[3].lsDb = 0.9402656448272927;
        s[3].pkHz = 2744.100589399832; s[3].pkQ = 0.24207548676049456; s[3].pkDb = -1.131797221423669;
        s[3].hsHz = 2000.0000000000002; s[3].hsDb = 0.5661132094225512;
        s[3].b2 = 0.005367307931190364; s[3].b3 = -0.02636941370836813;
        s[3].band = true; s[3].bandHz = 2090.3102223683286; s[3].bandQ = 0.2485910578273014;
        s[3].ap = 0.3605151434609223; s[3].tp = 0.04745433143856723;
        s[3].an = 0.4098076388487314; s[3].tn = 0.017360643084088894;
        s[3].g2 = -0.41583839576202847; s[3].g3 = 0.33741234760018113;
        s[3].noiseDbfs = -100.9;
        // Mode 4: nearly transparent; a tiny crossover step gives a constant-level 3rd harmonic (-67 dBFS)
        s[4].gainDb = 0.00688004022372772;
        s[4].hsHz = 1515.2732977686842; s[4].hsQ = 0.9136866130656428; s[4].hsDb = 0.15030329802020181;
        s[4].pkHz = 14000.0; s[4].pkQ = 4.846797614066071; s[4].pkDb = -2.0;
        s[4].b3 = 0.0002883181290773039;
        s[4].xo = 0.0009169910491408336; s[4].xk = 0.001;
        s[4].noiseDbfs = -108.7;
        return s;
    }();
    return p;
}

// One channel of one analog mode (no noise; the engine adds it).
class AnalogStage
{
public:
    static constexpr double dcBlockHz = 2.0;

    void prepare (int newMode, double fs) noexcept
    {
        mode = newMode < 0 ? 0 : (newMode > 4 ? 4 : newMode);
        p = analogModeParams()[size_t (mode)];
        gain = std::pow (10.0, p.gainDb / 20.0);
        useLs = p.lsDb != 0.0;
        usePk = p.pkDb != 0.0;
        useHs = p.hsDb != 0.0;
        const double nyq = 0.49 * fs;
        if (useLs) ls.setCoeffs (BiquadCoeffs::lowShelf (std::min (p.lsHz, nyq), fs, p.lsQ, p.lsDb));
        if (usePk) pk.setCoeffs (BiquadCoeffs::peaking (std::min (p.pkHz, nyq), fs, p.pkQ, p.pkDb));
        if (useHs) hs.setCoeffs (BiquadCoeffs::highShelf (std::min (p.hsHz, nyq), fs, p.hsQ, p.hsDb));
        if (p.band) bp.setCoeffs (BiquadCoeffs::bandPass (std::min (p.bandHz, nyq), fs, p.bandQ));
        dc.setCoeffs (BiquadCoeffs::highPass1 (dcBlockHz, fs));
        reset();
    }

    void reset() noexcept
    {
        ls.reset();
        pk.reset();
        hs.reset();
        bp.reset();
        dc.reset();
    }

    int getMode() const noexcept { return mode; }

    double process (double x) noexcept
    {
        if (mode == 0)
            return x;
        double u = x * gain;
        if (useLs) u = ls.process (u);
        if (usePk) u = pk.process (u);
        if (useHs) u = hs.process (u);
        double y = u;
        if (p.band)
        {
            const double b = bp.process (u);
            double phi = b >= 0.0 ? b - p.ap * p.tp * std::tanh (b / p.tp) : b - p.an * p.tn * std::tanh (b / p.tn);
            const double bc = std::clamp (b, -2.0, 2.0); // polynomial terms bounded beyond +6 dBFS (monotone)
            phi += p.g3 * bc * std::abs (bc) + p.g2 * bc * bc;
            y = u - b + phi;
        }
        const double yc = std::clamp (y, -2.0, 2.0);
        y += p.b2 * yc * yc + p.b3 * yc * std::abs (yc);
        if (p.xo != 0.0)
            y -= p.xo * std::tanh (y / p.xk);
        y = dc.process (y);
        // keep pathological input (e.g. full-scale garbage at +40 dB) from blowing up the polynomial terms
        if (! (std::abs (y) < 1.0e6))
        {
            reset();
            return 0.0;
        }
        return y;
    }

    // RMS of the mode's noise floor (linear).
    double noiseRms() const noexcept { return mode == 0 ? 0.0 : std::pow (10.0, p.noiseDbfs / 20.0); }

private:
    int mode = 0;
    AnalogModeParams p;
    double gain = 1.0;
    bool useLs = false, usePk = false, useHs = false;
    Biquad ls, pk, hs, bp, dc;
};

} // namespace craft::punch
