// Spec r2 section 6 / 7: tables T2 (band-noise equilibria, +-0.6 dB), T3 (step trajectories) and T7 (large-drop
// trajectories; both +-1.5 dB or reached within +-30 % of the stated time), at 44.1, 48 and 96 kHz.
#include <catch2/catch_test_macros.hpp>

#include "Tables.h"

using namespace craft::levelrider::test;

namespace
{
void report (const std::vector<CellResult>& cells)
{
    for (const auto& c : cells)
    {
        INFO (c.label << ": " << c.value << " vs " << c.ref);
        CHECK (c.pass);
    }
}

void checkT2T3 (double fs, size_t row)
{
    const auto tr = runT2 (fs, row, nullptr);
    std::vector<CellResult> cells;
    evalT2 (tr, row, cells);
    if (row == 0 || row == 3)
        evalT3 (tr, row == 3, cells);
    INFO ("fs " << fs);
    report (cells);
}
} // namespace

TEST_CASE ("T2/T3: +-12 Slow", "[T2][T3][dynamics]")
{
    for (double fs : { 44100.0, 48000.0, 96000.0 })
        checkT2T3 (fs, 0);
}
TEST_CASE ("T2: default +-6 Slow", "[T2][dynamics]")
{
    for (double fs : { 44100.0, 48000.0, 96000.0 })
        checkT2T3 (fs, 1);
}
TEST_CASE ("T2: +-3 Slow", "[T2][dynamics]")
{
    for (double fs : { 44100.0, 48000.0, 96000.0 })
        checkT2T3 (fs, 2);
}
TEST_CASE ("T2/T3: +-12 Fast", "[T2][T3][dynamics]")
{
    for (double fs : { 44100.0, 48000.0, 96000.0 })
        checkT2T3 (fs, 3);
}
TEST_CASE ("T2: +-6 Fast", "[T2][dynamics]")
{
    for (double fs : { 44100.0, 48000.0, 96000.0 })
        checkT2T3 (fs, 4);
}
TEST_CASE ("T2: +-12, Target -12", "[T2][dynamics]")
{
    for (double fs : { 44100.0, 48000.0, 96000.0 })
        checkT2T3 (fs, 5);
}
TEST_CASE ("T2: +-12, Target -33", "[T2][dynamics]")
{
    for (double fs : { 44100.0, 48000.0, 96000.0 })
        checkT2T3 (fs, 6);
}

TEST_CASE ("T7: trajectories after 6 - 36 dB drops, Slow", "[T7][dynamics]")
{
    for (double fs : { 44100.0, 48000.0, 96000.0 })
    {
        std::vector<CellResult> cells;
        evalT7 (fs, false, nullptr, cells);
        INFO ("fs " << fs);
        report (cells);
    }
}

TEST_CASE ("T7: trajectories after 6 - 36 dB drops, Fast", "[T7][dynamics]")
{
    for (double fs : { 44100.0, 48000.0, 96000.0 })
    {
        std::vector<CellResult> cells;
        evalT7 (fs, true, nullptr, cells);
        INFO ("fs " << fs);
        report (cells);
    }
}
