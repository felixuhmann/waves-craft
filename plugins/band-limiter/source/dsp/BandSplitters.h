// Craft Band Limiter - the two five-band splitters (spec r1 section 4). Host independent.
//
// LinearPhaseSplitter: FFT overlap-save with zero-phase low-pass kernels. band1 = L1, band_b = L_b - L_(b-1),
// band5 = x - L4, so the bands telescope to the (delayed) input exactly.
//
// LowLatencySplitter: tree of low-pass / high-pass pairs built on one 2nd-order denominator D per crossover:
//   l = 1 / D(s)^2,  A = D(-s) / D(s),  h = A - l
// so l / A and h / A are real, >= 0 for Q <= 0.707, and l + h = A. The lower-order branches get the allpasses of the
// remaining crossovers, so every band has the same phase (A1 A2 A3 A4) and the sum is that allpass.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <vector>

#include "CrossoverTable.h"
#include "Parameters.h"

namespace craft::band_limiter::dsp
{

inline constexpr double pi = 3.14159265358979323846;

// Normalised low-pass shape L(f) for a crossover at fc (spec 4.1). The measured columns are interpolated linearly in
// log2(fc), so every table point is reproduced and the High crossover steepens continuously between 5113 and 12000 Hz.
class ShapeColumns
{
public:
    explicit ShapeColumns (double fc) noexcept
    {
        const int last = xotable::numColumns - 1;
        fc = std::clamp (fc, xotable::columnHz[0], xotable::columnHz[last]);
        int i = 0;
        while (i < last - 1 && fc > xotable::columnHz[i + 1])
            ++i;
        lower = i;
        weight = std::log2 (fc / xotable::columnHz[i]) / std::log2 (xotable::columnHz[i + 1] / xotable::columnHz[i]);
    }

    double at (double octaveOffset) const noexcept
    {
        using namespace xotable;
        const double p = (octaveOffset - firstOffset) / offsetStep;
        if (p <= 0.0)
        {
            // Below the table the complement keeps its measured 2nd-order slope (factor 4 per octave).
            const double edge = column (0);
            return 1.0 - (1.0 - edge) * std::pow (4.0, octaveOffset - firstOffset);
        }
        if (p >= double (numOffsets - 1))
            return 0.0;
        const int k = int (p);
        const double t = p - double (k);
        return column (k) + t * (column (k + 1) - column (k));
    }

private:
    double column (int k) const noexcept
    {
        return xotable::lowpass[k][lower] + weight * (xotable::lowpass[k][lower + 1] - xotable::lowpass[k][lower]);
    }

    int lower = 0;
    double weight = 0.0;
};

// In-place radix-2 complex FFT of a fixed size. No allocation after construction.
class Fft
{
public:
    explicit Fft (int size) : n (size), twiddle (size_t (size / 2)), reversed (size_t (size))
    {
        int bits = 0;
        while ((1 << bits) < n)
            ++bits;
        for (int i = 0; i < n / 2; ++i)
            twiddle[size_t (i)] = std::polar (1.0, -2.0 * pi * double (i) / double (n));
        for (int i = 0; i < n; ++i)
        {
            int r = 0;
            for (int b = 0; b < bits; ++b)
                if (i & (1 << b))
                    r |= 1 << (bits - 1 - b);
            reversed[size_t (i)] = r;
        }
    }

    void forward (std::complex<double>* x) const noexcept
    {
        for (int i = 0; i < n; ++i)
            if (i < reversed[size_t (i)])
                std::swap (x[i], x[reversed[size_t (i)]]);
        for (int len = 2; len <= n; len <<= 1)
        {
            const int half = len / 2, step = n / len;
            for (int i = 0; i < n; i += len)
                for (int j = 0; j < half; ++j)
                {
                    const auto& w = twiddle[size_t (j * step)];
                    auto& a = x[i + j];
                    auto& b = x[i + j + half];
                    const double re = b.real() * w.real() - b.imag() * w.imag();
                    const double im = b.real() * w.imag() + b.imag() * w.real();
                    b = { a.real() - re, a.imag() - im };
                    a = { a.real() + re, a.imag() + im };
                }
        }
    }

