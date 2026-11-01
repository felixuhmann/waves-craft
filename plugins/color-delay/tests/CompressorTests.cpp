// Spec r1 section 5 (compressor model), section 10 (tolerances) and the section 11 compressor test vectors.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "TestHelpers.h"

using namespace cdtest;
using Catch::Approx;

namespace
{
// Steady-state gain (dB) of the wet path for a sine of the given peak level (dBFS at the compressor input).
// Levels above 0 dBFS are reached with the Input gain, as in the spec's measurement.
double sineGainDb (double levelDb, double freq = 1000.0, double seconds = 2.0)
{
    Settings s = plainWet();
    double amp = std::pow (10.0, levelDb / 20.0);
    if (levelDb > 0.0)
    {
        s.inputDb = levelDb;
        amp = 1.0;
    }
    const int n = (int) (48000 * seconds);
    const auto x = sine (freq, amp, n);
    const auto out = render (s, x, std::vector<double> ((size_t) n, 0.0));
    const size_t from = (size_t) n - 9600;
    return db (rms (out.l, from, (size_t) n) / (rms (x, from - 1, (size_t) n - 1) * std::pow (10.0, s.inputDb / 20.0)));
}

// Independent straight-line implementation of the spec section 5 model (gain per sample).
std::vector<double> modelGain (const std::vector<double>& l, const std::vector<double>& r, double fs)
{
    const double T = -23.23, R = 6.196, W = 23.22, p = 0.0292;
    const double rel = std::exp (-1.0 / (1.06e-3 * fs)), att = std::exp (-1.0 / (8.17e-3 * fs)),
                 relg = std::exp (-1.0 / (0.439 * fs));
    double e = 0.0, gr = 0.0;
    std::vector<double> g (l.size());
    for (size_t i = 0; i < l.size(); ++i)
    {
        const double lvl = std::max (std::abs (l[i]), std::abs (r[i]));
        e = lvl > e ? lvl : rel * e + (1.0 - rel) * lvl;
        const double X = 20.0 * std::log10 (std::max (e, 1e-30));
        double t = 0.0;
        if (2.0 * (X - T) > W) t = (X - T) * (1.0 - 1.0 / R);
        else if (2.0 * (X - T) >= -W) t = (1.0 - 1.0 / R) * (X - T + W / 2.0) * (X - T + W / 2.0) / (2.0 * W);
        const double c = t > gr ? att : relg;
        gr = t + c * (gr - t);
        g[i] = (1.0 - p) * std::pow (10.0, -gr / 20.0) + p;
    }
    return g;
}
} // namespace

TEST_CASE ("Compressor: static gain at 1 kHz within +-0.3 dB of the measured values", "[compressor]")
{
    const double pts[][2] = { { -33, -0.10 }, { -27, -1.07 }, { -21, -3.25 }, { -15, -6.78 }, { -9, -11.16 },
                              { -3, -15.58 }, { 0, -17.56 },  { 6, -20.83 },  { 12, -23.07 } };
    for (const auto& p : pts)
    {
        const double g = sineGainDb (p[0]);
        INFO ("level " << p[0] << " dBFS: gain " << g << " dB, expected " << p[1]);
        CHECK (g == Approx (p[1]).margin (0.3));
    }
}

TEST_CASE ("Test vector: 1 kHz sine, delay 0 ms, gain at -30 / -12 / 0 dBFS", "[compressor][vector]")
{
    CHECK (sineGainDb (-30.0) == Approx (-0.47).margin (0.3));
    CHECK (sineGainDb (-12.0) == Approx (-8.91).margin (0.3));
    CHECK (sineGainDb (0.0) == Approx (-17.56).margin (0.3));
}

TEST_CASE ("Compressor: frequency dependence at -6 dBFS (mid band)", "[compressor]")
{
    // 250 Hz - 4 kHz: -13.4 dB; 8 kHz -13.2, 16 kHz -12.8 (model within 1 dB). The LF part (30/60 Hz) is a known
    // deviation of the spec model at 30 Hz (model -13.1 dB vs -10.3 dB measured, see IMPLEMENTATION_NOTES.md).
    for (double f : { 250.0, 500.0, 1000.0, 2000.0, 4000.0 })
        CHECK (sineGainDb (-6.0, f) == Approx (-13.4).margin (0.3));
    CHECK (sineGainDb (-6.0, 8000.0) == Approx (-13.2).margin (1.0));
    CHECK (sineGainDb (-6.0, 16000.0) == Approx (-12.8).margin (1.0));
    CHECK (sineGainDb (-6.0, 120.0) == Approx (-13.3).margin (0.5));
    CHECK (sineGainDb (-6.0, 60.0, 3.0) == Approx (-12.8).margin (1.0));
}

