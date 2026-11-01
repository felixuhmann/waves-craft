// Spec r2 additions: 7.5 release after overloads, 6 low-frequency levels, 10 noise bands and hot-to-quiet rule,
// 11 output-stage recovery and over-ceiling gains. All at 44.1 / 48 / 96 kHz unless the table is rate-bound.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <complex>

#include "BurstTable.h"
#include "TestHelpers.h"

using namespace testutil;
using Catch::Approx;

namespace
{
const double rates[3] = { 44100.0, 48000.0, 96000.0 };

Settings analogSettings (int mode)
{
    Settings s = neutralSettings();
    s.analog = mode;
    return s;
}

// RMS gain (dB) over [a, b) at sample indices, output re the reference signal
double gainDb (const std::vector<double>& y, const std::vector<double>& x, size_t a, size_t b)
{
    return linToDb (rms (y, a, b) / rms (x, a, b));
}

void fft (std::vector<std::complex<double>>& a)
{
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i)
    {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j)
            std::swap (a[i], a[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1)
    {
        const auto wl = std::polar (1.0, -2.0 * pi / double (len));
        for (size_t i = 0; i < n; i += len)
        {
            std::complex<double> w (1.0);
            for (size_t k = 0; k < len / 2; ++k, w *= wl)
            {
                const auto u = a[i + k], v = a[i + k + len / 2] * w;
                a[i + k] = u + v;
                a[i + k + len / 2] = u - v;
            }
        }
    }
}
} // namespace

TEST_CASE ("Release after overloads (r2 section 7.5: +-1.5 dB, +-3 dB within 5 ms)", "[r2][burst]")
{
    const auto table = burst::loadTable (PUNCH_RELEASE_TABLE);
    REQUIRE (table.size() == 96);
    for (double fs : rates)
        for (double rel : { 10.0, 30.0, 100.0, 300.0, 1000.0, 3000.0 })
        {
            const auto rows = burst::rowsFor (table, rel);
            const auto got = burst::run (rows, rel, fs);
            for (size_t i = 0; i < rows.size(); ++i)
                for (size_t k = 0; k < burst::times.size(); ++k)
                {
                    INFO ("fs " << fs << " Release " << rel << " burst " << rows[i].burstMs << " ms at " << rows[i].burstDb
                                << " dBFS, t " << burst::times[k]);
                    CHECK (got[i][k] == Approx (rows[i].values[k]).margin (burst::times[k] <= 5.0 ? 3.0 : 1.5));
                }
        }
}

TEST_CASE ("Low-frequency level under compression (r2 section 6)", "[r2][detector]")
{
    const double amp[3] = { 0.1, 0.3, 0.95 }, gain[3] = { 4.86, -3.57, -10.23 }, h3[3] = { -35.7, -45.2, -38.8 };
    for (double fs : rates)
        for (int i = 0; i < 3; ++i)
        {
            const auto r = measureTone (fs, trajectorySettings(), 100.0, amp[i], 0.5, 2.0);
            INFO ("fs " << fs << " a " << amp[i] << ": " << r.gainDb << " / " << r.h3Dbc);
            CHECK (r.gainDb == Approx (gain[i]).margin (1.0));
            CHECK (r.h3Dbc == Approx (h3[i]).margin (4.0));
        }
}

TEST_CASE ("Analog noise band densities (r2 section 10, +-4 dB)", "[r2][analog]")
{
    const double f[7] = { 20, 63, 200, 630, 2000, 6300, 16000 };
    const double table[5][7] = { {},
                                 { 18.0, 18.6, 9.4, 1.1, -4.0, -7.8, -15.5 },
                                 { 5.2, 3.4, -5.6, -13.1, -16.4, -10.9, 4.2 },
                                 { 25.3, 17.8, 8.4, -4.5, -9.2, -7.2, -4.4 },
                                 { 18.5, 16.9, 15.1, 7.6, -2.2, -11.6, -21.0 } };
    for (double fs : rates)
        for (int m = 1; m <= 4; ++m)
        {
            const size_t N = 1u << 15; // about 0.7 s per frame at 48 kHz
            const int frames = 12;
            Signal in;
            in.l.assign (N * size_t (frames) + 8192, 0.0);
            in.r = in.l;
            const auto y = runAligned (in, fs, analogSettings (m)).l;
            std::vector<double> power (N / 2, 0.0);
            double wsum = 0.0;
            for (size_t i = 0; i < N; ++i)
            {
                const double w = 0.5 - 0.5 * std::cos (2.0 * pi * double (i) / double (N));
                wsum += w * w;
            }
            for (int fr = 0; fr < frames; ++fr)
            {
                std::vector<std::complex<double>> a (N);
                for (size_t i = 0; i < N; ++i)
                    a[i] = y[8192 + size_t (fr) * N + i] * (0.5 - 0.5 * std::cos (2.0 * pi * double (i) / double (N)));
                fft (a);
                for (size_t k = 0; k < N / 2; ++k)
                    power[k] += std::norm (a[k]);
            }
            // density re a white noise of the table's total level at 48 kHz (an absolute density: rate independent)
            const double total = dbToLin (craft::punch::analogModeParams()[size_t (m)].noiseDbfs);
            const double whitePerHz = total * total / 24000.0;
            for (int b = 0; b < 7; ++b)
            {
                double sum = 0.0;
                int count = 0;
                for (size_t k = 1; k < N / 2; ++k)
                {
                    const double hz = double (k) * fs / double (N);
                    if (hz >= f[b] && hz < 1.5 * f[b])
                    {
                        sum += power[k];
                        ++count;
                    }
                }
                REQUIRE (count > 0);
                const double perBin = sum / count / frames; // |X|^2 with window energy wsum
                const double perHz = 2.0 * perBin / (wsum * fs);
                const double db = 10.0 * std::log10 (perHz / whitePerHz);
                INFO ("fs " << fs << " Mode " << m << " band " << f[b] << ": " << db);
                CHECK (db == Approx (table[m][b]).margin (4.0));
            }
        }
}

