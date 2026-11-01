#include "PluginEditor.h"

#include "dsp/Parameters.h"

namespace craft::mastering_limiter
{

using namespace ui;

namespace
{
constexpr int editorW = 780, editorH = 460, headerH = 40, margin = 12;
constexpr int leftW = 220, centreW = 170, bottomH = 92, captionH = 16, rowH = 26;
} // namespace

MasteringLimiterEditor::MasteringLimiterEditor (MasteringLimiterProcessor& p) : AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&lnf);
    auto& st = proc.getState();
    threshold = st.getParameter (ids::threshold);
    ceiling = st.getParameter (ids::ceiling);
    release = st.getParameter (ids::release);

    auto levelText = [] (juce::RangedAudioParameter* prm) {
        return [prm] {
            const double db = prm->convertFrom0to1 (prm->getValue());
            return juce::String (std::abs (db) < 0.05 ? 0.0 : db, 1); // "-12.0" (unit shown in the caption)
        };
    };

    thresholdBox = std::make_unique<ValueBox> (*threshold, levelText (threshold));
    thresholdBox->setTooltip ("Threshold (dB): level above which the limiter reduces the gain.\n"
                              "Drag to change, click to type, double-click to reset");
    thresholdBox->onUserValue = [this] (float norm) { applyLevel (threshold, threshold->convertFrom0to1 (norm)); };
    ceilingBox = std::make_unique<ValueBox> (*ceiling, levelText (ceiling));
    ceilingBox->setTooltip ("Out Ceiling (dB): maximum output sample peak; the make-up gain is Ceiling - Threshold.\n"
                            "Drag to change, click to type, double-click to reset");
    ceilingBox->onUserValue = [this] (float norm) { applyLevel (ceiling, ceiling->convertFrom0to1 (norm)); };
    releaseBox = std::make_unique<ValueBox> (*release, [this] {
        return juce::String (dsp::formatMs (release->convertFrom0to1 (release->getValue())));
    });
    releaseBox->setTooltip ("Release (ms): recovery time (x10 per Release time). Inactive with Adaptive Release.\n"
                            "Drag to change, click to type, double-click to reset");
    for (auto* b : { thresholdBox.get(), ceilingBox.get(), releaseBox.get() })
        addAndMakeVisible (*b);

    // meters with draggable markers (spec 14)
    auto wireMeter = [this] (MarkerMeter& m, juce::RangedAudioParameter* prm) {
        m.onDragStart = [this, prm] { beginLevelGesture (prm); };
        m.onDragEnd = [this, prm] { endLevelGesture (prm); };
        m.onDrag = [this, prm] (float db) { applyLevel (prm, db); };
        m.onReset = [this, prm] {
            beginLevelGesture (prm);
            applyLevel (prm, prm->convertFrom0to1 (prm->getDefaultValue()));
            endLevelGesture (prm);
        };
        addAndMakeVisible (m);
    };
    wireMeter (inputMeter, threshold);
    wireMeter (outputMeter, ceiling);
    inputMeter.setTooltip ("Input level - drag the amber marker to set the Threshold");
    outputMeter.setTooltip ("Output level - drag the marker to set the Out Ceiling");
    addAndMakeVisible (grMeter);
    addAndMakeVisible (attenReadout);
    attenReadout.setTooltip ("Peak attenuation (dB) - click to reset");
    for (auto& r : outPeak)
    {
        r.setTooltip ("Output peak hold (dBFS) - click to reset");
        addAndMakeVisible (r);
    }

    adaptiveButton.setClickingTogglesState (true);
    adaptiveButton.setTooltip ("Adaptive Release: automatic, program-dependent release (the Release value is ignored)");
    adaptiveAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (st, ids::adaptive, adaptiveButton);
    addAndMakeVisible (adaptiveButton);

    linkButton.setClickingTogglesState (true);
    linkButton.setTooltip ("Link the Threshold and Ceiling markers (drag both together, keeping their offset)");
    linkButton.setToggleState (isLinked(), juce::dontSendNotification);
    linkButton.onClick = [this] {
        proc.getState().state.setProperty (linkMarkersProperty, linkButton.getToggleState(), nullptr);
    };
    addAndMakeVisible (linkButton);

    wordLength = std::make_unique<SegmentedChoice> (*st.getParameter (ids::wordLength),
                                                    juce::StringArray { "24", "22", "20", "18", "16" });
    wordLength->setTooltip ("Word Length of the output (bits)");
    dither = std::make_unique<SegmentedChoice> (*st.getParameter (ids::dither), juce::StringArray { "None", "Type 1", "Type 2" });
    dither->setTooltip ("Dither: Type 1 = TPDF +-1 LSB; Type 2 = lower-level dither, silent on digital silence");
    shaping = std::make_unique<SegmentedChoice> (*st.getParameter (ids::shaping),
                                                 juce::StringArray { "None", "Moderate", "Normal", "Ultra" });
    shaping->setTooltip ("Noise Shaping: moves the requantisation noise towards less audible frequencies");
    for (auto* c : { wordLength.get(), dither.get(), shaping.get() })
        addAndMakeVisible (*c);

    setSize (editorW, editorH);
    startTimerHz (30);
}

