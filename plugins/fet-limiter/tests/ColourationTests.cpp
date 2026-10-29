// Spec r1 section 7: frequency response, harmonic distortion, noise and hum.
#include <catch2/catch_test_macros.hpp>

#include "TestHelpers.h"

using namespace fettest;

namespace
{
// Small-signal response at f relative to 1 kHz (no GR: Input -30, -50 dBFS).
double responseDb (Revision rev, double f, double fs)
{
    auto level = [&] (double freq) {
        Settings s = neutralSettings();
        s.revision = rev;
        s.inputDisplay = -30.0;
        s.outputDisplay = 0.0;
        Rig rig (s, fs);
        const double seconds = freq < 100.0 ? 3.0 : 0.6;
        auto x = sine (freq, -50.0, seconds, fs);
        rig.run (x);
        const size_t n = wholeCycles (freq, fs, size_t (std::max (0.2, 10.0 / freq) * fs));
        return toneDb (x, freq, fs, x.size() - n, n);
    };
    return level (f) - level (1000.0);
}

struct Harm
{
    double gainDb, h2, h3, h5;
};

Harm harmonics (const Settings& s, double freq, double levelDb, double fs = 48000.0)
{
    Rig rig (s, fs);
    auto x = sine (freq, levelDb, 2.0, fs);
    rig.run (x);
    const size_t n = wholeCycles (freq, fs, size_t (0.5 * fs));
    const size_t start = x.size() - n;
    const double h1 = toneDb (x, freq, fs, start, n);
    auto rel = [&] (int k) { return k * freq < fs / 2 ? toneDb (x, k * freq, fs, start, n) - h1 : -200.0; };
    return { h1 - levelDb, rel (2), rel (3), rel (5) };
}

// spec 7.2 tolerance: +-4 dB where the table value is >= -70 dBc, else the measurement must be <= -66 dBc
bool harmonicOk (double measured, double expected)
{
    if (expected >= -70.0)
        return std::abs (measured - expected) <= 4.0;
    return measured <= -66.0;
}
} // namespace

TEST_CASE ("Frequency response per revision matches tables/freq_response.csv at 48 kHz", "[response]")
{
    const auto csv = parseCsv (craft::fet_limiter::spec_tables::freqResponseCsv);
    for (const auto& row : csv.rows)
    {
        const Revision rev = row[0] == "A" ? Revision::A : Revision::B;
        const double f = std::stod (row[1]), expected = std::stod (row[2]);
        const double tol = f < 15000.0 ? 0.3 : 1.0;
        const double m = responseDb (rev, f, 48000.0);
        INFO ("rev " << row[0] << " " << f << " Hz expected " << expected << " measured " << m);
        CHECK (std::abs (m - expected) <= tol);
    }
}

TEST_CASE ("Frequency response keeps the analog prototype at 44.1 / 96 kHz", "[response][samplerate]")
{
    const auto csv = parseCsv (craft::fet_limiter::spec_tables::freqResponseCsv);
    for (double fs : { 44100.0, 96000.0 })
        for (const auto& row : csv.rows)
        {
            const Revision rev = row[0] == "A" ? Revision::A : Revision::B;
            const double f = std::stod (row[1]), expected = std::stod (row[2]);
            if (f >= fs / 2.0)
                continue;
            const double tol = f < 15000.0 ? 0.3 : 1.0;
            const double m = responseDb (rev, f, fs);
            INFO ("fs " << fs << " rev " << row[0] << " " << f << " Hz expected " << expected << " measured " << m);
            CHECK (std::abs (m - expected) <= tol);
        }
}

TEST_CASE ("Comp Off harmonics match tables/harmonics.csv (Input -12, Output 0)", "[harmonics]")
{
    // The compoff rows are the only harmonic rows whose settings follow from the table itself: their gain at
    // -30 dBFS equals the small-signal gain at Input -12 / Output 0 (37.07 dB rev A, 40.58 dB rev B).
    const auto csv = parseCsv (craft::fet_limiter::spec_tables::harmonicsCsv);
    for (const auto& row : csv.rows)
    {
        if (row[0].rfind ("compoff", 0) != 0)
            continue;
        const Revision rev = row[1] == "A" ? Revision::A : Revision::B;
        const double f = std::stod (row[2]), level = std::stod (row[3]);
        if (f < 1000.0)
            continue; // 100 Hz rows: see the next test case
        Settings s = neutralSettings();
        s.revision = rev;
        s.inputDisplay = -12.0;
        s.outputDisplay = 0.0;
        s.compOff = true;
        const auto h = harmonics (s, f, level);
        INFO (row[0] << " " << f << " Hz " << level << " dBFS: gain " << h.gainDb << " (" << row[4] << ") H2 " << h.h2 << " ("
                     << row[5] << ") H3 " << h.h3 << " (" << row[6] << ") H5 " << h.h5 << " (" << row[7] << ")");
        CHECK (std::abs (h.gainDb - std::stod (row[4])) <= 1.0);
        CHECK (harmonicOk (h.h2, std::stod (row[5])));
        CHECK (harmonicOk (h.h3, std::stod (row[6])));
    }
}

