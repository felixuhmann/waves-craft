// Building blocks of the DSP core against brute-force definitions.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <random>
#include <vector>

#include "dsp/Primitives.h"

using namespace craft::limiter;
using Catch::Approx;

TEST_CASE ("SlidingMin equals the brute-force window minimum", "[primitives]")
{
    std::mt19937 rng (1);
    std::uniform_real_distribution<double> u (0.0, 1.0);
    for (int len : { 1, 2, 5, 64, 128 })
    {
        SlidingMin m;
        m.prepare (len);
        m.reset (1.0);
        std::vector<double> history;
        for (int i = 0; i < 5000; ++i)
        {
            // runs of equal values and random values
            const double v = (i / 37) % 3 == 0 ? 0.5 : u (rng);
            history.push_back (v);
            double expected = 1.0;
            for (int k = 0; k < len; ++k)
            {
                const int idx = int (history.size()) - 1 - k;
                expected = std::min (expected, idx >= 0 ? history[std::size_t (idx)] : 1.0);
            }
            REQUIRE (m.push (v) == expected);
        }
    }
}

TEST_CASE ("Boxcar equals the brute-force moving average and is exact for constant input", "[primitives]")
{
    std::mt19937 rng (2);
    std::uniform_real_distribution<double> u (0.0, 1.0);
    for (int len : { 1, 27, 38, 54, 75, 107, 150 })
    {
        Boxcar b;
        b.prepare (len);
        b.reset (1.0);
        std::vector<double> history;
        for (int i = 0; i < 3000; ++i)
        {
            const double v = u (rng);
            history.push_back (v);
            double s = 0.0;
            for (int k = 0; k < len; ++k)
            {
                const int idx = int (history.size()) - 1 - k;
                s += idx >= 0 ? history[std::size_t (idx)] : 1.0;
            }
            REQUIRE (b.push (v) == Approx (s / len).margin (1e-14));
        }
        // after random input, a window of exact ones returns exactly 1 once a full cycle has been recomputed
        for (int i = 0; i < 2 * len; ++i)
            b.push (1.0);
        for (int i = 0; i < 3 * len; ++i)
            REQUIRE (b.push (1.0) == 1.0);
    }
}

TEST_CASE ("DelayLine delays by exactly its length", "[primitives]")
{
    DelayLine<double> d;
    d.prepare (64);
    d.reset (0.0);
    for (int i = 0; i < 300; ++i)
        REQUIRE (d.push (double (i + 1)) == (i >= 64 ? double (i + 1 - 64) : 0.0));
}
