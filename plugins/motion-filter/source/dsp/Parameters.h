// Craft Motion Filter - parameter mappings (spec r1 section 3). Host-independent, no JUCE.
// Every host parameter's normalised value is the spec's r; fromNorm() is the spec mapping.
#pragma once

#include <array>
#include <cmath>

namespace craft::mf
{

inline double clamp01 (double r) noexcept { return r < 0.0 ? 0.0 : (r > 1.0 ? 1.0 : r); }
inline double clampd (double x, double lo, double hi) noexcept { return x < lo ? lo : (x > hi ? hi : x); }

enum class FilterType { LowPass = 0, HighPass, BandPass, BandReject, Comb, Amp };
constexpr int numFilterTypes = 6;
enum class LfoShape { Sine = 0, Triangle, Saw, Pulse, SampleHold };
constexpr int numLfoShapes = 5;

namespace param
{
    // Linear mapping lo + (hi - lo) r.
    template <int Lo, int Hi, int Def>
    struct Linear
    {
        static constexpr double minValue = Lo, maxValue = Hi, defaultValue = Def;
        static double fromNorm (double r) noexcept { return Lo + double (Hi - Lo) * clamp01 (r); }
        static double toNorm (double v) noexcept { return clamp01 ((v - Lo) / double (Hi - Lo)); }
    };

    // Exponential mapping base * ratio^r.
    struct ExpMap
    {
        double base, ratio;
        double fromNorm (double r) const noexcept { return base * std::pow (ratio, clamp01 (r)); }
        double toNorm (double v) const noexcept { return v <= base ? 0.0 : clamp01 (std::log (v / base) / std::log (ratio)); }
    };

    // Freq knob: 30 * 600^r Hz, 30 Hz .. 18 kHz, default 18 kHz (r = 1). The same knob position r
    // is the comb delay (11 + 17 r ms) and the amp gain (-80 + 80 r dB) for those filter types.
    struct Freq
    {
        static constexpr double minValue = 30.0, maxValue = 18000.0, defaultValue = 18000.0;
        static double fromNorm (double r) noexcept { return ExpMap { 30.0, 600.0 }.fromNorm (r); }
        static double toNorm (double v) noexcept { return ExpMap { 30.0, 600.0 }.toNorm (v); }
    };
    inline double combDelayMs (double r) noexcept { return 11.0 + 17.0 * clamp01 (r); }
    inline double ampGainDb (double r) noexcept { return -80.0 + 80.0 * clamp01 (r); }

    using Resonance = Linear<0, 100, 0>;    // % (comb feedback for the comb type)
    using Spread = Linear<-50, 50, 0>;      // %
    using Smooth = Linear<0, 100, 10>;      // %
    using Drive = Linear<0, 100, 0>;        // %
    using Crush = Linear<0, 100, 0>;        // 0..100
    using Gain = Linear<-40, 10, 0>;        // dB (input, output)
    using Mix = Linear<-100, 100, 100>;     // -100 dry .. +100 wet
    using Step = Linear<-100, 100, 0>;      // %
    using Swing = Linear<0, 75, 0>;         // %
    using Feedback = Linear<0, 100, 30>;    // %
    using DelayMix = Linear<-100, 100, -100>;
    using ModAmount = Linear<-100, 100, 0>; // %

