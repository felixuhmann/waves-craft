// Craft Colour Delay - reverb colour (spec r1 section 7.1, [target]).
//
// Own design: pre-delay -> per-channel series all-pass diffusers -> 16-line feedback delay network (FDN) with an
// orthogonal Hadamard feedback matrix (Jot / Stautner-Puckette style). Each line has a frequency-dependent
// absorption filter (broadband gain x first-order low shelf x two first-order high shelves) whose gains are fitted
// per line so that the attenuation per pass equals -60 dB * length / (RT60(f) * fs) at the seven octave-band
// centres of the spec's RT60 table (interpolated over Size). Outputs are taken with two orthogonal sign vectors,
// so the left and right tails are decorrelated; the output gain is normalised from the designed decay so that the
// impulse-response energy follows the spec (-0.7 dB at Size 1 % ... +0.8 dB at Size 100 %).
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <mutex>
#include <vector>

#include "Filters.h"
#include "Smoothing.h"

namespace craft::cdl
{

class Reverb
{
public:
    static constexpr int numLines = 16;
    static constexpr int numBands = 7;
    static constexpr std::array<double, numBands> bandHz { 125.0, 250.0, 500.0, 1000.0, 2000.0, 4000.0, 8000.0 };
    static constexpr std::array<double, 5> sizeRows { 1.0, 25.0, 50.0, 75.0, 100.0 };
    static constexpr double rt60Table[5][numBands] = {
        { 0.93, 0.89, 0.84, 0.83, 0.72, 0.61, 0.48 },
        { 1.56, 1.53, 1.46, 1.36, 1.27, 1.10, 0.87 },
        { 3.05, 2.99, 2.90, 2.74, 2.44, 2.05, 1.61 },
        { 3.80, 3.86, 3.55, 3.33, 3.06, 2.60, 2.02 },
        { 4.92, 4.65, 4.50, 4.20, 3.84, 3.25, 2.53 },
    };

    static std::array<double, numBands> rt60ForSize (double size) noexcept
    {
        size = std::clamp (size, 1.0, 100.0);
        int row = 0;
        while (row < 3 && size > sizeRows[(size_t) row + 1])
            ++row;
        const double t = (size - sizeRows[(size_t) row]) / (sizeRows[(size_t) row + 1] - sizeRows[(size_t) row]);
        std::array<double, numBands> rt {};
        for (int b = 0; b < numBands; ++b)
            rt[(size_t) b] = rt60Table[row][b] + t * (rt60Table[row + 1][b] - rt60Table[row][b]);
        return rt;
    }

    static double preDelayMs (double size) noexcept { return 13.0 + 0.11 * size; }
    // Impulse-response energy E = sum(hL^2 + hR^2) for a left-only impulse (spec r2 section 7.1), by Size row.
    static double energyTargetDb (double size) noexcept { return interpRows (size, { -0.7, -0.1, 0.5, 0.7, 0.8 }); }

    // Output tone shaping: first-order high shelf at 3 kHz (gain over Size), a gentle -1.3 dB dip at 250 Hz, a 38 Hz high-pass and,
    // at sample rates above 60 kHz, a 2nd-order low-pass at 20 kHz (keeps the response in the audio band, so the
    // energy does not depend on the sample rate; at 44.1/48 kHz a -1.8 dB dip at 15 kHz stands in for its in-band
    // effect). The pre-delay settles on whole samples (no interpolation loss). Together with the RT60-dependent energy per band this gives the
    // spec's octave-band energy profile (energy density per band re 1 kHz).
    static constexpr double toneShelfHz = 3000.0, bandLimitHz = 20000.0;
    static double toneShelfDb (double size) noexcept { return interpRows (size, { -3.3, -3.0, -2.8, -2.6, -2.4 }); }

