#include "Meters.h"

#include "EnhancerLookAndFeel.h"

namespace craft::enhancer::ui
{

void StereoMeter::push (std::array<float, 2> peaks)
{
    for (size_t c = 0; c < 2; ++c)
    {
        const float db = peaks[c] > 1.0e-6f ? 20.0f * std::log10 (peaks[c]) : -120.0f;
        level[c] = juce::jmax (db, level[c] - 1.5f);
        hold[c] = juce::jmax (hold[c], db);
    }
    repaint();
}

void StereoMeter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds();
    auto readouts = r.removeFromTop (16);
    r.removeFromTop (2);
    const int w = (r.getWidth() - 3) / 2;
    const auto columns = std::array<juce::Rectangle<int>, 2> { r.withWidth (w), r.withTrimmedLeft (r.getWidth() - w) };
    g.setFont (juce::Font (uiFont (10.5f)));
    const int rw = readouts.getWidth() / 2;
    for (size_t c = 0; c < 2; ++c)
    {
        const auto col = columns[c].toFloat();
        g.setColour (colours::well);
        g.fillRoundedRectangle (col, 2.0f);
        const float t = juce::jlimit (0.0f, 1.0f, (level[c] + 60.0f) / 66.0f); // -60 .. +6 dB
        const auto bar = col.withTrimmedTop (col.getHeight() * (1.0f - t));
        g.setColour (level[c] > 0.0f ? colours::bypass : juce::Colour (0xff58c4b6));
        g.fillRect (bar);
        g.setColour (colours::textDim.withAlpha (0.5f));
        const float y0 = col.getY() + col.getHeight() * (6.0f / 66.0f);
        g.drawHorizontalLine (int (y0), col.getX(), col.getRight());
        auto cell = readouts.removeFromLeft (rw);
        g.setColour (hold[c] > 0.0f ? colours::bypass : colours::textDim);
        g.drawText (hold[c] <= -119.0f ? juce::String ("-inf") : juce::String (hold[c], 1), cell, juce::Justification::centred);
    }
}

} // namespace craft::enhancer::ui
