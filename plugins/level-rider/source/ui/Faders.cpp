#include "Faders.h"

#include "LookAndFeel.h"

namespace craft::levelrider::ui
{

namespace
{
void setPhysical (juce::RangedAudioParameter& p, float physical) { p.setValueNotifyingHost (p.convertTo0to1 (physical)); }

juce::String signedDb (float v)
{
    const float r = std::round (v * 10.0f) / 10.0f;
    return (r > 0.0f ? "+" : "") + juce::String (r == 0.0f ? 0.0f : r, 1);
}
} // namespace

// ---------------------------------------------------------------------------------------------
TargetBar::TargetBar (juce::RangedAudioParameter& t)
    : param (t), attachment (t, [this] (float v) { targetDb = v; repaint(); })
{
    attachment.sendInitialUpdate();
    setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
    setTooltip ("Target level - drag; double-click resets. The bar shows the detected vocal level");
}

juce::Rectangle<float> TargetBar::track() const
{
    return getLocalBounds().toFloat().reduced (8.0f, 0.0f).withTrimmedTop (16.0f).withHeight (18.0f);
}

float TargetBar::xFor (float db) const
{
    const auto t = track();
    return t.getX() + (juce::jlimit (lowDb, highDb, db) - lowDb) / (highDb - lowDb) * t.getWidth();
}

float TargetBar::dbFor (float x) const
{
    const auto t = track();
    return lowDb + (x - t.getX()) / t.getWidth() * (highDb - lowDb);
}

void TargetBar::setLevel (float db, bool isRiding)
{
    // gentle display ballistics: instant rise, 20 dB/s fall, short peak hold
    const float fallen = levelDb - 20.0f / 30.0f;
    levelDb = std::max (db, fallen);
    if (db >= heldDb)
    {
        heldDb = db;
        holdCounter = 45;
    }
    else if (--holdCounter < 0)
        heldDb = std::max (db, heldDb - 0.5f);
    riding = isRiding;
    repaint();
}

void TargetBar::paint (juce::Graphics& g)
{
    const auto t = track();
    g.setColour (colours::well);
    g.fillRoundedRectangle (t.expanded (2.0f), 3.0f);

    // level indicator
    if (levelDb > lowDb)
    {
        const float x = xFor (levelDb);
        juce::ColourGradient grad (colours::accent2.withAlpha (0.35f), t.getX(), 0.0f, colours::accent2, t.getRight(), 0.0f, false);
        g.setGradientFill (grad);
        g.fillRect (juce::Rectangle<float>::leftTopRightBottom (t.getX(), t.getY() + 3.0f, x, t.getBottom() - 3.0f));
    }
    if (heldDb > lowDb)
    {
        g.setColour (colours::accent2.brighter (0.4f));
        g.fillRect (juce::Rectangle<float> (2.0f, t.getHeight() - 6.0f).withCentre ({ xFor (heldDb), t.getCentreY() }));
    }

    // scale
    g.setFont (juce::Font (uiFont (9.5f)));
    for (int db = -60; db <= 0; db += 6)
    {
        const float x = xFor (float (db));
        g.setColour (colours::panelEdge.brighter (0.4f));
        g.drawVerticalLine (int (x), t.getBottom() + 2.0f, t.getBottom() + (db % 12 == 0 ? 7.0f : 4.0f));
        if (db % 12 == 0)
        {
            g.setColour (colours::textDim);
            g.drawText (juce::String (db), juce::Rectangle<float> (30.0f, 11.0f).withCentre ({ x, t.getBottom() + 14.0f }),
                        juce::Justification::centred);
        }
    }
    // unreachable part of the Target range (below -42 dBFS)
    g.setColour (colours::background.withAlpha (0.45f));
    g.fillRect (juce::Rectangle<float>::leftTopRightBottom (t.getX(), t.getY() - 2.0f, xFor (-42.0f), t.getY() + 2.0f));

    // target handle: line through the bar and a flag above
    const float x = xFor (targetDb);
    g.setColour (colours::accent);
    g.fillRect (juce::Rectangle<float> (2.5f, t.getHeight() + 6.0f).withCentre ({ x, t.getCentreY() }));
    juce::Path flag;
    flag.addTriangle (x - 6.0f, t.getY() - 9.0f, x + 6.0f, t.getY() - 9.0f, x, t.getY() - 2.0f);
    g.fillPath (flag);
    g.setFont (juce::Font (uiFont (10.5f, true)));
    const auto labelArea = juce::Rectangle<float> (96.0f, 12.0f).withCentre ({ x, t.getY() - 13.0f });
    g.drawText ("TARGET " + juce::String (targetDb, 1), labelArea.withX (juce::jlimit (0.0f, getWidth() - 96.0f, labelArea.getX())),
                juce::Justification::centred);
}

void TargetBar::mouseDown (const juce::MouseEvent& e)
{
    attachment.beginGesture();
    attachment.setValueAsPartOfGesture (dbFor (e.position.x));
}

void TargetBar::mouseDrag (const juce::MouseEvent& e)
{
    const float fine = e.mods.isShiftDown() ? 0.2f : 1.0f;
    const float x = e.mouseDownPosition.x + float (e.getDistanceFromDragStartX()) * fine;
    attachment.setValueAsPartOfGesture (dbFor (x));
}

void TargetBar::mouseUp (const juce::MouseEvent&) { attachment.endGesture(); }

void TargetBar::mouseDoubleClick (const juce::MouseEvent&)
{
    attachment.setValueAsCompleteGesture (param.convertFrom0to1 (param.getDefaultValue()));
}

// ---------------------------------------------------------------------------------------------
void VerticalScale::drawTicks (juce::Graphics& g, float x, bool labelsLeft, float step) const
{
    g.setFont (juce::Font (uiFont (9.5f)));
    for (float db = -12.0f; db <= 12.01f; db += step)
    {
        const float y = yFor (db);
        const bool major = std::fmod (std::abs (db), 6.0f) < 0.01f;
        g.setColour (colours::panelEdge.brighter (major ? 0.5f : 0.2f));
        const float len = major ? 7.0f : 4.0f;
        if (labelsLeft)
            g.drawHorizontalLine (int (y), x - len, x);
        else
            g.drawHorizontalLine (int (y), x, x + len);
        if (major)
        {
            g.setColour (colours::textDim);
            const auto r = labelsLeft ? juce::Rectangle<float> (x - 34.0f, y - 6.0f, 24.0f, 12.0f)
                                      : juce::Rectangle<float> (x + 9.0f, y - 6.0f, 24.0f, 12.0f);
            g.drawText (signedDb (db).upToFirstOccurrenceOf (".", false, false), r,
                        labelsLeft ? juce::Justification::centredRight : juce::Justification::centredLeft);
        }
    }
}

// ---------------------------------------------------------------------------------------------
RangeFader::RangeFader (juce::RangedAudioParameter& mxP, juce::RangedAudioParameter& mnP, juce::RangedAudioParameter& idP)
    : maxP (mxP), minP (mnP), idleP (idP),
      maxA (mxP, [this] (float v) { maxDb = v; repaint(); }),
      minA (mnP, [this] (float v) { minDb = v; repaint(); }),
      idleA (idP, [this] (float v) { idleDb = v; repaint(); })
{
    maxA.sendInitialUpdate();
    minA.sendInitialUpdate();
    idleA.sendInitialUpdate();
    setTooltip ("Range: drag the Max / Min handles; the arrow sets the Idle position. Double-click resets");
}

void RangeFader::resized()
{
    const auto r = getLocalBounds().toFloat().reduced (0.0f, 10.0f);
    scale.setTrack (juce::Rectangle<float> (6.0f, r.getHeight()).withCentre (r.getCentre()));
}

void RangeFader::paint (juce::Graphics& g)
{
    const auto t = scale.track();
    g.setColour (colours::well);
    g.fillRoundedRectangle (t.expanded (1.5f, 3.0f), 3.0f);
    scale.drawTicks (g, t.getRight() + 14.0f, false, 3.0f);

    // band between the handles
    const float yMax = scale.yFor (maxDb), yMin = scale.yFor (minDb);
    g.setColour (colours::accent.withAlpha (0.55f));
    g.fillRect (juce::Rectangle<float>::leftTopRightBottom (t.getX(), yMax, t.getRight(), yMin));

    // handles
    auto cap = [&g, &t] (float y, const char* label, bool top) {
        const auto c = juce::Rectangle<float> (30.0f, 13.0f).withCentre ({ t.getCentreX(), y + (top ? -5.0f : 5.0f) });
        drawFaderCap (g, c, colours::accent, false);
        g.setColour (colours::textDim);
        g.setFont (juce::Font (uiFont (8.5f, true)));
        g.drawText (label, c.translated (0.0f, top ? -11.0f : 11.0f), juce::Justification::centred);
    };
    cap (yMax, "MAX", true);
    cap (yMin, "MIN", false);

    // idle arrow on the left of the track
    const float yIdle = scale.yFor (juce::jlimit (minDb, maxDb, idleDb));
    juce::Path arrow;
    const float ax = t.getX() - 20.0f;
    arrow.addTriangle (ax, yIdle - 6.0f, ax, yIdle + 6.0f, ax + 11.0f, yIdle);
    arrow.addRectangle (ax - 8.0f, yIdle - 1.5f, 8.0f, 3.0f);
    g.setColour (colours::accent2);
    g.fillPath (arrow);
}

juce::RangedAudioParameter* RangeFader::pick (const juce::MouseEvent& e) const
{
    if (e.position.x < scale.track().getX() - 8.0f)
        return &idleP;
    const float dMax = std::abs (e.position.y - (scale.yFor (maxDb) - 5.0f));
    const float dMin = std::abs (e.position.y - (scale.yFor (minDb) + 5.0f));
    return dMax <= dMin ? &maxP : &minP;
}

void RangeFader::mouseDown (const juce::MouseEvent& e)
{
    dragged = pick (e);
    dragged->beginChangeGesture();
    mouseDrag (e);
}

void RangeFader::mouseDrag (const juce::MouseEvent& e)
{
    if (dragged == nullptr)
        return;
    const float offset = dragged == &maxP ? 5.0f : (dragged == &minP ? -5.0f : 0.0f);
    setPhysical (*dragged, scale.dbFor (e.position.y + offset));
}

void RangeFader::mouseUp (const juce::MouseEvent&)
{
    if (dragged != nullptr)
        dragged->endChangeGesture();
    dragged = nullptr;
}

void RangeFader::mouseDoubleClick (const juce::MouseEvent& e)
{
    auto* p = pick (e);
    p->beginChangeGesture();
    p->setValueNotifyingHost (p->getDefaultValue());
    p->endChangeGesture();
}

// ---------------------------------------------------------------------------------------------
RiderFader::RiderFader (Callbacks c) : cb (std::move (c))
{
    setTooltip ("Rider: follows the ride gain. Drag to override (riding resumes from there); double-click returns to Idle");
}

void RiderFader::setState (float gainDb, float rangeMin, float rangeMax, float idleDb, bool isRiding)
{
    if (gainDb != g || rangeMin != mn || rangeMax != mx || idleDb != idle || isRiding != riding)
    {
        g = gainDb;
        mn = rangeMin;
        mx = rangeMax;
        idle = idleDb;
        riding = isRiding;
        repaint();
    }
}

void RiderFader::resized()
{
    const auto r = getLocalBounds().toFloat().reduced (0.0f, 16.0f);
    scale.setTrack (juce::Rectangle<float> (10.0f, r.getHeight()).withCentre (r.getCentre()));
}

void RiderFader::paint (juce::Graphics& g2)
{
    auto& gr = g2;
    const auto t = scale.track();

    // range band behind the track
    const auto band = juce::Rectangle<float>::leftTopRightBottom (t.getX() - 26.0f, scale.yFor (mx), t.getRight() + 26.0f, scale.yFor (mn));
    gr.setColour (colours::accent2.withAlpha (0.10f));
    gr.fillRoundedRectangle (band, 4.0f);
    gr.setColour (colours::accent2.withAlpha (0.45f));
    gr.drawRoundedRectangle (band, 4.0f, 1.0f);

    gr.setColour (colours::well);
    gr.fillRoundedRectangle (t.expanded (2.0f, 4.0f), 4.0f);
    scale.drawTicks (gr, t.getX() - 34.0f, true, 1.0f);
    scale.drawTicks (gr, t.getRight() + 34.0f, false, 1.0f);

    // unity line and idle marker
    gr.setColour (colours::textDim.withAlpha (0.5f));
    gr.drawHorizontalLine (int (scale.yFor (0.0f)), t.getX() - 26.0f, t.getRight() + 26.0f);
    const float yIdle = scale.yFor (juce::jlimit (mn, mx, idle));
    juce::Path arrow;
    arrow.addTriangle (t.getRight() + 28.0f, yIdle - 5.0f, t.getRight() + 28.0f, yIdle + 5.0f, t.getRight() + 19.0f, yIdle);
    gr.setColour (colours::accent2);
    gr.fillPath (arrow);

    // fill from unity to g
    const float y = scale.yFor (g), y0 = scale.yFor (0.0f);
    gr.setColour ((riding ? colours::accent : colours::textDim).withAlpha (0.65f));
    gr.fillRect (juce::Rectangle<float>::leftTopRightBottom (t.getX() + 2.0f, std::min (y, y0), t.getRight() - 2.0f, std::max (y, y0)));

    // cap
    const auto cap = juce::Rectangle<float> (64.0f, 30.0f).withCentre ({ t.getCentreX(), y });
    drawFaderCap (gr, cap, riding ? colours::accent : colours::accent2, dragging);
}

void RiderFader::mouseDown (const juce::MouseEvent& e)
{
    const float yCap = scale.yFor (g);
    grabOffset = std::abs (e.position.y - yCap) < 18.0f ? yCap - e.position.y : 0.0f;
    dragging = true;
    cb.begin (scale.dbFor (e.position.y + grabOffset));
}

void RiderFader::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging)
        cb.drag (scale.dbFor (e.position.y + grabOffset));
}

