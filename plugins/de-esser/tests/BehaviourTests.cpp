// Spec r1 sections 1, 3, 6, 10: parameters, identity without reduction, latency, mono variant, D2 smoothing.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "TestHelpers.h"

using namespace craft::deesser::test;
namespace pm = craft::deesser::dsp::param;

TEST_CASE ("Section 3 parameter mappings", "[params]")
{
    CHECK (pm::Threshold::fromNorm (0.0) == -80.0);
    CHECK (pm::Threshold::fromNorm (1.0) == 0.0);
    CHECK (pm::Threshold::fromNorm (0.455) == Catch::Approx (-43.6));
    CHECK (pm::Threshold::fromNorm (pm::Threshold::toNorm (-43.6)) == Catch::Approx (-43.6));
    CHECK (pm::Frequency::fromNorm (0.0) == 2000.0);
    CHECK (pm::Frequency::fromNorm (1.0) == 16000.0);
    CHECK (pm::Frequency::fromNorm (0.4870) == 5506.0);
    CHECK (pm::Frequency::toNorm (5506.0) == Catch::Approx (0.4870).margin (0.0001));
    CHECK (pm::snap<pm::Frequency> (5506.4) == 5506.0);
    CHECK_FALSE (switchState (0.49));
    CHECK (switchState (0.5));
    const Settings defaults;
    CHECK (defaults.thresholdDb == 0.0);
    CHECK (defaults.frequencyHz == 5506.0);
    CHECK (defaults.sidechain == SidechainMode::HighPass);
    CHECK (defaults.audio == AudioMode::Split);
    CHECK (defaults.monitor == MonitorMode::Output);
}

namespace
{
template <typename Sample>
void checkIdentity (AudioMode audio, SidechainMode sc, int channels, double fs)
{
    CAPTURE (int (audio), int (sc), channels, fs, sizeof (Sample));
    // Program material up to -6 dBFS peak with Threshold 0 dB: no reduction may ever happen.
    DeEsserEngine e (channels);
    Settings s;
    s.thresholdDb = 0.0;
    s.frequencyHz = 2000.0;
    s.audio = audio;
    s.sidechain = sc;
    e.setSettings (s);
    e.prepare (fs);

    std::mt19937 rng (3);
    std::uniform_real_distribution<double> u (-0.5, 0.5);
    const int n = int (fs);
    std::vector<Sample> in[2], out[2];
    for (int c = 0; c < 2; ++c)
    {
        in[c].resize (size_t (n));
        for (auto& v : in[c])
            v = Sample (u (rng));
        out[c] = in[c];
    }
    for (int pos = 0; pos < n; pos += 333)
    {
        if (pos == 9990) // Freq automation and a sidechain mode switch while playing
        {
            s.frequencyHz = 12345.0;
            s.sidechain = sc == SidechainMode::HighPass ? SidechainMode::BandPass : SidechainMode::HighPass;
            e.setSettings (s);
        }
        Sample* ch[2] = { out[0].data() + pos, out[1].data() + pos };
        e.process (ch, std::min (333, n - pos));
    }
    bool identical = true;
    for (int c = 0; c < channels; ++c)
        for (int i = 0; i < n; ++i)
            identical = identical && out[c][size_t (i)] == in[c][size_t (i)];
    CHECK (identical);
}
} // namespace

TEST_CASE ("No reduction: output bit-identical to input (both audio modes, stereo and mono)", "[identity]")
{
    for (auto audio : { AudioMode::Split, AudioMode::Wideband })
        for (auto sc : { SidechainMode::HighPass, SidechainMode::BandPass })
            for (int ch : { 2, 1 })
            {
                checkIdentity<float> (audio, sc, ch, 48000.0);
                checkIdentity<double> (audio, sc, ch, 44100.0);
            }
}

TEST_CASE ("Identity returns after reduction has released", "[identity]")
{
    const double fs = 48000.0;
    auto e = makeEngine (fs, -40.0, 5506.0, SidechainMode::HighPass, AudioMode::Split);
    Stereo in;
    in.l = steppedSine (8000.0, fs, { { -10.0, 4800 }, { -60.0, size_t (fs * 2) } });
    in.r = in.l;
    const auto out = runStereo (e, in);
    for (size_t i = in.size() - 4800; i < in.size(); ++i)
    {
        REQUIRE (out.l[i] == in.l[i]);
        REQUIRE (out.r[i] == in.r[i]);
    }
}

TEST_CASE ("Latency is zero", "[identity]")
{
    STATIC_REQUIRE (DeEsserEngine::getLatencySamples() == 0);
    // With reduction active the wideband output is the input times the current gain, sample by sample.
    const double fs = 48000.0;
    auto e = makeEngine (fs, -60.0, 2000.0, SidechainMode::HighPass, AudioMode::Wideband);
    const auto x = steppedSine (9000.0, fs, { { -6.0, 4800 } });
    for (double v : x)
    {
        double l = v, r = v;
        double* ch[2] = { &l, &r };
        e.process (ch, 1);
        REQUIRE (l == e.getCurrentGain() * v);
    }
}

