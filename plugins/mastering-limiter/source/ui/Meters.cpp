#include "Meters.h"

#include "LimiterLookAndFeel.h"

namespace craft::mastering_limiter::ui
{

namespace
{
constexpr float decayDbPerFrame = 0.9f; // ~27 dB/s at 30 frames/s
constexpr int holdFramesMax = 45;       // 1.5 s peak hold line
constexpr float scaleMarks[] = { 0.0f, -3.0f, -6.0f, -9.0f, -12.0f, -15.0f, -18.0f, -21.0f, -24.0f, -27.0f, -30.0f };

void fallTowards (float& level, float target)
{
    level = target >= level ? target : std::max (target, level - decayDbPerFrame);
}

void drawScale (juce::Graphics& g, juce::Rectangle<float> scale, juce::Rectangle<float> bars, bool textLeft)
{
    g.setFont (juce::Font (uiFont (9.5f)));
    for (float db : scaleMarks)
    {
        const float y = dbToY (db, bars);
        const bool major = int (db) % 6 == 0;
        g.setColour (colours::panelEdge.brighter (major ? 0.5f : 0.2f));
        if (textLeft)
            g.drawHorizontalLine (int (y), scale.getRight() - (major ? 5.0f : 3.0f), scale.getRight());
        else
            g.drawHorizontalLine (int (y), scale.getX(), scale.getX() + (major ? 5.0f : 3.0f));
        if (major)
        {
            g.setColour (colours::textDim);
            auto t = juce::Rectangle<float> (scale.getX(), y - 6.0f, scale.getWidth(), 12.0f);
            g.drawText (juce::String (int (-db)), textLeft ? t.withTrimmedRight (7.0f) : t.withTrimmedLeft (7.0f),
                        textLeft ? juce::Justification::centredRight : juce::Justification::centredLeft);
        }
    }
}

void drawBar (juce::Graphics& g, juce::Rectangle<float> r, float db, float hold, juce::Colour c)
{
    g.setColour (colours::well);
    g.fillRoundedRectangle (r, 2.0f);
    if (db > meterFloorDb)
    {
        auto fill = r.withTop (dbToY (db, r));
        juce::ColourGradient grad (c.brighter (0.25f), 0.0f, r.getY(), c.darker (0.4f), 0.0f, r.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRect (fill.reduced (1.5f, 0.0f).withTrimmedBottom (1.0f));
    }
    if (hold > meterFloorDb)
    {
        g.setColour (hold >= -0.05f ? colours::warn : c.brighter (0.5f));
        g.fillRect (juce::Rectangle<float> (r.getX() + 1.5f, dbToY (hold, r) - 1.0f, r.getWidth() - 3.0f, 2.0f));
    }
}
} // namespace

// ---------------------------------------------------------------------------------------------
MarkerMeter::MarkerMeter (juce::Colour c, ScaleSide s) : markerColour (c), side (s)
{
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
}

void MarkerMeter::setChannels (int n)
{
    channels = juce::jlimit (1, 2, n);
    repaint();
}

void MarkerMeter::pushLevels (float linL, float linR)
{
    const float in[2] = { linToDb (linL), linToDb (linR) };
    for (int c = 0; c < 2; ++c)
    {
        fallTowards (levelDb[c], in[c]);
        if (in[c] >= holdDb[c])
        {
            holdDb[c] = in[c];
            holdFrames[c] = holdFramesMax;
        }
        else if (--holdFrames[c] <= 0)
            fallTowards (holdDb[c], levelDb[c]);
    }
    repaint();
}

void MarkerMeter::setMarkerDb (float db)
{
    if (db != markerDb)
    {
        markerDb = db;
        repaint();
    }
}

void MarkerMeter::setLinkedHighlight (bool b)
{
    if (b != linked)
    {
        linked = b;
        repaint();
    }
}

juce::Rectangle<float> MarkerMeter::barsArea() const
{
    // [scale 24][bars][marker 18] (scale on the right for ScaleSide::Right)
    auto r = getLocalBounds().toFloat().reduced (0.0f, 8.0f);
    if (side == ScaleSide::Left)
        return r.withTrimmedLeft (24.0f).withTrimmedRight (18.0f);
    return r.withTrimmedLeft (18.0f).withTrimmedRight (24.0f);
}

juce::Rectangle<float> MarkerMeter::scaleArea() const
{
    auto r = getLocalBounds().toFloat().reduced (0.0f, 8.0f);
    return side == ScaleSide::Left ? r.removeFromLeft (24.0f) : r.removeFromRight (24.0f);
}

void MarkerMeter::paint (juce::Graphics& g)
{
    const auto bars = barsArea();
    drawScale (g, scaleArea(), bars, side == ScaleSide::Left);

    const float gap = 3.0f;
    const float w = (bars.getWidth() - gap * float (channels - 1)) / float (channels);
    for (int c = 0; c < channels; ++c)
        drawBar (g, { bars.getX() + float (c) * (w + gap), bars.getY(), w, bars.getHeight() }, levelDb[c], holdDb[c],
                 colours::accent2);

    // region above the marker (limited / over the ceiling) tinted
    const float my = dbToY (markerDb, bars);
    g.setColour (markerColour.withAlpha (0.10f));
    g.fillRect (bars.withBottom (my));

    // marker line across the bars and a triangular handle on the free side
    g.setColour (markerColour);
    g.fillRect (juce::Rectangle<float> (bars.getX() - 2.0f, my - 1.0f, bars.getWidth() + 4.0f, 2.0f));
    juce::Path tri;
    const bool handleRight = side == ScaleSide::Left;
    const float x0 = handleRight ? bars.getRight() + 3.0f : bars.getX() - 3.0f;
    const float x1 = handleRight ? x0 + 13.0f : x0 - 13.0f;
    tri.addTriangle (x0, my, x1, my - 7.0f, x1, my + 7.0f);
    g.setColour (dragging ? markerColour.brighter (0.3f) : markerColour);
    g.fillPath (tri);
    if (linked)
    {
        g.setColour (colours::text);
        g.strokePath (tri, juce::PathStrokeType (1.2f));
    }
}

void MarkerMeter::mouseDown (const juce::MouseEvent& e)
{
    dragging = true;
    dragStartDb = markerDb;
    dragStartY = e.position.y;
    if (onDragStart)
        onDragStart();
    // clicking near the marker grabs it; clicking elsewhere jumps the marker there
    const auto bars = barsArea();
    if (std::abs (e.position.y - dbToY (markerDb, bars)) > 8.0f)
    {
        dragStartDb = yToDb (e.position.y, bars);
        if (onDrag)
            onDrag (dragStartDb);
    }
    repaint();
}

void MarkerMeter::mouseDrag (const juce::MouseEvent& e)
{
    const auto bars = barsArea();
    const float pxPerDb = bars.getHeight() / -meterFloorDb;
    const float fine = e.mods.isShiftDown() ? 0.1f : 1.0f;
    const float db = juce::jlimit (meterFloorDb, 0.0f, dragStartDb - (e.position.y - dragStartY) / pxPerDb * fine);
    if (onDrag)
        onDrag (db);
}

void MarkerMeter::mouseUp (const juce::MouseEvent&)
{
    dragging = false;
    if (onDragEnd)
        onDragEnd();
    repaint();
}

void MarkerMeter::mouseDoubleClick (const juce::MouseEvent&)
{
    if (onReset)
        onReset();
}

// ---------------------------------------------------------------------------------------------
void GainReductionMeter::pushGain (float linGain)
{
    const float db = juce::jlimit (meterFloorDb, 0.0f, linToDb (linGain));
    // attenuation shows instantly and recovers with the meter decay
    grDb = db <= grDb ? db : std::min (db, grDb + decayDbPerFrame);
    if (db <= holdDb)
    {
        holdDb = db;
        holdFrames = holdFramesMax;
    }
    else if (--holdFrames <= 0)
        holdDb = std::min (0.0f, std::max (grDb, holdDb + decayDbPerFrame));
    repaint();
}

void GainReductionMeter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.0f, 8.0f);
    auto scale = r.removeFromLeft (24.0f);
    auto scaleR = r.removeFromRight (24.0f);
    auto bar = r.reduced (4.0f, 0.0f);
    drawScale (g, scale, bar, true);
    drawScale (g, scaleR, bar, false);

