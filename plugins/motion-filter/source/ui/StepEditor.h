// Craft Motion Filter - 16 bipolar step sliders of the sequencer (spec r1 section 13).
#pragma once

#include <array>
#include <memory>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace craft::mfp::ui
{

// Edit modes: Drag - grab one bar and move it relative to where you clicked;
//             Draw - paint values across the bars with the mouse (each bar takes the pointer height).
class StepEditor : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit StepEditor (juce::AudioProcessorValueTreeState&);

    void setDrawMode (bool draw) { drawMode = draw; }
    void setPlayingStep (int step);   // 0-based, -1 = none
    void setLastStep (int last);       // 1..16

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> bar (int i) const;
    int barAt (float x) const;
    float valueAtY (float y) const;
    void set (int i, float v);

    std::array<juce::RangedAudioParameter*, 16> params {};
    std::array<std::unique_ptr<juce::ParameterAttachment>, 16> attachments;
    std::array<float, 16> values {};
    std::array<bool, 16> inGesture {};
    bool drawMode = false;
    int playing = -1, last = 16;
    int dragBar = -1, lastDrawBar = -1;
    float dragStartValue = 0.0f, lastDrawValue = 0.0f;
};

} // namespace craft::mfp::ui
