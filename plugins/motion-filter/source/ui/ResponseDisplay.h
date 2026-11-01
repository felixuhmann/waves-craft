// Craft Motion Filter - live filter response display; drag to set Freq/Delay (x) and Res/Feedback (y).
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "plugin/PluginProcessor.h"

namespace craft::mfp::ui
{

class ResponseDisplay : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit ResponseDisplay (MotionFilterProcessor&);

    void update(); // from the editor timer

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    static constexpr double fMin = 20.0, fMax = 20000.0, dbMin = -36.0, dbMax = 30.0;

    juce::Rectangle<float> plot() const;
    float xOf (double f) const;
    double fOf (float x) const;
    float yOf (double db) const;
    double magnitudeDb (int channel, double f) const;
    juce::Path curve (int channel) const;

    MotionFilterProcessor& proc;
    juce::RangedAudioParameter* freqParam = nullptr;
    juce::RangedAudioParameter* resParam = nullptr;
    std::unique_ptr<juce::ParameterAttachment> freqAttachment, resAttachment;

    mf::Settings settings;
    double cutoff[2] { 18000.0, 18000.0 }, combMs[2] { 28.0, 28.0 }, resonance[2] {};
    bool dragging = false;
};

} // namespace craft::mfp::ui