    void inverse (std::complex<double>* x) const noexcept // normalised
    {
        for (int i = 0; i < n; ++i)
            x[i] = { x[i].imag(), x[i].real() };
        forward (x);
        const double scale = 1.0 / double (n);
        for (int i = 0; i < n; ++i)
            x[i] = { x[i].imag() * scale, x[i].real() * scale };
    }

private:
    int n;
    std::vector<std::complex<double>> twiddle;
    std::vector<int> reversed;
};

class LinearPhaseSplitter
{
public:
    static constexpr int fftSize = 8192;
    static constexpr int hop = 512;
    static constexpr int halfLength = 2888; // FIR half length D; D + hop = 3400, the rest of 3528 is look-ahead
    static constexpr int latency = halfLength + hop;

    LinearPhaseSplitter() : fft (fftSize)
    {
        frame.resize (size_t (fftSize));
        spectrum.resize (size_t (fftSize));
        work.resize (size_t (fftSize));
        for (auto& k : kernels)
            k.resize (size_t (fftSize));
        previous.resize (size_t (fftSize));
        for (auto& c : chunk)
            c.resize (size_t (hop));
        window.resize (size_t (halfLength + 1));
        const int taper = halfLength / 4;
        for (int i = 0; i <= halfLength; ++i)
        {
            const int fromEnd = halfLength - i;
            window[size_t (i)] = fromEnd >= taper ? 1.0 : 0.5 - 0.5 * std::cos (pi * double (fromEnd) / double (taper));
        }
    }

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        reset();
    }

    // Takes effect one crossover per hop, cross-faded over one hop (allocation free).
    void setCrossovers (const std::array<double, numCrossovers>& hz) noexcept
    {
        wanted = hz;
        std::sort (wanted.begin(), wanted.end()); // out-of-order settings: sorted (spec open question 3)
    }

    void reset()
    {
        std::fill (frame.begin(), frame.end(), std::complex<double> {});
        for (auto& c : chunk)
            std::fill (c.begin(), c.end(), std::complex<double> {});
        writePos = fftSize - hop; // frame is linearised at every hop: new samples fill its last `hop` positions
        readPos = 0;
        for (int k = 0; k < numCrossovers; ++k)
        {
            current[size_t (k)] = wanted[size_t (k)];
            design (kernels[size_t (k)], wanted[size_t (k)]);
        }
    }

    void process (double left, double right, double (&bands)[numBands][2]) noexcept
    {
        for (int b = 0; b < numBands; ++b)
        {
            bands[b][0] = chunk[size_t (b)][size_t (readPos)].real();
            bands[b][1] = chunk[size_t (b)][size_t (readPos)].imag();
        }
        frame[size_t (writePos)] = { left, right };
        ++writePos;
        if (++readPos == hop)
        {
            computeHop();
            readPos = 0;
            writePos = fftSize - hop;
        }
    }

    // Zero-phase value of low-pass k's kernel at frequency f (tests): the realised, truncated design.
    double realisedLowpass (int k, double f) const noexcept
    {
        const double bin = std::clamp (f / fs * double (fftSize), 0.0, double (fftSize) / 2.0);
        const int i = std::min (int (bin), fftSize / 2 - 1);
        const double t = bin - double (i);
        const auto& h = kernels[size_t (k)];
        return h[size_t (i)].real() + t * (h[size_t (i + 1)].real() - h[size_t (i)].real());
    }

private:
    void design (std::vector<std::complex<double>>& out, double fc) noexcept
    {
        const ShapeColumns shape (fc);
        work[0] = 1.0;
        for (int i = 1; i <= fftSize / 2; ++i)
        {
            const double v = shape.at (std::log2 (double (i) * fs / double (fftSize) / fc));
            work[size_t (i)] = v;
            if (i < fftSize / 2)
                work[size_t (fftSize - i)] = v;
        }
        fft.inverse (work.data());
        out[0] = work[0].real();
        for (int i = 1; i < fftSize / 2; ++i)
        {
            const double h = i <= halfLength ? work[size_t (i)].real() * window[size_t (i)] : 0.0;
            out[size_t (i)] = h;
            out[size_t (fftSize - i)] = h;
        }
        out[size_t (fftSize / 2)] = 0.0;
        fft.forward (out.data());
        for (auto& v : out)
            v = v.real(); // exactly real: left stays in the real part, right in the imaginary part
    }

