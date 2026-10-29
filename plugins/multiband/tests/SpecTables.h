// Craft Multiband - the spec r1 section 5 / 6 test vectors (envelope tables, noise vector), shared by the unit
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

// Release after a 30 ms burst at -10 dBFS from -50 dBFS (times after the burst ends; Attack 5 ms assumed).
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

// Section 6 noise vector: band-2 RMS (dBFS) -> band gain (dB), Range -12, Threshold -30, Knee 0.50.
struct NoisePoint
{
    double bandRmsDb, gainDb;
};
inline const std::vector<NoisePoint> noisePoints {
    { -32.5, -5.9 }, { -28.4, -7.3 }, { -24.0, -8.8 }, { -20.1, -9.5 }, { -16.1, -10.2 }, { -12.1, -10.4 },
};

// Tolerance of the envelope tables: +-1 dB or +-25 % of the change from the start value, whichever is larger.
inline double envelopeTolerance (double value, double start)
{
    const double rel = 0.25 * (value > start ? value - start : start - value);
    return rel > 1.0 ? rel : 1.0;
}

} // namespace craft::multiband::spec
