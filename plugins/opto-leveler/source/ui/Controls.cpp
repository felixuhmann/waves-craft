#include "Controls.h"

#include "OptoLookAndFeel.h"

namespace craft::opto_leveler::ui
{

namespace
{
constexpr float startAngle = juce::MathConstants<float>::pi * 1.25f;
constexpr float endAngle = juce::MathConstants<float>::pi * 2.75f;
constexpr int textBoxH = 20, captionH = 16;
} // namespace

void ResetSlider::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isAltDown() && isDoubleClickReturnEnabled())
    {
        setValue (getDoubleClickReturnValue(), juce::sendNotificationSync);
        return;
    }
    juce::Slider::mouseDown (e);
}

// ---------------------------------------------------------------------------------------------
Knob::Knob (juce::AudioProcessorValueTreeState& state, const char* paramId, const juce::String& cap, juce::Colour colour,
            std::vector<Tick> t)
    : param (*state.getParameter (paramId)), caption (cap), ticks (std::move (t))
{
    slider.setRotaryParameters (startAngle, endAngle, true);
    slider.setColour (juce::Slider::thumbColourId, colour);
    slider.setScrollWheelEnabled (true);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, paramId, slider);
    slider.setDoubleClickReturnValue (true, double (param.convertFrom0to1 (param.getDefaultValue())));
    slider.setTextBoxIsEditable (true);
    slider.setTooltip (caption + ": drag or type a value; double-click or Alt-click resets");
    addAndMakeVisible (slider);
}

juce::Rectangle<float> Knob::dialBounds() const
{
    auto r = getLocalBounds().withTrimmedTop (captionH).withTrimmedBottom (textBoxH + 2).toFloat();
    const float size = std::min (r.getWidth(), r.getHeight());
    return r.withSizeKeepingCentre (size, size);
}

void Knob::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (captionH);
    const auto dial = dialBounds();
    // leave room for the scale labels around large knobs
    const float inset = ticks.empty() ? 0.0f : std::max (14.0f, dial.getWidth() * 0.13f);
    const auto inner = dial.reduced (inset);
    const int boxW = std::min (r.getWidth(), 76);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, boxW, textBoxH);
    // rotary area = inner (centred on the dial), the value box directly below it
    slider.setBounds (juce::Rectangle<int> (int (inner.getX()), int (inner.getY()), int (inner.getWidth()),
                                            int (inner.getHeight()) + textBoxH));
}

void Knob::paint (juce::Graphics& g)
{
    g.setColour (colours::textDim);
    g.setFont (juce::Font (uiFont (11.5f, true)));
    g.drawText (caption.toUpperCase(), getLocalBounds().removeFromTop (captionH), juce::Justification::centred);

    if (ticks.empty())
        return;
    // scale: ticks at display values, positioned with the parameter's taper (the knob scale is non-linear, spec 2)
    const auto dial = dialBounds();
    const auto c = dial.getCentre();
    const float inset = std::max (14.0f, dial.getWidth() * 0.13f);
    const float rKnob = dial.getWidth() * 0.5f - inset;
    const float r0 = rKnob + 2.0f, r1 = rKnob + 6.0f, rText = rKnob + inset * 0.62f;
    g.setFont (juce::Font (uiFont (10.0f)));
    for (const auto& t : ticks)
    {
        const float pos = param.convertTo0to1 (t.value);
        const float a = startAngle + pos * (endAngle - startAngle);
        const float s = std::sin (a), co = -std::cos (a);
        g.setColour (colours::textDim.withAlpha (0.8f));
        g.drawLine (c.x + r0 * s, c.y + r0 * co, c.x + r1 * s, c.y + r1 * co, 1.2f);
        if (t.label.isNotEmpty())
        {
            const juce::Rectangle<float> box (c.x + rText * s - 16.0f, c.y + rText * co - 7.0f, 32.0f, 14.0f);
            g.setColour (colours::textDim);
            g.drawText (t.label, box, juce::Justification::centred);
        }
    }
}

// ---------------------------------------------------------------------------------------------
SegmentedChoice::SegmentedChoice (juce::RangedAudioParameter& p, const juce::StringArray& l, juce::Colour col)
    : param (p), labels (l), colour (col), attachment (p, [this] (float v) {
          selected = juce::roundToInt (v);
          repaint();
      })
{
    attachment.sendInitialUpdate();
}

juce::Rectangle<float> SegmentedChoice::segment (int i) const
{
    const auto r = getLocalBounds().toFloat();
    const float w = r.getWidth() / float (labels.size());
    return { r.getX() + w * float (i), r.getY(), w, r.getHeight() };
}

int SegmentedChoice::indexAt (juce::Point<float> p) const
{
    for (int i = 0; i < labels.size(); ++i)
        if (segment (i).contains (p))
            return i;
    return -1;
}

void SegmentedChoice::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (colours::well);
    g.fillRoundedRectangle (r, 4.0f);
    for (int i = 0; i < labels.size(); ++i)
    {
        auto s = segment (i).reduced (2.0f);
        if (i == selected)
        {
            g.setColour (colour);
            g.fillRoundedRectangle (s, 3.0f);
        }
        else if (i == hover)
        {
            g.setColour (colours::panel.brighter (0.15f));
            g.fillRoundedRectangle (s, 3.0f);
        }
        g.setColour (i == selected ? colours::background : colours::text);
        g.setFont (juce::Font (uiFont (11.5f, true)));
        g.drawFittedText (labels[i], s.toNearestInt(), juce::Justification::centred, 1, 0.7f);
    }
    g.setColour (colours::panelEdge.brighter (0.2f));
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
}

void SegmentedChoice::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isAltDown())
    {
        mouseDoubleClick (e);
        return;
    }
    const int i = indexAt (e.position);
    if (i >= 0 && i != selected)
        attachment.setValueAsCompleteGesture (float (i));
}

void SegmentedChoice::mouseDoubleClick (const juce::MouseEvent&)
{
    attachment.setValueAsCompleteGesture (param.convertFrom0to1 (param.getDefaultValue()));
}

void SegmentedChoice::mouseMove (const juce::MouseEvent& e)
{
    const int i = indexAt (e.position);
    if (i != hover)
    {
        hover = i;
        repaint();
    }
}

void SegmentedChoice::mouseExit (const juce::MouseEvent&)
{
    hover = -1;
    repaint();
}

} // namespace craft::opto_leveler::ui
