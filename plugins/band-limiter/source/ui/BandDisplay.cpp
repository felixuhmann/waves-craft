#include "BandDisplay.h"

#include "LookAndFeel.h"

namespace craft::band_limiter::ui
{

namespace
{
constexpr float minHz = 20.0f, maxHz = 20000.0f, maxAttenDb = 30.0f, gainRangeDb = 12.0f;
const juce::Colour bandColours[dsp::numBands] = { juce::Colour (0xffd9775f), juce::Colour (0xffe3a447), juce::Colour (0xffb9c46b),
                                                  juce::Colour (0xff58c4b6), juce::Colour (0xff7aa2e0) };

float physical (juce::RangedAudioParameter* p) { return p->convertFrom0to1 (p->getValue()); }
void setPhysical (juce::RangedAudioParameter* p, float v) { p->setValueNotifyingHost (p->convertTo0to1 (v)); }
} // namespace

BandDisplay::BandDisplay (std::array<juce::RangedAudioParameter*, dsp::numCrossovers> c,
                          std::array<juce::RangedAudioParameter*, dsp::numBands> g,
                          std::array<juce::RangedAudioParameter*, dsp::numBands> p)
    : crossovers (c), gains (g), priorities (p)
{
    setTooltip ("Dynamic gain per band: live line, smoothed trail, peak hold.\n"
                "Drag a crossover line sideways; drag a band dot up/down for its gain. Double-click resets.");
}

juce::Rectangle<float> BandDisplay::plot() const { return getLocalBounds().toFloat().reduced (8.0f, 8.0f).withTrimmedBottom (14.0f); }

float BandDisplay::freqToX (float hz) const
{
    const auto r = plot();
    return r.getX() + r.getWidth() * std::log (hz / minHz) / std::log (maxHz / minHz);
}

float BandDisplay::xToFreq (float x) const
{
    const auto r = plot();
    return minHz * std::pow (maxHz / minHz, juce::jlimit (0.0f, 1.0f, (x - r.getX()) / r.getWidth()));
}

float BandDisplay::attenToY (float db) const
{
    const auto r = plot();
    return r.getY() + r.getHeight() * juce::jlimit (0.0f, 1.0f, db / maxAttenDb);
}

float BandDisplay::gainDbToY (float db) const
{
    const auto r = plot();
    return r.getCentreY() - (r.getHeight() * 0.5f - 6.0f) * db / gainRangeDb;
}

float BandDisplay::crossover (int k) const { return physical (crossovers[size_t (k)]); }

float BandDisplay::bandCentreHz (int b) const
{
    const float lo = b == 0 ? minHz : crossover (b - 1);
    const float hi = b == dsp::numBands - 1 ? maxHz : crossover (b);
    return std::sqrt (lo * hi);
}

void BandDisplay::pushGains (const std::array<float, dsp::numBands>& linear)
{
    for (size_t b = 0; b < linear.size(); ++b)
    {
        const float a = juce::jlimit (0.0f, maxAttenDb, -linToDb (linear[b]));
        live[b] = a;
        trail[b] += (a - trail[b]) * (a > trail[b] ? 0.5f : 0.12f);
        if (a >= hold[b])
        {
            hold[b] = a;
            holdTimer[b] = 60;
        }
        else if (--holdTimer[b] <= 0)
            hold[b] = std::max (a, hold[b] - 0.3f);
    }
    repaint();
}

juce::Path BandDisplay::curve (const std::array<float, dsp::numBands>& atten) const
{
    // Stepped curve with soft corners through the bands' plateaus.
    juce::Path p;
    const auto r = plot();
    for (float x = r.getX(); x <= r.getRight(); x += 2.0f)
    {
        const float f = xToFreq (x);
        float value = 0.0f, sum = 0.0f;
        for (int b = 0; b < dsp::numBands; ++b)
        {
            float w = 1.0f;
            if (b > 0)
                w *= 1.0f / (1.0f + std::pow (crossover (b - 1) / f, 4.0f));
            if (b < dsp::numBands - 1)
                w *= 1.0f / (1.0f + std::pow (f / crossover (b), 4.0f));
            value += w * atten[size_t (b)];
            sum += w;
        }
        const float y = attenToY (sum > 0.0f ? value / sum : 0.0f);
        if (x == r.getX())
            p.startNewSubPath (x, y);
        else
            p.lineTo (x, y);
    }
    return p;
}

void BandDisplay::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (colours::well);
    g.fillRoundedRectangle (bounds, 6.0f);
    const auto r = plot();