    // Takes the output region of the frame (last `hop` valid samples, delay D from the newest sample).
    void outputRegion (const std::vector<std::complex<double>>& spec, const std::vector<std::complex<double>>& kernel,
                       std::vector<std::complex<double>>& dest) noexcept
    {
        for (int i = 0; i < fftSize; ++i)
            dest[size_t (i)] = spec[size_t (i)] * kernel[size_t (i)].real();
        fft.inverse (dest.data());
    }

    void computeHop() noexcept
    {
        // One changed crossover per hop.
        int changed = -1;
        for (int k = 0; k < numCrossovers && changed < 0; ++k)
            if (std::abs (std::log2 (wanted[size_t (k)] / current[size_t (k)])) > 0.001)
                changed = k;
        if (changed >= 0)
        {
            previous.swap (kernels[size_t (changed)]);
            current[size_t (changed)] = wanted[size_t (changed)];
            design (kernels[size_t (changed)], wanted[size_t (changed)]);
        }

        std::copy (frame.begin(), frame.end(), spectrum.begin());
        fft.forward (spectrum.data());

        const int start = fftSize - halfLength - hop; // first output position of this hop
        std::array<std::complex<double>, hop> below;
        below.fill ({});
        for (int k = 0; k < numCrossovers; ++k)
        {
            outputRegion (spectrum, kernels[size_t (k)], work);
            if (k == changed)
            {
                std::array<std::complex<double>, hop> fresh;
                for (int i = 0; i < hop; ++i)
                    fresh[size_t (i)] = work[size_t (start + i)];
                outputRegion (spectrum, previous, work);
                for (int i = 0; i < hop; ++i)
                {
                    const double t = (double (i) + 0.5) / double (hop);
                    work[size_t (start + i)] = work[size_t (start + i)] * (1.0 - t) + fresh[size_t (i)] * t;
                }
            }
            for (int i = 0; i < hop; ++i)
            {
                const auto y = work[size_t (start + i)];
                chunk[size_t (k)][size_t (i)] = y - below[size_t (i)];
                below[size_t (i)] = y;
            }
        }
        for (int i = 0; i < hop; ++i)
            chunk[numBands - 1][size_t (i)] = frame[size_t (start + i)] - below[size_t (i)];

        std::copy (frame.begin() + hop, frame.end(), frame.begin()); // keep the history for the next hop
    }

    Fft fft;
    double fs = 48000.0;
    std::vector<std::complex<double>> frame, spectrum, work, previous;
    std::array<std::vector<std::complex<double>>, numCrossovers> kernels;
    std::array<std::vector<std::complex<double>>, numBands> chunk;
    std::vector<double> window;
    std::array<double, numCrossovers> wanted = defaultCrossoverHz, current = defaultCrossoverHz;
    int writePos = 0, readPos = 0;
};

// TPT state-variable section with the normalised denominator D(s) = s^2 + s/Q + 1.
struct SvfCoefficients
{
    double g = 0.1, k = 1.4, a1 = 0.0, a2 = 0.0, a3 = 0.0;

