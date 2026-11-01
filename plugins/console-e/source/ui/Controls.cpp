#include "Controls.h"

#include "LookAndFeel.h"
#include "plugin/ParameterLayout.h"

namespace craft::consolee::ui
{

namespace
{
juce::String displayText (const juce::String& id, double r)
{
    const auto u = unitFor (id, r);
    return u.isEmpty() ? formatValue (id, r) : formatValue (id, r) + " " + u;
}

void setupValueLabel (juce::Label& l, juce::RangedAudioParameter& param, const juce::String& id)
{
    l.setJustificationType (juce::Justification::centred);
    l.setFont (juce::Font (uiFont (11.5f, true)));
    l.setColour (juce::Label::textColourId, colours::text);
    l.setColour (juce::Label::backgroundColourId, colours::well);
    l.setColour (juce::Label::outlineColourId, colours::panelEdge);
    l.setColour (juce::Label::textWhenEditingColourId, colours::text);
    l.setColour (juce::Label::backgroundWhenEditingColourId, colours::well);
    l.setColour (juce::Label::outlineWhenEditingColourId, colours::accent);
    l.setEditable (false, true, false);
    l.setTooltip ("Double-click to type a value");
    l.onTextChange = [&l, &param, id] {
        const double r = parseValue (id, l.getText(), param.getValue());
        param.beginChangeGesture();
        param.setValueNotifyingHost (float (juce::jlimit (0.0, 1.0, r)));
        param.endChangeGesture();
        l.setText (displayText (id, param.getValue()), juce::dontSendNotification);
    };
}
} // namespace

// ---------------------------------------------------------------------------------------------
Knob::Knob (juce::AudioProcessorValueTreeState& state, const char* id, juce::String text, bool bipolar, bool offAtZero)
    : param (*state.getParameter (id)), paramId (id), attachment (state, id, slider)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    slider.setMouseDragSensitivity (220);
    slider.setDoubleClickReturnValue (true, param.getDefaultValue(), juce::ModifierKeys::altModifier);
    slider.getProperties().set ("bipolar", bipolar);
    slider.getProperties().set ("offAtZero", offAtZero);
    slider.setPopupDisplayEnabled (false, false, nullptr);
    slider.onValueChange = [this] { refreshValue(); };
    addAndMakeVisible (slider);

    caption.setText (text.toUpperCase(), juce::dontSendNotification);
    caption.setJustificationType (juce::Justification::centred);
    caption.setFont (juce::Font (uiFont (10.5f, true)));
    caption.setColour (juce::Label::textColourId, colours::textDim);
    caption.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (caption);

    setupValueLabel (value, param, paramId);
    addAndMakeVisible (value);
    refreshValue();
}

void Knob::refreshValue()
{
    if (! value.isBeingEdited())
        value.setText (displayText (paramId, slider.getValue()), juce::dontSendNotification);
}

void Knob::resized()
{
    auto r = getLocalBounds();
    caption.setBounds (r.removeFromTop (14));
    value.setBounds (r.removeFromBottom (17).withSizeKeepingCentre (std::min (r.getWidth(), 70), 17));
    r.removeFromBottom (2);
    const int s = std::min (r.getWidth(), r.getHeight());
    slider.setBounds (r.withSizeKeepingCentre (s, s));
}

// ---------------------------------------------------------------------------------------------
Switch::Switch (juce::AudioProcessorValueTreeState& state, const char* id, const juce::String& text, const juce::String& tooltip,
                bool warn)
    : juce::TextButton (text), attachment (state, id, *this)
{
    setClickingTogglesState (true);
    setTooltip (tooltip);
    getProperties().set ("warn", warn);
}

// ---------------------------------------------------------------------------------------------
Selector::Selector (juce::AudioProcessorValueTreeState& state, const char* id, juce::String off, juce::String onT)
    : offText (std::move (off)), onText (std::move (onT)),
      attachment (*state.getParameter (id), [this] (float v) { on = v >= 0.5f; repaint(); })
{
    attachment.sendInitialUpdate();
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void Selector::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (colours::well);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (colours::panelEdge.brighter (0.2f));
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
    auto left = r.withWidth (r.getWidth() / 2.0f), right = left.withX (left.getRight());
    auto seg = (on ? right : left).reduced (2.0f);
    g.setColour (colours::accent.withAlpha (0.85f));
    g.fillRoundedRectangle (seg, 3.0f);
    g.setFont (juce::Font (uiFont (10.5f, true)));
    g.setColour (on ? colours::textDim : colours::background);
    g.drawText (offText.toUpperCase(), left, juce::Justification::centred);
    g.setColour (on ? colours::background : colours::textDim);
    g.drawText (onText.toUpperCase(), right, juce::Justification::centred);
}

void Selector::mouseUp (const juce::MouseEvent& e)
{
    if (! getLocalBounds().contains (e.getPosition()))
        return;
    const bool wantOn = e.position.x >= getWidth() / 2.0f;
    if (wantOn != on)
        attachment.setValueAsCompleteGesture (wantOn ? 1.0f : 0.0f);
}

// ---------------------------------------------------------------------------------------------
Fader::Fader (juce::AudioProcessorValueTreeState& state, const char* id) : paramId (id), attachment (state, id, slider)
{
    auto& param = *state.getParameter (id);
    slider.setSliderStyle (juce::Slider::LinearVertical);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    slider.setSliderSnapsToMousePosition (false);
    slider.setDoubleClickReturnValue (true, param.getDefaultValue(), juce::ModifierKeys::altModifier);
    slider.onValueChange = [this] { refreshValue(); };
    addAndMakeVisible (slider);
    setupValueLabel (value, param, paramId);
    addAndMakeVisible (value);
    refreshValue();
}

void Fader::refreshValue()
{
    if (! value.isBeingEdited())
        value.setText (displayText (paramId, slider.getValue()), juce::dontSendNotification);
}

juce::Rectangle<int> Fader::getTrackArea() const { return slider.getBounds().reduced (0, 12); }

void Fader::resized()
{
    auto r = getLocalBounds();
    value.setBounds (r.removeFromBottom (17).withSizeKeepingCentre (std::min (r.getWidth(), 70), 17));
    r.removeFromBottom (4);
    slider.setBounds (r.withSizeKeepingCentre (40, r.getHeight()));
}

void Fader::paint (juce::Graphics& g)
{
    // dB scale beside the track (positions follow the section 2 taper)
    const auto track = getTrackArea();
    g.setFont (juce::Font (uiFont (9.5f)));
    for (double db : { 12.0, 6.0, 0.0, -6.0, -12.0, -24.0 })
    {
        const double r = juce::jlimit (0.0, 1.0, double (parseValue (paramId, juce::String (db), 0.5)));
        const float y = float (track.getBottom()) - float (r) * float (track.getHeight());
        g.setColour (db == 0.0 ? colours::accent.withAlpha (0.8f) : colours::textDim);
        g.drawText (juce::String (int (db)), juce::Rectangle<float> (0.0f, y - 6.0f, float (track.getX()) - 2.0f, 12.0f),
                    juce::Justification::centredRight);
        g.fillRect (juce::Rectangle<float> (float (track.getX()) + 2.0f, y - 0.5f, 5.0f, 1.0f));
    }
}

} // namespace craft::consolee::ui
