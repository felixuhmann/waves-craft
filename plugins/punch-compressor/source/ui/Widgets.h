// Craft Punch Compressor - editor widgets (spec r1 section 14). All artwork is drawn in code.
#pragma once

#include <functional>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace craft::punch::ui
{

// Numeric value box showing a parameter in its section 2 format.
//  - click: type a value     - vertical drag: adjust     - double-click or Alt-click: reset to default
class ValueBox : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    ValueBox (juce::RangedAudioParameter& param, juce::String paramId, juce::String unit = {});

    void refresh();
    void setFontHeight (float h);
    // optional text override (e.g. "Off" plus the host tempo); empty = formatted value
    std::function<juce::String (const juce::String& formatted)> decorate;

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
    juce::Label label;
    float dragStart = 0.0f;
    bool dragging = false, gesture = false, suppressClick = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ValueBox)
};

// Rotary knob bound to a parameter: title above, editable value box below.
class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState&, const juce::String& paramId, const juce::String& title,
          const juce::String& unit, bool large = false);

    void refresh() { box.refresh(); }
    void setAccent (juce::Colour c);
    void setTitle (const juce::String& t);
    juce::Slider& getSlider() noexcept { return slider; }

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::Slider slider;
    ValueBox box;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
    juce::String title;
    bool large;
};

// Row of segments bound to a choice (or bool) parameter; segment i selects index i.
class Selector : public juce::Component, public juce::SettableTooltipClient
{
public:
    Selector (juce::RangedAudioParameter& param, juce::StringArray labels, juce::Colour accent);

    int getIndex() const;
    void refresh();
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

private:
    int segmentAt (juce::Point<int>) const;
    juce::RangedAudioParameter& param;
    juce::StringArray labels;
    juce::Colour accent;
    int shown = -1, hover = -1;
};

// Needle meter with a 0 ... -48 dB scale: input / output level (dBFS) or gain reduction (dB).
class VuMeter : public juce::Component, public juce::SettableTooltipClient
{
public:
    enum class Source { In, Gr, Out };

    void setSource (Source s);
    // linear peak (In / Out) or linear gain-reduction gain (GR) since the previous call
    void push (float value);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { peakDb = -100.0f; repaint(); }

private:
    float angleFor (float db) const;
    Source source = Source::Gr;
    float shownDb = 0.0f;    // needle position, 0 ... -48 (GR shown as -GR)
    float peakDb = -100.0f; // -100: none yet
};

} // namespace craft::punch::ui
