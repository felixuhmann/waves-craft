// Craft Band Limiter - peak-limiting mixer and complete engine (spec r1 sections 2, 5-8, 10). Host independent.
//
//   input -> band split -> band gains -> mixer (one gain per band, targets from the sum) -> sum
//         -> wideband look-ahead safety stage -> make-up -> requantiser
//
// All limiting happens in the "threshold domain" (limit theta = 10^(T/20)); make-up (C - T) follows, so the ceiling
// holds whenever the limiter output stays within theta. The constants of the distribution and release laws are fits
// to the normative tables (see IMPLEMENTATION_NOTES).
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>

#include "BandSplitters.h"
#include "Parameters.h"

namespace craft::band_limiter::dsp
{

// Maximum over the last `window` values, O(1) amortised, no allocation after prepare().
class SlidingMax
{
public:
    void prepare (int capacity)
    {
        int size = 16;
        while (size < capacity + 2)
            size *= 2;
        ring.resize (size_t (size));
        mask = size - 1;
        reset();
    }

    void reset() noexcept { head = tail = 0; index = 0; }

    inline double push (double v, int window) noexcept
    {
        while (tail != head && ring[size_t ((tail - 1) & mask)].value <= v)
            --tail;
        ring[size_t (tail & mask)] = { index, v };
        ++tail;
        const std::int64_t oldest = index - window + 1;
        while (ring[size_t (head & mask)].index < oldest)
            ++head;
        ++index;
        return ring[size_t (head & mask)].value;
    }

private:
    struct Entry
    {
        std::int64_t index;
        double value;
    };
    std::vector<Entry> ring;
    std::int64_t head = 0, tail = 0, index = 0;
    std::int64_t mask = 0;
};

// Box average of the last N gains; returns exactly 1 once the whole window is 1 (neutral stays bit exact).
class BoxSmoother
{
public:
    void prepare (int length)
    {
        buffer.assign (size_t (length), 1.0);
        reset();
    }

    void reset() noexcept
    {
        std::fill (buffer.begin(), buffer.end(), 1.0);
        sum = double (buffer.size());
        pos = 0;
        nonUnity = 0;
    }

    inline double process (double g) noexcept
    {
        const int n = int (buffer.size());
        sum += g - buffer[size_t (pos)];
        buffer[size_t (pos)] = g;
        if (++pos == n)
        {
            pos = 0;
            sum = 0.0; // remove rounding drift
            for (double v : buffer)
                sum += v;
        }
        nonUnity = g < 1.0 ? n : std::max (0, nonUnity - 1);
        return nonUnity == 0 ? 1.0 : sum / double (n);
    }

private:
    std::vector<double> buffer;
    double sum = 0.0;
    int pos = 0, nonUnity = 0;
};

// Per-band release law (spec 7). Gain recovers along a straight line in linear gain with a rate set by the Release
// value, after a lag; Adaptive kinds split the depth into a slow part (charged by how long the over lasted) and a fast
// part that decays within a few ms. Manual has no adaptation and a small settling dip when an over has fully ended.
class BandRelease
{
public:
    // Law constants fitted to tables/release_steps.csv and tables/release_bursts.csv
    static constexpr double rampBaseMs = 123.2, rampScale = 2.852, rampPower = 0.8857;
    static constexpr double chargeMs = 37.0, fastDecayMs = 4.5, dipFraction = 0.26, dipRampMs = 3.0;

    struct Mode
    {
        std::array<double, numBands> rampFactor;
        double lagFraction;
    };

    static const Mode& modeFor (Behaviour b) noexcept
    {
        static constexpr Mode table[5] = {
            { { 0.814, 0.831, 0.838, 0.560, 0.647 }, 0.110 }, // Adaptive
            { { 0.684, 0.815, 0.836, 0.686, 0.750 }, 0.162 }, // Aggressive
            { { 1.542, 1.051, 0.953, 0.478, 0.566 }, 0.248 }, // Warm
            { { 1.567, 1.063, 0.961, 0.481, 0.566 }, 0.241 }, // Scaled
            { { 1.000, 1.000, 1.000, 1.000, 1.000 }, 0.199 }, // Manual
        };
        return table[int (b)];
    }

