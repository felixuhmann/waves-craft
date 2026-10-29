#include "PluginEditor.h"

namespace craft::consoleg
{

using namespace ui;

namespace
{
constexpr int margin = 10, headerH = 44, bodyH = 404;
constexpr int dynW = 330, filterW = 104, eqW = 392, masterW = 214;
constexpr int knobW = 74, knobH = 92;
} // namespace

ui::Knob& ConsoleGEditor::knob (const char* id, const char* name, juce::Colour c, bool bipolar)
{
    knobs.push_back (std::make_unique<ui::Knob> (proc.getState(), id, name, c, bipolar));
    addAndMakeVisible (*knobs.back());
    return *knobs.back();
}

ui::Switch& ConsoleGEditor::toggle (const char* id, const char* text, juce::Colour c, const char* tip)
{
    switches.push_back (std::make_unique<ui::Switch> (proc.getState(), id, text, c, tip));
    addAndMakeVisible (*switches.back());
    return *switches.back();
}

ConsoleGEditor::ConsoleGEditor (ConsoleGProcessor& p) : AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&lnf);

    compRatio = &knob (ids::compRatio, "Ratio", colours::comp);
    compThreshold = &knob (ids::compThreshold, "Threshold", colours::comp);
    compRelease = &knob (ids::compRelease, "Release", colours::comp);
    compFast = &toggle (ids::compFast, "FAST", colours::comp, "Compressor fast attack (otherwise program-dependent)");
    expRange = &knob (ids::expRange, "Range", colours::expander);
    expThreshold = &knob (ids::expThreshold, "Threshold", colours::expander);
    expRelease = &knob (ids::expRelease, "Release", colours::expander);
    expFast = &toggle (ids::expFast, "FAST", colours::expander, "Expander / gate fast attack");
    expGate = &toggle (ids::expGate, "GATE", colours::expander, "On: gate, off: expander");
    dynBypass = &toggle (ids::dynBypass, "DYN BYPASS", colours::warn, "Remove the dynamics section");
    dynPostEq = &toggle (ids::dynPostEq, "POST EQ", colours::master, "Dynamics after the filters and EQ");
    filtersSc = &toggle (ids::filtersSidechain, "FILT > SC", colours::filters, "Filters act on the dynamics detector only");
    filtersSplit = &toggle (ids::filtersSplit, "SPLIT", colours::filters, "Filters before the dynamics");
    addAndMakeVisible (grMeter);
    grMeter.setTooltip ("Gain reduction of the dynamics section (dB)");

    hp = &knob (ids::hpFreq, "HP", colours::filters);
    lp = &knob (ids::lpFreq, "LP", colours::filters);

    hfGain = &knob (ids::hfGain, "Gain", colours::hf, true);
    hfFreq = &knob (ids::hfFreq, "Freq", colours::hf);
    hmfGain = &knob (ids::hmfGain, "Gain", colours::hmf, true);
    hmfFreq = &knob (ids::hmfFreq, "Freq", colours::hmf);
    hmfQ = &knob (ids::hmfQ, "Q", colours::hmf);
    hmfX3 = &toggle (ids::hmfX3, "x3", colours::hmf, "Multiply the HMF frequency by 3");
    lmfGain = &knob (ids::lmfGain, "Gain", colours::lmf, true);
    lmfFreq = &knob (ids::lmfFreq, "Freq", colours::lmf);
    lmfQ = &knob (ids::lmfQ, "Q", colours::lmf);
    lmfDiv3 = &toggle (ids::lmfDiv3, "/3", colours::lmf, "Divide the LMF frequency by 3");
    lfGain = &knob (ids::lfGain, "Gain", colours::lf, true);
    lfFreq = &knob (ids::lfFreq, "Freq", colours::lf);
    eqBypass = &toggle (ids::eqBypass, "EQ BYPASS", colours::warn, "Remove the equaliser");

    trim = &knob (ids::inputTrim, "Input", colours::master, true);
    output = std::make_unique<ui::Fader> (proc.getState(), ids::outputGain, "Output", colours::master);
    addAndMakeVisible (*output);
    analog = &toggle (ids::analog, "ANALOG", colours::comp, "Adds the console's low-level noise");
    polarity = &toggle (ids::polarity, "PHASE", colours::master, "Invert polarity");
    addAndMakeVisible (ioMeter);
    for (auto* b : { &meterIn, &meterOut })
    {
        addAndMakeVisible (*b);
        b->setColour (juce::TextButton::buttonColourId, colours::panel);
        b->setColour (juce::TextButton::buttonOnColourId, colours::master);
        b->setColour (juce::TextButton::textColourOffId, colours::textDim);
        b->setColour (juce::TextButton::textColourOnId, colours::background);
        b->setTooltip ("Meter source (display only)");
    }
    auto setSource = [this] (float v) {
        if (auto* prm = proc.getState().getParameter (ids::meterSource))
        {
            prm->beginChangeGesture();
            prm->setValueNotifyingHost (v);
            prm->endChangeGesture();
        }
    };
    meterIn.onClick = [setSource] { setSource (0.0f); };
    meterOut.onClick = [setSource] { setSource (1.0f); };

    setSize (margin * 5 + dynW + filterW + eqW + masterW, headerH + margin + bodyH + margin);
    startTimerHz (30);
}

