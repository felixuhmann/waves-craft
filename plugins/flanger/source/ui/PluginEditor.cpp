#include "PluginEditor.h"

#include "dsp/Format.h"
#include "dsp/Lfo.h"
#include "dsp/Parameters.h"

namespace craft::flanger
{

using namespace ui;

namespace
{
constexpr int editorW = 970, editorH = 336, headerH = 40, margin = 12;
constexpr int panelW[3] = { 214, 224, 258 }; // the output panel takes the rest
constexpr int groupH = 70, captionH = 16, rowH = 26;
} // namespace

FlangerEditor::FlangerEditor (FlangerProcessor& p) : AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&lnf);
    using Q = dsp::Quantity;

    mixBox = &makeBox (ids::mix, int (Q::Percent), "Mix: dry/wet balance (equal power)");
    feedbackBox = &makeBox (ids::feedback, int (Q::Percent), "Feedback: amount of the delayed signal fed back");
    filterBox = &makeBox (ids::filterFreq, int (Q::FilterFreq), "Filter frequency (wet signal)");
    delayBox = &makeBox (ids::delay, int (Q::Delay), "Delay: centre delay time");
    rateBox = &makeBox (ids::rate, int (Q::Rate), "Rate: LFO speed (Hz, or cycles per beat with Sync)");
    depthBox = &makeBox (ids::depth, int (Q::Percent), "Depth: LFO sweep range");
    if (hasStereo())
    {
        stereoBox = &makeBox (ids::stereo, int (Q::Stereo), "Stereo: phase lead of the right channel's LFO");
        addAndMakeVisible (phaseIndicator);
    }

    auto& st = proc.getState();
    gainFader = std::make_unique<Fader> (
        st, ids::gain,
        [this] {
            auto* prm = proc.getState().getParameter (ids::gain);
            return juce::String (dsp::formatNumber (dsp::Quantity::Gain, prm->convertFrom0to1 (prm->getValue()))) + " dB";
        },
        std::vector<ScaleMark> { { 12.0, "+12" }, { 6.0, "+6" }, { 0.0, "0" }, { -6.0, "-6" }, { -12.0, "-12" } });
    addAndMakeVisible (*gainFader);

    attach (ids::wetInvert, wetPolarity, "Wet polarity");
    attach (ids::feedbackInvert, feedbackPolarity, "Feedback polarity");
    attach (ids::filterType, filterType, "Filter type: low-pass / high-pass");
    attach (ids::filterOn, filterOn, "Filter on/off");
    attach (ids::tape, tape, "Tape: through-zero flanging (the dry signal is delayed to the sweep centre)");
    attach (ids::tempoSync, sync, "Tempo sync: Rate in LFO cycles per beat");
    attach (ids::link, link, "Link Rate and Depth (keeps Rate x Depth constant)");
    attach (ids::stop, stop, "Stop: freeze the LFO at its current position");
    attach (ids::waveform, waveform, "LFO waveform: sine / triangle");

    addAndMakeVisible (lfoDisplay);
    meters = std::make_unique<MeterPanel> (proc.getNumMeterChannels());
    addAndMakeVisible (*meters);

    setSize (editorW, editorH);
    startTimerHz (30);
}

FlangerEditor::~FlangerEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

ValueBox& FlangerEditor::makeBox (const char* id, int quantity, const juce::String& tooltip)
{
    auto* prm = proc.getState().getParameter (id);
    const juce::String pid (id);
    auto box = std::make_unique<ValueBox> (*prm, [this, prm, pid, quantity] {
        const auto text = juce::String (dsp::formatNumber (dsp::Quantity (quantity), prm->convertFrom0to1 (prm->getValue())));
        const auto unit = unitFor (pid, proc.isTempoSync());
        return unit.isEmpty() ? text : (pid == ids::stereo ? text + unit : text + " " + unit);
    });
    box->setTooltip (tooltip + "\nDrag to change, click to type, double-click to reset");
    addAndMakeVisible (*box);
    boxes.push_back (std::move (box));
    return *boxes.back();
}

