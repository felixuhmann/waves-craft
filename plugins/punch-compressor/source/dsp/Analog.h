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
//        -> minus a 1.5 ms moving average of the even-order part of the above, so hot passages leave no DC (r2 section 10)
//        -> DC blocker (first-order high-pass, 0.5 Hz; only for DC in the input)
//        -> + shaped noise (uncorrelated per channel; AnalogNoise, r2 band table)
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
        s[2].b2 = 0.00055;
        s[2].band = true; s[2].bandHz = 1747.855936250817; s[2].bandQ = 0.15113660695942635;
        s[2].g3 = 0.03335335796887562; s[2].g2 = 0.001690237606849793;
        s[2].noiseDbfs = -82.7;
        // Mode 3: "starved" - a wide band around 2 kHz sits in an asymmetric soft dead zone: about -5 dB at low
        // level, opening towards 0 dB at full scale, with strong 2nd and 3rd harmonics; low-frequency lift
        s[3].gainDb = -1.668308465444538;
        s[3].lsHz = 340.8712946973226; s[3].lsQ = 0.7551489017557865; s[3].lsDb = 1.9647429317757306; // low Q: no ringing
        s[3].pkHz = 7939.796221598246; s[3].pkQ = 4.0; s[3].pkDb = 1.876994119520577;
        s[3].hsHz = 11881.257016661573; s[3].hsDb = 1.357281071011852;
        s[3].b2 = 0.005303230793736924; s[3].b3 = -0.024861051353062207;
        s[3].band = true; s[3].bandHz = 1913.5888786542303; s[3].bandQ = 0.26602617898918873;
        s[3].ap = 0.35064083330553625; s[3].tp = 0.047896504333868585;
        s[3].an = 0.3992829761506841; s[3].tn = 0.017499762453887225;
        s[3].g2 = -0.4092616034076529; s[3].g3 = 0.33229161101648885;
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
    static constexpr double dcBlockHz = 0.5;

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
        evenLen = std::clamp (int (std::lround (0.0015 * fs)), 1, int (evenBuf.size()));
        reset();
    }

    void reset() noexcept
    {
        ls.reset();
        pk.reset();
        hs.reset();
        bp.reset();
        dc.reset();
        evenBuf.fill (0.0);
        evenPos = 0;
        evenSum = 0.0;
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
        const double b = p.band ? bp.process (u) : 0.0;
        double y = shape (u, b);
        y -= pushEven (0.5 * (y + shape (-u, -b))); // the even-order part is the only source of DC
        y = dc.process (y);
        // keep pathological input (e.g. full-scale garbage at +40 dB) from blowing up the polynomial terms
        if (! (std::abs (y) < 1.0e6))
        {
            reset();
            return 0.0;
        }
        return y;
    }

private:
    // The memoryless part of the chain.
    double shape (double u, double b) const noexcept
    {
        double y = p.band ? u - b + bandCurve (b) : u;
        const double yc = std::clamp (y, -2.0, 2.0);
        y += p.b2 * yc * yc + p.b3 * yc * std::abs (yc);
        if (p.xo != 0.0)
            y -= p.xo * std::tanh (y / p.xk);
        return y;
    }

    double bandCurve (double b) const noexcept
    {
        double phi = b >= 0.0 ? b - p.ap * p.tp * std::tanh (b / p.tp) : b - p.an * p.tn * std::tanh (b / p.tn);
        const double bc = std::clamp (b, -2.0, 2.0); // polynomial terms bounded beyond +6 dBFS (monotone)
        phi += p.g3 * bc * std::abs (bc) + p.g2 * bc * bc;
        return phi;
    }

    // Moving average over exactly 1.5 ms: a finite tail, so nothing lingers after a level drop.
    double pushEven (double e) noexcept
    {
        evenSum += e - evenBuf[size_t (evenPos)];
        evenBuf[size_t (evenPos)] = e;
        if (++evenPos == evenLen)
        {
            evenPos = 0;
            double sum = 0.0;
            for (int i = 0; i < evenLen; ++i)
                sum += evenBuf[size_t (i)];
            evenSum = sum;
        }
        return evenSum / evenLen;
    }

    int mode = 0;
    AnalogModeParams p;
    double gain = 1.0;
    bool useLs = false, usePk = false, useHs = false;
    Biquad ls, pk, hs, bp, dc;
    std::array<double, 800> evenBuf {}; // fixed: mode changes happen on the audio thread
    int evenPos = 0, evenLen = 96;
    double evenSum = 0.0;
};

