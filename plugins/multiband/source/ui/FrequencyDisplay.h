// Craft Multiband - frequency-domain display (spec r2 section 8): crossover markers / band handles, the
// Gain ... Gain+Range region of every band and the animated total gain curve.
#pragma once

#include <array>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "dsp/Parameters.h"

namespace craft::multiband::ui
{

class FrequencyDisplay : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit FrequencyDisplay (juce::AudioProcessorValueTreeState& state);

    // Current band gains in dB (incl. dynamics) and the sample rate, from the processor.
    void setLiveGains (const std::array<float, numBands>& gainsDb, double sampleRate);

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> plotArea() const;
    float xForFreq (double f) const;
    double freqForX (float x) const;
    float yForDb (double db) const;
    double dbPerPixel() const;
    double paramValue (juce::RangedAudioParameter* p) const { return p->convertFrom0to1 (p->getValue()); }
    juce::Point<float> handlePos (int band) const;
    int hitHandle (juce::Point<float> p) const;
    std::array<double, numCrossovers> crossovers() const;

    std::array<juce::RangedAudioParameter*, numCrossovers> xover {};
    std::array<juce::RangedAudioParameter*, numBands> gain {}, range {}, bypass {};
    juce::RangedAudioParameter* q = nullptr;
    juce::RangedAudioParameter* outputGain = nullptr;

    std::array<float, numBands> liveDb {};
    double fs = 48000.0;

    int hover = -1, active = -1;
    bool rangeMode = false;
    float startGain = 0.0f, startRange = 0.0f, startXover = 0.0f;
    juce::Point<float> dragStart;
};

} // namespace craft::multiband::ui
