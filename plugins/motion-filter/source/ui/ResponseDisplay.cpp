#include "ResponseDisplay.h"

#include <complex>

#include "MotionLookAndFeel.h"
#include "dsp/Laws.h"
#include "dsp/Vcf.h"

namespace craft::mfp::ui
{

ResponseDisplay::ResponseDisplay (MotionFilterProcessor& p) : proc (p)
{
    auto& st = proc.getState();
    freqParam = st.getParameter (ids::freq);
    resParam = st.getParameter (ids::res);
    if (freqParam != nullptr)
        freqAttachment = std::make_unique<juce::ParameterAttachment> (*freqParam, [] (float) {});
    if (resParam != nullptr)
        resAttachment = std::make_unique<juce::ParameterAttachment> (*resParam, [] (float) {});
    setTooltip ("Drag: horizontal = Freq / Delay / Gain, vertical = Resonance / Feedback. Double-click resets.");
    setMouseCursor (juce::MouseCursor::CrosshairCursor);
    update();
}

void ResponseDisplay::update()
{
    settings = proc.readSettings();
    for (int c = 0; c < 2; ++c)
    {
        cutoff[c] = proc.live.cutoff[c].load();
        combMs[c] = proc.live.combMs[c].load();
        resonance[c] = proc.live.resonance[c].load();
    }
    repaint();
}

juce::Rectangle<float> ResponseDisplay::plot() const { return getLocalBounds().toFloat().reduced (8.0f, 8.0f).withTrimmedBottom (10.0f); }

float ResponseDisplay::xOf (double f) const
{
    const auto p = plot();
    return p.getX() + p.getWidth() * float (std::log (f / fMin) / std::log (fMax / fMin));
}

double ResponseDisplay::fOf (float x) const
{
    const auto p = plot();
    return fMin * std::pow (fMax / fMin, double ((x - p.getX()) / p.getWidth()));
}

float ResponseDisplay::yOf (double db) const
{
    const auto p = plot();
    db = juce::jlimit (dbMin, dbMax, db);
    return p.getY() + p.getHeight() * float ((dbMax - db) / (dbMax - dbMin));
}

double ResponseDisplay::magnitudeDb (int c, double f) const
{
    switch (settings.type)
    {
        case mf::FilterType::Comb:
        {
            // y[n] = x[n-D] + g y[n-D]  ->  |H| = 1 / |1 - g e^(-jwD)|
            const double g = mf::law::combGain (resonance[c]);
            const double wd = 2.0 * mf::law::pi * f * combMs[c] * 0.001;
            const std::complex<double> den = 1.0 - g * std::polar (1.0, -wd);
            return 20.0 * std::log10 (1.0 / std::abs (den));
        }
        case mf::FilterType::Amp:
        {
            const double db = mf::param::ampGainDb (settings.freqNorm);
            return db <= -79.9 ? dbMin - 10.0 : db;
        }
        default:
        {
            const double q = mf::law::qFromResonance (resonance[c]);
            const double m = mf::prototypeMagnitude (settings.type, settings.slope24, f, cutoff[c], q);
            return 20.0 * std::log10 (std::max (m, 1.0e-6));
        }
    }
}

juce::Path ResponseDisplay::curve (int c) const
{
    juce::Path path;
    const auto p = plot();
    const int n = std::max (64, int (p.getWidth()) * (settings.type == mf::FilterType::Comb ? 8 : 1));
    for (int i = 0; i <= n; ++i)
    {
        const float x = p.getX() + p.getWidth() * float (i) / float (n);
        const float y = yOf (magnitudeDb (c, fOf (x)));
        if (i == 0)
            path.startNewSubPath (x, y);
        else
            path.lineTo (x, y);
    }
    return path;
}

void ResponseDisplay::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    g.setColour (colours::well);
    g.fillRoundedRectangle (area, 6.0f);
    const auto p = plot();