// Noise of one channel (r2 section 10): white noise through a chain of shelves that step between the density of
// neighbouring bands, scaled to the mode's total level. Coefficients are prepared once per mode.
class AnalogNoise
{
public:
    static constexpr int numShelves = 7;

    struct Shape
    {
        std::array<BiquadCoeffs, numShelves> shelves;
        double scale = 0.0; // input gain for unit-range uniform noise
    };

    static Shape makeShape (int mode, double fs) noexcept
    {
        // density re white of the same total, bands [f, 1.5 f), f = 20 / 63 / 200 / 630 / 2000 / 6300 / 16000 Hz
        static constexpr double density[5][7] = {
            {},
            { 18.0, 18.6, 9.4, 1.1, -4.0, -7.8, -15.5 },
            { 5.2, 3.4, -5.6, -13.1, -16.4, -10.9, 4.2 },
            { 25.3, 17.8, 8.4, -4.5, -9.2, -7.2, -4.4 },
            { 18.5, 16.9, 15.1, 7.6, -2.2, -11.6, -21.0 },
        };
        static constexpr double corner[numShelves - 1] = { 44.0, 138.0, 437.0, 1380.0, 4370.0, 12300.0 };
        // the 48 kHz shelves are squeezed towards Nyquist; these corners give the same bands at 88.2 kHz and above
        static constexpr double cornerHigh[numShelves - 1] = { 44.0, 138.0, 437.0, 1380.0, 8000.0, 10500.0 };
        Shape s;
        const int m = std::clamp (mode, 0, 4);
        if (m == 0)
            return s;
        // Absolute density is rate independent: the level is set from the chain at 48 kHz.
        const auto chain = [m] (double rate) {
            std::array<BiquadCoeffs, numShelves> c;
            for (int i = 0; i < numShelves - 1; ++i)
                c[size_t (i)] = BiquadCoeffs::highShelf (rate > 60000.0 ? cornerHigh[i] : corner[i], rate, 0.5, density[m][i + 1] - density[m][i]);
            // above the measured range at high rates: roll off so the total stays at the table value
            if (rate > 60000.0)
                c[size_t (numShelves - 1)] = BiquadCoeffs::highShelf (30000.0, rate, 0.7, -8.0);
            return c;
        };
        s.shelves = chain (fs);
        const auto reference = chain (48000.0);
        double power = 0.0;
        const int points = 2048;
        for (int k = 0; k < points; ++k)
        {
            const double w = pi * (k + 0.5) / points;
            double mag2 = 1.0;
            for (const auto& c : reference)
            {
                const double c1 = std::cos (w), s1 = std::sin (w), c2 = std::cos (2.0 * w), s2 = std::sin (2.0 * w);
                const double nr = c.b0 + c.b1 * c1 + c.b2 * c2, ni = c.b1 * s1 + c.b2 * s2;
                const double dr = 1.0 + c.a1 * c1 + c.a2 * c2, di = c.a1 * s1 + c.a2 * s2;
                mag2 *= (nr * nr + ni * ni) / (dr * dr + di * di);
            }
            power += mag2;
        }
        const double rms = std::pow (10.0, analogModeParams()[size_t (m)].noiseDbfs / 20.0);
        s.scale = rms * std::sqrt (3.0 * fs / 48000.0 / (power / points)); // uniform [-1, 1) has RMS 1/sqrt(3)
        return s;
    }

    void setShape (const Shape& s) noexcept
    {
        scale = s.scale;
        for (int i = 0; i < numShelves; ++i)
            f[size_t (i)].setCoeffs (s.shelves[size_t (i)]);
    }

    void seed (std::uint32_t v) noexcept { white.seed (v); }

    double next() noexcept
    {
        if (scale == 0.0)
            return 0.0;
        double x = scale * white.next();
        for (auto& b : f)
            x = b.process (x);
        return x;
    }

private:
    WhiteNoise white;
    std::array<Biquad, numShelves> f;
    double scale = 0.0;
};

} // namespace craft::punch
