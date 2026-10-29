// Craft Level Rider - the complete per-sample processor (spec r1 sections 1, 3, 6-9, D6).
// Host-independent: no JUCE dependency. Double precision internally; float or double buffers.
//
// Signal flow (section 3), per block of 1 ms with the gain interpolated per sample:
//   detector input = mean of the channels -> weighting (4) -> analytic power -> block mean power P
//   activity gate (7) decides Ride / Idle
//   Ride: level detector (6) -> static law (5) -> gain one-pole (6) -> g clamped to [Range Min, Range Max]
//   Idle: hold (Slow), then move g toward the Idle value (7.3)
//   output = input * 10^((g + Output) / 20)  -- a pure gain, identical on all channels, zero latency
#pragma once

#include <algorithm>
#include <cmath>

#include "Dynamics.h"
#include "Filters.h"
#include "Parameters.h"

namespace craft::levelrider::dsp
{

// Linear ramp updated once per detector block (D6: ~20 ms, click-free).
class BlockRamp
{
public:
    void setSteps (double steps) noexcept { numSteps = std::max (1.0, steps); }
    void snap (double v) noexcept
    {
        current = target = v;
        step = 0.0;
    }
    void setTarget (double t) noexcept
    {
        if (t == target)
            return;
        target = t;
        step = std::abs (target - current) / numSteps;
    }
    double next() noexcept
    {
        if (current < target)
            current = std::min (target, current + step);
        else if (current > target)
            current = std::max (target, current - step);
        return current;
    }
    double get() const noexcept { return current; }
    double getTarget() const noexcept { return target; }

private:
    double numSteps = 20.0, step = 0.0, current = 0.0, target = 0.0;
};

class RiderEngine
{
public:
    static constexpr double blockSeconds = 0.001;
    static constexpr double parameterSmoothingSeconds = 0.020; // D6
    static constexpr double slowIdleHoldSeconds = 0.5;          // 7.3
    static constexpr double musicAverageSeconds = 10.0;         // 8: running average of the music level
    static constexpr double musicLimitDb = 6.0;                 // 8: |T_eff - T| <= 6 dB
    static constexpr double musicReacquireSeconds = 1.0;        // 8: re-seed the average after this much silence
    static constexpr double readSmoothingSeconds = 0.005;       // 9: Read mode follows the host value
    static constexpr double bypassFadeSeconds = 0.010;          // host bypass crossfade

    static constexpr int getLatencySamples() noexcept { return 0; }

    void prepare (double sampleRate) noexcept
    {
        fs = sampleRate > 0.0 ? sampleRate : 48000.0;
        blockLen = std::max (1, int (std::lround (fs * blockSeconds)));
        dt = double (blockLen) / fs;
        vocal.prepare (fs);
        music.prepare (fs);
        const double steps = parameterSmoothingSeconds / dt;
        for (auto* r : { &targetRamp, &minRamp, &maxRamp, &outputRamp, &idleRamp })
            r->setSteps (steps);
        bypassRamp.setSteps (bypassFadeSeconds / dt);
        reset();
    }

    // Start-up state (7.3): g = Idle, detectors empty, gate Idle.
    void reset() noexcept
    {
        vocal.reset();
        music.reset();
        gate.reset();
        detector.reset();
        musicDetector.reset();
        targetRamp.snap (settings.targetDb);
        const auto rr = sortedRange (settings.rangeMinDb, settings.rangeMaxDb);
        minRamp.snap (rr.minDb);
        maxRamp.snap (rr.maxDb);
        outputRamp.snap (settings.outputDb);
        idleRamp.snap (settings.idleDb);
        gDb = clampTo (settings.idleDb, rr.minDb, rr.maxDb);
        if (mode == AutomationMode::Read)
            gDb = clampTo (hostRiderDb, -12.0, 12.0);
        idleTime = 1.0e9; // not riding at start-up, the fader sits at Idle already
        acc = 0.0;
        musicAcc = 0.0;
        musicNonZero = false;
        pos = 0;
        musicPresent = false;
        musicAbsentTime = 1.0e9;
        musicAverageDb = 0.0;
        musicOffsetDb = 0.0;
        bypassRamp.snap (bypassed ? 0.0 : 1.0);
        prevLin = nextLin = appliedGain (std::pow (10.0, (gDb + settings.outputDb) / 20.0));
    }

