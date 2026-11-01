#include "BandDisplay.h"

#include "EnhancerLookAndFeel.h"

namespace craft::enhancer::ui
{

namespace
{
constexpr double minHz = 16.0, maxHz = 22000.0;
constexpr float floorDb = -48.0f;
} // namespace

BandDisplay::BandDisplay (EnhancerProcessor& p) : proc (p)
{
    shown = proc.readSettings();
}

float BandDisplay::xForHz (double hz) const
{
    return float (std::log (hz / minHz) / std::log (maxHz / minHz)) * float (getWidth());
}

double BandDisplay::hzForX (float x) const
{
    return minHz * std::exp (double (x) / juce::jmax (1, getWidth()) * std::log (maxHz / minHz));
}

int BandDisplay::handleAt (float x) const
{
    const auto f = dsp::orderedCrossovers (shown.crossoverHz);
    int best = -1;
    float bestDist = 9.0f;
    for (int k = 0; k < dsp::numCrossovers; ++k)
    {
        const float d = std::abs (xForHz (f[(size_t) k]) - x);
        if (d < bestDist)
        {
            bestDist = d;
            best = k;
        }
    }
    return best;
}

void BandDisplay::refresh()
{
    const auto s = proc.readSettings();
    if (s.crossoverHz != shown.crossoverHz || s.solo != shown.solo || s.bypass != shown.bypass || s.bandDb != shown.bandDb)
    {
        shown = s;
        repaint();
    }
}

void BandDisplay::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    g.setColour (colours::well);
    g.fillRoundedRectangle (r, 5.0f);

    g.setFont (juce::Font (uiFont (10.0f)));
    for (double hz : { 20.0, 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0, 20000.0 })
    {
        const float x = xForHz (hz);
        g.setColour (colours::grid);
        g.drawVerticalLine (int (x), r.getY(), r.getBottom());
        g.setColour (colours::textDim);
        g.drawText (hz >= 1000.0 ? juce::String (hz / 1000.0, 0) + "k" : juce::String (hz, 0), int (x) + 2, getHeight() - 14, 30, 12, juce::Justification::left);
    }

    const double fs = juce::jmax (44100.0, proc.getCurrentSampleRate());
    const auto f = dsp::orderedCrossovers (shown.crossoverHz);
    const int w = juce::jmax (1, getWidth() / 3);
    std::array<juce::Path, dsp::numBands> paths;
    const float h = r.getHeight() - 16.0f;
    auto yFor = [&] (double mag) {
        const float db = float (20.0 * std::log10 (std::max (mag, 1.0e-6)));
        return juce::jlimit (0.0f, h, 4.0f + db / floorDb * (h - 4.0f));
    };
    for (int i = 0; i <= w; ++i)
    {
        const float x = float (i) * r.getWidth() / float (w);
        const double hz = std::min (hzForX (x), 0.49 * fs);
        const auto resp = dsp::bandResponses (f, hz, fs);
        for (size_t b = 0; b < dsp::numBands; ++b)
        {
            const float y = yFor (resp[b].abs());
            if (i == 0)
                paths[b].startNewSubPath (x, y);
            else
                paths[b].lineTo (x, y);
        }
    }
    bool anySolo = false;
    for (bool s : shown.solo)
        anySolo = anySolo || s;
    for (size_t b = 0; b < dsp::numBands; ++b)
    {
        const bool audible = ! anySolo || shown.solo[b];
        const auto col = bandColour ((int) b).withMultipliedAlpha (audible ? 1.0f : 0.25f);
        auto fill = paths[b];
        fill.lineTo (r.getWidth(), h);
        fill.lineTo (0.0f, h);
        fill.closeSubPath();
        const bool active = ! dsp::param::BandGain::isOff (shown.bandDb[b]) && ! shown.bypass[b];
        g.setColour (col.withAlpha (audible ? (active ? 0.28f : 0.08f) : 0.03f));
        g.fillPath (fill);
        g.setColour (col);
        g.strokePath (paths[b], juce::PathStrokeType (1.5f));
    }

    static const char* names[] = { "LO", "LM", "HM", "HI" };
    for (int k = 0; k < dsp::numCrossovers; ++k)
    {
        const float x = xForHz (f[(size_t) k]);
        g.setColour (colours::text.withAlpha (k == dragging ? 1.0f : 0.7f));
        g.fillRect (x - 0.75f, 0.0f, 1.5f, h);
        g.fillRoundedRectangle (x - 11.0f, 2.0f, 22.0f, 13.0f, 3.0f);
        g.setColour (colours::background);
        g.setFont (juce::Font (uiFont (9.5f, true)));
        g.drawText (names[k], int (x) - 11, 2, 22, 13, juce::Justification::centred);
    }
}

void BandDisplay::mouseMove (const juce::MouseEvent& e)
{
    setMouseCursor (handleAt (e.position.x) >= 0 ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::NormalCursor);
}

void BandDisplay::mouseDown (const juce::MouseEvent& e)
{
    dragging = handleAt (e.position.x);
    if (dragging >= 0)
        if (auto* p = proc.getState().getParameter (ids::crossover (dragging)))
            p->beginChangeGesture();
    repaint();
}

float BandDisplay::limitToNeighbours (EnhancerProcessor& proc, int k, float norm)
{
    auto& st = proc.getState();
    if (k > 0)
        norm = juce::jmax (norm, st.getParameter (ids::crossover (k - 1))->getValue());
    if (k < dsp::numCrossovers - 1)
        norm = juce::jmin (norm, st.getParameter (ids::crossover (k + 1))->getValue());
    return norm;
}

void BandDisplay::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging < 0)
        return;
    const auto norm = float (dsp::param::Crossover::toNorm (hzForX (e.position.x)));
    if (auto* p = proc.getState().getParameter (ids::crossover (dragging)))
        p->setValueNotifyingHost (limitToNeighbours (proc, dragging, norm));
}

void BandDisplay::mouseUp (const juce::MouseEvent&)
{
    if (dragging >= 0)
        if (auto* p = proc.getState().getParameter (ids::crossover (dragging)))
            p->endChangeGesture();
    dragging = -1;
    repaint();
}

} // namespace craft::enhancer::ui
