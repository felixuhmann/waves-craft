// Spec r1 sections 5 and 9: shuffler magnitude/phase, DC gains, corner independence of fs.
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "TestHelpers.h"

using namespace testutil;
using Catch::Matchers::WithinAbs;

namespace
{
struct Row
{
    double shuffle, freq, f, mDb, mDeg, sDb, sDeg;
};

const Row table[] = {
    { 3.0, 650, 5, -2.553, -0.9, 6.989, -1.0 },
    { 3.0, 650, 200, -2.330, -34.5, 6.337, -36.3 },
    { 3.0, 650, 650, -1.276, -90.0, 3.495, -90.0 },
    { 3.0, 650, 1500, -0.405, -133.0, 1.166, -131.0 },
    { 3.0, 650, 4000, -0.063, -161.8, 0.188, -160.7 },
    { 3.0, 650, 20000, 0.000, -179.2, 0.000, -179.2 },
    { 3.0, 350, 200, -1.921, -59.8, 5.201, -61.4 },
    { 3.0, 1400, 1400, -1.276, -90.0, 3.495, -90.0 },
    { 3.0, 1400, 4000, -0.269, -142.0, 0.786, -140.1 },
};

// Table values are rounded to 3 decimals (dB) and 1 decimal (degrees).
constexpr double dbTol = 0.0015, degTol = 0.06;
} // namespace

TEST_CASE ("Section 9 shuffler table at 44.1 kHz", "[shuffler][spec9]")
{
    for (const auto& r : table)
    {
        INFO ("shuffle " << r.shuffle << " freq " << r.freq << " f " << r.f);
        Settings s;
        s.shuffle = r.shuffle;
        s.frequencyHz = r.freq;
        const auto h = msResponseAt (Variant::Full, s, 44100.0, r.f);
        CHECK_THAT (magDb (h.m), WithinAbs (r.mDb, dbTol));
        CHECK_THAT (phaseDeg (h.m), WithinAbs (r.mDeg, degTol));
        CHECK_THAT (magDb (h.s), WithinAbs (r.sDb, dbTol));
        CHECK_THAT (phaseDeg (h.s), WithinAbs (r.sDeg, degTol));
    }
}

TEST_CASE ("Shuffler table at 48 and 96 kHz (same frequency ratio, small bilinear warping)", "[shuffler][samplerate]")
{
    for (double fs : { 48000.0, 96000.0 })
        for (const auto& r : table)
        {
            if (r.f > 1500.0)
                continue; // warping differs noticeably only well above the corner
            INFO ("fs " << fs << " freq " << r.freq << " f " << r.f);
            Settings s;
            s.shuffle = r.shuffle;
            s.frequencyHz = r.freq;
            const auto h = msResponseAt (Variant::Full, s, fs, r.f);
            CHECK_THAT (magDb (h.m), WithinAbs (r.mDb, 0.01));
            CHECK_THAT (phaseDeg (h.m), WithinAbs (r.mDeg, 0.25));
            CHECK_THAT (magDb (h.s), WithinAbs (r.sDb, 0.01));
            CHECK_THAT (phaseDeg (h.s), WithinAbs (r.sDeg, 0.25));
        }
}

TEST_CASE ("Shuffler DC gains", "[shuffler][spec9]")
{
    struct Dc
    {
        double shuffle, trim, mDb, sDb;
    };
    const Dc rows[] = {
        { 1.5, 0, -1.413, 2.109 }, { 2.0, 0, -2.041, 3.979 }, { 2.5, 0, -2.366, 5.593 }, { 3.0, 0, -2.553, 6.990 },
        { 3.0, 3, 0.447, 9.990 },  { 1.0, 3, 3.000, 3.000 },  { 1.0, -3, -3.000, -3.000 },
    };
    for (double fs : { 44100.0, 48000.0, 96000.0 })
        for (const auto& r : rows)
        {
            INFO ("fs " << fs << " shuffle " << r.shuffle << " trim " << r.trim);
            Settings s;
            s.shuffle = r.shuffle;
            s.bassTrimDb = r.trim;
            const auto h = msResponseAt (Variant::Full, s, fs, 0.0);
            CHECK_THAT (magDb (h.m), WithinAbs (r.mDb, dbTol));
            CHECK_THAT (magDb (h.s), WithinAbs (r.sDb, dbTol));
            CHECK_THAT (phaseDeg (h.m), WithinAbs (0.0, 1e-6));
            CHECK_THAT (phaseDeg (h.s), WithinAbs (0.0, 1e-6));
        }
}