TEST_CASE ("Test vector: gain recovery after a -6 -> -40 dBFS step", "[compressor][vector]")
{
    Settings s = plainWet();
    const double fs = 48000.0;
    const int n1 = 48000, n2 = 72000;
    std::vector<double> x ((size_t) (n1 + n2));
    for (int i = 0; i < n1 + n2; ++i)
        x[(size_t) i] = (i < n1 ? std::pow (10.0, -6.0 / 20.0) : 0.01) * std::sin (twoPi * 1000.0 * i / fs);
    const auto out = render (s, x, std::vector<double> (x.size(), 0.0));
    auto gainAt = [&] (double ms) {
        const size_t c = (size_t) n1 + (size_t) (ms * fs / 1000.0);
        const size_t from = ms <= 0.0 ? (size_t) n1 - 480 : c - 240, to = ms <= 0.0 ? (size_t) n1 : c + 240;
        return db (rms (out.l, from, to) / rms (x, from - 1, to - 1));
    };
    CHECK (gainAt (0.0) == Approx (-13.42).margin (0.3));
    CHECK (gainAt (100.0) == Approx (-10.78).margin (0.3));
    // the spec's model gives -4.45 dB here (observed -5.35 dB); the model is normative, see notes
    CHECK (gainAt (500.0) == Approx (-5.35).margin (1.0));
    CHECK (gainAt (1200.0) > -1.0);
}

TEST_CASE ("Compressor envelope matches the section 5 model on steps and bursts (<= 0.5 dB RMS)", "[compressor]")
{
    const double fs = 48000.0;
    const int n = 48000 * 4;
    std::vector<double> x ((size_t) n);
    // staircase / bursts: -40, -20, -6, -30, 0, -45, -12 dBFS segments of varying length, 1 kHz and 300 Hz
    const double lv[] = { -40, -20, -6, -30, 0, -45, -12, -25 };
    const int len[] = { 9600, 24000, 12000, 30000, 4800, 36000, 24000, 51600 };
    int pos = 0;
    for (int k = 0; k < 8; ++k)
        for (int i = 0; i < len[k] && pos < n; ++i, ++pos)
            x[(size_t) pos] = std::pow (10.0, lv[k] / 20.0) * std::sin (twoPi * (k % 2 ? 300.0 : 1000.0) * pos / fs);
    const auto out = render (plainWet(), x, std::vector<double> ((size_t) n, 0.0));
    const auto g = modelGain (x, std::vector<double> ((size_t) n, 0.0), fs);
    std::vector<double> ref ((size_t) n, 0.0);
    for (int i = 1; i < n; ++i)
        ref[(size_t) i] = x[(size_t) i - 1] * g[(size_t) i - 1]; // 1-sample delay line
    double acc = 0.0;
    int cnt = 0;
    for (int i = 480; i + 480 <= n; i += 480)
    {
        const double a = rms (out.l, (size_t) i, (size_t) i + 480), b = rms (ref, (size_t) i, (size_t) i + 480);
        if (b < 1e-5) continue;
        const double d = db (a / b);
        acc += d * d;
        ++cnt;
    }
    CHECK (std::sqrt (acc / cnt) <= 0.5);
}

TEST_CASE ("Compressor: stereo-linked, gain applied equally to both channels", "[compressor]")
{
    const int n = 48000;
    const auto loud = sine (1000.0, 0.5, n);
    const auto quiet = sine (700.0, 0.001, n);
    const auto out = render (plainWet(), loud, quiet);
    // the quiet right channel is compressed by the loud left channel's gain
    const double gl = db (rms (out.l, 38400, 48000) / rms (loud, 38399, 47999));
    const double gr = db (rms (out.r, 38400, 48000) / rms (quiet, 38399, 47999));
    CHECK (gl < -10.0);
    CHECK (gr == Approx (gl).margin (0.1));
}

TEST_CASE ("Compressor: isolated impulse peak within 1 dB of the reference (0.5 FS -> x0.908)", "[compressor]")
{
    const auto out = render (plainWet(), impulse (4800, 0.5), std::vector<double> (4800, 0.0));
    CHECK (std::abs (db (out.l[1] / 0.5) - db (0.908)) <= 1.0);
}