void RiderFader::mouseUp (const juce::MouseEvent&)
{
    if (dragging)
        cb.end();
    dragging = false;
    repaint();
}

void RiderFader::mouseDoubleClick (const juce::MouseEvent&) { cb.resetToIdle(); }

// ---------------------------------------------------------------------------------------------
OutputFader::OutputFader (juce::RangedAudioParameter& p)
    : param (p), attachment (p, [this] (float v) { valueDb = v; repaint(); })
{
    attachment.sendInitialUpdate();
    setTooltip ("Output trim - drag; double-click resets. The light above shows output clipping (click to clear)");
}

juce::Rectangle<float> OutputFader::clipArea() const
{
    return juce::Rectangle<float> (12.0f, 12.0f).withCentre ({ getWidth() / 2.0f, 8.0f });
}

void OutputFader::setClip (bool clipped, float peak)
{
    if (clipped != clip || std::abs (peak - peakLevel) > 0.001f)
    {
        clip = clipped;
        peakLevel = peak;
        repaint();
    }
}

void OutputFader::resized()
{
    const auto r = getLocalBounds().toFloat().withTrimmedTop (26.0f).reduced (0.0f, 10.0f);
    scale.setTrack (juce::Rectangle<float> (6.0f, r.getHeight()).withCentre (r.getCentre()));
}