    void prepare (double sampleRate) noexcept
    {
        fs = sampleRate;
        chargeCoef = 1.0 - std::exp (-1000.0 / (chargeMs * fs));
        fastCoef = std::exp (-1000.0 / (fastDecayMs * fs));
        dipRampSamples = dipRampMs * 0.001 * fs;
        reset();
    }

    void reset() noexcept
    {
        g = 1.0;
        charge = 0.0;
        t = 0.0;
        releasing = false;
    }

    void configure (int band, double releaseMs, Behaviour behaviour) noexcept
    {
        const auto& m = modeFor (behaviour);
        adaptive = behaviour != Behaviour::Manual;
        const double T = (rampBaseMs + rampScale * std::pow (std::max (releaseMs, 0.01), rampPower)) * m.rampFactor[size_t (band)];
        ratePerSample = 1000.0 / (T * fs);
        lagSamples = m.lagFraction * releaseMs * 0.001 * fs;
    }

    double getGain() const noexcept { return g; }

    inline double process (double target) noexcept
    {
        if (g >= 1.0 && target >= 1.0)
            return 1.0;

        if (adaptive && target < 1.0)
            charge += (1.0 - charge) * chargeCoef;

        if (target <= g && ! (releasing && dipping))
        {
            startFrom (target);
            return g;
        }

        // Manual: the over has fully ended shortly after the detector started to let go, so the settling dip applies.
        if (! releasing || (! adaptive && ! dipping && target >= 1.0 && t < dipRampSamples * 8.0))
            beginRelease (target);

        t += 1.0;
        double curve = slowStart + std::max (0.0, t - lag) * ratePerSample;
        if (fastFactor < 1.0)
        {
            curve *= std::pow (fastFactor, fastPart);
            fastPart *= fastCoef;
        }
        if (dipping && t < dipRampSamples)
            curve = start + (curve - start) * t / dipRampSamples;

        if (curve > target)
        {
            startFrom (target);
            return g;
        }
        g = curve;
        if (g >= 1.0 - 1.0e-9)
        {
            g = 1.0;
            charge = 0.0;
            releasing = false;
        }
        return g;
    }

private:
    inline void startFrom (double target) noexcept
    {
        g = target;
        releasing = false;
        dipping = false;
    }

    inline void beginRelease (double target) noexcept
    {
        releasing = true;
        t = 0.0;
        start = g;
        lag = lagSamples;
        dipping = false;
        fastPart = 1.0;
        fastFactor = 1.0;
        if (adaptive)
        {
            // gain = slow * fast; depth in dB splits by the charge
            slowStart = std::pow (g, charge);
            fastFactor = slowStart > 0.0 ? g / slowStart : 1.0;
        }
        else if (target >= 1.0)
        {
            // Settling dip after a finished over; the lag shortens so the later ramp keeps its position.
            slowStart = std::pow (g, 1.0 + dipFraction);
            lag = std::max (0.0, lagSamples - (g - slowStart) / ratePerSample);
            dipping = true;
        }
        else
            slowStart = g;
    }

    double fs = 48000.0, chargeCoef = 0.0, fastCoef = 0.0, dipRampSamples = 144.0;
    double ratePerSample = 0.001, lagSamples = 0.0;
    double g = 1.0, charge = 0.0, t = 0.0, start = 1.0, lag = 0.0, slowStart = 1.0, fastFactor = 1.0, fastPart = 1.0;
    bool adaptive = true, releasing = false, dipping = false;
};

class BandLimiterEngine
{
public:
    static constexpr int maxChannels = 2;
    static constexpr int lpLatency = 3528;
    static constexpr int llLatency = 64;

