// Craft Audio doubler - parameter-bound controls (spec r1 section 12).
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace craft::doubler::ui
{

// Thin helper around a host parameter for the custom displays: physical values and change gestures.
struct ParamHandle
{
    juce::RangedAudioParameter* p = nullptr;

    ParamHandle() = default;
    ParamHandle (juce::AudioProcessorValueTreeState& s, const juce::String& id) : p (s.getParameter (id)) {}

    explicit operator bool() const noexcept { return p != nullptr; }
    double get() const { return p != nullptr ? double (p->convertFrom0to1 (p->getValue())) : 0.0; }
    bool on() const { return p != nullptr && p->getValue() >= 0.5f; }
    double getDefault() const { return p != nullptr ? double (p->convertFrom0to1 (p->getDefaultValue())) : 0.0; }
    void begin() { if (p) p->beginChangeGesture(); }
    void end() { if (p) p->endChangeGesture(); }
    void set (double physical) { if (p) p->setValueNotifyingHost (p->convertTo0to1 (float (physical))); }
    void setNormalised (float r) { if (p) p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, r)); }
    void setGesture (double physical) { begin(); set (physical); end(); }
    void resetToDefault() { if (p) { p->beginChangeGesture(); p->setValueNotifyingHost (p->getDefaultValue()); p->endChangeGesture(); } }
};

// Numeric value box (spec section 12): vertical drag changes the value (Shift = fine), double-click opens a
// text editor, Alt-click (or Alt-double-click) resets to the default.
class ValueBox : public juce::Component, public juce::SettableTooltipClient
{
public:
    ValueBox (juce::AudioProcessorValueTreeState&, const juce::String& paramId, juce::String unit = {});

    void refresh();
    void setAccent (juce::Colour c) { accent = c; repaint(); }
    void setFontHeight (float h);

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    juce::String currentText() const;

    ParamHandle param;
    const juce::String paramId, unit;
    juce::Label label;
    juce::Colour accent;
    float dragStart = 0.0f;
    bool dragging = false, gesture = false, suppress = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ValueBox)
};

// Small rotary knob with its value printed beside or below it.
class Knob : public juce::Component
{
public:
    enum class Layout { ValueRight, ValueBelow };

    Knob (juce::AudioProcessorValueTreeState&, const juce::String& paramId, juce::String unit, Layout layout);

    void refresh() { box.refresh(); }
    void setAccent (juce::Colour c);
    void resized() override;

private:
    juce::Slider slider;
    ValueBox box;
    Layout layout;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
};

// Vertical fader (master output).
class Fader : public juce::Component
{
public:
    Fader (juce::AudioProcessorValueTreeState&, const juce::String& paramId);
    void refresh() { box.refresh(); }
    void paint (juce::Graphics&) override;
    void resized() override;
    juce::Rectangle<int> getTrackBounds() const { return slider.getBounds(); }

private:
    juce::Slider slider;
    ValueBox box;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
};

// Toggle buttons bound to bool parameters, in several looks.
class ToggleButton : public juce::Button
{
public:
    enum class Style { Led, VoiceNumber, OctaveArrow, Text };

    ToggleButton (Style, juce::String text, juce::Colour colour = juce::Colour (0xffe3a447));
    void paintButton (juce::Graphics&, bool over, bool down) override;

private:
    Style style;
    juce::Colour colour;
};

// Two-position switch drawn as a segmented control; toggle state = right segment.
class SegmentSwitch : public juce::Button
{
public:
    SegmentSwitch (juce::String left, juce::String right);
    void paintButton (juce::Graphics&, bool over, bool down) override;

private:
    juce::String leftText, rightText;
};

// Segmented selector for a multi-position parameter (input routing: 0, 1, 2).
class ChoiceSegments : public juce::Component, public juce::SettableTooltipClient
{
public:
    ChoiceSegments (juce::AudioProcessorValueTreeState&, const juce::String& paramId, juce::StringArray labels);
    void refresh();
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }

private:
    int segmentAt (juce::Point<int>) const;
    ParamHandle param;
    juce::StringArray labels;
    int current = 0;
};

// Momentary button: the parameter is 1 while the button is held (Modulation Reset, a rising-edge trigger).
class MomentaryButton : public juce::Component, public juce::SettableTooltipClient
{
public:
    MomentaryButton (juce::AudioProcessorValueTreeState&, const juce::String& paramId, juce::String text);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    ParamHandle param;
    juce::String text;
    bool held = false;
};

} // namespace craft::doubler::ui