    void set (double f, double Q, double fs) noexcept
    {
        g = std::tan (pi * std::min (f, 0.45 * fs) / fs);
        k = 1.0 / Q;
        a1 = 1.0 / (1.0 + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
};

struct SvfState
{
    double ic1 = 0.0, ic2 = 0.0;
    void reset() noexcept { ic1 = ic2 = 0.0; }

    // lp = 1/D x, bp = s/D x (unit: normalised s)
    inline void tick (const SvfCoefficients& c, double x, double& lp, double& bp) noexcept
    {
        const double v3 = x - ic2;
        const double v1 = c.a1 * ic1 + c.a2 * v3;
        const double v2 = ic2 + c.a2 * ic1 + c.a3 * v3;
        ic1 = 2.0 * v1 - ic1;
        ic2 = 2.0 * v2 - ic2;
        bp = v1;
        lp = v2;
    }
};

class LowLatencySplitter
{
public:
    // Fit of the shape to tables/xo_lowlatency_bands.csv: section frequency relative to the crossover, and Q.
    static constexpr std::array<double, numCrossovers> relativeFrequency { 1.12714413, 1.26889458, 1.22373604, 1.0961271 };
    static constexpr std::array<double, numCrossovers> sectionQ { 0.60193379, 0.55919015, 0.57733092, 0.6129967 };

    void prepare (double sampleRate) noexcept
    {
        fs = sampleRate;
        rampCoef = 1.0 - std::exp (-double (updateInterval) / (0.01 * fs));
        reset();
    }

    void setCrossovers (const std::array<double, numCrossovers>& hz) noexcept
    {
        wanted = hz;
        std::sort (wanted.begin(), wanted.end());
    }

    void reset() noexcept
    {
        current = wanted;
        updateCoefficients();
        for (auto& ch : state)
            for (auto& s : ch)
                s.reset();
        countdown = updateInterval;
    }

    inline void process (double left, double right, double (&bands)[numBands][2]) noexcept
    {
        if (--countdown <= 0)
        {
            countdown = updateInterval;
            bool moved = false;
            for (int k = 0; k < numCrossovers; ++k)
                if (current[size_t (k)] != wanted[size_t (k)])
                {
                    double next = current[size_t (k)] * std::pow (wanted[size_t (k)] / current[size_t (k)], rampCoef);
                    if (std::abs (next / wanted[size_t (k)] - 1.0) < 1.0e-4)
                        next = wanted[size_t (k)];
                    current[size_t (k)] = next;
                    moved = true;
                }
            if (moved)
                updateCoefficients();
        }
        channel (0, left, bands);
        channel (1, right, bands);
    }

    int getCommonDelayEstimate() const noexcept { return 0; }

private:
    void updateCoefficients() noexcept
    {
        for (int k = 0; k < numCrossovers; ++k)
            coeff[size_t (k)].set (current[size_t (k)] * relativeFrequency[size_t (k)], sectionQ[size_t (k)], fs);
    }

    // Allpass A = (s^2 - s/Q + 1)/D = x - 2 (s/Q)/D x
    inline double allpass (int ch, int slot, int k, double x) noexcept
    {
        double lp, bp;
        state[size_t (ch)][size_t (slot)].tick (coeff[size_t (k)], x, lp, bp);
        return x - 2.0 * coeff[size_t (k)].k * bp;
    }

    inline void split (int ch, int slot, int k, double x, double& low, double& high) noexcept
    {
        double lp1, bp1, lp2, bp2;
        state[size_t (ch)][size_t (slot)].tick (coeff[size_t (k)], x, lp1, bp1);
        state[size_t (ch)][size_t (slot + 1)].tick (coeff[size_t (k)], lp1, lp2, bp2);
        low = lp2;
        high = (x - 2.0 * coeff[size_t (k)].k * bp1) - lp2;
    }

    inline void channel (int ch, double x, double (&bands)[numBands][2]) noexcept
    {
        double l4, h4, l3, h3, l2, h2, l1, h1;
        split (ch, 0, 3, x, l4, h4);
        split (ch, 2, 2, l4, l3, h3);
        split (ch, 4, 1, l3, l2, h2);
        split (ch, 6, 0, l2, l1, h1);
        bands[4][ch] = allpass (ch, 8, 0, allpass (ch, 9, 1, allpass (ch, 10, 2, h4)));
        bands[3][ch] = allpass (ch, 11, 0, allpass (ch, 12, 1, h3));
        bands[2][ch] = allpass (ch, 13, 0, h2);
        bands[1][ch] = h1;
        bands[0][ch] = l1;
    }

    static constexpr int updateInterval = 32;
    double fs = 48000.0, rampCoef = 0.1;
    std::array<double, numCrossovers> wanted = defaultCrossoverHz, current = defaultCrossoverHz;
    std::array<SvfCoefficients, numCrossovers> coeff;
    std::array<std::array<SvfState, 14>, 2> state;
    int countdown = updateInterval;
};

} // namespace craft::band_limiter::dsp
