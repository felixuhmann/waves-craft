// Spec r1 sections 1, 3, 8, 9, D5, D6: pure gain, zero latency, identical channel gain, Output trim, the music
// sidechain rule (bit-exact when inactive), Read / override, bypass and host block-size independence.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "TestHelpers.h"

using namespace craft::levelrider::test;
using Catch::Approx;

namespace
{
// Stereo band noise with different L / R content; returns {input, output}.
struct Run
{
    std::vector<double> inL, inR, outL, outR;
};

Run runStereo (RiderEngine& e, double seconds, double levelDb, unsigned seed, int block = 512, const std::vector<double>* sc = nullptr)
{
    const double fs = e.getSampleRate();
    BandNoise a (fs, seed), b (fs, seed + 100);
    const size_t n = size_t (seconds * fs);
    Run r;
    r.inL.resize (n);
    r.inR.resize (n);
    for (size_t i = 0; i < n; ++i)
    {
        r.inL[i] = dbToGain (levelDb) * a.next();
        r.inR[i] = dbToGain (levelDb - 3.0) * b.next();
    }
    r.outL = r.inL;
    r.outR = r.inR;
    for (size_t pos = 0; pos < n; pos += size_t (block))
    {
        const int len = int (std::min (size_t (block), n - pos));
        double* ch[2] = { r.outL.data() + pos, r.outR.data() + pos };
        if (sc != nullptr)
        {
            const double* s[2] = { sc->data() + pos, sc->data() + pos };
            e.process (ch, 2, s, 2, len);
        }
        else
            e.process (ch, 2, (const double* const*) nullptr, 0, len);
    }
    return r;
}
} // namespace

TEST_CASE ("Output is a pure gain, identical on both channels, with zero latency", "[engine]")
{
    auto e = makeEngine (48000.0, -12, 12);
    CHECK (RiderEngine::getLatencySamples() == 0);
    const auto r = runStereo (e, 10.0, -27.0, 1);
    double worst = 0.0;
    for (size_t i = 0; i < r.inL.size(); ++i)
        if (std::abs (r.inL[i]) > 1e-4 && std::abs (r.inR[i]) > 1e-4)
        {
            // same instantaneous gain on L and R, sample-aligned (no delay, no filtering)
            const double gl = r.outL[i] / r.inL[i], gr = r.outR[i] / r.inR[i];
            worst = std::max (worst, std::abs (gl - gr) / gl);
        }
    CHECK (worst < 1e-12);

    // Residual against the per-block gain (section 3: one gain per 1 ms block, interpolated per sample):
    // least-squares fit of (a + b n) * x per 48-sample block, <= -120 dB. Nothing but that gain reaches the output.
    const size_t win = 48;
    double resid = 0.0, energy = 0.0;
    for (size_t p = 0; p + win <= r.inL.size(); p += win)
    {
        double s00 = 0, s01 = 0, s11 = 0, y0 = 0, y1 = 0;
        for (size_t i = p; i < p + win; ++i)
        {
            const double x = r.inL[i], t = double (i - p);
            s00 += x * x;
            s01 += x * x * t;
            s11 += x * x * t * t;
            y0 += x * r.outL[i];
            y1 += x * t * r.outL[i];
        }
        const double det = s00 * s11 - s01 * s01;
        const double a = (y0 * s11 - y1 * s01) / det, b = (y1 * s00 - y0 * s01) / det;
        for (size_t i = p; i < p + win; ++i)
        {
            const double d = r.outL[i] - (a + b * double (i - p)) * r.inL[i];
            resid += d * d;
            energy += r.outL[i] * r.outL[i];
        }
    }
    INFO ("residual " << 10.0 * std::log10 (resid / energy) << " dB");
    CHECK (10.0 * std::log10 (resid / energy) <= -120.0);
}

TEST_CASE ("A settled gain is a scalar per 5 ms window (no colouration)", "[engine]")
{
    auto e = makeEngine (48000.0, -12, 12);
    runSegments (e, { { -21.0, 8.0 } }, Stimulus::Sine, 1000.0); // continues phase-continuously below
    // after settling the gain is constant: output = k * input per 5 ms window, residual <= -120 dB
    std::vector<double> l (240), r (240), in (240);
    double resid = 0.0, energy = 0.0;
    for (int w = 0; w < 100; ++w)
    {
        for (size_t i = 0; i < 240; ++i)
            l[i] = r[i] = in[i] = dbToGain (-21.0) * std::sqrt (2.0)
                                  * std::sin (2.0 * pi * 1000.0 * double (384000 + w * 240 + int (i)) / 48000.0);
        double* ch[2] = { l.data(), r.data() };
        e.process (ch, 2, (const double* const*) nullptr, 0, 240);
        double xy = 0, xx = 0;
        for (size_t i = 0; i < 240; ++i)
        {
            xy += in[i] * l[i];
            xx += in[i] * in[i];
        }
        for (size_t i = 0; i < 240; ++i)
        {
            resid += (l[i] - xy / xx * in[i]) * (l[i] - xy / xx * in[i]);
            energy += l[i] * l[i];
        }
    }
    CHECK (10.0 * std::log10 (resid / energy + 1e-300) <= -120.0);
}

