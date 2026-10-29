#include "VectorDisplay.h"

#include "CraftLookAndFeel.h"
#include "plugin/ParameterLayout.h"

namespace craft::ui
{

static constexpr double floorDb = -24.0;

VectorDisplay::VectorDisplay (juce::AudioProcessorValueTreeState& s, dsp::Variant v) : state (s), variant (v)
{
    setMouseCursor (juce::MouseCursor::CrosshairCursor);
}

void VectorDisplay::setSettings (const dsp::Settings& s)
{
    auto same = [] (const dsp::Settings& a, const dsp::Settings& b) {
        return a.gainDb == b.gainDb && a.width == b.width && a.rotationDeg == b.rotationDeg
            && a.asymmetryDeg == b.asymmetryDeg && a.swap == b.swap && a.polarity1 == b.polarity1
            && a.polarity2 == b.polarity2 && a.msInput == b.msInput;
    };
    if (hasSettings && same (s, settings))
        return;
    settings = s;
    hasSettings = true;
    repaint();
}

VectorDisplay::Probe VectorDisplay::probe (const dsp::Matrix2& a, double inL, double inR)
{
    const double lo = a.a00 * inL + a.a01 * inR;
    const double ro = a.a10 * inL + a.a11 * inR;
    const double mag = std::sqrt (lo * lo + ro * ro);
    Probe p;
    p.angleDeg = std::atan2 (lo - ro, lo + ro) * 180.0 / dsp::pi;
    p.levelDb = mag > 0.0 ? juce::jlimit (floorDb, 0.0, 20.0 * std::log10 (mag)) : floorDb;
    return p;
}

juce::Rectangle<float> VectorDisplay::plotArea() const
{
    // the semicircle: width = 2 * height
    auto r = getLocalBounds().toFloat().reduced (10.0f, 8.0f);
    r.removeFromBottom (16.0f); // base scale
    const float radius = std::min (r.getWidth() / 2.0f, r.getHeight());
    return { r.getCentreX() - radius, r.getBottom() - radius, 2.0f * radius, radius };
}

juce::Point<float> VectorDisplay::pointFor (const Probe& p) const
{
    const auto area = plotArea();
    const float radius = area.getHeight() * float ((p.levelDb - floorDb) / -floorDb);
    const float th = float (p.angleDeg * dsp::pi / 180.0);
    return { area.getCentreX() - radius * std::sin (th), area.getBottom() - radius * std::cos (th) };
}

void VectorDisplay::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour (colours::well);
    g.fillRoundedRectangle (bounds, 6.0f);
    g.setColour (colours::panelEdge);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, 1.0f);

    const auto area = plotArea();
    const juce::Point<float> origin { area.getCentreX(), area.getBottom() };
    const float R = area.getHeight();

    // dB rings every 6 dB
    for (int db = 0; db > -24; db -= 6)
    {
        const float rr = R * float ((db - floorDb) / -floorDb);
        juce::Path arc;
        arc.addCentredArc (origin.x, origin.y, rr, rr, 0.0f, -juce::MathConstants<float>::halfPi,
                           juce::MathConstants<float>::halfPi, true);
        g.setColour (db == 0 ? colours::panelEdge.brighter (0.5f) : colours::panelEdge.brighter (0.1f));
        g.strokePath (arc, juce::PathStrokeType (db == 0 ? 1.4f : 1.0f));
    }
    // speaker directions (+-45 deg) and centre
    g.setColour (colours::panelEdge.brighter (0.25f));
    for (float a : { -45.0f, 0.0f, 45.0f })
    {
        const float th = juce::degreesToRadians (a);
        const float dashes[] = { 3.0f, 4.0f };
        g.drawDashedLine ({ origin, { origin.x - R * std::sin (th), origin.y - R * std::cos (th) } }, dashes, 2, 1.0f);
    }
    // small speaker markers
    for (float a : { -45.0f, 45.0f })
    {
        const float th = juce::degreesToRadians (a);
        const juce::Point<float> c { origin.x - (R + 1.0f) * std::sin (th), origin.y - (R + 1.0f) * std::cos (th) };
        g.setColour (colours::textDim);
        g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre (c));
    }
    // base line and scale "0 -6 -12 -18 -24 -18 -12 -6 0"
    g.setColour (colours::panelEdge.brighter (0.5f));
    g.drawHorizontalLine (int (origin.y), origin.x - R, origin.x + R);
    g.setFont (juce::Font (uiFont (10.0f)));
    for (int db = 0; db >= -24; db -= 6)
        for (int side : { -1, 1 })
        {
            if (db == -24 && side == 1)
                continue;
            const float x = origin.x + float (side) * R * float ((db - floorDb) / -floorDb);
            g.setColour (colours::textDim);
            g.drawText (juce::String (db), juce::Rectangle<float> (30.0f, 12.0f).withCentre ({ x, origin.y + 9.0f }),
                        juce::Justification::centred);
        }
    g.setFont (juce::Font (uiFont (10.0f, true)));
    g.drawText ("L", juce::Rectangle<float> (12.0f, 12.0f).withCentre ({ origin.x - R * 0.74f - 8.0f, origin.y - R * 0.74f + 2.0f }),
                juce::Justification::centred);
    g.drawText ("R", juce::Rectangle<float> (12.0f, 12.0f).withCentre ({ origin.x + R * 0.74f + 8.0f, origin.y - R * 0.74f + 2.0f }),
                juce::Justification::centred);

    if (! hasSettings)
        return;

    // probes through the broadband path (steps 1-6 and 8)
    const auto a = dsp::broadbandLR (settings, variant);
    const double c = std::sqrt (0.5);
    const Probe probes[3] = { probe (a, 1.0, 0.0), probe (a, c, c), probe (a, 0.0, 1.0) };
    juce::Point<float> pts[3];
    for (int i = 0; i < 3; ++i)
        pts[i] = pointFor (probes[i]);

    g.saveState();
    g.reduceClipRegion (getLocalBounds().reduced (2));
    juce::Path tri;
    tri.addTriangle (pts[0], pts[1], pts[2]);
    g.setColour (colours::accent2.withAlpha (0.22f));
    g.fillPath (tri);
    g.setColour (colours::accent2.withAlpha (0.55f));
    g.strokePath (tri, juce::PathStrokeType (1.0f));

    const juce::Colour lineColours[3] = { colours::accent, colours::text, colours::accent2 };
    for (int i = 0; i < 3; ++i)
    {
        g.setColour (lineColours[i]);
        g.drawLine ({ origin, pts[i] }, 2.2f);
        g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre (pts[i]));
    }
    g.restoreState();

    g.setColour (colours::textDim);
    g.setFont (juce::Font (uiFont (10.0f)));
    g.drawText ("drag: gain / rotation    alt-drag: width / asymmetry", getLocalBounds().reduced (8, 4).toFloat(),
                juce::Justification::topLeft);
}

