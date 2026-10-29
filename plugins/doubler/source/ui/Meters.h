// Craft Audio doubler - output section: master fader, stereo meters, peak counters, clip indicator
// (spec r1 section 12; display only).
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "Controls.h"

namespace craft::doubler::ui
{

class PeakBox : public juce::Component, public juce::SettableTooltipClient
{
public:
    PeakBox() { setTooltip ("Peak level since the last reset - click to reset"); }
    void setPeak (float db);
    void reset();
    float getPeak() const noexcept { return peakDb; }
    std::function<void()> onReset;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override
    {
        reset();
        if (onReset)
            onReset();
    }

private:
    float peakDb = -200.0f;
};

class OutputSection : public juce::Component
{
public:
    OutputSection (juce::AudioProcessorValueTreeState&, const juce::String& masterId);

    void pushPeaks (float left, float right);
    void refresh() { fader.refresh(); }

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    juce::Rectangle<int> barArea (int ch) const;
    juce::Rectangle<int> clipArea() const;
    void resetAll();

    Fader fader;
    float levelDb[2] = { -100.0f, -100.0f };
    PeakBox peak[2];
    bool clipped = false;
};

} // namespace craft::doubler::ui
