#include "Controls.h"

#include "Theme.h"
#include "plugin/ParameterLayout.h"

namespace craft::multiband::ui
{

namespace
{
float toDb (float lin) { return lin > 0.0f ? 20.0f * std::log10 (lin) : -200.0f; }

void beginAll (std::array<juce::RangedAudioParameter*, 4>& ps) { for (auto* p : ps) p->beginChangeGesture(); }
void endAll (std::array<juce::RangedAudioParameter*, 4>& ps) { for (auto* p : ps) p->endChangeGesture(); }
} // namespace

// ---------------------------------------------------------------------------------------------
ValueBox::ValueBox (juce::RangedAudioParameter& p, juce::String id, juce::String u)
    : param (p), paramId (std::move (id)), unit (std::move (u)), accent (colours::accent)
{
    label.setJustificationType (juce::Justification::centred);
    label.setFont (juce::Font (uiFont (12.5f, true)));
    label.setColour (juce::Label::textColourId, colours::text);
    label.setColour (juce::Label::textWhenEditingColourId, colours::text);
    label.setColour (juce::Label::backgroundWhenEditingColourId, colours::well);
    label.setColour (juce::Label::outlineWhenEditingColourId, colours::accent);
    label.setInterceptsMouseClicks (false, true);
    label.onTextChange = [this] {
        const float norm = param.getValueForText (label.getText().trim());
        param.beginChangeGesture();
        param.setValueNotifyingHost (norm);
        param.endChangeGesture();
        refresh();
    };
    label.onEditorHide = [this] { refresh(); };
    addAndMakeVisible (label);
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
    setTooltip (param.getName (64) + ": click to type, drag to adjust (Shift = fine), double-click to reset");
    refresh();
}

juce::String ValueBox::currentText() const
{
    auto t = formatValue (paramId, param.convertFrom0to1 (param.getValue()));
    if (unit.isNotEmpty() && t != "Soft" && t != "Hard")
        t << " " << unit;
    return t;
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
    if (suppressClick || (! dragging && e.getDistanceFromDragStart() < 3))
        return;
    if (! dragging)
    {
        dragging = gesture = true;
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
        startTimer (juce::MouseEvent::getDoubleClickTimeout() + 20);
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
    label.setText (formatValue (paramId, param.convertFrom0to1 (param.getValue())), juce::dontSendNotification);
    label.showEditor();
    if (auto* ed = label.getCurrentTextEditor())
    {
        ed->setJustification (juce::Justification::centred);
        ed->selectAll();
    }
}

// ---------------------------------------------------------------------------------------------
OffsetBox::OffsetBox (std::array<juce::RangedAudioParameter*, 4> ps, std::function<juce::String (double)> d)
    : params (ps), describe (std::move (d))
{
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
    setTooltip ("Drag to offset all bands together (differences are kept)");
}

void OffsetBox::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (colours::well);
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (isMouseOverOrDragging() ? colours::accent.withAlpha (0.8f) : colours::panelEdge);
    g.drawRoundedRectangle (r, 3.0f, 1.0f);
    g.setFont (juce::Font (uiFont (12.0f, true)));
    if (dragging)
    {
        g.setColour (colours::accent);
        g.drawText (text, getLocalBounds(), juce::Justification::centred);
    }
    else
    {
        // up/down chevrons: "drag me"
        g.setColour (colours::textDim);
        const auto c = r.getCentre();
        juce::Path p;
        p.addTriangle (c.x - 5.0f, c.y - 2.0f, c.x + 5.0f, c.y - 2.0f, c.x, c.y - 8.0f);
        p.addTriangle (c.x - 5.0f, c.y + 2.0f, c.x + 5.0f, c.y + 2.0f, c.x, c.y + 8.0f);
        g.fillPath (p);
    }
}

void OffsetBox::mouseDown (const juce::MouseEvent&)
{
    for (size_t i = 0; i < 4; ++i)
        start[i] = params[i]->getValue();
    dragging = true;
    text = describe (0.0);
    beginAll (params);
    repaint();
}

void OffsetBox::mouseDrag (const juce::MouseEvent& e)
{
    const float sensitivity = e.mods.isShiftDown() ? 0.0005f : 0.004f;
    float delta = -float (e.getDistanceFromDragStartY()) * sensitivity;
    // keep every band inside its range so that the differences are preserved
    float lo = -1.0f, hi = 1.0f;
    for (auto s : start)
    {
        lo = std::max (lo, -s);
        hi = std::min (hi, 1.0f - s);
    }
    delta = juce::jlimit (lo, hi, delta);
    for (size_t i = 0; i < 4; ++i)
        params[i]->setValueNotifyingHost (start[i] + delta);
    text = describe (double (delta));
    repaint();
}

void OffsetBox::mouseUp (const juce::MouseEvent&)
{
    endAll (params);
    dragging = false;
    repaint();
}

// ---------------------------------------------------------------------------------------------
PillToggle::PillToggle (const juce::String& text, juce::Colour on) : juce::Button (text), onColour (on)
{
    setClickingTogglesState (true);
}

void PillToggle::paintButton (juce::Graphics& g, bool over, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    const bool on = getToggleState();
    g.setColour (on ? onColour.withAlpha (down ? 0.7f : 0.85f) : (down ? colours::well.brighter (0.05f) : colours::well));
    g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
    g.setColour (over ? onColour : colours::panelEdge.brighter (0.2f));
    g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.0f);
    g.setColour (on ? colours::well : colours::textDim);
    g.setFont (juce::Font (uiFont (10.5f, true)));
    g.drawText (getButtonText(), getLocalBounds(), juce::Justification::centred);
}