ConsoleGEditor::~ConsoleGEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void ConsoleGEditor::timerCallback()
{
    for (auto& k : knobs)
        k->refresh();
    output->refresh();

    const auto m = proc.takeMeters();
    const float gr = m.gainReductionDb;
    grMeter.push (&gr, 1.0f / 30.0f);

    const bool showOut = proc.getState().getParameter (ids::meterSource)->getValue() >= 0.5f;
    meterIn.setToggleState (! showOut, juce::dontSendNotification);
    meterOut.setToggleState (showOut, juce::dontSendNotification);
    const auto& src = showOut ? m.output : m.input;
    const float db[2] = { 20.0f * std::log10 (std::max (src[0], 1.0e-6f)), 20.0f * std::log10 (std::max (src[1], 1.0e-6f)) };
    ioMeter.setChannels (m.channels);
    ioMeter.push (db, 1.0f / 30.0f);
}

void ConsoleGEditor::paint (juce::Graphics& g)
{
    g.fillAll (colours::background);

    // header: wordmark and a small original emblem (three stacked bars)
    auto header = getLocalBounds().removeFromTop (headerH).reduced (margin, 6);
    auto emblem = header.removeFromLeft (26).toFloat().reduced (2.0f, 6.0f);
    const juce::Colour bars[] = { colours::comp, colours::expander, colours::filters };
    for (int i = 0; i < 3; ++i)
    {
        g.setColour (bars[i]);
        g.fillRoundedRectangle (emblem.getX(), emblem.getY() + float (i) * emblem.getHeight() / 3.0f, emblem.getWidth() * (1.0f - 0.2f * float (i)),
                                emblem.getHeight() / 3.0f - 2.0f, 1.5f);
    }
    header.removeFromLeft (8);
    g.setColour (colours::text);
    g.setFont (juce::Font (uiFont (17.0f, true)));
    g.drawText ("CRAFT CONSOLE G", header, juce::Justification::centredLeft);
    g.setColour (colours::textDim);
    g.setFont (juce::Font (uiFont (11.5f)));
    g.drawText ("channel strip  -  Craft Audio", header, juce::Justification::centredRight);

    drawPanel (g, compArea.toFloat(), colours::comp, "Compressor");
    drawPanel (g, expArea.toFloat(), colours::expander, "Expander / Gate");
    drawPanel (g, routeArea.toFloat(), colours::master, "Dynamics routing");
    drawPanel (g, filterPanel.toFloat(), colours::filters, "Filters");
    drawPanel (g, eqPanel.toFloat(), colours::hmf, "Equaliser");
    drawPanel (g, masterPanel.toFloat(), colours::master, "Master");

    // band headers inside the EQ panel
    const char* names[] = { "HF", "HMF", "LMF", "LF" };
    const juce::Colour cols[] = { colours::hf, colours::hmf, colours::lmf, colours::lf };
    for (size_t i = 0; i < 4; ++i)
    {
        auto b = bandAreas[i].toFloat();
        g.setColour (colours::well.withAlpha (0.45f));
        g.fillRoundedRectangle (b, 5.0f);
        auto t = b.removeFromTop (18.0f);
        g.setColour (cols[i]);
        g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre ({ t.getX() + 12.0f, t.getCentreY() }));
        g.setFont (juce::Font (uiFont (11.5f, true)));
        g.drawText (names[i], t.withTrimmedLeft (20.0f), juce::Justification::centredLeft);
    }

    // HP/LP response glyphs (the filter knobs' "off" position is fully counter-clockwise)
    g.setColour (colours::textDim);
    g.setFont (juce::Font (uiFont (9.5f)));
    g.drawText ("ccw = off", filterPanel.withTrimmedTop (filterPanel.getHeight() - 24).reduced (6, 0), juce::Justification::centred);
}