    // Mixer law constants (fitted to tables/distribution_*.csv).
    static constexpr double priorityExponent = 0.82; // kappa of Appendix A (the clamp at unity gain saturates it)
    static constexpr double shareExponent = 0.082;   // louder band takes a slightly larger share of the budget
    static constexpr double releaseTilt = 0.14;      // over-dependent shift towards bands with faster release
    static constexpr double tiltCentreDb = 10.0;
    static constexpr double topBandBiasDb = 1.0;     // band 5 keeps slightly more level (five-tone rows)

    explicit BandLimiterEngine (Variant v = Variant::LinearMulti) : variant (v)
    {
        linearPhase = isLinearPhase (v);
        lookAhead = linearPhase ? 64 : 32;
        if (linearPhase)
            lpSplitter = std::make_unique<LinearPhaseSplitter>();
    }

    Variant getVariant() const noexcept { return variant; }
    int getLatencySamples() const noexcept { return linearPhase ? lpLatency : llLatency; }
    double getSampleRate() const noexcept { return fs; }

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        const int maxHold = int (std::ceil (0.021 * fs)) + lookAhead + 4;
        for (auto& p : bandPeak)
            p.prepare (maxHold);
        sumPeak.prepare (maxHold);
        widePeak.prepare (lookAhead + 2);
        for (auto& s : bandSmooth)
            s.prepare (lookAhead);
        wideSmooth.prepare (lookAhead);
        for (auto& ch : bandDelay)
            for (auto& d : ch)
                d.assign (size_t (lookAhead), 0.0);
        for (auto& d : wideDelay)
            d.assign (size_t (lookAhead), 0.0);
        makeupDelay.assign (size_t (2 * lookAhead), 1.0);
        dryDelay[0].assign (size_t (getLatencySamples()), 0.0);
        dryDelay[1].assign (size_t (getLatencySamples()), 0.0);
        for (auto& r : release)
            r.prepare (fs);
        for (auto& r : requant)
            r.prepare (fs, 0);
        requant[0].prepare (fs, 0x1234567ull);
        requant[1].prepare (fs, 0x89abcdefull);
        if (linearPhase)
            lpSplitter->prepare (fs);
        else
            llSplitter.prepare (fs);
        wideReleaseCoef = 1.0 - std::exp (-1000.0 / (8.0 * fs));
        envAttackCoef = 1.0 - std::exp (-1000.0 / (15.0 * fs));
        rampDbPerSample = 1000.0 / (20.0 * fs) * 6.0; // 6 dB per 20 ms
        gainStep = 1000.0 / (10.0 * fs);
        bypassStep = 1000.0 / (10.0 * fs);
        prepared = true;
        formatApplied = false; // the noise-shaping filters depend on the sample rate
        applySettings (settings);
        reset();
    }

    void reset()
    {
        if (linearPhase)
            lpSplitter->reset();
        else
            llSplitter.reset();
        for (auto& p : bandPeak)
            p.reset();
        sumPeak.reset();
        widePeak.reset();
        for (auto& s : bandSmooth)
            s.reset();
        wideSmooth.reset();
        for (auto& ch : bandDelay)
            for (auto& d : ch)
                std::fill (d.begin(), d.end(), 0.0);
        for (auto& d : wideDelay)
            std::fill (d.begin(), d.end(), 0.0);
        for (auto& d : dryDelay)
            std::fill (d.begin(), d.end(), 0.0);
        for (auto& r : release)
            r.reset();
        for (auto& r : requant)
            r.reset();
        delayPos = 0;
        dryPos = 0;
        makeupPos = 0;
        wideGain = 1.0;
        envelope.fill (0.0);
        counter = 0;
        for (int b = 0; b < numBands; ++b)
        {
            bandGain[size_t (b)] = bandGainTarget[size_t (b)];
            soloGain[size_t (b)] = soloTarget[size_t (b)];
        }
        thresholdDb = settings.thresholdDb;
        ceilingDb = settings.ceilingDb;
        updateStatic();
        std::fill (makeupDelay.begin(), makeupDelay.end(), makeup);
        bypassMix = bypassed ? 1.0 : 0.0;
        updateHolds (nullptr);
        resetMeters();
    }

