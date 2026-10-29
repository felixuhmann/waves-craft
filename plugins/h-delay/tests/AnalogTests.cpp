// Spec r3 section 9: Analog modes - small-signal response, distortion, noise inside the loop, bandwidth.
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "TestHelpers.h"

using namespace testutil;

namespace
{
constexpr double fs = 48000.0;
const double respFreq[] = { 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000 };
const double respTable[5][10] = {
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { -3.4, -3.0, -1.7, -0.8, -0.4, -0.3, -0.3, -0.3, -0.4, -0.5 },
    { -0.7, -0.7, -0.7, -0.7, -0.8, -1.0, -1.4, -1.5, -1.7, -1.7 },
    { -2.1, -6.3, -8.9, -5.8, -3.6, -2.1, -1.1, -0.6, -0.5, -0.5 },
    { -0.4, -0.3, -0.4, -0.5, -0.5, -0.5, 0.0, -0.2, -0.2, 0.0 },
};
constexpr double none = 1.0; // "-": H2 below -110 dBc on the reference
// fundamental (dB re input) / H2 / H3 (dBc) of a 100 Hz tone at 0.03 / 0.1 / 0.3 / 0.95
const double distTable[5][4][3] = {
    { { -0.2, none, -52.9 }, { -0.8, none, -42.6 }, { -2.1, none, -33.5 }, { -5.3, none, -24.7 } },
    { { -2.1, -42, -44 }, { -2.2, -34, -35.5 }, { -2.9, -29, -30 }, { -5.5, -28, -24 } },
    { { -0.9, -70, -55 }, { -1.4, -60, -45 }, { -2.5, -53, -35 }, { -5.5, -50, -26 } },
    { { -10.6, -13.6, -9.3 }, { -3.6, -19.5, -20.2 }, { -2.9, -23.2, -33.8 }, { -5.5, -30.6, -27.5 } },
    { { -0.6, none, -36 }, { -0.8, none, -40.6 }, { -2.1, none, -33.4 }, { -5.3, none, -24.7 } },
};
const double amps[] = { 0.03, 0.1, 0.3, 0.95 };

double noiseRms (int mode, double feedback, double delayMs = 100.0)
{
    auto s = coreSettings (delayMs, feedback);
    s.analog = mode;
    auto e = makeEngine (s, fs);
    const size_t n = size_t (fs * 8.0);
    std::vector<double> l (n, 0.0), r (n, 0.0);
    auto y = run (e, l, r);
    double sum = 0.0;
    for (size_t i = size_t (2.0 * fs); i < n; ++i)
        sum += y.l[i] * y.l[i];
    return std::sqrt (sum / double (n - size_t (2.0 * fs)));
}
} // namespace

TEST_CASE ("Analog modes: small-signal response relative to Off", "[analog]")
{
    const double a = std::pow (10.0, -30.0 / 20.0);
    for (int i = 0; i < 10; ++i)
    {
        const double f = respFreq[i];
        const double off = toneHarmonics (0, a, f, fs).h1;
        for (int mode = 1; mode <= 4; ++mode)
        {
            const double got = db (toneHarmonics (mode, a, f, fs).h1 / off);
            // mode 3 is level/stimulus dependent: the sweep row is informative, +-3 dB at 50-500 Hz
            const double tol = (mode == 3 && f >= 50.0 && f <= 500.0) ? 3.0 : 1.0;
            INFO ("mode " << mode << " at " << f << " Hz: " << got << " dB");
            CHECK (std::abs (got - respTable[mode][i]) <= tol);
        }
    }
}

TEST_CASE ("Analog modes: 100 Hz distortion table (r2)", "[analog]")
{
    for (int mode = 0; mode <= 4; ++mode)
        for (int k = 0; k < 4; ++k)
        {
            const auto h = toneHarmonics (mode, amps[k], 100.0, fs);
            const double fund = db (h.h1 / amps[k]);
            const double h2 = db (h.h2 / h.h1 + 1e-300), h3 = db (h.h3 / h.h1);
            INFO ("mode " << mode << " a " << amps[k] << ": fund " << fund << " H2 " << h2 << " H3 " << h3);
            CHECK (std::abs (fund - distTable[mode][k][0]) <= ((mode == 3 && k == 0) ? 1.5 : 0.5));
            if (distTable[mode][k][1] == none)
                CHECK (h2 <= (mode == 0 ? -150.0 : -100.0)); // mode 4: the loop noise sets the floor
            else
                CHECK (std::abs (h2 - distTable[mode][k][1]) <= 4.0);
            CHECK (std::abs (h3 - distTable[mode][k][2]) <= 4.0);
        }
}

TEST_CASE ("Analog modes: noise inside the loop builds up with feedback", "[analog][noise]")
{
    const double fb[] = { 0, 25, 50, 75, 100 };
    const double table[5][5] = { { 0, 0, 0, 0, 0 },
                                 { -102.7, -102.4, -101.4, -99.0, -86.0 },
                                 { -85.5, -85.3, -84.3, -82.0, -68.8 },
                                 { -106.0, -105.8, -104.8, -102.5, -89.2 },
                                 { -108.7, -108.6, -107.5, -105.2, -92.4 } };
    CHECK (noiseRms (0, 60.0) == 0.0); // Off: digital silence
    for (int mode = 1; mode <= 4; ++mode)
        for (int i = 0; i < 5; ++i)
        {
            const double got = db (noiseRms (mode, fb[i]));
            INFO ("mode " << mode << " feedback " << fb[i] << " %: " << got << " dBFS");
            CHECK (std::abs (got - table[mode][i]) <= (fb[i] >= 100.0 ? 6.0 : 3.0));
        }
    // at 100 % the noise keeps accumulating per pass: more passes (shorter delay) -> more noise
    CHECK (db (noiseRms (2, 100.0, 10.0)) > db (noiseRms (2, 100.0, 500.0)) + 6.0);
}

