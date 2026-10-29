// Craft Audio doubler - one voice: delay line, splicing pitch shifter, feedback loop, LFO
// (spec r1 sections 5, 6, 7 and 9).
//
// Pitch shifter: a time-domain splicing shifter in the WSOLA family (public: Verhelst & Roelands,
// ICASSP 1993; Zoelzer, DAFX, delay-line pitch shifting). A read tap moves through the voice delay
// line at the pitch ratio rho (its delay changes by 1 - rho per sample). When the tap reaches the edge
// of the window [Dn - Wh, Dn + Wh] a new tap is started J samples back towards the window, J in
// [Jmin, Jmax] chosen to maximise the normalised cross-correlation of the signal at the old and new read
// positions, and the two taps are cross-faded with complementary linear gains.
#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

#include "Lfo.h"
#include "Parameters.h"
#include "Shelf.h"
#include "Smoothing.h"

namespace craft::doubler
{

// Design-rate constants of spec sections 5 and 6, scaled to the actual rate (review decision D2):
// every time is converted to round(time * fs) samples.
struct RangeConfig
{
    int latency = 0;         // inherent latency Lr
    double halfWindow = 0.0; // Wh
    double jumpMin = 0.0;    // Jmin
    double jumpMax = 0.0;    // Jmax
    double fadeTravel = 0.0; // delay travel of the old tap during a splice cross-fade (implementation choice)
    int corrWindow = 0;      // length of the similarity window of the splice search (implementation choice)

    static int samples (double seconds, double fs) noexcept { return int (std::lround (seconds * fs)); }

    static RangeConfig make (bool range20, double fs) noexcept
    {
        RangeConfig c;
        if (! range20)
        {
            c.latency = samples (320.0 / 44100.0, fs); // 7.256 ms
            c.halfWindow = samples (6.37e-3, fs);
            c.jumpMin = samples (5.6e-3, fs);
            c.jumpMax = samples (11.1e-3, fs);
        }
        else
        {
            c.latency = samples (1088.0 / 44100.0, fs); // 24.671 ms
            c.halfWindow = samples (23.6e-3, fs);
            c.jumpMin = samples (23.9e-3, fs);
            c.jumpMax = samples (35.0e-3, fs);
        }
        // splice cross-fade: the old tap travels 1.7 ms (design rate) while it fades out, centred on the
        // window edge, so it overshoots the edge by at most 0.85 ms - inside the Lr - Wh margin (0.89 ms
        // at Range 80, 1.07 ms at Range 20) even at the smallest nominal delay
        c.fadeTravel = samples (1.7e-3, fs);
        c.corrWindow = int (c.jumpMin);
        return c;
    }
};

class Voice
{
public:
    static constexpr double minDelay = 2.0; // samples; keeps every read strictly in the past

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        // longest delay: 100 ms + Lr(20 Hz) + Wh + fade overshoot, plus the search history
        const int needed = int (std::ceil (0.2 * fs)) + 8192;
        int size = 1;
        while (size < needed)
            size <<= 1;
        buffer.assign (size_t (size), 0.0);
        mask = size - 1;
        maxDelay = double (size) - 4.0 * RangeConfig::samples (35e-3, fs) - 64.0;
        scratch.assign (size_t (size), 0.0);

        centsSm.setTimeConstant (1.0e-3, fs);
        depthSm.setTimeConstant (1.0e-3, fs);
        fbGainSm.setTimeConstant (1.0e-3, fs);
        staticMix.setRampTime (5.0e-3, fs);
        xfStep = 1.0 / std::max (1.0, 5.0e-3 * fs);
        coarseStep = std::max (1, int (std::lround (fs / 24000.0)));
    }

    // Per-block targets. dnSamples = nominal delay Dn (spec section 5) in samples.
    void setTargets (const RangeConfig& range, double dnSamples, double centsTarget, double depthTarget, double rateHz,
                     double feedback, double fbHighPassHz, bool staticPath)
    {
        cfg = range;
        dnTarget = std::clamp (dnSamples, minDelay, maxDelay - 4.0 * cfg.halfWindow);
        centsSm.setTarget (centsTarget);
        depthSm.setTarget (depthTarget);
        lfo.setFrequency (rateHz, fs);
        fbGainSm.setTarget (feedback);
        hp.setFrequency (fbHighPassHz, fs);
        staticTarget = staticPath;
        staticMix.setTarget (staticPath ? 1.0 : 0.0);
        // leaving the exact-delay path: the reset shifter (one tap at Dn) produces the same output, so
        // the switch is seamless and needs no ramp
        if (! staticPath && staticMix.getCurrent() >= 1.0)
            staticMix.snap();
    }

