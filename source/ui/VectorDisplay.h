// Craft Audio stereo imager - stereo vector display (spec r1 section 12).
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "dsp/Matrix.h"

namespace craft::ui
{

// Probe-based display of the broadband path (no shuffler). Drag: vertical = Gain,
// horizontal = Rotation; with Alt: vertical = Width, horizontal = Asymmetry.
class VectorDisplay : public juce::Component
{
public:
    VectorDisplay (juce::AudioProcessorValueTreeState&, dsp::Variant);

    void setSettings (const dsp::Settings&);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    // Geometry helpers (public for clarity / reuse)
    struct Probe
    {
        double angleDeg; // from vertical, left positive
        double levelDb;  // clamped to [-24, 0]
    };
    static Probe probe (const dsp::Matrix2& a, double inL, double inR);

private:
    juce::Rectangle<float> plotArea() const;
    juce::Point<float> pointFor (const Probe&) const;

    juce::AudioProcessorValueTreeState& state;
    dsp::Variant variant;
    dsp::Settings settings;
    bool hasSettings = false;

    juce::RangedAudioParameter *dragX = nullptr, *dragY = nullptr;
    float startX = 0, startY = 0;
};

} // namespace craft::ui