void OutputFader::paint (juce::Graphics& g)
{
    drawLed (g, clipArea(), colours::warn, clip ? 1.0f : 0.0f);
    g.setColour (clip ? colours::warn : colours::textDim);
    g.setFont (juce::Font (uiFont (8.5f, true)));
    g.drawText ("CLIP", clipArea().translated (24.0f, 0.0f).withWidth (30.0f), juce::Justification::centredLeft);

    const auto t = scale.track();
    g.setColour (colours::well);
    g.fillRoundedRectangle (t.expanded (1.5f, 3.0f), 3.0f);
    scale.drawTicks (g, t.getRight() + 14.0f, false, 3.0f);

    // output peak level as a thin bar left of the track, on a 0 .. -36 dBFS scale mapped onto the fader height
    const float peakDb = peakLevel > 0.0f ? 20.0f * std::log10 (peakLevel) : -100.0f;
    const float frac = juce::jlimit (0.0f, 1.0f, (peakDb + 36.0f) / 36.0f);
    const auto meter = juce::Rectangle<float> (4.0f, t.getHeight()).withCentre ({ t.getX() - 16.0f, t.getCentreY() });
    g.setColour (colours::well);
    g.fillRect (meter);
    g.setColour (peakDb > -0.1f ? colours::warn : colours::accent2);
    g.fillRect (meter.withTop (meter.getBottom() - frac * meter.getHeight()));

    const float y = scale.yFor (valueDb), y0 = scale.yFor (0.0f);
    g.setColour (colours::accent.withAlpha (0.55f));
    g.fillRect (juce::Rectangle<float>::leftTopRightBottom (t.getX(), std::min (y, y0), t.getRight(), std::max (y, y0)));
    drawFaderCap (g, juce::Rectangle<float> (34.0f, 16.0f).withCentre ({ t.getCentreX(), y }), colours::accent, dragging);
}

void OutputFader::mouseDown (const juce::MouseEvent& e)
{
    if (clipArea().expanded (6.0f).contains (e.position))
    {
        if (onClearClip)
            onClearClip();
        return;
    }
    const float yCap = scale.yFor (valueDb);
    grabOffset = std::abs (e.position.y - yCap) < 12.0f ? yCap - e.position.y : 0.0f;
    dragging = true;
    attachment.beginGesture();
    attachment.setValueAsPartOfGesture (scale.dbFor (e.position.y + grabOffset));
}

void OutputFader::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging)
        attachment.setValueAsPartOfGesture (scale.dbFor (e.position.y + grabOffset));
}

void OutputFader::mouseUp (const juce::MouseEvent&)
{
    if (dragging)
        attachment.endGesture();
    dragging = false;
    repaint();
}

void OutputFader::mouseDoubleClick (const juce::MouseEvent&)
{
    attachment.setValueAsCompleteGesture (param.convertFrom0to1 (param.getDefaultValue()));
}

} // namespace craft::levelrider::ui
