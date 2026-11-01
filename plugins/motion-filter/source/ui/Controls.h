// Craft Motion Filter - parameter-bound controls (all vector drawn).
#pragma once

#include <functional>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace craft::mfp::ui
{

using APVTS = juce::AudioProcessorValueTreeState;

// Rotary knob with title above and the parameter's value text below. Optionally shows an alternate
// parameter instead (e.g. a tempo-sync choice) while a switch parameter is on.
class Knob : public juce::Component
{
public:
    Knob (APVTS&, const juce::String& paramId, const juce::String& title, juce::Colour accent);

    void setAlternate (APVTS&, const juce::String& altParamId, const juce::String& switchParamId);
    void setTitle (const juce::String& t);
    void refresh(); // from the editor timer: alternate visibility and value text

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::Slider& active() noexcept { return showingAlt ? altSlider : slider; }

    juce::Slider slider, altSlider;
    std::unique_ptr<APVTS::SliderAttachment> attachment, altAttachment;
    juce::RangedAudioParameter* param = nullptr;
    juce::RangedAudioParameter* altParam = nullptr;
    juce::RangedAudioParameter* switchParam = nullptr;
    juce::String title, valueText;
    bool showingAlt = false;
};

// Small horizontal bipolar slider with a label on its left (modulation amounts).
class ModSlider : public juce::Component
{
public:
    ModSlider (APVTS&, const juce::String& paramId, const juce::String& label, juce::Colour colour);
    void refresh();
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::Slider slider;
    std::unique_ptr<APVTS::SliderAttachment> attachment;
    juce::RangedAudioParameter* param = nullptr;
    juce::String label, valueText;
    juce::Colour colour;
};

// Two-position switch bound to a bool parameter, drawn as a segmented control.
class SegmentToggle : public juce::Button
{
public:
    SegmentToggle (APVTS&, const juce::String& paramId, juce::String offText, juce::String onText, juce::Colour accent);
    void paintButton (juce::Graphics&, bool over, bool down) override;

private:
    juce::String offText, onText;
    juce::Colour accent;
    std::unique_ptr<APVTS::ButtonAttachment> attachment;
};

// Row (or grid) of segments bound to a choice parameter; segments can draw their own icon.
class ChoiceStrip : public juce::Component, public juce::SettableTooltipClient
{
public:
    using IconPainter = std::function<void (juce::Graphics&, juce::Rectangle<float>, int index, juce::Colour)>;

    ChoiceStrip (APVTS&, const juce::String& paramId, juce::StringArray labels, juce::Colour accent, int columns = 0);
    void setIconPainter (IconPainter p) { icon = std::move (p); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> cell (int i) const;
    int indexAt (juce::Point<float>) const;

    juce::RangedAudioParameter* param = nullptr;
    juce::StringArray labels;
    juce::Colour accent;
    int columns, selected = 0, hover = -1;
    IconPainter icon;
    std::unique_ptr<juce::ParameterAttachment> attachment;
};

// Integer value box: vertical drag or wheel changes the value, double-click resets.
class DragValue : public juce::Component, public juce::SettableTooltipClient
{
public:
    DragValue (APVTS&, const juce::String& paramId, const juce::String& title, juce::Colour accent);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    juce::RangedAudioParameter* param = nullptr;
    juce::String title;
    juce::Colour accent;
    float value = 0.0f, dragStartValue = 0.0f;
    std::unique_ptr<juce::ParameterAttachment> attachment;
};

} // namespace craft::mfp::ui
