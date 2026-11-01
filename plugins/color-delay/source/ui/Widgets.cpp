#include "Widgets.h"

#include "ColourLookAndFeel.h"

namespace craft::ui
{

// ---------------------------------------------------------------------------------------------
Knob::Knob (juce::AudioProcessorValueTreeState& st, const juce::String& id, juce::String t, Style s, juce::Colour a)
    : state (st), title (std::move (t)), style (s), accent (a)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    slider.setMouseDragSensitivity (style == Style::Large ? 260 : 200);
    slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
    slider.addListener (this);
    slider.addMouseListener (this, false);
    slider.setPopupDisplayEnabled (false, false, nullptr);
    addAndMakeVisible (slider);
    bindTo (id);
}

Knob::~Knob() { slider.removeListener (this); }

void Knob::bindTo (const juce::String& id)
{
    if (id == paramId && attachment != nullptr)
        return;
    attachment.reset();
    paramId = id;
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, paramId, slider);
    if (auto* p = state.getParameter (paramId))
        slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()), juce::ModifierKeys::altModifier);
    repaint();
}

void Knob::setDimmed (bool d)
{
    if (d != dimmed)
    {
        dimmed = d;
        slider.setColour (juce::Slider::rotarySliderFillColourId, dimmed ? accent.withMultipliedSaturation (0.35f) : accent);
        repaint();
    }
}

juce::String Knob::valueText() const
{
    auto* p = state.getParameter (paramId);
    if (p == nullptr)
        return {};
    auto text = p->getCurrentValueAsText();
    const auto unit = p->getLabel();
    if (unit.isNotEmpty() && text != "Off")
        text << (unit == "%" ? "" : " ") << unit;
    return text;
}

juce::Rectangle<int> Knob::knobArea() const
{
    auto r = getLocalBounds();
    if (style == Style::Large)
        r.removeFromTop (20);
    else
        r.removeFromBottom (style == Style::Medium ? 20 : 18);
    const int side = std::min (r.getWidth(), r.getHeight());
    return r.withSizeKeepingCentre (side, side);
}

void Knob::resized() { slider.setBounds (knobArea()); }

void Knob::paint (juce::Graphics& g)
{
    const bool active = slider.isMouseOverOrDragging();
    if (style == Style::Large)
    {
        drawTitle (g, getLocalBounds().removeFromTop (18), title, juce::Justification::centred,
                   dimmed ? colours::textDim.darker (0.3f) : colours::textDim, 12.0f);
        return;
    }
    auto r = getLocalBounds().removeFromBottom (style == Style::Medium ? 20 : 18);
    if (active)
    {
        g.setColour (colours::well);
        g.fillRoundedRectangle (r.toFloat().reduced (2.0f, 1.0f), 3.0f);
        g.setColour (accent);
        g.setFont (juce::Font (uiFont (12.0f, true)));
        g.drawFittedText (valueText(), r, juce::Justification::centred, 1);
    }
    else
    {
        drawTitle (g, r, title, juce::Justification::centred, dimmed ? colours::textDim.darker (0.3f) : colours::textDim,
                   style == Style::Medium ? 11.5f : 10.5f);
    }
}

void Knob::paintOverChildren (juce::Graphics& g)
{
    if (style != Style::Large)
        return;
    // value in the knob centre
    const auto k = knobArea().toFloat();
    const auto inner = k.withSizeKeepingCentre (k.getWidth() * 0.5f, 22.0f);
    g.setColour (dimmed ? colours::textDim : colours::text);
    g.setFont (juce::Font (uiFont (std::min (17.0f, k.getWidth() * 0.12f), true)));
    g.drawFittedText (valueText(), inner.toNearestInt(), juce::Justification::centred, 1);
}

// ---------------------------------------------------------------------------------------------
LedButton::LedButton (const juce::String& text, juce::Colour l) : juce::Button (text), led (l)
{
    setClickingTogglesState (true);
}

void LedButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    const bool on = getToggleState();
    g.setColour (down ? colours::well.brighter (0.05f) : colours::well);
    g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
    g.setColour (over ? led.withAlpha (0.8f) : colours::panelEdge.brighter (0.15f));
    g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.0f);

    // indicator light
    const float d = r.getHeight() * 0.42f;
    const auto dot = juce::Rectangle<float> (d, d).withCentre ({ r.getX() + r.getHeight() * 0.55f, r.getCentreY() });
    if (on)
    {
        g.setColour (led.withAlpha (0.25f));
        g.fillEllipse (dot.expanded (d * 0.45f));
        g.setColour (led);
        g.fillEllipse (dot);
        g.setColour (juce::Colours::white.withAlpha (0.55f));
        g.fillEllipse (dot.reduced (d * 0.3f).translated (-d * 0.1f, -d * 0.1f));
    }
    else
    {
        g.setColour (colours::panel.brighter (0.1f));
        g.fillEllipse (dot);
        g.setColour (colours::panelEdge.brighter (0.3f));
        g.drawEllipse (dot, 1.0f);
    }

    g.setColour (on ? colours::text : colours::textDim);
    g.setFont (juce::Font (uiFont (std::min (12.5f, r.getHeight() * 0.55f), true)).withExtraKerningFactor (0.06f));
    g.drawFittedText (getButtonText().toUpperCase(), r.withTrimmedLeft (r.getHeight() * 0.95f).withTrimmedRight (6.0f).toNearestInt(),
                      juce::Justification::centred, 1);
}

