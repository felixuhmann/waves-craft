// Craft Dynamics - meters (spec 12; display only).
#pragma once

#include <functional>

#include <juce_gui_basics/juce_gui_basics.h>

namespace craft::dynamics::ui
{

// Numeric peak hold; a click resets it.
class PeakHoldBox : public juce::Component, public juce::SettableTooltipClient
{
public:
    void setPeak (float db);
    void reset();
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { reset(); }
    void setWarnAboveZero (bool w) { warnAboveZero = w; }

private:
    float peakDb = -200.0f;
    bool warnAboveZero = true;
};

// Per-module meters: gain change (reduction downward, increase upward) and detector level.
class ModuleMeters : public juce::Component
{
public:
    explicit ModuleMeters (juce::Colour colour);
    void setValues (float gainDb, float levelDb);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::Colour colour;
    float gain = 0.0f, level = -120.0f;
    PeakHoldBox gainHold;
};

// Input and output peak meters with clip LEDs and peak hold boxes.
class IoMeters : public juce::Component
{
public:
    explicit IoMeters (int numChannels);
    void push (const float* inPeaks, const float* outPeaks);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    juce::Rectangle<int> barArea (int index) const; // 0..3: in L, in R, out L, out R
    int numBars() const { return 2 * channels; }

    const int channels;
    float levelDb[4] = { -100.0f, -100.0f, -100.0f, -100.0f };
    bool clipped[4] = {};
    PeakHoldBox hold[4];
};

} // namespace craft::dynamics::ui