TEST_CASE ("Bass Trim alone: same phase response as section 5", "[shuffler]")
{
    Settings s;
    s.bassTrimDb = 3.0;
    const auto h = msResponseAt (Variant::Full, s, 48000.0, s.frequencyHz);
    CHECK_THAT (phaseDeg (h.m), WithinAbs (-90.0, 1e-4));
    CHECK_THAT (phaseDeg (h.s), WithinAbs (-90.0, 1e-4));
    CHECK_THAT (magDb (h.m), WithinAbs (1.5, 1e-4)); // |H| = sqrt(G)
    const auto hf = msResponseAt (Variant::Full, s, 48000.0, 23999.0);
    CHECK_THAT (magDb (hf.m), WithinAbs (0.0, 1e-3));
}

TEST_CASE ("Corner at Frequency for any G and sample rate: -90 deg and |H| = sqrt(G)", "[shuffler][samplerate]")
{
    for (double fs : { 44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0 })
        for (double shuffle : { 1.0, 1.7, 3.0 })
            for (double trim : { -3.0, 0.0, 2.4 })
                for (double freq : { 350.0, 650.0, 1400.0 })
                {
                    if (! shufflerActive (shuffle, trim))
                        continue;
                    INFO ("fs " << fs << " shuffle " << shuffle << " trim " << trim << " freq " << freq);
                    Settings s;
                    s.shuffle = shuffle;
                    s.bassTrimDb = trim;
                    s.frequencyHz = freq;
                    double gM, gS;
                    shufflerGains (shuffle, trim, gM, gS);
                    const auto h = msResponseAt (Variant::Full, s, fs, freq, 1 << 15);
                    CHECK_THAT (phaseDeg (h.m), WithinAbs (-90.0, 1e-4));
                    CHECK_THAT (phaseDeg (h.s), WithinAbs (-90.0, 1e-4));
                    CHECK_THAT (std::abs (h.m), WithinAbs (std::sqrt (gM), 1e-6));
                    CHECK_THAT (std::abs (h.s), WithinAbs (std::sqrt (gS), 1e-6));
                }
}

TEST_CASE ("No extra HF loss at high sample rates (section 5.1)", "[shuffler][samplerate]")
{
    // At f >> Frequency the section tends to unity magnitude; compare with the analytic
    // bilinear model at 96 and 192 kHz up to 20 kHz.
    for (double fs : { 96000.0, 192000.0 })
    {
        Settings s;
        s.shuffle = 3.0;
        s.frequencyHz = 650;
        for (double f : { 5500.0, 8000.0, 12000.0, 16000.0, 20000.0 })
        {
            double gM, gS;
            shufflerGains (s.shuffle, 0.0, gM, gS);
            const auto c = shufflerSectionCoeffs (gM, s.frequencyHz, fs);
            const auto z = std::polar (1.0, -2.0 * pi * f / fs);
            const auto expected = (c.b0 + c.b1 * z) / (1.0 + c.a1 * z);
            const auto h = msResponseAt (Variant::Full, s, fs, f, 1 << 15);
            INFO ("fs " << fs << " f " << f);
            CHECK (std::abs (h.m - expected) < 1e-9);
            CHECK (magDb (h.m) > -0.05);
            CHECK (magDb (h.m) < 0.0);
        }
    }
}

TEST_CASE ("Shuffler gains formula", "[shuffler]")
{
    double gM, gS;
    shufflerGains (1.0, 0.0, gM, gS);
    CHECK (gM == 1.0);
    CHECK (gS == 1.0);
    shufflerGains (3.0, 0.0, gM, gS);
    CHECK_THAT (gS, WithinAbs (std::sqrt (5.0), 1e-14));
    CHECK (shufflerActive (1.0, 0.0) == false);
    CHECK (shufflerActive (1.1, 0.0) == true);
    CHECK (shufflerActive (1.0, -0.1) == true);
}
