// Craft Mastering Limiter - meters with draggable Threshold / Ceiling markers, gain-reduction meter and
// peak-hold readouts (spec r1 section 14; ballistics are not specified, spec 11).
#pragma once

#include <functional>

#include <juce_gui_basics/juce_gui_basics.h>

namespace craft::mastering_limiter::ui
{

// Vertical level bars (1 or 2 channels) on a 0 .. -30 dB scale with a draggable marker beside them.
class MarkerMeter : public juce::Component, public juce::SettableTooltipClient
{
public:
    enum class ScaleSide { Left, Right };

    MarkerMeter (juce::Colour markerColour, ScaleSide side);

    void setChannels (int n);
    void pushLevels (float linL, float linR); // linear peaks since the previous frame
    void setMarkerDb (float db);               // current parameter value (display)
    void setLinkedHighlight (bool b);

    // user interaction (the editor applies the value, possibly to both linked markers)
    std::function<void()> onDragStart, onDragEnd, onReset;
    std::function<void (float db)> onDrag;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> barsArea() const;
    juce::Rectangle<float> scaleArea() const;

    juce::Colour markerColour;
    ScaleSide side;
    int channels = 2;
    float levelDb[2] = { -100.0f, -100.0f }, holdDb[2] = { -100.0f, -100.0f };
    int holdFrames[2] = { 0, 0 };
    float markerDb = 0.0f;
    bool linked = false, dragging = false;
    float dragStartDb = 0.0f, dragStartY = 0.0f;
};

// Gain-reduction meter: bar growing downward from 0 dB (0 .. -30 dB).
class GainReductionMeter : public juce::Component
{
public:
    void pushGain (float linGain); // minimum limiter gain since the previous frame
    void paint (juce::Graphics&) override;

private:
    float grDb = 0.0f, holdDb = 0.0f;
    int holdFrames = 0;
};

// Numeric peak-hold readout; click resets it.
class PeakReadout : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit PeakReadout (bool attenuation = false) : atten (attenuation) {}

    void push (float db);
    void reset();
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { reset(); }

private:
    bool atten;
    float value = -200.0f;
};

} // namespace craft::mastering_limiter::ui
