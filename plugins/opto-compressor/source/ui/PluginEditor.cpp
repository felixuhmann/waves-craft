#include "PluginEditor.h"

namespace craft::opto
{

namespace
{
const juce::Colour bgTop (0xff1d2a30), bgBottom (0xff141c21), panel (0xff24343b), ink (0xffdfe8e6), dim (0xff8ba0a3);
const juce::Colour accent (0xfff0a24a), accentCool (0xff4fc4b5);
constexpr int width = 760, height = 440;
} // namespace

OptoLookAndFeel::OptoLookAndFeel()
{
    setColour (juce::Label::textColourId, ink);
    setDefaultSansSerifTypefaceName ("Segoe UI");
}

void OptoLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider& s)
{
    const auto area = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (4.0f);
    const float size = juce::jmin (area.getWidth(), area.getHeight());
    const auto c = area.getCentre();
    const float radius = size * 0.5f, stroke = juce::jmax (3.0f, size * 0.045f);
    const float angle = start + pos * (end - start);

    juce::Path track, value;
    track.addCentredArc (c.x, c.y, radius - stroke, radius - stroke, 0, start, end, true);
    value.addCentredArc (c.x, c.y, radius - stroke, radius - stroke, 0, start, angle, true);
    g.setColour (bgBottom.brighter (0.15f));
    g.strokePath (track, juce::PathStrokeType (stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (s.isEnabled() ? accent : dim);
    g.strokePath (value, juce::PathStrokeType (stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float body = radius - stroke * 2.6f;
    g.setColour (panel.brighter (0.2f));
    g.fillEllipse (c.x - body, c.y - body, body * 2, body * 2);
    g.setColour (ink);
    const juce::Point<float> a = c.getPointOnCircumference (body * 0.45f, angle), b = c.getPointOnCircumference (body * 0.92f, angle);
    g.drawLine ({ a, b }, juce::jmax (2.0f, stroke * 0.7f));
}

void OptoLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool)
{
    const auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (b.getToggleState() ? accent : (over ? panel.brighter (0.25f) : panel.brighter (0.08f)));
    g.fillRoundedRectangle (r, 4.0f);
}

void OptoLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    g.setColour (b.getToggleState() ? bgBottom : ink);
    g.setFont (juce::FontOptions (juce::jmin (13.0f, float (b.getHeight()) * 0.5f)));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred, 1);
}

// ---------------------------------------------------------------------------------------------
Knob::Knob()
{
    setSliderStyle (juce::Slider::RotaryVerticalDrag);
    setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
    setMouseDragSensitivity (220);
    setScrollWheelEnabled (true);
}

void Knob::mouseDown (const juce::MouseEvent& e)
{
    gesture = true;
    juce::Slider::mouseDown (e);
}
void Knob::mouseUp (const juce::MouseEvent& e)
{
    juce::Slider::mouseUp (e);
    gesture = false;
}
void Knob::mouseDoubleClick (const juce::MouseEvent& e)
{
    gesture = true;
    juce::Slider::mouseDoubleClick (e);
    gesture = false;
}
void Knob::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& d)
{
    gesture = true;
    juce::Slider::mouseWheelMove (e, d);
    gesture = false;
}

Segments::Segments (juce::RangedAudioParameter& param, juce::StringArray labels)
    : attachment (param, [this] (float v) {
          const int idx = juce::roundToInt (v);
          for (int i = 0; i < buttons.size(); ++i)
              buttons[i]->setToggleState (i == idx, juce::dontSendNotification);
      })
{
    for (int i = 0; i < labels.size(); ++i)
    {
        auto* b = buttons.add (new juce::TextButton (labels[i]));
        b->onClick = [this, i] { attachment.setValueAsCompleteGesture (float (i)); };
        addAndMakeVisible (b);
    }
    attachment.sendInitialUpdate();
}

void Segments::resized()
{
    auto r = getLocalBounds();
    const int w = r.getWidth() / juce::jmax (1, buttons.size());
    for (auto* b : buttons)
        b->setBounds (r.removeFromLeft (w));
}

