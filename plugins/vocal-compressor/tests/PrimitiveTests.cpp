#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <deque>
#include <random>

#include "dsp/Primitives.h"

using namespace craft::vocal;

TEST_CASE ("SlidingMax equals a brute-force window maximum", "[primitives]")
{
    std::mt19937 rng (3);
    std::uniform_real_distribution<double> dist (0.0, 1.0);
    for (int len : { 1, 2, 65, 129 })
    {
        SlidingMax m;
        m.prepare (len);
        m.reset (0.25);
        std::deque<double> window (std::size_t (len), 0.25);
        for (int i = 0; i < 5000; ++i)
        {
            const double v = (i % 300 < 100) ? dist (rng) : 0.1 * dist (rng);
            window.push_back (v);
            window.pop_front();
            const double expected = *std::max_element (window.begin(), window.end());
            REQUIRE (m.push (v) == expected);
        }
    }
}

TEST_CASE ("Boxcar is an exact moving average", "[primitives]")
{
    Boxcar b;
    b.prepare (38);
    b.reset (1.0);
    for (int i = 0; i < 100; ++i)
        REQUIRE (b.push (1.0) == 1.0);
    std::vector<double> hist (38, 1.0);
    std::mt19937 rng (5);
    std::uniform_real_distribution<double> dist (0.0, 1.0);
    for (int i = 0; i < 1000; ++i)
    {
        const double v = dist (rng);
        hist.erase (hist.begin());
        hist.push_back (v);
        double s = 0.0;
        for (double x : hist)
            s += x;
        REQUIRE_THAT (b.push (v), Catch::Matchers::WithinAbs (s / 38.0, 1e-12));
    }
}

TEST_CASE ("Look-ahead configuration per sample rate (4.1, 4.5, D7)", "[primitives]")
{
    const auto c48 = LookaheadConfig::forSampleRate (48000.0);
    CHECK ((c48.lookahead == 64 && c48.boxA == 38 && c48.boxB == 27));
    const auto c44 = LookaheadConfig::forSampleRate (44100.0);
    CHECK ((c44.lookahead == 64 && c44.boxA == 38 && c44.boxB == 27));
    const auto c96 = LookaheadConfig::forSampleRate (96000.0);
    CHECK ((c96.lookahead == 128 && c96.boxA == 75 && c96.boxB == 54));
    const auto c192 = LookaheadConfig::forSampleRate (192000.0);
    CHECK ((c192.lookahead == 256 && c192.boxA == 150 && c192.boxB == 108));
}