TEST_CASE ("Hot-to-quiet: no DC or level transient after a hot passage (r2 section 10)", "[r2][analog]")
{
    for (double fs : rates)
        for (int m = 1; m <= 4; ++m)
        {
            // -9 dBFS -> -43 dBFS at Output +12 dB
            Settings s = analogSettings (m);
            s.outputDb = 12.0;
            const size_t hot = size_t (0.5 * fs), n = hot + size_t (0.5 * fs);
            std::vector<double> x (n);
            for (size_t i = 0; i < n; ++i)
                x[i] = (i < hot ? dbToLin (-9.0) : dbToLin (-43.0)) * std::sin (2.0 * pi * 1000.0 * double (i) / fs);
            const auto y = runAligned (stereo (x), fs, s).l;
            const size_t ms = size_t (std::lround (fs / 1000.0));
            const auto mean = [&] (size_t from) { // over whole periods (10 ms) at every rate
                double acc = 0.0;
                const size_t len = size_t (std::lround (0.01 * fs));
                for (size_t i = from; i < from + len; ++i)
                    acc += y[i];
                return acc / double (len);
            };
            const double settled = gainDb (y, x, hot + size_t (0.3 * fs), hot + size_t (0.45 * fs));
            for (size_t t = 2; t < 300; t += 2)
            {
                INFO ("fs " << fs << " Mode " << m << " t " << t << " ms");
                CHECK (std::abs (mean (hot + t * ms)) <= 0.002);
                if (t < 200)
                    CHECK (gainDb (y, x, hot + t * ms, hot + (t + 1) * ms) == Approx (settled).margin (t == 2 ? 1.0 : 0.5)); // see notes A11
            }
            if (m == 3)
                CHECK (settled == Approx (7.9).margin (0.5));
        }
}

TEST_CASE ("Output stage reduces only as needed and recovers within 2 ms (r2 section 11)", "[r2][stage]")
{
    for (double fs : rates)
    {
        const size_t ms = size_t (std::lround (fs / 1000.0));
        SECTION ("bursts over -30 dBFS at Output +12 (fs " + std::to_string (int (fs)) + ")")
        {
            for (double burstMs : { 1.0, 5.0, 20.0, 200.0 })
            {
                Settings s = neutralSettings();
                s.outputDb = 12.0;
                const size_t start = size_t (0.1 * fs), len = size_t (burstMs * fs / 1000.0), n = start + len + size_t (0.1 * fs);
                std::vector<double> x (n);
                for (size_t i = 0; i < n; ++i)
                    x[i] = (i >= start && i < start + len ? dbToLin (-6.0) : dbToLin (-30.0)) * std::sin (2.0 * pi * 1000.0 * double (i) / fs);
                const auto y = runAligned (stereo (x), fs, s).l;
                INFO ("burst " << burstMs);
                if (burstMs >= 5.0)
                    CHECK (gainDb (y, x, start + 2 * ms, start + len - ms) == Approx (6.0).margin (0.5));
                CHECK (gainDb (y, x, start + len + 2 * ms, start + len + 3 * ms) == Approx (12.0).margin (0.5));
                CHECK (maxAbs (y) <= 1.0);
                CHECK (maxAbs (y) >= 0.99);
            }
        }
        SECTION ("-6 dBFS step at R 4, T -24, Output +12 (fs " + std::to_string (int (fs)) + ")")
        {
            Settings s = trajectorySettings();
            s.outputDb = 12.0;
            const size_t start = size_t (0.5 * fs), n = start + size_t (0.3 * fs);
            std::vector<double> x (n);
            for (size_t i = 0; i < n; ++i)
                x[i] = (i >= start ? dbToLin (-6.0) : dbToLin (-40.0)) * std::sin (2.0 * pi * 1000.0 * double (i) / fs);
            const auto y = runAligned (stereo (x), fs, s).l;
            for (size_t t = 0; t < 12; ++t)
            {
                INFO ("t " << t);
                CHECK (gainDb (y, x, start + t * ms, start + (t + 1) * ms) == Approx (6.0).margin (0.5));
            }
            const double own = -7.64 + 12.0;
            CHECK (gainDb (y, x, start + 100 * ms, start + 101 * ms) == Approx (own).margin (0.5));
            double minGain = 100.0;
            for (size_t t = 12; t < 100; ++t)
                minGain = std::min (minGain, gainDb (y, x, start + t * ms, start + (t + 1) * ms));
            CHECK (minGain >= own - 0.5); // no extra dip
        }
    }
}
