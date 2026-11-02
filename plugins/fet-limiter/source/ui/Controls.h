// Craft FET Limiter - parameter-bound controls (spec r1 section 12).
#pragma once

#include <functional>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace craft::fet_limiter::ui
{

// Rotary control with a value readout. Vertical drag (Shift = fine) or the wheel changes the value, double-click or
// Alt-click resets to the default, a click on the readout types a value. Every user change goes through
// onGestureStart / setNormalised / onGestureEnd, so the editor can link controls (Auto Makeup).
class Knob : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    struct ScaleLabel
    {
        float norm;
        juce::String text;
    };

    Knob (juce::RangedAudioParameter& param, const juce::String& title, bool large, std::vector<ScaleLabel> scale = {},
          juce::String caption = {});
    ~Knob() override;

    void setAccent (juce::Colour c);

    // optional hooks (set by the editor)
    std::function<void()> onGestureStart, onGestureEnd;
    std::function<void (float newNormalised)> onUserValue; // replaces the plain parameter write when set

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    void timerCallback() override;
    void beginGesture();
    void endGesture();
    void setNormalised (float v);
    void refreshText();
    juce::Rectangle<float> dialArea() const;

    juce::RangedAudioParameter& param;
    juce::ParameterAttachment attachment;
    juce::String title, caption;
    bool large;
    std::vector<ScaleLabel> scale;
    juce::Colour accent;
    juce::Label readout;
    float dragStart = 0.0f;
    bool dragging = false, inGesture = false, suppressClick = false, clickOnReadout = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Knob)
};

// A row or column of latching buttons selecting one index of a stepped parameter (Ratio, Meter, Analog, Revision).
class ButtonGroup : public juce::Component, public juce::SettableTooltipClient
{
public:
    ButtonGroup (juce::RangedAudioParameter& param, const juce::StringArray& labels, bool vertical,
                 std::vector<int> order = {});

    void setAccent (juce::Colour c);
    int getSelected() const noexcept { return selected; }
    std::function<void (int)> onChange; // called after the selection changed (any source)

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

private:
    int slotAt (juce::Point<float> p) const;
    juce::Rectangle<float> slot (int i) const;

    juce::RangedAudioParameter& param;
    juce::StringArray labels;
    bool vertical;
    std::vector<int> order; // slot -> parameter index
    juce::ParameterAttachment attachment;
    juce::Colour accent;
    int selected = 0, hover = -1;
};

// On/off switch bound to a bool parameter, drawn as a latching button with an indicator lamp.
class ToggleSwitch : public juce::Component, public juce::SettableTooltipClient
{
public:
    ToggleSwitch (juce::RangedAudioParameter& param, const juce::String& text);

    void setAccent (juce::Colour c);
    bool isOn() const noexcept { return on; }

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }

private:
    juce::RangedAudioParameter& param;
    juce::String text;
    juce::ParameterAttachment attachment;
    juce::Colour accent;
    bool on = false;
};

} // namespace craft::fet_limiter::ui