TEST_CASE ("Split mode leaves the band below Freq untouched under full reduction", "[split]")
{
    const double fs = 48000.0;
    // A strong 12 kHz tone forces maximum reduction; a quiet 200 Hz tone shows the low band.
    auto e = makeEngine (fs, -80.0, 5506.0, SidechainMode::HighPass, AudioMode::Split);
    const size_t n = size_t (fs);
    Stereo in (n);
    const auto hi = steppedSine (12000.0, fs, { { -6.0, n } });
    const auto lo = steppedSine (200.0, fs, { { -30.0, n } });
    for (size_t i = 0; i < n; ++i)
        in.l[i] = in.r[i] = hi[i] + lo[i];
    const auto out = runStereo (e, in);
    const double low = gainToDb (std::abs (phasor (out.l, 200.0, fs, n / 2, n) / phasor (in.l, 200.0, fs, n / 2, n)));
    const double high = gainToDb (std::abs (phasor (out.l, 12000.0, fs, n / 2, n) / phasor (in.l, 12000.0, fs, n / 2, n)));
    CHECK (low > -0.1);   // first-order split: ~ -0.06 dB at 200 Hz for Freq 5506 (9.5 shape)
    CHECK (high < -17.0); // ~ -18 dB at 12 kHz (between the 9.5 values for 11 and 16 kHz)
}

TEST_CASE ("Mono variant (D3) equals the stereo variant fed L = R", "[mono]")
{
    const double fs = 48000.0;
    for (auto audio : { AudioMode::Split, AudioMode::Wideband })
    {
        auto st = makeEngine (fs, -50.0, 4000.0, SidechainMode::BandPass, audio, MonitorMode::Output, 2);
        auto mo = makeEngine (fs, -50.0, 4000.0, SidechainMode::BandPass, audio, MonitorMode::Output, 1);
        Stereo in;
        in.l = steppedSine (6000.0, fs, { { -40.0, 4800 }, { -12.0, 4800 }, { -50.0, 9600 } });
        in.r = in.l;
        const auto out = runStereo (st, in);
        auto mono = in.l;
        double* ch[1] = { mono.data() };
        mo.process (ch, int (mono.size()));
        for (size_t i = 0; i < mono.size(); ++i)
            REQUIRE (mono[i] == out.l[i]);
    }
}

TEST_CASE ("D2: mode switches crossfade and parameter changes glide (click-free, <= 20 ms)", "[smoothing]")
{
    const double fs = 48000.0;
    const size_t n = size_t (fs * 0.2);
    const auto x = steppedSine (7000.0, fs, { { -6.0, n } });
    // the largest sample-to-sample step of the steady sine is the reference for "no discontinuity"
    double steady = 0;
    for (size_t i = 1; i < n; ++i)
        steady = std::max (steady, std::abs (x[i] - x[i - 1]));

    struct Change
    {
        const char* name;
        void (*apply) (Settings&);
    };
    const Change changes[] = {
        { "monitor", [] (Settings& s) { s.monitor = MonitorMode::Sidechain; } },
        { "audio", [] (Settings& s) { s.audio = AudioMode::Wideband; } },
        { "sidechain", [] (Settings& s) { s.sidechain = SidechainMode::BandPass; } },
        { "threshold", [] (Settings& s) { s.thresholdDb = -60.0; } },
        { "frequency", [] (Settings& s) { s.frequencyHz = 16000.0; } },
    };
    for (const auto& c : changes)
    {
        CAPTURE (c.name);
        auto e = makeEngine (fs, -30.0, 3000.0, SidechainMode::HighPass, AudioMode::Split);
        Settings s = e.getSettings();
        std::vector<double> y = x, ref = x;
        const size_t at = n / 2;
        double* ch[2] = { y.data(), ref.data() };
        e.process (ch, int (at));
        c.apply (s);
        e.setSettings (s);
        double* ch2[2] = { y.data() + at, ref.data() + at };
        e.process (ch2, int (n - at));
        double jump = 0;
        for (size_t i = at; i < at + 48; ++i)
            jump = std::max (jump, std::abs (y[i] - y[i - 1]));
        CHECK (jump <= steady * 1.05);
    }

    // a ramp settles within 20 ms: afterwards the processor equals one started with the new settings
    auto a = makeEngine (fs, -80.0, 3000.0, SidechainMode::HighPass, AudioMode::Split, MonitorMode::Output);
    auto s = a.getSettings();
    s.monitor = MonitorMode::Sidechain;
    s.frequencyHz = 9000.0;
    s.sidechain = SidechainMode::BandPass;
    a.setSettings (s);
    std::vector<double> l (size_t (fs * 0.02)), r (l.size());
    double* ch[2] = { l.data(), r.data() };
    a.process (ch, int (l.size()));
    CHECK (a.getSettings().frequencyHz == 9000.0);
    // after 20 ms of silence the monitor of an impulse equals a fresh engine with the target settings
    auto b = makeEngine (fs, -80.0, 9000.0, SidechainMode::BandPass, AudioMode::Split, MonitorMode::Sidechain);
    std::vector<double> l1 (64, 0.0), r1 (64, 0.0), l2 (64, 0.0), r2 (64, 0.0);
    l1[0] = l2[0] = 1.0;
    double* c1[2] = { l1.data(), r1.data() };
    double* c2[2] = { l2.data(), r2.data() };
    a.process (c1, 64);
    b.process (c2, 64);
    for (size_t i = 0; i < 64; ++i)
        CHECK (l1[i] == Catch::Approx (l2[i]).margin (1e-12));
}
