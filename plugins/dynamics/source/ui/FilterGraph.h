// Craft Dynamics - filter module response with a Freq/Q crosshair (spec 12).
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "dsp/DynamicsEngine.h"

namespace craft::dynamics::ui
{

class FilterGraph : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit FilterGraph (juce::AudioProcessorValueTreeState& state);

    void update (const dsp::Settings& s, double sampleRate);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> plotArea() const;
    float xForHz (double hz) const;
    double hzForX (float x) const;
    float yForDb (double db) const;
    float yForQ (double q) const;
    double qForY (float y) const;
    void setFromMouse (juce::Point<float> p);

    juce::AudioProcessorValueTreeState& state;
    dsp::FilterType type = dsp::FilterType::LowPass;
    double freq = 7179.0, q = 0.6, fs = 48000.0;
    bool usedByModule = false;
    bool dragging = false;
};

} // namespace craft::dynamics::ui
