// Craft De-Esser - display-only meters (spec r3 section 12; ballistics unspecified, section 7).
#pragma once

#include <functional>
#include <vector>

#include <juce_gui_basics/juce_gui_basics.h>

namespace craft::deesser::ui
{

// Numeric peak readout; click to reset.
class PeakReadout : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit PeakReadout (bool reductionStyle = false);
    void push (float db);  // keeps the maximum (or, for reduction, the deepest) value
    void reset();
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { reset(); }

private:
    const bool reduction;
    float peakDb;
};

struct MeterScale
{
    float value;         // dB
    juce::String text;
};

// Vertical bar meter. `toFraction` maps a dB value to 0 (bottom) .. 1 (top); for a reduction meter the
// bar grows from the top downwards and `toFraction` maps reduction to 0 (top) .. 1 (bottom).
class BarMeter : public juce::Component
{
public:
    BarMeter (int numBars, bool reductionStyle, std::function<float (float)> toFraction, std::vector<MeterScale> scale);

    void setLevels (const float* db);      // one value per bar (dB; reduction meters: positive dB of reduction)
    void setZone (float fromFraction, float toFraction, juce::Colour tint); // background tint band
    void setScaleOnLeft (bool left) { scaleLeft = left; }
    void setBarLabels (juce::StringArray labels) { barLabels = std::move (labels); }

    // Area of the bars (component coordinates) - used to align the threshold slider with the scale.
    juce::Rectangle<int> getBarsArea() const;

    void paint (juce::Graphics&) override;

private:
    const int bars;
    const bool reduction;
    std::function<float (float)> toFrac;
    std::vector<MeterScale> scale;
    std::vector<float> levels;
    bool scaleLeft = true;
    juce::StringArray barLabels;
    float zoneFrom = 0.0f, zoneTo = 0.0f;
    juce::Colour zoneTint;
};

} // namespace craft::deesser::ui