// ---------------------------------------------------------------------------------------------
SegmentSwitch::SegmentSwitch (juce::RangedAudioParameter& p, juce::String off, juce::String onT)
    : param (p), offText (std::move (off)), onText (std::move (onT))
{
    setTooltip (param.getName (64));
    refresh();
}

void SegmentSwitch::refresh()
{
    const bool v = param.getValue() >= 0.5f;
    if (v != on)
    {
        on = v;
        repaint();
    }
}

void SegmentSwitch::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (colours::well);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (colours::panelEdge.brighter (0.2f));
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
    auto left = r.withWidth (r.getWidth() * 0.5f), right = left.withX (left.getRight());
    auto sel = (on ? right : left).reduced (2.0f);
    g.setColour (colours::accent.withAlpha (0.9f));
    g.fillRoundedRectangle (sel, 3.0f);
    g.setFont (juce::Font (uiFont (11.0f, true)));
    g.setColour (on ? colours::textDim : colours::well);
    g.drawText (offText, left, juce::Justification::centred);
    g.setColour (on ? colours::well : colours::textDim);
    g.drawText (onText, right, juce::Justification::centred);
}

void SegmentSwitch::mouseDown (const juce::MouseEvent& e)
{
    const bool wantOn = e.position.x >= float (getWidth()) * 0.5f;
    param.beginChangeGesture();
    param.setValueNotifyingHost (wantOn ? 1.0f : 0.0f);
    param.endChangeGesture();
    refresh();
}

// ---------------------------------------------------------------------------------------------
namespace
{
constexpr float meterTopDb = 0.0f, meterBottomDb = -80.0f;
} // namespace

ThresholdMeter::ThresholdMeter (juce::RangedAudioParameter& t, juce::Colour c) : threshold (t), colour (c)
{
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
    setTooltip ("Band level (sine-peak dBFS); drag the line to set the Threshold, double-click to reset");
}

juce::Rectangle<float> ThresholdMeter::barArea() const
{
    auto r = getLocalBounds().toFloat();
    r.removeFromLeft (22.0f); // scale
    return r.withTrimmedTop (6.0f).withTrimmedBottom (6.0f).withSizeKeepingCentre (std::min (r.getWidth(), 18.0f), r.getHeight() - 12.0f);
}

float ThresholdMeter::yForDb (float db) const
{
    const auto b = barArea();
    db = juce::jlimit (meterBottomDb, meterTopDb, db);
    return b.getY() + (meterTopDb - db) / (meterTopDb - meterBottomDb) * b.getHeight();
}

