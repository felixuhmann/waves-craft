#include "Widgets.h"

#include "HDelayLookAndFeel.h"
#include "dsp/DelayTime.h"
#include "dsp/Parameters.h"

namespace craft::hdelay::ui
{

namespace
{
constexpr float rotaryStart = juce::MathConstants<float>::pi * 1.25f;
constexpr float rotaryEnd = juce::MathConstants<float>::pi * 2.75f;

void beginSet (juce::RangedAudioParameter* p)
{
    if (p != nullptr)
        p->beginChangeGesture();
}
void endSet (juce::RangedAudioParameter* p)
{
    if (p != nullptr)
        p->endChangeGesture();
}
} // namespace

// =============================================================================================
Knob::Knob (juce::AudioProcessorValueTreeState& state, const juce::String& paramId, const juce::String& t, bool isLarge)
    : attachment (state, paramId, slider), title (t), large (isLarge)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setRotaryParameters (rotaryStart, rotaryEnd, true);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 90, 18);
    slider.setTextBoxIsEditable (true);
    slider.setMouseDragSensitivity (large ? 300 : 220);
    slider.setVelocityBasedMode (false);
    auto* p = state.getParameter (paramId);
    slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()), juce::ModifierKeys::altModifier);
    slider.setTooltip (title + " - drag, double-click or Alt-click to reset, click the value to type");
    addAndMakeVisible (slider);
}

void Knob::setScaleEnds (const juce::String& l, const juce::String& h)
{
    if (l != lo || h != hi)
    {
        lo = l;
        hi = h;
        repaint();
    }
}

void Knob::setDetentLabels (const juce::StringArray& labels)
{
    detents = labels;
    repaint();
}

void Knob::setAccent (juce::Colour c)
{
    slider.setColour (juce::Slider::thumbColourId, c);
}

juce::Rectangle<int> Knob::knobArea() const
{
    auto r = slider.getBounds();
    r.removeFromBottom (20);
    const int s = std::min (r.getWidth(), r.getHeight());
    return r.withSizeKeepingCentre (s, s);
}

void Knob::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (large ? 20 : 17);
    if (! detents.isEmpty() || lo.isNotEmpty())
        r = r.reduced (large ? 14 : 10, 0);
    if (! detents.isEmpty())
        r = r.withTrimmedTop (12);
    slider.setBounds (r);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, std::min (getWidth(), 96), 18);
}

void Knob::paint (juce::Graphics& g)
{
    g.setColour (colours::textDim);
    g.setFont (juce::Font (uiFont (large ? 13.0f : 11.5f, true)));
    g.drawText (title.toUpperCase(), getLocalBounds().removeFromTop (large ? 18 : 15), juce::Justification::centred);

    const auto k = knobArea().toFloat();
    const auto c = k.getCentre();
    const float radius = k.getWidth() * 0.5f;
    g.setFont (juce::Font (uiFont (10.0f)));
    auto labelAt = [&] (float angle, const juce::String& text, float extra) {
        const float rr = radius + extra;
        const juce::Point<float> p (c.x + rr * std::sin (angle), c.y - rr * std::cos (angle));
        g.drawText (text, juce::Rectangle<float> (44.0f, 12.0f).withCentre (p), juce::Justification::centred);
    };
    if (lo.isNotEmpty())
    {
        g.setColour (colours::textDim);
        labelAt (rotaryStart, lo, 14.0f);
        labelAt (rotaryEnd, hi, 14.0f);
    }
    if (detents.size() > 1)
    {
        const int sel = juce::roundToInt (slider.getValue() - slider.getMinimum());
        for (int i = 0; i < detents.size(); ++i)
        {
            const float a = rotaryStart + (rotaryEnd - rotaryStart) * float (i) / float (detents.size() - 1);
            g.setColour (i == sel ? colours::accent : colours::textDim);
            labelAt (a, detents[i], 11.0f);
        }
    }
}

// =============================================================================================
void TapPad::mouseDown (const juce::MouseEvent&)
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    const double interval = now - lastTapMs;
    lastTapMs = now;
    if (interval >= 40.0 && interval <= 4000.0)
    {
        ++count;
        if (onTap)
            onTap (interval);
    }
    else
        count = 0;
    flash = 1.0f;
    startTimerHz (60);
    repaint();
}

