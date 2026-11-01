// Craft Peak Limiter - parameter-bound controls (spec r1 section 12).
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace craft::limiter::ui
{

// Numeric value box: shows the parameter value in its spec format.
//  - single click: type a value       - vertical drag: change the value
//  - double-click or Alt-click: reset to default
class ValueBox : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    ValueBox (juce::RangedAudioParameter& param, juce::String paramId, bool framed = true);

    void refresh();
    void setFontHeight (float h);

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
    const juce::String paramId;
    const bool framed;
    juce::Label label;
    float dragStart = 0.0f;
    bool dragging = false, gesture = false, suppressClick = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ValueBox)
};

// Vertical dB fader drawn over one or two level meters (Threshold over the input meters, Out Ceiling over the
// output meters). Scale 0 ... -30 dB; the meter and the fader share one dB-to-pixel mapping.
class MeterFader : public juce::Component
{
public:
    MeterFader (juce::AudioProcessorValueTreeState&, const juce::String& paramId, const juce::String& title,
                const juce::String& meterLabel, juce::Colour handleColour);

    void setChannels (int numChannels);
    // linear sample peaks since the previous call
    void pushLevels (float peak1, float peak2);
    void refresh() { box.refresh(); }

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    float yForDb (float db) const;
    juce::Rectangle<float> barRect (int ch) const;

    juce::Slider slider;
    ValueBox box;
    juce::String title, meterLabel;
    juce::Colour handleColour;
    int channels = 2;
    float levelDb[2] = { -100.0f, -100.0f }, holdDb[2] = { -100.0f, -100.0f };
    int holdTicks[2] = { 0, 0 };
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
};

// Release knob: logarithmic 0.01 ... 1000 ms, value shown (and editable) in its centre, end labels below.
class ReleaseKnob : public juce::Component
{
public:
    ReleaseKnob (juce::AudioProcessorValueTreeState&, const juce::String& paramId);

    void refresh() { box.refresh(); }
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::Slider slider;
    ValueBox box;
    juce::Label unit;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
};

// On/off switch with an indicator lamp (Auto Release).
class LampSwitch : public juce::Button
{
public:
    explicit LampSwitch (const juce::String& text);
    void paintButton (juce::Graphics&, bool over, bool down) override;
};

} // namespace craft::limiter::ui
