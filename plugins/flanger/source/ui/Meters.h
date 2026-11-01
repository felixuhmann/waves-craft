// Craft Flanger - output peak meters with clip indicators and numeric peak hold (spec 12; display only).
#pragma once

#include <functional>

#include <juce_gui_basics/juce_gui_basics.h>

namespace craft::flanger::ui
{

class PeakHoldBox : public juce::Component, public juce::SettableTooltipClient
{
public:
    void setPeak (float db);
    void reset();
    float getPeak() const noexcept { return peakDb; }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

    std::function<void()> onReset;

private:
    float peakDb = -200.0f;
};

class MeterPanel : public juce::Component
{
public:
    explicit MeterPanel (int numChannels);

    // linear peaks since the previous call
    void pushPeaks (float p1, float p2);

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override; // click on a clip LED resets it

private:
    juce::Rectangle<int> barArea (int ch) const;
    juce::Rectangle<float> clipArea (int ch) const;

    const int channels;
    float levelDb[2] = { -100.0f, -100.0f };
    bool clipped[2] = { false, false };
    PeakHoldBox hold[2];
};

} // namespace craft::flanger::ui
