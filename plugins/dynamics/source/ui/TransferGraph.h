// Craft Dynamics - input/output transfer graph with draggable threshold markers (spec 12).
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "dsp/DynamicsEngine.h"

namespace craft::dynamics::ui
{

class TransferGraph : public juce::Component, public juce::SettableTooltipClient
{
public:
    TransferGraph (juce::AudioProcessorValueTreeState& state, bool hasComp, bool hasGate);

    // Settings and the current detector levels; repaints when something visible changed.
    void update (const dsp::Settings& s, float compLevelDb, float gateLevelDb);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;

private:
    enum Marker { None = -1, Threshold = 0, Open = 1, Close = 2 };

    juce::Rectangle<float> plotArea() const;
    juce::Rectangle<float> markerStrip() const;
    float xFor (double db) const;
    float yFor (double db) const;
    double dbForX (float x) const;
    double markerDb (int marker) const;
    juce::RangedAudioParameter* paramFor (int marker) const;
    int markerAt (juce::Point<float> p) const;
    void drawMarker (juce::Graphics&, int marker, juce::Colour c, bool filled) const;

    juce::AudioProcessorValueTreeState& state;
    const bool hasComp, hasGate;
    dsp::Settings settings;
    float compLevel = -200.0f, gateLevel = -200.0f;
    int dragging = None, hover = None;
    bool first = true;
};

} // namespace craft::dynamics::ui
