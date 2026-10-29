// Craft Audio doubler - parameter mapping, quantisation and settings (spec r1 section 3).
// Host-independent: no JUCE dependency.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>

namespace craft::doubler
{

inline constexpr double pi = 3.14159265358979323846;
inline constexpr int maxVoices = 4;

// Round x to the nearest multiple of 1/stepsPerUnit (ties away from zero).
inline double quantise (double x, double stepsPerUnit) noexcept { return std::round (x * stepsPerUnit) / stepsPerUnit; }
inline double clamp01 (double r) noexcept { return r < 0.0 ? 0.0 : (r > 1.0 ? 1.0 : r); }

// Each mapping: range, quantisation resolution, r -> value (quantised) and value -> r (exact inverse
// of the continuous mapping). Defaults are per parameter and live in Settings below.
namespace param
{
    // linear mapping lo + (hi - lo) r
    template <int Lo, int Hi, int Steps>
    struct Linear
    {
        static constexpr double minValue = Lo, maxValue = Hi, stepsPerUnit = Steps;
        static double fromNorm (double r) noexcept { return quantise (Lo + double (Hi - Lo) * clamp01 (r), Steps); }
        static double toNorm (double v) noexcept { return clamp01 ((v - Lo) / double (Hi - Lo)); }
    };

    using MasterGain = Linear<-24, 0, 10>;   // dB, 0.1 dB
    using LevelGain  = Linear<-48, 0, 10>;   // direct / voice gain, dB, 0.1 dB
    using Pan        = Linear<-45, 45, 10>;  // degrees, 0.1 deg
    using EqGain     = Linear<-88, 12, 10>;  // dB, 0.1 dB
    using DelayMs    = Linear<0, 100, 10>;   // ms, 0.1 ms
    using Feedback   = Linear<0, 100, 1>;    // %, 1 %
    using Detune     = Linear<-100, 100, 1>; // cents, 1 cent
    using Depth      = Linear<-200, 200, 1>; // cents, 1 cent

    // Frequency taper F(r) = 15.625 * 2^(125 r / 12) Hz, 1 Hz resolution.
    struct Frequency
    {
        static constexpr double minValue = 15.625, maxValue = 21357.0, stepsPerUnit = 1.0;
        static double raw (double r) noexcept { return 15.625 * std::pow (2.0, 125.0 * clamp01 (r) / 12.0); }
        static double fromNorm (double r) noexcept { return quantise (raw (r), 1.0); }
        static double toNorm (double v) noexcept
        {
            if (v <= 15.625) return 0.0;
            return clamp01 (std::log2 (v / 15.625) * 12.0 / 125.0);
        }
    };

    // Modulation rate 0.1 * 4000^r Hz, 0.1 resolution.
    struct Rate
    {
        static constexpr double minValue = 0.1, maxValue = 400.0, stepsPerUnit = 10.0;
        static double fromNorm (double r) noexcept { return std::max (0.1, quantise (0.1 * std::pow (4000.0, clamp01 (r)), 10.0)); }
        static double toNorm (double v) noexcept
        {
            if (v <= 0.1) return 0.0;
            return clamp01 (std::log (v / 0.1) / std::log (4000.0));
        }
    };

    // Three-way input routing: r < 0.3 / < 0.8 / >= 0.8 -> 0 / 1 / 2.
    struct Routing
    {
        static constexpr double minValue = 0.0, maxValue = 2.0, stepsPerUnit = 1.0;
        static double fromNorm (double r) noexcept { return r < 0.3 ? 0.0 : (r < 0.8 ? 1.0 : 2.0); }
        static double toNorm (double v) noexcept { return v < 0.5 ? 0.0 : (v < 1.5 ? 0.55 : 1.0); }
    };

    // Snap an arbitrary physical value to the nearest value reachable through the mapping.
    template <typename P>
    double snap (double v) noexcept { return P::fromNorm (P::toNorm (v)); }
} // namespace param

// Input routing (spec section 3): A feeds voices 1 and 3, B feeds voices 2 and 4.
//   A: 0 = L, 1 = (L+R)/2, 2 = (L-R)/2       B: 0 = R, 1 = (R+L)/2, 2 = (R-L)/2
enum class Routing { Direct = 0, Sum = 1, Difference = 2 };

struct VoiceSettings
{
    bool on = false;
    double gainDb = -6.0, panDeg = 0.0, delayMs = 10.0, feedbackPct = 0.0;
    bool octave = false;
    double detuneCents = 0.0, depthCents = 0.0, rateHz = 1.0;
};

struct Settings
{
    double masterDb = 0.0;
    double fbHighPassHz = 16.0;
    bool modReset = false;   // trigger: a rising edge restarts every LFO
    bool range20 = false;    // false: 80 Hz, true: 20 Hz
    bool alignDirect = false;
    bool directOn = true;
    double directGainDb = 0.0, directPanDeg = 0.0;
    bool eqLowOn = true, eqHighOn = true;
    double eqLowGainDb = 0.0, eqLowFreqHz = 101.0;
    double eqHighGainDb = 4.1, eqHighFreqHz = 1306.0;
    std::array<VoiceSettings, maxVoices> voices {};
    Routing routingA = Routing::Direct, routingB = Routing::Direct;
    bool bypass = false;
};

// Defaults of spec section 3 for the 4-voice and the 2-voice variant.
inline Settings defaultSettings (int numVoices)
{
    Settings s;
    if (numVoices >= 4)
    {
        const bool on[4] = { true, false, true, false };
        const double gain[4] = { -6, -12, -6, -12 }, pan[4] = { -45, 45, 45, -45 }, delay[4] = { 9.4, 16.0, 23.7, 21.0 },
                     detune[4] = { 6, 10, -6, -10 };
        for (int i = 0; i < 4; ++i)
        {
            auto& v = s.voices[size_t (i)];
            v.on = on[i];
            v.gainDb = gain[i];
            v.panDeg = pan[i];
            v.delayMs = delay[i];
            v.detuneCents = detune[i];
        }
    }
    else
    {
        // 2-voice variant: gains -6/-6 dB, pans -45/+45, delays 9.4/23.7 ms, detunes +6/-6, both on.
        const double pan[2] = { -45, 45 }, delay[2] = { 9.4, 23.7 }, detune[2] = { 6, -6 };
        for (int i = 0; i < 2; ++i)
        {
            auto& v = s.voices[size_t (i)];
            v.on = true;
            v.gainDb = -6.0;
            v.panDeg = pan[i];
            v.delayMs = delay[i];
            v.detuneCents = detune[i];
        }
        for (int i = 2; i < 4; ++i)
            s.voices[size_t (i)].on = false;
    }
    return s;
}

} // namespace craft::doubler