TEST_CASE ("Analog noise spectrum is concentrated below ~100 Hz and independent per channel", "[analog][noise]")
{
    auto s = coreSettings (100.0, 0.0);
    s.analog = 2;
    auto e = makeEngine (s, fs);
    const size_t n = size_t (fs * 3.0);
    std::vector<double> l (n, 0.0), r (n, 0.0);
    auto y = run (e, l, r);
    std::vector<double> seg (y.l.begin() + long (fs), y.l.begin() + long (2 * fs));
    double low = 0.0, high = 0.0;
    for (double f = 5.0; f <= 100.0; f += 0.5)
        low = std::max (low, std::abs (dtft (seg, f, fs)));
    for (double f = 300.0; f < 20000.0; f *= 1.1)
        high = std::max (high, std::abs (dtft (seg, f, fs)));
    CHECK (db (high / low) < -20.0);
    CHECK (y.l[size_t (fs) + 1234] != y.r[size_t (fs) + 1234]);
}

TEST_CASE ("Wet path is full-band at 88.2 / 96 kHz", "[analog][bandwidth]")
{
    for (double rate : { 88200.0, 96000.0 })
        for (int mode : { 0, 2 })
        {
            auto s = coreSettings (1.0, 0.0);
            s.analog = mode;
            const double a = 0.0316;
            auto gainAt = [&] (double f) {
                auto e = makeEngine (s, rate);
                const size_t len = size_t (rate); // 1 s: whole periods for integer f
                auto x = sine (a, f, rate, size_t (rate * 0.3) + len);
                auto y = run (e, x, x);
                return db (std::abs (bin (y.l, size_t (rate * 0.3), len, f, rate)) / a);
            };
            const double ref = gainAt (1000.0);
            for (double f : { 22000.0, 26000.0, 30000.0, 0.45 * rate })
            {
                const double rel = gainAt (std::round (f)) - ref;
                INFO (rate << " Hz, mode " << mode << ", " << f << " Hz: " << rel << " dB");
                CHECK (rel > -2.5);
            }
        }
}

TEST_CASE ("Analog modes have no wow or flutter: echo timing unchanged", "[analog]")
{
    for (int mode = 1; mode <= 4; ++mode)
    {
        auto s = coreSettings (50.0, 0.0);
        s.analog = mode;
        auto e = makeEngine (s, fs);
        // a 1 kHz tone burst: the echo's phase must equal the input's phase delayed by exactly D
        auto x = sine (0.3, 1000.0, fs, 12000);
        auto y = run (e, x, x);
        const auto in = bin (x, 2000, 4800, 1000.0, fs);
        const auto out = bin (y.l, 2000 + 2400, 4800, 1000.0, fs);
        const double dphi = std::arg (out / in);
        // phase difference reflects only the mode's (minimum-phase) colouration filters: tiny at 1 kHz,
        // and identical every run (no time-varying delay)
        CHECK (std::abs (dphi) < 0.35);
        auto e2 = makeEngine (s, fs);
        auto y2 = run (e2, x, x);
        CHECK (std::abs (std::arg (bin (y2.l, 2000 + 2400, 4800, 1000.0, fs) / out)) < 1e-6);
    }
}

TEST_CASE ("Analog modes: click levels relative to Off (r3 9.2.1)", "[analog][clicks]")
{
    struct Row
    {
        double amp, feedback;
        int mode;
        double ratio[6];
    };
    const Row rows[] = {
        { 0.3, 80, 1, { -0.34, -0.29, -0.27, -0.24, -0.22, -0.22 } },
        { 0.3, 80, 2, { -0.92, -0.79, -0.71, -0.66, -0.61, -0.57 } },
        { 0.3, 80, 3, { -0.42, -0.35, -0.30, -0.27, -0.24, -0.22 } },
        { 0.3, 80, 4, { -0.02, -0.02, -0.02, -0.01, -0.02, -0.02 } },
        { 0.5, 60, 1, { -0.28, -0.24, -0.21, -0.21, -0.21, -0.21 } },
        { 0.5, 60, 3, { -0.29, -0.23, -0.21, -0.17, -0.15, -0.16 } },
    };
    auto peaks = [] (int mode, double amp, double feedback) {
        auto s = coreSettings (100.0, feedback);
        s.analog = mode;
        auto e = makeEngine (s, fs);
        auto y = impulse (e, 7 * 4801, amp, 0.0);
        std::array<double, 6> p {};
        for (int k = 0; k < 6; ++k)
        {
            const int n = 4800 + k * 4801;
            for (int i = n - 3; i <= n + 3; ++i)
                p[size_t (k)] = std::max (p[size_t (k)], std::abs (y.l[size_t (i)]));
        }
        return p;
    };
    for (const auto& row : rows)
    {
        const auto off = peaks (0, row.amp, row.feedback);
        const auto m = peaks (row.mode, row.amp, row.feedback);
        for (int k = 0; k < 6; ++k)
        {
            const double ratio = db (m[size_t (k)] / off[size_t (k)]);
            INFO ("mode " << row.mode << ", click " << row.amp << ", feedback " << row.feedback << " %, echo " << k + 1
                          << ": " << ratio << " dB (target " << row.ratio[k] << ")");
            CHECK (std::abs (ratio - row.ratio[k]) <= 0.5);
            CHECK (ratio <= 0.05); // never louder than Off
        }
    }
}
