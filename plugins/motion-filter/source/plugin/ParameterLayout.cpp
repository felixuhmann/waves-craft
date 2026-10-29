#include "ParameterLayout.h"

#include "dsp/Parameters.h"

namespace craft::mfp
{

namespace mf = craft::mf;

juce::String ids::step (int index0) { return "step" + juce::String (index0 + 1); }

juce::String ids::mod (int source, int dest)
{
    static const char* src[] = { "Lfo", "Seq", "Fol" };
    static const char* dst[] = { "Freq", "Res", "Time" };
    return juce::String ("mod") + src[juce::jlimit (0, 2, source)] + dst[juce::jlimit (0, 2, dest)];
}

namespace
{
juce::String trimmed (double v, int decimals)
{
    const double q = std::pow (10.0, decimals);
    v = std::round (v * q) / q;
    if (v == 0.0)
        v = 0.0; // no "-0"
    return juce::String (v, decimals);
}

juce::String formatHz (double hz)
{
    if (hz >= 10000.0) return trimmed (hz / 1000.0, 1) + " kHz";
    if (hz >= 1000.0)  return trimmed (hz / 1000.0, 2) + " kHz";
    if (hz >= 100.0)   return trimmed (hz, 0) + " Hz";
    return trimmed (hz, 1) + " Hz";
}

juce::String signedText (double v, int decimals, const juce::String& unit)
{
    auto t = trimmed (v, decimals);
    if (std::round (v * std::pow (10.0, decimals)) > 0)
        t = "+" + t;
    return t + unit;
}

using ToText = std::function<juce::String (double)>;

// Float parameter whose normalised value is r and whose value is the spec's mapping of r.
template <typename P>
std::unique_ptr<juce::AudioParameterFloat> makeFloat (const juce::String& id, const juce::String& name, const juce::String& unit,
                                                      ToText toText)
{
    juce::NormalisableRange<float> range (
        float (P::minValue), float (P::maxValue),
        [] (float, float, float r) { return float (P::fromNorm (r)); },
        [] (float, float, float v) { return float (P::toNorm (v)); });

    auto attributes = juce::AudioParameterFloatAttributes()
                          .withLabel (unit)
                          .withStringFromValueFunction ([toText] (float v, int) { return toText (double (v)); })
                          .withValueFromStringFunction ([] (const juce::String& text) {
                              auto t = text.trim().toLowerCase();
                              double mult = 1.0;
                              if (t.containsChar ('k'))
                                  mult = 1000.0;
                              return float (juce::jlimit (P::minValue, P::maxValue, t.getDoubleValue() * mult));
                          });
    return std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, range, float (P::defaultValue),
                                                        attributes);
}

std::unique_ptr<juce::AudioParameterBool> makeBool (const juce::String& id, const juce::String& name, bool def,
                                                    const juce::String& offText, const juce::String& onText)
{
    auto attributes = juce::AudioParameterBoolAttributes()
                          .withStringFromValueFunction ([offText, onText] (bool v, int) { return v ? onText : offText; })
                          .withValueFromStringFunction ([onText] (const juce::String& t) {
                              return t.trim().equalsIgnoreCase (onText) || t.trim() == "1";
                          });
    return std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id, 1 }, name, def, attributes);
}

template <size_t N>
juce::StringArray noteNames (const std::array<mf::NoteLength, N>& list)
{
    juce::StringArray a;
    for (const auto& n : list)
        a.add (n.text);
    return a;
}

