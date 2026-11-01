#include "FilterGraph.h"

#include "DynamicsLookAndFeel.h"
#include "plugin/ParameterLayout.h"

namespace craft::dynamics::ui
{

namespace
{
constexpr double minHz = 16.0, maxHz = 22000.0, topDb = 6.0, bottomDb = -42.0;
}

FilterGraph::FilterGraph (juce::AudioProcessorValueTreeState& s) : state (s)
{
    setTooltip ("Active band (amber) and passive band (lavender).\nDrag: horizontal = Freq, vertical = Q. Double-click resets.");
    setMouseCursor (juce::MouseCursor::CrosshairCursor);
}

juce::Rectangle<float> FilterGraph::plotArea() const
{
    return getLocalBounds().toFloat().withTrimmedBottom (14.0f).withTrimmedLeft (26.0f).reduced (2.0f);
}

float FilterGraph::xForHz (double hz) const
{
    const auto p = plotArea();
    return p.getX() + p.getWidth() * float (std::log (hz / minHz) / std::log (maxHz / minHz));
}

double FilterGraph::hzForX (float x) const
{
    const auto p = plotArea();
    return minHz * std::pow (maxHz / minHz, juce::jlimit (0.0, 1.0, double ((x - p.getX()) / p.getWidth())));
}

float FilterGraph::yForDb (double db) const
{
    const auto p = plotArea();
    return p.getY() + p.getHeight() * float ((topDb - juce::jlimit (bottomDb, topDb, db)) / (topDb - bottomDb));
}

// Q control: 0.6 at the top of the plot, 0.1 at the bottom
float FilterGraph::yForQ (double qv) const
{
    const auto p = plotArea();
    return p.getY() + p.getHeight() * float ((0.6 - qv) / 0.5);
}

double FilterGraph::qForY (float y) const
{
    const auto p = plotArea();
    return juce::jlimit (0.1, 0.6, 0.6 - 0.5 * double ((y - p.getY()) / p.getHeight()));
}

void FilterGraph::update (const dsp::Settings& s, double sampleRate)
{
    const bool used = s.compEq != dsp::EqMode::Wideband || s.gateEq != dsp::EqMode::Wideband || s.monitor == dsp::Monitor::Passive;
    if (s.filterType != type || s.filterFreqHz != freq || s.filterQ != q || sampleRate != fs || used != usedByModule)
    {
        type = s.filterType;
        freq = s.filterFreqHz;
        q = s.filterQ;
        fs = sampleRate > 0.0 ? sampleRate : 48000.0;
        usedByModule = used;
        repaint();
    }
}

void FilterGraph::paint (juce::Graphics& g)
{
    const auto p = plotArea();
    g.setColour (colours::well);
    g.fillRoundedRectangle (p.expanded (2.0f), 4.0f);

    g.setFont (juce::Font (uiFont (9.0f)));
    for (double hz : { 20.0, 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0, 20000.0 })
    {
        const float x = xForHz (hz);
        g.setColour (colours::grid);
        g.drawVerticalLine (int (x), p.getY(), p.getBottom());
        const bool label = hz == 100.0 || hz == 1000.0 || hz == 10000.0 || hz == 20.0;
        if (label)
        {
            g.setColour (colours::textDim);
            g.drawText (hz >= 1000.0 ? juce::String (int (hz / 1000.0)) + "k" : juce::String (int (hz)),
                        juce::Rectangle<float> (x - 15.0f, p.getBottom() + 2.0f, 30.0f, 12.0f), juce::Justification::centred);
        }
    }
    for (double db : { 0.0, -12.0, -24.0, -36.0 })
    {
        g.setColour (db == 0.0 ? colours::grid.brighter (0.2f) : colours::grid);
        g.drawHorizontalLine (int (yForDb (db)), p.getX(), p.getRight());
        g.setColour (colours::textDim);
        g.drawText (juce::String (int (db)), juce::Rectangle<float> (0.0f, yForDb (db) - 6.0f, 24.0f, 12.0f), juce::Justification::centredRight);
    }

    // responses of the active band |A| = |H|^2 and the passive band |P| = 1 - |A| (spec 7)
    const auto d = dsp::filterdesign::design (type, freq, q, fs);
    juce::Path active, passive;
    const int steps = juce::jmax (64, int (p.getWidth()));
    for (int i = 0; i <= steps; ++i)
    {
        const double hz = minHz * std::pow (maxHz / minHz, double (i) / steps);
        const double a = hz < 0.5 * fs ? std::norm (d.h.response (2.0 * dsp::filterdesign::pi * hz / fs)) : 0.0;
        const double aDb = 20.0 * std::log10 (std::max (a, 1.0e-6));
        const double pDb = 20.0 * std::log10 (std::max (1.0 - a, 1.0e-6));
        const juce::Point<float> pa (xForHz (hz), yForDb (aDb)), pp (xForHz (hz), yForDb (pDb));
        if (i == 0)
            active.startNewSubPath (pa), passive.startNewSubPath (pp);
        else
            active.lineTo (pa), passive.lineTo (pp);
    }
    g.saveState();
    g.reduceClipRegion (p.toNearestInt());
    const float alpha = usedByModule ? 1.0f : 0.45f;
    g.setColour (colours::passive.withAlpha (alpha));
    g.strokePath (passive, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (colours::accent.withAlpha (alpha));
    g.strokePath (active, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // crosshair: Freq horizontally, Q vertically
    const juce::Point<float> c (xForHz (freq), yForQ (q));
    g.setColour (colours::text.withAlpha (dragging ? 0.55f : 0.3f));
    g.drawVerticalLine (int (c.x), p.getY(), p.getBottom());
    g.drawHorizontalLine (int (c.y), p.getX(), p.getRight());
    g.setColour (colours::text);
    g.drawEllipse (juce::Rectangle<float> (10.0f, 10.0f).withCentre (c), 1.5f);
    g.restoreState();
}

void FilterGraph::setFromMouse (juce::Point<float> pos)
{
    if (auto* f = state.getParameter (ids::filterFreq))
        f->setValueNotifyingHost (f->convertTo0to1 (float (hzForX (pos.x))));
    if (auto* qp = state.getParameter (ids::filterQ))
        qp->setValueNotifyingHost (qp->convertTo0to1 (float (qForY (pos.y))));
}

void FilterGraph::mouseDown (const juce::MouseEvent& e)
{
    dragging = true;
    for (auto* id : { ids::filterFreq, ids::filterQ })
        if (auto* prm = state.getParameter (id))
            prm->beginChangeGesture();
    setFromMouse (e.position);
}

void FilterGraph::mouseDrag (const juce::MouseEvent& e) { setFromMouse (e.position); }

void FilterGraph::mouseUp (const juce::MouseEvent&)
{
    dragging = false;
    for (auto* id : { ids::filterFreq, ids::filterQ })
        if (auto* prm = state.getParameter (id))
            prm->endChangeGesture();
    repaint();
}

void FilterGraph::mouseDoubleClick (const juce::MouseEvent&)
{
    for (auto* id : { ids::filterFreq, ids::filterQ })
        if (auto* prm = state.getParameter (id))
        {
            prm->beginChangeGesture();
            prm->setValueNotifyingHost (prm->getDefaultValue());
            prm->endChangeGesture();
        }
}

} // namespace craft::dynamics::ui
