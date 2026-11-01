#include "PluginEditor.h"

#include "dsp/Parameters.h"

namespace craft
{

using namespace ui;

namespace
{
constexpr int editorW = 940, editorH = 610, headerH = 44, margin = 12, gap = 10;
constexpr int leftW = 78, rightW = 96, delayH = 318;
} // namespace

ColourDelayEditor::ColourDelayEditor (ColourDelayProcessor& p)
    : AudioProcessorEditor (p), proc (p),
      feedbackL (p.getState(), ids::feedbackL, "Feedback L", K::Style::Small, colours::accent),
      feedbackR (p.getState(), ids::feedbackR, "Feedback R", K::Style::Small, colours::accent),
      delayL (p.getState(), ids::noteL, "Delay L", K::Style::Large, colours::accent),
      delayR (p.getState(), ids::noteR, "Delay R", K::Style::Large, colours::accent),
      dryWet (p.getState(), ids::dryWet, "Dry/Wet", K::Style::Medium, colours::accent2),
      hp (p.getState(), ids::hp, "HP", K::Style::Medium, colours::accent2),
      lp (p.getState(), ids::lp, "LP", K::Style::Medium, colours::accent2),
      input (p.getState(), ids::input, "Input", K::Style::Small, colours::accent2),
      output (p.getState(), ids::output, "Output", K::Style::Small, colours::accent2),
      verbLevel (p.getState(), ids::verbLevel, "Level", K::Style::Medium, colours::reverb),
      verbSize (p.getState(), ids::verbSize, "Size", K::Style::Small, colours::reverb),
      distLevel (p.getState(), ids::distLevel, "Level", K::Style::Medium, colours::distortion),
      distSend (p.getState(), ids::distSend, "Send", K::Style::Small, colours::distortion),
      distWidth (p.getState(), ids::distWidth, "Width", K::Style::Small, colours::distortion),
      dblLevel (p.getState(), ids::doublerLevel, "Level", K::Style::Medium, colours::doubler),
      dblDetune (p.getState(), ids::doublerDetune, "Detune", K::Style::Small, colours::doubler),
      phLevel (p.getState(), ids::phaserLevel, "Level", K::Style::Medium, colours::phaser),
      phRate (p.getState(), ids::phaserRate, "Rate", K::Style::Small, colours::phaser),
      mode (p.getState(), ids::mode, { "ms", "Host" }, colours::accent),
      monitor (p.getState(), ids::monitor, { "L", "ST", "R" }, colours::accent2)
{
    setLookAndFeel (&lnf);

    for (auto* k : { &feedbackL, &feedbackR, &delayL, &delayR, &dryWet, &hp, &lp, &input, &output, &verbLevel, &verbSize,
                     &distLevel, &distSend, &distWidth, &dblLevel, &dblDetune, &phLevel, &phRate })
        addAndMakeVisible (*k);

    attachButton (ids::delayLink, delayLink, "Link: the right delay follows the left (time and feedback)");
    attachButton (ids::filterLink, filterLink, "HP/LP link: moving one filter moves the other inversely");
    attachButton (ids::verbOn, verbOn, "Reverb colour on/off");
    attachButton (ids::distOn, distOn, "Distortion colour on/off");
    attachButton (ids::doublerOn, dblOn, "Doubler colour on/off");
    attachButton (ids::phaserOn, phOn, "Phaser colour on/off");

    mode.setTooltip ("Delay time in milliseconds or synced to the host tempo");
    monitor.setTooltip ("Monitor: left only, stereo, right only");
    addAndMakeVisible (mode);
    addAndMakeVisible (monitor);

    attachCombo (ids::noteL, noteL);
    attachCombo (ids::noteR, noteR);
    addAndMakeVisible (inMeter);
    addAndMakeVisible (outMeter);

    setSize (editorW, editorH);
    updateModeAndLink();
    startTimerHz (30);
}

ColourDelayEditor::~ColourDelayEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void ColourDelayEditor::attachButton (const char* id, juce::Button& b, const juce::String& tip)
{
    b.setTooltip (tip);
    addAndMakeVisible (b);
    buttonAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.getState(), id, b));
}

void ColourDelayEditor::attachCombo (const char* id, juce::ComboBox& c)
{
    for (int i = 0; i < cdl::param::numNotes; ++i)
        c.addItem (cdl::param::noteNames[(size_t) i], i + 1);
    c.setTooltip ("Note value (Host mode)");
    addAndMakeVisible (c);
    comboAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.getState(), id, c));
}

void ColourDelayEditor::updateModeAndLink()
{
    hostMode = mode.getIndex() == 1;
    delayL.bindTo (hostMode ? ids::noteL : ids::delayMsL);
    delayR.bindTo (hostMode ? ids::noteR : ids::delayMsR);
    noteL.setEnabled (hostMode);
    noteR.setEnabled (hostMode);

    if (auto* v = proc.getState().getRawParameterValue (ids::delayLink))
        linked = v->load() >= 0.5f;
    delayR.setDimmed (linked);
    feedbackR.setDimmed (linked);
}

