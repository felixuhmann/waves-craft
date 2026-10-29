// Craft Multiband - the spec r2 section 5 / 6 test vectors (envelope tables, noise vector), shared by the unit
// tests and the offline fitting tool.
#pragma once

#include <vector>

namespace craft::multiband::spec
{

struct EnvelopeRow
{
    const char* name;
    double attackMs, releaseMs;
    double knee, rangeDb;
    bool opto, autoRelease;
    std::vector<double> values; // dB at the table times
};

// Attack after a -50 -> -10 dBFS step (band 2, 1 kHz, Threshold -30; Manual / Electro, Release 50 ms).
inline const std::vector<double> attackTimesMs { 1, 2, 5, 10, 20, 50, 100, 200, 500 };
inline const std::vector<EnvelopeRow> attackRows {
    { "Attack 0.5 ms", 0.5, 50, 0.5, -12, false, false, { -9.7, -10.3, -10.1, -10.2, -10.2, -10.2, -10.2, -10.2, -10.2 } },
    { "Attack 5 ms", 5, 50, 0.5, -12, false, false, { -2.7, -3.1, -5.5, -8.2, -9.9, -10.2, -10.2, -10.2, -10.2 } },
    { "Attack 1 ms", 1, 50, 0.5, -12, false, false, { -7.5, -8.9, -10.1, -10.2, -10.2, -10.2, -10.2, -10.2, -10.2 } },
    { "Attack 20 ms", 20, 50, 0.5, -12, false, false, { -1.3, -1.1, -1.7, -3.0, -5.2, -8.8, -10.1, -10.2, -10.2 } },
    { "Attack 50 ms", 50, 50, 0.5, -12, false, false, { -1.0, -0.6, -0.8, -1.4, -2.4, -5.1, -8.0, -9.8, -10.2 } },
    { "Attack 200 ms", 200, 50, 0.5, -12, false, false, { -0.9, -0.4, -0.3, -0.5, -0.8, -1.6, -2.9, -5.1, -8.8 } },
    { "Attack 5 ms, Knee Hard", 5, 50, 0.75, -12, false, false, { -2.5, -3.0, -5.4, -8.2, -10.0, -10.3, -10.3, -10.3, -10.3 } },
    { "Attack 5 ms, Knee Soft", 5, 50, 0.0, -12, false, false, { -3.0, -3.4, -5.6, -8.1, -9.6, -9.9, -9.9, -9.9, -9.9 } },
    { "Attack 5 ms, Range -24", 5, 50, 0.5, -24, false, false, { -3.7, -4.6, -8.4, -13.8, -18.6, -19.6, -19.7, -19.7, -19.7 } },
    { "Attack 5 ms, Range +12", 5, 50, 0.5, 12, false, false, { 3.5, 5.5, 8.2, 9.7, 10.3, 10.4, 10.4, 10.4, 10.4 } },
};

// Release after 1 s at -10 dBFS, step to -50 dBFS (Attack 5 ms).
inline const std::vector<double> releaseTimesMs { 20, 50, 100, 200, 500, 1000, 2000 };
inline const std::vector<EnvelopeRow> releaseRows {
    { "Rel 5 ms, Electro", 5, 5, 0.5, -12, false, false, { -7.0, -3.5, -1.4, -0.4, -0.2, -0.2, -0.2 } },
    { "Rel 20 ms, Electro", 5, 20, 0.5, -12, false, false, { -8.5, -4.9, -1.9, -0.4, -0.2, -0.2, -0.2 } },
    { "Rel 50 ms, Electro", 5, 50, 0.5, -12, false, false, { -9.3, -6.7, -3.3, -0.7, -0.2, -0.2, -0.2 } },
    { "Rel 200 ms, Electro", 5, 200, 0.5, -12, false, false, { -9.9, -8.9, -6.8, -3.7, -0.2, -0.2, -0.2 } },
    { "Rel 500 ms, Electro", 5, 500, 0.5, -12, false, false, { -10.1, -9.6, -8.5, -6.5, -2.7, -0.2, -0.2 } },
    { "Rel 1000 ms, Electro", 5, 1000, 0.5, -12, false, false, { -10.1, -9.9, -9.3, -8.0, -5.1, -2.2, -0.2 } },
    { "Rel 5000 ms, Electro", 5, 5000, 0.5, -12, false, false, { -10.1, -10.1, -10.0, -9.7, -8.7, -7.4, -5.5 } },
    { "Rel 20 ms, Opto", 5, 20, 0.5, -12, true, false, { -6.2, -3.2, -1.3, -0.3, -0.2, -0.2, -0.2 } },
    { "Rel 50 ms, Opto", 5, 50, 0.5, -12, true, false, { -6.3, -3.2, -1.3, -0.3, -0.2, -0.2, -0.2 } },
    { "Rel 200 ms, Opto", 5, 200, 0.5, -12, true, false, { -6.7, -3.7, -1.8, -0.7, -0.2, -0.2, -0.2 } },
    { "Rel 500 ms, Opto", 5, 500, 0.5, -12, true, false, { -7.4, -4.6, -2.6, -1.5, -0.8, -0.4, -0.2 } },
    { "Rel 1000 ms, Opto", 5, 1000, 0.5, -12, true, false, { -7.9, -5.4, -3.6, -2.5, -1.7, -1.1, -0.5 } },
    { "Rel 50 ms, Electro, Knee Hard", 5, 50, 0.75, -12, false, false, { -9.4, -6.6, -3.2, -0.5, 0.0, 0.0, 0.0 } },
    { "Rel 50 ms, Electro, Knee Soft", 5, 50, 0.0, -12, false, false, { -9.1, -6.7, -3.6, -1.1, -0.6, -0.6, -0.6 } },
    { "Rel 50 ms, Electro, Range -24", 5, 50, 0.5, -24, false, false, { -17.0, -10.7, -4.9, -1.2, -0.5, -0.5, -0.5 } },
    { "Rel 50 ms, Electro, Range +12", 5, 50, 0.5, 12, false, false, { 10.1, 8.9, 5.9, 1.5, 0.1, 0.1, 0.1 } },
};

// Section 5.2: release after a 30 ms burst at -10 dBFS from -50 dBFS (times after the burst ends; Attack 5 ms; the
// burst follows 2.5 s at -50 dBFS after a 1 s overload).
inline const std::vector<double> burstTimesMs { 20, 50, 100, 200, 500 };
inline const std::vector<EnvelopeRow> burstRows {
    { "Rel 50, Electro, Manual", 5, 50, 0.5, -12, false, false, { -1.2, -0.8, -0.4, -0.2, -0.2 } },
    { "Rel 50, Electro, Auto", 5, 50, 0.5, -12, false, true, { -1.1, -0.7, -0.4, -0.2, -0.2 } },
    { "Rel 500, Electro, Manual", 5, 500, 0.5, -12, false, false, { -4.0, -2.5, -1.9, -1.0, -0.2 } },
    { "Rel 500, Electro, Auto", 5, 500, 0.5, -12, false, true, { -1.8, -1.3, -0.9, -0.3, -0.2 } },
    { "Rel 500, Opto, Manual", 5, 500, 0.5, -12, true, false, { -4.8, -3.4, -2.7, -2.2, -1.3 } },
    { "Rel 500, Opto, Auto", 5, 500, 0.5, -12, true, true, { -2.1, -1.5, -1.4, -1.3, -0.8 } },
    { "Rel 200, Electro, Manual", 5, 200, 0.5, -12, false, false, { -2.8, -1.5, -0.9, -0.3, -0.2 } },
    { "Rel 200, Electro, Auto", 5, 200, 0.5, -12, false, true, { -1.4, -1.1, -0.5, -0.2, -0.2 } },
    { "Rel 1000, Electro, Manual", 5, 1000, 0.5, -12, false, false, { -4.5, -3.1, -2.4, -1.9, -0.2 } },
    { "Rel 1000, Electro, Auto", 5, 1000, 0.5, -12, false, true, { -1.9, -1.4, -1.2, -0.5, -0.2 } },
    { "Rel 200, Opto, Manual", 5, 200, 0.5, -12, true, false, { -4.6, -3.1, -2.2, -1.7, -0.5 } },
    { "Rel 200, Opto, Auto", 5, 200, 0.5, -12, true, true, { -2.0, -1.4, -1.3, -0.9, -0.3 } },
    { "Rel 1000, Opto, Manual", 5, 1000, 0.5, -12, true, false, { -4.9, -3.6, -2.9, -2.5, -1.9 } },
    { "Rel 1000, Opto, Auto", 5, 1000, 0.5, -12, true, true, { -2.2, -1.7, -1.6, -1.5, -1.1 } },
};

// Spec r2 section 5.3: release after overloads of different duration and level. One continuous stimulus per row
// (1 kHz, 0.5 s silence, then -50 dBFS 1 s | -10 1 s | -50 2.5 s | -10 5 ms | -50 2.5 s | -10 100 ms | -50 2.5 s |
// -10 300 ms | -50 2.5 s | -20 1 s | -50 2.5 s | -4 1 s | -50 2.5 s); values: band gain at the end of each of the five
// measured overloads ("start") and +20 / +50 / +100 / +200 / +500 ms after it.
struct OverloadRow
{
    const char* name;
    double rangeDb, knee, thresholdDb, attackMs, releaseMs;
    bool opto, autoRelease;
    std::vector<std::vector<double>> values; // 5 overloads x { start, +20, +50, +100, +200, +500 }
};
inline const std::vector<double> overloadTimesMs { 20, 50, 100, 200, 500 };
inline const std::vector<OverloadRow> overloadRows {
    { "Range -6, Attack 5, Release 50, Manual, Electro", -6, 0.5, -30, 5, 50, false, false,
      { { -2.1, -0.1, -0.1, -0.1, -0.1, -0.1 }, { -5.1, -2.2, -1.8, -0.9, -0.2, -0.1 }, { -5.1, -4.1, -3.2, -1.7, -0.3, -0.1 }, { -3.7, -3.5, -2.7, -1.4, -0.3, -0.1 }, { -5.6, -5.2, -4.0, -2.1, -0.4, -0.1 } } },
    { "Range +6, Attack 5, Release 50, Manual, Electro", 6, 0.5, -30, 5, 50, false, false,
      { { 2.7, 0.2, 0.1, 0.1, 0.1, 0.0 }, { 5.2, 3.0, 2.5, 1.3, 0.3, 0.0 }, { 5.2, 4.6, 3.9, 2.4, 0.5, 0.0 }, { 3.8, 3.6, 2.9, 1.5, 0.3, 0.0 }, { 5.6, 5.4, 4.6, 2.9, 0.7, 0.0 } } },
    { "Range -18, Attack 10, Release 100, Manual, Electro", -18, 0.5, -30, 10, 100, false, false,
      { { -2.5, -0.4, -0.4, -0.3, -0.3, -0.3 }, { -15.0, -5.7, -4.5, -3.1, -0.8, -0.3 }, { -15.0, -10.5, -8.8, -5.6, -1.8, -0.3 }, { -11.0, -10.4, -8.5, -5.3, -1.7, -0.3 }, { -16.4, -15.4, -11.8, -7.0, -2.3, -0.3 } } },
    { "Range -12, Attack 2, Release 2000, Manual, Electro", -12, 0.5, -30, 2, 2000, false, false,
      { { -6.9, -2.0, -1.0, -0.7, -0.3, -0.2 }, { -10.2, -5.5, -4.5, -4.2, -4.0, -3.1 }, { -10.2, -8.0, -7.9, -7.7, -7.2, -5.8 }, { -7.4, -7.4, -7.3, -7.1, -6.6, -5.4 }, { -11.0, -11.0, -10.9, -10.5, -9.7, -7.5 } } },
    { "Range -12, Attack 2, Release 2000, Auto, Electro", -12, 0.5, -30, 2, 2000, false, true,
      { { -6.9, -0.8, -0.5, -0.3, -0.2, -0.2 }, { -10.2, -3.8, -3.7, -3.7, -3.4, -2.6 }, { -10.2, -7.9, -7.8, -7.6, -7.1, -5.7 }, { -7.4, -7.4, -7.3, -7.1, -6.6, -5.4 }, { -11.0, -11.0, -10.9, -10.5, -9.7, -7.5 } } },
    { "Range -12, Attack 10, Release 100, Manual, Opto", -12, 0.5, -30, 10, 100, true, false,
      { { -2.0, -2.7, -1.7, -1.0, -0.7, -0.2 }, { -10.2, -4.6, -3.6, -2.7, -1.2, -0.2 }, { -10.2, -6.7, -4.3, -2.2, -0.8, -0.2 }, { -7.4, -6.3, -4.2, -2.3, -0.8, -0.2 }, { -11.0, -6.8, -3.5, -1.4, -0.4, -0.2 } } },
    { "Range -12, Attack 10, Release 100, Auto, Opto", -12, 0.5, -30, 10, 100, true, true,
      { { -2.0, -1.4, -0.8, -0.6, -0.4, -0.2 }, { -10.2, -3.5, -3.1, -2.2, -1.0, -0.2 }, { -10.2, -6.6, -4.2, -2.2, -0.8, -0.2 }, { -7.4, -6.3, -4.2, -2.3, -0.8, -0.2 }, { -11.0, -6.8, -3.5, -1.4, -0.4, -0.2 } } },
    { "Range -18, Knee Hard, Attack 5, Release 300, Auto, Electro", -18, 0.75, -30, 5, 300, false, true,
      { { -4.2, -0.2, -0.1, -0.0, -0.0, -0.0 }, { -15.2, -4.4, -4.2, -3.5, -1.7, -0.0 }, { -15.2, -10.6, -9.9, -8.1, -5.1, -0.2 }, { -11.1, -10.8, -10.0, -8.1, -5.1, -0.2 }, { -16.5, -16.0, -14.4, -11.1, -6.6, -0.5 } } },
    { "Range -12, Threshold -40, Attack 20, Release 200, Auto, Opto", -12, 0.5, -40, 20, 200, true, true,
      { { -1.9, -2.1, -1.6, -1.4, -1.3, -1.0 }, { -11.2, -4.0, -3.9, -3.3, -2.3, -1.2 }, { -11.4, -7.5, -5.2, -3.2, -1.9, -1.1 }, { -10.2, -7.0, -4.3, -2.4, -1.4, -1.0 }, { -11.7, -7.9, -4.5, -2.3, -1.3, -1.0 } } },
    { "Range +9, Knee Soft, Attack 1, Release 30, Manual, Electro", 9, 0.0, -30, 1, 30, false, false,
      { { 7.2, 0.8, 0.6, 0.5, 0.4, 0.4 }, { 7.4, 4.5, 3.4, 1.8, 0.7, 0.4 }, { 7.4, 6.5, 5.2, 2.9, 0.9, 0.4 }, { 5.4, 5.0, 3.8, 2.0, 0.7, 0.4 }, { 8.1, 7.7, 6.3, 3.7, 1.1, 0.4 } } },
};

// The section 5.3 stimulus: segments (duration s, level dBFS; -200 = digital silence) and the indices of the five
// measured overloads (5 ms, 100 ms, 300 ms at -10 dBFS, 1 s at -20 dBFS, 1 s at -4 dBFS).
struct Segment
{
    double seconds, levelDb;
};
inline const std::vector<Segment> overloadStimulus {
    { 0.5, -200 }, { 1.0, -50 }, { 1.0, -10 }, { 2.5, -50 }, { 0.005, -10 }, { 2.5, -50 }, { 0.1, -10 }, { 2.5, -50 },
    { 0.3, -10 }, { 2.5, -50 }, { 1.0, -20 }, { 2.5, -50 }, { 1.0, -4 }, { 2.5, -50 },
};
inline const std::vector<int> overloadSegments { 4, 6, 8, 10, 12 };

// Section 6 noise vector: band-2 RMS (dBFS) -> band gain (dB), Range -12, Threshold -30, Knee 0.50.
struct NoisePoint
{
    double bandRmsDb, gainDb;
};
inline const std::vector<NoisePoint> noisePoints {
    { -32.5, -5.9 }, { -28.4, -7.3 }, { -24.0, -8.8 }, { -20.1, -9.5 }, { -16.1, -10.2 }, { -12.1, -10.4 },
};

// Tolerance of the envelope tables: +-1 dB or +-25 % of |value - start| (start = band gain before the transition /
// at the end of the overload), whichever is larger.
inline double envelopeTolerance (double value, double start)
{
    const double rel = 0.25 * (value > start ? value - start : start - value);
    return rel > 1.0 ? rel : 1.0;
}

} // namespace craft::multiband::spec