float ThresholdMeter::dbForY (float y) const
{
    const auto b = barArea();
    return meterTopDb - (y - b.getY()) / b.getHeight() * (meterTopDb - meterBottomDb);
}

void ThresholdMeter::pushLevel (float lin)
{
    const float db = toDb (lin);
    levelDb = std::max (db, levelDb - 1.0f);
    if (db >= holdDb)
    {
        holdDb = db;
        holdTicks = 45;
    }
    else if (--holdTicks <= 0)
        holdDb = std::max (db, holdDb - 1.0f);
    repaint();
}

void ThresholdMeter::paint (juce::Graphics& g)
{
    const auto b = barArea();
    // scale
    g.setFont (juce::Font (uiFont (9.5f)));
    for (int db = 0; db >= -80; db -= 10)
    {
        const float y = yForDb (float (db));
        g.setColour (colours::panelEdge.brighter (0.35f));
        g.drawHorizontalLine (int (y), b.getX() - 5.0f, b.getX() - 2.0f);
        if (db % 20 == 0)
        {
            g.setColour (colours::textDim);
            g.drawText (juce::String (db), juce::Rectangle<float> (0.0f, y - 6.0f, b.getX() - 6.0f, 12.0f),
                        juce::Justification::centredRight);
        }
    }
    g.setColour (colours::well);
    g.fillRoundedRectangle (b.expanded (2.0f), 3.0f);

    const float thr = threshold.convertFrom0to1 (threshold.getValue());
    if (levelDb > meterBottomDb)
    {
        const float top = yForDb (levelDb);
        const float thrY = yForDb (thr);
        // below threshold: dim band colour; above: full band colour
        g.setColour (colour.withAlpha (0.45f));
        g.fillRect (b.withTop (std::max (top, thrY)));
        if (top < thrY)
        {
            g.setColour (colour);
            g.fillRect (juce::Rectangle<float>::leftTopRightBottom (b.getX(), top, b.getRight(), thrY));
        }
    }
    if (holdDb > meterBottomDb)
    {
        g.setColour (colours::text.withAlpha (0.8f));
        g.fillRect (juce::Rectangle<float> (b.getX(), yForDb (holdDb) - 1.0f, b.getWidth(), 2.0f));
    }

    // threshold line with a grab triangle on the right
    const float y = yForDb (thr);
    g.setColour (dragging || isMouseOver() ? colours::text : colours::text.withAlpha (0.85f));
    g.fillRect (juce::Rectangle<float> (b.getX() - 3.0f, y - 1.0f, b.getWidth() + 6.0f, 2.0f));
    juce::Path tri;
    tri.addTriangle (b.getRight() + 3.0f, y, b.getRight() + 11.0f, y - 6.0f, b.getRight() + 11.0f, y + 6.0f);
    g.fillPath (tri);
}

void ThresholdMeter::mouseDown (const juce::MouseEvent& e)
{
    dragging = true;
    threshold.beginChangeGesture();
    mouseDrag (e);
}

void ThresholdMeter::mouseDrag (const juce::MouseEvent& e)
{
    const float db = juce::jlimit (-80.0f, 0.0f, dbForY (e.position.y));
    threshold.setValueNotifyingHost (threshold.convertTo0to1 (db));
    repaint();
}

void ThresholdMeter::mouseUp (const juce::MouseEvent&)
{
    if (dragging)
        threshold.endChangeGesture();
    dragging = false;
    repaint();
}

void ThresholdMeter::mouseDoubleClick (const juce::MouseEvent&)
{
    threshold.beginChangeGesture();
    threshold.setValueNotifyingHost (threshold.getDefaultValue());
    threshold.endChangeGesture();
    repaint();
}

// ---------------------------------------------------------------------------------------------
OutputSection::OutputSection (juce::AudioProcessorValueTreeState& state, const juce::String& id)
    : box (*state.getParameter (id), id, "dB"), attachment (state, id, fader)
{
    fader.setSliderStyle (juce::Slider::LinearVertical);
    fader.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    fader.setSliderSnapsToMousePosition (false);
    fader.setMouseDragSensitivity (240);
    fader.setColour (juce::Slider::thumbColourId, colours::accent);
    auto* p = state.getParameter (id);
    fader.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()), juce::ModifierKeys::altModifier);
    addAndMakeVisible (fader);
    addAndMakeVisible (box);
}