void FlangerEditor::attach (const char* id, juce::Button& b, const juce::String& tooltip)
{
    b.setTooltip (tooltip);
    addAndMakeVisible (b);
    buttonAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.getState(), id, b));
}

void FlangerEditor::timerCallback()
{
    for (auto& b : boxes)
        b->refresh();
    gainFader->refresh();

    const auto s = proc.readSettings();
    const auto v = proc.getLfoView();
    LfoDisplay::State ls;
    for (int ch = 0; ch < 2; ++ch)
    {
        ls.phase[ch] = v.phase[ch];
        ls.value[ch] = v.value[ch];
        ls.delayMs[ch] = v.delayMs[ch];
    }
    ls.triangle = s.triangle;
    ls.stopped = s.stop;
    ls.staticLfo = ! s.stop && dsp::model::lfoHz (s) <= 0.0;
    ls.channels = proc.getNumMeterChannels();
    lfoDisplay.setState (ls);

    if (hasStereo())
        phaseIndicator.setOffsetDegrees (float (dsp::stereoOffsetCycles (s.stereoDeg) * 360.0));

    const auto pk = proc.takePeaks();
    meters->pushPeaks (pk[0], pk[1]);
}

void FlangerEditor::paint (juce::Graphics& g)
{
    g.fillAll (colours::background);

    // header
    auto header = getLocalBounds().removeFromTop (headerH);
    g.setColour (colours::panel);
    g.fillRect (header);
    g.setColour (colours::accent);
    g.fillRect (header.removeFromBottom (2));
    auto h = getLocalBounds().removeFromTop (headerH).reduced (margin, 0);

    // logo mark: a swept comb - three offset sine strokes
    auto mark = h.removeFromLeft (30).toFloat().withSizeKeepingCentre (26, 18);
    for (int k = 0; k < 3; ++k)
    {
        juce::Path wave;
        for (int i = 0; i <= 24; ++i)
        {
            const float t = float (i) / 24.0f;
            const juce::Point<float> pt (mark.getX() + t * mark.getWidth(),
                                         mark.getCentreY() + (float (k) - 1.0f) * 5.0f
                                             - 3.0f * std::sin (juce::MathConstants<float>::twoPi * (t + 0.15f * float (k))));
            if (i == 0)
                wave.startNewSubPath (pt);
            else
                wave.lineTo (pt);
        }
        g.setColour (k == 1 ? colours::accent : colours::accent2.withAlpha (k == 0 ? 0.9f : 0.5f));
        g.strokePath (wave, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    h.removeFromLeft (8);
    g.setColour (colours::text);
    g.setFont (juce::Font (uiFont (17.0f, true)));
    g.drawText (proc.getName().toUpperCase(), h, juce::Justification::centredLeft);
    g.setColour (colours::textDim);
    g.setFont (juce::Font (uiFont (11.0f)));
    g.drawText ("CRAFT AUDIO  v" CRAFT_VERSION_STRING, h, juce::Justification::centredRight);

    const char* titles[4] = { "Mix / Feedback / Filter", "Delay / Rate / Depth", "LFO", "Output" };
    for (int i = 0; i < 4; ++i)
    {
        drawPanel (g, panels[size_t (i)].toFloat());
        drawSectionTitle (g, panels[size_t (i)].reduced (12, 8).removeFromTop (16), titles[i]);
    }
    for (const auto& [r, text] : captions)
    {
        g.setColour (colours::textDim);
        g.setFont (juce::Font (uiFont (10.5f, true)));
        g.drawText (text, r, juce::Justification::centredLeft);
    }

    // Link bracket: joins the Rate and Depth boxes to the link toggle
    {
        const auto rb = rateBox->getBounds(), db = depthBox->getBounds(), lb = link.getBounds();
        const float x0 = float (rb.getRight()) + 1.0f, x1 = float (lb.getCentreX());
        const auto col = link.getToggleState() ? colours::accent : colours::panelEdge.brighter (0.4f);
        g.setColour (col);
        juce::Path br;
        br.startNewSubPath (x0, float (rb.getCentreY()));
        br.lineTo (x1, float (rb.getCentreY()));
        br.lineTo (x1, float (lb.getY()));
        br.startNewSubPath (x1, float (lb.getBottom()));
        br.lineTo (x1, float (db.getCentreY()));
        br.lineTo (x0, float (db.getCentreY()));
        g.strokePath (br, juce::PathStrokeType (1.5f));
    }
}

void FlangerEditor::resized()
{
    captions.clear();
    auto body = getLocalBounds().withTrimmedTop (headerH).reduced (margin);
    for (int i = 0; i < 3; ++i)
    {
        panels[size_t (i)] = body.removeFromLeft (panelW[i]);
        body.removeFromLeft (margin);
    }
    panels[3] = body;

    auto content = [] (juce::Rectangle<int> panel) {
        auto r = panel.reduced (12, 8);
        r.removeFromTop (24);
        return r;
    };
    auto group = [this] (juce::Rectangle<int>& area, const juce::String& caption) {
        auto gr = area.removeFromTop (groupH);
        captions.emplace_back (gr.removeFromTop (captionH), caption);
        gr.removeFromTop (4);
        return gr.removeFromTop (rowH);
    };

    // 1. Mix / Feedback / Filter
    {
        auto a = content (panels[0]);
        auto row = group (a, "MIX");
        wetPolarity.setBounds (row.removeFromRight (34));
        row.removeFromRight (8);
        mixBox->setBounds (row);

        row = group (a, "FEEDBACK");
        feedbackPolarity.setBounds (row.removeFromRight (34));
        row.removeFromRight (8);
        feedbackBox->setBounds (row);

        row = group (a, "FILTER");
        filterType.setBounds (row.removeFromLeft (58));
        row.removeFromLeft (6);
        filterOn.setBounds (row.removeFromRight (34));
        row.removeFromRight (6);
        filterBox->setBounds (row);
    }

    // 2. Delay / Rate / Depth  (Link sits in a column between the Rate and Depth boxes and the toggles)
    {
        auto a = content (panels[1]);
        constexpr int boxW = 96, linkColW = 28, gap = 6;
        auto row = group (a, "DELAY");
        delayBox->setBounds (row.removeFromLeft (boxW));
        tape.setBounds (row.removeFromRight (row.getWidth() - (linkColW + 2 * gap)));

        auto rateRow = group (a, "RATE");
        rateBox->setBounds (rateRow.removeFromLeft (boxW));
        sync.setBounds (rateRow.removeFromRight (rateRow.getWidth() - (linkColW + 2 * gap)));

        auto depthRow = group (a, "DEPTH");
        depthBox->setBounds (depthRow.removeFromLeft (boxW));
        const int linkX = depthBox->getRight() + gap;
        const int midY = (rateBox->getBounds().getCentreY() + depthBox->getBounds().getCentreY()) / 2;
        link.setBounds (linkX, midY - 13, linkColW, 26);
    }

    // 3. LFO
    {
        auto a = content (panels[2]);
        auto top = a.removeFromTop (rowH);
        waveform.setBounds (top.removeFromLeft (96));
        stop.setBounds (top.removeFromRight (72));
        a.removeFromTop (10);
        if (hasStereo())
        {
            auto stereoArea = a.removeFromBottom (captionH + 4 + rowH);
            captions.emplace_back (stereoArea.removeFromTop (captionH), "STEREO (R LEADS L)");
            stereoArea.removeFromTop (4);
            stereoBox->setBounds (stereoArea.removeFromLeft (96));
            stereoArea.removeFromLeft (10);
            phaseIndicator.setBounds (stereoArea.removeFromLeft (rowH + 4).withSizeKeepingCentre (rowH + 4, rowH + 4));
            a.removeFromBottom (10);
        }
        lfoDisplay.setBounds (a);
    }

    // 4. Output
    {
        auto a = content (panels[3]);
        captions.emplace_back (a.removeFromTop (captionH).removeFromLeft (70), "GAIN");
        a.removeFromTop (4);
        gainFader->setBounds (a.removeFromLeft (64));
        a.removeFromLeft (4);
        meters->setBounds (a);
    }
}

} // namespace craft::flanger
