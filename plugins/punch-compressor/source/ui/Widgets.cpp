#include "Widgets.h"

#include "PunchLookAndFeel.h"
#include "plugin/ParameterLayout.h"

namespace craft::punch::ui
{

// ---------------------------------------------------------------------------------------------
ValueBox::ValueBox (juce::RangedAudioParameter& p, juce::String id, juce::String u)
    : param (p), paramId (std::move (id)), unit (std::move (u))
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
    setTooltip ("Click to type, drag to adjust, double-click or Alt-click to reset");
    refresh();
}

void ValueBox::setFontHeight (float h) { label.setFont (juce::Font (uiFont (h, true))); }

juce::String ValueBox::currentText() const
{
    auto t = formatValue (paramId, param.convertFrom0to1 (param.getValue()));
    if (decorate)
        return decorate (t);
    return unit.isEmpty() ? t : t + " " + unit;
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
    const float sensitivity = e.mods.isShiftDown() ? 0.0005f : 0.005f;
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
        startTimer (juce::MouseEvent::getDoubleClickTimeout() + 20); // edit unless a double-click follows
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
        ed->setText (formatValue (paramId, param.convertFrom0to1 (param.getValue())), false);
        ed->selectAll();
    }
}

// ---------------------------------------------------------------------------------------------
Knob::Knob (juce::AudioProcessorValueTreeState& state, const juce::String& id, const juce::String& t, const juce::String& unit,
            bool isLarge)
    : box (*state.getParameter (id), id, unit), attachment (state, id, slider), title (t), large (isLarge)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    slider.setRotaryParameters (juce::degreesToRadians (-140.0f), juce::degreesToRadians (140.0f), true);
    slider.setMouseDragSensitivity (large ? 320 : 240);
    auto* p = state.getParameter (id);
    slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()), juce::ModifierKeys::altModifier);
    slider.setPopupDisplayEnabled (false, false, nullptr);
    addAndMakeVisible (slider);
    if (large)
        box.setFontHeight (16.0f);
    addAndMakeVisible (box);
}

void Knob::setAccent (juce::Colour c)
{
    slider.setColour (juce::Slider::thumbColourId, c);
    repaint();
}

void Knob::setTitle (const juce::String& t)
{
    title = t;
    repaint();
}

void Knob::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (large ? 20 : 18);
    const int boxH = large ? 26 : 22;
    box.setBounds (r.removeFromBottom (boxH).withSizeKeepingCentre (large ? 110 : 78, boxH));
    r.removeFromBottom (4);
    const int d = std::min (r.getWidth(), r.getHeight());
    slider.setBounds (r.withSizeKeepingCentre (d, d));
}

void Knob::paint (juce::Graphics& g)
{
    drawTitle (g, getLocalBounds().removeFromTop (large ? 18 : 16), title);
}

// ---------------------------------------------------------------------------------------------
Selector::Selector (juce::RangedAudioParameter& p, juce::StringArray l, juce::Colour a) : param (p), labels (std::move (l)), accent (a)
{
    refresh();
}

int Selector::getIndex() const
{
    const int n = labels.size();
    return juce::jlimit (0, n - 1, juce::roundToInt (param.getValue() * float (n - 1)));
}

void Selector::refresh()
{
    const int i = getIndex();
    if (i != shown)
    {
        shown = i;
        repaint();
    }
}

int Selector::segmentAt (juce::Point<int> p) const
{
    const int n = labels.size();
    return juce::jlimit (0, n - 1, p.x * n / std::max (1, getWidth()));
}

void Selector::mouseDown (const juce::MouseEvent& e)
{
    const int n = labels.size();
    const int i = segmentAt (e.getPosition());
    param.beginChangeGesture();
    param.setValueNotifyingHost (n > 1 ? float (i) / float (n - 1) : 0.0f);
    param.endChangeGesture();
    refresh();
}

void Selector::mouseMove (const juce::MouseEvent& e)
{
    const int h = segmentAt (e.getPosition());
    if (h != hover)
    {
        hover = h;
        repaint();
    }
}

void Selector::mouseExit (const juce::MouseEvent&)
{
    hover = -1;
    repaint();
}

void Selector::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (colours::well);
    g.fillRoundedRectangle (r, 4.0f);
    const int n = labels.size();
    const float w = r.getWidth() / float (n);
    g.setFont (juce::Font (uiFont (11.0f, true)));
    for (int i = 0; i < n; ++i)
    {
        auto seg = juce::Rectangle<float> (r.getX() + w * float (i), r.getY(), w, r.getHeight()).reduced (2.0f);
        const bool on = i == shown;
        if (on)
        {
            g.setColour (accent.withAlpha (0.22f));
            g.fillRoundedRectangle (seg, 3.0f);
            g.setColour (accent);
            g.fillRoundedRectangle (seg.removeFromBottom (2.0f).reduced (4.0f, 0.0f), 1.0f);
            seg = juce::Rectangle<float> (r.getX() + w * float (i), r.getY(), w, r.getHeight()).reduced (2.0f);
        }
        else if (i == hover)
        {
            g.setColour (colours::panelEdge.withAlpha (0.6f));
            g.fillRoundedRectangle (seg, 3.0f);
        }
        g.setColour (on ? colours::text : colours::textDim);
        g.drawText (labels[i], seg, juce::Justification::centred);
        if (i > 0)
        {
            g.setColour (colours::panelEdge);
            g.drawVerticalLine (int (r.getX() + w * float (i)), r.getY() + 5.0f, r.getBottom() - 5.0f);
        }
    }
    g.setColour (colours::panelEdge);
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
}

