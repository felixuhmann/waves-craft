// Craft Enhancer - band region display with draggable crossover handles (spec r1 section 9).
#pragma once

#include <array>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "plugin/PluginProcessor.h"

namespace craft::enhancer::ui
{

class BandDisplay : public juce::Component
{
public:
    explicit BandDisplay (EnhancerProcessor&);

    void refresh(); // from the editor timer; repaints when the settings moved
    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    // Normalised value of crossover k limited by its neighbours.
    static float limitToNeighbours (EnhancerProcessor&, int k, float norm);

private:
    float xForHz (double hz) const;
    double hzForX (float x) const;
    int handleAt (float x) const;

    EnhancerProcessor& proc;
    dsp::Settings shown;
    int dragging = -1;
};

} // namespace craft::enhancer::ui
