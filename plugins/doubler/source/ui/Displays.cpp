#include "Displays.h"

#include "DoublerLookAndFeel.h"
#include "dsp/Shelf.h"
#include "plugin/ParameterLayout.h"

namespace craft::doubler::ui
{

namespace
{
constexpr float dotRadius = 8.5f;

void drawDot (juce::Graphics& g, juce::Point<float> p, juce::Colour c, const juce::String& label, bool active, bool hot)
{
    auto r = juce::Rectangle<float> (2 * dotRadius, 2 * dotRadius).withCentre (p);
    if (active)
    {
        g.setColour (c.withAlpha (hot ? 0.35f : 0.18f));
        g.fillEllipse (r.expanded (hot ? 6.0f : 4.0f));
        g.setColour (c);
        g.fillEllipse (r);
        g.setColour (colours::background);
    }
    else
    {
        g.setColour (colours::well);
        g.fillEllipse (r);
        g.setColour (c.withAlpha (0.55f));
        g.drawEllipse (r, 1.5f);
    }
    g.setFont (juce::Font (uiFont (10.5f, true)));
    g.drawText (label, r, juce::Justification::centred);
}
} // namespace

// ---------------------------------------------------------------------------------------------
DisplayBase::DisplayBase (juce::String t) : title (std::move (t)) {}

juce::Rectangle<float> DisplayBase::plotArea() const
{
    return getLocalBounds().toFloat().reduced (10.0f).withTrimmedTop (16.0f);
}

void DisplayBase::paint (juce::Graphics& g)
{
    drawPanel (g, getLocalBounds().toFloat());
    drawCaption (g, getLocalBounds().reduced (10, 6).removeFromTop (14), title, juce::Justification::centredLeft);
    auto area = plotArea();
    g.setColour (colours::well);
    g.fillRoundedRectangle (area, 4.0f);
    paintContent (g, area);
}

// ---------------------------------------------------------------------------------------------
StageDisplay::StageDisplay (juce::AudioProcessorValueTreeState& s, int numVoices) : DisplayBase ("Stage")
{
    for (int n = 0; n < numVoices; ++n)
        items.push_back ({ ParamHandle (s, ids::voice (n, ids::on)), ParamHandle (s, ids::voice (n, ids::gain)),
                           ParamHandle (s, ids::voice (n, ids::pan)), colours::voice (n), juce::String (n + 1) });
    items.push_back ({ ParamHandle (s, ids::directOn), ParamHandle (s, ids::directGain), ParamHandle (s, ids::directPan),
                       colours::direct, "D" });
    setTooltip ("Drag a dot: angle = pan, distance from the centre = gain. Double-click toggles it on/off.");
    refresh();
}

void StageDisplay::refresh()
{
    bool changed = false;
    for (auto& it : items)
    {
        const double g = it.gain.get(), p = it.pan.get();
        const bool a = it.on.on();
        changed |= g != it.g || p != it.p || a != it.active;
        it.g = g;
        it.p = p;
        it.active = a;
    }
    if (changed)
        repaint();
}

juce::Point<float> StageDisplay::centre() const
{
    const auto a = plotArea();
    return { a.getCentreX(), a.getBottom() - 12.0f };
}

float StageDisplay::radius() const
{
    const auto a = plotArea();
    return std::max (10.0f, std::min (a.getWidth() * 0.5f - 16.0f, a.getHeight() - 26.0f));
}

juce::Point<float> StageDisplay::positionOf (const Item& it) const
{
    const float rho = float ((it.g + 48.0) / 48.0) * radius();
    const float th = float (it.p * 2.0) * juce::MathConstants<float>::pi / 180.0f;
    const auto c = centre();
    return { c.x + rho * std::sin (th), c.y - rho * std::cos (th) };
}

int StageDisplay::hitTest (juce::Point<float> p) const
{
    for (int i = 0; i < int (items.size()); ++i) // voices first: they are drawn on top
        if (positionOf (items[size_t (i)]).getDistanceFrom (p) <= dotRadius + 3.0f)
            return i;
    return -1;
}

void StageDisplay::paintContent (juce::Graphics& g, juce::Rectangle<float> area)
{
    const auto c = centre();
    const float R = radius();
    const float halfTurn = juce::MathConstants<float>::pi;

    // gain rings every 12 dB and pan spokes every 22.5 deg
    g.setColour (colours::grid);
    for (int db = 0; db >= -36; db -= 12)
    {
        const float r = R * float (db + 48) / 48.0f;
        juce::Path arc;
        arc.addCentredArc (c.x, c.y, r, r, 0.0f, -halfTurn / 2, halfTurn / 2, true);
        g.strokePath (arc, juce::PathStrokeType (db == 0 ? 1.5f : 1.0f));
    }
    for (int k = -2; k <= 2; ++k)
    {
        const float th = float (k) * halfTurn / 4.0f;
        g.drawLine (c.x, c.y, c.x + R * std::sin (th), c.y - R * std::cos (th), 1.0f);
    }
    g.setColour (colours::textDim);
    g.setFont (juce::Font (uiFont (10.0f, true)));
    g.drawText ("L", juce::Rectangle<float> (c.x - R - 14.0f, c.y - 8.0f, 12.0f, 12.0f), juce::Justification::centred);
    g.drawText ("R", juce::Rectangle<float> (c.x + R + 2.0f, c.y - 8.0f, 12.0f, 12.0f), juce::Justification::centred);
    g.setFont (juce::Font (uiFont (9.0f)));
    for (int db = 0; db >= -36; db -= 12)
    {
        const float r = R * float (db + 48) / 48.0f;
        g.drawText (juce::String (db), juce::Rectangle<float> (c.x + 2.0f, c.y - r - 1.0f, 26.0f, 11.0f),
                    juce::Justification::centredLeft);
    }
    // listener
    g.setColour (colours::panelEdge.brighter (0.5f));
    g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre (c));
    juce::ignoreUnused (area);