    // LFO free rate 0.1 * 1000^r Hz, default 4 Hz.
    struct LfoRate
    {
        static constexpr double minValue = 0.1, maxValue = 100.0, defaultValue = 4.0;
        static double fromNorm (double r) noexcept { return ExpMap { 0.1, 1000.0 }.fromNorm (r); }
        static double toNorm (double v) noexcept { return ExpMap { 0.1, 1000.0 }.toNorm (v); }
    };
    // Sequencer free rate 0.1 * 5000^r steps/s, default 4.
    struct SeqRate
    {
        static constexpr double minValue = 0.1, maxValue = 500.0, defaultValue = 4.0;
        static double fromNorm (double r) noexcept { return ExpMap { 0.1, 5000.0 }.fromNorm (r); }
        static double toNorm (double v) noexcept { return ExpMap { 0.1, 5000.0 }.toNorm (v); }
    };
    // Delay free time 50 * 20^r ms, default 415 ms.
    struct DelayTime
    {
        static constexpr double minValue = 50.0, maxValue = 1000.0, defaultValue = 415.0;
        static double fromNorm (double r) noexcept { return ExpMap { 50.0, 20.0 }.fromNorm (r); }
        static double toNorm (double v) noexcept { return ExpMap { 50.0, 20.0 }.toNorm (v); }
    };
} // namespace param

// Note-length choice lists (fractions of a whole note). Length in quarter-note beats = 4 * num / den.
struct NoteLength
{
    int num, den;
    const char* text;
    double beats() const noexcept { return 4.0 * num / den; }
};

inline constexpr std::array<NoteLength, 12> lfoSyncChoices { { { 4, 1, "4/1" }, { 2, 1, "2/1" }, { 1, 1, "1/1" },
    { 1, 2, "1/2" }, { 3, 8, "3/8" }, { 1, 4, "1/4" }, { 3, 16, "3/16" }, { 1, 6, "1/6" }, { 1, 8, "1/8" },
    { 1, 12, "1/12" }, { 1, 16, "1/16" }, { 1, 32, "1/32" } } };
constexpr int lfoSyncDefault = 9; // 1/12

inline constexpr std::array<NoteLength, 10> seqSyncChoices { { { 1, 1, "1/1" }, { 1, 2, "1/2" }, { 3, 8, "3/8" },
    { 1, 4, "1/4" }, { 3, 16, "3/16" }, { 1, 6, "1/6" }, { 1, 8, "1/8" }, { 1, 12, "1/12" }, { 1, 16, "1/16" },
    { 1, 32, "1/32" } } };
constexpr int seqSyncDefault = 9; // 1/32

inline constexpr std::array<NoteLength, 10> delaySyncChoices { { { 1, 32, "1/32" }, { 1, 16, "1/16" }, { 1, 12, "1/12" },
    { 1, 8, "1/8" }, { 1, 6, "1/6" }, { 3, 16, "3/16" }, { 1, 4, "1/4" }, { 3, 8, "3/8" }, { 1, 2, "1/2" },
    { 1, 1, "1/1" } } };
constexpr int delaySyncDefault = 3; // 1/8

inline constexpr std::array<double, 3> delayRatioChoices { { 1.0, 1.5, 2.0 } };

// Modulation sources and destinations (nine amounts = sources x destinations).
enum ModSource { modLfo = 0, modSeq = 1, modFollower = 2 };
enum ModDest { destFreq = 0, destRes = 1, destTime = 2 };

// The complete, physical-valued settings of the processor (what the host parameters mean).
struct Settings
{
    FilterType type = FilterType::LowPass;
    bool slope24 = false;
    double freqNorm = 1.0;   // knob position r of Freq/Delay/Gain
    double resonance = 0.0;  // %
    double spread = 0.0;     // %
    double smooth = 10.0;    // %
    double drive = 0.0;      // %
    double crush = 0.0;      // 0..100
    double inputDb = 0.0, outputDb = 0.0;
    double mix = 100.0;      // -100..100

    LfoShape lfoShape = LfoShape::Sine;
    double lfoRateHz = 4.0;
    bool lfoSync = true;
    int lfoSyncIndex = lfoSyncDefault;

    std::array<double, 16> steps {}; // %
    int lastStep = 16;
    double seqRate = 4.0;            // steps per second
    bool seqSync = true;
    int seqSyncIndex = seqSyncDefault;
    double swing = 0.0;              // %

    double delayMs = 415.0;
    bool delaySync = true;
    int delaySyncIndex = delaySyncDefault;
    int delayRatioIndex = 0;
    double delayFeedback = 30.0;     // %
    double delayMix = -100.0;        // -100..100
    bool delayAnalog = false;
    bool delayRouteFeedback = false;

    bool followSidechain = false;
    bool keyboardTracking = false;

    double mod[3][3] {};             // [ModSource][ModDest], % (-100..100)

    bool bypass = false;
};

// Host transport for tempo sync (from the JUCE playhead in the plug-in).
struct Transport
{
    bool valid = false;     // a playhead with tempo was available
    bool playing = false;
    double bpm = 120.0;
    double ppq = 0.0;       // position (quarter notes) at the first sample of the block
};

} // namespace craft::mf