    // Host bypass: crossfades the applied gain to exactly 1 within 10 ms (detection keeps running).
    void setBypassed (bool b) noexcept
    {
        bypassed = b;
        bypassRamp.setTarget (b ? 0.0 : 1.0);
    }

    void setSettings (const Settings& s) noexcept
    {
        settings = s;
        targetRamp.setTarget (s.targetDb);
        const auto rr = sortedRange (s.rangeMinDb, s.rangeMaxDb);
        minRamp.setTarget (rr.minDb);
        maxRamp.setTarget (rr.maxDb);
        outputRamp.setTarget (s.outputDb);
        idleRamp.setTarget (s.idleDb);
    }
    const Settings& getSettings() const noexcept { return settings; }

    // Section 9.
    void setAutomationMode (AutomationMode m) noexcept { mode = m; }
    AutomationMode getAutomationMode() const noexcept { return mode; }
    void setHostRiderDb (double db) noexcept { hostRiderDb = clampTo (db, -12.0, 12.0); }
    // GUI drag of the Rider fader: overrides g while active; riding resumes from that value on release.
    void setOverride (bool active, double db) noexcept
    {
        overrideActive = active;
        overrideDb = clampTo (db, -12.0, 12.0);
    }

    // channels: numChannels pointers of n samples, processed in place.
    // sidechain: numSidechain pointers (may be 0 / nullptr when the bus is absent or disabled).
    template <typename Sample>
    void process (Sample* const* channels, int numChannels, const Sample* const* sidechain, int numSidechain, int n) noexcept
    {
        if (numChannels <= 0 || n <= 0)
            return;
        const double invCh = 1.0 / double (numChannels);
        const bool haveSc = sidechain != nullptr && numSidechain > 0;
        const double invSc = haveSc ? 1.0 / double (numSidechain) : 0.0;

        for (int i = 0; i < n; ++i)
        {
            double sum = 0.0;
            for (int c = 0; c < numChannels; ++c)
                sum += double (channels[c][i]);
            acc += vocal.process (sum * invCh);

            if (haveSc)
            {
                double s = 0.0;
                for (int c = 0; c < numSidechain; ++c)
                    s += double (sidechain[c][i]);
                musicNonZero = musicNonZero || s != 0.0;
                musicAcc += music.process (s * invSc);
            }

            ++pos;
            const double lin = prevLin + (nextLin - prevLin) * (double (pos) / double (blockLen));
            for (int c = 0; c < numChannels; ++c)
                channels[c][i] = Sample (double (channels[c][i]) * lin);

            if (pos >= blockLen)
                endOfBlock (haveSc);
        }
    }

    // ---- state readers (any thread may poll copies through the plug-in) ---------------------------
    double getGainDb() const noexcept { return gDb; }            // ride gain g (dB), the Rider fader
    double getAppliedGainDb() const noexcept { return 20.0 * std::log10 (std::max (nextLin, 1e-12)); }
    bool isRiding() const noexcept { return gate.isRiding(); }
    double getDetectorLevelDb() const noexcept { return detector.levelDb(); } // section 6 level L
    double getSmoothLevelDb() const noexcept { return gate.smoothLevelDb(); }  // section 7.1 level
    double getMusicOffsetDb() const noexcept { return musicOffsetDb; }        // T_eff - T (section 8)
    double getEffectiveTargetDb() const noexcept { return targetRamp.get() + musicOffsetDb; }
    double getRangeMinDb() const noexcept { return minRamp.get(); }
    double getRangeMaxDb() const noexcept { return maxRamp.get(); }
    int getBlockLength() const noexcept { return blockLen; }
    double getSampleRate() const noexcept { return fs; }
    const WeightedPower& getVocalFrontEnd() const noexcept { return vocal; }

private:
    static RangePair sortedRange (double mn, double mx) noexcept
    {
        if (mn > mx)
            std::swap (mn, mx);
        return { mn, mx };
    }

    double appliedGain (double ride) const noexcept
    {
        const double mix = bypassRamp.get();
        if (mix >= 1.0)
            return ride;
        if (mix <= 0.0)
            return 1.0;
        return 1.0 + mix * (ride - 1.0);
    }