    void reset()
    {
        std::fill (buffer.begin(), buffer.end(), 0.0);
        writePos = 0;
        dnCur = dnNext = dnTarget;
        xfActive = false;
        xf = 0.0;
        centsSm.snap();
        depthSm.snap();
        fbGainSm.snap();
        staticMix.snap();
        hp.reset();
        yPrev = 0.0;
        lfo.reset();
        resetShifter();
    }

    void resetLfo() noexcept { lfo.reset(); }

    double process (double in) noexcept
    {
        // feedback: previous output through the high-pass, loop period Dn + 1 sample (spec section 9)
        const double fb = fbGainSm.next() * hp.process (yPrev);
        writePos = (writePos + 1) & mask;
        buffer[size_t (writePos)] = in + fb;

        // pitch ratio (spec sections 6 and 7)
        const double dep = depthSm.next();
        cents = centsSm.next() + dep * lfo.next();

        const double sm = staticMix.next();
        if (sm >= 1.0 && staticTarget)
            resetShifter();
        else
            advanceShifter (std::exp2 (cents / 1200.0));

        // nominal delay changes: cross-fade between the old and the new delay (spec section 11.3)
        double y;
        if (xfActive)
        {
            xf += xfStep;
            if (xf >= 1.0)
            {
                xfActive = false;
                dnCur = dnNext;
                y = core (dnCur, sm);
            }
            else
                y = (1.0 - xf) * core (dnCur, sm) + xf * core (dnNext, sm);
        }
        else
        {
            if (dnTarget != dnCur)
            {
                dnNext = dnTarget;
                xfActive = true;
                xf = 0.0;
            }
            y = core (dnCur, sm);
        }

        if (! std::isfinite (y)) // safety: never let a non-finite value circulate in the loop
        {
            reset();
            y = 0.0;
        }
        yPrev = y;
        return y;
    }

    // --- introspection (tests and GUI) ---
    double getCents() const noexcept { return cents; }
    double getNominalDelay() const noexcept { return dnCur; }
    bool isStatic() const noexcept { return staticMix.getCurrent() >= 1.0; }
    // delay of the louder tap, relative to the undelayed input, in samples
    double getDominantDelay() const noexcept { return dnCur + ((fading && w < 0.5) ? offOld : offCur); }
    double getLastJump() const noexcept { return lastJump; }
    int getSpliceCount() const noexcept { return spliceCount; }

private:
    double readAt (double d) const noexcept
    {
        d = std::clamp (d, minDelay, maxDelay);
        const double pos = double (writePos) - d;
        const double fl = std::floor (pos);
        const int i = int (fl);
        const double mu = pos - fl;
        const double a = buffer[size_t (i & mask)];
        if (mu == 0.0)
            return a;
        const double b = buffer[size_t ((i + 1) & mask)];
        return a + mu * (b - a);
    }

    double core (double dn, double sm) const noexcept
    {
        const double ys = sm > 0.0 ? readAt (dn) : 0.0;
        if (sm >= 1.0)
            return ys;
        double yShift = readAt (dn + offCur);
        if (fading)
            yShift = w * yShift + (1.0 - w) * readAt (dn + offOld);
        return sm * ys + (1.0 - sm) * yShift;
    }

    void resetShifter() noexcept
    {
        offCur = offOld = 0.0;
        w = 1.0;
        fading = false;
    }

    void advanceShifter (double rho) noexcept
    {
        const double v = 1.0 - rho; // change of the tap delay per sample
        offCur += v;
        if (fading)
        {
            offOld += v;
            w += std::abs (v) / fadeLen;
            if (w >= 1.0)
            {
                w = 1.0;
                fading = false;
            }
        }
        if (! fading && v != 0.0)
            checkSplice (v);
    }

    void checkSplice (double v) noexcept
    {
        const double dn = dnCur, d = dn + offCur;
        if (v < 0.0) // rho > 1: the delay shrinks towards Dn - Wh
        {
            const double lo = dn - cfg.halfWindow;
            const double L = std::max (8.0, std::min (cfg.fadeTravel, 2.0 * (lo - minDelay)));
            // start the fade so the louder tap changes exactly at the window edge
            if (d <= lo + 0.5 * L)
                splice (true, L);
        }
        else // rho < 1: the delay grows towards Dn + Wh
        {
            const double hi = dn + cfg.halfWindow;
            const double L = std::max (8.0, std::min (cfg.fadeTravel, 2.0 * (maxDelay - hi)));
            if (d >= hi - 0.5 * L)
                splice (false, L);
        }
    }

