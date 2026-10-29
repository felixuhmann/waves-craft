// Craft Mastering Limiter - requantiser with dither and error-feedback noise shaping (spec r1 sections 6, 7).
// Host independent, one instance per channel.
//
//   u = v + sum_{k>=1} h[k] e[n-k]          (error feedback through the NTF tail)
//   y = D * round((u + d) / D)              (D = 2^(1-b), d = dither)
//   e = y - u                               (white: dither + rounding error)
// so the total output error y - v = H(z) e with the monic minimum-phase NTF H (spec 7.3).
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

#include "NoiseShapingCoefficients.h"
#include "Parameters.h"

namespace craft::mastering_limiter::dsp
{

// Small, fast PRNG (xoshiro128+ style mixing of a 64-bit state; public-domain algorithm family).
class Random
{
public:
    explicit Random (std::uint64_t seed = 0x9e3779b97f4a7c15ull) { setSeed (seed); }

    void setSeed (std::uint64_t seed) noexcept
    {
        s0 = splitmix (seed);
        s1 = splitmix (seed);
        if (s0 == 0 && s1 == 0)
            s1 = 1;
    }

    std::uint64_t next() noexcept // xorshift128+
    {
        std::uint64_t x = s0;
        const std::uint64_t y = s1;
        s0 = y;
        x ^= x << 23;
        s1 = x ^ y ^ (x >> 17) ^ (y >> 26);
        return s1 + y;
    }

    // uniform in [-0.5, 0.5), 53-bit resolution
    double uniformCentred() noexcept { return double (next() >> 11) * (1.0 / 9007199254740992.0) - 0.5; }

private:
    std::uint64_t splitmix (std::uint64_t& x) noexcept
    {
        std::uint64_t z = (x += 0x9e3779b97f4a7c15ull);
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
        return z ^ (z >> 31);
    }

    std::uint64_t s0 = 1, s1 = 2;
};

// Index of the NTF design for a sample rate (nearest designed rate, log distance).
inline int ntfRateIndex (double sampleRate) noexcept
{
    int best = 0;
    double bestDist = 1.0e9;
    for (int i = 0; i < ntf::numRates; ++i)
    {
        const double d = std::abs (std::log (sampleRate / ntf::designRates[size_t (i)]));
        if (d < bestDist)
        {
            bestDist = d;
            best = i;
        }
    }
    return best;
}

class Requantiser
{
public:
    static constexpr int histLen = ntf::numTaps - 1;

    // Type 2 dither (spec 7.1, 12 D3; the mechanism is open question 2):
    //  - no shaping: RPDF +-1/2 LSB (total error 0.41 LSB rms);
    //  - shaping, moving signal: small TPDF of +-a LSB (a per shaping mode). A moving signal self-dithers the
    //    shaped quantiser, so this little extra keeps the shaped error about 4.8 dB below Type 1 (spec 7.1/7.2)
    //    while decorrelating the rounding error from the signal;
    //  - shaping, static non-zero input (range < staticRangeLsb over the last 32..64 samples): the shaper would
    //    fall into limit cycles / idle tones (D3), so RPDF +-1/2 LSB is used instead;
    //  - exact digital silence: no dither, output exactly zero (see process()).
    static constexpr double type2ShapedAmplitude[4] = { 0.5, 0.2, 0.18, 0.15 };
    static constexpr double staticRangeLsb = 2.0;
    static constexpr int activityBlock = 32;

    void prepare (double sampleRate, std::uint64_t seed)
    {
        rateIndex = ntfRateIndex (sampleRate);
        rng.setSeed (seed);
        reset();
    }

    void reset() noexcept
    {
        hist.fill (0.0);
        pos = 0;
        lastError = 0.0;
        blockMin = 1.0e300;
        blockMax = -1.0e300;
        blockCount = 0;
        prevRange = 0.0;
    }

    void setFormat (int bits, Dither d, Shaping s) noexcept
    {
        bits = bits < 1 ? 1 : (bits > 32 ? 32 : bits);
        lsb = std::ldexp (1.0, 1 - bits);
        invLsb = std::ldexp (1.0, bits - 1);
        if (s != shaping)
            reset(); // switching the NTF at a block boundary: start from an empty error history
        dither = d;
        shaping = s;
        coeffs = shaping == Shaping::None ? nullptr : ntf::coefficients[rateIndex][int (shaping) - 1];
    }

    double getLsb() const noexcept { return lsb; }
    double getLastError() const noexcept { return lastError; } // e[n] in LSB (tests)

    double process (double v) noexcept
    {
        // Digital silence stays exactly zero without Type 1 dither (spec 6, 7.1, 12 D3).
        if (v == 0.0 && dither != Dither::Type1)
        {
            push (0.0);
            lastError = 0.0;
            return 0.0;
        }

        double u = v;
        if (coeffs != nullptr)
        {
            // hist holds e[n-1], e[n-2], ... (in LSB) starting at pos, duplicated for contiguous access
            const double* e = hist.data() + pos;
            double acc = 0.0;
            for (int k = 0; k < histLen; ++k)
                acc += coeffs[k + 1] * e[k];
            u += acc * lsb;
        }

        double d = 0.0; // in LSB
        switch (dither)
        {
            case Dither::None: break;
            case Dither::Type1: d = rng.uniformCentred() + rng.uniformCentred(); break; // TPDF +-1 LSB
            case Dither::Type2:
            {
                const double vq = v * invLsb;
                blockMin = std::min (blockMin, vq);
                blockMax = std::max (blockMax, vq);
                const bool moving = prevRange >= staticRangeLsb || blockMax - blockMin >= staticRangeLsb;
                if (++blockCount == activityBlock)
                {
                    prevRange = blockMax - blockMin;
                    blockMin = 1.0e300;
                    blockMax = -1.0e300;
                    blockCount = 0;
                }
                if (shaping != Shaping::None && moving)
                    d = type2ShapedAmplitude[int (shaping)] * (rng.uniformCentred() + rng.uniformCentred());
                else
                    d = rng.uniformCentred(); // RPDF +-1/2 LSB
                break;
            }
        }

        const double uq = u * invLsb; // exact scaling by a power of two
        const double yq = std::nearbyint (uq + d);
        const double err = yq - uq;
        push (err);
        lastError = err;

        const double y = yq * lsb;
        return y > 1.0 ? 1.0 : (y < -1.0 ? -1.0 : y); // clip at full scale (spec 6, implementer rule)
    }

private:
    void push (double e) noexcept
    {
        pos = (pos == 0 ? histLen : pos) - 1;
        hist[size_t (pos)] = e;
        hist[size_t (pos + histLen)] = e;
    }

    std::array<double, 2 * histLen> hist {};
    int pos = 0;
    const double* coeffs = nullptr;
    int rateIndex = 1;
    double lsb = std::ldexp (1.0, -23), invLsb = std::ldexp (1.0, 23);
    Dither dither = Dither::None;
    Shaping shaping = Shaping::None;
    Random rng;
    double lastError = 0.0;
    double blockMin = 1.0e300, blockMax = -1.0e300, prevRange = 0.0; // input activity (in LSB)
    int blockCount = 0;
};

} // namespace craft::mastering_limiter::dsp
