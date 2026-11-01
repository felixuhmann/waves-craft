// Craft Dynamic Distortion - small response curve of the tone section (EQ + LP), spec r1 section 12.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "dsp/DynamicDistortion.h"

namespace craft::ddp::ui
{

class ToneDisplay : public juce::Component
{
public:
    void setSettings (const dd::Settings& s); // repaints when the tone settings changed
    void paint (juce::Graphics&) override;

    // Magnitude (dB) of the tone EQ and LP at f, from the analog prototypes used by the DSP core.
    static double responseDb (const dd::Settings& s, double f);

private:
    dd::Settings settings;
    bool valid = false;
};

} // namespace craft::ddp::ui
