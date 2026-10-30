// Renders the editor to PNG with some programme running through it (development aid, not shipped).
// Usage: vocal_compressor_snapshot <output directory>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "plugin/PluginProcessor.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File outDir (argc > 1 ? juce::String (argv[1]) : juce::File::getCurrentWorkingDirectory().getFullPathName());
    outDir.createDirectory();

    using craft::vocal::VocalCompressorProcessor;
    namespace ids = craft::vocal::ids;
    auto set = [] (juce::AudioProcessorValueTreeState& s, const char* id, float physical) {
        if (auto* p = s.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (physical));
    };

    struct Case
    {
        const char* name;
        bool mono;
        float comp, gate, gain;
    };
    const Case cases[] = { { "default", false, 0.0f, -80.0f, 0.0f },
                           { "compressing", false, -18.0f, -50.0f, -3.0f },
                           { "mono", true, -30.0f, -66.0f, -12.0f } };

    for (const auto& c : cases)
    {
        VocalCompressorProcessor proc;
        if (c.mono)
        {
            auto layout = proc.getBusesLayout();
            layout.inputBuses.getReference (0) = juce::AudioChannelSet::mono();
            layout.outputBuses.getReference (0) = juce::AudioChannelSet::mono();
            proc.setBusesLayout (layout);
        }
        set (proc.getState(), ids::compression, c.comp);
        set (proc.getState(), ids::gate, c.gate);
        set (proc.getState(), ids::gain, c.gain);

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
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
        auto f = outDir.getChildFile (juce::String ("vocal-compressor-") + c.name + ".png");
        f.deleteFile();
        juce::FileOutputStream out (f);
        juce::PNGImageFormat().writeImageToStream (img, out);
        std::cout << "wrote " << f.getFullPathName() << " (" << ed->getWidth() << "x" << ed->getHeight() << ")" << std::endl;
    }
    return 0;
}
