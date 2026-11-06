// Renders the editor to PNG with some programme running through it (development aid, not shipped).
// Usage: punch_compressor_snapshot <output directory>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "plugin/PluginProcessor.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File outDir (argc > 1 ? juce::String (argv[1]) : juce::File::getCurrentWorkingDirectory().getFullPathName());
    outDir.createDirectory();

    using craft::punch::PunchCompressorProcessor;
    namespace ids = craft::punch::ids;
    auto set = [] (juce::AudioProcessorValueTreeState& s, const char* id, float physical) {
        if (auto* p = s.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (physical));
    };
    auto setNorm = [] (juce::AudioProcessorValueTreeState& s, const char* id, float r) {
        if (auto* p = s.getParameter (id))
            p->setValueNotifyingHost (r);
    };

    struct Case
    {
        const char* name;
        float thr, ratio, sync, meter;
    };
    const Case cases[] = { { "default", -18.0f, 3.0f, 1.0f, 0.5f },
                           { "host-sync", -30.0f, 8.0f, 0.5f, 0.0f },
                           { "bpm-sync", -24.0f, 4.0f, 0.0f, 1.0f } };

    for (const auto& c : cases)
    {
        PunchCompressorProcessor proc;
        set (proc.getState(), ids::threshold, c.thr);
        set (proc.getState(), ids::ratio, c.ratio);
        setNorm (proc.getState(), ids::sync, c.sync);
        setNorm (proc.getState(), ids::meter, c.meter);

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
                    buf.setSample (ch, i, (rng.nextFloat() * 2.0f - 1.0f) * (ch == 0 ? 0.5f : 0.3f));
            proc.processBlock (buf, midi);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (12);
        }
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.5f);
        auto f = outDir.getChildFile (juce::String ("punch-compressor-") + c.name + ".png");
        f.deleteFile();
        juce::FileOutputStream out (f);
        juce::PNGImageFormat().writeImageToStream (img, out);
        std::cout << "wrote " << f.getFullPathName() << " (" << ed->getWidth() << "x" << ed->getHeight() << ")" << std::endl;
    }
    return 0;
}
