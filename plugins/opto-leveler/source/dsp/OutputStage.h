// Craft Opto Leveler - output stage: tube colour (T7 / T7b), response (T8), analog noise (spec 8), smoothing.
// Host independent.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "Filters.h"

namespace craft::opto_leveler::dsp
{

// Memoryless colour curve on the post-make-up signal (fit: tools/fit_colour.py).
//   y = x + b Xe (sqrt(1 + (x/Xe)^2) - 1) + c Xo u|u| / (1 + |u|),  u = x / Xo
// The even term gives H2 rising 1 dB/dB with the output level and saturating near -46 dBc (T7b), the odd term
// H3 / H5 rising 1 dB/dB (H5 17 dB below H3) and the +0.05 ... +0.1 dB level-dependent gain of T7.
struct Colour
{
    static constexpr double b = 0.0101071, xe = 0.221167, c = 0.020744, xo = 0.532867;

    static double process (double x) noexcept
    {
        const double ue = x / xe;
        const double uo = x / xo;
        const double au = std::abs (uo);
        return x + b * xe * (std::sqrt (1.0 + ue * ue) - 1.0) + c * xo * uo * au / (1.0 + au);
    }
};

// Colour with a mild pre-emphasis (+3.15 dB shelf at 2.86 kHz) and the exact inverse de-emphasis around it, so that
// high fundamentals drive the curve slightly harder (T7: harmonics of 5 kHz are ~2.5 dB higher than of 1 kHz) while
// the small-signal response stays flat.
struct ColourStage
{
    static constexpr double emphasisHz = 2858.4, emphasisDb = 3.15;

    void prepare (double fs)
    {
        pre = FirstOrder::highShelf (emphasisHz, emphasisDb, fs);
        const double g = std::pow (10.0, emphasisDb / 20.0);
        de = FirstOrder::highShelf (emphasisHz / g, -emphasisDb, fs);
        reset();
    }
    void reset()
    {
        pre.reset();
        de.reset();
    }
    double process (double x) noexcept { return de.process (Colour::process (pre.process (x))); }
    double magnitude (double f, double fs) const { return pre.magnitude (f, fs) * de.magnitude (f, fs); }
    void flushDenormals() noexcept
    {
        if (std::abs (pre.s) < 1.0e-30)
            pre.s = 0.0;
        if (std::abs (de.s) < 1.0e-30)
            de.s = 0.0;
    }

    FirstOrder pre, de;
};

// Small-signal response of the output stage (T8; fit: tools/fit_response.py). The 5 Hz high-pass also removes the
// DC produced by the even colour term.
struct Response
{
    static constexpr double flatDb = -0.0272824, highPassHz = 4.96602, shelfHz = 3999.54, shelfDb = -0.589197;

    void prepare (double fs)
    {
        hp = FirstOrder::highPass (highPassHz, fs);
        shelf = FirstOrder::highShelf (shelfHz, shelfDb, fs);
        flat = std::pow (10.0, flatDb / 20.0);
        reset();
    }
    void reset()
    {
        hp.reset();
        shelf.reset();
    }
    double process (double x) noexcept { return flat * shelf.process (hp.process (x)); }
    double magnitude (double f, double fs) const { return flat * hp.magnitude (f, fs) * shelf.magnitude (f, fs); }
    void flushDenormals() noexcept
    {
        if (std::abs (hp.s) < 1.0e-30)
            hp.s = 0.0;
        if (std::abs (shelf.s) < 1.0e-30)
            shelf.s = 0.0;
    }

    FirstOrder hp, shelf;
    double flat = 1.0;
};

// Spec 8: white noise (-135 dBFS/Hz, independent per channel) plus mains hum harmonics (same on both channels).
class AnalogNoise
{
public:
    enum class Type
    {
        Off = 0,
        Hz50 = 1,
        Hz60 = 2
    };

    static constexpr double densityDbPerHz = -135.0;
    // peak level (dBFS) of hum harmonics k = 1 ... 6
    static constexpr double hum50[6] = { -121.0, -98.0, -117.0, -106.0, -135.0, -121.0 };
    static constexpr double hum60[6] = { -130.0, -99.0, -121.0, -106.0, -128.0, -127.0 };

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        // white noise: total power = density * fs / 2 (density per Hz over 0 ... fs/2)
        sigma = std::sqrt (std::pow (10.0, densityDbPerHz / 10.0) * fs * 0.5);
        reset();
        setType (type, true);
    }

    void reset()
    {
        phase = 0.0;
        for (int ch = 0; ch < 2; ++ch)
            rng[ch] = 0x9E3779B97F4A7C15ull * uint64_t (ch + 1) + 0x632BE59BD9B4E019ull;
    }

    void setType (Type t, bool force = false)
    {
        if (t == type && ! force)
            return;
        type = t;
        const double* levels = t == Type::Hz60 ? hum60 : hum50;
        mains = t == Type::Hz60 ? 60.0 : 50.0;
        for (int k = 0; k < 6; ++k)
            amp[k] = std::pow (10.0, levels[k] / 20.0);
        step = 2.0 * pi * mains / fs;
    }
    Type getType() const noexcept { return type; }

    // hum sample (identical on all channels); advances the phase
    double nextHum() noexcept
    {
        const double s1 = std::sin (phase), c1 = std::cos (phase);
        // sin(k t) by the Chebyshev recurrence
        double sPrev = 0.0, sCur = s1, sum = amp[0] * s1;
        for (int k = 2; k <= 6; ++k)
        {
            const double sNext = 2.0 * c1 * sCur - sPrev;
            sPrev = sCur;
            sCur = sNext;
            sum += amp[k - 1] * sCur;
        }
        phase += step;
        if (phase >= 2.0 * pi)
            phase -= 2.0 * pi;
        return sum;
    }

    // Gaussian white noise sample for channel ch (0/1): Irwin-Hall sum of 4 uniforms, unit variance.
    double nextNoise (int ch) noexcept
    {
        auto& s = rng[ch & 1];
        double acc = 0.0;
        for (int i = 0; i < 4; ++i)
        {
            s ^= s << 13;
            s ^= s >> 7;
            s ^= s << 17;
            acc += double (s >> 11) * (1.0 / 9007199254740992.0);
        }
        return (acc - 2.0) * std::sqrt (3.0) * sigma; // var of the sum = 4/12
    }

    double noiseSigma() const noexcept { return sigma; }

private:
    double fs = 48000.0, sigma = 0.0, phase = 0.0, step = 0.0, mains = 50.0;
    double amp[6] {};
    uint64_t rng[2] {};
    Type type = Type::Off;
};

// Linear ramp to a target within a fixed time (spec D4: click-free within 20 ms).
class Ramp
{
public:
    void prepare (double fs, double seconds) { steps = std::max (1, int (std::lround (fs * seconds))); }
    void setImmediate (double v) noexcept
    {
        value = targetValue = v;
        remaining = 0;
    }
    void setTarget (double v) noexcept
    {
        if (v == targetValue)
            return;
        targetValue = v;
        remaining = steps;
        increment = (targetValue - value) / double (steps);
    }
    double next() noexcept
    {
        if (remaining > 0)
        {
            value = --remaining == 0 ? targetValue : value + increment;
        }
        return value;
    }
    bool isRamping() const noexcept { return remaining > 0; }
    double current() const noexcept { return value; }
    double target() const noexcept { return targetValue; }

private:
    double value = 0.0, targetValue = 0.0, increment = 0.0;
    int steps = 960, remaining = 0;
};

} // namespace craft::opto_leveler::dsp
