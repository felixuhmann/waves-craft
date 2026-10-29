#include "TransferGraph.h"

#include "DynamicsLookAndFeel.h"
#include "plugin/ParameterLayout.h"

namespace craft::dynamics::ui
{

namespace
{
constexpr double rangeDb = 100.0; // both axes: 0 ... -100 dB
constexpr float leftMargin = 30.0f, bottomMargin = 34.0f, stripH = 14.0f;

bool sameCurves (const dsp::Settings& a, const dsp::Settings& b)
{
    return a.compThresholdDb == b.compThresholdDb && a.ratioPos == b.ratioPos && a.makeupDb == b.makeupDb && a.peakRef == b.peakRef
        && a.compBypass == b.compBypass && a.expander == b.expander && a.gateOpenDb == b.gateOpenDb && a.gateCloseDb == b.gateCloseDb
        && a.floorValue == b.floorValue && a.gateBypass == b.gateBypass;
}
} // namespace

TransferGraph::TransferGraph (juce::AudioProcessorValueTreeState& s, bool comp, bool gate) : state (s), hasComp (comp), hasGate (gate)
{
    setTooltip ("Input level (horizontal) against output level (vertical).\nDrag the markers below the graph to set the thresholds.");
}

juce::Rectangle<float> TransferGraph::plotArea() const
{
    return getLocalBounds().toFloat().withTrimmedLeft (leftMargin).withTrimmedBottom (bottomMargin).withTrimmedTop (6.0f).withTrimmedRight (8.0f);
}

juce::Rectangle<float> TransferGraph::markerStrip() const
{
    const auto p = plotArea();
    return { p.getX() - 8.0f, p.getBottom() + 3.0f, p.getWidth() + 16.0f, stripH };
}

float TransferGraph::xFor (double db) const
{
    const auto p = plotArea();
    return p.getX() + p.getWidth() * float (juce::jlimit (0.0, 1.0, (db + rangeDb) / rangeDb));
}

float TransferGraph::yFor (double db) const
{
    const auto p = plotArea();
    return p.getBottom() - p.getHeight() * float (juce::jlimit (0.0, 1.0, (db + rangeDb) / rangeDb));
}

double TransferGraph::dbForX (float x) const
{
    const auto p = plotArea();
    return juce::jlimit (-rangeDb, 0.0, double ((x - p.getX()) / p.getWidth()) * rangeDb - rangeDb);
}

juce::RangedAudioParameter* TransferGraph::paramFor (int m) const
{
    switch (m)
    {
        case Threshold: return state.getParameter (ids::compThreshold);
        case Open:      return state.getParameter (ids::gateOpen);
        case Close:     return state.getParameter (ids::gateClose);
        default:        return nullptr;
    }
}

double TransferGraph::markerDb (int m) const
{
    switch (m)
    {
        case Threshold: return settings.compThresholdDb;
        case Open:      return settings.gateOpenDb == dsp::minusInfinityDb ? -rangeDb : settings.gateOpenDb;
        case Close:     return settings.gateCloseDb == dsp::minusInfinityDb ? -rangeDb : settings.gateCloseDb;
        default:        return 0.0;
    }
}

void TransferGraph::update (const dsp::Settings& s, float compLevelDb, float gateLevelDb)
{
    const bool curvesChanged = first || ! sameCurves (s, settings);
    const bool moved = std::abs (compLevelDb - compLevel) > 0.1f || std::abs (gateLevelDb - gateLevel) > 0.1f;
    first = false;
    settings = s;
    compLevel = compLevelDb;
    gateLevel = gateLevelDb;
    if (curvesChanged || moved)
        repaint();
}

void TransferGraph::drawMarker (juce::Graphics& g, int m, juce::Colour c, bool filled) const
{
    const auto strip = markerStrip();
    const float x = xFor (markerDb (m));
    juce::Path tri;
    tri.addTriangle (x, strip.getY(), x - 6.0f, strip.getBottom(), x + 6.0f, strip.getBottom());
    const bool hot = m == hover || m == dragging;
    g.setColour (hot ? c.brighter (0.4f) : c);
    if (filled)
        g.fillPath (tri);
    else
        g.strokePath (tri, juce::PathStrokeType (1.6f));
    // guide line into the plot while hovering or dragging
    if (hot)
    {
        const auto p = plotArea();
        g.setColour (c.withAlpha (0.35f));
        g.drawVerticalLine (int (x), p.getY(), p.getBottom());
    }
}

void TransferGraph::paint (juce::Graphics& g)
{
    const auto p = plotArea();
    g.setColour (colours::well);
    g.fillRoundedRectangle (p.expanded (2.0f), 4.0f);

    // grid every 10 dB, labels every 20 dB
    g.setFont (juce::Font (uiFont (9.5f)));
    for (int db = 0; db >= -100; db -= 10)
    {
        g.setColour (db % 20 == 0 ? colours::grid.brighter (0.15f) : colours::grid);
        g.drawVerticalLine (int (xFor (db)), p.getY(), p.getBottom());
        g.drawHorizontalLine (int (yFor (db)), p.getX(), p.getRight());
        if (db % 20 == 0)
        {
            g.setColour (colours::textDim);
            g.drawText (juce::String (db), juce::Rectangle<float> (0.0f, yFor (db) - 6.0f, leftMargin - 4.0f, 12.0f), juce::Justification::centredRight);
        }
    }
    // unity diagonal
    g.setColour (colours::textDim.withAlpha (0.25f));
    g.drawLine (xFor (-rangeDb), yFor (-rangeDb), xFor (0.0), yFor (0.0), 1.0f);

    auto curve = [&] (auto outFor) {
        juce::Path path;
        bool started = false;
        for (double in = -rangeDb; in <= 0.0 + 1e-9; in += 0.5)
        {
            const double out = outFor (in);
            const juce::Point<float> pt (xFor (in), yFor (std::max (out, -rangeDb - 1.0)));
            if (! started)
                path.startNewSubPath (pt), started = true;
            else
                path.lineTo (pt);
        }
        return path;
    };

    g.saveState();
    g.reduceClipRegion (p.toNearestInt());
    if (hasGate)
    {
        const bool on = ! settings.gateBypass;
        auto path = curve ([this] (double in) {
            const double gl = std::abs (dsp::DynamicsEngine::staticGateGain (settings, in));
            return gl > 0.0 ? in + 20.0 * std::log10 (gl) : -1000.0;
        });
        g.setColour (colours::accent2.withAlpha (on ? 1.0f : 0.3f));
        g.strokePath (path, juce::PathStrokeType (on ? 2.0f : 1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    if (hasComp)
    {
        const bool on = ! settings.compBypass;
        auto path = curve ([this] (double in) { return in + dsp::DynamicsEngine::staticCompressorGainDb (settings, in); });
        g.setColour (colours::accent.withAlpha (on ? 1.0f : 0.3f));
        g.strokePath (path, juce::PathStrokeType (on ? 2.2f : 1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    // current detector level on each curve
    auto dot = [&] (double in, double out, juce::Colour c) {
        if (in < -rangeDb)
            return;
        const juce::Point<float> pt (xFor (in), yFor (out));
        g.setColour (c.withAlpha (0.3f));
        g.fillEllipse (juce::Rectangle<float> (14.0f, 14.0f).withCentre (pt));
        g.setColour (c.brighter (0.3f));
        g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre (pt));
    };
    if (hasGate && ! settings.gateBypass)
    {
        const double gl = std::abs (dsp::DynamicsEngine::staticGateGain (settings, gateLevel));
        dot (gateLevel, gl > 0.0 ? gateLevel + 20.0 * std::log10 (gl) : -rangeDb, colours::accent2);
    }
    if (hasComp && ! settings.compBypass)
        dot (compLevel, compLevel + dsp::DynamicsEngine::staticCompressorGainDb (settings, compLevel), colours::accent);
    g.restoreState();

    // axis captions
    g.setColour (colours::textDim);
    g.setFont (juce::Font (uiFont (9.5f, true)));
    g.drawText ("INPUT dB", juce::Rectangle<float> (p.getX(), float (getHeight()) - 14.0f, p.getWidth(), 12.0f), juce::Justification::centredRight);
    g.drawText ("OUT", juce::Rectangle<float> (p.getX() + 4.0f, p.getY() + 2.0f, 40.0f, 12.0f), juce::Justification::centredLeft);

    // markers
    if (hasGate)
    {
        drawMarker (g, Close, colours::accent2, false);
        drawMarker (g, Open, colours::accent2, true);
    }
    if (hasComp)
        drawMarker (g, Threshold, colours::accent, true);
}

int TransferGraph::markerAt (juce::Point<float> pos) const
{
    if (! markerStrip().expanded (0.0f, 6.0f).contains (pos))
        return None;
    int best = None;
    float bestD = 9.0f;
    for (int m : { Threshold, Open, Close })
    {
        if ((m == Threshold && ! hasComp) || (m != Threshold && ! hasGate))
            continue;
        const float d = std::abs (pos.x - xFor (markerDb (m)));
        if (d < bestD)
            bestD = d, best = m;
    }
    return best;
}

void TransferGraph::mouseMove (const juce::MouseEvent& e)
{
    const int h = markerAt (e.position);
    if (h != hover)
    {
        hover = h;
        setMouseCursor (h == None ? juce::MouseCursor::NormalCursor : juce::MouseCursor::LeftRightResizeCursor);
        repaint();
    }
}

void TransferGraph::mouseDown (const juce::MouseEvent& e)
{
    dragging = markerAt (e.position);
    if (auto* prm = paramFor (dragging))
    {
        prm->beginChangeGesture();
        if (e.mods.isAltDown() || e.getNumberOfClicks() > 1)
            prm->setValueNotifyingHost (prm->getDefaultValue());
    }
}

void TransferGraph::mouseDrag (const juce::MouseEvent& e)
{
    if (auto* prm = paramFor (dragging))
    {
        const double db = dbForX (e.position.x);
        prm->setValueNotifyingHost (prm->convertTo0to1 (float (db)));
        repaint();
    }
}

void TransferGraph::mouseUp (const juce::MouseEvent&)
{
    if (auto* prm = paramFor (dragging))
        prm->endChangeGesture();
    dragging = None;
    repaint();
}

} // namespace craft::dynamics::ui
