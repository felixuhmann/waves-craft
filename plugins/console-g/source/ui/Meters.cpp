#include "Meters.h"

#include "ConsoleLookAndFeel.h"

namespace craft::consoleg::ui
{

BarMeter::BarMeter (bool isReduction) : reduction (isReduction)
{
    if (reduction)
        shown[0] = shown[1] = peak[0] = peak[1] = 0.0f;
}

float BarMeter::fraction (float db) const noexcept
{
    if (reduction)
        return juce::jlimit (0.0f, 1.0f, -db / 30.0f);
    return juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 66.0f);
}

void BarMeter::push (const float* db, float dt)
{
    for (int c = 0; c < (reduction ? 1 : channels); ++c)
    {
        if (reduction)
        {
            // more reduction = more negative: rise instantly, recover at 20 dB/s
            shown[c] = db[c] < shown[c] ? db[c] : std::min (0.0f, shown[c] + 20.0f * dt);
            if (db[c] < peak[c] || (peakAge[c] += dt) > 1.5f)
            {
                peak[c] = db[c];
                peakAge[c] = 0.0f;
            }
        }
        else
        {
            shown[c] = db[c] > shown[c] ? db[c] : std::max (-100.0f, shown[c] - 20.0f * dt);
            if (db[c] > peak[c] || (peakAge[c] += dt) > 1.5f)
            {
                peak[c] = db[c];
                peakAge[c] = 0.0f;
            }
        }
    }
    repaint();
}

void BarMeter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    auto scaleArea = r.removeFromLeft (20.0f);
    g.setColour (colours::well);
    g.fillRoundedRectangle (r, 3.0f);

    // scale
    g.setFont (juce::Font (uiFont (9.0f)));
    const float marks[] = { 0.0f, -6.0f, -12.0f, -18.0f, -24.0f, -30.0f, -42.0f, -54.0f };
    const float rmarks[] = { 0.0f, -3.0f, -6.0f, -10.0f, -15.0f, -20.0f, -30.0f };
    auto yOf = [&] (float db) {
        const float f = fraction (db);
        return reduction ? r.getY() + 2.0f + f * (r.getHeight() - 4.0f) : r.getBottom() - 2.0f - f * (r.getHeight() - 4.0f);
    };
    const float* m = reduction ? rmarks : marks;
    const int nm = reduction ? 7 : 8;
    for (int i = 0; i < nm; ++i)
    {
        const float y = yOf (m[i]);
        g.setColour (colours::textDim);
        g.drawText (juce::String (int (m[i])), scaleArea.withY (y - 6.0f).withHeight (12.0f).withTrimmedRight (3.0f),
                    juce::Justification::centredRight);
        g.setColour (colours::panelEdge);
        g.drawHorizontalLine (int (y), r.getX(), r.getRight());
    }

    const int n = reduction ? 1 : channels;
    const float gap = 3.0f, w = (r.getWidth() - gap * float (n + 1)) / float (n);
    for (int c = 0; c < n; ++c)
    {
        const float x = r.getX() + gap + float (c) * (w + gap);
        if (reduction)
        {
            const float y1 = yOf (shown[c]);
            g.setColour (colours::comp.withAlpha (0.85f));
            g.fillRect (juce::Rectangle<float> (x, r.getY() + 2.0f, w, std::max (0.0f, y1 - r.getY() - 2.0f)));
            g.setColour (colours::text);
            g.fillRect (juce::Rectangle<float> (x, yOf (peak[c]) - 1.0f, w, 2.0f));
        }
        else
        {
            const float y0 = yOf (shown[c]);
            juce::ColourGradient grad (colours::expander, x, r.getBottom(), colours::warn, x, yOf (6.0f), false);
            grad.addColour (fraction (-12.0f), colours::expander);
            grad.addColour (fraction (-3.0f), colours::comp);
            g.setGradientFill (grad);
            g.fillRect (juce::Rectangle<float> (x, y0, w, std::max (0.0f, r.getBottom() - 2.0f - y0)));
            g.setColour (peak[c] > 0.0f ? colours::warn : colours::text);
            g.fillRect (juce::Rectangle<float> (x, yOf (peak[c]) - 1.0f, w, 2.0f));
        }
    }
}

} // namespace craft::consoleg::ui