void OutputSection::setChannels (int n)
{
    channels = juce::jlimit (1, 2, n);
}

void OutputSection::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (18);
    box.setBounds (r.removeFromBottom (22).withSizeKeepingCentre (std::min (r.getWidth(), 72), 22));
    r.removeFromBottom (6);
    auto left = r.removeFromLeft (r.getWidth() / 2);
    fader.setBounds (left.withTrimmedLeft (18));
    meterArea = r.reduced (4, 0);
}

float OutputSection::meterY (float db) const
{
    db = juce::jlimit (-60.0f, 0.0f, db);
    return float (meterArea.getY()) + 8.0f + (-db / 60.0f) * float (meterArea.getHeight() - 16);
}

void OutputSection::pushLevels (float l, float r)
{
    const float in[2] = { l, r };
    for (size_t c = 0; c < 2; ++c)
    {
        const float db = toDb (in[c]);
        levelDb[c] = std::max (db, levelDb[c] - 1.0f);
        if (db >= holdDb[c])
        {
            holdDb[c] = db;
            holdTicks[c] = 45;
        }
        else if (--holdTicks[c] <= 0)
            holdDb[c] = std::max (db, holdDb[c] - 1.0f);
    }
    box.refresh();
    repaint (meterArea.expanded (2));
}

void OutputSection::paint (juce::Graphics& g)
{
    drawTitle (g, getLocalBounds().removeFromTop (16), "Output");

    // fader scale +18 ... -18
    const auto fb = fader.getBounds();
    g.setFont (juce::Font (uiFont (9.5f)));
    for (int db = 18; db >= -18; db -= 6)
    {
        const float y = float (fb.getY()) + float (fader.getPositionOfValue (double (db)));
        g.setColour (colours::panelEdge.brighter (0.35f));
        g.drawHorizontalLine (int (y), float (fb.getX() + 2), float (fb.getX() + 6));
        g.setColour (colours::textDim);
        g.drawText ((db > 0 ? "+" : "") + juce::String (db), juce::Rectangle<float> (0.0f, y - 6.0f, float (fb.getX()) - 1.0f, 12.0f),
                    juce::Justification::centredRight);
    }

    // meters 0 ... -60 dBFS
    const float gap = 3.0f;
    const auto ma = meterArea.toFloat();
    const float w = channels == 2 ? (ma.getWidth() - gap) / 2.0f : ma.getWidth() * 0.6f;
    for (int c = 0; c < channels; ++c)
    {
        const float x = channels == 2 ? ma.getX() + float (c) * (w + gap) : ma.getCentreX() - w / 2.0f;
        const auto bar = juce::Rectangle<float>::leftTopRightBottom (x, meterY (0.0f), x + w, meterY (-60.0f));
        g.setColour (colours::well);
        g.fillRoundedRectangle (bar.expanded (1.5f), 2.0f);
        if (levelDb[size_t (c)] > -60.0f)
        {
            juce::ColourGradient grad (colours::signal.darker (0.3f), 0.0f, bar.getBottom(), colours::signal.brighter (0.2f), 0.0f,
                                       bar.getY(), false);
            g.setGradientFill (grad);
            g.fillRect (bar.withTop (meterY (levelDb[size_t (c)])));
            if (levelDb[size_t (c)] > 0.0f)
            {
                g.setColour (colours::warn);
                g.fillRect (bar.withHeight (3.0f));
            }
        }
        if (holdDb[size_t (c)] > -60.0f)
        {
            g.setColour (colours::text.withAlpha (0.8f));
            g.fillRect (juce::Rectangle<float> (bar.getX(), meterY (holdDb[size_t (c)]) - 1.0f, bar.getWidth(), 2.0f));
        }
    }
}

} // namespace craft::multiband::ui