void TapPad::timerCallback()
{
    flash = std::max (0.0f, flash - 0.08f);
    if (flash <= 0.0f)
        stopTimer();
    repaint();
}

void TapPad::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    juce::ColourGradient grad (juce::Colour (0xff262b31), r.getX(), r.getY(), juce::Colour (0xff121518), r.getX(), r.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (r, 10.0f);
    if (flash > 0.0f)
    {
        g.setColour (colours::accent.withAlpha (0.55f * flash));
        g.fillRoundedRectangle (r, 10.0f);
    }
    g.setColour (isMouseOver() ? colours::accent.withAlpha (0.8f) : colours::panelEdge.brighter (0.3f));
    g.drawRoundedRectangle (r, 10.0f, 1.5f);
    // inner pad texture: concentric rounded squares
    g.setColour (colours::panelEdge.withAlpha (0.6f));
    g.drawRoundedRectangle (r.reduced (r.getWidth() * 0.16f), 7.0f, 1.0f);

    g.setColour (flash > 0.3f ? colours::background : colours::text);
    g.setFont (juce::Font (uiFont (20.0f, true)));
    g.drawText ("TAP", r, juce::Justification::centred);
    // beat dots
    const float d = 5.0f;
    for (int i = 0; i < 4; ++i)
    {
        const auto dot = juce::Rectangle<float> (d, d).withCentre ({ r.getCentreX() + (float (i) - 1.5f) * 11.0f, r.getBottom() - 14.0f });
        g.setColour (i < (count % 5) ? colours::accent : colours::well);
        g.fillEllipse (dot);
    }
}

// =============================================================================================
DelayDisplay::DelayDisplay (HDelayProcessor& p) : proc (p)
{
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
    refresh();
}

void DelayDisplay::refresh()
{
    shown = proc.readSettings();
    repaint();
}

juce::Rectangle<float> DelayDisplay::valueArea (int line) const
{
    auto r = getLocalBounds().toFloat().reduced (10.0f, 0.0f);
    r.removeFromTop (20.0f);
    r.removeFromBottom (22.0f);
    if (! shown.dual)
        return r;
    return line == 0 ? r.removeFromTop (r.getHeight() * 0.5f) : r.withTrimmedTop (r.getHeight() * 0.5f);
}

juce::Rectangle<float> DelayDisplay::tempoArea() const
{
    return getLocalBounds().toFloat().reduced (10.0f, 0.0f).removeFromBottom (22.0f).removeFromLeft (getWidth() * 0.5f);
}

juce::Rectangle<float> DelayDisplay::startArea() const
{
    return getLocalBounds().toFloat().reduced (8.0f, 3.0f).removeFromBottom (18.0f).removeFromRight (96.0f);
}

DelayDisplay::Target DelayDisplay::hitTest (juce::Point<int> pt) const
{
    const auto p = pt.toFloat();
    if (shown.pingPong && startArea().contains (p))
        return Target::Start;
    if (shown.sync == param::Sync::Bpm && tempoArea().contains (p))
        return Target::Tempo;
    if (shown.dual && valueArea (1).contains (p))
        return Target::Right;
    if (valueArea (0).contains (p) || ! shown.dual)
        return Target::Left;
    return Target::None;
}

const char* DelayDisplay::paramFor (Target t) const
{
    const bool ms = shown.sync == param::Sync::Ms;
    switch (t)
    {
        case Target::Left:  return ms ? ids::delayMs : ids::delayNote;
        case Target::Right: return ms ? ids::delayRMs : ids::delayRNote;
        case Target::Tempo: return ids::bpm;
        default:            return nullptr;
    }
}

void DelayDisplay::mouseDown (const juce::MouseEvent& e)
{
    dragTarget = hitTest (e.getPosition());
    dragParam = nullptr;
    if (dragTarget == Target::Start)
    {
        if (auto* p = proc.getState().getParameter (ids::pingPongStart))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->getValue() >= 0.5f ? 0.0f : 1.0f);
            p->endChangeGesture();
        }
        refresh();
        return;
    }
    if (auto* id = paramFor (dragTarget))
    {
        dragParam = proc.getState().getParameter (id);
        dragStartNorm = dragParam->getValue();
        beginSet (dragParam);
    }
}