// ---------------------------------------------------------------------------------------------
SegmentedChoice::SegmentedChoice (juce::AudioProcessorValueTreeState& st, const juce::String& id, juce::StringArray l, juce::Colour a)
    : param (*st.getParameter (id)), labels (std::move (l)), accent (a),
      attachment (param, [this] (float v) {
          index = juce::jlimit (0, labels.size() - 1, (int) std::lround (v));
          repaint();
      })
{
    attachment.sendInitialUpdate();
}

void SegmentedChoice::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (colours::well);
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (isMouseOver() ? accent.withAlpha (0.6f) : colours::panelEdge);
    g.drawRoundedRectangle (r, 5.0f, 1.0f);
    const int n = labels.size();
    const float w = (r.getWidth() - 4.0f) / (float) n;
    g.setFont (juce::Font (uiFont (11.5f, true)));
    for (int i = 0; i < n; ++i)
    {
        const auto seg = juce::Rectangle<float> (r.getX() + 2.0f + w * (float) i, r.getY() + 2.0f, w, r.getHeight() - 4.0f);
        if (i == index)
        {
            g.setColour (accent);
            g.fillRoundedRectangle (seg.reduced (1.0f, 0.0f), 4.0f);
        }
        g.setColour (i == index ? colours::background : colours::textDim);
        g.drawText (labels[i], seg, juce::Justification::centred);
    }
}

void SegmentedChoice::mouseDown (const juce::MouseEvent& e)
{
    const int n = labels.size();
    const int i = juce::jlimit (0, n - 1, (int) (e.position.x / (float) getWidth() * (float) n));
    if (e.mods.isAltDown())
    {
        attachment.setValueAsCompleteGesture (param.convertFrom0to1 (param.getDefaultValue()));
        return;
    }
    // two segments: a click anywhere toggles; more: pick the clicked segment
    attachment.setValueAsCompleteGesture ((float) (n == 2 ? 1 - index : i));
}

// ---------------------------------------------------------------------------------------------
static constexpr float meterTop = 6.0f, meterBottom = -48.0f;

void Meter::push (float pl, float pr)
{
    const float in[2] = { pl, pr };
    for (int c = 0; c < 2; ++c)
    {
        const float db = in[c] > 0.0f ? 20.0f * std::log10 (in[c]) : -100.0f;
        level[c] = std::max (db, level[c] - 1.2f); // ~36 dB/s fall at 30 Hz
        if (db >= hold[c])
        {
            hold[c] = db;
            holdAge[c] = 0;
        }
        else if (++holdAge[c] > 45)
        {
            hold[c] = std::max (hold[c] - 1.5f, level[c]);
        }
    }
    repaint();
}

void Meter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds();
    drawTitle (g, r.removeFromTop (16), title, juce::Justification::centred, colours::textDim, 10.5f);
    r.removeFromTop (4);
    auto scaleArea = r.removeFromLeft (r.getWidth() / 2 - 8);
    r.reduce (0, 6);
    auto bars = r.withTrimmedRight (2);
    const float top = (float) bars.getY(), height = (float) bars.getHeight();
    auto yOf = [&] (float db) { return top + height * (meterTop - juce::jlimit (meterBottom, meterTop, db)) / (meterTop - meterBottom); };

    g.setFont (juce::Font (uiFont (9.5f)));
    for (float db : { 6.0f, 0.0f, -6.0f, -12.0f, -24.0f, -36.0f, -48.0f })
    {
        const float y = yOf (db);
        g.setColour (colours::panelEdge.brighter (0.3f));
        g.drawHorizontalLine ((int) y, (float) scaleArea.getRight() - 4.0f, (float) scaleArea.getRight());
        g.setColour (colours::textDim);
        g.drawText (juce::String ((int) db), juce::Rectangle<float> ((float) scaleArea.getX(), y - 6.0f, (float) scaleArea.getWidth() - 6.0f, 12.0f),
                    juce::Justification::centredRight);
    }

    const float bw = ((float) bars.getWidth() - 4.0f) / 2.0f;
    for (int c = 0; c < 2; ++c)
    {
        const auto b = juce::Rectangle<float> ((float) bars.getX() + (float) c * (bw + 4.0f), top, bw, height);
        g.setColour (colours::well);
        g.fillRoundedRectangle (b.expanded (1.5f), 2.5f);
        const float y = yOf (level[c]);
        if (level[c] > meterBottom)
        {
            juce::ColourGradient grad (colours::accent2, 0.0f, b.getBottom(), colours::warn, 0.0f, b.getY(), false);
            grad.addColour ((0.0 - meterBottom) / (meterTop - meterBottom) - 0.12, colours::accent);
            g.setGradientFill (grad);
            g.fillRect (b.withTop (y));
        }
        if (hold[c] > meterBottom)
        {
            g.setColour (hold[c] > 0.0f ? colours::warn : colours::text);
            g.fillRect (b.withTop (yOf (hold[c]) - 1.0f).withHeight (2.0f));
        }
    }
}

} // namespace craft::ui
