// Renders the editor to PNG with some programme running through it (development aid, not shipped).
// Usage: multiband_snapshot <output directory>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "plugin/PluginProcessor.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File outDir (argc > 1 ? juce::String (argv[1]) : juce::File::getCurrentWorkingDirectory().getFullPathName());
    outDir.createDirectory();

    using craft::multiband::MultibandProcessor;
    namespace ids = craft::multiband::ids;
    using BP = ids::BandParam;
    auto set = [] (juce::AudioProcessorValueTreeState& s, const juce::String& id, float physical) {
        if (auto* p = s.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (physical));
    };

    for (int variant = 0; variant < 2; ++variant)
    {
        MultibandProcessor proc;
        auto& st = proc.getState();
        if (variant == 1)
        {
            set (st, ids::band (0, BP::gain), 3.0f);
            set (st, ids::band (1, BP::range), -14.0f);
            set (st, ids::band (2, BP::range), 6.0f);
            set (st, ids::band (3, BP::gain), -4.5f);
            set (st, ids::band (2, BP::solo), 1.0f);
            set (st, ids::knee, 0.75f);
            set (st, ids::opto, 1.0f);
            set (st, ids::outputGain, 2.0f);
        }
        proc.setRateAndBufferSizeDetails (48000.0, 512);
        proc.prepareToPlay (48000.0, 512);
        std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
        ed->setVisible (true);

        juce::Random rng (7);
        juce::AudioBuffer<float> buf (2, 512);
        juce::MidiBuffer midi;
        double ph = 0.0;
        for (int block = 0; block < 90; ++block)
        {
            for (int i = 0; i < 512; ++i)
            {
                const float n = (rng.nextFloat() * 2.0f - 1.0f) * 0.25f;
                const float tone = 0.4f * float (std::sin (ph));
                ph += 2.0 * 3.14159265358979 * 70.0 / 48000.0;
                buf.setSample (0, i, n + tone);
                buf.setSample (1, i, 0.8f * n + tone);
            }
            proc.processBlock (buf, midi);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (12);
        }
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.5f);
        auto f = outDir.getChildFile (juce::String ("multiband-") + (variant == 0 ? "default" : "edited") + ".png");
        f.deleteFile();
        juce::FileOutputStream out (f);
        juce::PNGImageFormat().writeImageToStream (img, out);
        std::cout << "wrote " << f.getFullPathName() << " (" << ed->getWidth() << "x" << ed->getHeight() << ")" << std::endl;
    }
    return 0;
}