void DelayDisplay::mouseDrag (const juce::MouseEvent& e)
{
    if (dragParam == nullptr)
        return;
    const float dy = float (e.getDistanceFromDragStartY());
    float norm;
    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (dragParam))
    {
        const int n = choice->choices.size();
        const int start = juce::roundToInt (dragStartNorm * float (n - 1));
        const int idx = juce::jlimit (0, n - 1, start - juce::roundToInt (dy / 12.0f));
        norm = float (idx) / float (n - 1);
    }
    else
    {
        const float sens = e.mods.isShiftDown() ? 0.0004f : 0.003f;
        norm = juce::jlimit (0.0f, 1.0f, dragStartNorm - dy * sens);
    }
    dragParam->setValueNotifyingHost (norm);
    refresh();
}

void DelayDisplay::mouseUp (const juce::MouseEvent&)
{
    endSet (dragParam);
    dragParam = nullptr;
}

void DelayDisplay::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (auto* id = paramFor (hitTest (e.getPosition())))
        if (auto* p = proc.getState().getParameter (id))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->getDefaultValue());
            p->endChangeGesture();
        }
    refresh();
}

void DelayDisplay::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (colours::lcd);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (colours::accent2.withAlpha (0.25f));
    g.drawRoundedRectangle (r, 6.0f, 1.0f);
    // faint scan lines
    g.setColour (juce::Colours::white.withAlpha (0.018f));
    for (float y = r.getY() + 3.0f; y < r.getBottom() - 2.0f; y += 3.0f)
        g.drawHorizontalLine (int (y), r.getX() + 4.0f, r.getRight() - 4.0f);

    const auto lit = colours::accent2;
    const auto dim = colours::accent2.withAlpha (0.45f);
    const auto& s = shown;
    const bool noteMode = s.sync != param::Sync::Ms;
    const double tempo = s.sync == param::Sync::Host ? s.hostBpm : s.bpm;

    // header line
    auto top = getLocalBounds().toFloat().reduced (10.0f, 3.0f).removeFromTop (16.0f);
    g.setFont (juce::Font (uiFont (10.5f, true)));
    g.setColour (dim);
    const char* modeText = s.sync == param::Sync::Host ? "HOST SYNC" : (s.sync == param::Sync::Bpm ? "BPM SYNC" : "MILLISECONDS");
    g.drawText (modeText, top, juce::Justification::centredLeft);
    juce::String tags;
    if (s.dual) tags << "DUAL  ";
    if (s.pingPong) tags << "PING PONG";
    g.drawText (tags.trim(), top, juce::Justification::centredRight);

    auto valueText = [&] (int c) -> juce::String {
        if (! noteMode)
            return juce::String (param::formatMs (resolvedDelayMs (s, c)));
        const int note = (c == 1 && s.dual) ? s.delayRNote : s.delayNote;
        const int eff = effectiveNote (note, std::clamp (tempo, 1.0, 1000.0));
        juce::String t (param::noteNames[size_t (eff)]);
        if (eff != note)
            t << "*"; // longer than 3500 ms: falls back to the longest fitting note
        return t;
    };

    const bool hot = isMouseOverOrDragging();
    if (! s.dual)
    {
        auto a = valueArea (0);
        if (noteMode)
        {
            g.setColour (dim);
            g.setFont (juce::Font (uiFont (12.0f, true)));
            g.drawText (juce::String (param::formatMs (resolvedDelayMs (s, 0))), a.removeFromBottom (16.0f),
                        juce::Justification::centred);
        }
        g.setColour (lit);
        g.setFont (juce::Font (uiFont (std::min (a.getHeight() * 0.62f, 52.0f), true)));
        g.drawFittedText (valueText (0), a.toNearestInt(), juce::Justification::centred, 1, 0.6f);
    }
    else
    {
        for (int c = 0; c < 2; ++c)
        {
            auto a = valueArea (c);
            g.setColour (dim);
            g.setFont (juce::Font (uiFont (12.0f, true)));
            g.drawText (c == 0 ? "L" : "R", a.removeFromLeft (18.0f), juce::Justification::centredLeft);
            g.setColour (lit);
            g.setFont (juce::Font (uiFont (std::min (a.getHeight() * 0.6f, 34.0f), true)));
            g.drawFittedText (valueText (c), a.toNearestInt(), juce::Justification::centred, 1, 0.6f);
            if (noteMode)
            {
                g.setColour (dim);
                g.setFont (juce::Font (uiFont (10.5f)));
                g.drawText (juce::String (param::formatMs (resolvedDelayMs (s, c))), a.removeFromRight (64.0f),
                            juce::Justification::centredRight);
            }
        }
    }

    // footer: tempo and ping-pong start selector
    g.setFont (juce::Font (uiFont (11.5f, true)));
    if (noteMode)
    {
        g.setColour (s.sync == param::Sync::Bpm && hot ? lit : dim);
        juce::String t = (s.sync == param::Sync::Host ? "HOST " : "") + juce::String (tempo, 1) + " BPM";
        g.drawText (t, tempoArea(), juce::Justification::centredLeft);
    }
    if (s.pingPong)
    {
        auto a = startArea();
        g.setColour (dim);
        g.setFont (juce::Font (uiFont (10.0f, true)));
        g.drawText ("START", a.removeFromLeft (38.0f), juce::Justification::centredLeft);
        auto lBox = a.removeFromLeft (a.getWidth() * 0.5f).reduced (1.5f, 1.0f);
        auto rBox = a.reduced (1.5f, 1.0f);
        for (int i = 0; i < 2; ++i)
        {
            const bool sel = (i == 1) == s.pingPongStartRight;
            const auto box = i == 0 ? lBox : rBox;
            g.setColour (sel ? lit : colours::well);
            g.fillRoundedRectangle (box, 3.0f);
            g.setColour (sel ? colours::lcd : dim);
            g.setFont (juce::Font (uiFont (11.0f, true)));
            g.drawText (i == 0 ? "L" : "R", box, juce::Justification::centred);
        }
    }
}