    void setSettings (const Settings& s) noexcept { applySettings (s); }
    const Settings& getSettings() const noexcept { return settings; }
    void setBypassed (bool b) noexcept { bypassed = b; }

    template <typename Sample>
    void process (Sample* const* channels, int numChannels, int numSamples) noexcept
    {
        numChannels = std::clamp (numChannels, 1, maxChannels);
        const int total = 2 * lookAhead;
        for (int i = 0; i < numSamples; ++i)
        {
            if (thresholdDb != settings.thresholdDb || ceilingDb != settings.ceilingDb)
                rampLevels();

            const double xl = double (channels[0][i]);
            const double xr = numChannels > 1 ? double (channels[1][i]) : 0.0;
            inPeak[0] = std::max (inPeak[0], std::abs (xl));
            inPeak[1] = std::max (inPeak[1], std::abs (xr));

            double y[2];
            processSample (xl, xr, y);

            const double mk = makeupDelay[size_t (makeupPos)];
            makeupDelay[size_t (makeupPos)] = makeup;
            if (++makeupPos == total)
                makeupPos = 0;

            if (bypassMix != (bypassed ? 1.0 : 0.0))
                bypassMix = bypassed ? std::min (1.0, bypassMix + bypassStep) : std::max (0.0, bypassMix - bypassStep);

            for (int ch = 0; ch < numChannels; ++ch)
            {
                double v = y[ch] * mk;
                v = std::clamp (v, -ceilingLin, ceilingLin); // spec 5: exact sample-peak ceiling
                if (std::abs (v) < 1.0e-10)
                    v = 0.0; // numerical residue of the splitters is digital silence for the requantiser
                const double dry = dryDelay[size_t (ch)][size_t (dryPos)];
                dryDelay[size_t (ch)][size_t (dryPos)] = ch == 0 ? xl : xr;
                double out;
                if (bypassMix >= 1.0)
                    out = dry;
                else
                {
                    out = requant[size_t (ch)].process (v);
                    if (bypassMix > 0.0)
                        out += (dry - out) * bypassMix;
                }
                channels[ch][i] = Sample (out);
                outPeak[size_t (ch)] = std::max (outPeak[size_t (ch)], std::abs (out));
            }
            if (++dryPos == getLatencySamples())
                dryPos = 0;
        }
    }

    // Meter data since resetMeters() (audio thread).
    double getInputPeak (int ch) const noexcept { return inPeak[size_t (ch)]; }
    double getOutputPeak (int ch) const noexcept { return outPeak[size_t (ch)]; }
    double getBandMinGain (int b) const noexcept { return bandMinGain[size_t (b)]; }
    double getMinGain() const noexcept { return minGain; }
    double getBandPeak (int b) const noexcept { return bandMeter[size_t (b)]; }
    void resetMeters() noexcept
    {
        inPeak.fill (0.0);
        outPeak.fill (0.0);
        bandMinGain.fill (1.0);
        bandMeter.fill (0.0);
        minGain = 1.0;
    }

    LinearPhaseSplitter* getLinearSplitter() noexcept { return lpSplitter.get(); }

private:
    void applySettings (const Settings& s) noexcept
    {
        const bool formatChanged = s.wordLength != settings.wordLength || s.dither != settings.dither
                                   || s.shaping != settings.shaping || ! formatApplied;
        settings = s;
        std::array<double, numCrossovers> xo;
        for (int k = 0; k < numCrossovers; ++k)
            xo[size_t (k)] = param::clampCrossover (k, s.crossoverHz[size_t (k)]);
        if (linearPhase)
            lpSplitter->setCrossovers (xo);
        else
            llSplitter.setCrossovers (xo);

        bool anySolo = false;
        for (bool v : s.solo)
            anySolo = anySolo || v;
        for (int b = 0; b < numBands; ++b)
        {
            const auto ub = size_t (b);
            bandGainTarget[ub] = s.gainDb[ub] == 0.0 ? 1.0 : std::pow (10.0, s.gainDb[ub] / 20.0);
            soloTarget[ub] = (! anySolo || s.solo[ub]) ? 1.0 : 0.0;
            priorityWeight[ub] = std::pow (10.0, (priorityExponent * s.priority[ub] + (b == numBands - 1 ? topBandBiasDb : 0.0)) / 20.0);
            tiltSlope[ub] = -releaseTilt * std::log2 (std::max (s.releaseMs[ub], 0.01) / 400.0) * std::log (10.0) / 20.0;
            release[ub].configure (b, s.releaseMs[ub], s.behaviour);
        }
        separation = std::clamp (s.separation, 0.0, 1.0);
        if (formatChanged && prepared)
        {
            for (auto& r : requant)
                r.setFormat (s.wordLength, s.dither, s.shaping);
            formatApplied = true;
        }
        if (prepared)
            updateHoldLengths (xo);
    }

