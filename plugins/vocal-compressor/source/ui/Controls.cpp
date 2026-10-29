#include "Controls.h"

#include "VocalLookAndFeel.h"
#include "plugin/ParameterLayout.h"

namespace craft::vocal::ui
{

namespace
{
constexpr int titleH = 18, boxH = 24, readoutH = 24, gapH = 6, scaleW = 26;

float toDb (float lin) { return lin > 0.0f ? 20.0f * std::log10 (lin) : -200.0f; }

juce::String dbText (float db, bool showSign = true)
{
    if (db <= -99.95f)
        return "-Inf";
    const auto s = juce::String (db, 1);
    return showSign && db > 0.0f ? "+" + s : s;
}
} // namespace

// ---------------------------------------------------------------------------------------------
ValueBox::ValueBox (juce::RangedAudioParameter& p, juce::String id, juce::Colour a) : param (p), paramId (std::move (id)), accent (a)
{
    label.setJustificationType (juce::Justification::centred);
    label.setFont (juce::Font (uiFont (13.0f, true)));
    label.setColour (juce::Label::textColourId, colours::text);
    label.setColour (juce::Label::textWhenEditingColourId, colours::text);
    label.setColour (juce::Label::backgroundWhenEditingColourId, colours::well);
    label.setColour (juce::Label::outlineWhenEditingColourId, accent);
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

void ValueBox::refresh()
{
    if (! label.isBeingEdited())
    {
        const auto t = formatValue (paramId, param.convertFrom0to1 (param.getValue()));
        if (label.getText() != t)
            label.setText (t, juce::dontSendNotification);
    }
}

void ValueBox::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (colours::well);
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (isMouseOverOrDragging() ? accent.withAlpha (0.8f) : colours::panelEdge);
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
    const float sensitivity = e.mods.isShiftDown() ? 0.0005f : 0.004f;
    param.setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, dragStart - float (e.getDistanceFromDragStartY()) * sensitivity));
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
void LinkSlider::mouseDown (const juce::MouseEvent& e)
{
    linking = partner != nullptr && e.mods.isCommandDown() && ! e.mods.isAltDown();
    startValue = getValue();
    if (linking)
    {
        partnerStart = partner->convertFrom0to1 (partner->getValue());
        partner->beginChangeGesture();
    }
    Slider::mouseDown (e);
}

void LinkSlider::mouseDrag (const juce::MouseEvent& e)
{
    Slider::mouseDrag (e);
    if (linking)
    {
        const double delta = getValue() - startValue;
        const auto& range = partner->getNormalisableRange();
        const double v = juce::jlimit (double (range.start), double (range.end), partnerStart + delta);
        partner->setValueNotifyingHost (partner->convertTo0to1 (float (v)));
    }
}

void LinkSlider::mouseUp (const juce::MouseEvent& e)
{
    Slider::mouseUp (e);
    if (linking)
        partner->endChangeGesture();
    linking = false;
}

// ---------------------------------------------------------------------------------------------
FaderStrip::FaderStrip (juce::AudioProcessorValueTreeState& state, const juce::String& id, const juce::String& t, Meter m,
                        juce::Colour a)
    : box (*state.getParameter (id), id, a), title (t), meter (m), accent (a), attachment (state, id, slider)
{
    slider.setSliderStyle (juce::Slider::LinearVertical);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    slider.setSliderSnapsToMousePosition (false);
    slider.setMouseDragSensitivity (300);
    slider.setVelocityModeParameters (1.0, 1, 0.0, false); // Ctrl/Cmd is the linking modifier, not velocity mode
    slider.setColour (juce::Slider::thumbColourId, accent);
    auto* p = state.getParameter (id);
    slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()), juce::ModifierKeys::altModifier);
    slider.setPopupDisplayEnabled (false, false, nullptr);
    bottomValue = p->getNormalisableRange().start;
    addAndMakeVisible (slider);
    addAndMakeVisible (box);

    if (meter == Meter::gainReduction)
        slider.setTooltip ("Compression (Ctrl/Cmd-drag moves Gain along, double-click resets)");
    else if (meter == Meter::outputPeaks)
        slider.setTooltip ("Output gain (Ctrl/Cmd-drag moves Compression along, double-click resets)");
    else
        slider.setTooltip ("Gate threshold (bottom = off, double-click resets)");
}

void FaderStrip::setChannels (int n)
{
    n = juce::jlimit (1, 2, n);
    if (n != channels)
    {
        channels = n;
        repaint();
    }
}