void VectorDisplay::mouseDown (const juce::MouseEvent& e)
{
    if (variant == dsp::Variant::MsMatrix)
        return;
    const bool alt = e.mods.isAltDown();
    dragX = state.getParameter (alt ? ids::asymmetry : ids::rotation);
    dragY = state.getParameter (alt ? ids::width : ids::gain);
    for (auto* p : { dragX, dragY })
        if (p != nullptr)
            p->beginChangeGesture();
    startX = dragX != nullptr ? dragX->getValue() : 0.0f;
    startY = dragY != nullptr ? dragY->getValue() : 0.0f;
}

void VectorDisplay::mouseDrag (const juce::MouseEvent& e)
{
    const auto area = plotArea();
    const float fine = e.mods.isShiftDown() ? 0.1f : 1.0f;
    if (dragX != nullptr)
        dragX->setValueNotifyingHost (
            juce::jlimit (0.0f, 1.0f, startX + fine * float (e.getDistanceFromDragStartX()) / area.getWidth()));
    if (dragY != nullptr)
        dragY->setValueNotifyingHost (
            juce::jlimit (0.0f, 1.0f, startY - fine * float (e.getDistanceFromDragStartY()) / (1.5f * area.getHeight())));
}

void VectorDisplay::mouseUp (const juce::MouseEvent&)
{
    for (auto* p : { dragX, dragY })
        if (p != nullptr)
            p->endChangeGesture();
    dragX = dragY = nullptr;
}

} // namespace craft::ui