void ConsoleGEditor::resized()
{
    auto body = getLocalBounds().withTrimmedTop (headerH).reduced (margin, 0).withTrimmedBottom (margin);
    dynPanel = body.removeFromLeft (dynW);
    body.removeFromLeft (margin);
    filterPanel = body.removeFromLeft (filterW);
    body.removeFromLeft (margin);
    eqPanel = body.removeFromLeft (eqW);
    body.removeFromLeft (margin);
    masterPanel = body.removeFromLeft (masterW);

    // dynamics column: compressor, expander, routing
    auto d = dynPanel;
    compArea = d.removeFromTop (150);
    d.removeFromTop (8);
    expArea = d.removeFromTop (150);
    d.removeFromTop (8);
    routeArea = d;

    auto layoutRow = [] (juce::Rectangle<int> area, std::initializer_list<juce::Component*> comps) {
        for (auto* c : comps)
            c->setBounds (area.removeFromLeft (knobW).withHeight (knobH));
    };
    {
        auto a = compArea.withTrimmedTop (24).reduced (6, 4);
        grMeter.setBounds (a.removeFromRight (50).withTrimmedBottom (2));
        auto row = a.removeFromTop (knobH);
        layoutRow (row, { compRatio, compThreshold, compRelease });
        a.removeFromTop (4);
        compFast->setBounds (a.removeFromLeft (70).withHeight (22));
    }
    {
        auto a = expArea.withTrimmedTop (24).reduced (6, 4);
        auto side = a.removeFromRight (78);
        auto row = a.removeFromTop (knobH);
        layoutRow (row, { expRange, expThreshold, expRelease });
        side.removeFromTop (16);
        expGate->setBounds (side.removeFromTop (24).reduced (2, 0));
        side.removeFromTop (8);
        expFast->setBounds (side.removeFromTop (24).reduced (2, 0));
    }
    {
        auto a = routeArea.withTrimmedTop (28).reduced (8, 4);
        auto r1 = a.removeFromTop (26);
        a.removeFromTop (6);
        auto r2 = a.removeFromTop (26);
        const int w = r1.getWidth() / 2;
        dynBypass->setBounds (r1.removeFromLeft (w).reduced (3, 0));
        dynPostEq->setBounds (r1.reduced (3, 0));
        filtersSc->setBounds (r2.removeFromLeft (w).reduced (3, 0));
        filtersSplit->setBounds (r2.reduced (3, 0));
    }

    // filters
    {
        auto a = filterPanel.withTrimmedTop (30).reduced (8, 0);
        hp->setBounds (a.removeFromTop (knobH + 6).withSizeKeepingCentre (knobW + 10, knobH + 6));
        a.removeFromTop (14);
        lp->setBounds (a.removeFromTop (knobH + 6).withSizeKeepingCentre (knobW + 10, knobH + 6));
    }

    // EQ: HF, HMF, LMF, LF columns
    {
        auto a = eqPanel.withTrimmedTop (28).reduced (6, 0);
        auto bottom = a.removeFromBottom (36);
        eqBypass->setBounds (bottom.withSizeKeepingCentre (120, 24));
        const int colW = a.getWidth() / 4;
        for (size_t i = 0; i < 4; ++i)
            bandAreas[i] = a.removeFromLeft (colW).reduced (3, 0);
        auto place = [] (juce::Rectangle<int> area, std::initializer_list<juce::Component*> comps, juce::Component* sw) {
            area.removeFromTop (20);
            for (auto* c : comps)
                c->setBounds (area.removeFromTop (knobH).withSizeKeepingCentre (knobW + 8, knobH));
            if (sw != nullptr)
            {
                area.removeFromTop (4);
                sw->setBounds (area.removeFromTop (24).withSizeKeepingCentre (64, 24));
            }
        };
        place (bandAreas[0], { hfGain, hfFreq }, nullptr);
        place (bandAreas[1], { hmfGain, hmfFreq, hmfQ }, hmfX3);
        place (bandAreas[2], { lmfGain, lmfFreq, lmfQ }, lmfDiv3);
        place (bandAreas[3], { lfGain, lfFreq }, nullptr);
    }

    // master
    {
        auto a = masterPanel.withTrimmedTop (28).reduced (8, 4);
        auto meterCol = a.removeFromRight (72);
        auto buttons = meterCol.removeFromBottom (24);
        meterIn.setBounds (buttons.removeFromLeft (buttons.getWidth() / 2).reduced (1, 0));
        meterOut.setBounds (buttons.reduced (1, 0));
        meterCol.removeFromBottom (6);
        ioMeter.setBounds (meterCol);
        trim->setBounds (a.removeFromTop (knobH).withSizeKeepingCentre (knobW + 8, knobH));
        auto sw = a.removeFromBottom (60);
        analog->setBounds (sw.removeFromTop (26).reduced (4, 0));
        sw.removeFromTop (6);
        polarity->setBounds (sw.removeFromTop (26).reduced (4, 0));
        output->setBounds (a.reduced (14, 4));
    }
}

} // namespace craft::consoleg
