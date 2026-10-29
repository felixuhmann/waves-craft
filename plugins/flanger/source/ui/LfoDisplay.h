// Craft Flanger - LFO picture with moving L/R position markers, and the stereo phase indicator (spec 12).
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace craft::flanger::ui
{

// One LFO cycle drawn as a curve; the current position of each channel's LFO moves along it, and a
// sweep bar underneath shows the modulation value (-1 ... +1) of each channel.
class LfoDisplay : public juce::Component
{
public:
    struct State
    {
        float phase[2] {};
        float value[2] {};
        float delayMs[2] {};
        bool triangle = false, stopped = false, staticLfo = false;
        int channels = 2;
    };

    void setState (const State& s);
    void paint (juce::Graphics&) override;

private:
    State st;
};

// Small dial showing the right channel's LFO lead over the left (spec 5.3).
class StereoPhaseIndicator : public juce::Component
{
public:
    void setOffsetDegrees (float deg);
    void paint (juce::Graphics&) override;

private:
    float offsetDeg = 0.0f;
};

} // namespace craft::flanger::ui
