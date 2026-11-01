#include "PluginEditor.h"

namespace craft::enhancer
{

using namespace ui;

ValueBox* EnhancerEditor::box (std::unique_ptr<ValueBox>& slot, const juce::String& id)
{
    slot = std::make_unique<ValueBox> (*proc.getState().getParameter (id));
    addAndMakeVisible (*slot);
    return slot.get();
}

EnhancerEditor::EnhancerEditor (EnhancerProcessor& p) : juce::AudioProcessorEditor (p), proc (p), display (p)
{
    setLookAndFeel (&lnf);
    auto& st = proc.getState();
    addAndMakeVisible (display);

    for (int k = 0; k < 4; ++k)
    {
        box (crossoverBoxes[(size_t) k], ids::crossover (k));
        crossoverBoxes[(size_t) k]->limit = [this, k] (float v) { return BandDisplay::limitToNeighbours (proc, k, v); };
    }

    auto knob = [&] (const juce::String& id) {
        auto s = std::make_unique<BoundSlider> (st, id, juce::Slider::RotaryHorizontalVerticalDrag, colours::accent);
        s->getProperties().set ("centreDetent", true);
        addAndMakeVisible (*s);
        return s;
    };
    inputKnob = knob (ids::input);
    outputKnob = knob (ids::output);
    box (inputBox, ids::input);
    box (outputBox, ids::output);
    box (punchBox, ids::punch);
    box (directBox, ids::direct);
    directFader = std::make_unique<BoundSlider> (st, ids::direct, juce::Slider::LinearVertical, colours::text);
    addAndMakeVisible (*directFader);
    addAndMakeVisible (inputMeter);
    addAndMakeVisible (outputMeter);

    for (int b = 0; b < 5; ++b)
    {
        auto& s = strips[(size_t) b];
        const auto col = bandColour (b);
        s.solo = std::make_unique<BoundToggle> (st, ids::solo (b), "S", colours::solo);
        s.bypass = std::make_unique<BoundToggle> (st, ids::bypass (b), "B", colours::bypass);
        s.fader = std::make_unique<BoundSlider> (st, ids::bandGain (b), juce::Slider::LinearVertical, col);
        s.widthKnob = std::make_unique<BoundSlider> (st, ids::width (b), juce::Slider::RotaryHorizontalVerticalDrag, col);
        addAndMakeVisible (*s.solo);
        addAndMakeVisible (*s.bypass);
        addAndMakeVisible (*s.fader);
        addAndMakeVisible (*s.widthKnob);
        box (s.gainBox, ids::bandGain (b));
        box (s.widthBox, ids::width (b));
    }

    trimValue.setJustificationType (juce::Justification::centred);
    trimValue.setFont (juce::Font (uiFont (12.0f, true)));
    trimValue.setText ("-", juce::dontSendNotification);
    trimValue.setTooltip ("Gain that would bring the highest output peak to 0 dBFS; click a meter to reset");
    addAndMakeVisible (trimValue);

    setSize (860, 610);
    startTimerHz (30);
}

EnhancerEditor::~EnhancerEditor() { setLookAndFeel (nullptr); }

void EnhancerEditor::paint (juce::Graphics& g)
{
    g.fillAll (colours::background);
    g.setColour (colours::text);
    g.setFont (juce::Font (uiFont (15.0f, true)));
    g.drawText ("CRAFT ENHANCER", 14, 6, 300, 20, juce::Justification::centredLeft);

    drawSection (g, leftArea, "Input", colours::accent);
    drawSection (g, centreArea, {}, colours::accent);
    drawSection (g, rightArea, "Output", colours::accent);

    auto caption = [&] (juce::Rectangle<int> r, const juce::String& t, juce::Colour c = colours::textDim) {
        g.setColour (c);
        g.setFont (juce::Font (uiFont (10.5f, true)));
        g.drawText (t, r, juce::Justification::centred);
    };
    static const char* xnames[] = { "LO", "LM", "HM", "HI" };
    for (int k = 0; k < 4; ++k)
        caption (crossoverBoxes[(size_t) k]->getBounds().withWidth (24).translated (-26, 0), xnames[k]);

    caption (directArea.withHeight (16).translated (0, 2), "DIRECT");
    for (int b = 0; b < 5; ++b)
    {
        caption (stripAreas[(size_t) b].withHeight (16).translated (0, 2), "BAND " + ids::bandLetter (b), bandColour (b));
        if (stereoShown)
            caption (strips[(size_t) b].widthKnob->getBounds().withHeight (12).translated (0, -13), "WIDTH");
    }
    caption (punchBox->getBounds().withHeight (14).translated (0, -15), "PUNCH");
    caption (trimValue.getBounds().withHeight (14).translated (0, -15), "TRIM");
}

void EnhancerEditor::resized()
{
    auto r = getLocalBounds().reduced (10);
    r.removeFromTop (20);
    displayArea = r.removeFromTop (170);
    display.setBounds (displayArea);
    r.removeFromTop (6);
    xoverRow = r.removeFromTop (22);
    const int xw = 80, gap = 40;
    int x = xoverRow.getCentreX() - (4 * xw + 3 * gap) / 2;
    for (auto& b : crossoverBoxes)
    {
        b->setBounds (x, xoverRow.getY(), xw, xoverRow.getHeight());
        x += xw + gap;
    }
    r.removeFromTop (8);

    leftArea = r.removeFromLeft (120);
    rightArea = r.removeFromRight (120);
    r.removeFromLeft (6);
    r.removeFromRight (6);
    centreArea = r;

    auto l = leftArea.reduced (10);
    l.removeFromTop (18);
    inputKnob->setBounds (l.removeFromTop (70).withSizeKeepingCentre (70, 70));
    inputBox->setBounds (l.removeFromTop (20).withSizeKeepingCentre (72, 20));
    l.removeFromTop (6);
    inputMeter.setBounds (l.withSizeKeepingCentre (70, l.getHeight()));

    auto o = rightArea.reduced (10);
    o.removeFromTop (18);
    outputKnob->setBounds (o.removeFromTop (70).withSizeKeepingCentre (70, 70));
    outputBox->setBounds (o.removeFromTop (20).withSizeKeepingCentre (72, 20));
    auto bottom = o.removeFromBottom (72);
    bottom.removeFromTop (16);
    punchBox->setBounds (bottom.removeFromLeft (bottom.getWidth() / 2).reduced (2, 0).withHeight (20));
    trimValue.setBounds (bottom.reduced (2, 0).withHeight (20));
    o.removeFromTop (6);
    outputMeter.setBounds (o.withSizeKeepingCentre (70, o.getHeight() - 4));

    auto c = centreArea.reduced (6, 4);
    const int columnWidth = c.getWidth() / 6;
    directArea = c.removeFromLeft (columnWidth);
    auto da = directArea;
    da.removeFromTop (44);
    directBox->setBounds (da.removeFromBottom (22).reduced (6, 0));
    directFader->setBounds (da.reduced (0, 4));

    for (size_t b = 0; b < 5; ++b)
    {
        stripAreas[b] = c.removeFromLeft (columnWidth);
        auto sr = stripAreas[b];
        sr.removeFromTop (18);
        auto buttons = sr.removeFromTop (22).reduced (6, 0);
        auto& s = strips[b];
        s.solo->setBounds (buttons.removeFromLeft (buttons.getWidth() / 2).withTrimmedRight (2));
        s.bypass->setBounds (buttons.withTrimmedLeft (2));
        s.widthBox->setBounds (sr.removeFromBottom (20).reduced (8, 0));
        s.widthKnob->setBounds (sr.removeFromBottom (44).withSizeKeepingCentre (44, 44));
        sr.removeFromBottom (14);
        s.gainBox->setBounds (sr.removeFromBottom (20).reduced (6, 0));
        s.fader->setBounds (sr.reduced (0, 4));
    }
}

void EnhancerEditor::timerCallback()
{
    display.refresh();
    for (auto& b : crossoverBoxes)
        b->refresh();
    for (auto* b : { inputBox.get(), outputBox.get(), directBox.get(), punchBox.get() })
        b->refresh();
    const bool stereo = proc.getMainBusNumOutputChannels() > 1;
    for (auto& s : strips)
    {
        s.gainBox->refresh();
        s.widthBox->refresh();
        if (stereo != stereoShown)
        {
            s.widthBox->setVisible (stereo);
            s.widthKnob->setVisible (stereo);
        }
    }
    if (stereo != stereoShown)
    {
        stereoShown = stereo;
        repaint();
    }
    inputMeter.push (proc.takeInputPeaks());
    outputMeter.push (proc.takeOutputPeaks());
    const float hold = outputMeter.getMaxHoldDb();
    trimValue.setText (hold <= -119.0f ? juce::String ("-") : juce::String (-hold, 1), juce::dontSendNotification);
}

} // namespace craft::enhancer