MasteringLimiterEditor::~MasteringLimiterEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

float MasteringLimiterEditor::valueDb (juce::RangedAudioParameter* p) const { return p->convertFrom0to1 (p->getValue()); }

bool MasteringLimiterEditor::isLinked() const
{
    return bool (proc.getState().state.getProperty (linkMarkersProperty, false));
}

juce::RangedAudioParameter* MasteringLimiterEditor::partnerOf (juce::RangedAudioParameter* p) const
{
    return p == threshold ? ceiling : threshold;
}

void MasteringLimiterEditor::beginLevelGesture (juce::RangedAudioParameter* p)
{
    p->beginChangeGesture();
    if (isLinked())
        partnerOf (p)->beginChangeGesture();
}

void MasteringLimiterEditor::endLevelGesture (juce::RangedAudioParameter* p)
{
    p->endChangeGesture();
    if (isLinked())
        partnerOf (p)->endChangeGesture();
}

void MasteringLimiterEditor::applyLevel (juce::RangedAudioParameter* p, float db)
{
    const float lo = float (dsp::param::Level::minValue), hi = float (dsp::param::Level::maxValue);
    db = float (dsp::param::Level::snap (juce::jlimit (lo, hi, db)));
    if (! isLinked())
    {
        p->setValueNotifyingHost (p->convertTo0to1 (db));
        return;
    }
    // keep the offset: limit the move so that the partner stays inside its range too
    auto* other = partnerOf (p);
    const float cur = valueDb (p), otherCur = valueDb (other);
    const float delta = juce::jlimit (lo - otherCur, hi - otherCur, db - cur);
    p->setValueNotifyingHost (p->convertTo0to1 (float (dsp::param::Level::snap (cur + delta))));
    other->setValueNotifyingHost (other->convertTo0to1 (float (dsp::param::Level::snap (otherCur + delta))));
}

void MasteringLimiterEditor::timerCallback()
{
    thresholdBox->refresh();
    ceilingBox->refresh();
    releaseBox->refresh();
    releaseBox->setEnabled (! adaptiveButton.getToggleState()); // spec 2 note / D2
    linkButton.setToggleState (isLinked(), juce::dontSendNotification);

    const int ch = proc.getNumMeterChannels();
    inputMeter.setChannels (ch);
    outputMeter.setChannels (ch);
    inputMeter.setMarkerDb (valueDb (threshold));
    outputMeter.setMarkerDb (valueDb (ceiling));
    inputMeter.setLinkedHighlight (isLinked());
    outputMeter.setLinkedHighlight (isLinked());

    const auto m = proc.takeMeters();
    inputMeter.pushLevels (m.input[0], ch > 1 ? m.input[1] : m.input[0]);
    outputMeter.pushLevels (m.output[0], ch > 1 ? m.output[1] : m.output[0]);
    grMeter.pushGain (m.minGain);
    attenReadout.push (-linToDb (m.minGain));
    for (int c = 0; c < 2; ++c)
    {
        outPeak[c].setVisible (c < ch);
        outPeak[c].push (linToDb (m.output[c]));
    }
}

