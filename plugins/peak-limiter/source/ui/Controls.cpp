#include "Controls.h"

#include "LimiterLookAndFeel.h"
#include "plugin/ParameterLayout.h"

namespace craft::limiter::ui
{

namespace
{
constexpr float scaleMinDb = -30.0f;

float toDb (float lin) { return lin > 0.0f ? 20.0f * std::log10 (lin) : -200.0f; }
} // namespace

// ---------------------------------------------------------------------------------------------
ValueBox::ValueBox (juce::RangedAudioParameter& p, juce::String id, bool f) : param (p), paramId (std::move (id)), framed (f)
{
    label.setJustificationType (juce::Justification::centred);
    label.setFont (juce::Font (uiFont (13.0f, true)));
    label.setColour (juce::Label::textColourId, colours::text);
    label.setColour (juce::Label::textWhenEditingColourId, colours::text);
    label.setColour (juce::Label::backgroundWhenEditingColourId, colours::well);
    label.setColour (juce::Label::outlineWhenEditingColourId, colours::accent);
    label.setInterceptsMouseClicks (false, true);
    label.onTextChange = [this] {
        const float norm = param.getValueForText (label.getText());
        param.beginChangeGesture();
        param.setValueNotifyingHost (norm);
        param.endChangeGesture();
        refresh();
    };
    label.onEditorHide = [this] { refresh(); };
    addAndMakeVisible (label);
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
    setTooltip ("Click to type, drag to adjust, double-click to reset");
    refresh();
}

void ValueBox::setFontHeight (float h) { label.setFont (juce::Font (uiFont (h, true))); }

juce::String ValueBox::currentText() const
{
    return formatValue (paramId, param.convertFrom0to1 (param.getValue()));
}

void ValueBox::refresh()
{
    if (! label.isBeingEdited())
    {
        const auto t = currentText();
        if (label.getText() != t)
            label.setText (t, juce::dontSendNotification);
    }
}

void ValueBox::paint (juce::Graphics& g)
{
    if (! framed)
        return;
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (colours::well);
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (isMouseOverOrDragging() ? colours::accent.withAlpha (0.7f) : colours::panelEdge);
    g.drawRoundedRectangle (r, 3.0f, 1.0f);
}

void ValueBox::resized() { label.setBounds (getLocalBounds()); }

void ValueBox::resetToDefault()
{
    param.beginChangeGesture();
    param.setValueNotifyingHost (param.getDefaultValue());
    param.endChangeGesture();
    refresh();
}

void ValueBox::mouseDown (const juce::MouseEvent& e)
{
    stopTimer();
    dragging = false;
    suppressClick = false;
    dragStart = param.getValue();
    if (e.mods.isAltDown())
    {
        resetToDefault();
        suppressClick = true;
    }
}

void ValueBox::mouseDrag (const juce::MouseEvent& e)
{
    if (suppressClick)
        return;
    if (! dragging && e.getDistanceFromDragStart() < 3)
        return;
    if (! dragging)
    {
        dragging = true;
        gesture = true;
        param.beginChangeGesture();
    }
    const float sensitivity = e.mods.isShiftDown() ? 0.0005f : 0.005f; // 200 px for the full range
    const float v = juce::jlimit (0.0f, 1.0f, dragStart - float (e.getDistanceFromDragStartY()) * sensitivity);
    param.setValueNotifyingHost (v);
    refresh();
}

void ValueBox::mouseUp (const juce::MouseEvent&)
{
    if (gesture)
    {
        param.endChangeGesture();
        gesture = false;
    }
    if (! dragging && ! suppressClick)
        startTimer (juce::MouseEvent::getDoubleClickTimeout() + 20); // open the editor unless a double-click follows
    dragging = false;
}

void ValueBox::mouseDoubleClick (const juce::MouseEvent&)
{
    stopTimer();
    resetToDefault();
}

void ValueBox::timerCallback()
{
    stopTimer();
    label.showEditor();
    if (auto* ed = label.getCurrentTextEditor())
    {
        ed->setJustification (juce::Justification::centred);
        ed->selectAll();
    }
}

// ---------------------------------------------------------------------------------------------
MeterFader::MeterFader (juce::AudioProcessorValueTreeState& state, const juce::String& id, const juce::String& t,
                        const juce::String& ml, juce::Colour handle)
    : box (*state.getParameter (id), id), title (t), meterLabel (ml), handleColour (handle), attachment (state, id, slider)
{
    slider.setSliderStyle (juce::Slider::LinearVertical);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    slider.setSliderSnapsToMousePosition (false);
    slider.setMouseDragSensitivity (240);
    slider.setColour (juce::Slider::thumbColourId, handleColour);
    slider.getProperties().set (LimiterLookAndFeel::overlayProperty, true);
    auto* p = state.getParameter (id);
    slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()), juce::ModifierKeys::altModifier);
    slider.setPopupDisplayEnabled (false, false, nullptr);
    addAndMakeVisible (slider);
    addAndMakeVisible (box);
}

