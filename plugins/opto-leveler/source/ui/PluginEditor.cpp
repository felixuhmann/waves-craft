#include "PluginEditor.h"

namespace craft::opto_leveler
{

using namespace ui;

namespace
{
constexpr int editorW = 780, editorH = 500, headerH = 40, margin = 12;
constexpr int sideW = 238, bottomH = 150, captionH = 16, rowH = 26;

std::vector<Knob::Tick> hundredScale()
{
    std::vector<Knob::Tick> t;
    for (int v = 0; v <= 100; v += 10)
        t.push_back ({ float (v), v % 20 == 0 ? juce::String (v) : juce::String() });
    return t;
}
} // namespace

OptoLevelerEditor::OptoLevelerEditor (OptoLevelerProcessor& p) : AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&lnf);
    auto& st = proc.getState();

    gainKnob = std::make_unique<Knob> (st, ids::gain, "Gain", colours::accent, hundredScale());
    gainKnob->getSlider().setTooltip ("Gain: make-up gain (32.28 = unity). Drag or type; double-click / Alt-click resets");
    prKnob = std::make_unique<Knob> (st, ids::peakReduction, "Peak Reduction", colours::accent, hundredScale());
    prKnob->getSlider().setTooltip ("Peak Reduction: amount of leveling. Drag or type; double-click / Alt-click resets");
    hiFreqKnob = std::make_unique<Knob> (st, ids::hiFreq, "Hi Freq", colours::accent2,
                                         std::vector<Knob::Tick> { { 0.0f, "0" }, { 50.0f, "" }, { 100.0f, "flat" } });
    hiFreqKnob->getSlider().setTooltip ("Hi Freq: detector frequency weighting (100 = flat; lower values make the "
                                        "detector less sensitive to the mid range)");
    mixKnob = std::make_unique<Knob> (st, ids::mix, "Mix", colours::accent2);
    mixKnob->getSlider().setTooltip ("Mix: blend of processed and unprocessed signal");
    trimKnob = std::make_unique<Knob> (st, ids::trim, "Trim", colours::accent2);
    trimKnob->getSlider().setTooltip ("Trim: output level (dB)");
    for (auto* k : { gainKnob.get(), prKnob.get(), hiFreqKnob.get(), mixKnob.get(), trimKnob.get() })
        addAndMakeVisible (*k);

    modeChoice = std::make_unique<SegmentedChoice> (*st.getParameter (ids::mode), juce::StringArray { "COMPRESS", "LIMIT" },
                                                    colours::accent);
    modeChoice->setTooltip ("Compress: gentle ratio; Limit: steeper ratio");
    vuParam = st.getParameter (ids::vu);
    vuChoice = std::make_unique<SegmentedChoice> (*vuParam, juce::StringArray { "IN", "GR", "OUT" }, colours::accent2);
    vuChoice->setTooltip ("Meter source (display only)");
    analogChoice = std::make_unique<SegmentedChoice> (*st.getParameter (ids::analog),
                                                      juce::StringArray { "OFF", "50 Hz", "60 Hz" }, colours::accent2);
    analogChoice->setTooltip ("Analog: adds low-level noise and mains hum");
    for (auto* c : { modeChoice.get(), vuChoice.get(), analogChoice.get() })
        addAndMakeVisible (*c);

    autoMakeupButton.setClickingTogglesState (true);
    autoMakeupButton.setTooltip ("Auto Makeup: Gain follows Peak Reduction to keep the output level roughly constant");
    autoMakeupAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (st, ids::autoMakeup,
                                                                                                   autoMakeupButton);
    addAndMakeVisible (autoMakeupButton);

    addAndMakeVisible (meter);

    setSize (editorW, editorH);
    lastTick = juce::Time::getMillisecondCounterHiRes() * 1.0e-3;
    startTimerHz (30);
}

OptoLevelerEditor::~OptoLevelerEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void OptoLevelerEditor::timerCallback()
{
    const double now = juce::Time::getMillisecondCounterHiRes() * 1.0e-3;
    const double dt = juce::jlimit (0.0, 0.5, now - lastTick);
    lastTick = now;
    meter.setSource (LevelMeter::Source (juce::jlimit (0, 2, juce::roundToInt (vuParam->convertFrom0to1 (vuParam->getValue())))));
    const auto m = proc.takeMeters();
    if (m.valid)
        meter.push (m.inAverage, m.outAverage, m.grDb, dt);
    else
        meter.push (0.0, 0.0, 0.0, dt);
}