void FaderStrip::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (titleH + 2);
    box.setBounds (r.removeFromTop (boxH).withSizeKeepingCentre (70, boxH));
    r.removeFromTop (gapH + 8);
    r.removeFromBottom (readoutH + gapH + 16); // readouts + label row
    r.removeFromLeft (scaleW);
    r.removeFromRight (6);
    slider.setBounds (r);
}

juce::Rectangle<int> FaderStrip::readoutArea() const { return getLocalBounds().removeFromBottom (readoutH); }

float FaderStrip::yForValue (float v) const
{
    v = juce::jlimit (bottomValue, 0.0f, v);
    return float (slider.getY()) + float (slider.getPositionOfValue (v));
}

juce::Rectangle<float> FaderStrip::meterRect() const
{
    const auto sb = slider.getBounds().toFloat();
    const float w = meter == Meter::gainReduction ? std::min (sb.getWidth() - 20.0f, 40.0f) : std::min (sb.getWidth() - 16.0f, 34.0f);
    return juce::Rectangle<float>::leftTopRightBottom (sb.getCentreX() - w * 0.5f, yForValue (0.0f), sb.getCentreX() + w * 0.5f,
                                                       yForValue (bottomValue));
}

juce::Rectangle<float> FaderStrip::barRect (int ch) const
{
    const auto m = meterRect();
    if (meter != Meter::outputPeaks || channels == 1)
        return m;
    const float gap = 4.0f, w = (m.getWidth() - gap) * 0.5f;
    return m.withWidth (w).translated (float (ch) * (w + gap), 0.0f);
}

void FaderStrip::pushInputEnergy (float meanSquare)
{
    const float db = meanSquare > 0.0f ? 10.0f * std::log10 (meanSquare) : -200.0f;
    levelDb[0] = std::max (db, levelDb[0] - 1.0f); // ~30 dB/s fall at 30 Hz refresh
    repaint();
}

void FaderStrip::pushGainReduction (float minGain)
{
    const float gr = minGain < 1.0f ? -toDb (std::max (minGain, 1.0e-9f)) : 0.0f;
    grDb = std::max (gr, grDb - 1.0f);
    if (grDb < 0.05f)
        grDb = 0.0f;
    grPeakDb = std::max (grPeakDb, gr);
    repaint();
}

void FaderStrip::pushOutputPeaks (float l, float r)
{
    const float in[2] = { l, r };
    for (int c = 0; c < 2; ++c)
    {
        const float db = toDb (in[c]);
        levelDb[c] = std::max (db, levelDb[c] - 0.9f);
        peakDb[c] = std::max (peakDb[c], db);
    }
    repaint();
}

void FaderStrip::resetPeaks()
{
    grPeakDb = 0.0f;
    peakDb[0] = peakDb[1] = -200.0f;
    repaint();
}

void FaderStrip::mouseDown (const juce::MouseEvent& e)
{
    if (readoutArea().contains (e.getPosition()) && meter != Meter::inputEnergy)
        resetPeaks();
}

void FaderStrip::paintScale (juce::Graphics& g)
{
    const auto sb = slider.getBounds();
    g.setFont (juce::Font (uiFont (10.5f)));
    const int step = meter == Meter::inputEnergy ? 12 : 6;
    const int bottom = int (std::round (-bottomValue));
    for (int db = 0; db <= bottom; db += (meter == Meter::inputEnergy ? 6 : 3))
    {
        const float y = yForValue (float (-db));
        g.setColour (colours::panelEdge.brighter (0.35f));
        g.drawHorizontalLine (int (y), float (sb.getX() - 6), float (sb.getX() - 2));
        const bool labelled = db % step == 0 && (meter != Meter::inputEnergy || (db >= 12 && db <= 72));
        if (labelled || db == 0)
        {
            g.setColour (colours::textDim);
            g.drawText (juce::String (db), juce::Rectangle<int> (0, int (y) - 7, sb.getX() - 8, 14),
                        juce::Justification::centredRight);
        }
    }
    if (meter == Meter::inputEnergy)
    {
        g.setColour (colours::textDim);
        g.drawText ("OFF", juce::Rectangle<int> (0, int (yForValue (bottomValue)) - 7, sb.getX() - 4, 14),
                    juce::Justification::centredRight);
    }
}

