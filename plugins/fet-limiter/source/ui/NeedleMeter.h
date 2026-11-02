// Craft FET Limiter - moving-needle meter with over LED (spec 12; original drawing).
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace craft::fet_limiter::ui
{

class NeedleMeter : public juce::Component
{
public:
    enum class Mode
    {
        GainReduction,
        Level
    };

    void setMode (Mode m);
    void setAccent (juce::Colour c);

    // Called at the editor's refresh rate. value: gain reduction in dB (>= 0) or a linear peak level;
    // overPeak: linear output peak (lights the over LED at >= 0 dBFS).
    void update (float value, float overPeak, float dtSeconds);

    void paint (juce::Graphics&) override;

private:
    static float scaleToNorm (float meterDb);

    Mode mode = Mode::GainReduction;
    juce::Colour accent;
    float needleDb = 0.0f;  // position in meter dB (-20 .. +3)
    float overHold = 0.0f;  // seconds left
};

} // namespace craft::fet_limiter::ui