    static double interpRows (double size, std::array<double, 5> v) noexcept
    {
        size = std::clamp (size, 1.0, 100.0);
        int row = 0;
        while (row < 3 && size > sizeRows[(size_t) row + 1])
            ++row;
        const double t = (size - sizeRows[(size_t) row]) / (sizeRows[(size_t) row + 1] - sizeRows[(size_t) row]);
        return v[(size_t) row] + t * (v[(size_t) row + 1] - v[(size_t) row]);
    }

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        const double scale = fs / 48000.0;

        // FDN line lengths (samples at 48 kHz), mutually prime, ~11 ... 41 ms
        static constexpr int baseLengths[numLines] = { 509,  571,  631,  701,  761,  829,  887,  953,
                                                       1021, 1091, 1151, 1223, 1291, 1361, 1427, 1499 };
        for (int i = 0; i < numLines; ++i)
        {
            lengths[(size_t) i] = std::max (8, (int) std::lround (baseLengths[i] * 1.36 * scale));
            lines[(size_t) i].assign ((size_t) lengths[(size_t) i], 0.0);
        }

        // diffusers (per input channel), lengths in samples at 48 kHz
        static constexpr int diffLengths[2][numDiffusers] = { { 142, 107, 379, 277 }, { 151, 113, 359, 293 } };
        for (int c = 0; c < 2; ++c)
            for (int d = 0; d < numDiffusers; ++d)
                diffusers[(size_t) c][(size_t) d].buf.assign ((size_t) std::max (2, (int) std::lround (diffLengths[c][d] * scale)), 0.0);

        preL.assign ((size_t) (std::ceil (0.030 * fs) + 4), 0.0);
        preR.assign (preL.size(), 0.0);
        preDelaySmoother.setTimeConstant (0.05, fs);
        for (auto& b : bandLimit)
            b = Biquad();
        if (fs > 60000.0)
            for (auto& b : bandLimit)
                b.setLowPass (bandLimitHz, 0.7071, fs);
        else // equivalent in-band effect of the 20 kHz low-pass where there is no room for it below Nyquist
            for (auto& b : bandLimit)
                b.setPeak (15000.0, 0.9, -1.8, fs);
        for (auto& b : lowMidDip)
            b.setPeak (250.0, 1.0, -1.3, fs);
        for (auto& b : lowCut)
            b.setHighPass (38.0, 0.7071, fs);

        // Input / output vectors from mutually orthogonal Hadamard rows, scrambled by a fixed random sign diagonal:
        //   inL = D h3, inR = D h5, outL = directMix D h3 + sqrt(1 - directMix^2) D h6, outR likewise with h5 / h9.
        // So <outL, inL> = <outR, inR> = directMix (diffused onset at the pre-delay), <outL, inR> = <outR, inL> = 0
        // and <outL, outR> = 0 (decorrelated outputs).
        std::array<double, numLines> sign {};
        unsigned seed = 0x2545F491u;
        for (auto& v : sign)
        {
            seed = seed * 1664525u + 1013904223u;
            v = ((seed >> 16) & 1u) ? 1.0 : -1.0;
        }
        auto row = [&sign] (int k, int i) {
            int bits = k & i, parity = 0;
            while (bits) { parity ^= bits & 1; bits >>= 1; }
            return sign[(size_t) i] * (parity ? -0.25 : 0.25);
        };
        const double other = std::sqrt (1.0 - directMix * directMix);
        for (int i = 0; i < numLines; ++i)
        {
            inL[(size_t) i] = row (3, i);
            inR[(size_t) i] = row (5, i);
            outL[(size_t) i] = directMix * row (3, i) + other * row (6, i);
            outR[(size_t) i] = directMix * row (5, i) + other * row (9, i);
        }

        sizeSmoother.setTimeConstant (0.05, fs);
        calibrate();
        designedSize = -1.0;
        setSize (54.9);
        sizeSmoother.snapToTarget();
        updateDesign (sizeSmoother.getCurrent());
        reset();
    }