    // inactive first, then active; the direct below the voices
    for (int pass = 0; pass < 2; ++pass)
        for (int i = int (items.size()) - 1; i >= 0; --i)
        {
            const auto& it = items[size_t (i)];
            if (it.active != (pass == 1))
                continue;
            drawDot (g, positionOf (it), it.colour, it.label, it.active, dragging == i);
        }
}

void StageDisplay::mouseDown (const juce::MouseEvent& e)
{
    dragging = hitTest (e.position);
    if (dragging >= 0)
    {
        items[size_t (dragging)].gain.begin();
        items[size_t (dragging)].pan.begin();
        repaint();
    }
}

void StageDisplay::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging < 0)
        return;
    auto& it = items[size_t (dragging)];
    const auto v = e.position - centre();
    double th = std::atan2 (double (v.x), double (-v.y)) * 180.0 / juce::MathConstants<double>::pi;
    if (v.y > 0.0f) // below the baseline: stay on the nearest side
        th = v.x < 0 ? -90.0 : 90.0;
    const double rho = juce::jlimit (0.0, 1.0, double (v.getDistanceFromOrigin()) / double (radius()));
    it.pan.set (juce::jlimit (-45.0, 45.0, th / 2.0));
    it.gain.set (-48.0 + 48.0 * rho);
    refresh();
}

void StageDisplay::mouseUp (const juce::MouseEvent&)
{
    if (dragging >= 0)
    {
        items[size_t (dragging)].gain.end();
        items[size_t (dragging)].pan.end();
    }
    dragging = -1;
    repaint();
}

void StageDisplay::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int i = hitTest (e.position);
    if (i >= 0)
        items[size_t (i)].on.setGesture (items[size_t (i)].on.on() ? 0.0 : 1.0);
    refresh();
}

// ---------------------------------------------------------------------------------------------
DelayPlaneDisplay::DelayPlaneDisplay (juce::AudioProcessorValueTreeState& s, int numVoices, std::function<double (int)> live)
    : DisplayBase ("Delay / Detune"), range (s, ids::range), align (s, ids::align), liveCents (std::move (live))
{
    for (int n = 0; n < numVoices; ++n)
        items.push_back ({ ParamHandle (s, ids::voice (n, ids::on)), ParamHandle (s, ids::voice (n, ids::delay)),
                           ParamHandle (s, ids::voice (n, ids::detune)), ParamHandle (s, ids::voice (n, ids::depth)),
                           ParamHandle (s, ids::voice (n, ids::octave)) });
    setTooltip ("Drag a voice: horizontal = delay, vertical = detune. The bar shows the modulation depth. "
                "Double-click toggles the voice.");
    refresh();
}