// =============================================================================================
void OutputMeter::pushPeaks (float l, float r)
{
    const float in[2] = { l, r };
    for (int c = 0; c < 2; ++c)
    {
        const float d = in[c] > 0.0f ? 20.0f * std::log10 (in[c]) : -100.0f;
        levelDb[c] = std::max (d, levelDb[c] - 1.2f); // ~36 dB/s fall at 30 Hz refresh
        if (d >= peakHoldDb)
        {
            peakHoldDb = d;
            peakHoldFrames = 45;
        }
        if (in[c] >= 1.0f)
            clipHold = 60;
    }
    if (peakHoldFrames > 0 && --peakHoldFrames == 0)
        peakHoldDb = std::max (levelDb[0], levelDb[1]);
    if (clipHold > 0)
        --clipHold;
    repaint();
}

void OutputMeter::paint (juce::Graphics& g)
{
    constexpr float floorDb = -48.0f;
    auto r = getLocalBounds().toFloat();
    // LED
    auto led = juce::Rectangle<float> (12.0f, 12.0f).withCentre ({ r.getCentreX() + 8.0f, r.getY() + 9.0f });
    const bool clip = clipHold > 0;
    const bool hotLed = peakHoldDb > -6.0f;
    g.setColour (clip ? colours::warn : (hotLed ? colours::accent : colours::well));
    g.fillEllipse (led);
    if (clip)
    {
        g.setColour (colours::warn.withAlpha (0.3f));
        g.fillEllipse (led.expanded (3.0f));
    }
    g.setColour (colours::panelEdge.brighter (0.3f));
    g.drawEllipse (led, 1.0f);

    auto bars = r.withTrimmedTop (24.0f).withTrimmedBottom (4.0f);
    auto scale = bars.removeFromLeft (24.0f);
    const float barW = 8.0f;
    auto b0 = juce::Rectangle<float> (bars.getX() + 2.0f, bars.getY(), barW, bars.getHeight());
    auto b1 = b0.translated (barW + 4.0f, 0.0f);

    g.setFont (juce::Font (uiFont (9.5f)));
    for (int db = 0; db >= -48; db -= 6)
    {
        const float y = bars.getY() + bars.getHeight() * float (db) / floorDb;
        g.setColour (colours::panelEdge.brighter (0.3f));
        g.drawHorizontalLine (int (y), scale.getRight() - 4.0f, scale.getRight());
        if (db % 12 == 0)
        {
            g.setColour (colours::textDim);
            g.drawText (juce::String (db), juce::Rectangle<float> (scale.getX() - 4.0f, y - 6.0f, scale.getWidth() - 2.0f, 12.0f),
                        juce::Justification::centredRight);
        }
    }
    for (int c = 0; c < 2; ++c)
    {
        const auto b = c == 0 ? b0 : b1;
        g.setColour (colours::well);
        g.fillRoundedRectangle (b.expanded (1.5f), 2.5f);
        const float frac = juce::jlimit (0.0f, 1.0f, 1.0f - levelDb[c] / floorDb);
        juce::ColourGradient grad (colours::accent2, 0.0f, b.getBottom(), colours::warn, 0.0f, b.getY(), false);
        grad.addColour (0.8, colours::accent);
        g.setGradientFill (grad);
        g.fillRect (b.withTop (b.getBottom() - b.getHeight() * frac));
    }
    // peak hold line
    if (peakHoldDb > floorDb)
    {
        const float y = bars.getY() + bars.getHeight() * juce::jlimit (0.0f, 1.0f, peakHoldDb / floorDb);
        g.setColour (colours::text.withAlpha (0.8f));
        g.drawHorizontalLine (int (y), b0.getX(), b1.getRight());
    }
}

