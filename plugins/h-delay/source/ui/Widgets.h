// Craft Hybrid Delay - editor widgets (spec r1 section 13). All artwork is drawn in code.
#pragma once

#include <functional>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "plugin/PluginProcessor.h"

namespace craft::hdelay::ui
{

// Rotary knob bound to a parameter: title above, editable value below, optional scale-end labels
// and detent labels. Double-click or Alt-click resets; click the value to type one.
class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState&, const juce::String& paramId, const juce::String& title, bool large = false);

    void setScaleEnds (const juce::String& lo, const juce::String& hi);
    void setDetentLabels (const juce::StringArray& labels);
    void setAccent (juce::Colour c);
    juce::Slider& getSlider() noexcept { return slider; }

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::Rectangle<int> knobArea() const;

    juce::Slider slider;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
    juce::String title, lo, hi;
    juce::StringArray detents;
    bool large;
};

// Large square tap pad. Calls onTap with the interval of the last two taps (ms).
class TapPad : public juce::Component, private juce::Timer
{
public:
    std::function<void (double intervalMs)> onTap;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    double lastTapMs = -1.0e9;
    float flash = 0.0f;
    int count = 0;
};

// Dark display: delay time(s), tempo, Ping Pong start selector. Values can be dragged vertically.
class DelayDisplay : public juce::Component
{
public:
    explicit DelayDisplay (HDelayProcessor&);
    void refresh();

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    enum class Target { None, Left, Right, Tempo, Start };
    Target hitTest (juce::Point<int>) const;
    juce::Rectangle<float> valueArea (int line) const;
    juce::Rectangle<float> tempoArea() const;
    juce::Rectangle<float> startArea() const;
    const char* paramFor (Target) const;

    HDelayProcessor& proc;
    Settings shown;
    Target dragTarget = Target::None;
    float dragStartNorm = 0.0f;
    juce::RangedAudioParameter* dragParam = nullptr;
};

// Vertical output meter, 0 .. -48 dBFS, with a peak / clip LED (click to reset).
class OutputMeter : public juce::Component
{
public:
    void pushPeaks (float l, float r);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { clipHold = 0; peakHoldDb = -100.0f; repaint(); }

private:
    float levelDb[2] = { -100.0f, -100.0f };
    float peakHoldDb = -100.0f;
    int clipHold = 0, peakHoldFrames = 0;
};

// Small ring between the filter knobs: dragging moves both filter frequencies by the same ratio.
class LinkRing : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit LinkRing (juce::AudioProcessorValueTreeState&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    juce::AudioProcessorValueTreeState& state;
    juce::RangedAudioParameter *hp, *lp;
    float startHp = 0.0f, startLp = 0.0f;
    bool hpOn = false, lpOn = false, dragging = false;
    float turn = 0.0f;
};

} // namespace craft::hdelay::ui
