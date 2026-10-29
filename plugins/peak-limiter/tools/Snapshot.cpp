// Renders the editor to PNG with some programme running through it (development aid, not shipped).
// Usage: peak_limiter_snapshot <output directory>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "plugin/PluginProcessor.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File outDir (argc > 1 ? juce::String (argv[1]) : juce::File::getCurrentWorkingDirectory().getFullPathName());
    outDir.createDirectory();

    using craft::limiter::PeakLimiterProcessor;
    namespace ids = craft::limiter::ids;
    auto set = [] (juce::AudioProcessorValueTreeState& s, const char* id, float physical) {
        if (auto* p = s.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (physical));
    };

    struct Case
    {
        const char* name;
        bool mono;
        float thr, ceil, rel;
        bool autoRel;
    };
    const Case cases[] = { { "default", false, 0.0f, 0.0f, 1.0f, false },
                           { "limiting", false, -12.0f, -0.3f, 31.62f, true },
                           { "mono", true, -6.5f, -1.0f, 316.23f, false } };

    for (const auto& c : cases)
    {
        PeakLimiterProcessor proc;
        if (c.mono)
        {
            auto layout = proc.getBusesLayout();
            layout.inputBuses.getReference (0) = juce::AudioChannelSet::mono();
            layout.outputBuses.getReference (0) = juce::AudioChannelSet::mono();
            proc.setBusesLayout (layout);
        }
        set (proc.getState(), ids::threshold, c.thr);
        set (proc.getState(), ids::ceiling, c.ceil);
        set (proc.getState(), ids::release, c.rel);
        set (proc.getState(), ids::autoRelease, c.autoRel ? 1.0f : 0.0f);

        proc.setRateAndBufferSizeDetails (48000.0, 512);
        proc.prepareToPlay (48000.0, 512);
        std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
        ed->setVisible (true);

        juce::Random rng (7);
        juce::AudioBuffer<float> buf (proc.getTotalNumOutputChannels(), 512);
        juce::MidiBuffer midi;
        for (int block = 0; block < 60; ++block)
        {
            for (int ch = 0; ch < buf.getNumChannels(); ++ch)
                for (int i = 0; i < 512; ++i)
                    buf.setSample (ch, i, (rng.nextFloat() * 2.0f - 1.0f) * (ch == 0 ? 0.9f : 0.6f));
            proc.processBlock (buf, midi);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (12);
        }
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
        auto f = outDir.getChildFile (juce::String ("peak-limiter-") + c.name + ".png");
        f.deleteFile();
        juce::FileOutputStream out (f);
        juce::PNGImageFormat().writeImageToStream (img, out);
        std::cout << "wrote " << f.getFullPathName() << " (" << ed->getWidth() << "x" << ed->getHeight() << ")" << std::endl;
    }
    return 0;
}
