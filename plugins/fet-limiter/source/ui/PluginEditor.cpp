#include "PluginEditor.h"

#include "dsp/AutoMakeup.h"

namespace craft::fet_limiter
{

namespace
{
std::vector<ui::Knob::ScaleLabel> levelScale()
{
    // spec 12: -inf, 48, 36, 30, 24, 18, 12, 6, 0 (positions follow the host mapping: r = k / 8)
    const char* texts[] = { "-inf", "48", "36", "30", "24", "18", "12", "6", "0" };
    std::vector<ui::Knob::ScaleLabel> s;
    for (int k = 0; k <= 8; ++k)
        s.push_back ({ float (k) / 8.0f, texts[k] });
    return s;
}

std::vector<ui::Knob::ScaleLabel> timeScale()
{
    std::vector<ui::Knob::ScaleLabel> s;
    for (int k = 1; k <= 7; ++k)
        s.push_back ({ float (k - 1) / 6.0f, juce::String (k) });
    return s;
}

juce::RangedAudioParameter& param (FetLimiterProcessor& p, const char* id) { return *p.getState().getParameter (id); }
} // namespace

FetLimiterEditor::FetLimiterEditor (FetLimiterProcessor& p)
    : AudioProcessorEditor (p), processor (p),
      input (param (p, ids::input), "Input", true, levelScale()),
      output (param (p, ids::output), "Output", true, levelScale()),
      attack (param (p, ids::attack), "Attack", false, timeScale(), "slow  <   >  fast"),
      release (param (p, ids::release), "Release", false, timeScale(), "slow  <   >  fast"),
      mix (param (p, ids::mix), "Mix", false),
      trim (param (p, ids::trim), "Trim", false),
      ratio (param (p, ids::ratio), { "20", "12", "8", "4", "All" }, true),
      meterMode (param (p, ids::meter), { "GR", "In", "Out" }, true),
      analog (param (p, ids::analog), { "50 Hz", "60 Hz", "Off" }, false),
      revision (param (p, ids::revision), { "B", "A" }, false, { 1, 0 }),
      compOff (param (p, ids::compOff), "Comp Off"),
      autoMakeup (param (p, ids::autoMakeup), "Auto Makeup")
{
    setLookAndFeel (&lnf);
    for (juce::Component* c : std::initializer_list<juce::Component*> { &input, &output, &attack, &release, &mix, &trim, &ratio,
                                                                         &meterMode, &analog, &revision, &compOff, &autoMakeup, &meter })
        addAndMakeVisible (c);

    input.setTooltip ("Input level into the gain cell (drives the compression). Double-click / Alt-click resets.");
    output.setTooltip ("Output level after the gain cell.");
    attack.setTooltip ("Attack: 1 = slowest, 7 = fastest.");
    release.setTooltip ("Release (program dependent): 1 = slowest, 7 = fastest.");
    ratio.setTooltip ("Ratio. All = all ratios engaged at once (fast, aggressive).");
    meterMode.setTooltip ("Meter shows gain reduction, input or output level.");
    compOff.setTooltip ("Bypasses the gain reduction; gain staging and colouration stay active.");
    autoMakeup.setTooltip ("Output follows your Input moves so the overall level stays roughly constant.");
    analog.setTooltip ("Analog noise and mains hum (50 / 60 Hz), or Off.");
    revision.setTooltip ("Revision A: late, low-noise voicing. Revision B: early voicing.");
    mix.setTooltip ("Parallel mix of the processed and the dry signal.");
    trim.setTooltip ("Final output trim.");

    // Auto Makeup: Output follows user gestures on Input (not host automation of Input).
    auto& outParam = param (p, ids::output);
    auto& inParam = param (p, ids::input);
    input.onGestureStart = [this, &outParam] {
        linking = autoMakeup.isOn();
        if (linking)
        {
            linkInputStart = paramValue (ids::input);
            linkOutputStart = paramValue (ids::output);
            outParam.beginChangeGesture();
        }
    };
    input.onUserValue = [this, &inParam, &outParam] (float norm) {
        inParam.setValueNotifyingHost (norm);
        if (! linking)
            return;
        const auto s = processor.readSettings();
        const double now = dsp::param::Level::fromNorm (double (norm));
        const double o = dsp::autoMakeupOutput (s.revision, s.ratio, s.compOff, linkInputStart, now, linkOutputStart);
        outParam.setValueNotifyingHost (float (dsp::param::Level::toNorm (o)));
    };
    input.onGestureEnd = [this, &outParam] {
        if (linking)
            outParam.endChangeGesture();
        linking = false;
    };

    meterMode.onChange = [this] (int m) {
        meter.setMode (m == 0 ? ui::NeedleMeter::Mode::GainReduction : ui::NeedleMeter::Mode::Level);
    };
    meter.setMode (meterMode.getSelected() == 0 ? ui::NeedleMeter::Mode::GainReduction : ui::NeedleMeter::Mode::Level);

    applyTheme();
    setSize (940, 400);
    lastTime = juce::Time::getMillisecondCounterHiRes();
    startTimerHz (30);
}

FetLimiterEditor::~FetLimiterEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

double FetLimiterEditor::paramValue (const char* id) const
{
    auto* p = processor.getState().getParameter (id);
    return double (p->convertFrom0to1 (p->getValue()));
}

void FetLimiterEditor::applyTheme()
{
    const int rev = juce::roundToInt (paramValue (ids::revision));
    if (rev == themeRevision)
        return;
    themeRevision = rev;
    const auto accent = rev == 1 ? ui::colours::themeA : ui::colours::themeB;
    lnf.setAccent (accent);
    for (auto* k : { &input, &output, &attack, &release, &mix, &trim })
        k->setAccent (accent);
    for (auto* b : { &ratio, &meterMode, &analog, &revision })
        b->setAccent (accent);
    compOff.setAccent (accent);
    autoMakeup.setAccent (accent);
    meter.setAccent (accent);
    repaint();
}

void FetLimiterEditor::timerCallback()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    const float dt = float (juce::jlimit (0.0, 0.2, (now - lastTime) * 0.001));
    lastTime = now;
    applyTheme();
    const auto m = processor.takeMeters();
    const int mode = meterMode.getSelected();
    const float value = mode == 0 ? m.grDb : (mode == 1 ? m.inputPeak : m.outputPeak);
    meter.update (value, m.outputPeak, dt);
}