    // grid: decades and dB lines
    g.setFont (juce::Font (uiFont (9.5f)));
    for (double f : { 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0 })
    {
        const float x = xOf (f);
        g.setColour (colours::grid);
        g.drawVerticalLine (int (x), p.getY(), p.getBottom());
        g.setColour (colours::textDim.withAlpha (0.7f));
        g.drawText (f >= 1000.0 ? juce::String (int (f / 1000.0)) + "k" : juce::String (int (f)),
                    juce::Rectangle<float> (x - 16.0f, p.getBottom() + 1.0f, 32.0f, 11.0f), juce::Justification::centred);
    }
    for (double db : { -24.0, -12.0, 0.0, 12.0, 24.0 })
    {
        const float y = yOf (db);
        g.setColour (db == 0.0 ? colours::grid.brighter (0.25f) : colours::grid);
        g.drawHorizontalLine (int (y), p.getX(), p.getRight());
        g.setColour (colours::textDim.withAlpha (0.6f));
        g.drawText ((db > 0 ? "+" : "") + juce::String (int (db)), juce::Rectangle<float> (p.getX() + 2.0f, y - 11.0f, 30.0f, 10.0f),
                    juce::Justification::centredLeft);
    }

    // curves: right channel (if spread / follower make it differ) behind, left in front
    const bool stereo = std::abs (cutoff[0] - cutoff[1]) > 0.5 || std::abs (combMs[0] - combMs[1]) > 1.0e-3
                        || std::abs (resonance[0] - resonance[1]) > 0.05;
    juce::Graphics::ScopedSaveState clip (g);
    g.reduceClipRegion (p.toNearestInt().expanded (0, 4));
    for (int c = stereo ? 1 : 0; c >= 0; --c)
    {
        const auto path = curve (c);
        const auto colour = c == 0 ? colours::accent : colours::lfo;
        juce::Path fill (path);
        fill.lineTo (p.getRight(), p.getBottom());
        fill.lineTo (p.getX(), p.getBottom());
        fill.closeSubPath();
        g.setGradientFill (juce::ColourGradient (colour.withAlpha (0.22f), 0.0f, p.getY(), colour.withAlpha (0.02f), 0.0f,
                                                 p.getBottom(), false));
        g.fillPath (fill);
        g.setColour (colour);
        g.strokePath (path, juce::PathStrokeType (c == 0 ? 2.0f : 1.4f, juce::PathStrokeType::curved));
    }

    // handle at the knob position (x = Freq knob, y = resonance knob)
    const double knobHz = mf::param::Freq::fromNorm (settings.freqNorm);
    const float hx = xOf (std::max (fMin, knobHz));
    const float hy = p.getBottom() - p.getHeight() * float (settings.resonance / 100.0);
    g.setColour (colours::text.withAlpha (dragging ? 0.9f : 0.6f));
    g.drawEllipse (juce::Rectangle<float> (12.0f, 12.0f).withCentre ({ hx, hy }), 1.5f);
    g.fillEllipse (juce::Rectangle<float> (4.0f, 4.0f).withCentre ({ hx, hy }));
}

void ResponseDisplay::mouseDown (const juce::MouseEvent& e)
{
    dragging = true;
    if (freqAttachment) freqAttachment->beginGesture();
    if (resAttachment) resAttachment->beginGesture();
    mouseDrag (e);
}

void ResponseDisplay::mouseDrag (const juce::MouseEvent& e)
{
    const auto p = plot();
    const double hz = juce::jlimit (mf::param::Freq::minValue, mf::param::Freq::maxValue, fOf (e.position.x));
    const double res = juce::jlimit (0.0, 100.0, double ((p.getBottom() - e.position.y) / p.getHeight() * 100.0f));
    if (freqAttachment) freqAttachment->setValueAsPartOfGesture (float (hz));
    if (resAttachment) resAttachment->setValueAsPartOfGesture (float (res));
    update();
}

void ResponseDisplay::mouseUp (const juce::MouseEvent&)
{
    dragging = false;
    if (freqAttachment) freqAttachment->endGesture();
    if (resAttachment) resAttachment->endGesture();
    repaint();
}

void ResponseDisplay::mouseDoubleClick (const juce::MouseEvent&)
{
    if (freqAttachment && freqParam) freqAttachment->setValueAsCompleteGesture (freqParam->convertFrom0to1 (freqParam->getDefaultValue()));
    if (resAttachment && resParam) resAttachment->setValueAsCompleteGesture (resParam->convertFrom0to1 (resParam->getDefaultValue()));
}

} // namespace craft::mfp::ui
