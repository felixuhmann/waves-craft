#include "PluginEditor.h"

#ifndef CONSOLE_E_VERSION_STRING
 #define CONSOLE_E_VERSION_STRING "0.0.0"
#endif

namespace craft::consolee
{

using namespace ui;

namespace
{
constexpr int headerH = 44, margin = 12, dynW = 300, eqW = 476, ioW = 236, bodyH = 556;
constexpr const char* rowNames[6] = { "FILTERS", "HF", "HMF", "LMF", "LF", "" };
} // namespace

ConsoleEditor::ConsoleEditor (ConsoleProcessor& p)
    : AudioProcessorEditor (p), proc (p),
      compRatio (p.getState(), ids::compRatio, "Ratio"),
      compThreshold (p.getState(), ids::compThreshold, "Threshold"),
      compRelease (p.getState(), ids::compRelease, "Release"),
      compFast (p.getState(), ids::compFast, "FAST ATTACK", "Compressor fast attack (about 1-5 ms instead of program-dependent)"),
      expRange (p.getState(), ids::expRange, "Range"),
      expThreshold (p.getState(), ids::expThreshold, "Threshold"),
      expRelease (p.getState(), ids::expRelease, "Release"),
      expFast (p.getState(), ids::expFast, "FAST ATTACK", "Expander/gate fast opening (under 1 ms)"),
      expMode (p.getState(), ids::gate, "Expander", "Gate"),
      dynBypass (p.getState(), ids::dynBypass, "DYN BYPASS", "Compressor and expander pass audio unchanged", true),
      channelOut (p.getState(), ids::channelOut, "POST EQ", "Dynamics after the equaliser (channel out)"),
      hp (p.getState(), ids::hp, "High-Pass", false, true),
      lp (p.getState(), ids::lp, "Low-Pass", false, true),
      split (p.getState(), ids::split, "SPLIT", "Filters before the dynamics"),
      hfGain (p.getState(), ids::hfGain, "Gain", true),
      hfFreq (p.getState(), ids::hfFreq, "Freq"),
      hfBell (p.getState(), ids::hfBell, "BELL", "HF band: bell instead of shelf"),
      hmfGain (p.getState(), ids::hmfGain, "Gain", true),
      hmfFreq (p.getState(), ids::hmfFreq, "Freq"),
      hmfQ (p.getState(), ids::hmfQ, "Q"),
      lmfGain (p.getState(), ids::lmfGain, "Gain", true),
      lmfFreq (p.getState(), ids::lmfFreq, "Freq"),
      lmfQ (p.getState(), ids::lmfQ, "Q"),
      lfGain (p.getState(), ids::lfGain, "Gain", true),
      lfFreq (p.getState(), ids::lfFreq, "Freq"),
      lfBell (p.getState(), ids::lfBell, "BELL", "LF band: bell instead of shelf"),
      eqBypass (p.getState(), ids::eqBypass, "EQ BYPASS", "Equaliser and filters pass audio unchanged", true),
      eqToSidechain (p.getState(), ids::eqToSidechain, "EQ TO SIDECHAIN", "The equaliser filters the dynamics detector instead of the audio"),
      inputTrim (p.getState(), ids::inputTrim, "Input Trim", true),
      polarity (p.getState(), ids::polarity, "POLARITY", "Invert the polarity"),
      analogue (p.getState(), ids::analogue, "ANALOGUE", "Adds the low analogue noise floor"),
      meterSelect (p.getState(), ids::meterOutput, "In", "Out"),
      outputFader (p.getState(), ids::outputFader)
{
    setLookAndFeel (&lnf);
    expMode.setTooltip ("Expander (gradual) or gate (hard switching)");
    meterSelect.setTooltip ("Meter the input (after the trim) or the output");

    for (juce::Component* c : std::initializer_list<juce::Component*> {
             &compRatio, &compThreshold, &compRelease, &compFast, &compMeter, &expRange, &expThreshold, &expRelease, &expFast, &expMode,
             &expMeter, &dynBypass, &channelOut, &hp, &lp, &split, &hfGain, &hfFreq, &hfBell, &hmfGain, &hmfFreq, &hmfQ, &lmfGain,
             &lmfFreq, &lmfQ, &lfGain, &lfFreq, &lfBell, &eqBypass, &eqToSidechain, &inputTrim, &polarity, &analogue, &meterSelect,
             &levelMeter, &outputFader })
        addAndMakeVisible (c);

    setSize (margin + dynW + margin + eqW + margin + ioW + margin, headerH + margin + bodyH + margin);
    startTimerHz (30);
}

ConsoleEditor::~ConsoleEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void ConsoleEditor::timerCallback()
{
    const auto m = proc.takeMeters();
    compMeter.setReduction (-m.compReductionDb);
    expMeter.setReduction (-m.expanderGainDb);
    levelMeter.setLevels (m.level[0], m.level[1], proc.getNumAudioChannels() == 1);
}

void ConsoleEditor::paint (juce::Graphics& g)
{
    g.fillAll (colours::background);

    // header: logo mark (two overlapping response curves) and the product name
    auto header = getLocalBounds().removeFromTop (headerH);
    g.setColour (colours::panel);
    g.fillRect (header);
    g.setColour (colours::accent);
    g.fillRect (header.removeFromBottom (2));
    auto h = getLocalBounds().removeFromTop (headerH).reduced (margin, 0);
    auto mark = h.removeFromLeft (34).toFloat().withSizeKeepingCentre (30, 18);
    juce::Path bellCurve, shelfCurve;
    for (int i = 0; i <= 30; ++i)
    {
        const float t = float (i) / 30.0f;
        const float b = std::exp (-std::pow ((t - 0.45f) / 0.16f, 2.0f));
        const float s = 1.0f / (1.0f + std::exp (-(t - 0.7f) * 14.0f));
        const juce::Point<float> pb (mark.getX() + t * mark.getWidth(), mark.getBottom() - 2.0f - b * (mark.getHeight() - 4.0f));
        const juce::Point<float> ps (mark.getX() + t * mark.getWidth(), mark.getBottom() - 2.0f - 0.55f * s * (mark.getHeight() - 4.0f));
        if (i == 0) { bellCurve.startNewSubPath (pb); shelfCurve.startNewSubPath (ps); }
        else { bellCurve.lineTo (pb); shelfCurve.lineTo (ps); }
    }
    g.setColour (colours::accent2);
    g.strokePath (bellCurve, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (colours::accent);
    g.strokePath (shelfCurve, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    h.removeFromLeft (6);
    g.setColour (colours::text);
    g.setFont (juce::Font (uiFont (17.0f, true)));
    g.drawText ("CRAFT CONSOLE E", h, juce::Justification::centredLeft);
    g.setColour (colours::textDim);
    g.setFont (juce::Font (uiFont (11.0f)));
    g.drawText (juce::String ("CRAFT AUDIO  v" CONSOLE_E_VERSION_STRING) + (proc.getNumAudioChannels() == 1 ? "  MONO" : "  STEREO"), h,
                juce::Justification::centredRight);

    drawPanel (g, dynPanel.toFloat(), "DYNAMICS");
    drawPanel (g, eqPanel.toFloat(), "EQUALISER");
    drawPanel (g, ioPanel.toFloat(), "INPUT / OUTPUT");

    auto subTitle = [&g] (juce::Rectangle<int> r, const juce::String& t) {
        g.setColour (colours::well.withAlpha (0.6f));
        g.fillRoundedRectangle (r.toFloat(), 4.0f);
        g.setColour (colours::accent2.withAlpha (0.9f));
        g.setFont (juce::Font (uiFont (10.5f, true)));
        g.drawText (t, r.removeFromTop (18).reduced (8, 0), juce::Justification::centredLeft);
    };
    subTitle (compBlock, "COMPRESSOR / LIMITER");
    subTitle (expBlock, "EXPANDER / GATE");

    // EQ row labels and separators
    for (size_t i = 0; i < eqRows.size(); ++i)
    {
        const auto row = eqRows[i];
        if (i > 0)
        {
            g.setColour (colours::panelEdge);
            g.fillRect (row.getX() + 8, row.getY(), row.getWidth() - 16, 1);
        }
        g.setColour (i == 0 ? colours::accent2 : colours::text);
        g.setFont (juce::Font (uiFont (12.0f, true)));
        g.drawText (rowNames[i], row.withWidth (58).withTrimmedLeft (10), juce::Justification::centredLeft);
    }
}

void ConsoleEditor::resized()
{
    auto body = getLocalBounds().withTrimmedTop (headerH).reduced (margin);
    dynPanel = body.removeFromLeft (dynW);
    body.removeFromLeft (margin);
    eqPanel = body.removeFromLeft (eqW);
    body.removeFromLeft (margin);
    ioPanel = body;

    // ---- dynamics
    {
        auto r = dynPanel.reduced (10).withTrimmedTop (22);
        auto routing = r.removeFromBottom (30);
        r.removeFromBottom (10);
        const int blockH = (r.getHeight() - 10) / 2;
        compBlock = r.removeFromTop (blockH);
        r.removeFromTop (10);
        expBlock = r;

        auto layoutBlock = [] (juce::Rectangle<int> b, ui::Knob& k1, ui::Knob& k2, ui::Knob& k3, juce::Component& meter,
                               juce::Component& b1, juce::Component* b2) {
            b = b.reduced (6).withTrimmedTop (16);
            auto knobs = b.removeFromTop (100);
            const int w = knobs.getWidth() / 3;
            k1.setBounds (knobs.removeFromLeft (w));
            k2.setBounds (knobs.removeFromLeft (w));
            k3.setBounds (knobs);
            b.removeFromTop (8);
            auto buttons = b.removeFromTop (24);
            b1.setBounds (buttons.removeFromLeft (112));
            if (b2 != nullptr)
                b2->setBounds (buttons.removeFromRight (128));
            b.removeFromTop (10);
            meter.setBounds (b.removeFromTop (34));
        };
        layoutBlock (compBlock, compRatio, compThreshold, compRelease, compMeter, compFast, nullptr);
        layoutBlock (expBlock, expRange, expThreshold, expRelease, expMeter, expFast, &expMode);
        const int bw = (routing.getWidth() - 10) / 2;
        dynBypass.setBounds (routing.removeFromLeft (bw));
        routing.removeFromLeft (10);
        channelOut.setBounds (routing);
    }

    // ---- equaliser: filters, HF, HMF, LMF, LF, then the bypass / sidechain buttons
    {
        auto r = eqPanel.reduced (6).withTrimmedTop (22);
        auto buttons = r.removeFromBottom (30).reduced (60, 0);
        r.removeFromBottom (8);
        const int rowH = r.getHeight() / 5;
        for (size_t i = 0; i < 5; ++i)
            eqRows[i] = r.removeFromTop (rowH);
        eqRows[5] = {};

        auto place = [] (juce::Rectangle<int> row, juce::Component& a, juce::Component& b, juce::Component& c, bool cIsButton) {
            row = row.withTrimmedLeft (60).reduced (0, 6);
            const int w = row.getWidth() / 3;
            a.setBounds (row.removeFromLeft (w));
            b.setBounds (row.removeFromLeft (w));
            if (cIsButton)
                c.setBounds (row.withSizeKeepingCentre (std::min (row.getWidth() - 20, 100), 24));
            else
                c.setBounds (row);
        };
        place (eqRows[0], hp, lp, split, true);
        place (eqRows[1], hfGain, hfFreq, hfBell, true);
        place (eqRows[2], hmfGain, hmfFreq, hmfQ, false);
        place (eqRows[3], lmfGain, lmfFreq, lmfQ, false);
        place (eqRows[4], lfGain, lfFreq, lfBell, true);
        const int bw = (buttons.getWidth() - 12) / 2;
        eqBypass.setBounds (buttons.removeFromLeft (bw));
        buttons.removeFromLeft (12);
        eqToSidechain.setBounds (buttons);
    }

    // ---- I/O
    {
        auto r = ioPanel.reduced (10).withTrimmedTop (22);
        auto top = r.removeFromTop (96);
        inputTrim.setBounds (top.removeFromLeft (top.getWidth() / 2).reduced (2, 0));
        top.reduce (6, 18);
        polarity.setBounds (top.removeFromTop (26));
        top.removeFromTop (8);
        analogue.setBounds (top.removeFromTop (26));
        r.removeFromTop (10);
        auto sel = r.removeFromTop (24);
        meterSelect.setBounds (sel.removeFromLeft (r.getWidth() / 2 + 6).reduced (2, 0));
        r.removeFromTop (8);
        auto meterArea = r.removeFromLeft (r.getWidth() / 2 + 6);
        levelMeter.setBounds (meterArea.withTrimmedBottom (21).reduced (2, 0));
        outputFader.setBounds (r);
    }
}

} // namespace craft::consolee