void MeterFader::setChannels (int n)
{
    n = juce::jlimit (1, 2, n);
    if (n != channels)
    {
        channels = n;
        repaint();
    }
}

void MeterFader::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (22);
    box.setBounds (r.removeFromBottom (24).withSizeKeepingCentre (64, 24));
    r.removeFromBottom (18); // meter label row
    r.removeFromLeft (26);   // scale labels
    slider.setBounds (r.withWidth (std::min (r.getWidth(), 70)));
}

float MeterFader::yForDb (float db) const
{
    db = juce::jlimit (scaleMinDb, 0.0f, db);
    return float (slider.getY()) + float (slider.getPositionOfValue (db));
}

juce::Rectangle<float> MeterFader::barRect (int ch) const
{
    const auto sb = slider.getBounds().toFloat();
    const float left = sb.getX() + 28.0f, right = sb.getRight() - 4.0f;
    const float top = yForDb (0.0f), bottom = yForDb (scaleMinDb);
    const float gap = 4.0f;
    const float w = channels == 2 ? (right - left - gap) / 2.0f : (right - left) * 0.6f;
    const float x = channels == 2 ? left + float (ch) * (w + gap) : left + (right - left - w) / 2.0f;
    return juce::Rectangle<float>::leftTopRightBottom (x, top, x + w, bottom);
}

void MeterFader::pushLevels (float p1, float p2)
{
    const float in[2] = { p1, p2 };
    for (int c = 0; c < 2; ++c)
    {
        const float db = toDb (in[c]);
        levelDb[c] = std::max (db, levelDb[c] - 0.9f); // ~27 dB/s fall at 30 Hz refresh
        if (db >= holdDb[c])
        {
            holdDb[c] = db;
            holdTicks[c] = 45; // 1.5 s
        }
        else if (--holdTicks[c] <= 0)
            holdDb[c] = std::max (db, holdDb[c] - 0.9f);
    }
    repaint (slider.getBounds().expanded (2));
}

void MeterFader::paint (juce::Graphics& g)
{
    drawTitle (g, getLocalBounds().removeFromTop (18), title);

    const auto sb = slider.getBounds();
    // scale: 0 / 6 / 12 / 18 / 24 / 30
    g.setFont (juce::Font (uiFont (10.5f)));
    for (int db = 0; db >= -30; db -= 3)
    {
        const float y = yForDb (float (db));
        g.setColour (colours::panelEdge.brighter (0.35f));
        g.drawHorizontalLine (int (y), float (sb.getX() - 6), float (sb.getX() - 2));
        if (db % 6 == 0)
        {
            g.setColour (colours::textDim);
            g.drawText (juce::String (-db), juce::Rectangle<int> (0, int (y) - 7, sb.getX() - 8, 14),
                        juce::Justification::centredRight);
        }
    }

    // meter wells and bars
    for (int c = 0; c < channels; ++c)
    {
        const auto b = barRect (c);
        g.setColour (colours::well);
        g.fillRoundedRectangle (b.expanded (2.0f), 3.0f);
        for (int db = -6; db > -30; db -= 6)
        {
            g.setColour (colours::panel.withAlpha (0.9f));
            g.drawHorizontalLine (int (yForDb (float (db))), b.getX(), b.getRight());
        }
        if (levelDb[c] > scaleMinDb)
        {
            const auto fill = b.withTop (yForDb (levelDb[c]));
            juce::ColourGradient grad (colours::accent2.darker (0.3f), 0.0f, b.getBottom(), colours::accent2.brighter (0.2f),
                                       0.0f, b.getY(), false);
            g.setGradientFill (grad);
            g.fillRect (fill);
            if (levelDb[c] > 0.0f)
            {
                g.setColour (colours::warn);
                g.fillRect (b.withHeight (3.0f));
            }
        }
        if (holdDb[c] > scaleMinDb)
        {
            g.setColour (colours::text.withAlpha (0.8f));
            g.fillRect (juce::Rectangle<float> (b.getX(), yForDb (holdDb[c]) - 1.0f, b.getWidth(), 2.0f));
        }
    }

    // meter label under the bars ("IN L R" / "OUT")
    auto labelRow = juce::Rectangle<int> (sb.getX(), sb.getBottom() + 1, sb.getWidth(), 16);
    g.setFont (juce::Font (uiFont (10.5f, true)));
    g.setColour (colours::textDim);
    if (channels == 2)
    {
        for (int c = 0; c < 2; ++c)
        {
            const auto b = barRect (c);
            g.drawText (c == 0 ? "L" : "R", juce::Rectangle<float> (b.getX(), float (labelRow.getY()), b.getWidth(), 14.0f),
                        juce::Justification::centred);
        }
        g.drawText (meterLabel, labelRow.withWidth (26), juce::Justification::centredLeft);
    }
    else
    {
        g.drawText (meterLabel, labelRow, juce::Justification::centred);
    }
}