void OptoLevelerEditor::paint (juce::Graphics& g)
{
    g.fillAll (colours::background);

    // header
    auto header = getLocalBounds().removeFromTop (headerH);
    g.setColour (colours::panel);
    g.fillRect (header);
    g.setColour (colours::accent);
    g.fillRect (header.removeFromBottom (2));
    auto h = getLocalBounds().removeFromTop (headerH).reduced (margin, 0);

    // logo mark: a light source over a cell - a small disc with three rays above a bar
    {
        auto mark = h.removeFromLeft (28).toFloat().withSizeKeepingCentre (24, 22);
        const juce::Point<float> sun (mark.getCentreX(), mark.getY() + 8.0f);
        g.setColour (colours::accent);
        g.fillEllipse (juce::Rectangle<float> (8.0f, 8.0f).withCentre (sun));
        for (float a : { -0.9f, 0.0f, 0.9f })
            g.drawLine (sun.x + 6.0f * std::sin (a), sun.y - 6.0f * std::cos (a), sun.x + 9.5f * std::sin (a),
                        sun.y - 9.5f * std::cos (a), 1.5f);
        g.setColour (colours::accent2);
        g.fillRoundedRectangle (juce::Rectangle<float> (mark.getX() + 2.0f, mark.getBottom() - 5.0f, mark.getWidth() - 4.0f, 4.0f),
                                2.0f);
    }
    h.removeFromLeft (8);
    g.setColour (colours::text);
    g.setFont (juce::Font (uiFont (17.0f, true)));
    g.drawText ("CRAFT OPTO LEVELER", h, juce::Justification::centredLeft);
    g.setColour (colours::textDim);
    g.setFont (juce::Font (uiFont (11.0f)));
    g.drawText ("CRAFT AUDIO  v" CRAFT_VERSION_STRING, h, juce::Justification::centredRight);

    drawPanel (g, leftPanel.toFloat());
    drawPanel (g, centrePanel.toFloat());
    drawPanel (g, rightPanel.toFloat());
    drawPanel (g, bottomPanel.toFloat());
    for (const auto& [r, text] : captions)
    {
        g.setColour (colours::textDim);
        g.setFont (juce::Font (uiFont (11.0f, true)));
        g.drawText (text, r, juce::Justification::centred);
    }
}

void OptoLevelerEditor::resized()
{
    captions.clear();
    auto body = getLocalBounds().withTrimmedTop (headerH).reduced (margin);
    bottomPanel = body.removeFromBottom (bottomH);
    body.removeFromBottom (margin);
    leftPanel = body.removeFromLeft (sideW);
    body.removeFromLeft (margin);
    rightPanel = body.removeFromRight (sideW);
    body.removeFromRight (margin);
    centrePanel = body;

    gainKnob->setBounds (leftPanel.reduced (10, 12));
    prKnob->setBounds (rightPanel.reduced (10, 12));

    // centre: meter, meter source, mode, auto makeup
    {
        auto a = centrePanel.reduced (12, 12);
        auto bottom = a.removeFromBottom (rowH);
        modeChoice->setBounds (bottom.removeFromLeft (bottom.getWidth() * 3 / 5 - 4));
        bottom.removeFromLeft (8);
        autoMakeupButton.setBounds (bottom);
        a.removeFromBottom (10);
        auto vuRow = a.removeFromBottom (rowH - 2);
        vuChoice->setBounds (vuRow.withSizeKeepingCentre (std::min (vuRow.getWidth(), 150), vuRow.getHeight()));
        a.removeFromBottom (8);
        meter.setBounds (a);
    }

    // bottom strip: Analog, Hi Freq, Mix, Trim
    {
        auto a = bottomPanel.reduced (14, 8);
        auto analogArea = a.removeFromLeft (200);
        captions.emplace_back (analogArea.removeFromTop (captionH + 14).withTrimmedTop (14), "ANALOG");
        analogArea.removeFromTop (8);
        analogChoice->setBounds (analogArea.removeFromTop (rowH));
        a.removeFromLeft (20);
        const int w = a.getWidth() / 3;
        hiFreqKnob->setBounds (a.removeFromLeft (w).withSizeKeepingCentre (std::min (w, 150), a.getHeight()));
        mixKnob->setBounds (a.removeFromLeft (w).withSizeKeepingCentre (std::min (w, 110), a.getHeight()));
        trimKnob->setBounds (a.withSizeKeepingCentre (std::min (w, 110), a.getHeight()));
    }
}

} // namespace craft::opto_leveler
