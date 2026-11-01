// Craft Band Limiter - gain/frequency display and level meters (spec r1 section 13). Original artwork.
#pragma once

#include <array>
#include <functional>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "dsp/Parameters.h"

namespace craft::band_limiter::ui
{

// Five bands' dynamic gain over frequency: live line, smoothed trail and peak-hold line; four crossover handles drag
// horizontally, band handles drag vertically (gain, plus priority when linked).
class BandDisplay : public juce::Component, public juce::SettableTooltipClient
{
public:
    BandDisplay (std::array<juce::RangedAudioParameter*, dsp::numCrossovers> crossovers,
                 std::array<juce::RangedAudioParameter*, dsp::numBands> gains,
                 std::array<juce::RangedAudioParameter*, dsp::numBands> priorities);

    void pushGains (const std::array<float, dsp::numBands>& linearGains);
    void setLinked (const std::array<bool, dsp::numBands>& linked) { gainPriorityLinked = linked; }

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> plot() const;
    float freqToX (float hz) const;
    float xToFreq (float x) const;
    float attenToY (float db) const;
    float gainDbToY (float db) const;
    float crossover (int k) const;
    float bandCentreHz (int b) const;
    juce::Path curve (const std::array<float, dsp::numBands>& atten) const;

    std::array<juce::RangedAudioParameter*, dsp::numCrossovers> crossovers;
    std::array<juce::RangedAudioParameter*, dsp::numBands> gains, priorities;
    std::array<float, dsp::numBands> live {}, trail {}, hold {};
    std::array<int, dsp::numBands> holdTimer {};
    std::array<bool, dsp::numBands> gainPriorityLinked {};
    int dragCrossover = -1, dragBand = -1;
    float priorityOffset = 0.0f;
};

// Vertical level meter with an optional draggable marker (Threshold / Ceiling): 0 dB at the top, -30 dB at the bottom.
class LevelMeter : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit LevelMeter (juce::RangedAudioParameter* markerParam, juce::Colour markerColour);

    void setLevel (float linearPeak);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

    std::function<void (float db)> onMarkerDrag; // replaces the direct write (link handle)

private:
    void setMarker (float y);
    juce::RangedAudioParameter* param;
    juce::Colour colour;
    float level = 0.0f;
};

// Attenuation bar, 0..30 dB downward.
class AttenuationMeter : public juce::Component, public juce::SettableTooltipClient
{
public:
    void setGain (float linear);
    void paint (juce::Graphics&) override;

private:
    float atten = 0.0f, peakHold = 0.0f;
    int holdTimer = 0;
};

} // namespace craft::band_limiter::ui