// =============================================================================================
LinkRing::LinkRing (juce::AudioProcessorValueTreeState& s)
    : state (s), hp (s.getParameter (ids::hiPass)), lp (s.getParameter (ids::loPass))
{
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
    setTooltip ("Link - drag to move both filter frequencies by the same ratio");
}

void LinkRing::mouseDown (const juce::MouseEvent&)
{
    startHp = hp->getValue();
    startLp = lp->getValue();
    hpOn = startHp >= float (param::HiPass::offBelow);
    lpOn = startLp <= float (param::LoPass::offAbove);
    dragging = true;
    if (hpOn) hp->beginChangeGesture();
    if (lpOn) lp->beginChangeGesture();
}

void LinkRing::mouseDrag (const juce::MouseEvent& e)
{
    const float dr = -float (e.getDistanceFromDragStartY()) * (e.mods.isShiftDown() ? 0.0004f : 0.002f);
    // the frequency is 19 * 1058^r, so equal steps in r are equal frequency ratios
    if (hpOn) hp->setValueNotifyingHost (juce::jlimit (float (param::HiPass::offBelow), 1.0f, startHp + dr));
    if (lpOn) lp->setValueNotifyingHost (juce::jlimit (0.0f, float (param::LoPass::offAbove), startLp + dr));
    turn = dr;
    repaint();
}

void LinkRing::mouseUp (const juce::MouseEvent&)
{
    if (hpOn) hp->endChangeGesture();
    if (lpOn) lp->endChangeGesture();
    dragging = false;
    turn = 0.0f;
    repaint();
}

void LinkRing::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    const float s = std::min (r.getWidth(), r.getHeight()) - 4.0f;
    const auto ring = juce::Rectangle<float> (s, s).withCentre (r.getCentre());
    const bool hot = isMouseOverOrDragging();
    g.setColour (colours::well);
    g.drawEllipse (ring, 5.0f);
    g.setColour (hot ? colours::accent2 : colours::accent2.withAlpha (0.6f));
    g.drawEllipse (ring, 1.5f);
    // two interlocked links
    const auto c = ring.getCentre();
    const float lw = s * 0.22f, lh = s * 0.13f;
    const float rot = turn * 6.0f;
    juce::Path p;
    p.addRoundedRectangle (juce::Rectangle<float> (lw, lh).withCentre ({ c.x - lw * 0.32f, c.y }), lh * 0.5f);
    p.addRoundedRectangle (juce::Rectangle<float> (lw, lh).withCentre ({ c.x + lw * 0.32f, c.y }), lh * 0.5f);
    p.applyTransform (juce::AffineTransform::rotation (-0.6f + rot, c.x, c.y));
    g.strokePath (p, juce::PathStrokeType (1.6f));
}

} // namespace craft::hdelay::ui
