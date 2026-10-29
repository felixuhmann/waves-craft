// Craft Peak Limiter - small DSP building blocks (host-independent, no JUCE).
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace craft::limiter
{

// Look-ahead and smoothing lengths per sample rate (spec r1 sections 3 and 4.5).
struct LookaheadConfig
{
    int latency = 64; // L
    int boxA = 38;    // first moving average
    int boxB = 27;    // second moving average (A + B - 1 = L)

    static LookaheadConfig forSampleRate (double fs) noexcept
    {
        if (fs <= 50000.0)
            return { 64, 38, 27 };
        if (fs <= 100000.0)
            return { 128, 75, 54 };
        return { 256, 150, 107 }; // 176.4 / 192 kHz: extrapolated by the spec (open question 2)
    }
};

// Minimum over the last `length` pushed values (sliding window), O(1) amortised: monotonic deque
// on fixed storage. The window starts filled with `initial`.
class SlidingMin
{
public:
    void prepare (int windowLength)
    {
        length = std::max (1, windowLength);
        capacity = static_cast<std::size_t> (length) + 1;
        values.assign (capacity, 0.0);
        stamps.assign (capacity, 0);
    }

    void reset (double initial) noexcept
    {
        // A single entry stamped 0 stands for a full window of `initial` values; real pushes start at time 1,
        // so it expires exactly when the window holds `length` real values.
        values[0] = initial;
        stamps[0] = 0;
        head = 0;
        tail = 1;
        count = 1;
        time = 1;
    }

    // Push the newest value and return the minimum of the last `length` values (including it).
    double push (double v) noexcept
    {
        const long long now = time++;
        // drop larger-or-equal values from the back
        while (count > 0)
        {
            const std::size_t back = (tail + capacity - 1) % capacity;
            if (values[back] < v)
                break;
            tail = back;
            --count;
        }
        values[tail] = v;
        stamps[tail] = now;
        tail = (tail + 1) % capacity;
        ++count;
        // drop expired values from the front: valid stamps are > now - length
        while (stamps[head] <= now - length)
        {
            head = (head + 1) % capacity;
            --count;
        }
        return values[head];
    }

private:
    int length = 1;
    std::size_t capacity = 2, head = 0, tail = 0, count = 0;
    long long time = 0;
    std::vector<double> values;
    std::vector<long long> stamps;
};

// Causal moving average of the last `length` values (boxcar). Running sum, recomputed exactly once per
// buffer cycle so rounding cannot accumulate; a window of identical values yields exactly that value.
class Boxcar
{
public:
    void prepare (int len)
    {
        length = std::max (1, len);
        buffer.assign (static_cast<std::size_t> (length), 1.0);
    }

    void reset (double initial) noexcept
    {
        std::fill (buffer.begin(), buffer.end(), initial);
        pos = 0;
        sum = initial * length;
    }

    double push (double v) noexcept
    {
        sum += v - buffer[pos];
        buffer[pos] = v;
        if (++pos == static_cast<std::size_t> (length))
        {
            pos = 0;
            double s = 0.0;
            for (double x : buffer)
                s += x;
            sum = s;
        }
        return sum / length;
    }

private:
    int length = 1;
    std::size_t pos = 0;
    double sum = 0.0;
    std::vector<double> buffer;
};

// Fixed-length delay line (delay >= 1).
template <typename T>
class DelayLine
{
public:
    void prepare (int delaySamples) { buffer.assign (static_cast<std::size_t> (std::max (1, delaySamples)), T {}); }
    void reset (T initial) noexcept
    {
        std::fill (buffer.begin(), buffer.end(), initial);
        pos = 0;
    }
    // Write the newest value, return the value written `delay` pushes ago.
    T push (T v) noexcept
    {
        const T out = buffer[pos];
        buffer[pos] = v;
        if (++pos == buffer.size())
            pos = 0;
        return out;
    }

private:
    std::vector<T> buffer;
    std::size_t pos = 0;
};

// One-pole smoother that snaps to the exact target once within a relative epsilon, so settled values are
// bit-identical to their targets.
class Smoother
{
public:
    void setTimeConstant (double seconds, double sampleRate) noexcept { coeff = std::exp (-1.0 / (seconds * sampleRate)); }
    void setTarget (double t) noexcept { target = t; }
    void snap() noexcept { current = target; }
    double getCurrent() const noexcept { return current; }
    double next() noexcept
    {
        if (current != target)
        {
            current = target + coeff * (current - target);
            if (std::abs (current - target) <= 1.0e-10 * std::abs (target))
                current = target;
        }
        return current;
    }

private:
    double coeff = 0.0, current = 1.0, target = 1.0;
};

} // namespace craft::limiter