    void reset() noexcept
    {
        for (auto& l : lines)
            std::fill (l.begin(), l.end(), 0.0);
        std::fill (pos.begin(), pos.end(), 0);
        for (auto& ch : diffusers)
            for (auto& d : ch)
            {
                std::fill (d.buf.begin(), d.buf.end(), 0.0);
                d.pos = 0;
            }
        for (auto& f : absorb)
            for (auto& s : f)
                s.reset();
        std::fill (preL.begin(), preL.end(), 0.0);
        std::fill (preR.begin(), preR.end(), 0.0);
        for (auto& f : toneShelf) f.reset();
        for (auto& b : bandLimit) b.reset();
        for (auto& b : lowMidDip) b.reset();
        for (auto& b : lowCut) b.reset();
        preWrite = 0;
        preDelaySmoother.snapToTarget();
    }

    void setSize (double size) noexcept
    {
        sizeSmoother.setTarget (std::clamp (size, 1.0, 100.0));
        preDelaySmoother.setTarget (std::round (preDelayMs (sizeSmoother.getTarget()) * 0.001 * fs)); // whole samples once settled (no interpolation loss)
    }

    void snapParameters() noexcept
    {
        sizeSmoother.snapToTarget();
        preDelaySmoother.snapToTarget();
        updateDesign (sizeSmoother.getCurrent());
    }

    void process (double inLeft, double inRight, double& outLeft, double& outRight) noexcept
    {
        if (! sizeSmoother.isSettled() && (++designCounter & 63) == 0)
            updateDesign (sizeSmoother.next());
        else if (! sizeSmoother.isSettled())
            sizeSmoother.next();
        else if (designedSize != sizeSmoother.getCurrent())
            updateDesign (sizeSmoother.getCurrent());

        // pre-delay (fractional, linear interpolation; smoothed when Size moves)
        const double pd = preDelaySmoother.next();
        const double l = readPreDelay (preL, inLeft, pd);
        const double r = readPreDelay (preR, inRight, pd);
        if (++preWrite == (int) preL.size())
            preWrite = 0;

        // diffusion
        double dl = l, dr = r;
        for (auto& d : diffusers[0])
            dl = d.process (dl, diffusion);
        for (auto& d : diffusers[1])
            dr = d.process (dr, diffusion);

        // FDN: read line outputs, filter, mix, inject input, write
        std::array<double, numLines> x {};
        for (int i = 0; i < numLines; ++i)
        {
            const auto& line = lines[(size_t) i];
            double v = line[(size_t) pos[(size_t) i]];
            auto& f = absorb[(size_t) i];
            v = f[2].process (f[1].process (f[0].process (v))) * lineGain[(size_t) i];
            x[(size_t) i] = v;
        }
        feedbackMatrix (x);
        double yl = 0.0, yr = 0.0;
        for (int i = 0; i < numLines; ++i)
        {
            const double s = x[(size_t) i] + inL[(size_t) i] * dl + inR[(size_t) i] * dr;
            auto& line = lines[(size_t) i];
            line[(size_t) pos[(size_t) i]] = s;
            if (++pos[(size_t) i] == lengths[(size_t) i])
                pos[(size_t) i] = 0;
            yl += outL[(size_t) i] * s;
            yr += outR[(size_t) i] * s;
        }
        outLeft = bandLimit[0].process (lowCut[0].process (lowMidDip[0].process (toneShelf[0].process (yl * outputGain))));
        outRight = bandLimit[1].process (lowCut[1].process (lowMidDip[1].process (toneShelf[1].process (yr * outputGain))));
    }

    // design inspection (tests)
    double getOutputGain() const noexcept { return outputGain; }
    double designedAttenuationDb (int line, double hz) const noexcept
    {
        const double w = 2.0 * pi * hz / fs;
        const auto& f = absorb[(size_t) line];
        return 20.0 * std::log10 (lineGain[(size_t) line] * std::abs (f[0].response (w) * f[1].response (w) * f[2].response (w)));
    }
    int getLineLength (int line) const noexcept { return lengths[(size_t) line]; }

private:
    static constexpr int numDiffusers = 4;
    static constexpr double diffusion = 0.62;
    static constexpr double directMix = 0.12;
    static constexpr double shelfHz[3] = { 300.0, 1800.0, 5500.0 }; // low shelf, high shelf, high shelf