void FaderStrip::paint (juce::Graphics& g)
{
    drawTitle (g, getLocalBounds().removeFromTop (titleH), title);
    paintScale (g);

    const auto m = meterRect();
    const int bars = meter == Meter::outputPeaks ? channels : 1;
    for (int c = 0; c < bars; ++c)
    {
        const auto b = barRect (c);
        g.setColour (colours::well);
        g.fillRoundedRectangle (b.expanded (2.0f), 3.0f);
        const int step = meter == Meter::inputEnergy ? 12 : 6;
        for (int db = step; db < int (-bottomValue); db += step)
        {
            g.setColour (colours::panel.withAlpha (0.9f));
            g.drawHorizontalLine (int (yForValue (float (-db))), b.getX(), b.getRight());
        }

        if (meter == Meter::gainReduction)
        {
            if (grDb > 0.0f)
            {
                const auto fill = b.withBottom (yForValue (-grDb));
                juce::ColourGradient grad (accent.withAlpha (0.85f), 0.0f, b.getY(), colours::warn.withAlpha (0.9f), 0.0f,
                                           b.getBottom(), false);
                g.setGradientFill (grad);
                g.fillRect (fill.reduced (3.0f, 0.0f));
            }
            if (grPeakDb > 0.05f)
            {
                g.setColour (colours::text.withAlpha (0.8f));
                g.fillRect (juce::Rectangle<float> (b.getX() + 3.0f, yForValue (-grPeakDb) - 1.0f, b.getWidth() - 6.0f, 2.0f));
            }
        }
        else
        {
            const float lv = levelDb[c];
            if (lv > bottomValue)
            {
                const auto fill = b.withTop (yForValue (lv));
                juce::ColourGradient grad (colours::signal.darker (0.4f), 0.0f, b.getBottom(), colours::signal.brighter (0.2f),
                                           0.0f, b.getY(), false);
                g.setGradientFill (grad);
                g.fillRect (fill);
                if (meter == Meter::outputPeaks && lv > 0.0f)
                {
                    g.setColour (colours::warn);
                    g.fillRect (b.withHeight (3.0f));
                }
            }
            if (meter == Meter::outputPeaks && peakDb[c] > bottomValue)
            {
                g.setColour (colours::text.withAlpha (0.8f));
                g.fillRect (juce::Rectangle<float> (b.getX(), yForValue (peakDb[c]) - 1.0f, b.getWidth(), 2.0f));
            }
        }
    }
    juce::ignoreUnused (m);

    // label row + readouts
    auto labelRow = getLocalBounds().withTrimmedBottom (readoutH + 2).removeFromBottom (16);
    g.setColour (colours::textDim);
    g.setFont (juce::Font (uiFont (10.5f, true)));
    auto ro = readoutArea().toFloat();
    if (meter == Meter::inputEnergy)
    {
        g.drawText ("INPUT", labelRow, juce::Justification::centred);
        drawReadout (g, ro.withSizeKeepingCentre (70.0f, ro.getHeight()), dbText (levelDb[0]), colours::panelEdge);
    }
    else if (meter == Meter::gainReduction)
    {
        g.drawText ("PEAK REDUCTION", labelRow, juce::Justification::centred);
        drawReadout (g, ro.withSizeKeepingCentre (70.0f, ro.getHeight()),
                     grPeakDb > 0.05f ? "-" + juce::String (grPeakDb, 1) : juce::String ("0.0"),
                     grPeakDb > 0.05f ? accent.withAlpha (0.8f) : colours::panelEdge);
    }
    else
    {
        if (channels == 2)
        {
            g.drawText ("L", barRect (0).withY (float (labelRow.getY())).withHeight (16.0f), juce::Justification::centred);
            g.drawText ("R", barRect (1).withY (float (labelRow.getY())).withHeight (16.0f), juce::Justification::centred);
            const float w = (ro.getWidth() - 6.0f) * 0.5f;
            for (int c = 0; c < 2; ++c)
                drawReadout (g, ro.withWidth (w).translated (float (c) * (w + 6.0f), 0.0f), dbText (peakDb[c]),
                             peakDb[c] > 0.0f ? colours::warn : colours::panelEdge);
        }
        else
        {
            g.drawText ("OUT", labelRow, juce::Justification::centred);
            drawReadout (g, ro.withSizeKeepingCentre (70.0f, ro.getHeight()), dbText (peakDb[0]),
                         peakDb[0] > 0.0f ? colours::warn : colours::panelEdge);
        }
    }
}

} // namespace craft::vocal::ui
