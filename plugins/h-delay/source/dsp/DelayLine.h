// Craft Hybrid Delay - circular delay line with band-limited fractional read (spec r1 section 8).
// Interpolation: Kaiser-windowed sinc, 16 taps, 512 tabulated phases with linear interpolation between
// phases (T. I. Laakso et al., "Splitting the unit delay", IEEE SPM 1996). Integer delays read the
// stored sample exactly, so unmodulated echoes land on exact samples (review decision 11.1).
#pragma once

#include <array>
#include <cmath>
#include <vector>

#include "Biquad.h"

namespace craft::hdelay
{

class SincTable
{
public:
    static constexpr int halfTaps = 8;
    static constexpr int taps = 2 * halfTaps;
    static constexpr int phases = 512;

    static const SincTable& get()
    {
        static const SincTable t;
        return t;
    }

    // coefficients for fractional position mu in [0, 1): weights of x[i0 - halfTaps + 1 + j], j = 0..taps-1
    // where the read position is i0 + mu.
    const double* row (int phase) const noexcept { return table.data() + size_t (phase) * taps; }

private:
    SincTable() : table (size_t (phases + 1) * taps)
    {
        const double beta = 7.0;
        auto bessel0 = [] (double x) {
            double sum = 1.0, term = 1.0;
            for (int k = 1; k < 40; ++k)
            {
                term *= (x / (2.0 * k)) * (x / (2.0 * k));
                sum += term;
            }
            return sum;
        };
        const double norm = bessel0 (beta);
        for (int p = 0; p <= phases; ++p)
        {
            const double mu = double (p) / phases;
            double sum = 0.0;
            for (int j = 0; j < taps; ++j)
            {
                const double t = double (j - (halfTaps - 1)) - mu; // tap offset from the read position
                const double sinc = std::abs (t) < 1.0e-12 ? 1.0 : std::sin (pi * t) / (pi * t);
                const double u = t / double (halfTaps);
                const double w = std::abs (u) >= 1.0 ? 0.0 : bessel0 (beta * std::sqrt (1.0 - u * u)) / norm;
                table[size_t (p) * taps + size_t (j)] = sinc * w;
                sum += sinc * w;
            }
            for (int j = 0; j < taps; ++j) // unity DC gain at every phase
                table[size_t (p) * taps + size_t (j)] /= sum;
        }
    }
    std::vector<double> table;
};

class DelayLine
{
public:
    void prepare (int maxDelaySamples)
    {
        int size = 1;
        while (size < maxDelaySamples + SincTable::taps + 4)
            size <<= 1;
        buffer.assign (size_t (size), 0.0);
        mask = size - 1;
        writePos = 0;
    }

    void reset() noexcept
    {
        std::fill (buffer.begin(), buffer.end(), 0.0);
        writePos = 0;
    }

    // Value written `delay` samples ago (delay >= SincTable::halfTaps). Call before push() for this sample.
    double read (double delay) const noexcept
    {
        const double pos = double (writePos) - delay; // fractional index of the wanted sample
        const double fl = std::floor (pos);
        const double mu = pos - fl;
        const int i0 = int (fl);
        if (mu == 0.0)
            return buffer[size_t (i0 & mask)];

        const auto& tab = SincTable::get();
        const double ph = mu * SincTable::phases;
        const int p = int (ph);
        const double f = ph - double (p);
        const double* r0 = tab.row (p);
        const double* r1 = tab.row (p + 1);
        double acc = 0.0;
        const int start = i0 - (SincTable::halfTaps - 1);
        for (int j = 0; j < SincTable::taps; ++j)
        {
            const double h = r0[j] + f * (r1[j] - r0[j]);
            acc += h * buffer[size_t ((start + j) & mask)];
        }
        return acc;
    }

    void push (double x) noexcept
    {
        buffer[size_t (writePos)] = x;
        writePos = (writePos + 1) & mask;
    }

private:
    std::vector<double> buffer { 0.0 };
    int mask = 0, writePos = 0;
};

} // namespace craft::hdelay
