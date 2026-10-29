// Craft Multiband - parameter-bound controls (spec r1 section 8).
#pragma once

#include <array>
#include <functional>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "dsp/Parameters.h"

namespace craft::multiband::ui
{

// Numeric value box bound to one parameter.
//  - single click: type a value       - vertical drag: change the value (Shift = fine)
//  - double-click or Alt-click: reset to default
class ValueBox : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    ValueBox (juce::RangedAudioParameter& param, juce::String paramId, juce::String unit = {});

    void refresh();
    void setAccent (juce::Colour c) { accent = c; repaint(); }

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void resetToDefault();
    juce::String currentText() const;

    juce::RangedAudioParameter& param;
    const juce::String paramId, unit;
    juce::Colour accent;
    juce::Label label;
    float dragStart = 0.0f;
    bool dragging = false, gesture = false, suppressClick = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ValueBox)
};

// Master offset box: a vertical drag moves the same parameter of all four bands by the same amount in the
// host-normalised domain (dB for Threshold / Gain / Range, a common ratio for the logarithmic Attack / Release),
// limited so that no band hits its end stop, so the differences between bands are preserved.
class OffsetBox : public juce::Component, public juce::SettableTooltipClient
{
public:
    OffsetBox (std::array<juce::RangedAudioParameter*, 4> params, std::function<juce::String (double)> describeDelta);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    std::array<juce::RangedAudioParameter*, 4> params;
    std::array<float, 4> start {};
    std::function<juce::String (double)> describe;
    juce::String text;
    bool dragging = false;
};

// Pill-shaped toggle bound to a bool parameter.
class PillToggle : public juce::Button
{
public:
    PillToggle (const juce::String& text, juce::Colour onColour);
    void paintButton (juce::Graphics&, bool over, bool down) override;

private:
    juce::Colour onColour;
};

// Two-state selector drawn as a segmented switch (Auto / Manual, Electro / Opto) bound to a bool parameter.
class SegmentSwitch : public juce::Component
{
public:
    SegmentSwitch (juce::RangedAudioParameter& param, juce::String offText, juce::String onText);
    void refresh();
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    juce::RangedAudioParameter& param;
    juce::String offText, onText;
    bool on = false;
};

// Band input meter (0 ... -80 dBFS) with a draggable Threshold line.
class ThresholdMeter : public juce::Component
{
public:
    ThresholdMeter (juce::RangedAudioParameter& threshold, juce::Colour colour);

    void pushLevel (float linearPeak);
    void refresh() { repaint(); }

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> barArea() const;
    float yForDb (float db) const;
    float dbForY (float y) const;

    juce::RangedAudioParameter& threshold;
    juce::Colour colour;
    float levelDb = -120.0f, holdDb = -120.0f;
    int holdTicks = 0;
    bool dragging = false;
};

// Output section: Output Gain fader beside the output level meters.
class OutputSection : public juce::Component
{
public:
    OutputSection (juce::AudioProcessorValueTreeState& state, const juce::String& paramId);

    void setChannels (int n);
    void pushLevels (float l, float r);
    void refresh() { box.refresh(); }

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    float meterY (float db) const;

    juce::Slider fader;
    ValueBox box;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
    int channels = 2;
    std::array<float, 2> levelDb { -120.0f, -120.0f }, holdDb { -120.0f, -120.0f };
    std::array<int, 2> holdTicks {};
    juce::Rectangle<int> meterArea;
};

} // namespace craft::multiband::ui