// ---------------------------------------------------------------------------------------------
void LevelMeter::setValues (float levelDb, float reductionDb, int newMode)
{
    level = levelDb;
    reduction = reductionDb;
    mode = newMode;
    repaint();
}

void LevelMeter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (bgBottom);
    g.fillRoundedRectangle (r, 6.0f);
    auto bar = r.reduced (34.0f, 12.0f).withTrimmedRight (8.0f);

    const bool isGr = mode == 1;
    const float lo = isGr ? -30.0f : -48.0f, hi = isGr ? 0.0f : 6.0f;
    auto yOf = [&] (float db) { return juce::jmap (juce::jlimit (lo, hi, db), lo, hi, bar.getBottom(), bar.getY()); };

    g.setColour (panel);
    g.fillRoundedRectangle (bar, 3.0f);
    if (isGr)
    {
        const float y = yOf (reduction);
        g.setColour (accent);
        g.fillRoundedRectangle (bar.withBottom (y), 3.0f);
    }
    else
    {
        const float y = yOf (level);
        g.setColour (level > 0.0f ? accent : accentCool);
        g.fillRoundedRectangle (bar.withTop (y), 3.0f);
    }

    g.setFont (juce::FontOptions (11.0f));
    const float step = isGr ? 5.0f : 12.0f;
    for (float d = hi; d >= lo; d -= step)
    {
        const float y = yOf (d);
        g.setColour (dim);
        g.drawHorizontalLine (juce::roundToInt (y), bar.getRight(), bar.getRight() + 5.0f);
        g.drawText (juce::String (juce::roundToInt (d)), juce::Rectangle<float> (0.0f, y - 8.0f, 30.0f, 16.0f), juce::Justification::centredRight);
    }
}

// ---------------------------------------------------------------------------------------------
OptoEditor::OptoEditor (OptoProcessor& p)
    : AudioProcessorEditor (p),
      processor (p),
      modeSel (*p.getState().getParameter (ids::mode), { "Comp", "Limit" }),
      analogSel (*p.getState().getParameter (ids::analog), { "Off", "50", "60" }),
      vuSel (*p.getState().getParameter (ids::vuDisplay), { "In", "GR", "Out" }),
      autoAttachment (p.getState(), ids::autoMakeup, autoMakeup)
{
    setLookAndFeel (&lnf);
    addControl (gain, ids::gain, "Gain");
    addControl (peak, ids::peakReduction, "Peak Reduction");
    addControl (hiFreq, ids::hiFreq, "Hi Freq");
    addControl (mix, ids::mix, "Mix");
    addControl (trim, ids::trim, "Trim");

    autoMakeup.setClickingTogglesState (true);
    addAndMakeVisible (autoMakeup);
    addAndMakeVisible (modeSel);
    addAndMakeVisible (analogSel);
    addAndMakeVisible (vuSel);
    addAndMakeVisible (meter);

    lastPr = peak.knob.getValue();
    peak.knob.onValueChange = [this] { onPeakReductionMoved(); };

    setSize (width, height);
    updateReadouts();
    startTimerHz (30);
}

OptoEditor::~OptoEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void OptoEditor::addControl (Control& c, const char* id, const juce::String& title)
{
    c.title = title;
    addAndMakeVisible (c.knob);
    c.attachment = std::make_unique<SliderAttachment> (processor.getState(), id, c.knob);
    c.readout.setJustificationType (juce::Justification::centred);
    c.readout.setFont (juce::FontOptions (13.0f));
    c.readout.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (c.readout);
}

// Spec 8: only an editor gesture on Peak Reduction moves Gain; host automation never reaches this branch.
void OptoEditor::onPeakReductionMoved()
{
    const double now = peak.knob.getValue();
    if (peak.knob.isUserGesture() && autoMakeup.getToggleState())
    {
        auto* g = processor.getState().getParameter (ids::gain);
        const double next = juce::jlimit (0.0, 10.0, double (g->convertFrom0to1 (g->getValue())) + dsp::autoMakeupGainDelta (lastPr, now));
        g->beginChangeGesture();
        g->setValueNotifyingHost (g->convertTo0to1 (float (next)));
        g->endChangeGesture();
    }
    lastPr = now;
}