void DelayPlaneDisplay::refresh()
{
    bool changed = range.on() != range20 || align.on() != aligned;
    range20 = range.on();
    aligned = align.on();
    for (size_t n = 0; n < items.size(); ++n)
    {
        auto& it = items[n];
        const double d = it.delay.get(), c = it.detune.get(), dep = it.depth.get();
        const bool a = it.on.on(), o = it.octave.on();
        double live = liveCents ? liveCents (int (n)) + (o ? 1200.0 : 0.0) : c;
        if (dep == 0.0)
            live = c;
        changed |= d != it.d || c != it.c || dep != it.dep || a != it.active || o != it.oct || std::abs (live - it.live) > 0.05;
        it.d = d;
        it.c = c;
        it.dep = dep;
        it.active = a;
        it.oct = o;
        it.live = live;
    }
    if (changed)
        repaint();
}

juce::Point<float> DelayPlaneDisplay::toScreen (double delayMs, double cents) const
{
    const auto a = plotArea().reduced (12.0f, 10.0f);
    return { a.getX() + a.getWidth() * float (delayMs / 100.0), a.getCentreY() - a.getHeight() * 0.5f * float (cents / 100.0) };
}

int DelayPlaneDisplay::hitTest (juce::Point<float> p) const
{
    for (int i = 0; i < int (items.size()); ++i)
        if (toScreen (items[size_t (i)].d, items[size_t (i)].c).getDistanceFrom (p) <= dotRadius + 3.0f)
            return i;
    return -1;
}

void DelayPlaneDisplay::paintContent (juce::Graphics& g, juce::Rectangle<float> area)
{
    const auto inner = area.reduced (12.0f, 10.0f);
    // the region below the inherent latency, where delays are clamped (Align = No)
    if (! aligned)
    {
        const double lrMs = (range20 ? 1088.0 : 320.0) / 44.1;
        auto z = inner.withRight (toScreen (lrMs, 0).x);
        g.setColour (colours::panel.withAlpha (0.8f));
        g.fillRect (z);
        g.setColour (colours::grid.brighter (0.15f));
        for (float x = z.getX() - z.getHeight(); x < z.getRight(); x += 6.0f)
        {
            const float x0 = std::max (x, z.getX()), x1 = std::min (x + z.getHeight(), z.getRight());
            if (x1 > x0)
                g.drawLine (x0, z.getBottom() - (x0 - x), x1, z.getBottom() - (x1 - x), 1.0f);
        }
    }

    g.setFont (juce::Font (uiFont (9.0f)));
    for (int ms = 0; ms <= 100; ms += 25)
    {
        const float x = toScreen (ms, 0).x;
        g.setColour (colours::grid);
        g.drawVerticalLine (int (x), inner.getY(), inner.getBottom());
        g.setColour (colours::textDim);
        g.drawText (juce::String (ms), juce::Rectangle<float> (x - 14.0f, inner.getBottom() - 1.0f, 28.0f, 10.0f),
                    juce::Justification::centred);
    }
    for (int c = -100; c <= 100; c += 50)
    {
        const float y = toScreen (0, c).y;
        g.setColour (c == 0 ? colours::panelEdge.brighter (0.4f) : colours::grid);
        g.drawHorizontalLine (int (y), inner.getX(), inner.getRight());
        if (c != 0 && c != -100)
        {
            g.setColour (colours::textDim);
            g.drawText ((c > 0 ? "+" : "") + juce::String (c), juce::Rectangle<float> (inner.getRight() - 26.0f, y - 11.0f, 26.0f, 10.0f),
                        juce::Justification::centredRight);
        }
    }
    g.setColour (colours::textDim);
    g.drawText ("ms", juce::Rectangle<float> (inner.getRight() - 30.0f, inner.getBottom() - 11.0f, 28.0f, 10.0f),
                juce::Justification::centredRight);

    for (int pass = 0; pass < 2; ++pass)
        for (int i = int (items.size()) - 1; i >= 0; --i)
        {
            const auto& it = items[size_t (i)];
            if (it.active != (pass == 1))
                continue;
            const auto col = colours::voice (i);
            const auto p = toScreen (it.d, it.c);
            // modulation depth bar (horizontal, length ~ |depth|)
            if (it.dep != 0.0)
            {
                const float half = 6.0f + float (std::abs (it.dep) / 200.0) * inner.getWidth() * 0.12f;
                g.setColour (col.withAlpha (it.active ? 0.45f : 0.2f));
                g.fillRoundedRectangle (juce::Rectangle<float> (2.0f * half, 5.0f).withCentre (p), 2.5f);
                if (it.active)
                {
                    // live modulated pitch as a small tick
                    const auto lp = toScreen (it.d, juce::jlimit (-100.0, 100.0, it.live));
                    g.setColour (col.withAlpha (0.9f));
                    g.fillRoundedRectangle (juce::Rectangle<float> (14.0f, 3.0f).withCentre (lp), 1.5f);
                }
            }
            drawDot (g, p, col, juce::String (i + 1), it.active, dragging == i);
            if (it.oct)
            {
                g.setColour (col);
                g.setFont (juce::Font (uiFont (9.0f, true)));
                g.drawText ("-8va", juce::Rectangle<float> (p.x + dotRadius, p.y - dotRadius - 10.0f, 26.0f, 10.0f),
                            juce::Justification::centredLeft);
            }
        }
}

