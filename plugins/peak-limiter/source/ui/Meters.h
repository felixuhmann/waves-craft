// Craft Peak Limiter - gain reduction meter (spec r1 section 12; ballistics are display-only, section 7).
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace craft::limiter::ui
{

// Vertical gain reduction bar 0 ... -30 dB hanging from the top, with a click-to-reset peak readout below.
class GainReductionMeter : public juce::Component, public juce::SettableTooltipClient
{
public:
    GainReductionMeter();

    // smallest linear gain applied since the previous call (1 = no reduction)
    void pushGain (float minGain);
    void resetPeak();

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> barArea() const;
    juce::Rectangle<float> readoutArea() const;
    float yForGr (float grDb) const;

    float grDb = 0.0f;     // displayed reduction (positive dB)
    float peakGrDb = 0.0f; // maximum since reset
};

} // namespace craft::limiter::ui