    void updateHoldLengths (std::array<double, numCrossovers> xo) noexcept
    {
        std::sort (xo.begin(), xo.end());
        // Detector hold: band 2 one period of its lower edge, upper bands half a period (1..20 ms); band 1 20 ms;
        // never less than look-ahead + 1.
        holdSamples[0] = int (0.020 * fs);
        for (int b = 1; b < numBands; ++b)
            holdSamples[size_t (b)] = int (std::clamp ((b == 1 ? 1.0 : 0.5) / xo[size_t (b - 1)], 0.001, 0.020) * fs) + 1;
        for (auto& h : holdSamples)
            h = std::max (h, lookAhead + 1);
    }

    void updateHolds (const double* env) noexcept
    {
        // The sum detector holds as long as the lowest band that carries a significant part of the signal.
        int hold = lookAhead + 1;
        if (env != nullptr)
        {
            double maxEnv = 0.0;
            for (int b = 0; b < numBands; ++b)
                maxEnv = std::max (maxEnv, env[b]);
            for (int b = 0; b < numBands; ++b)
                if (env[b] >= 0.1 * maxEnv)
                    hold = std::max (hold, holdSamples[size_t (b)]);
        }
        sumHold = hold;
    }

    void rampLevels() noexcept
    {
        auto step = [this] (double& v, double target) {
            v = v < target ? std::min (target, v + rampDbPerSample) : std::max (target, v - rampDbPerSample);
        };
        step (thresholdDb, settings.thresholdDb);
        step (ceilingDb, settings.ceilingDb);
        updateStatic();
    }

    void updateStatic() noexcept
    {
        theta = thresholdDb == 0.0 ? 1.0 : std::pow (10.0, thresholdDb / 20.0);
        ceilingLin = ceilingDb == 0.0 ? 1.0 : std::pow (10.0, ceilingDb / 20.0);
        const double makeupDb = ceilingDb - thresholdDb;
        makeup = makeupDb == 0.0 ? 1.0 : std::pow (10.0, makeupDb / 20.0);
    }

    // Measured over-attenuation at intermediate Separation (spec 6.3, D1): grows with how concentrated the signal is.
    static double separationExcessDb (double s, double concentration) noexcept
    {
        static constexpr double table[5] = { 0.0, 0.70, 1.04, 0.96, 0.0 };
        const double p = std::clamp (s, 0.0, 1.0) * 4.0;
        const int i = std::min (int (p), 3);
        const double base = table[i] + (p - double (i)) * (table[i + 1] - table[i]);
        if (base <= 0.0)
            return 0.0;
        return std::min (8.0, base * std::pow (concentration / 0.278, 2.33));
    }

