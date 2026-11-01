// Craft Console E - display-only meters (spec r1 section 13).
#pragma once

#include <vector>

#include <juce_gui_basics/juce_gui_basics.h>

namespace craft::consolee::ui
{

// Horizontal reduction meter: the bar grows from the right edge (0 dB) toward the left.
class ReductionMeter : public juce::Component
{
public:
    ReductionMeter (juce::String caption, float rangeDb, std::initializer_list<float> ticks);
    void setReduction (float reductionDb); // >= 0 dB of gain reduction
    void paint (juce::Graphics&) override;

private:
    juce::String caption;
    float rangeDb, value = 0, hold = 0;
    int holdCounter = 0;
    std::vector<float> ticks;
};

// Vertical stereo level meter, -60 ... +6 dBFS.
class LevelMeter : public juce::Component
{
public:
    void setLevels (float leftLinear, float rightLinear, bool mono);
    void paint (juce::Graphics&) override;

private:
    float barDb[2] { -100, -100 }, peakDb[2] { -100, -100 };
    int peakHold[2] {};
    bool mono = false;
};

} // namespace craft::consolee::ui