    struct AllPass
    {
        std::vector<double> buf;
        int pos = 0;
        double process (double x, double g) noexcept
        {
            const double d = buf[(size_t) pos];
            const double v = x + g * d;
            buf[(size_t) pos] = v;
            if (++pos == (int) buf.size())
                pos = 0;
            return d - g * v;
        }
    };

    // per-channel circular buffers sharing the write index
    double readPreDelay (std::vector<double>& buf, double x, double d) noexcept
    {
        buf[(size_t) preWrite] = x;
        const int n = (int) buf.size();
        const int di = (int) d;
        const double fr = d - di;
        int i0 = preWrite - di;
        if (i0 < 0) i0 += n;
        int i1 = i0 - 1;
        if (i1 < 0) i1 += n;
        return buf[(size_t) i0] + fr * (buf[(size_t) i1] - buf[(size_t) i0]);
    }

    static void hadamard (std::array<double, numLines>& x) noexcept
    {
        for (int h = 1; h < numLines; h <<= 1)
            for (int i = 0; i < numLines; i += h << 1)
                for (int j = i; j < i + h; ++j)
                {
                    const double a = x[(size_t) j], b = x[(size_t) j + (size_t) h];
                    x[(size_t) j] = a + b;
                    x[(size_t) j + (size_t) h] = a - b;
                }
        for (auto& v : x)
            v *= 0.25; // 1/sqrt(16): orthogonal
    }

    // Feedback matrix: normalised Hadamard followed by a signed cyclic shift (still orthogonal). The plain
    // Hadamard matrix is involutory (H H = I), which would send energy back to the same lines every second pass.
    static void feedbackMatrix (std::array<double, numLines>& x) noexcept
    {
        hadamard (x);
        static constexpr double sgn[numLines] = { 1, -1, 1, 1, -1, 1, -1, -1, 1, 1, -1, 1, 1, -1, -1, 1 };
        const double first = x[0];
        for (int i = 0; i < numLines - 1; ++i)
            x[(size_t) i] = sgn[i] * x[(size_t) i + 1];
        x[numLines - 1] = sgn[numLines - 1] * first;
    }

    // first-order shelving sections, bilinear with prewarping at fc (unit-gain at the other end):
    //   high shelf (s sqrt V + 1) / (s / sqrt V + 1), low shelf (s + sqrt V) / (s + 1 / sqrt V)
    static void setHighShelf (FirstOrder& f, double fc, double gainDb, double fs) noexcept
    {
        const double k = std::tan (pi * fc / fs), rv = std::pow (10.0, gainDb / 40.0);
        const double d0 = 1.0 / rv + k, d1 = k - 1.0 / rv;
        f.b0 = (rv + k) / d0;
        f.b1 = (k - rv) / d0;
        f.a1 = d1 / d0;
    }
    static void setLowShelf (FirstOrder& f, double fc, double gainDb, double fs) noexcept
    {
        const double k = std::tan (pi * fc / fs), rv = std::pow (10.0, gainDb / 40.0);
        const double d0 = 1.0 + k / rv, d1 = k / rv - 1.0;
        f.b0 = (1.0 + k * rv) / d0;
        f.b1 = (k * rv - 1.0) / d0;
        f.a1 = d1 / d0;
    }