    // Mixer: gains that bring the sum of the bands down to the budget (spec 6, Appendix A).
    inline void computeTargets (const double* env, double sumPeakValue, double* target) noexcept
    {
        if (sumPeakValue <= theta)
        {
            for (int b = 0; b < numBands; ++b)
                target[b] = 1.0;
            return;
        }

        double sumEnv = 0.0, sumSq = 0.0;
        for (int b = 0; b < numBands; ++b)
        {
            sumEnv += env[b];
            sumSq += env[b] * env[b];
        }
        const double over = 20.0 * std::log10 (sumPeakValue / theta);

        // Budget: threshold, scaled by the coherence of the bands (their peaks do not line up in the sum).
        // Never below the threshold: right after an over the band detectors may already have let go of it.
        double budget = theta * std::max (1.0, sumEnv / sumPeakValue);
        if (separation > 0.0 && separation < 1.0)
        {
            const double excess = separationExcessDb (separation, sumSq / (sumEnv * sumEnv)) * std::min (1.0, over / 3.0);
            budget *= std::pow (10.0, -excess / 20.0);
        }

        const double meanEnv = sumEnv / double (numBands);
        double shape[numBands];
        for (int b = 0; b < numBands; ++b)
        {
            const double detector = std::max (separation * env[b] + (1.0 - separation) * meanEnv, 1.0e-12);
            shape[b] = priorityWeight[size_t (b)] * std::exp (tiltSlope[size_t (b)] * (over - tiltCentreDb))
                       * std::pow (detector, shareExponent - 1.0);
            target[b] = 1.0;
        }

        // Water filling: bands that would rise above unity stay at unity, the others share what is left.
        bool active[numBands] = { true, true, true, true, true };
        for (int pass = 0; pass < numBands; ++pass)
        {
            double fixedPart = 0.0, weighted = 0.0;
            for (int b = 0; b < numBands; ++b)
            {
                if (active[b])
                    weighted += shape[b] * env[b];
                else
                    fixedPart += env[b];
            }
            if (weighted <= 0.0)
                break;
            const double lambda = std::max (budget - fixedPart, 0.0) / weighted;
            bool clamped = false;
            for (int b = 0; b < numBands; ++b)
                if (active[b] && lambda * shape[b] >= 1.0)
                {
                    active[b] = false;
                    target[b] = 1.0;
                    clamped = true;
                }
            if (! clamped)
            {
                for (int b = 0; b < numBands; ++b)
                    if (active[b])
                        target[b] = std::max (lambda * shape[b], 1.0e-4);
                break;
            }
        }
    }