void OptoEditor::updateReadouts()
{
    auto text = [&] (Control& c, const char* id) {
        auto* p = processor.getState().getParameter (id);
        c.readout.setText (p->getCurrentValueAsText(), juce::dontSendNotification);
    };
    text (gain, ids::gain);
    text (peak, ids::peakReduction);
    text (mix, ids::mix);
    text (trim, ids::trim);
    auto* hf = processor.getState().getParameter (ids::hiFreq);
    hiFreq.readout.setText (hf->getValue() >= 0.9995f ? juce::String ("Flat") : hf->getCurrentValueAsText(), juce::dontSendNotification);
}

void OptoEditor::timerCallback()
{
    const auto m = processor.takeMeters();
    const int vu = juce::roundToInt (processor.getState().getParameter (ids::vuDisplay)->convertFrom0to1 (
        processor.getState().getParameter (ids::vuDisplay)->getValue()));
    heldIn = juce::jmax (m.inPeak, heldIn * 0.85f);
    heldOut = juce::jmax (m.outPeak, heldOut * 0.85f);
    const float lvl = vu == 0 ? heldIn : heldOut;
    meter.setValues (juce::Decibels::gainToDecibels (lvl, -100.0f), m.reductionDb, vu);
    updateReadouts();
}

void OptoEditor::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (bgTop, 0, 0, bgBottom, 0, float (getHeight()), false));
    g.fillAll();

    g.setColour (panel);
    g.fillRoundedRectangle (getLocalBounds().toFloat().removeFromBottom (140.0f).reduced (14.0f, 10.0f), 8.0f);

    g.setColour (ink);
    g.setFont (juce::FontOptions (19.0f, juce::Font::bold));
    g.drawText ("Craft Opto Compressor", 22, 10, 300, 28, juce::Justification::centredLeft);

    g.setColour (dim);
    g.setFont (juce::FontOptions (13.0f));
    for (auto* c : { &gain, &peak, &hiFreq, &mix, &trim })
        g.drawText (c->title, c->knob.getBounds().withHeight (18).translated (-20, -19).withWidth (c->knob.getWidth() + 40), juce::Justification::centred);
    g.drawText ("Analog", analogSel.getBounds().translated (0, -20).withHeight (18), juce::Justification::centred);
}

void OptoEditor::resized()
{
    auto place = [] (Control& c, juce::Rectangle<int> r) {
        c.readout.setBounds (r.removeFromBottom (20));
        c.knob.setBounds (r);
    };
    auto r = getLocalBounds();
    auto strip = r.removeFromBottom (140).reduced (24, 18);
    r.removeFromTop (44);
    r = r.reduced (20, 4);

    place (gain, r.removeFromLeft (220).withTrimmedTop (22));
    auto right = r.removeFromRight (220);
    auto rightKnob = right.withTrimmedTop (22).withTrimmedBottom (44);
    place (peak, rightKnob);
    auto rightButtons = right.removeFromBottom (36);
    modeSel.setBounds (rightButtons.removeFromLeft (108));
    autoMakeup.setBounds (rightButtons.withTrimmedLeft (6));

    auto centre = r.reduced (14, 0);
    vuSel.setBounds (centre.removeFromBottom (30));
    meter.setBounds (centre.withTrimmedBottom (8));

    const int cell = strip.getWidth() / 4;
    auto analogCell = strip.removeFromLeft (cell).reduced (8, 0);
    analogSel.setBounds (analogCell.withSizeKeepingCentre (analogCell.getWidth(), 32).translated (0, 8));
    place (hiFreq, strip.removeFromLeft (cell).withTrimmedTop (14));
    place (mix, strip.removeFromLeft (cell).withTrimmedTop (14));
    place (trim, strip.withTrimmedTop (14));
}

} // namespace craft::opto
