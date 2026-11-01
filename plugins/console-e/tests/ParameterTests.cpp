// Spec r2 section 2: the 21-point tapers (read from the spec), ratio lookup, labels and defaults.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "TestHelpers.h"

using namespace craft::consolee::test;
using Catch::Approx;

namespace
{
// rows of the section 2 table: control name -> 21 cells
std::map<std::string, std::vector<std::string>> taperTable()
{
    std::map<std::string, std::vector<std::string>> out;
    std::ifstream in (CONSOLE_E_SPEC);
    std::string line;
    bool inTable = false;
    while (std::getline (in, line))
    {
        if (line.rfind ("| Control | 0.00", 0) == 0) { inTable = true; continue; }
        if (! inTable) continue;
        if (line.rfind ("|---", 0) == 0) continue;
        if (line.rfind ("|", 0) != 0) break;
        auto cells = splitCells (line);
        out[cells[0]] = std::vector<std::string> (cells.begin() + 1, cells.end());
    }
    return out;
}
} // namespace

TEST_CASE ("Tapers reproduce the 21 points of the section 2 table", "[parameters]")
{
    const auto table = taperTable();
    REQUIRE (table.size() == 16);
    const std::vector<std::pair<std::string, const param::Taper*>> map {
        { "Input trim (dB)", &param::inputTrim }, { "Output fader (dB)", &param::outputFader },
        { "Comp threshold (dB)", &param::compThreshold }, { "Comp release (s)", &param::compRelease },
        { "Exp threshold (dB)", &param::expThreshold }, { "Exp range (dB)", &param::expRange },
        { "Exp release (s)", &param::expRelease }, { "EQ gains LF/LMF/HMF/HF (dB, label)", &param::bandGain },
        { "LF frequency (Hz, label)", &param::lfFreq }, { "LMF frequency (kHz, label)", &param::lmfFreq },
        { "HMF frequency (kHz, label)", &param::hmfFreq }, { "HF frequency (kHz, label)", &param::hfFreq },
        { "LMF Q / HMF Q", &param::midQ } };
    for (const auto& [name, taper] : map)
    {
        INFO (name);
        const auto& cells = table.at (name);
        REQUIRE (cells.size() == 21);
        for (int k = 0; k < 21; ++k)
        {
            CHECK ((*taper) (k / 20.0) == Approx (std::stod (cells[size_t (k)])).margin (1e-9));
            if (k > 0 && k < 20)
                CHECK ((*taper) ((k + 0.5) / 20.0) == Approx (0.5 * (std::stod (cells[size_t (k)]) + std::stod (cells[size_t (k + 1)]))));
        }
        CHECK (taper->toNorm ((*taper) (0.63)) == Approx (0.63));
    }
    // ratio
    const auto& ratio = table.at ("Comp ratio (:1)");
    for (int k = 0; k < 20; ++k)
        CHECK (param::ratioValue (k / 20.0) == Approx (std::stod (ratio[size_t (k)])));
    CHECK (std::isinf (param::ratioValue (1.0)));
    // filter labels
    const auto& hp = table.at ("HP frequency (Hz, label)");
    const auto& lp = table.at ("LP frequency (kHz, label)");
    CHECK (hp[0] == "Off");
    CHECK (lp[0] == "Off");
    CHECK (param::hpLabel (0.0) == 0.0);
    CHECK (param::lpLabel (0.0) == 0.0);
    for (int k = 1; k < 21; ++k)
    {
        CHECK (param::hpLabel (k / 20.0) == Approx (std::stod (hp[size_t (k)])));
        CHECK (param::lpLabel (k / 20.0) == Approx (std::stod (lp[size_t (k)])));
    }
}

TEST_CASE ("Ratio: in-between values, infinity at the top", "[parameters]")
{
    CHECK (param::ratioValue (0.025) == Approx (1.15));
    CHECK (param::ratioToNorm (INFINITY) == 1.0);
    CHECK (param::ratioToNorm (4.0) == Approx (0.4 + 0.05 * 0.3 / 0.6));
    CHECK (param::ratioInverse (1.0) == 0.0);
}

TEST_CASE ("Defaults display exactly the section 2 values", "[parameters]")
{
    namespace d = param::defaults;
    CHECK (param::inputTrim (d::inputTrim) == Approx (0.0).margin (1e-9));
    CHECK (param::outputFader (d::outputFader) == Approx (0.0).margin (1e-9));
    CHECK (param::compThreshold (d::compThreshold) == Approx (0.0).margin (1e-9));
    CHECK (param::ratioValue (d::compRatio) == Approx (1.0));
    CHECK (param::compRelease (d::compRelease) == Approx (0.40));
    CHECK (param::expThreshold (d::expThreshold) == Approx (-30.0));
    CHECK (param::expRange (d::expRange) == Approx (0.0));
    CHECK (param::expRelease (d::expRelease) == Approx (0.20).margin (0.005));
    CHECK (std::round (param::lfFreq (d::lfFreq)) == 60.0);
    CHECK (std::round (param::lmfFreq (d::lmfFreq) * 100.0) == 50.0);
    CHECK (param::hmfFreq (d::hmfFreq) == Approx (3.50));
    CHECK (param::hfFreq (d::hfFreq) == Approx (8.00));
    CHECK (param::midQ (d::lmfQ) == Approx (2.50));
    CHECK (param::midQ (d::hmfQ) == Approx (1.50));
    // the positions are the ones the spec lists
    CHECK (d::lfFreq == 0.23);
    CHECK (d::lmfFreq == 0.26);
    CHECK (d::expRelease == 0.35);
}
