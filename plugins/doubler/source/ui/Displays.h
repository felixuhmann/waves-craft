// Craft Audio doubler - the three top-row displays (spec r1 section 12), all drawn in code:
// stage (pan / gain), delay-detune plane, voice EQ curve.
#pragma once

#include <array>
#include <functional>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "Controls.h"

namespace craft::doubler::ui
{

// Common base: a panel with a title that owns a set of draggable points.
class DisplayBase : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit DisplayBase (juce::String title);
    void paint (juce::Graphics&) override;

    // re-read the parameters (called by the editor timer); repaints when something changed
    virtual void refresh() = 0;

protected:
    juce::Rectangle<float> plotArea() const;
    virtual void paintContent (juce::Graphics&, juce::Rectangle<float> area) = 0;
    juce::String title;
};

// Semicircular stage: one dot per voice plus the direct; angle = pan, radius = gain.
class StageDisplay : public DisplayBase
{
public:
    StageDisplay (juce::AudioProcessorValueTreeState&, int numVoices);
    void refresh() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    struct Item
    {
        ParamHandle on, gain, pan;
        juce::Colour colour;
        juce::String label;
        double g = 0, p = 0;
        bool active = false;
    };
    void paintContent (juce::Graphics&, juce::Rectangle<float>) override;
    juce::Point<float> centre() const;
    float radius() const;
    juce::Point<float> positionOf (const Item&) const;
    int hitTest (juce::Point<float>) const;

    std::vector<Item> items; // voices first, the direct last
    int dragging = -1;
};

// Delay / detune plane: x = delay 0..100 ms, y = detune +-100 cents; a bar shows the modulation depth.
class DelayPlaneDisplay : public DisplayBase
{
public:
    DelayPlaneDisplay (juce::AudioProcessorValueTreeState&, int numVoices, std::function<double (int)> liveCents);
    void refresh() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    struct Item
    {
        ParamHandle on, delay, detune, depth, octave;
        double d = 0, c = 0, dep = 0, live = 0;
        bool active = false, oct = false;
    };
    void paintContent (juce::Graphics&, juce::Rectangle<float>) override;
    juce::Point<float> toScreen (double delayMs, double cents) const;
    int hitTest (juce::Point<float>) const;

    std::vector<Item> items;
    ParamHandle range, align;
    bool range20 = false, aligned = false;
    std::function<double (int)> liveCents;
    int dragging = -1;
};

// Voice EQ curve with draggable low (L) and high (H) shelf markers; double-click toggles a shelf.
class EqDisplay : public DisplayBase
{
public:
    explicit EqDisplay (juce::AudioProcessorValueTreeState&);
    void refresh() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    struct Band
    {
        ParamHandle on, gain, freq;
        double g = 0, f = 1000;
        bool active = false;
    };
    void paintContent (juce::Graphics&, juce::Rectangle<float>) override;
    float xOf (double hz) const;
    double hzAt (float x) const;
    float yOf (double db) const;
    double dbAt (float y) const;
    juce::Point<float> markerOf (const Band&) const;
    int hitTest (juce::Point<float>) const;

    std::array<Band, 2> bands;
    int dragging = -1;
};

} // namespace craft::doubler::ui