std::unique_ptr<juce::AudioParameterChoice> makeChoice (const juce::String& id, const juce::String& name,
                                                        const juce::StringArray& items, int def)
{
    return std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id, 1 }, name, items, def);
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    namespace pm = mf::param;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // filter
    auto type = makeChoice (ids::type, "Filter Type", { "Low-pass", "High-pass", "Band-pass", "Band-reject", "Comb", "Amp" }, 0);
    auto* typeParam = type.get();
    layout.add (std::move (type));
    layout.add (makeBool (ids::slope, "Slope", false, "12 dB", "24 dB"));

    // Freq/Delay/Gain share the knob position r: the displayed value follows the filter type.
    layout.add (makeFloat<pm::Freq> (ids::freq, "Freq/Delay/Gain", "", [typeParam] (double hz) {
        const double r = pm::Freq::toNorm (hz);
        switch (typeParam->getIndex())
        {
            case int (mf::FilterType::Comb): return trimmed (pm::combDelayMs (r), 1) + " ms";
            case int (mf::FilterType::Amp):
            {
                const double d = pm::ampGainDb (r);
                return d <= -79.95 ? juce::String ("-inf dB") : trimmed (d, 1) + " dB";
            }
            default: return formatHz (hz);
        }
    }));
    layout.add (makeFloat<pm::Resonance> (ids::res, "Resonance/Feedback", "%", [] (double v) { return trimmed (v, 0) + " %"; }));
    layout.add (makeFloat<pm::Spread> (ids::spread, "Spread", "%", [] (double v) { return signedText (v, 0, " %"); }));
    layout.add (makeFloat<pm::Smooth> (ids::smooth, "Smooth", "%", [] (double v) { return trimmed (v, 0) + " %"; }));
    layout.add (makeFloat<pm::Drive> (ids::drive, "Drive", "%", [] (double v) { return trimmed (v, 0) + " %"; }));
    layout.add (makeFloat<pm::Crush> (ids::crush, "Crush", "", [] (double v) { return v < 0.05 ? juce::String ("Off") : trimmed (v, 0); }));
    layout.add (makeBool (ids::kbd, "Keyboard Tracking", false, "Off", "On"));

    // mixer
    layout.add (makeFloat<pm::Gain> (ids::input, "Input", "dB", [] (double v) { return signedText (v, 1, " dB"); }));
    layout.add (makeFloat<pm::Gain> (ids::output, "Output", "dB", [] (double v) { return signedText (v, 1, " dB"); }));
    layout.add (makeFloat<pm::Mix> (ids::mix, "Mix", "", [] (double v) {
        if (v <= -99.5) return juce::String ("Dry");
        if (v >= 99.5) return juce::String ("Wet");
        return signedText (v, 0, "");
    }));

    // LFO
    layout.add (makeChoice (ids::lfoShape, "LFO Shape", { "Sine", "Triangle", "Saw", "Pulse", "S&H" }, 0));
    layout.add (makeFloat<pm::LfoRate> (ids::lfoRate, "LFO Rate", "Hz", [] (double v) { return trimmed (v, v < 10 ? 2 : 1) + " Hz"; }));
    layout.add (makeBool (ids::lfoSync, "LFO Sync", true, "Free", "Sync"));
    layout.add (makeChoice (ids::lfoSyncRate, "LFO Sync Rate", noteNames (mf::lfoSyncChoices), mf::lfoSyncDefault));

    // sequencer
    for (int k = 0; k < 16; ++k)
        layout.add (makeFloat<pm::Step> (ids::step (k), "Step " + juce::String (k + 1), "%",
                                         [] (double v) { return signedText (v, 0, " %"); }));
    layout.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { ids::seqLast, 1 }, "Last Step", 1, 16, 16));
    layout.add (makeFloat<pm::SeqRate> (ids::seqRate, "Sequencer Rate", "steps/s",
                                        [] (double v) { return trimmed (v, v < 10 ? 2 : (v < 100 ? 1 : 0)) + " /s"; }));
    layout.add (makeBool (ids::seqSync, "Sequencer Sync", true, "Free", "Sync"));
    layout.add (makeChoice (ids::seqSyncRate, "Sequencer Sync Rate", noteNames (mf::seqSyncChoices), mf::seqSyncDefault));
    layout.add (makeFloat<pm::Swing> (ids::swing, "Swing", "%", [] (double v) { return trimmed (v, 0) + " %"; }));

    // delay
    layout.add (makeFloat<pm::DelayTime> (ids::dlyTime, "Delay Time", "ms", [] (double v) { return trimmed (v, 0) + " ms"; }));
    layout.add (makeBool (ids::dlySync, "Delay Sync", true, "Free", "Sync"));
    layout.add (makeChoice (ids::dlySyncRate, "Delay Sync Rate", noteNames (mf::delaySyncChoices), mf::delaySyncDefault));
    layout.add (makeChoice (ids::dlyRatio, "Delay Right Ratio", { "x1", "x1.5", "x2" }, 0));
    layout.add (makeFloat<pm::Feedback> (ids::dlyFeedback, "Delay Feedback", "%", [] (double v) { return trimmed (v, 0) + " %"; }));
    layout.add (makeFloat<pm::DelayMix> (ids::dlyMix, "Delay Dry/Wet", "", [] (double v) {
        if (v <= -99.5) return juce::String ("Dry");
        if (v >= 99.5) return juce::String ("Delay");
        return signedText (v, 0, "");
    }));
    layout.add (makeBool (ids::dlyMode, "Delay Mode", false, "Digital", "Analog"));
    layout.add (makeBool (ids::dlyRoute, "Delay Route", false, "Pre", "Feedback"));

    // follower / modulation amounts
    layout.add (makeBool (ids::folSource, "Follower Source", false, "Input", "Sidechain"));
    static const char* srcNames[] = { "LFO", "Seq", "Follower" };
    static const char* dstNames[] = { "Freq", "Res", "Time" };
    for (int d = 0; d < 3; ++d)
        for (int src = 0; src < 3; ++src)
            layout.add (makeFloat<pm::ModAmount> (ids::mod (src, d), juce::String (srcNames[src]) + " > " + dstNames[d], "%",
                                                  [] (double v) { return signedText (v, 0, " %"); }));

    layout.add (makeBool (ids::bypass, "Bypass", false, "Off", "On"));
    return layout;
}

} // namespace craft::mfp
