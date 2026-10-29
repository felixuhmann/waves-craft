// Craft Audio stereo imager - output meters with peak hold (spec r1 section 12; display only).
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "Controls.h"

namespace craft::ui
{

class PeakHoldBox : public juce::Component, public juce::SettableTooltipClient
{
public:
    void setPeak (float db);
    void reset();
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { reset(); }

private:
    float peakDb = -200.0f;
};

class MeterPanel : public juce::Component
{
public:
    MeterPanel();

    // linear peaks for the two channels since the previous call
    void pushPeaks (float p1, float p2);
    void setMode (bool msMode);
    bool getMode() const noexcept { return modeSwitch.getToggleState(); }

    std::function<void (bool)> onModeChange;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::Rectangle<int> barArea (int ch) const;
    float levelDb[2] = { -100.0f, -100.0f };
    PeakHoldBox hold[2];
    SegmentSwitch modeSwitch { "L-R", "M-S" };
};

} // namespace craft::ui