    g.setFont (juce::Font (uiFont (10.0f)));
    for (float hz : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
    {
        const float x = freqToX (hz);
        g.setColour (colours::panelEdge.withAlpha (0.6f));
        g.drawVerticalLine (int (x), r.getY(), r.getBottom());
        g.setColour (colours::textDim);
        g.drawText (hz >= 1000.0f ? juce::String (int (hz / 1000.0f)) + "k" : juce::String (int (hz)),
                    juce::Rectangle<float> (x - 16.0f, r.getBottom() + 1.0f, 32.0f, 12.0f), juce::Justification::centred);
    }
    for (float db : { 6.0f, 12.0f, 18.0f, 24.0f })
    {
        const float y = attenToY (db);
        g.setColour (colours::panelEdge.withAlpha (0.6f));
        g.drawHorizontalLine (int (y), r.getX(), r.getRight());
        g.setColour (colours::textDim);
        g.drawText ("-" + juce::String (int (db)), juce::Rectangle<float> (r.getX() + 2.0f, y - 12.0f, 24.0f, 12.0f), juce::Justification::centredLeft);
    }

    g.setColour (colours::warn.withAlpha (0.5f));
    g.strokePath (curve (hold), juce::PathStrokeType (1.0f));
    {
        auto fill = curve (trail);
        fill.lineTo (r.getRight(), r.getY());
        fill.lineTo (r.getX(), r.getY());
        fill.closeSubPath();
        g.setColour (colours::warn.withAlpha (0.18f));
        g.fillPath (fill);
    }
    g.setColour (colours::warn);
    g.strokePath (curve (live), juce::PathStrokeType (1.6f));

    for (int k = 0; k < dsp::numCrossovers; ++k)
    {
        const float x = freqToX (crossover (k));
        g.setColour (dragCrossover == k ? colours::accent : colours::text.withAlpha (0.55f));
        g.drawLine (x, r.getY(), x, r.getBottom(), dragCrossover == k ? 2.0f : 1.0f);
        juce::Path tri;
        tri.addTriangle (x - 5.0f, r.getY(), x + 5.0f, r.getY(), x, r.getY() + 7.0f);
        g.fillPath (tri);
    }
    for (int b = 0; b < dsp::numBands; ++b)
    {
        const float x = freqToX (bandCentreHz (b));
        const float y = gainDbToY (physical (gains[size_t (b)]));
        g.setColour (bandColours[b]);
        g.fillEllipse (x - 6.0f, y - 6.0f, 12.0f, 12.0f);
        g.setColour (colours::background);
        g.setFont (juce::Font (uiFont (10.0f, true)));
        g.drawText (juce::String (b + 1), juce::Rectangle<float> (x - 6.0f, y - 6.0f, 12.0f, 12.0f), juce::Justification::centred);
    }
}

void BandDisplay::mouseDown (const juce::MouseEvent& e)
{
    const auto pos = e.position;
    dragBand = dragCrossover = -1;
    for (int b = 0; b < dsp::numBands; ++b)
        if (pos.getDistanceFrom ({ freqToX (bandCentreHz (b)), gainDbToY (physical (gains[size_t (b)])) }) < 9.0f)
            dragBand = b;
    if (dragBand >= 0)
    {
        gains[size_t (dragBand)]->beginChangeGesture();
        priorities[size_t (dragBand)]->beginChangeGesture();
        priorityOffset = physical (priorities[size_t (dragBand)]) - physical (gains[size_t (dragBand)]);
    }
    else
    {
        float best = 8.0f;
        for (int k = 0; k < dsp::numCrossovers; ++k)
        {
            const float d = std::abs (pos.x - freqToX (crossover (k)));
            if (d < best)
            {
                best = d;
                dragCrossover = k;
            }
        }
        if (dragCrossover >= 0)
            crossovers[size_t (dragCrossover)]->beginChangeGesture();
    }
    repaint();
}

void BandDisplay::mouseDrag (const juce::MouseEvent& e)
{
    if (dragCrossover >= 0)
        setPhysical (crossovers[size_t (dragCrossover)], xToFreq (e.position.x));
    else if (dragBand >= 0)
    {
        const auto r = plot();
        const float db = juce::jlimit (-gainRangeDb, gainRangeDb, (r.getCentreY() - e.position.y) / (r.getHeight() * 0.5f - 6.0f) * gainRangeDb);
        setPhysical (gains[size_t (dragBand)], db);
        if (gainPriorityLinked[size_t (dragBand)])
            setPhysical (priorities[size_t (dragBand)], juce::jlimit (-gainRangeDb, gainRangeDb, db + priorityOffset));
    }
    repaint();
}

void BandDisplay::mouseUp (const juce::MouseEvent&)
{
    if (dragCrossover >= 0)
        crossovers[size_t (dragCrossover)]->endChangeGesture();
    if (dragBand >= 0)
    {
        gains[size_t (dragBand)]->endChangeGesture();
        priorities[size_t (dragBand)]->endChangeGesture();
    }
    dragBand = dragCrossover = -1;
    repaint();
}

void BandDisplay::mouseDoubleClick (const juce::MouseEvent& e)
{
    for (int b = 0; b < dsp::numBands; ++b)
        if (e.position.getDistanceFrom ({ freqToX (bandCentreHz (b)), gainDbToY (physical (gains[size_t (b)])) }) < 9.0f)
        {
            gains[size_t (b)]->setValueNotifyingHost (gains[size_t (b)]->getDefaultValue());
            return;
        }
    for (int k = 0; k < dsp::numCrossovers; ++k)
        if (std::abs (e.position.x - freqToX (crossover (k))) < 8.0f)
        {
            crossovers[size_t (k)]->setValueNotifyingHost (crossovers[size_t (k)]->getDefaultValue());
            return;
        }
}

LevelMeter::LevelMeter (juce::RangedAudioParameter* markerParam, juce::Colour markerColour) : param (markerParam), colour (markerColour) {}

void LevelMeter::setLevel (float linearPeak)
{
    level = std::max (linearPeak, level * 0.85f);
    repaint();
}

void LevelMeter::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    const auto bar = r.withTrimmedLeft (8.0f).withTrimmedRight (8.0f);
    g.setColour (colours::well);
    g.fillRoundedRectangle (bar, 3.0f);
    const float top = dbToY (linToDb (level), bar);
    if (level > 1.0e-4f)
    {
        g.setColour (colours::accent2);
        g.fillRoundedRectangle (bar.withTop (top), 3.0f);
    }
    if (param != nullptr)
    {
        const float y = dbToY (param->convertFrom0to1 (param->getValue()), bar);
        g.setColour (colour);
        g.fillRect (r.getX(), y - 1.0f, r.getWidth(), 2.0f);
        juce::Path tri;
        tri.addTriangle (r.getX(), y - 5.0f, r.getX(), y + 5.0f, r.getX() + 7.0f, y);
        g.fillPath (tri);
    }
}

