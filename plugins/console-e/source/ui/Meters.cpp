#include "Meters.h"

#include "LookAndFeel.h"

namespace craft::consolee::ui
{

// ---------------------------------------------------------------------------------------------
ReductionMeter::ReductionMeter (juce::String c, float range, std::initializer_list<float> t)
    : caption (std::move (c)), rangeDb (range), ticks (t)
{
}

void ReductionMeter::setReduction (float reductionDb)
{
    reductionDb = juce::jlimit (0.0f, rangeDb, reductionDb);
    // instant rise, smooth fall (display only)
    value = reductionDb > value ? reductionDb : value + (reductionDb - value) * 0.25f;
    if (reductionDb >= hold)
    {
        hold = reductionDb;
        holdCounter = 45;
    }
    else if (--holdCounter < 0)
        hold = std::max (0.0f, hold - 0.5f);
    repaint();
}

void ReductionMeter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    auto labelRow = r.removeFromBottom (12.0f);
    auto bar = r.reduced (0.0f, 1.0f);
    g.setColour (colours::well);
    g.fillRoundedRectangle (bar, 3.0f);

    auto xFor = [&bar, this] (float db) { return bar.getRight() - db / rangeDb * bar.getWidth(); };
    const float x = xFor (value);
    if (value > 0.05f)
    {
        g.setGradientFill (juce::ColourGradient (colours::accent2, bar.getRight(), 0, colours::accent, bar.getX(), 0, false));
        g.fillRoundedRectangle (juce::Rectangle<float>::leftTopRightBottom (x, bar.getY() + 2, bar.getRight() - 2, bar.getBottom() - 2), 2.0f);
    }
    if (hold > 0.05f)
    {
        g.setColour (colours::text.withAlpha (0.8f));
        g.fillRect (juce::Rectangle<float> (xFor (hold) - 1.0f, bar.getY() + 2, 2.0f, bar.getHeight() - 4));
    }
    g.setFont (juce::Font (uiFont (9.0f)));
    for (float t : ticks)
    {
        const float tx = xFor (t);
        g.setColour (colours::panelEdge.brighter (0.4f));
        g.fillRect (juce::Rectangle<float> (tx - 0.5f, bar.getBottom() - 4, 1.0f, 3.0f));
        g.setColour (colours::textDim);
        const float lx = juce::jlimit (labelRow.getX(), labelRow.getRight() - 24.0f, tx - 12.0f);
        g.drawText (juce::String (int (t)), juce::Rectangle<float> (lx, labelRow.getY(), 24, 12),
                    tx - 12.0f < labelRow.getX() ? juce::Justification::centredLeft
                    : tx + 12.0f > labelRow.getRight() ? juce::Justification::centredRight : juce::Justification::centred);
    }
    g.setColour (colours::textDim);
    g.setFont (juce::Font (uiFont (9.5f, true)));
    g.drawText (caption, bar.reduced (6.0f, 0.0f), juce::Justification::centredLeft);
}

// ---------------------------------------------------------------------------------------------
namespace
{
constexpr float meterMin = -60.0f, meterMax = 6.0f;
float fraction (float db) { return juce::jlimit (0.0f, 1.0f, (db - meterMin) / (meterMax - meterMin)); }
} // namespace

void LevelMeter::setLevels (float l, float r, bool isMono)
{
    mono = isMono;
    const float in[2] = { l, r };
    for (int c = 0; c < 2; ++c)
    {
        const float db = in[c] > 0.0f ? 20.0f * std::log10 (in[c]) : -100.0f;
        barDb[c] = db > barDb[c] ? db : std::max (db, barDb[c] - 1.2f); // ~36 dB/s fall at 30 Hz
        if (db >= peakDb[c])
        {
            peakDb[c] = db;
            peakHold[c] = 45;
        }
        else if (--peakHold[c] < 0)
            peakDb[c] -= 1.0f;
    }
    repaint();
}

void LevelMeter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    auto scale = r.removeFromLeft (22.0f);
    auto bars = r;
    g.setColour (colours::well);
    g.fillRoundedRectangle (bars, 3.0f);
    const int n = mono ? 1 : 2;
    const float gap = 3.0f;
    const float w = (bars.getWidth() - gap * float (n + 1)) / float (n);
    bars.reduce (0.0f, 6.0f); // room for the end labels of the scale
    auto yFor = [&bars] (float db) { return bars.getBottom() - 2.0f - fraction (db) * (bars.getHeight() - 4.0f); };
    for (int c = 0; c < n; ++c)
    {
        const auto col = juce::Rectangle<float> (bars.getX() + gap + float (c) * (w + gap), bars.getY() + 2.0f, w, bars.getHeight() - 4.0f);
        const float top = yFor (barDb[c]);
        if (barDb[c] > meterMin)
        {
            juce::ColourGradient grad (colours::accent2, 0, col.getBottom(), colours::warn, 0, yFor (meterMax), false);
            grad.addColour (fraction (-12.0f), colours::accent2);
            grad.addColour (fraction (-3.0f), colours::accent);
            g.setGradientFill (grad);
            g.fillRect (juce::Rectangle<float>::leftTopRightBottom (col.getX(), top, col.getRight(), col.getBottom()));
        }
        if (peakDb[c] > meterMin)
        {
            g.setColour (peakDb[c] > 0.0f ? colours::warn : colours::text);
            g.fillRect (juce::Rectangle<float> (col.getX(), yFor (peakDb[c]) - 1.0f, col.getWidth(), 2.0f));
        }
    }
    g.setFont (juce::Font (uiFont (9.0f)));
    for (float db : { 6.0f, 0.0f, -6.0f, -12.0f, -18.0f, -24.0f, -36.0f, -48.0f, -60.0f })
    {
        const float y = yFor (db);
        g.setColour (db == 0.0f ? colours::accent : colours::textDim);
        g.drawText (juce::String (int (db)), juce::Rectangle<float> (scale.getX(), y - 6.0f, scale.getWidth() - 3.0f, 12.0f),
                    juce::Justification::centredRight);
    }
}

} // namespace craft::consolee::ui
