#include "FrequencyDisplay.h"

#include "Theme.h"
#include "dsp/Crossover.h"
#include "plugin/ParameterLayout.h"

namespace craft::multiband::ui
{

namespace
{
constexpr double fMin = 20.0, fMax = 20000.0;
constexpr double dbTop = 24.0, dbBottom = -30.0;
constexpr float handleRadius = 7.0f;
} // namespace

FrequencyDisplay::FrequencyDisplay (juce::AudioProcessorValueTreeState& state)
{
    using BP = ids::BandParam;
    for (int k = 0; k < numCrossovers; ++k)
        xover[size_t (k)] = state.getParameter (ids::crossover (k));
    for (int b = 0; b < numBands; ++b)
    {
        gain[size_t (b)] = state.getParameter (ids::band (b, BP::gain));
        range[size_t (b)] = state.getParameter (ids::band (b, BP::range));
        bypass[size_t (b)] = state.getParameter (ids::band (b, BP::bypass));
    }
    q = state.getParameter (ids::xoverQ);
    outputGain = state.getParameter (ids::outputGain);
    setTooltip ("Drag a handle sideways to move its crossover, up/down for the band Gain; Shift-drag sets the Range. "
                "Double-click resets.");
}

void FrequencyDisplay::setLiveGains (const std::array<float, numBands>& g, double sampleRate)
{
    liveDb = g;
    fs = sampleRate > 0.0 ? sampleRate : 48000.0;
    repaint();
}

juce::Rectangle<float> FrequencyDisplay::plotArea() const
{
    return getLocalBounds().toFloat().withTrimmedLeft (34.0f).withTrimmedBottom (16.0f).withTrimmedTop (6.0f).withTrimmedRight (8.0f);
}

float FrequencyDisplay::xForFreq (double f) const
{
    const auto a = plotArea();
    const double t = std::log (std::clamp (f, 1.0, 1.0e6) / fMin) / std::log (fMax / fMin);
    return a.getX() + float (t) * a.getWidth();
}

double FrequencyDisplay::freqForX (float x) const
{
    const auto a = plotArea();
    return fMin * std::pow (fMax / fMin, double ((x - a.getX()) / a.getWidth()));
}

float FrequencyDisplay::yForDb (double db) const
{
    const auto a = plotArea();
    return a.getY() + float ((dbTop - db) / (dbTop - dbBottom)) * a.getHeight();
}

double FrequencyDisplay::dbPerPixel() const { return (dbTop - dbBottom) / double (plotArea().getHeight()); }

std::array<double, numCrossovers> FrequencyDisplay::crossovers() const
{
    return { paramValue (xover[0]), paramValue (xover[1]), paramValue (xover[2]) };
}

juce::Point<float> FrequencyDisplay::handlePos (int b) const
{
    const auto x = crossovers();
    double f;
    if (b < numCrossovers)
        f = x[size_t (b)];
    else
        f = std::sqrt (std::max (x[2], fMin) * fMax); // band 4 has no crossover above it: centre of its region
    const auto a = plotArea();
    return { juce::jlimit (a.getX(), a.getRight(), xForFreq (f)),
             juce::jlimit (a.getY(), a.getBottom(), yForDb (paramValue (gain[size_t (b)]))) };
}

int FrequencyDisplay::hitHandle (juce::Point<float> p) const
{
    int best = -1;
    float bestD = 14.0f;
    for (int b = 0; b < numBands; ++b)
    {
        const float d = handlePos (b).getDistanceFrom (p);
        if (d < bestD)
        {
            bestD = d;
            best = b;
        }
    }
    return best;
}

void FrequencyDisplay::paint (juce::Graphics& g)
{
    const auto a = plotArea();
    g.setColour (colours::well);
    g.fillRoundedRectangle (a.expanded (2.0f), 4.0f);

    // grid
    g.setFont (juce::Font (uiFont (9.5f)));
    for (double decade = 10.0; decade <= 10000.0; decade *= 10.0)
        for (int m = 1; m <= 9; ++m)
        {
            const double f = decade * m;
            if (f < fMin || f > fMax)
                continue;
            const float x = xForFreq (f);
            const bool major = m == 1 || m == 2 || m == 5;
            g.setColour (major ? colours::grid.brighter (0.25f) : colours::grid);
            g.drawVerticalLine (int (x), a.getY(), a.getBottom());
            if (major)
            {
                g.setColour (colours::textDim);
                const auto txt = f >= 1000.0 ? juce::String (f / 1000.0) + "k" : juce::String (int (f));
                g.drawText (txt, juce::Rectangle<float> (x - 20.0f, a.getBottom() + 2.0f, 40.0f, 12.0f), juce::Justification::centred);
            }
        }
    for (int db = 24; db >= -30; db -= 6)
    {
        const float y = yForDb (db);
        g.setColour (db == 0 ? colours::panelEdge.brighter (0.5f) : colours::grid.brighter (0.15f));
        g.drawHorizontalLine (int (y), a.getX(), a.getRight());
        g.setColour (colours::textDim);
        g.drawText ((db > 0 ? "+" : "") + juce::String (db), juce::Rectangle<float> (0.0f, y - 6.0f, a.getX() - 5.0f, 12.0f),
                    juce::Justification::centredRight);
    }

    g.saveState();
    g.reduceClipRegion (a.toNearestInt());

    // band regions: between Gain and Gain + Range
    const auto x = crossovers();
    std::array<double, numBands + 1> edges { fMin * 0.5, x[0], x[1], x[2], fMax * 2.0 };
    for (int b = 0; b < numBands; ++b)
    {
        const double lo = std::min (edges[size_t (b)], edges[size_t (b + 1)]);
        const double hi = std::max (edges[size_t (b)], edges[size_t (b + 1)]);
        const float x0 = xForFreq (lo), x1 = xForFreq (hi);
        const double gDb = paramValue (gain[size_t (b)]);
        const double rDb = paramValue (range[size_t (b)]);
        const bool byp = paramValue (bypass[size_t (b)]) > 0.5;
        const auto col = byp ? colours::bypass : colours::band (b);
        const float y0 = yForDb (byp ? 0.0 : gDb), y1 = yForDb (byp ? 0.0 : gDb + rDb);
        g.setColour (col.withAlpha (0.18f));
        g.fillRect (juce::Rectangle<float>::leftTopRightBottom (x0, std::min (y0, y1), x1, std::max (y0, y1)));
        g.setColour (col.withAlpha (0.9f));
        g.drawLine (x0, y0, x1, y0, 1.5f);
        const float dashes[] = { 4.0f, 3.0f };
        g.setColour (col.withAlpha (0.6f));
        g.drawDashedLine (juce::Line<float> (x0, y1, x1, y1), dashes, 2, 1.0f);
    }

    // crossover markers
    for (int k = 0; k < numCrossovers; ++k)
    {
        const float xx = xForFreq (x[size_t (k)]);
        g.setColour (colours::text.withAlpha (0.18f));
        g.drawVerticalLine (int (xx), a.getY(), a.getBottom());
    }

    // animated total gain: |sum_b g_b B_b(f)| with the current band gains and the exact crossover responses
    {
        std::array<CrossoverSection, numCrossovers> secs;
        const double qv = paramValue (q);
        for (int k = 0; k < numCrossovers; ++k)
            secs[size_t (k)] = CrossoverSection::design (x[size_t (k)], qv, fs);
        std::array<double, numBands> lin {};
        for (int b = 0; b < numBands; ++b)
            lin[size_t (b)] = std::pow (10.0, double (liveDb[size_t (b)]) / 20.0);
        const double out = paramValue (outputGain);
        juce::Path curve;
        const int steps = std::max (32, int (a.getWidth() / 3.0f));
        for (int i = 0; i <= steps; ++i)
        {
            const float px = a.getX() + a.getWidth() * float (i) / float (steps);
            const double f = std::min (freqForX (px), 0.499 * fs);
            const auto r = bandResponses (secs, f, fs);
            std::complex<double> sum {};
            for (int b = 0; b < numBands; ++b)
                sum += lin[size_t (b)] * r[size_t (b)];
            const double db = 20.0 * std::log10 (std::max (std::abs (sum), 1.0e-9)) + out;
            const float py = yForDb (std::clamp (db, dbBottom - 6.0, dbTop + 6.0));
            if (i == 0) curve.startNewSubPath (px, py);
            else curve.lineTo (px, py);
        }
        g.setColour (colours::text.withAlpha (0.12f));
        g.strokePath (curve, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (colours::text);
        g.strokePath (curve, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    g.restoreState();

    // handles
    for (int b = 0; b < numBands; ++b)
    {
        const auto p = handlePos (b);
        const auto col = colours::band (b);
        const bool hot = b == hover || b == active;
        if (hot)
        {
            g.setColour (col.withAlpha (0.25f));
            g.fillEllipse (juce::Rectangle<float> (handleRadius * 3.2f, handleRadius * 3.2f).withCentre (p));
        }
        g.setColour (colours::well);
        g.fillEllipse (juce::Rectangle<float> (handleRadius * 2.0f, handleRadius * 2.0f).withCentre (p));
        g.setColour (col);
        g.drawEllipse (juce::Rectangle<float> (handleRadius * 2.0f, handleRadius * 2.0f).withCentre (p), 2.0f);
        g.setFont (juce::Font (uiFont (9.5f, true)));
        g.drawText (juce::String (b + 1), juce::Rectangle<float> (16.0f, 12.0f).withCentre (p), juce::Justification::centred);
    }

    // readout of the active / hovered handle
    const int shown = active >= 0 ? active : hover;
    if (shown >= 0)
    {
        juce::String t = "Band " + juce::String (shown + 1) + "   Gain " + param::formatDb (paramValue (gain[size_t (shown)]))
                         + " dB   Range " + param::formatDb (paramValue (range[size_t (shown)])) + " dB";
        if (shown < numCrossovers)
            t << "   X" << (shown + 1) << " " << param::formatHz (paramValue (xover[size_t (shown)])) << " Hz";
        g.setColour (colours::text);
        g.setFont (juce::Font (uiFont (11.0f, true)));
        g.drawText (t, a.reduced (8.0f, 4.0f).removeFromTop (14.0f), juce::Justification::centredLeft);
    }
}

void FrequencyDisplay::mouseMove (const juce::MouseEvent& e)
{
    const int h = hitHandle (e.position);
    if (h != hover)
    {
        hover = h;
        setMouseCursor (h >= 0 ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void FrequencyDisplay::mouseExit (const juce::MouseEvent&)
{
    hover = -1;
    repaint();
}

void FrequencyDisplay::mouseDown (const juce::MouseEvent& e)
{
    active = hitHandle (e.position);
    if (active < 0)
        return;
    rangeMode = e.mods.isShiftDown() || e.mods.isCommandDown();
    dragStart = e.position;
    startGain = float (paramValue (gain[size_t (active)]));
    startRange = float (paramValue (range[size_t (active)]));
    gain[size_t (active)]->beginChangeGesture();
    range[size_t (active)]->beginChangeGesture();
    if (active < numCrossovers)
    {
        startXover = xover[size_t (active)]->getValue();
        xover[size_t (active)]->beginChangeGesture();
    }
}

void FrequencyDisplay::mouseDrag (const juce::MouseEvent& e)
{
    if (active < 0)
        return;
    const double fine = e.mods.isAltDown() ? 0.2 : 1.0;
    const double dDb = -double (e.position.y - dragStart.y) * dbPerPixel() * fine;
    if (rangeMode)
    {
        auto* p = range[size_t (active)];
        p->setValueNotifyingHost (p->convertTo0to1 (float (juce::jlimit (-24.0, 18.0, double (startRange) + dDb))));
    }
    else
    {
        auto* p = gain[size_t (active)];
        p->setValueNotifyingHost (p->convertTo0to1 (float (juce::jlimit (-18.0, 18.0, double (startGain) + dDb))));
        if (active < numCrossovers)
        {
            // horizontal: log frequency, i.e. linear in the crossover's normalised value
            const double octaves = double (e.position.x - dragStart.x) / double (plotArea().getWidth()) * std::log2 (fMax / fMin) * fine;
            const double perOctave = 1.0 / std::log2 (param::Crossover::maxValue / param::Crossover::minValue);
            xover[size_t (active)]->setValueNotifyingHost (float (juce::jlimit (0.0, 1.0, double (startXover) + octaves * perOctave)));
        }
    }
    repaint();
}

void FrequencyDisplay::mouseUp (const juce::MouseEvent&)
{
    if (active >= 0)
    {
        gain[size_t (active)]->endChangeGesture();
        range[size_t (active)]->endChangeGesture();
        if (active < numCrossovers)
            xover[size_t (active)]->endChangeGesture();
    }
    active = -1;
    repaint();
}

void FrequencyDisplay::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int h = hitHandle (e.position);
    if (h < 0)
        return;
    auto reset = [] (juce::RangedAudioParameter* p) {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->getDefaultValue());
        p->endChangeGesture();
    };
    if (e.mods.isShiftDown())
        reset (range[size_t (h)]);
    else
        reset (gain[size_t (h)]);
    repaint();
}

} // namespace craft::multiband::ui
