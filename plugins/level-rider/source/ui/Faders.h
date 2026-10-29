// Craft Level Rider - Target bar and the three vertical faders (spec r1 section 13), vector-drawn.
#pragma once

#include <functional>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace craft::levelrider::ui
{

// Horizontal Target slider over a level indicator of the detected vocal level (dBFS).
class TargetBar : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit TargetBar (juce::RangedAudioParameter& target);

    void setLevel (float db, bool riding);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

    static constexpr float lowDb = -60.0f, highDb = 0.0f;

private:
    juce::Rectangle<float> track() const;
    float xFor (float db) const;
    float dbFor (float x) const;

    juce::RangedAudioParameter& param;
    juce::ParameterAttachment attachment;
    float targetDb = -21.0f, levelDb = -200.0f, heldDb = -200.0f;
    int holdCounter = 0;
    bool riding = false;
};

// Shared vertical +-12 dB fader geometry.
class VerticalScale
{
public:
    void setTrack (juce::Rectangle<float> t) { trackArea = t; }
    juce::Rectangle<float> track() const { return trackArea; }
    float yFor (float db) const { return trackArea.getBottom() - (db + 12.0f) / 24.0f * trackArea.getHeight(); }
    float dbFor (float y) const { return juce::jlimit (-12.0f, 12.0f, (trackArea.getBottom() - y) / trackArea.getHeight() * 24.0f - 12.0f); }
    void drawTicks (juce::Graphics&, float x, bool labelsLeft, float step) const;

private:
    juce::Rectangle<float> trackArea;
};

// Range fader: Max and Min handles plus the Idle arrow marker between them.
class RangeFader : public juce::Component, public juce::SettableTooltipClient
{
public:
    RangeFader (juce::RangedAudioParameter& maxParam, juce::RangedAudioParameter& minParam, juce::RangedAudioParameter& idleParam);

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    juce::RangedAudioParameter* pick (const juce::MouseEvent&) const;

    juce::RangedAudioParameter &maxP, &minP, &idleP;
    juce::ParameterAttachment maxA, minA, idleA;
    float maxDb = 6.0f, minDb = -6.0f, idleDb = 0.0f;
    VerticalScale scale;
    juce::RangedAudioParameter* dragged = nullptr;
};

// The large Rider fader: shows the ride gain g, marks the range, can be dragged to override g (section 9).
class RiderFader : public juce::Component, public juce::SettableTooltipClient
{
public:
    struct Callbacks
    {
        std::function<void (double)> begin, drag;
        std::function<void()> end;
        std::function<void()> resetToIdle;
    };
    explicit RiderFader (Callbacks);

    void setState (float gainDb, float rangeMin, float rangeMax, float idleDb, bool riding);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    Callbacks cb;
    VerticalScale scale;
    float g = 0.0f, mn = -6.0f, mx = 6.0f, idle = 0.0f, grabOffset = 0.0f;
    bool riding = false, dragging = false;
};

// Output trim fader (+-12 dB) with a clip indicator above it.
class OutputFader : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit OutputFader (juce::RangedAudioParameter& output);

    void setClip (bool clipped, float peak);
    std::function<void()> onClearClip;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> clipArea() const;

    juce::RangedAudioParameter& param;
    juce::ParameterAttachment attachment;
    VerticalScale scale;
    float valueDb = 0.0f, grabOffset = 0.0f, peakLevel = 0.0f;
    bool clip = false, dragging = false;
};

} // namespace craft::levelrider::ui
