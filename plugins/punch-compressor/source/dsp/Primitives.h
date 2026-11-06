// Craft Punch Compressor - small DSP building blocks (host-independent, no JUCE).
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace craft::punch
{

inline constexpr double pi = 3.14159265358979323846;

// Look-ahead H per sample rate (spec section 12): 64 at 44.1/48 kHz, 128 at 88.2/96 kHz, 256 above
// (extrapolated). Rates below 44.1 kHz use 64 as well; rates between the steps use the lower step.
inline int lookaheadForSampleRate (double fs) noexcept
{
    if (fs < 80000.0) return 64;
    if (fs < 160000.0) return 128;
    return 256;
}

// Fixed-length delay line (delay >= 0). push() returns the value written `delay` pushes ago.
template <typename T>
class DelayLine
{
public:
    void prepare (int delaySamples)
    {
        delay = std::max (0, delaySamples);
        buffer.assign (static_cast<size_t> (delay) + 1, T {});
        pos = 0;
    }
    void reset (T v = T {}) noexcept
    {
        std::fill (buffer.begin(), buffer.end(), v);
        pos = 0;
    }
    T push (T v) noexcept
    {
        if (delay == 0)
            return v;
        buffer[pos] = v;
        pos = pos + 1 == buffer.size() ? 0 : pos + 1;
        return buffer[pos];
    }
    int getDelay() const noexcept { return delay; }

private:
    std::vector<T> buffer { T {} };
    size_t pos = 0;
    int delay = 0;
};

// Delay line with a variable integer delay 0 ... maxDelay (punch side-chain delay).
class VariableDelay
{
public:
    void prepare (int maxDelay)
    {
        buffer.assign (static_cast<size_t> (std::max (0, maxDelay)) + 1, 0.0);
        pos = 0;
    }
    void reset (double v) noexcept
    {
        std::fill (buffer.begin(), buffer.end(), v);
        pos = 0;
    }
    double push (double v, int delay) noexcept
    {
        buffer[pos] = v;
        const size_t n = buffer.size();
        const size_t d = static_cast<size_t> (std::clamp (delay, 0, int (n) - 1));
        const double out = buffer[(pos + n - d) % n];
        pos = pos + 1 == n ? 0 : pos + 1;
        return out;
    }

private:
    std::vector<double> buffer { 0.0 };
    size_t pos = 0;
};

// Extremum over the last `length` pushed values (sliding window), O(1) amortised with a monotonic deque on
// fixed storage. Less = true: minimum, false: maximum.
template <bool Min>
class SlidingExtremum
{
public:
    void prepare (int windowLength)
    {
        length = std::max (1, windowLength);
        capacity = static_cast<size_t> (length) + 1;
        values.assign (capacity, 0.0);
        stamps.assign (capacity, 0);
    }
    // The window starts filled with `initial`.
    void reset (double initial) noexcept
    {
        values[0] = initial;
        stamps[0] = 0;
        head = 0;
        tail = 1;
        count = 1;
        time = 1;
    }
    double push (double v) noexcept
    {
        const long long now = time++;
        while (count > 0)
        {
            const size_t back = (tail + capacity - 1) % capacity;
            if (Min ? (values[back] < v) : (values[back] > v))
                break;
            tail = back;
            --count;
        }
        values[tail] = v;
        stamps[tail] = now;
        tail = (tail + 1) % capacity;
        ++count;
        while (stamps[head] <= now - length)
        {
            head = (head + 1) % capacity;
            --count;
        }
        return values[head];
    }

private:
    int length = 1;
    size_t capacity = 2, head = 0, tail = 0, count = 0;
    long long time = 0;
    std::vector<double> values { 0.0, 0.0 };
    std::vector<long long> stamps { 0, 0 };
};
using SlidingMax = SlidingExtremum<false>;
using SlidingMin = SlidingExtremum<true>;

// Causal moving average (boxcar). The running sum is recomputed once per buffer cycle so rounding cannot
// accumulate; a window of identical values returns exactly that value (keeps the neutral path bit-exact).
class Boxcar
{
public:
    void prepare (int len)
    {
        length = std::max (1, len);
        buffer.assign (static_cast<size_t> (length), 1.0);
        reset (1.0);
    }
    void reset (double v) noexcept
    {
        std::fill (buffer.begin(), buffer.end(), v);
        pos = 0;
        sum = v * length;
        last = v;
        run = length;
    }
    double push (double v) noexcept
    {
        if (v == last)
            run = run < length ? run + 1 : length;
        else
        {
            last = v;
            run = 1;
        }
        sum += v - buffer[pos];
        buffer[pos] = v;
        if (++pos == static_cast<size_t> (length))
        {
            pos = 0;
            double s = 0.0;
            for (double x : buffer)
                s += x;
            sum = s;
        }
        return run >= length ? v : sum / length;
    }

private:
    int length = 1, run = 1;
    size_t pos = 0;
    double sum = 0.0, last = 1.0;
    std::vector<double> buffer { 1.0 };
};

// One-pole parameter smoother that snaps to the exact target once within `eps` (absolute), so settled values
// are bit-identical to their targets (spec D5: changes settle within 20 ms).
class Smoother
{
public:
    void setTimeConstant (double seconds, double sampleRate) noexcept { coeff = std::exp (-1.0 / (seconds * sampleRate)); }
    void setTarget (double t) noexcept { target = t; }
    void snap() noexcept { current = target; }
    double getCurrent() const noexcept { return current; }
    double getTarget() const noexcept { return target; }
    bool isSettled() const noexcept { return current == target; }
    double next (double eps = 1.0e-9) noexcept
    {
        if (current != target)
        {
            current = target + coeff * (current - target);
            if (std::abs (current - target) <= eps)
                current = target;
        }
        return current;
    }

private:
    double coeff = 0.0, current = 0.0, target = 0.0;
};

// Biquad, transposed direct form II.
struct BiquadCoeffs
{
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;

    static BiquadCoeffs normalised (double b0, double b1, double b2, double a0, double a1, double a2) noexcept
    {
        return { b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0 };
    }
    // RBJ "Audio EQ Cookbook" designs (bilinear transform, pre-warped at f).
    static BiquadCoeffs peaking (double f, double fs, double q, double db) noexcept
    {
        const double A = std::pow (10.0, db / 40.0), w = 2.0 * pi * f / fs, c = std::cos (w), al = std::sin (w) / (2.0 * q);
        return normalised (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
    }
    static BiquadCoeffs lowShelf (double f, double fs, double q, double db) noexcept
    {
        const double A = std::pow (10.0, db / 40.0), w = 2.0 * pi * f / fs, c = std::cos (w), al = std::sin (w) / (2.0 * q);
        const double s = 2.0 * std::sqrt (A) * al;
        return normalised (A * ((A + 1) - (A - 1) * c + s), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - s),
                           (A + 1) + (A - 1) * c + s, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - s);
    }
    static BiquadCoeffs highShelf (double f, double fs, double q, double db) noexcept
    {
        const double A = std::pow (10.0, db / 40.0), w = 2.0 * pi * f / fs, c = std::cos (w), al = std::sin (w) / (2.0 * q);
        const double s = 2.0 * std::sqrt (A) * al;
        return normalised (A * ((A + 1) + (A - 1) * c + s), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - s),
                           (A + 1) - (A - 1) * c + s, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - s);
    }
    static BiquadCoeffs bandPass (double f, double fs, double q) noexcept // 0 dB peak
    {
        const double w = 2.0 * pi * f / fs, c = std::cos (w), al = std::sin (w) / (2.0 * q);
        return normalised (al, 0.0, -al, 1 + al, -2 * c, 1 - al);
    }
    static BiquadCoeffs highPass1 (double f, double fs) noexcept // first order (b2 = a2 = 0)
    {
        const double k = std::tan (pi * f / fs);
        return { 1.0 / (1.0 + k), -1.0 / (1.0 + k), 0.0, (k - 1.0) / (k + 1.0), 0.0 };
    }
};

class Biquad
{
public:
    void setCoeffs (const BiquadCoeffs& c) noexcept { k = c; }
    void reset() noexcept { s1 = s2 = 0.0; }
    double process (double x) noexcept
    {
        const double y = k.b0 * x + s1;
        s1 = k.b1 * x - k.a1 * y + s2;
        s2 = k.b2 * x - k.a2 * y;
        return y;
    }

private:
    BiquadCoeffs k;
    double s1 = 0.0, s2 = 0.0;
};

// xorshift32 white noise, uniform in [-1, 1).
class WhiteNoise
{
public:
    void seed (std::uint32_t s) noexcept { state = s != 0 ? s : 0x9e3779b9u; }
    double next() noexcept
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return double (state) / 2147483648.0 - 1.0;
    }

private:
    std::uint32_t state = 0x9e3779b9u;
};

} // namespace craft::punch