void DelayPlaneDisplay::mouseDown (const juce::MouseEvent& e)
{
    dragging = hitTest (e.position);
    if (dragging >= 0)
    {
        items[size_t (dragging)].delay.begin();
        items[size_t (dragging)].detune.begin();
        repaint();
    }
}

void DelayPlaneDisplay::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging < 0)
        return;
    auto& it = items[size_t (dragging)];
    const auto a = plotArea().reduced (12.0f, 10.0f);
    it.delay.set (juce::jlimit (0.0, 100.0, 100.0 * double ((e.position.x - a.getX()) / a.getWidth())));
    it.detune.set (juce::jlimit (-100.0, 100.0, -200.0 * double ((e.position.y - a.getCentreY()) / a.getHeight())));
    refresh();
}

void DelayPlaneDisplay::mouseUp (const juce::MouseEvent&)
{
    if (dragging >= 0)
    {
        items[size_t (dragging)].delay.end();
        items[size_t (dragging)].detune.end();
    }
    dragging = -1;
    repaint();
}

void DelayPlaneDisplay::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int i = hitTest (e.position);
    if (i >= 0)
        items[size_t (i)].on.setGesture (items[size_t (i)].on.on() ? 0.0 : 1.0);
    refresh();
}

// ---------------------------------------------------------------------------------------------
namespace
{
constexpr double fLo = 16.0, fHi = 21357.0, dbTop = 15.0, dbBottom = -30.0;
}

EqDisplay::EqDisplay (juce::AudioProcessorValueTreeState& s) : DisplayBase ("Voice EQ")
{
    bands[0] = { ParamHandle (s, ids::eqLowOn), ParamHandle (s, ids::eqLowGain), ParamHandle (s, ids::eqLowFreq) };
    bands[1] = { ParamHandle (s, ids::eqHighOn), ParamHandle (s, ids::eqHighGain), ParamHandle (s, ids::eqHighFreq) };
    setTooltip ("Drag L / H: frequency and gain of the low and high shelf (voices only). Double-click toggles a shelf.");
    refresh();
}

void EqDisplay::refresh()
{
    bool changed = false;
    for (auto& b : bands)
    {
        const double g = b.gain.get(), f = b.freq.get();
        const bool a = b.on.on();
        changed |= g != b.g || f != b.f || a != b.active;
        b.g = g;
        b.f = f;
        b.active = a;
    }
    if (changed)
        repaint();
}

float EqDisplay::xOf (double hz) const
{
    const auto a = plotArea().reduced (8.0f, 6.0f);
    return a.getX() + a.getWidth() * float (std::log (hz / fLo) / std::log (fHi / fLo));
}

double EqDisplay::hzAt (float x) const
{
    const auto a = plotArea().reduced (8.0f, 6.0f);
    return fLo * std::pow (fHi / fLo, juce::jlimit (0.0, 1.0, double ((x - a.getX()) / a.getWidth())));
}

float EqDisplay::yOf (double db) const
{
    const auto a = plotArea().reduced (8.0f, 6.0f);
    return a.getY() + a.getHeight() * float ((dbTop - juce::jlimit (dbBottom, dbTop, db)) / (dbTop - dbBottom));
}

double EqDisplay::dbAt (float y) const
{
    const auto a = plotArea().reduced (8.0f, 6.0f);
    return dbTop - (dbTop - dbBottom) * double ((y - a.getY()) / a.getHeight());
}