void FetLimiterEditor::paint (juce::Graphics& g)
{
    g.fillAll (ui::colours::background);
    const auto accent = lnf.getAccent();

    // header
    g.setColour (ui::colours::text);
    g.setFont (juce::Font (ui::uiFont (17.0f, true)));
    g.drawText ("CRAFT", headerArea.withWidth (70), juce::Justification::centredLeft);
    g.setColour (accent);
    g.drawText ("FET LIMITER", headerArea.withTrimmedLeft (64).withWidth (160), juce::Justification::centredLeft);
    g.setColour (ui::colours::textDim);
    g.setFont (juce::Font (ui::uiFont (11.5f)));
    g.drawText (themeRevision == 1 ? "Revision A  -  late, low-noise voicing" : "Revision B  -  early voicing",
                headerArea.withTrimmedLeft (230).withWidth (300), juce::Justification::centredLeft);
    g.drawText ("Craft Audio  v" CRAFT_VERSION_STRING, headerArea, juce::Justification::centredRight);

    ui::drawSection (g, gainArea, "Gain", accent);
    ui::drawSection (g, dynArea, "Dynamics", accent);
    ui::drawSection (g, meterArea, "Meter", accent);
    ui::drawSection (g, bottomArea, {}, accent);

    // labels of the bottom strip
    g.setColour (ui::colours::textDim);
    g.setFont (juce::Font (ui::uiFont (10.5f, true)));
    auto label = [&g] (juce::Component& c, const char* text) {
        g.drawText (text, c.getBounds().translated (0, -16).withHeight (14), juce::Justification::centredLeft);
    };
    label (analog, "ANALOG");
    label (revision, "REVISION");
    g.drawText ("RATIO", ratio.getBounds().translated (0, -18).withHeight (14), juce::Justification::centred);
}

void FetLimiterEditor::resized()
{
    auto r = getLocalBounds().reduced (12);
    headerArea = r.removeFromTop (30);
    r.removeFromTop (6);
    bottomArea = r.removeFromBottom (86);
    r.removeFromBottom (10);

    gainArea = r.removeFromLeft (320);
    r.removeFromLeft (10);
    meterArea = r.removeFromRight (330);
    r.removeFromRight (10);
    dynArea = r;

    // Gain: two large rotaries
    {
        auto a = gainArea.reduced (10).withTrimmedTop (22);
        input.setBounds (a.removeFromLeft (a.getWidth() / 2));
        output.setBounds (a);
    }
    // Dynamics: Attack / Release + ratio column
    {
        auto a = dynArea.reduced (10).withTrimmedTop (22);
        auto col = a.removeFromRight (64);
        ratio.setBounds (col.withTrimmedTop (22).withTrimmedBottom (4));
        a.removeFromRight (6);
        attack.setBounds (a.removeFromLeft (a.getWidth() / 2).withTrimmedTop (8));
        release.setBounds (a.withTrimmedTop (8));
    }
    // Meter: needle meter + GR / In / Out column + Comp Off
    {
        auto a = meterArea.reduced (10).withTrimmedTop (22);
        auto col = a.removeFromRight (80);
        compOff.setBounds (col.removeFromBottom (30));
        col.removeFromBottom (8);
        meterMode.setBounds (col);
        a.removeFromRight (8);
        meter.setBounds (a);
    }
    // Bottom strip
    {
        auto a = bottomArea.reduced (12, 8);
        auto am = a.removeFromLeft (130);
        autoMakeup.setBounds (am.withSizeKeepingCentre (130, 32));
        a.removeFromLeft (18);
        auto an = a.removeFromLeft (200);
        analog.setBounds (an.withTrimmedTop (22).withHeight (32));
        a.removeFromLeft (18);
        auto rv = a.removeFromLeft (110);
        revision.setBounds (rv.withTrimmedTop (22).withHeight (32));
        a.removeFromLeft (10);
        trim.setBounds (a.removeFromRight (120));
        mix.setBounds (a.removeFromRight (120));
    }
}

} // namespace craft::fet_limiter
