// Craft De-Esser - parameter-bound controls (spec r1 section 12).
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace craft::deesser::ui
{

// Numeric value box showing the parameter in its spec format.
//  - single click: type a value   - vertical drag: change the value   - double-click or Alt-click: default
class ValueBox : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    ValueBox (juce::RangedAudioParameter& param, juce::String paramId, juce::String unit);

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
    juce::String currentText() const;

    juce::RangedAudioParameter& param;
    const juce::String paramId, unit;
    juce::Label label;
    float dragStart = 0.0f;
    bool dragging = false, gesture = false, suppressClick = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ValueBox)
};

// Two-state parameter shown as one button that names the current state; a click switches state.
class ModeToggle : public juce::Component, public juce::SettableTooltipClient
{
public:
    ModeToggle (juce::RangedAudioParameter& param, juce::String offText, juce::String onText);

    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    juce::RangedAudioParameter& param;
    juce::String offText, onText;
    bool on = false;
    juce::ParameterAttachment attachment;
};

// Sidechain filter-mode selector: a curve icon (high-pass / band-pass), the mode name and a drop-down arrow.
class FilterSelector : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit FilterSelector (juce::RangedAudioParameter& param);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

    // Small vector curve of a high-pass (bandPass = false) or band-pass response inside r.
    static void drawCurveIcon (juce::Graphics&, juce::Rectangle<float> r, bool bandPass, juce::Colour);

private:
    juce::RangedAudioParameter& param;
    bool highPass = true;
    juce::ParameterAttachment attachment;
};

// Monitor: two stacked buttons (Audio / SideChain); the active one is lit.
class MonitorSelector : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit MonitorSelector (juce::RangedAudioParameter& param);

    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    bool isMonitoringSidechain() const noexcept { return sidechain; }

private:
    juce::Rectangle<float> buttonArea (int index) const;

    juce::RangedAudioParameter& param;
    bool sidechain = false;
    juce::ParameterAttachment attachment;
};

} // namespace craft::deesser::ui
