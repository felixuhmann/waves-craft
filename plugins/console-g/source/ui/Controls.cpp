#include "Controls.h"

#include "ConsoleLookAndFeel.h"

namespace craft::consoleg::ui
{

namespace
{
void setupValueLabel (juce::Label& l, juce::RangedAudioParameter& param)
{
    l.setJustificationType (juce::Justification::centred);
    l.setFont (juce::Font (uiFont (11.5f, true)));
    l.setColour (juce::Label::textColourId, colours::text);
    l.setColour (juce::Label::backgroundWhenEditingColourId, colours::well);
    l.setColour (juce::Label::textWhenEditingColourId, colours::text);
    l.setColour (juce::Label::outlineWhenEditingColourId, colours::panelEdge);
    l.setEditable (true, false, false);
    l.setTooltip ("Click to type a value");
    l.onTextChange = [&param, &l] {
        const float v = param.getValueForText (l.getText());
        param.beginChangeGesture();
        param.setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, v));
        param.endChangeGesture();
    };
}

juce::String valueText (juce::RangedAudioParameter& p)
{
    auto t = p.getCurrentValueAsText();
    const auto unit = p.getLabel();
    return (unit.isEmpty() || t == "Off" || t == "+Inf") ? t : t + " " + unit;
}
} // namespace

Knob::Knob (juce::AudioProcessorValueTreeState& state, const char* paramId, juce::String n, juce::Colour accent, bool bipolar)
    : param (*state.getParameter (paramId)), name (std::move (n)), attachment (state, paramId, slider)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    slider.setMouseDragSensitivity (220);
    slider.setDoubleClickReturnValue (true, param.getDefaultValue());
    slider.setColour (juce::Slider::thumbColourId, accent);
    slider.getProperties().set ("bipolar", bipolar);
    slider.setTooltip (name + " - drag, double-click resets");
    slider.onValueChange = [this] { refresh(); };
    addAndMakeVisible (slider);
    setupValueLabel (value, param);
    addAndMakeVisible (value);
    refresh();
}

void Knob::refresh()
{
    if (! value.isBeingEdited())
        value.setText (valueText (param), juce::dontSendNotification);
}

void Knob::paint (juce::Graphics& g)
{
    g.setColour (colours::textDim);
    g.setFont (juce::Font (uiFont (11.0f, true)));
    g.drawText (name.toUpperCase(), getLocalBounds().removeFromTop (14), juce::Justification::centred);
}

void Knob::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (14);
    value.setBounds (r.removeFromBottom (16));
    slider.setBounds (r.reduced (2));
}

Fader::Fader (juce::AudioProcessorValueTreeState& state, const char* paramId, juce::String n, juce::Colour accent)
    : param (*state.getParameter (paramId)), name (std::move (n)), attachment (state, paramId, slider)
{
    slider.setSliderStyle (juce::Slider::LinearVertical);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    slider.setDoubleClickReturnValue (true, param.getDefaultValue());
    slider.setColour (juce::Slider::thumbColourId, accent);
    slider.setTooltip (name + " - drag, double-click resets");
    slider.onValueChange = [this] { refresh(); };
    addAndMakeVisible (slider);
    setupValueLabel (value, param);
    addAndMakeVisible (value);
    refresh();
}

void Fader::refresh()
{
    if (! value.isBeingEdited())
        value.setText (valueText (param), juce::dontSendNotification);
}

void Fader::paint (juce::Graphics& g)
{
    g.setColour (colours::textDim);
    g.setFont (juce::Font (uiFont (11.0f, true)));
    g.drawText (name.toUpperCase(), getLocalBounds().removeFromTop (14), juce::Justification::centred);
}

void Fader::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (14);
    value.setBounds (r.removeFromBottom (16));
    slider.setBounds (r.reduced (0, 6));
}

Switch::Switch (juce::AudioProcessorValueTreeState& state, const char* paramId, juce::String text, juce::Colour accent,
                juce::String tip)
    : juce::ToggleButton (text), attachment (state, paramId, *this)
{
    setColour (juce::ToggleButton::tickColourId, accent);
    if (tip.isNotEmpty())
        setTooltip (tip);
}

} // namespace craft::consoleg::ui