    inline void processSample (double xl, double xr, double (&y)[2]) noexcept
    {
        double bands[numBands][2];
        if (linearPhase)
            lpSplitter->process (xl, xr, bands);
        else
            llSplitter.process (xl, xr, bands);

        double env[numBands];
        double sum0 = 0.0, sum1 = 0.0;
        for (int b = 0; b < numBands; ++b)
        {
            const auto ub = size_t (b);
            if (bandGain[ub] != bandGainTarget[ub])
                bandGain[ub] = bandGain[ub] < bandGainTarget[ub] ? std::min (bandGainTarget[ub], bandGain[ub] + gainStep)
                                                                  : std::max (bandGainTarget[ub], bandGain[ub] - gainStep);
            if (soloGain[ub] != soloTarget[ub])
                soloGain[ub] = soloGain[ub] < soloTarget[ub] ? std::min (soloTarget[ub], soloGain[ub] + gainStep)
                                                              : std::max (soloTarget[ub], soloGain[ub] - gainStep);
            bands[b][0] *= bandGain[ub];
            bands[b][1] *= bandGain[ub];
            sum0 += bands[b][0];
            sum1 += bands[b][1];
            // Rises with a short time constant: the broadband splash of a level step in the other bands must not
            // eat the mixer's budget; a real onset meanwhile goes to the wideband stage.
            const double raw = bandPeak[ub].push (std::max (std::abs (bands[b][0]), std::abs (bands[b][1])), holdSamples[ub]);
            auto& smooth = envelope[ub];
            smooth = raw < smooth ? raw : smooth + (raw - smooth) * envAttackCoef;
            env[b] = smooth;
        }
        const double sumValue = sumPeak.push (std::max (std::abs (sum0), std::abs (sum1)), sumHold);

        if (++counter >= 32)
        {
            counter = 0;
            updateHolds (env);
        }

        double target[numBands];
        computeTargets (env, sumValue, target);

        double out0 = 0.0, out1 = 0.0;
        double weighted = 0.0, weight = 0.0;
        for (int b = 0; b < numBands; ++b)
        {
            const auto ub = size_t (b);
            const double g = bandSmooth[ub].process (release[ub].process (target[b]));
            auto& d0 = bandDelay[0][ub][size_t (delayPos)];
            auto& d1 = bandDelay[1][ub][size_t (delayPos)];
            bandMeter[ub] = std::max (bandMeter[ub], std::max (std::abs (d0), std::abs (d1)));
            const double gs = g * soloGain[ub];
            out0 += gs * d0;
            out1 += gs * d1;
            d0 = bands[b][0];
            d1 = bands[b][1];
            bandMinGain[ub] = std::min (bandMinGain[ub], g);
            weighted += g * env[b];
            weight += env[b];
        }
        if (weight > 1.0e-9)
            minGain = std::min (minGain, weighted / weight * wideGain);

        // Wideband safety stage: guarantees the threshold on the actual sum (spec 5, Appendix A).
        const double widePk = widePeak.push (std::max (std::abs (out0), std::abs (out1)), lookAhead + 1);
        const double wideTarget = widePk > theta ? theta / widePk : 1.0;
        double next = wideGain + (1.0 - wideGain) * wideReleaseCoef;
        if (next > 1.0 - 1.0e-9)
            next = 1.0;
        wideGain = std::min (wideTarget, next);
        const double gw = wideSmooth.process (wideGain);

        y[0] = wideDelay[0][size_t (delayPos)] * gw;
        y[1] = wideDelay[1][size_t (delayPos)] * gw;
        wideDelay[0][size_t (delayPos)] = out0;
        wideDelay[1][size_t (delayPos)] = out1;
        if (++delayPos == lookAhead)
            delayPos = 0;
    }

    Variant variant;
    bool linearPhase = true, prepared = false, formatApplied = false, bypassed = false;
    int lookAhead = 64;
    double fs = 48000.0;
    Settings settings;

    std::unique_ptr<LinearPhaseSplitter> lpSplitter;
    LowLatencySplitter llSplitter;

    std::array<SlidingMax, numBands> bandPeak;
    SlidingMax sumPeak, widePeak;
    std::array<BoxSmoother, numBands> bandSmooth;
    BoxSmoother wideSmooth;
    std::array<BandRelease, numBands> release;
    std::array<std::array<std::vector<double>, numBands>, 2> bandDelay;
    std::array<std::vector<double>, 2> wideDelay, dryDelay;
    std::vector<double> makeupDelay;
    std::array<Requantiser, 2> requant;

    int delayPos = 0, dryPos = 0, makeupPos = 0, counter = 0, sumHold = 65;
    std::array<int, numBands> holdSamples { 65, 65, 65, 65, 65 };
    std::array<double, numBands> bandGain { 1, 1, 1, 1, 1 }, bandGainTarget { 1, 1, 1, 1, 1 };
    std::array<double, numBands> soloGain { 1, 1, 1, 1, 1 }, soloTarget { 1, 1, 1, 1, 1 };
    std::array<double, numBands> priorityWeight { 1, 1, 1, 1, 1 }, tiltSlope { 0, 0, 0, 0, 0 };
    double separation = 1.0;
    double thresholdDb = 0.0, ceilingDb = 0.0, theta = 1.0, ceilingLin = 1.0, makeup = 1.0;
    double wideGain = 1.0, wideReleaseCoef = 0.001, envAttackCoef = 0.004;
    std::array<double, numBands> envelope {};
    double rampDbPerSample = 0.01, gainStep = 0.001, bypassStep = 0.001, bypassMix = 0.0;

    std::array<double, 2> inPeak {}, outPeak {};
    std::array<double, numBands> bandMinGain { 1, 1, 1, 1, 1 }, bandMeter {};
    double minGain = 1.0;
};

} // namespace craft::band_limiter::dsp
