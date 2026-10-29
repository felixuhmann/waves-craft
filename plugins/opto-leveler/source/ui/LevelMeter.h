// Craft Opto Leveler - centre level meter: needle gauge showing In / GR / Out with VU-style ballistics
// (about 300 ms integration, spec 12). Original artwork drawn in code.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace craft::opto_leveler::ui
{

class LevelMeter : public juce::Component
{
public:
    enum class Source
    {
        In = 0,
        GR = 1,
        Out = 2
    };

    LevelMeter();

    void setSource (Source s);
    // Pushes one block of measurements (rectified averages of in / out, mean GR in dB); dt = seconds since the last push.
    void push (double inAverage, double outAverage, double grDb, double dt);

    void paint (juce::Graphics&) override;

    static constexpr float minVu = -20.0f, maxVu = 3.0f;
    static constexpr double referenceDbfs = -18.0; // 0 VU = -18 dBFS (sine RMS)

private:
    float valueToAngle (float v) const;
    double reading() const;

    Source source = Source::GR;
    double inState = 0.0, outState = 0.0, grState = 0.0;
};

} // namespace craft::opto_leveler::ui