// ---------------------------------------------------------------------------------------------
ReleaseKnob::ReleaseKnob (juce::AudioProcessorValueTreeState& state, const juce::String& id)
    : box (*state.getParameter (id), id, false), attachment (state, id, slider)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    slider.setRotaryParameters (juce::degreesToRadians (-135.0f), juce::degreesToRadians (135.0f), true);
    slider.setMouseDragSensitivity (260);
    auto* p = state.getParameter (id);
    slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()), juce::ModifierKeys::altModifier);
    addAndMakeVisible (slider);
    box.setFontHeight (15.0f);
    addAndMakeVisible (box);
    unit.setText ("ms", juce::dontSendNotification);
    unit.setFont (juce::Font (uiFont (10.0f, true)));
    unit.setColour (juce::Label::textColourId, colours::textDim);
    unit.setJustificationType (juce::Justification::centred);
    unit.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (unit);
}

void ReleaseKnob::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (22);
    auto labels = r.removeFromBottom (16);
    juce::ignoreUnused (labels);
    const int d = std::min (r.getWidth(), r.getHeight());
    slider.setBounds (r.withSizeKeepingCentre (d, d));
    box.setBounds (juce::Rectangle<int> (d / 2, 22).withCentre (slider.getBounds().getCentre().translated (0, -3)));
    unit.setBounds (juce::Rectangle<int> (40, 12).withCentre (slider.getBounds().getCentre().translated (0, 14)));
}

void ReleaseKnob::paint (juce::Graphics& g)
{
    drawTitle (g, getLocalBounds().removeFromTop (18), "Release");
    const auto sb = slider.getBounds().toFloat();
    g.setColour (colours::textDim);

    // end labels at the arc ends
    const float r = sb.getWidth() * 0.5f - 4.0f;
    const auto c = sb.getCentre();
    const float a0 = juce::degreesToRadians (-135.0f), a1 = juce::degreesToRadians (135.0f);
    g.setFont (juce::Font (uiFont (10.5f)));
    const auto p0 = juce::Point<float> (c.x + r * std::sin (a0), c.y - r * std::cos (a0));
    const auto p1 = juce::Point<float> (c.x + r * std::sin (a1), c.y - r * std::cos (a1));
    g.drawText ("0.01", juce::Rectangle<float> (40.0f, 14.0f).withCentre (p0.translated (-2.0f, 12.0f)),
                juce::Justification::centred);
    g.drawText ("1000", juce::Rectangle<float> (40.0f, 14.0f).withCentre (p1.translated (2.0f, 12.0f)),
                juce::Justification::centred);
}

// ---------------------------------------------------------------------------------------------
LampSwitch::LampSwitch (const juce::String& text) : juce::Button (text)
{
    setClickingTogglesState (true);
}

void LampSwitch::paintButton (juce::Graphics& g, bool over, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    const bool on = getToggleState();
    g.setColour (down ? colours::well.brighter (0.05f) : colours::well);
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (over ? colours::accent.withAlpha (0.7f) : colours::panelEdge.brighter (0.2f));
    g.drawRoundedRectangle (r, 5.0f, 1.0f);

    // lamp
    auto lamp = r.removeFromLeft (r.getHeight()).reduced (r.getHeight() * 0.3f);
    if (on)
    {
        g.setColour (colours::accent.withAlpha (0.25f));
        g.fillEllipse (lamp.expanded (3.0f));
    }
    g.setColour (on ? colours::accent : colours::panelEdge);
    g.fillEllipse (lamp);
    g.setColour (juce::Colour (0xff0b0d0f));
    g.drawEllipse (lamp, 1.0f);

    g.setColour (on ? colours::text : colours::textDim);
    g.setFont (juce::Font (uiFont (11.5f, true)));
    g.drawText (getButtonText(), r.withTrimmedRight (6.0f), juce::Justification::centred);
}

} // namespace craft::limiter::ui
