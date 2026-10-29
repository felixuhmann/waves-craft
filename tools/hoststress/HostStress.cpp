// Craft Audio - headless plug-in stress host (development aid, shared by all plug-ins; not shipped).
//
// Loads a VST3 (or any format JUCE can host) and, for a given wall-clock time, keeps cycling through
// configurations: sample rate 44.1 / 48 / 96 kHz, stereo or mono-in layouts, float or double precision,
// prepared maximum block size 32 ... 4096. Per configuration an audio thread processes noise, impulses, silence,
// denormal-range, full-scale square and DC signals with random block sizes (1 ... prepared maximum) while
// randomly automating every parameter (sometimes to the extreme ends). Concurrently the message thread
// automates parameters through gestures, saves / restores the state, re-prepares / releases the plug-in and
// (with --editor) opens and closes the editor in a window. Every output sample is checked for NaN / Inf and
// runaway (> +40 dBFS).
//
// Usage: craft_hoststress <plugin file> [--seconds N] [--seed S] [--editor]
// Exit code 0 = no failure.
#include <atomic>
#include <cmath>
#include <iostream>
#include <mutex>
#include <random>
#include <thread>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace
{
struct Stats
{
    std::atomic<long long> samples { 0 }, blocks { 0 }, failures { 0 }, prepares { 0 }, stateRestores { 0 },
        paramMoves { 0 }, editorOpens { 0 };
};

class StressHost
{
public:
    StressHost (juce::AudioPluginInstance& p, unsigned seed, Stats& s) : plugin (p), rng (seed), stats (s) {}

    // ---- audio thread -------------------------------------------------------------------------------
    void audioLoop (double seconds)
    {
        const auto end = juce::Time::getMillisecondCounterHiRes() + seconds * 1000.0;
        while (juce::Time::getMillisecondCounterHiRes() < end && ! stop.load())
        {
            {
                std::lock_guard<std::mutex> lock (prepareLock);
                if (prepared)
                    processSomeBlocks (64);
            }
            std::this_thread::yield();
        }
        stop = true;
    }

    // ---- message thread -----------------------------------------------------------------------------
    void prepareRandom()
    {
        std::lock_guard<std::mutex> lock (prepareLock);
        if (prepared)
            plugin.releaseResources();
        const double rates[] = { 44100.0, 48000.0, 96000.0 };
        const int maxBlocks[] = { 32, 64, 256, 512, 1024, 4096, 4096 };
        sampleRate = rates[rng() % 3];
        maxBlock = maxBlocks[rng() % 7];

        auto layout = plugin.getBusesLayout();
        const bool monoIn = rng() % 3 == 0;
        auto tryLayout = layout;
        if (tryLayout.inputBuses.size() > 0)
            tryLayout.inputBuses.getReference (0) = monoIn ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo();
        if (tryLayout.outputBuses.size() > 0)
            tryLayout.outputBuses.getReference (0) = juce::AudioChannelSet::stereo();
        if (plugin.checkBusesLayoutSupported (tryLayout))
            plugin.setBusesLayout (tryLayout);

        doublePrecision = plugin.supportsDoublePrecisionProcessing() && rng() % 2 == 0;
        plugin.setProcessingPrecision (doublePrecision ? juce::AudioProcessor::doublePrecision
                                                       : juce::AudioProcessor::singlePrecision);
        plugin.setRateAndBufferSizeDetails (sampleRate, maxBlock);
        plugin.prepareToPlay (sampleRate, maxBlock);
        const int ch = std::max (2, std::max (plugin.getTotalNumInputChannels(), plugin.getTotalNumOutputChannels()));
        fbuf.setSize (ch, maxBlock);
        dbuf.setSize (ch, maxBlock);
        prepared = true;
        ++stats.prepares;
        std::cout << "  prepare: " << sampleRate << " Hz, max block " << maxBlock << ", "
                  << (doublePrecision ? "double" : "float") << ", in " << plugin.getTotalNumInputChannels() << " ch"
                  << std::endl;
    }

    void releaseOnly()
    {
        std::lock_guard<std::mutex> lock (prepareLock);
        if (prepared)
            plugin.releaseResources();
        prepared = false;
    }

    void messageThreadAction()
    {
        auto& params = plugin.getParameters();
        const int r = int (rng() % 100);
        if (r < 60 && params.size() > 0)
        {
            auto* p = params[int (rng() % unsigned (params.size()))];
            p->beginChangeGesture();
            p->setValueNotifyingHost (randomValue (*p));
            p->endChangeGesture();
            ++stats.paramMoves;
        }
        else if (r < 80)
        {
            juce::MemoryBlock mb;
            plugin.getStateInformation (mb);
            plugin.setStateInformation (mb.getData(), int (mb.getSize())); // restore while audio is running
            ++stats.stateRestores;
        }
        else if (r < 83)
            prepareRandom();
        else if (r < 84)
        {
            releaseOnly();
            prepareRandom();
        }
    }

    std::atomic<bool> stop { false };

private:
    float randomValue (juce::AudioProcessorParameter& p)
    {
        const int r = int (rng() % 10);
        if (r < 2) return 0.0f;
        if (r < 4) return 1.0f;
        const int steps = p.getNumSteps();
        const float u = float (rng() % 1000001) / 1000000.0f;
        if (p.isDiscrete() && steps > 1 && steps < 1000)
            return float (int (u * float (steps - 1) + 0.5f)) / float (steps - 1);
        return u;
    }

    template <typename Sample>
    void fill (juce::AudioBuffer<Sample>& b, int n)
    {
        std::uniform_real_distribution<double> u (-1.0, 1.0);
        for (int c = 0; c < b.getNumChannels(); ++c)
        {
            auto* d = b.getWritePointer (c);
            for (int i = 0; i < n; ++i)
            {
                double v = 0.0;
                switch (signal)
                {
                    case 0: v = u (rng); break;
                    case 1: v = (rng() % 300 == 0) ? (rng() % 2 ? 1.0 : -1.0) : 0.0; break;
                    case 2: v = 0.0; break;
                    case 3: v = 1.0e-39 * u (rng); break;
                    case 4: v = ((phase++ / 37) % 2) ? 1.0 : -1.0; break;
                    default: v = c == 0 ? 1.0 : -1.0; break;
                }
                d[i] = Sample (v);
            }
        }
    }

    template <typename Sample>
    void runBlock (juce::AudioBuffer<Sample>& buf, int n)
    {
        juce::AudioBuffer<Sample> view (buf.getArrayOfWritePointers(), buf.getNumChannels(), n);
        fill (view, n);
        juce::MidiBuffer midi;
        plugin.processBlock (view, midi);
        for (int c = 0; c < view.getNumChannels(); ++c)
        {
            const auto* d = view.getReadPointer (c);
            for (int i = 0; i < n; ++i)
                if (! std::isfinite (double (d[i])) || std::abs (double (d[i])) > 100.0)
                {
                    if (++stats.failures <= 20)
                        std::cout << "FAIL: ch " << c << " sample " << i << " value " << double (d[i]) << " at "
                                  << sampleRate << " Hz, block " << n << ", signal " << signal << std::endl;
                    break;
                }
        }
        stats.samples += n;
        ++stats.blocks;
    }

    void processSomeBlocks (int count)
    {
        auto& params = plugin.getParameters();
        for (int k = 0; k < count; ++k)
        {
            // host automation from the audio thread
            if (params.size() > 0 && rng() % 4 == 0)
            {
                auto* p = params[int (rng() % unsigned (params.size()))];
                p->setValue (randomValue (*p));
                ++stats.paramMoves;
            }
            if (rng() % 64 == 0)
                signal = int (rng() % 6);
            const int n = 1 + int (rng() % unsigned (maxBlock));
            if (doublePrecision)
                runBlock (dbuf, n);
            else
                runBlock (fbuf, n);
        }
    }

    juce::AudioPluginInstance& plugin;
    std::mt19937 rng;
    Stats& stats;
    std::mutex prepareLock;
    bool prepared = false, doublePrecision = false;
    double sampleRate = 48000.0;
    int maxBlock = 512, signal = 0;
    long long phase = 0;
    juce::AudioBuffer<float> fbuf;
    juce::AudioBuffer<double> dbuf;
};
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    if (argc < 2)
    {
        std::cout << "usage: craft_hoststress <plugin file> [--seconds N] [--seed S] [--editor]" << std::endl;
        return 2;
    }
    const juce::String path (argv[1]);
    double seconds = 60.0;
    unsigned seed = 1;
    bool withEditor = false;
    for (int i = 2; i < argc; ++i)
    {
        const juce::String a (argv[i]);
        if (a == "--seconds" && i + 1 < argc) seconds = juce::String (argv[++i]).getDoubleValue();
        else if (a == "--seed" && i + 1 < argc) seed = unsigned (juce::String (argv[++i]).getIntValue());
        else if (a == "--editor") withEditor = true;
    }

    juce::AudioPluginFormatManager fm;
    juce::addDefaultFormatsToManager (fm);
    juce::OwnedArray<juce::PluginDescription> types;
    for (auto* f : fm.getFormats())
        if (f->fileMightContainThisPluginType (path))
            f->findAllTypesForFile (types, path);
    if (types.isEmpty())
    {
        std::cout << "no plug-in found in " << path << std::endl;
        return 2;
    }
    juce::String error;
    auto plugin = fm.createPluginInstance (*types[0], 48000.0, 512, error);
    if (plugin == nullptr)
    {
        std::cout << "could not load: " << error << std::endl;
        return 2;
    }
    std::cout << "loaded " << plugin->getName() << " (" << plugin->getParameters().size() << " parameters), "
              << seconds << " s, seed " << seed << (withEditor ? ", with editor" : "") << std::endl;

    Stats stats;
    StressHost host (*plugin, seed, stats);
    host.prepareRandom();
    std::thread audio ([&] { host.audioLoop (seconds); });

    std::mt19937 rng (seed * 7919u + 1u);
    std::unique_ptr<juce::DocumentWindow> window;
    juce::AudioProcessorEditor* editor = nullptr;
    auto closeEditor = [&] {
        if (window != nullptr)
        {
            window->clearContentComponent();
            window.reset();
        }
        if (editor != nullptr)
        {
            delete editor;
            editor = nullptr;
        }
    };
    const auto start = juce::Time::getMillisecondCounterHiRes();
    double lastReport = start;
    while (! host.stop.load())
    {
        juce::MessageManager::getInstance()->runDispatchLoopUntil (20);
        host.messageThreadAction();
        if (withEditor && rng() % 40 == 0)
        {
            if (editor == nullptr)
            {
                editor = plugin->createEditorAndMakeActive();
                if (editor != nullptr)
                {
                    window = std::make_unique<juce::DocumentWindow> ("hoststress", juce::Colours::black,
                                                                     juce::DocumentWindow::closeButton, true);
                    window->setContentNonOwned (editor, true);
                    window->setVisible (true);
                    ++stats.editorOpens;
                }
            }
            else
                closeEditor();
        }
        const auto now = juce::Time::getMillisecondCounterHiRes();
        if (now - lastReport > 10000.0)
        {
            lastReport = now;
            std::cout << "  t=" << int ((now - start) / 1000.0) << " s: " << stats.samples.load() << " samples, "
                      << stats.failures.load() << " failures" << std::endl;
        }
    }
    audio.join();
    closeEditor();
    host.releaseOnly();
    plugin.reset();

    std::cout << "done: " << stats.samples.load() << " samples in " << stats.blocks.load() << " blocks, "
              << stats.prepares.load() << " prepares, " << stats.stateRestores.load() << " state restores, "
              << stats.paramMoves.load() << " parameter moves, " << stats.editorOpens.load() << " editor opens, "
              << stats.failures.load() << " FAILURES" << std::endl;
    return stats.failures.load() == 0 ? 0 : 1;
}