void MasteringLimiterEditor::paint (juce::Graphics& g)
{
    g.fillAll (colours::background);

    // header
    auto header = getLocalBounds().removeFromTop (headerH);
    g.setColour (colours::panel);
    g.fillRect (header);
    g.setColour (colours::accent);
    g.fillRect (header.removeFromBottom (2));
    auto h = getLocalBounds().removeFromTop (headerH).reduced (margin, 0);

    // logo mark: a waveform squeezed under a flat lid
    {
        auto mark = h.removeFromLeft (30).toFloat().withSizeKeepingCentre (26, 20);
        juce::Path wave;
        for (int i = 0; i <= 40; ++i)
        {
            const float t = float (i) / 40.0f;
            const float raw = std::sin (juce::MathConstants<float>::twoPi * 2.0f * t) * (0.4f + 0.8f * t);
            const float v = juce::jlimit (-0.62f, 0.62f, raw);
            const juce::Point<float> pt (mark.getX() + t * mark.getWidth(), mark.getCentreY() - v * mark.getHeight() * 0.7f);
            if (i == 0)
                wave.startNewSubPath (pt);
            else
                wave.lineTo (pt);
        }
        g.setColour (colours::accent2);
        g.strokePath (wave, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (colours::accent);
        const float lid = mark.getCentreY() - 0.62f * mark.getHeight() * 0.7f - 2.0f;
        g.fillRect (juce::Rectangle<float> (mark.getX(), lid - 1.0f, mark.getWidth(), 2.0f));
    }
    h.removeFromLeft (8);
    g.setColour (colours::text);
    g.setFont (juce::Font (uiFont (17.0f, true)));
    g.drawText ("CRAFT MASTERING LIMITER", h, juce::Justification::centredLeft);
    g.setColour (colours::textDim);
    g.setFont (juce::Font (uiFont (11.0f)));
    const double fs = proc.getSampleRate() > 0.0 ? proc.getSampleRate() : 48000.0;
    g.drawText ("latency " + juce::String (proc.getLatencySamples()) + " smp @ " + juce::String (fs / 1000.0, 1)
                    + " kHz     CRAFT AUDIO  v" CRAFT_VERSION_STRING,
                h, juce::Justification::centredRight);

    const char* titles[4] = { "Input / Threshold", "Atten", "Output / Ceiling", "Release / Requantise" };
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

    // make-up gain readout between the panels
    {
        const float mk = valueDb (ceiling) - valueDb (threshold);
        auto r = panels[1].reduced (12, 8);
        r = r.withTop (attenReadout.getBottom() + 6).withHeight (16);
        g.setColour (colours::textDim);
        g.setFont (juce::Font (uiFont (10.5f)));
        g.drawText ("make-up " + juce::String (mk >= 0.0f ? "+" : "") + juce::String (mk, 1) + " dB", r,
                    juce::Justification::centred);
    }
}

void MasteringLimiterEditor::resized()
{
    captions.clear();
    auto body = getLocalBounds().withTrimmedTop (headerH).reduced (margin);
    panels[3] = body.removeFromBottom (bottomH);
    body.removeFromBottom (margin);
    panels[0] = body.removeFromLeft (leftW);
    body.removeFromLeft (margin);
    panels[1] = body.removeFromLeft (centreW);
    body.removeFromLeft (margin);
    panels[2] = body;

    auto content = [] (juce::Rectangle<int> panel) {
        auto r = panel.reduced (12, 8);
        r.removeFromTop (22);
        return r;
    };

    // input + threshold
    {
        auto a = content (panels[0]);
        auto bottom = a.removeFromBottom (rowH);
        captions.emplace_back (bottom.removeFromLeft (80), "THRESHOLD dB");
        thresholdBox->setBounds (bottom.removeFromRight (90));
        a.removeFromBottom (6);
        inputMeter.setBounds (a.withSizeKeepingCentre (std::min (a.getWidth(), 150), a.getHeight()));
    }
    // gain reduction
    {
        auto a = content (panels[1]);
        auto link = a.removeFromBottom (rowH);
        linkButton.setBounds (link.withSizeKeepingCentre (90, rowH));
        a.removeFromBottom (24); // make-up text
        auto rd = a.removeFromBottom (rowH);
        attenReadout.setBounds (rd.withSizeKeepingCentre (80, 22));
        a.removeFromBottom (4);
        grMeter.setBounds (a.withSizeKeepingCentre (std::min (a.getWidth(), 110), a.getHeight()));
    }
    // output + ceiling + peak holds
    {
        auto a = content (panels[2]);
        auto bottom = a.removeFromBottom (rowH);
        captions.emplace_back (bottom.removeFromLeft (100), "OUT CEILING dB");
        ceilingBox->setBounds (bottom.removeFromRight (90));
        a.removeFromBottom (6);
        auto meterArea = a.removeFromLeft (150);
        outputMeter.setBounds (meterArea);
        a.removeFromLeft (12);
        auto holds = a.withSizeKeepingCentre (a.getWidth(), 3 * captionH + 2 * 24);
        captions.emplace_back (holds.removeFromTop (captionH), "PEAK HOLD dBFS");
        for (int c = 0; c < 2; ++c)
        {
            auto row = holds.removeFromTop (28);
            captions.emplace_back (row.removeFromLeft (16), c == 0 ? "L" : "R");
            outPeak[c].setBounds (row.removeFromLeft (80).withSizeKeepingCentre (80, 22));
        }
    }
    // bottom row
    {
        auto a = content (panels[3]);
        auto group = [&] (int w, const juce::String& caption) {
            auto gr = a.removeFromLeft (w);
            a.removeFromLeft (14);
            captions.emplace_back (gr.removeFromTop (captionH), caption);
            gr.removeFromTop (4);
            return gr.removeFromTop (rowH + 2);
        };
        auto rel = group (190, "RELEASE");
        releaseBox->setBounds (rel.removeFromLeft (96));
        rel.removeFromLeft (8);
        adaptiveButton.setBounds (rel);
        wordLength->setBounds (group (150, "WORD LENGTH (BITS)"));
        dither->setBounds (group (150, "DITHER"));
        shaping->setBounds (a.removeFromTop (captionH + 4 + rowH + 2).withTrimmedTop (captionH + 4));
        captions.emplace_back (shaping->getBounds().translated (0, -(captionH + 4)).withHeight (captionH), "NOISE SHAPING");
    }
}

} // namespace craft::mastering_limiter
