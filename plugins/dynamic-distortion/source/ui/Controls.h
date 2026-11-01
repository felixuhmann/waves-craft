// Craft Dynamic Distortion - parameter-bound controls (all vector drawn).
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace craft::ddp::ui
{

using APVTS = juce::AudioProcessorValueTreeState;

// Rotary knob with its title above and the value readout below. Double-click or Alt-click resets the
// parameter to its default; clicking the readout lets the user type a value (units optional).
class Knob : public juce::Component
{
public:
    Knob (APVTS&, const juce::String& paramId, const juce::String& title, juce::Colour accent);

    void refresh(); // from the editor timer: value text
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::Slider slider;
    juce::Label value;
    std::unique_ptr<APVTS::SliderAttachment> attachment;
    juce::RangedAudioParameter* param = nullptr;
    juce::String title;
};

// Vertical stereo peak meter (dBFS scale -48 .. +6) with peak hold.
class LevelMeter : public juce::Component
{
public:
    void push (float left, float right, int channels); // linear peaks since the last call
    void paint (juce::Graphics&) override;

private:
    float level[2] { -100.0f, -100.0f }, hold[2] { -100.0f, -100.0f };
    int holdTicks[2] { 0, 0 }, numChannels = 2;
};

} // namespace craft::ddp::ui