TEST_CASE ("An impulse passes without delay", "[engine]")
{
    auto e = makeEngine (48000.0, -12, 12);
    std::vector<double> l (1000, 0.0), r (1000, 0.0);
    l[0] = r[0] = 0.5;
    double* ch[2] = { l.data(), r.data() };
    e.process (ch, 2, (const double* const*) nullptr, 0, 1000);
    CHECK (l[0] == Approx (0.5)); // g = Idle = 0 dB at start-up
    for (size_t i = 1; i < l.size(); ++i)
        CHECK (l[i] == 0.0);
}

TEST_CASE ("Output trim is exact (+6.000 dB for +6)", "[engine]")
{
    for (double out : { -12.0, -6.0, 6.0, 12.0 })
    {
        RiderEngine e;
        Settings s;
        s.outputDb = out;
        s.idleDb = 0.0;
        e.setSettings (s);
        e.prepare (48000.0);
        // silence keeps the rider at Idle (0 dB); a tiny DC probe below every threshold measures the gain
        std::vector<double> l (48000, 1e-6), r (48000, 1e-6);
        double* ch[2] = { l.data(), r.data() };
        e.process (ch, 2, (const double* const*) nullptr, 0, 48000);
        INFO ("Output " << out);
        CHECK (gainToDb (l.back() / 1e-6) == Approx (out).margin (1e-9));
    }
}

TEST_CASE ("Mono equals stereo with L = R (detection on the channel mean)", "[engine]")
{
    auto stereo = makeEngine (48000.0, -6, 6);
    auto mono = makeEngine (48000.0, -6, 6);
    BandNoise noise (48000.0, 21);
    std::vector<double> l (480), r (480), m (480);
    for (int b = 0; b < 2000; ++b)
    {
        const double level = (b / 400) % 2 ? -12.0 : -30.0;
        for (size_t i = 0; i < 480; ++i)
            l[i] = r[i] = m[i] = dbToGain (level) * noise.next();
        double* ch[2] = { l.data(), r.data() };
        stereo.process (ch, 2, (const double* const*) nullptr, 0, 480);
        double* mc[1] = { m.data() };
        mono.process (mc, 1, (const double* const*) nullptr, 0, 480);
        for (size_t i = 0; i < 480; ++i)
            REQUIRE (m[i] == l[i]);
    }
}

TEST_CASE ("Host block size does not change the output", "[engine]")
{
    auto a = makeEngine (44100.0, -12, 12, true);
    auto b = makeEngine (44100.0, -12, 12, true);
    const auto ra = runStereo (a, 4.0, -15.0, 3, 1);
    const auto rb = runStereo (b, 4.0, -15.0, 3, 997);
    CHECK (ra.outL == rb.outL);
    CHECK (ra.outR == rb.outR);
}

TEST_CASE ("Float buffers follow the double path", "[engine]")
{
    auto a = makeEngine (48000.0, -12, 12);
    auto b = makeEngine (48000.0, -12, 12);
    BandNoise noise (48000.0, 4);
    std::vector<double> dl (256), dr (256);
    std::vector<float> fl (256), fr (256);
    double worst = 0.0;
    for (int blk = 0; blk < 2000; ++blk)
    {
        for (size_t i = 0; i < 256; ++i)
        {
            fl[i] = fr[i] = float (0.1 * noise.next());
            dl[i] = dr[i] = double (fl[i]);
        }
        double* dc[2] = { dl.data(), dr.data() };
        float* fc[2] = { fl.data(), fr.data() };
        a.process (dc, 2, (const double* const*) nullptr, 0, 256);
        b.process (fc, 2, (const float* const*) nullptr, 0, 256);
        for (size_t i = 0; i < 256; ++i)
            worst = std::max (worst, std::abs (dl[i] - double (fl[i])));
    }
    CHECK (worst < 1e-6);
}

TEST_CASE ("Section 8: an inactive sidechain is bit-exact", "[engine][sidechain]")
{
    // no sidechain bus vs. a silent sidechain vs. Music Sensitivity Off with a loud sidechain
    BandNoise music (48000.0, 77);
    std::vector<double> silent (size_t (48000 * 6), 0.0), loud (size_t (48000 * 6));
    for (auto& v : loud)
        v = 0.3 * music.next();

    auto ref = makeEngine (48000.0, -12, 12);
    const auto a = runStereo (ref, 6.0, -20.0, 8);

    auto withSilent = makeEngine (48000.0, -12, 12);
    const auto b = runStereo (withSilent, 6.0, -20.0, 8, 512, &silent);
    CHECK (a.outL == b.outL);
    CHECK (a.outR == b.outR);

    RiderEngine off;
    Settings s;
    s.rangeMinDb = -12.0;
    s.rangeMaxDb = 12.0;
    s.musicOff = true;
    off.setSettings (s);
    off.prepare (48000.0);
    const auto c = runStereo (off, 6.0, -20.0, 8, 512, &loud);
    CHECK (a.outL == c.outL);
    CHECK (a.outR == c.outR);
}

