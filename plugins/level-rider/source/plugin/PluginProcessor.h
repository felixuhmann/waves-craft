// Craft Level Rider - AudioProcessor (stereo or mono main bus, optional music sidechain bus).
// One class serves both products; the Live product adds the Spill parameter (section 7.4).
#pragma once

#include <atomic>

#include <juce_audio_processors/juce_audio_processors.h>

#include "ParameterLayout.h"
#include "dsp/RiderEngine.h"

namespace craft::levelrider
{

class LevelRiderProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    explicit LevelRiderProcessor (bool live);
    ~LevelRiderProcessor() override;

    bool isLive() const noexcept { return live; }

    // AudioProcessor
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlock (juce::AudioBuffer<double>&, juce::MidiBuffer&) override;
    bool supportsDoublePrecisionProcessing() const override { return true; }
    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParam; }

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override;
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getState() noexcept { return state; }

    // ---- section 9: automation mode and Rider fader --------------------------------------------
    dsp::AutomationMode getAutomationMode() const noexcept { return dsp::AutomationMode (automationMode.load()); }
    void setAutomationMode (dsp::AutomationMode m); // message thread; saved with the state
    // GUI drag of the Rider fader (message thread): overrides g while held.
    void beginRiderDrag (double db);
    void dragRider (double db);
    void endRiderDrag();

    // ---- display values (GUI thread) ------------------------------------------------------------
    struct Display
    {
        float gainDb = 0.0f;          // current ride gain g
        float vocalLevelDb = -200.0f; // detected (weighted, smoothed) vocal level
        float musicOffsetDb = 0.0f;   // |T_eff - T| indicator (signed)
        bool riding = false;
        bool sidechainActive = false;
        float outputPeak = 0.0f;      // linear, since the previous call
        bool clipped = false;         // latched until clearClip()
        float rangeMinDb = -6.0f, rangeMaxDb = 6.0f;
    };
    Display takeDisplay() noexcept;
    void clearClip() noexcept { clipLatch = false; }

private:
    template <typename Sample>
    void processAudio (juce::AudioBuffer<Sample>& buffer);
    dsp::Settings readSettings() noexcept; // audio thread (tracks the Range crossing rule)
    void timerCallback() override;          // message thread: Rider parameter sync, Range push

    float normalised (juce::RangedAudioParameter* p) const noexcept { return p != nullptr ? p->getValue() : 0.0f; }

    const bool live;
    juce::AudioProcessorValueTreeState state;
    dsp::RiderEngine engine;

    juce::RangedAudioParameter *target = nullptr, *vocalSens = nullptr, *musicSens = nullptr, *rangeMax = nullptr,
                               *rangeMin = nullptr, *idle = nullptr, *rider = nullptr, *output = nullptr, *attack = nullptr,
                               *spill = nullptr;
    juce::AudioParameterBool* bypassParam = nullptr;

    // section 2 Range rule: the most recently changed control wins
    float lastMinRaw = -1.0f, lastMaxRaw = -1.0f;
    bool maxChangedLast = true;
    std::atomic<float> resolvedMin { -6.0f }, resolvedMax { 6.0f };

    std::atomic<int> automationMode { int (dsp::AutomationMode::Off) };
    std::atomic<bool> overrideActive { false };
    std::atomic<float> overrideDb { 0.0f };
    bool writeGestureOpen = false; // message thread
    bool dragGestureOpen = false;  // message thread

    std::atomic<float> dispGain { 0.0f }, dispLevel { -200.0f }, dispMusic { 0.0f }, dispOutPeak { 0.0f };
    std::atomic<bool> dispRiding { false }, dispSidechain { false }, clipLatch { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LevelRiderProcessor)
};

} // namespace craft::levelrider