juce::Point<float> EqDisplay::markerOf (const Band& b) const { return { xOf (b.f), yOf (b.g) }; }

int EqDisplay::hitTest (juce::Point<float> p) const
{
    for (int i = 1; i >= 0; --i)
        if (markerOf (bands[size_t (i)]).getDistanceFrom (p) <= dotRadius + 3.0f)
            return i;
    return -1;
}

void EqDisplay::paintContent (juce::Graphics& g, juce::Rectangle<float> area)
{
    const auto inner = area.reduced (8.0f, 6.0f);
    g.setFont (juce::Font (uiFont (9.0f)));
    for (double f : { 20.0, 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0, 20000.0 })
    {
        const float x = xOf (f);
        g.setColour (colours::grid);
        g.drawVerticalLine (int (x), inner.getY(), inner.getBottom());
        const bool major = f == 100.0 || f == 1000.0 || f == 10000.0;
        if (major)
        {
            g.setColour (colours::textDim);
            g.drawText (f >= 1000.0 ? juce::String (int (f / 1000)) + "k" : juce::String (int (f)),
                        juce::Rectangle<float> (x + 2.0f, inner.getBottom() - 11.0f, 30.0f, 10.0f), juce::Justification::centredLeft);
        }
    }
    for (int db = 12; db >= -24; db -= 12)
    {
        const float y = yOf (db);
        g.setColour (db == 0 ? colours::panelEdge.brighter (0.4f) : colours::grid);
        g.drawHorizontalLine (int (y), inner.getX(), inner.getRight());
        g.setColour (colours::textDim);
        g.drawText ((db > 0 ? "+" : "") + juce::String (db), juce::Rectangle<float> (inner.getX() + 2.0f, y - 11.0f, 30.0f, 10.0f),
                    juce::Justification::centredLeft);
    }

    // combined response of the enabled shelves (48 kHz design rate for display)
    const double fs = 48000.0;
    const auto low = designShelf (ShelfType::Low, bands[0].f, bands[0].g, fs);
    const auto high = designShelf (ShelfType::High, bands[1].f, bands[1].g, fs);
    juce::Path curve;
    for (float x = inner.getX(); x <= inner.getRight(); x += 1.5f)
    {
        const double f = std::min (hzAt (x), 0.499 * fs);
        double db = 0.0;
        if (bands[0].active)
            db += 20.0 * std::log10 (std::abs (low.response (f, fs)) + 1e-12);
        if (bands[1].active)
            db += 20.0 * std::log10 (std::abs (high.response (f, fs)) + 1e-12);
        const float y = yOf (db);
        if (x == inner.getX())
            curve.startNewSubPath (x, y);
        else
            curve.lineTo (x, y);
    }
    juce::Path fill (curve);
    fill.lineTo (inner.getRight(), yOf (0));
    fill.lineTo (inner.getX(), yOf (0));
    fill.closeSubPath();
    g.setColour (colours::accent2.withAlpha (0.12f));
    g.fillPath (fill);
    g.setColour (colours::accent2);
    g.strokePath (curve, juce::PathStrokeType (2.0f));

    const char* names[2] = { "L", "H" };
    for (int i = 0; i < 2; ++i)
        drawDot (g, markerOf (bands[size_t (i)]), i == 0 ? colours::accent : colours::accent2, names[i],
                 bands[size_t (i)].active, dragging == i);
}

void EqDisplay::mouseDown (const juce::MouseEvent& e)
{
    dragging = hitTest (e.position);
    if (dragging >= 0)
    {
        bands[size_t (dragging)].gain.begin();
        bands[size_t (dragging)].freq.begin();
        repaint();
    }
}

void EqDisplay::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging < 0)
        return;
    auto& b = bands[size_t (dragging)];
    b.freq.set (hzAt (e.position.x));
    b.gain.set (juce::jlimit (dbBottom, 12.0, dbAt (e.position.y)));
    refresh();
}

void EqDisplay::mouseUp (const juce::MouseEvent&)
{
    if (dragging >= 0)
    {
        bands[size_t (dragging)].gain.end();
        bands[size_t (dragging)].freq.end();
    }
    dragging = -1;
    repaint();
}

void EqDisplay::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int i = hitTest (e.position);
    if (i >= 0)
        bands[size_t (i)].on.setGesture (bands[size_t (i)].on.on() ? 0.0 : 1.0);
    refresh();
}

} // namespace craft::doubler::ui
