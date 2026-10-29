// Craft Vocal Compressor - parameter-bound controls and their meters (spec r1 section 12; meter ballistics are
// display only, section 7).
#pragma once

#include <functional>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace craft::vocal::ui
{

// Numeric value box: shows the parameter value in its display format.
//  - single click: type a value       - vertical drag: change the value (Shift: fine)
//  - double-click or Alt-click: reset to default
class ValueBox : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    ValueBox (juce::RangedAudioParameter& param, juce::String paramId, juce::Colour accent);

    void refresh();
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void resetToDefault();

    juce::RangedAudioParameter& param;
    const juce::String paramId;
    const juce::Colour accent;
    juce::Label label;
    float dragStart = 0.0f;
    bool dragging = false, gesture = false, suppressClick = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ValueBox)
};

// Vertical fader whose Ctrl/Cmd-drag also moves a partner parameter by the same number of dB (linking gesture).
class LinkSlider : public juce::Slider
{
public:
    juce::RangedAudioParameter* partner = nullptr;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    bool linking = false;
    double startValue = 0.0, partnerStart = 0.0;
};

// One strip: title, value box, a vertical fader drawn over its meter, readouts below.
class FaderStrip : public juce::Component
{
public:
    enum class Meter
    {
        inputEnergy,   // Gate: input energy, scale 12 ... 72 dB below full scale
        gainReduction, // Compression: total gain reduction with peak hold
        outputPeaks    // Gain: L/R output sample peaks with peak hold
    };

    FaderStrip (juce::AudioProcessorValueTreeState&, const juce::String& paramId, const juce::String& title, Meter,
                juce::Colour accent);

    void setLinkPartner (juce::RangedAudioParameter* p) { slider.partner = p; }
    void setChannels (int numChannels);
    void refresh() { box.refresh(); }

    // Meter feeds (called at the GUI refresh rate).
    void pushInputEnergy (float meanSquare);
    void pushGainReduction (float minGain);
    void pushOutputPeaks (float peakL, float peakR);
    void resetPeaks();

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    float yForValue (float v) const;
    juce::Rectangle<float> meterRect() const;
    juce::Rectangle<float> barRect (int ch) const;
    juce::Rectangle<int> readoutArea() const;
    void paintScale (juce::Graphics&);

    LinkSlider slider;
    ValueBox box;
    juce::String title;
    Meter meter;
    juce::Colour accent;
    float bottomValue = -36.0f;
    int channels = 2;

    float levelDb[2] = { -200.0f, -200.0f }; // input energy (ch 0) / output peaks
    float peakDb[2] = { -200.0f, -200.0f };  // output peak hold
    float grDb = 0.0f, grPeakDb = 0.0f;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
};

} // namespace craft::vocal::ui