// ---------------------------------------------------------------------------------------------
void VuMeter::setSource (Source s)
{
    if (s != source)
    {
        source = s;
        shownDb = s == Source::Gr ? 0.0f : -100.0f;
        peakDb = -100.0f;
        repaint();
    }
}

void VuMeter::push (float v)
{
    float db;
    if (source == Source::Gr)
        db = v < 1.0f ? (v > 0.0f ? 20.0f * std::log10 (v) : -100.0f) : 0.0f; // -GR
    else
        db = v > 0.0f ? 20.0f * std::log10 (v) : -100.0f;

    const bool gr = source == Source::Gr;
    // needle ballistics (display only): fast towards more deflection, ~24 dB/s back
    if (gr)
    {
        shownDb = db < shownDb ? db : std::min (0.0f, shownDb + 0.8f);
        if (db < 0.0f && (peakDb < -99.0f || db < peakDb))
            peakDb = db;
    }
    else
    {
        shownDb = db > shownDb ? db : std::max (-100.0f, shownDb - 0.8f);
        if (db > peakDb)
            peakDb = db;
    }
    repaint();
}

float VuMeter::angleFor (float db) const
{
    const float t = juce::jlimit (0.0f, 1.0f, (db + 48.0f) / 48.0f);
    return juce::degreesToRadians (-52.0f + 104.0f * t);
}

void VuMeter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    // bezel and face
    g.setColour (colours::well);
    g.fillRoundedRectangle (r, 6.0f);
    auto face = r.reduced (5.0f);
    juce::ColourGradient grad (colours::dial, face.getX(), face.getY(), colours::dial.darker (0.25f), face.getX(),
                               face.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (face, 4.0f);

    const auto pivot = juce::Point<float> (face.getCentreX(), face.getBottom() + face.getHeight() * 0.35f);
    const float radius = face.getHeight() * 1.05f;
    const auto ink = juce::Colour (0xff2a2622);

    // scale arc: the last 6 dB (0 ... -6) in copper
    juce::Path arc;
    arc.addCentredArc (pivot.x, pivot.y, radius, radius, 0.0f, angleFor (-48.0f), angleFor (-6.0f), true);
    g.setColour (ink);
    g.strokePath (arc, juce::PathStrokeType (1.5f));
    juce::Path hot;
    hot.addCentredArc (pivot.x, pivot.y, radius, radius, 0.0f, angleFor (-6.0f), angleFor (0.0f), true);
    g.setColour (colours::punch);
    g.strokePath (hot, juce::PathStrokeType (3.0f));

    g.setFont (juce::Font (uiFont (10.0f, true)));
    for (int db = 0; db >= -48; db -= 3)
    {
        const float a = angleFor (float (db));
        const bool major = db % 12 == 0 || db == -6;
        const float r0 = radius - (major ? 7.0f : 4.0f);
        g.setColour (db > -6 ? colours::punch : ink);
        g.drawLine (pivot.x + r0 * std::sin (a), pivot.y - r0 * std::cos (a), pivot.x + radius * std::sin (a),
                    pivot.y - radius * std::cos (a), major ? 1.5f : 1.0f);
        if (major)
        {
            const float rl = radius + 9.0f;
            g.drawText (juce::String (db), juce::Rectangle<float> (26.0f, 12.0f).withCentre ({ pivot.x + rl * std::sin (a), pivot.y - rl * std::cos (a) }),
                        juce::Justification::centred);
        }
    }

    // caption
    g.setColour (ink.withAlpha (0.8f));
    g.setFont (juce::Font (uiFont (11.0f, true)));
    const char* caption = source == Source::Gr ? "GAIN REDUCTION  dB" : (source == Source::In ? "INPUT  dBFS" : "OUTPUT  dBFS");
    g.drawText (caption, face.withTrimmedTop (face.getHeight() * 0.52f).removeFromTop (16.0f), juce::Justification::centred);

    // peak readout
    g.setFont (juce::Font (uiFont (10.5f)));
    const juce::String peakText = peakDb < -99.0f ? juce::String ("--") : juce::String (peakDb, 1);
    g.drawText ((source == Source::Gr ? "MAX " : "PEAK ") + peakText, face.removeFromBottom (16.0f).reduced (8.0f, 0.0f),
                juce::Justification::centredRight);

    // needle, clipped to the face
    {
        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (r.reduced (5.0f).toNearestInt());
        const float a = angleFor (std::max (-50.0f, shownDb));
        const float r1 = radius + 4.0f;
        g.setColour (colours::punch.darker (0.5f));
        g.drawLine (pivot.x, pivot.y, pivot.x + r1 * std::sin (a), pivot.y - r1 * std::cos (a), 1.8f);
    }
    // glass edge
    g.setColour (juce::Colours::white.withAlpha (0.06f));
    g.drawRoundedRectangle (r.reduced (5.5f), 4.0f, 1.0f);
}

} // namespace craft::punch::ui
