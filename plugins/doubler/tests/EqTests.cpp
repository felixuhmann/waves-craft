// Spec section 11.2: voice EQ magnitude table (48 kHz) and sample-rate independence; the EQ never
// touches the direct path.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "TestHelpers.h"

using namespace testutil;
using Catch::Approx;

namespace
{
struct Row
{
    ShelfType type;
    double freq, gain;
    double db[7];
};

const double freqs[7] = { 20, 100, 250, 1000, 4000, 10000, 20000 };
const Row table[] = {
    { ShelfType::Low, 281, 12, { 11.92, 10.34, 6.16, 0.79, 0.05, 0.01, 0.00 } },
    { ShelfType::Low, 281, -12, { -11.70, -7.44, -2.61, -0.21, -0.01, 0.00, 0.00 } },
    { ShelfType::Low, 281, -88, { -38.44, -12.70, -3.69, -0.28, -0.02, 0.00, 0.00 } },
    { ShelfType::High, 1306, 4.1, { 0.00, 0.01, 0.08, 0.96, 3.42, 4.00, 4.10 } },
    { ShelfType::High, 1189, 12, { 0.00, 0.07, 0.44, 4.44, 10.80, 11.84, 11.99 } },
    { ShelfType::High, 1189, -24, { 0.00, -0.02, -0.14, -2.04, -12.98, -21.28, -23.86 } },
    { ShelfType::High, 5040, 12, { 0.00, 0.00, 0.03, 0.40, 4.24, 9.68, 11.88 } },
};

double tolerance (ShelfType t, double f) { return (t == ShelfType::High && f > 4000.0) ? 0.5 : 0.15; }
} // namespace

TEST_CASE ("Shelf magnitudes match the table of spec section 11.2", "[eq]")
{
    const double fs = 48000.0;
    for (const auto& row : table)
    {
        const auto c = designShelf (row.type, row.freq, row.gain, fs);
        for (int k = 0; k < 7; ++k)
        {
            const double db = 20.0 * std::log10 (std::abs (c.response (freqs[k], fs)));
            INFO ((row.type == ShelfType::Low ? "low " : "high ") << row.freq << " Hz " << row.gain << " dB at " << freqs[k]);
            CHECK (db == Approx (row.db[k]).margin (tolerance (row.type, freqs[k])));
        }
    }
}

TEST_CASE ("Engine EQ acts on the voice sum only, measured through the engine", "[eq]")
{
    const double fs = GENERATE (44100.0, 48000.0, 96000.0);
    const int n = 1 << 16;

    // voice 1 (delay 20 ms, pan -45) with the low shelf 281 Hz +12 dB and the high shelf 1189 Hz -24 dB
    auto s = soloSettings();
    s.voices[0].panDeg = -45.0;
    s.eqLowOn = s.eqHighOn = true;
    s.eqLowFreqHz = 281.0;
    s.eqLowGainDb = 12.0;
    s.eqHighFreqHz = 1189.0;
    s.eqHighGainDb = -24.0;
    auto e = makeEngine (s, fs);
    std::vector<double> L (size_t (n), 0.0), R (size_t (n), 0.0);
    L[0] = 1.0;
    const auto o = run (e, L, R);
    const int d = int (0.02 * fs);
    for (double f : { 20.0, 100.0, 1000.0, 4000.0, 10000.0 })
    {
        cd acc = 0.0;
        for (int i = d; i < n; ++i)
            acc += o.l[size_t (i)] * std::polar (1.0, -2.0 * pi * f * double (i - d) / fs);
        const double expected = 20.0 * std::log10 (std::abs (designShelf (ShelfType::Low, 281, 12, fs).response (f, fs)
                                                              * designShelf (ShelfType::High, 1189, -24, fs).response (f, fs)));
        INFO ("fs " << fs << " f " << f);
        CHECK (20.0 * std::log10 (std::abs (acc)) == Approx (expected).margin (1e-6));
        // sample-rate independent: the analog prototype is the same at every rate
        const double at48 = 20.0 * std::log10 (std::abs (designShelf (ShelfType::Low, 281, 12, 48000.0).response (f, 48000.0)));
        const double atFs = 20.0 * std::log10 (std::abs (designShelf (ShelfType::Low, 281, 12, fs).response (f, fs)));
        CHECK (atFs == Approx (at48).margin (0.15));
    }

    // the direct path is untouched by the EQ
    auto sd = directSettings();
    sd.eqLowOn = sd.eqHighOn = true;
    sd.eqLowGainDb = 12.0;
    sd.eqHighGainDb = -24.0;
    auto ed = makeEngine (sd, fs);
    const auto x = noise (4096, 9);
    const auto od = run (ed, x, x);
    CHECK (maxAbsDiff (od.l, x) <= 1e-12);
}

TEST_CASE ("EQ gain changes are smoothed without clicks", "[eq]")
{
    const double fs = 48000.0;
    auto s = soloSettings();
    s.voices[0].panDeg = -45.0;
    s.eqLowOn = true;
    s.eqLowFreqHz = 300.0;
    s.eqLowGainDb = 0.0;
    auto e = makeEngine (s, fs);
    const auto x = sine (int (0.5 * fs), 100.0, fs);
    std::vector<double> l = x, r = x;
    const int half = int (0.25 * fs);
    e.process (l.data(), r.data(), half);
    s.eqLowGainDb = 12.0;
    e.setSettings (s);
    e.process (l.data() + half, r.data() + half, int (x.size()) - half);
    // a 100 Hz sine at amplitude <= 2: per-sample steps stay below the sine's own slope
    double maxStep = 0.0;
    for (size_t i = size_t (half) - 10; i < size_t (half) + size_t (0.02 * fs); ++i)
        maxStep = std::max (maxStep, std::abs (l[i + 1] - l[i]));
    CHECK (maxStep < 2.0 * 2.0 * pi * 100.0 / fs * 2.0);
}