    void splice (bool down, double L) noexcept
    {
        const double q = dnCur + offCur;
        double jLo = cfg.jumpMin, jHi = cfg.jumpMax;
        if (down)
            jHi = std::min (jHi, maxDelay - q - 1.0);
        else
            jHi = std::min (jHi, q - minDelay - 1.0);
        jLo = std::min (jLo, jHi);
        if (jHi < 1.0)
            return; // no room (only at absurd settings); keep the tap and try again next sample

        const double J = search (q, down, jLo, jHi);
        offOld = offCur;
        offCur += down ? J : -J;
        w = 0.0;
        fading = true;
        fadeLen = L;
        lastJump = J;
        ++spliceCount;
    }

    // Jump J in [jLo, jHi] that maximises the normalised cross-correlation between the signal around the
    // old read position (delay q) and around the candidate (delay q + J if down, q - J otherwise).
    double search (double q, bool down, double jLo, double jHi) noexcept
    {
        const int jMinI = int (std::ceil (jLo)), jMaxI = int (std::floor (jHi));
        if (jMaxI <= jMinI)
            return jHi;
        const int len = cfg.corrWindow;
        // reference segment [s, e] (absolute indices, may be negative before masking)
        const int centre = writePos - int (std::lround (q));
        int e = centre + len / 2;
        if (down)
            e = std::min (e, writePos - 1);
        else
            e = std::min (e, writePos - 1 - jMaxI);
        const int s = e - len + 1;

        // copy the region the search touches into a linear scratch buffer
        const int r0 = down ? s - jMaxI : s;
        const int rLen = len + jMaxI;
        for (int i = 0; i < rLen; ++i)
            scratch[size_t (i)] = buffer[size_t ((r0 + i) & mask)];
        const double* ref = scratch.data() + (s - r0);
        auto candidate = [&] (int j) { return ref + (down ? -j : j); };

        double refEnergy = 0.0;
        for (int i = 0; i < len; ++i)
            refEnergy += ref[i] * ref[i];
        if (refEnergy < 1.0e-12 * len)
            return std::round (0.5 * (jLo + jHi)); // silence: any jump is as good as another

        auto ncc = [&] (int j, int stride) {
            const double* c = candidate (j);
            double ab = 0.0, aa = 0.0, bb = 0.0;
            for (int i = 0; i < len; i += stride)
            {
                ab += ref[i] * c[i];
                aa += ref[i] * ref[i];
                bb += c[i] * c[i];
            }
            return ab / std::sqrt (aa * bb + 1.0e-30);
        };

        // coarse search on a decimated grid, then full resolution around the best lag
        int best = jMinI;
        double bestC = -2.0;
        for (int j = jMinI; j <= jMaxI; j += coarseStep)
        {
            const double c = ncc (j, coarseStep);
            if (c > bestC)
            {
                bestC = c;
                best = j;
            }
        }
        const int f0 = std::max (jMinI, best - coarseStep + 1), f1 = std::min (jMaxI, best + coarseStep - 1);
        bestC = -2.0;
        for (int j = f0; j <= f1; ++j)
        {
            const double c = ncc (j, 1);
            if (c > bestC)
            {
                bestC = c;
                best = j;
            }
        }
        // parabolic refinement to a fractional jump
        double J = best;
        if (best > jMinI && best < jMaxI)
        {
            const double cm = ncc (best - 1, 1), cp = ncc (best + 1, 1);
            const double den = cm - 2.0 * bestC + cp;
            if (den < 0.0)
                J += std::clamp (0.5 * (cm - cp) / den, -0.5, 0.5);
        }
        return std::clamp (J, jLo, jHi);
    }

    double fs = 48000.0;
    RangeConfig cfg;
    std::vector<double> buffer, scratch;
    int mask = 0, writePos = 0;
    double maxDelay = 1000.0;

    // nominal delay and its cross-fade
    double dnTarget = 0.0, dnCur = 0.0, dnNext = 0.0, xf = 0.0, xfStep = 0.01;
    bool xfActive = false;

    // shifter state: tap offsets relative to Dn (samples), weight of the current tap
    double offCur = 0.0, offOld = 0.0, w = 1.0, fadeLen = 1.0, lastJump = 0.0;
    bool fading = false;
    int spliceCount = 0, coarseStep = 2;
    Ramp staticMix; // 1 = exact delay path (no shifter)
    bool staticTarget = true;

    // pitch / modulation
    OnePole centsSm, depthSm;
    TriangleLfo lfo;
    double cents = 0.0;

    // feedback
    OnePole fbGainSm;
    FeedbackHighPass hp;
    double yPrev = 0.0;
};

} // namespace craft::doubler
