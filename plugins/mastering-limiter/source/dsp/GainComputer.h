// Craft Mastering Limiter - look-ahead gain computer (spec r1 sections 3, 4 and 5). Host independent.
//
// Per input sample t the computer receives the linked sample peak p[t] and returns the gain g[n] of the
// aligned output sample n = t - L together with its required gain r[n]:
//   r[t]  = min(1, T / p[t])                                  (step 2)
//   h[n]  = min(r[n+1] .. r[n+L]) = min over the last L r's   (step 3, sliding minimum, current excluded)
//   q[n]  = release follower of h                             (step 4 fixed / section 5 adaptive)
//   s[n]  = MA_B(MA_A(q))[n]                                  (step 5, causal boxcars, A + B - 1 = L)
//   g[n]  = min(s[n], r[n])                                   (step 6, ceiling enforcement)
#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

namespace craft::mastering_limiter::dsp
{

struct LatencyConfig
{
    int latency;   // L
    int boxcarA;   // first boxcar length
    int boxcarB;   // second boxcar length
};

// Spec section 3 / 4 step 5. Non-standard rates use the nearest family (boundaries at the geometric middle).
inline LatencyConfig latencyConfigFor (double sampleRate) noexcept
{
    if (sampleRate < 66000.0)
        return { 64, 38, 27 };
    if (sampleRate < 132000.0)
        return { 128, 75, 54 };
    return { 256, 150, 107 };
}

// Causal moving average of fixed length. Exactly 1.0 whenever the whole window is 1.0, so a limiter that
// never limits is bit-transparent (spec 4: Threshold = Ceiling = 0 without overs is bit-exact).
class Boxcar
{
public:
    void prepare (int length)
    {
        len = std::max (1, length);
        buf.assign (size_t (len), 1.0);
        reset();
    }

    void reset()
    {
        std::fill (buf.begin(), buf.end(), 1.0);
        pos = 0;
        sum = double (len);
        notOne = 0;
    }

    double process (double x) noexcept
    {
        const double old = buf[size_t (pos)];
        buf[size_t (pos)] = x;
        if (++pos == len)
            pos = 0;
        notOne += (x != 1.0 ? 1 : 0) - (old != 1.0 ? 1 : 0);
        if (notOne == 0)
        {
            sum = double (len); // drop accumulated rounding error
            return 1.0;
        }
        sum += x - old;
        return sum / double (len);
    }

private:
    std::vector<double> buf;
    int len = 1, pos = 0, notOne = 0;
    double sum = 1.0;
};

// Sliding minimum over the last `window` values (monotonic deque in a ring buffer).
class SlidingMin
{
public:
    void prepare (int window)
    {
        win = std::max (1, window);
        cap = win + 1;
        idx.assign (size_t (cap), 0);
        val.assign (size_t (cap), 1.0);
        reset();
    }

    void reset()
    {
        head = tail = 0;
        count = 0;
        t = 0;
    }

    // push the newest value and return the minimum of the last `window` values (missing values count as 1)
    double push (double x) noexcept
    {
        // drop from the back everything >= x
        while (count > 0)
        {
            const int last = (tail + cap - 1) % cap;
            if (val[size_t (last)] < x)
                break;
            tail = last;
            --count;
        }
        idx[size_t (tail)] = t;
        val[size_t (tail)] = x;
        tail = (tail + 1) % cap;
        ++count;
        // expire from the front
        while (idx[size_t (head)] <= t - win)
        {
            head = (head + 1) % cap;
            --count;
        }
        ++t;
        const double m = val[size_t (head)];
        return t < win ? std::min (m, 1.0) : m;
    }

private:
    std::vector<long long> idx;
    std::vector<double> val;
    int win = 1, cap = 2, head = 0, tail = 0, count = 0;
    long long t = 0;
};

// Release follower coefficient: the linear distance to the target shrinks x10 per `releaseSeconds`.
inline double releaseCoefficient (double releaseSeconds, double sampleRate) noexcept
{
    return std::exp (-std::log (10.0) / (releaseSeconds * sampleRate));
}

class GainComputer
{
public:
    // Adaptive Release realisation (spec 5, recommended form): fast and slow followers, q = min(fast, slow).
    static constexpr double adaptiveFastReleaseMs = 0.65;
    static constexpr double adaptiveSlowAttackTauMs = 99.0;
    static constexpr double adaptiveSlowReleaseMs = 500.0;

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        cfg = latencyConfigFor (sampleRate);
        hold.prepare (cfg.latency);
        maA.prepare (cfg.boxcarA);
        maB.prepare (cfg.boxcarB);
        rDelay.assign (size_t (cfg.latency), 1.0);
        cFast = releaseCoefficient (adaptiveFastReleaseMs * 1.0e-3, fs);
        aSlow = std::exp (-1.0 / (adaptiveSlowAttackTauMs * 1.0e-3 * fs));
        cSlow = releaseCoefficient (adaptiveSlowReleaseMs * 1.0e-3, fs);
        setRelease (releaseMs, adaptive);
        reset();
    }

    void reset()
    {
        hold.reset();
        maA.reset();
        maB.reset();
        std::fill (rDelay.begin(), rDelay.end(), 1.0);
        rPos = 0;
        q = fast = slow = 1.0;
    }

    void setRelease (double ms, bool adaptiveOn) noexcept
    {
        releaseMs = ms;
        adaptive = adaptiveOn;
        if (fs > 0.0)
            cFixed = releaseCoefficient (std::max (ms, 1.0e-3) * 1.0e-3, fs);
    }

    int getLatency() const noexcept { return cfg.latency; }
    LatencyConfig getConfig() const noexcept { return cfg; }

    struct Out
    {
        double gain;     // g[n]
        double required; // r[n]
    };

    // `peak` = linked sample peak of input sample t, `threshold` = linear T used for this sample.
    Out process (double peak, double threshold) noexcept
    {
        const double r = peak > threshold ? threshold / peak : 1.0;  // step 2
        const double rn = rDelay[size_t (rPos)];                      // r[n], n = t - L
        rDelay[size_t (rPos)] = r;
        if (++rPos == cfg.latency)
            rPos = 0;

        const double h = hold.push (r); // min(r[n+1] .. r[n+L])

        if (! adaptive)
        {
            q = h < q ? h : h - (h - q) * cFixed; // step 4
        }
        else
        {
            fast = h < fast ? h : h - (h - fast) * cFast;
            slow = h - (h - slow) * (h < slow ? aSlow : cSlow);
            q = std::min (fast, slow);
        }

        const double s = maB.process (maA.process (q)); // step 5
        return { std::min (s, rn), rn };                 // step 6
    }

private:
    double fs = 0.0;
    LatencyConfig cfg { 64, 38, 27 };
    SlidingMin hold;
    Boxcar maA, maB;
    std::vector<double> rDelay;
    int rPos = 0;

    double releaseMs = 1.0;
    bool adaptive = true;
    double cFixed = 0.0, cFast = 0.0, aSlow = 0.0, cSlow = 0.0;
    double q = 1.0, fast = 1.0, slow = 1.0;
};

} // namespace craft::mastering_limiter::dsp