void ColourDelayEditor::timerCallback()
{
    const auto in = proc.takeInputPeaks(), out = proc.takeOutputPeaks();
    inMeter.push (in[0], in[1]);
    outMeter.push (out[0], out[1]);
    const bool wasHost = hostMode, wasLinked = linked;
    updateModeAndLink();
    if (wasHost != hostMode || wasLinked != linked)
        repaint();
}

// ---------------------------------------------------------------------------------------------
void ColourDelayEditor::drawBadge (juce::Graphics& g, juce::Rectangle<float> r)
{
    // product badge: graphite capsule, two-ring mark, name and the four colour dots
    juce::ColourGradient grad (juce::Colour (0xff2a3037), r.getX(), r.getY(), juce::Colour (0xff171a1e), r.getX(), r.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
    g.setColour (colours::accent.withAlpha (0.55f));
    g.drawRoundedRectangle (r.reduced (1.0f), r.getHeight() * 0.5f, 1.2f);

    auto inner = r.reduced (r.getHeight() * 0.32f, 6.0f);
    auto mark = inner.removeFromLeft (30.0f).withSizeKeepingCentre (28.0f, 18.0f);
    g.setColour (colours::accent);
    g.drawEllipse (mark.withWidth (18.0f), 2.0f);
    g.setColour (colours::accent2);
    g.drawEllipse (mark.withTrimmedLeft (10.0f), 2.0f);
    inner.removeFromLeft (8.0f);

    auto top = inner.removeFromTop (inner.getHeight() * 0.62f);
    g.setColour (colours::text);
    g.setFont (juce::Font (uiFont (18.0f, true)).withExtraKerningFactor (0.12f));
    g.drawText ("COLOUR DELAY", top, juce::Justification::centredLeft);

    const juce::Colour dots[] = { colours::reverb, colours::distortion, colours::doubler, colours::phaser };
    float x = inner.getX() + 1.0f;
    for (auto c : dots)
    {
        g.setColour (c);
        g.fillEllipse (x, inner.getCentreY() - 3.0f, 6.0f, 6.0f);
        x += 11.0f;
    }
    g.setColour (colours::textDim);
    g.setFont (juce::Font (uiFont (9.5f, true)).withExtraKerningFactor (0.1f));
    g.drawText ("CRAFT AUDIO", inner.withTrimmedLeft (x - inner.getX() + 4.0f), juce::Justification::centredLeft);
}

void ColourDelayEditor::paint (juce::Graphics& g)
{
    g.fillAll (colours::background);

    // header
    auto header = getLocalBounds().removeFromTop (headerH);
    g.setColour (colours::panel);
    g.fillRect (header);
    {
        // thin four-colour rule under the header
        auto rule = header.removeFromBottom (2).toFloat();
        const juce::Colour cs[] = { colours::accent, colours::reverb, colours::distortion, colours::doubler, colours::phaser };
        const float w = rule.getWidth() / 5.0f;
        for (int i = 0; i < 5; ++i)
        {
            g.setColour (cs[i]);
            g.fillRect (rule.withX (rule.getX() + w * (float) i).withWidth (w));
        }
    }
    auto h = getLocalBounds().removeFromTop (headerH).reduced (margin, 0);
    g.setColour (colours::text);
    g.setFont (juce::Font (uiFont (16.0f, true)).withExtraKerningFactor (0.06f));
    g.drawText ("CRAFT COLOUR DELAY", h, juce::Justification::centredLeft);
    g.setColour (colours::textDim);
    g.setFont (juce::Font (uiFont (11.0f)));
    g.drawText (juce::String ("CRAFT AUDIO  v") + CRAFT_VERSION_STRING
                    + (hostMode ? "   |   " + juce::String (proc.getTempo(), 1) + " BPM" : juce::String()),
                h, juce::Justification::centredRight);

    // panels
    drawPanel (g, leftCol.toFloat(), colours::accent2);
    drawPanel (g, rightCol.toFloat(), colours::accent2);
    drawPanel (g, delayPanel.toFloat(), colours::accent);
    const juce::Colour fxCols[] = { colours::reverb, colours::distortion, colours::doubler, colours::phaser };
    for (int i = 0; i < 4; ++i)
        drawPanel (g, fxPanels[i].toFloat(), fxCols[i]);

    drawBadge (g, badge.toFloat());

    // link indication between the two delay knobs
    if (linked)
    {
        const auto a = delayL.getBounds().getCentre().toFloat(), b = delayR.getBounds().getCentre().toFloat();
        g.setColour (colours::accent.withAlpha (0.18f));
        const float y = delayPanel.toFloat().getBottom() - 16.0f;
        g.drawLine (a.x, y, b.x, y, 2.0f);
        drawTitle (g, juce::Rectangle<int> (0, (int) y - 7, 70, 14).withCentre ({ (int) ((a.x + b.x) / 2), (int) y }), "linked",
                   juce::Justification::centred, colours::accent.withAlpha (0.8f), 10.0f);
    }

    // monitor caption
    drawTitle (g, monitor.getBounds().translated (0, -16).withHeight (14), "Monitor", juce::Justification::centred,
               colours::textDim, 10.0f);
}

void ColourDelayEditor::layoutEffect (int index, juce::Rectangle<int> area)
{
    auto r = area.reduced (10, 8);
    r.removeFromTop (4);
    LedButton* buttons[] = { &verbOn, &distOn, &dblOn, &phOn };
    buttons[index]->setBounds (r.removeFromTop (26).withSizeKeepingCentre (std::min (r.getWidth(), 150), 26));
    r.removeFromTop (6);

    Knob* level[] = { &verbLevel, &distLevel, &dblLevel, &phLevel };
    auto right = r.removeFromRight (r.getWidth() * 2 / 5);
    level[index]->setBounds (r.withSizeKeepingCentre (r.getWidth(), std::min (r.getHeight(), 122)));

    std::vector<Knob*> small;
    if (index == 0) small = { &verbSize };
    if (index == 1) small = { &distSend, &distWidth };
    if (index == 2) small = { &dblDetune };
    if (index == 3) small = { &phRate };
    const int h = std::min (78, right.getHeight() / (int) small.size());
    auto col = right.withSizeKeepingCentre (right.getWidth(), h * (int) small.size());
    for (auto* k : small)
        k->setBounds (col.removeFromTop (h));
}

void ColourDelayEditor::resized()
{
    auto body = getLocalBounds().withTrimmedTop (headerH).reduced (margin);
    leftCol = body.removeFromLeft (leftW);
    body.removeFromLeft (gap);
    rightCol = body.removeFromRight (rightW);
    body.removeFromRight (gap);
    delayPanel = body.removeFromTop (delayH);
    body.removeFromTop (gap);
    const int fxW = (body.getWidth() - 3 * gap) / 4;
    for (int i = 0; i < 4; ++i)
    {
        fxPanels[i] = body.removeFromLeft (fxW);
        body.removeFromLeft (gap);
        layoutEffect (i, fxPanels[i]);
    }

    // left: input meter above the Input knob
    {
        auto c = leftCol.reduced (8, 10);
        input.setBounds (c.removeFromBottom (80));
        c.removeFromBottom (6);
        inMeter.setBounds (c);
    }
    // right: output meter above the Output knob and the Monitor switch
    {
        auto c = rightCol.reduced (8, 10);
        monitor.setBounds (c.removeFromBottom (26));
        c.removeFromBottom (20);
        output.setBounds (c.removeFromBottom (80));
        c.removeFromBottom (6);
        outMeter.setBounds (c.reduced (6, 0));
    }

    // delay panel
    auto d = delayPanel.reduced (14, 10);
    d.removeFromTop (4);
    auto top = d.removeFromTop (76);
    feedbackL.setBounds (top.removeFromLeft (76));
    feedbackR.setBounds (top.removeFromRight (76));
    top.reduce (8, 0);
    delayLink.setBounds (top.removeFromLeft (84).withSizeKeepingCentre (84, 26));
    mode.setBounds (top.removeFromRight (96).withSizeKeepingCentre (96, 26));
    badge = top.withSizeKeepingCentre (std::min (top.getWidth() - 20, 250), 58);

    d.removeFromTop (6);
    d.removeFromBottom (22); // space for the link indicator
    const int bigW = 200;
    auto left = d.removeFromLeft (bigW), right = d.removeFromRight (bigW);
    noteL.setBounds (left.removeFromBottom (26).withSizeKeepingCentre (120, 26));
    noteR.setBounds (right.removeFromBottom (26).withSizeKeepingCentre (120, 26));
    left.removeFromBottom (4);
    right.removeFromBottom (4);
    delayL.setBounds (left);
    delayR.setBounds (right);

    auto mid = d.reduced (6, 0).withSizeKeepingCentre (d.getWidth() - 12, 104);
    const int linkW = 58;
    const int kw = (mid.getWidth() - linkW) / 3;
    dryWet.setBounds (mid.removeFromLeft (kw));
    lp.setBounds (mid.removeFromRight (kw));
    hp.setBounds (mid.removeFromLeft (kw));
    filterLink.setBounds (mid.withSizeKeepingCentre (linkW, 22).translated (0, -10));
}

} // namespace craft