    // Fits broadband gain c and shelf gains (dB) to the per-pass attenuation target at the octave centres:
    // linear least squares on the small-gain sensitivities, refined by three Gauss-Newton steps on the exact response.
    void designLine (int i, const std::array<double, numBands>& rt)
    {
        // fit points: the seven octave centres plus 16 kHz (RT extrapolated geometrically from 4 and 8 kHz) when it
        // lies below 0.45 fs, so the high-frequency decay does not depend on the sample rate
        constexpr int maxPts = numBands + 1;
        double hz[maxPts], target[maxPts];
        int nb = 0;
        for (int b = 0; b < numBands; ++b, ++nb)
        {
            hz[nb] = bandHz[(size_t) b];
            target[nb] = -60.0 * lengths[(size_t) i] / (rt[(size_t) b] * fs);
        }
        if (16000.0 < 0.45 * fs)
        {
            const double rt16 = rt[6] * rt[6] / rt[5];
            hz[nb] = 16000.0;
            target[nb] = -60.0 * lengths[(size_t) i] / (rt16 * fs);
            ++nb;
        }

        // sensitivities: d(dB)/d(gain dB) at zero gain
        double J[maxPts][4];
        for (int b = 0; b < nb; ++b)
        {
            const double t = std::tan (pi * hz[b] / fs);
            J[b][0] = 1.0;
            for (int s = 0; s < 3; ++s)
            {
                const double kc = std::tan (pi * shelfHz[s] / fs);
                const double x = (t / kc) * (t / kc);
                J[b][s + 1] = s == 0 ? 1.0 / (1.0 + x) : x / (1.0 + x);
            }
        }

        std::array<double, 4> p { 0.0, 0.0, 0.0, 0.0 };
        auto& f = absorb[(size_t) i];
        for (int iter = 0; iter < 4; ++iter)
        {
            setLowShelf (f[0], shelfHz[0], p[1], fs);
            setHighShelf (f[1], shelfHz[1], p[2], fs);
            setHighShelf (f[2], shelfHz[2], p[3], fs);
            double res[maxPts];
            for (int b = 0; b < nb; ++b)
            {
                const double w = 2.0 * pi * hz[b] / fs;
                const double db = p[0] + 20.0 * std::log10 (std::abs (f[0].response (w) * f[1].response (w) * f[2].response (w)));
                res[b] = target[b] - db;
            }
            // normal equations (4x4)
            double A[4][5] {};
            for (int r = 0; r < 4; ++r)
            {
                for (int c = 0; c < 4; ++c)
                    for (int b = 0; b < nb; ++b)
                        A[r][c] += J[b][r] * J[b][c];
                for (int b = 0; b < nb; ++b)
                    A[r][4] += J[b][r] * res[b];
                A[r][r] += 1.0e-9;
            }
            for (int c = 0; c < 4; ++c) // Gauss-Jordan
            {
                int piv = c;
                for (int r = c + 1; r < 4; ++r)
                    if (std::abs (A[r][c]) > std::abs (A[piv][c])) piv = r;
                for (int k = 0; k < 5; ++k) std::swap (A[c][k], A[piv][k]);
                for (int r = 0; r < 4; ++r)
                    if (r != c)
                    {
                        const double m = A[r][c] / A[c][c];
                        for (int k = c; k < 5; ++k) A[r][k] -= m * A[c][k];
                    }
            }
            for (int c = 0; c < 4; ++c)
                p[(size_t) c] += A[c][4] / A[c][c];
        }
        setLowShelf (f[0], shelfHz[0], p[1], fs);
        setHighShelf (f[1], shelfHz[1], p[2], fs);
        setHighShelf (f[2], shelfHz[2], p[3], fs);
        lineGain[(size_t) i] = std::pow (10.0, p[0] / 20.0);
    }