void LevelMeter::setMarker (float y)
{
    const float db = yToDb (y, getLocalBounds().toFloat());
    if (onMarkerDrag)
        onMarkerDrag (db);
    else
        param->setValueNotifyingHost (param->convertTo0to1 (db));
    repaint();
}

void LevelMeter::mouseDown (const juce::MouseEvent& e)
{
    if (param != nullptr)
    {
        param->beginChangeGesture();
        setMarker (e.position.y);
    }
}

void LevelMeter::mouseDrag (const juce::MouseEvent& e)
{
    if (param != nullptr)
        setMarker (e.position.y);
}

void LevelMeter::mouseUp (const juce::MouseEvent&)
{
    if (param != nullptr)
        param->endChangeGesture();
}

void LevelMeter::mouseDoubleClick (const juce::MouseEvent&)
{
    if (param != nullptr)
        param->setValueNotifyingHost (param->getDefaultValue());
}

void AttenuationMeter::setGain (float linear)
{
    const float a = juce::jlimit (0.0f, 30.0f, -linToDb (linear));
    atten = std::max (a, atten * 0.9f);
    if (a >= peakHold)
    {
        peakHold = a;
        holdTimer = 60;
    }
    else if (--holdTimer <= 0)
        peakHold = std::max (a, peakHold - 0.3f);
    repaint();
}

void AttenuationMeter::paint (juce::Graphics& g)
{
    auto bar = getLocalBounds().toFloat().withTrimmedLeft (8.0f).withTrimmedRight (8.0f);
    g.setColour (colours::well);
    g.fillRoundedRectangle (bar, 3.0f);
    g.setColour (colours::warn);
    g.fillRoundedRectangle (bar.withHeight (bar.getHeight() * atten / 30.0f), 3.0f);
    g.setColour (colours::text);
    g.fillRect (bar.getX(), bar.getY() + bar.getHeight() * peakHold / 30.0f - 1.0f, bar.getWidth(), 1.5f);
}

} // namespace craft::band_limiter::ui
