#include "Controls.h"

#include "DistortionLookAndFeel.h"

namespace craft::ddp::ui
{

// ---------------------------------------------------------------------------------------------
Knob::Knob (APVTS& st, const juce::String& paramId, const juce::String& t, juce::Colour accent) : title (t)
{
    param = st.getParameter (paramId);
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
    slider.setColour (juce::Slider::thumbColourId, accent);
    slider.setMouseDragSensitivity (240);
    addAndMakeVisible (slider);
    attachment = std::make_unique<APVTS::SliderAttachment> (st, paramId, slider);
    if (param != nullptr)
    {
        // double-click and Alt-click reset to the default
        slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()), juce::ModifierKeys::altModifier);
        slider.setTooltip (param->getName (64) + " - double-click or Alt-click to reset");
    }

    value.setJustificationType (juce::Justification::centred);
    value.setFont (juce::Font (uiFont (12.0f)));
    value.setEditable (true, false, false); // single click on the readout: type a value
    value.setColour (juce::Label::textColourId, colours::text);
    value.setTooltip ("Click to type a value");
    value.onTextChange = [this] {
        if (param == nullptr)
            return;
        const float norm = param->getValueForText (value.getText());
        param->beginChangeGesture();
        param->setValueNotifyingHost (norm);
        param->endChangeGesture();
        refresh();
    };
    addAndMakeVisible (value);
    refresh();
}

void Knob::refresh()
{
    if (param == nullptr || value.isBeingEdited())
        return;
    const auto text = param->getCurrentValueAsText();
    if (text != value.getText())
        value.setText (text, juce::dontSendNotification);
}

void Knob::paint (juce::Graphics& g)
{
    auto r = getLocalBounds();
    g.setColour (colours::textDim);
    g.setFont (juce::Font (uiFont (10.5f, true)).withExtraKerningFactor (0.1f));
    g.drawText (title.toUpperCase(), r.removeFromTop (15), juce::Justification::centred);
    auto box = value.getBounds().toFloat().reduced (6.0f, 1.0f);
    g.setColour (colours::well);
    g.fillRoundedRectangle (box, 3.0f);
}

void Knob::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (15);
    value.setBounds (r.removeFromBottom (18));
    const int s = std::min (r.getWidth(), r.getHeight());
    slider.setBounds (r.withSizeKeepingCentre (s, s));
}

// ---------------------------------------------------------------------------------------------
namespace
{
constexpr float meterMin = -48.0f, meterMax = 6.0f;
}

void LevelMeter::push (float left, float right, int channels)
{
    numChannels = juce::jlimit (1, 2, channels);
    const float in[2] { left, right };
    for (int c = 0; c < 2; ++c)
    {
        const float d = juce::Decibels::gainToDecibels (in[c], -100.0f);
        level[c] = std::max (d, level[c] - 1.6f); // ~50 dB/s fall at 30 Hz
        if (d >= hold[c])
        {
            hold[c] = d;
            holdTicks[c] = 45;
        }
        else if (--holdTicks[c] <= 0)
            hold[c] = std::max (level[c], hold[c] - 1.0f);
    }
    repaint();
}

void LevelMeter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (colours::well);
    g.fillRoundedRectangle (r, 3.0f);
    auto inner = r.reduced (3.0f);
    auto yOf = [&] (float db) {
        const float t = juce::jlimit (0.0f, 1.0f, (db - meterMin) / (meterMax - meterMin));
        return inner.getBottom() - t * inner.getHeight();
    };
    const float gap = 2.0f, w = (inner.getWidth() - gap) / 2.0f;
    for (int c = 0; c < 2; ++c)
    {
        const int src = numChannels == 1 ? 0 : c;
        auto bar = juce::Rectangle<float> (inner.getX() + float (c) * (w + gap), inner.getY(), w, inner.getHeight());
        const float top = yOf (level[src]);
        juce::ColourGradient grad (colours::tone, 0.0f, yOf (meterMin), colours::heat, 0.0f, yOf (0.0f), false);
        grad.addColour (0.75, colours::heatHot);
        g.setGradientFill (grad);
        g.fillRect (bar.withTop (top));
        g.setColour (hold[src] > 0.0f ? colours::heat : colours::text.withAlpha (0.8f));
        g.fillRect (bar.withY (yOf (hold[src]) - 1.0f).withHeight (1.5f));
    }
    // 0 dB and -12 / -24 dB marks
    for (float db : { 0.0f, -12.0f, -24.0f, -36.0f })
    {
        g.setColour (db == 0.0f ? colours::text.withAlpha (0.5f) : colours::panelEdge.brighter (0.2f));
        g.fillRect (juce::Rectangle<float> (r.getX(), yOf (db), r.getWidth(), 1.0f));
    }
}

} // namespace craft::ddp::ui
