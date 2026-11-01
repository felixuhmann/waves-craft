// Craft Motion Filter - host parameters (spec r1 section 3). The host-normalised value is the spec's r.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace craft::mfp
{

namespace ids
{
    inline constexpr const char* type = "type";
    inline constexpr const char* slope = "slope";
    inline constexpr const char* freq = "freq";
    inline constexpr const char* res = "res";
    inline constexpr const char* spread = "spread";
    inline constexpr const char* smooth = "smooth";
    inline constexpr const char* drive = "drive";
    inline constexpr const char* crush = "crush";
    inline constexpr const char* input = "input";
    inline constexpr const char* output = "output";
    inline constexpr const char* mix = "mix";
    inline constexpr const char* lfoShape = "lfoShape";
    inline constexpr const char* lfoRate = "lfoRate";
    inline constexpr const char* lfoSync = "lfoSync";
    inline constexpr const char* lfoSyncRate = "lfoSyncRate";
    inline constexpr const char* seqLast = "seqLast";
    inline constexpr const char* seqRate = "seqRate";
    inline constexpr const char* seqSync = "seqSync";
    inline constexpr const char* seqSyncRate = "seqSyncRate";
    inline constexpr const char* swing = "swing";
    inline constexpr const char* dlyTime = "dlyTime";
    inline constexpr const char* dlySync = "dlySync";
    inline constexpr const char* dlySyncRate = "dlySyncRate";
    inline constexpr const char* dlyRatio = "dlyRatio";
    inline constexpr const char* dlyFeedback = "dlyFeedback";
    inline constexpr const char* dlyMix = "dlyMix";
    inline constexpr const char* dlyMode = "dlyMode";
    inline constexpr const char* dlyRoute = "dlyRoute";
    inline constexpr const char* folSource = "folSource";
    inline constexpr const char* kbd = "kbd";
    inline constexpr const char* bypass = "bypass";

    // step1 .. step16
    juce::String step (int index0);
    // modulation amounts: source 0..2 (LFO, SEQ, FOLLOW) x destination 0..2 (FREQ, RES, TIME)
    juce::String mod (int source, int dest);

    // non-parameter state (editor preferences)
    inline constexpr const char* drawMode = "stepDrawMode";
} // namespace ids

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

} // namespace craft::mfp