    void updateDesign (double size)
    {
        designedSize = size;
        for (auto& f : toneShelf)
            setHighShelf (f, toneShelfHz, toneShelfDb (size), fs);
        const auto rt = rt60ForSize (size);
        for (int i = 0; i < numLines; ++i)
            designLine (i, rt);

        // energy normalisation: an impulse injected with unit energy circulates with total energy
        // ~ 1 / (1 - g^2(f)) summed over passes; the output vector picks 1/N of it per pass.
        constexpr int numFreqs = 48;
        double acc = 0.0;
        for (int k = 0; k < numFreqs; ++k)
        {
            const double hz = (k + 0.5) / numFreqs * 0.5 * fs;
            const double w = 2.0 * pi * hz / fs;
            double g2 = 0.0;
            for (int i = 0; i < numLines; ++i)
            {
                const auto& f = absorb[(size_t) i];
                const double g = lineGain[(size_t) i] * std::abs (f[0].response (w) * f[1].response (w) * f[2].response (w));
                g2 += g * g;
            }
            g2 /= numLines;
            acc += g2 / std::max (1.0e-6, 1.0 - g2);
        }
        // direct (diffused) part + recirculating part
        const double energy = directMix * directMix + acc / numFreqs / numLines;
        outputGain = std::pow (10.0, (energyTargetDb (size) + energyCorrectionDb (size)) / 20.0) / std::sqrt (energy);
    }

    // Correction of the energy model above (which assumes that every pass spreads evenly over the lines; the
    // coherent sums of equal-length paths make the real energy deviate by up to a few dB, depending on the exact
    // line lengths, i.e. on the sample rate). It is measured once per sample rate on the impulse response of this
    // network at the five Size rows (mean of the L and R energies for a one-sided impulse) and cached.
    double energyCorrectionDb (double size) const noexcept
    {
        size = std::clamp (size, 1.0, 100.0);
        int row = 0;
        while (row < 3 && size > sizeRows[(size_t) row + 1])
            ++row;
        const double t = (size - sizeRows[(size_t) row]) / (sizeRows[(size_t) row + 1] - sizeRows[(size_t) row]);
        return corrDb[(size_t) row] + t * (corrDb[(size_t) row + 1] - corrDb[(size_t) row]);
    }

    void calibrate()
    {
        static std::mutex mutex;
        static std::map<long, std::array<double, 5>> cache;
        const long key = std::lround (fs);
        {
            const std::lock_guard<std::mutex> lock (mutex);
            if (auto it = cache.find (key); it != cache.end())
            {
                corrDb = it->second;
                return;
            }
        }
        std::array<double, 5> result {};
        for (size_t row = 0; row < 5; ++row)
        {
            corrDb.fill (0.0);
            const double size = sizeRows[row];
            sizeSmoother.reset (size);
            preDelaySmoother.reset (std::round (preDelayMs (size) * 0.001 * fs));
            updateDesign (size);
            reset();
            double total = 0.0, window = 0.0;
            const int windowLen = (int) (0.1 * fs), maxLen = (int) (10.0 * fs);
            for (int n = 0; n < maxLen; ++n)
            {
                double l = 0.0, r = 0.0;
                process (n == 0 ? 1.0 : 0.0, 0.0, l, r);
                const double e = l * l + r * r;
                total += e;
                window += e;
                if ((n + 1) % windowLen == 0)
                {
                    if (n > (int) (0.2 * fs) && window < 1.0e-7 * total)
                        break;
                    window = 0.0;
                }
            }
            result[row] = energyTargetDb (size) - 10.0 * std::log10 (std::max (total, 1.0e-30));
        }
        corrDb = result;
        const std::lock_guard<std::mutex> lock (mutex);
        cache[key] = result;
    }

    std::array<double, 5> corrDb {};
    double fs = 48000.0;
    std::array<std::vector<double>, numLines> lines;
    std::array<int, numLines> lengths {}, pos {};
    std::array<std::array<FirstOrder, 3>, numLines> absorb;
    std::array<double, numLines> lineGain {};
    std::array<double, numLines> inL {}, inR {}, outL {}, outR {};
    std::array<std::array<AllPass, numDiffusers>, 2> diffusers;
    std::vector<double> preL, preR;
    int preWrite = 0;
    OnePoleSmoother sizeSmoother, preDelaySmoother;
    std::array<FirstOrder, 2> toneShelf;
    std::array<Biquad, 2> bandLimit, lowMidDip, lowCut;
    double designedSize = -1.0, outputGain = 1.0;
    unsigned designCounter = 0;
};

} // namespace craft::cdl
