// Detector (spec r2 4): calibration, linking, PDR. Normative envelope tables: R2TableTests.cpp.
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>


#include "TestHelpers.h"

using namespace testutil;
using Catch::Approx;

namespace
{
Settings limiterSettings (double thresholdDb = -40.0)
{
    Settings s;
    s.ratioPos = 0.8; // 50:1
    s.compThresholdDb = thresholdDb;
    return s;
}

} // namespace

TEST_CASE ("Detector reading of a steady sine: x_det = peak + 1.277 dB at 2/50 ms", "[detector]")
{
    // seen through the 50:1 curve: the 10.1 values sit exactly on Appendix A rows
    for (double fs : { 44100.0, 48000.0, 96000.0 })
    {
        auto e = makeEngine (limiterSettings(), fs);
        auto in = sine (int (0.3 * fs), fs, 1000.0, -20.0);
        auto out = runStereo (e, in, in);
        CHECK (e.getMeters().compLevelDb == Approx (-20.0 + 1.277).margin (0.03));
    }
}

TEST_CASE ("Stereo linking: max of the channel powers", "[detector][key]")
{
    const double fs = 48000.0;
    const int n = int (0.4 * fs);
    const auto s = limiterSettings();
    auto loud = sine (n, fs, 1000.0, -10.0);
    auto quiet = sine (n, fs, 1000.0, -20.0);
    std::vector<double> silent (size_t (n), 0.0);

    auto gainOf = [&] (std::vector<double> l, std::vector<double> r, bool useLeft) {
        auto e = makeEngine (s, fs);
        const auto ref = useLeft ? l : r;
        auto out = runStereo (e, std::move (l), std::move (r));
        return ratioDb (useLeft ? out.l : out.r, ref, n / 2, n);
    };
    const double both = gainOf (loud, loud, true);
    CHECK (gainOf (loud, silent, true) == Approx (both).margin (1.0e-6));
    CHECK (gainOf (loud, quiet, true) == Approx (both).margin (1.0e-6));
    CHECK (gainOf (quiet, loud, false) == Approx (both).margin (1.0e-6));
    CHECK (gainOf (loud, quiet, false) == Approx (both).margin (1.0e-6)); // the quiet channel gets the same gain
}

TEST_CASE ("PDR: short bursts release fast, then slowly", "[detector][pdr]")
{
    const double fs = 48000.0;
    auto s = limiterSettings (-30.0);
    s.compAttackMs = 1.0;
    s.compReleaseMs = 1000.0;
    s.pdrMs = 22.0;
    const int pre = int (0.3 * fs), burst = int (0.003 * fs), post = int (1.0 * fs);
    auto in = sine (pre + burst + post, fs, 1000.0, [&] (int i) { return (i >= pre && i < pre + burst) ? dbToGain (-10.0) : dbToGain (-40.0); });
    auto e = makeEngine (s, fs);
    auto out = runStereo (e, in, in);
    const auto g = frameGainDb (out.l, in, fs, pre + burst, 900);
    // fast part: most of the reduction is gone within ~20 ms
    CHECK (g[30] > -4.0);
    // slow part: a residual reduction remains for hundreds of ms (release 1000 ms)
    CHECK (g[300] < -0.2);
    // without PDR the whole release is slow
    s.pdrMs = 0.0;
    auto e2 = makeEngine (s, fs);
    auto out2 = runStereo (e2, in, in);
    const auto g2 = frameGainDb (out2.l, in, fs, pre + burst, 900);
    CHECK (g2[30] < g[30] - 2.0);
}