    g.setColour (colours::well);
    g.fillRoundedRectangle (bar, 2.0f);
    if (grDb < 0.0f)
    {
        const float y = dbToY (grDb, bar);
        juce::ColourGradient grad (colours::warn.brighter (0.2f), 0.0f, bar.getY(), colours::warn.darker (0.3f), 0.0f,
                                   bar.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRect (bar.withBottom (y).reduced (2.0f, 0.0f).withTrimmedTop (1.0f));
    }
    if (holdDb < -0.05f)
    {
        g.setColour (colours::text);
        g.fillRect (juce::Rectangle<float> (bar.getX() + 2.0f, dbToY (holdDb, bar) - 1.0f, bar.getWidth() - 4.0f, 2.0f));
    }
}

// ---------------------------------------------------------------------------------------------
void PeakReadout::push (float db)
{
    if (db > value + 0.005f)
    {
        value = db;
        repaint();
    }
}

void PeakReadout::reset()
{
    value = -200.0f;
    repaint();
}

void PeakReadout::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (colours::well);
    g.fillRoundedRectangle (r, 3.0f);
    const bool over = ! atten && value >= -0.005f;
    g.setColour (over ? colours::warn : colours::panelEdge);
    g.drawRoundedRectangle (r, 3.0f, 1.0f);

    juce::String text;
    if (atten)
        text = value > 0.005f ? juce::String (value, 1) : juce::String ("0.0");
    else
        text = value > -150.0f ? juce::String (value, 1) : juce::String ("-inf");
    g.setColour (over ? colours::warn : colours::text);
    g.setFont (juce::Font (uiFont (11.5f, true)));
    g.drawFittedText (text, getLocalBounds(), juce::Justification::centred, 1);
}

} // namespace craft::mastering_limiter::ui