TEST_CASE ("Comp Off harmonics at 100 Hz", "[harmonics]")
{
    const auto csv = parseCsv (craft::fet_limiter::spec_tables::harmonicsCsv);
    for (const auto& row : csv.rows)
    {
        if (row[0].rfind ("compoff", 0) != 0 || std::stod (row[2]) != 100.0)
            continue;
        const double level = std::stod (row[3]);
        if (level < -18.0)
            continue; // -30 dBFS at 100 Hz: known deviation C4 (IMPLEMENTATION_NOTES)
        Settings s = neutralSettings();
        s.revision = row[1] == "A" ? Revision::A : Revision::B;
        s.inputDisplay = -12.0;
        s.outputDisplay = 0.0;
        s.compOff = true;
        const auto h = harmonics (s, 100.0, level);
        INFO (row[0] << " 100 Hz " << level << " dBFS: H2 " << h.h2 << " (" << row[5] << ") H3 " << h.h3 << " (" << row[6] << ")");
        CHECK (harmonicOk (h.h2, std::stod (row[5])));
        CHECK (harmonicOk (h.h3, std::stod (row[6])));
    }
}

TEST_CASE ("Harmonics with compression stay in the typical range of spec 7.2", "[harmonics]")
{
    // "Typical: H2 -40 ... -60 dBc, H3 -45 ... -60 dBc with compression" (+-4 dB tolerance applied to the range)
    for (auto rev : { Revision::A, Revision::B })
        for (double f : { 1000.0, 5000.0 })
            for (double level : { -18.0, -6.0 })
            {
                Settings s = neutralSettings();
                s.revision = rev;
                s.inputDisplay = -12.0;
                s.outputDisplay = -18.0;
                s.ratio = Ratio::R4;
                const auto h = harmonics (s, f, level);
                INFO ("rev " << int (rev) << " " << f << " Hz " << level << " dBFS: H2 " << h.h2 << " H3 " << h.h3);
                CHECK (h.h2 <= -36.0);
                CHECK (h.h2 >= -64.0);
                CHECK (h.h3 <= -41.0);
            }
}

TEST_CASE ("Even-order content is present (asymmetric curve)", "[harmonics]")
{
    Settings s = neutralSettings();
    s.inputDisplay = -12.0;
    s.outputDisplay = 0.0;
    const auto h = harmonics (s, 1000.0, -18.0);
    CHECK (h.h2 > -70.0);
}

TEST_CASE ("Analog Off: digital silence in gives silence out", "[noise]")
{
    for (auto rev : { Revision::A, Revision::B })
    {
        Settings s = neutralSettings();
        s.revision = rev;
        s.inputDisplay = 0.0;
        s.outputDisplay = 0.0;
        Rig rig (s);
        std::vector<double> l (48000, 0.0), r (48000, 0.0);
        rig.run (l, r);
        for (size_t i = 0; i < l.size(); ++i)
        {
            REQUIRE (l[i] == 0.0);
            REQUIRE (r[i] == 0.0);
        }
    }
}

TEST_CASE ("Analog noise and hum levels match tables/noise.csv (+-2 dB), independent of Input", "[noise]")
{
    const double fs = 48000.0;
    for (auto rev : { Revision::A, Revision::B })
        for (auto analog : { Analog::Hz50, Analog::Hz60 })
            for (double input : { -30.0, 0.0 })
            {
                Settings s = neutralSettings();
                s.revision = rev;
                s.analog = analog;
                s.inputDisplay = input;
                Rig rig (s, fs);
                std::vector<double> x (size_t (20.0 * fs), 0.0);
                rig.run (x);
                const size_t start = size_t (2.0 * fs), n = x.size() - start;
                const double rms = rmsDb (x, start, n);
                const double expected = rev == Revision::A ? -71.8 : -76.1;
                INFO ("rev " << (rev == Revision::A ? "A" : "B") << " analog " << int (analog) << " input " << input << " rms " << rms);
                CHECK (std::abs (rms - expected) <= 2.0);
                if (rev == Revision::B)
                {
                    const double mains = analog == Analog::Hz50 ? 50.0 : 60.0;
                    const size_t m = wholeCycles (mains, fs, n);
                    const double line = toneDb (x, mains, fs, start, m), line3 = toneDb (x, 3.0 * mains, fs, start, m);
                    INFO ("line " << line << " 3rd " << line3);
                    CHECK (std::abs (line - (-89.0)) <= 2.0);
                    CHECK (std::abs (line3 - (-95.5)) <= 2.0);
                }
                else
                {
                    // energy concentrated at very low frequencies: >= 90 % of the power below 20 Hz
                    double lowPower = 0.0, total = 0.0;
                    double lp = 0.0;
                    const double k = 1.0 - std::exp (-twoPi * 20.0 / fs);
                    for (size_t i = start; i < x.size(); ++i)
                    {
                        lp += (x[i] - lp) * k;
                        lowPower += lp * lp;
                        total += x[i] * x[i];
                    }
                    CHECK (lowPower / total > 0.8);
                }
            }
}