TEST_CASE ("Section 8: louder music raises the effective target, limited to +-6 dB", "[engine][sidechain]")
{
    for (double sens : { 0.0, 12.0 })
    {
        RiderEngine e;
        Settings s;
        s.rangeMinDb = -12.0;
        s.rangeMaxDb = 12.0;
        s.musicSensDb = sens;
        e.setSettings (s);
        e.prepare (48000.0);
        BandNoise vocal (48000.0, 1), music (48000.0, 2);
        std::vector<double> l (48), r (48), m (48);
        double maxOffset = 0.0, offsetAfterRise = 0.0;
        for (int b = 0; b < 30000; ++b)
        {
            const double musicDb = b < 15000 ? -30.0 : -18.0; // music gets 12 dB louder after 15 s
            for (size_t i = 0; i < 48; ++i)
            {
                l[i] = r[i] = dbToGain (-21.0) * vocal.next();
                m[i] = dbToGain (musicDb) * music.next();
            }
            double* ch[2] = { l.data(), r.data() };
            const double* sc[1] = { m.data() };
            e.process (ch, 2, sc, 1, 48);
            maxOffset = std::max (maxOffset, std::abs (e.getMusicOffsetDb()));
            if (b == 17000)
                offsetAfterRise = e.getMusicOffsetDb();
        }
        INFO ("Music Sensitivity " << sens);
        CHECK (offsetAfterRise > 2.0);           // T_eff rises with the music
        CHECK (maxOffset <= 6.0 + 1e-12);        // limited to +-6 dB
        CHECK (e.getEffectiveTargetDb() - (-21.0) == Approx (e.getMusicOffsetDb()));
        if (sens == 12.0)
            CHECK (maxOffset == Approx (6.0));   // k = 10^(12/20) * 0.5 ~ 2: the 12 dB step saturates the limit
    }
}

TEST_CASE ("Section 9: Read follows the host value, override holds g and riding resumes from it", "[engine][automation]")
{
    auto e = makeEngine (48000.0, -12, 12);
    e.setAutomationMode (AutomationMode::Read);
    e.setHostRiderDb (7.5);
    runSegments (e, { { -21.0, 1.0 } });
    CHECK (e.getGainDb() == Approx (7.5).margin (1e-6));

    e.setAutomationMode (AutomationMode::Off);
    e.setOverride (true, -10.0);
    runSegments (e, { { -21.0, 0.2 } });
    CHECK (e.getGainDb() == Approx (-10.0));
    e.setOverride (false, -10.0);
    const auto tr = runSegments (e, { { -21.0, 0.05 } });
    CHECK (tr.at (0.005) < -9.0); // resumes from the dragged value, no jump
}

TEST_CASE ("Bypass fades to an exact unity gain", "[engine]")
{
    auto e = makeEngine (48000.0, -12, 12);
    runSegments (e, { { -9.0, 6.0 } }); // rider well below 0 dB
    e.setBypassed (true);
    BandNoise n (48000.0, 3);
    std::vector<double> l (4800), r (4800), in (4800);
    for (size_t i = 0; i < l.size(); ++i)
        l[i] = r[i] = in[i] = 0.3 * n.next();
    double* ch[2] = { l.data(), r.data() };
    e.process (ch, 2, (const double* const*) nullptr, 0, 4800);
    for (size_t i = 2400; i < l.size(); ++i)
        REQUIRE (l[i] == in[i]);
}

TEST_CASE ("Parameter changes are smoothed over about 20 ms (D6)", "[engine]")
{
    RiderEngine e;
    Settings s;
    s.idleDb = 0.0;
    e.setSettings (s);
    e.prepare (48000.0);
    std::vector<double> l (4800, 1e-6), r (4800, 1e-6);
    double* ch[2] = { l.data(), r.data() };
    e.process (ch, 2, (const double* const*) nullptr, 0, 4800);
    s.outputDb = 12.0;
    e.setSettings (s);
    std::fill (l.begin(), l.end(), 1e-6);
    std::fill (r.begin(), r.end(), 1e-6);
    e.process (ch, 2, (const double* const*) nullptr, 0, 4800);
    const double at10ms = gainToDb (l[480] / 1e-6), at30ms = gainToDb (l[1440] / 1e-6);
    CHECK (at10ms > 3.0);
    CHECK (at10ms < 9.0);
    CHECK (at30ms == Approx (12.0).margin (1e-9));
    double maxStep = 0.0;
    for (size_t i = 1; i < 1440; ++i)
        maxStep = std::max (maxStep, std::abs (gainToDb (l[i] / 1e-6) - gainToDb (l[i - 1] / 1e-6)));
    CHECK (maxStep < 0.05); // no zipper steps
}