    void endOfBlock (bool haveSc) noexcept
    {
        const double p = acc / double (blockLen);
        acc = 0.0;
        pos = 0;
        vocal.flush();

        // D6 parameter smoothing (once per block)
        const double targetDb = targetRamp.next();
        double mn = minRamp.next(), mx = maxRamp.next();
        if (mn > mx)
            mn = mx = 0.5 * (mn + mx);
        const double outDb = outputRamp.next();
        const double idleDb = clampTo (idleRamp.next(), mn, mx);

        updateMusic (haveSc);

        const Ballistics b = settings.fast ? Ballistics::fast() : Ballistics::slow();
        const bool ride = gate.update (p, -62.0 - settings.vocalSensDb, settings.spillEnabled, settings.spillDb, dt);

        if (overrideActive)
        {
            gDb = overrideDb;
            idleTime = 0.0;
        }
        else if (mode == AutomationMode::Read)
        {
            // 9: g follows the host's Rider automation; the internal engine is bypassed.
            gDb += (hostRiderDb - gDb) * onePoleCoeff (readSmoothingSeconds, dt);
            if (std::abs (hostRiderDb - gDb) < 1e-9)
                gDb = hostRiderDb;
        }
        else if (ride)
        {
            idleTime = 0.0;
            detector.update (p, b, dt);
            const double gStar = StaticLaw::gainDb (detector.levelDb(), targetDb + musicOffsetDb, mn, mx);
            const double tau = gStar > gDb ? b.up : b.down;
            gDb += (gStar - gDb) * onePoleCoeff (tau, dt);
            gDb = clampTo (gDb, mn, mx);
        }
        else
        {
            // 7.3 idle: the level detector is frozen; Slow holds g for 0.5 s, Fast moves at once.
            idleTime += dt;
            const double hold = settings.fast ? 0.0 : slowIdleHoldSeconds;
            if (idleTime > hold + 1e-9)
            {
                const double stepDb = idleSpeedDbPerSecond (gDb) * dt;
                if (gDb < idleDb)
                    gDb = std::min (idleDb, gDb + stepDb);
                else if (gDb > idleDb)
                    gDb = std::max (idleDb, gDb - stepDb);
            }
            gDb = clampTo (gDb, mn, mx);
        }

        bypassRamp.next();
        prevLin = nextLin;
        nextLin = appliedGain (std::pow (10.0, (gDb + outDb) / 20.0));
    }

    // Section 8 (decision D3): T_eff = T + k (M - Mbar), k = 10^(MusicSens/20) * 0.5, limited to +-6 dB.
    void updateMusic (bool haveSc) noexcept
    {
        const double p = musicAcc / double (blockLen);
        musicAcc = 0.0;
        music.flush();
        const bool present = haveSc && musicNonZero;
        musicNonZero = false;

        if (! present || settings.musicOff)
        {
            musicAbsentTime += dt;
            musicOffsetDb = 0.0;
            return;
        }

        const bool reacquire = ! musicPresent || musicAbsentTime > musicReacquireSeconds;
        if (reacquire)
            musicDetector.reset (p); // start at the current level: no spurious offset when the music starts
        musicDetector.update (p, Ballistics::slow(), dt);
        const double m = musicDetector.levelDb();
        if (reacquire)
            musicAverageDb = m;
        musicPresent = true;
        musicAbsentTime = 0.0;
        musicAverageDb += (m - musicAverageDb) * onePoleCoeff (musicAverageSeconds, dt);
        const double k = std::pow (10.0, settings.musicSensDb / 20.0) * 0.5;
        musicOffsetDb = clampTo (k * (m - musicAverageDb), -musicLimitDb, musicLimitDb);
    }

    Settings settings;
    AutomationMode mode = AutomationMode::Off;
    double hostRiderDb = 0.0, overrideDb = 0.0;
    bool overrideActive = false;

    double fs = 48000.0, dt = 0.001;
    int blockLen = 48, pos = 0;

    WeightedPower vocal, music;
    ActivityGate gate;
    LevelDetector detector, musicDetector;
    BlockRamp targetRamp, minRamp, maxRamp, outputRamp, idleRamp, bypassRamp;
    bool bypassed = false;

    double acc = 0.0, musicAcc = 0.0;
    bool musicNonZero = false, musicPresent = false;
    double musicAbsentTime = 1.0e9, musicAverageDb = 0.0, musicOffsetDb = 0.0;

    double gDb = 0.0, idleTime = 1.0e9;
    double prevLin = 1.0, nextLin = 1.0;
};

} // namespace craft::levelrider::dsp
