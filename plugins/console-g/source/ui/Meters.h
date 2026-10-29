// Craft Console G - meters (display only): gain reduction and input / output level.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace craft::consoleg::ui
{

// Vertical bar meter. Level meter: bars grow from the bottom (-60 ... +6 dBFS). Reduction meter: hangs from the
// top (0 ... -30 dB). Ballistics: instant rise, ~20 dB/s fall, peak marker held 1.5 s.
class BarMeter : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit BarMeter (bool reduction);

    void setChannels (int n) { channels = juce::jlimit (1, 2, n); }
    void push (const float* dbValues, float dtSeconds); // dB per channel (reduction meter: one value <= 0)
    void paint (juce::Graphics&) override;

private:
    float fraction (float db) const noexcept;

    const bool reduction;
    int channels = 1;
    float shown[2] { -100.0f, -100.0f }, peak[2] { -100.0f, -100.0f }, peakAge[2] {};
};

} // namespace craft::consoleg::ui
