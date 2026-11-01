// Renders the editor to PNG (development aid, not shipped).
// Usage: motion_filter_snapshot <output directory>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "plugin/PluginProcessor.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File outDir (argc > 1 ? juce::String (argv[1]) : juce::File::getCurrentWorkingDirectory().getFullPathName());
    outDir.createDirectory();

    using craft::mfp::MotionFilterProcessor;
    namespace ids = craft::mfp::ids;
    auto set = [] (MotionFilterProcessor& p, const juce::String& id, float physical) {
        if (auto* prm = p.getState().getParameter (id))
            prm->setValueNotifyingHost (prm->convertTo0to1 (physical));
    };

    struct Case
    {
        const char* name;
        std::function<void (MotionFilterProcessor&)> setup;
    };
    const Case cases[] = {
        { "default", [] (auto&) {} },
        { "bandpass-seq", [&] (auto& p) {
              set (p, ids::type, 2.0f);
              set (p, ids::freq, 900.0f);
              set (p, ids::res, 70.0f);
              set (p, ids::slope, 1.0f);
              set (p, ids::spread, 30.0f);
              set (p, ids::mod (1, 0), 60.0f);
              set (p, ids::mod (0, 1), -25.0f);
              set (p, ids::lfoShape, 3.0f);
              set (p, ids::seqLast, 12.0f);
              for (int k = 0; k < 16; ++k)
                  set (p, ids::step (k), float (std::sin (k * 0.9) * 80.0));
              set (p, ids::dlyMix, -20.0f);
              set (p, ids::drive, 35.0f);
          } },
        { "comb", [&] (auto& p) {
              set (p, ids::type, 4.0f);
              set (p, ids::freq, 300.0f);
              set (p, ids::res, 80.0f);
              set (p, ids::lfoSync, 0.0f);
              set (p, ids::dlySync, 0.0f);
              set (p, ids::dlyRoute, 1.0f);
          } },
    };

    for (const auto& c : cases)
    {
        MotionFilterProcessor proc;
        c.setup (proc);
        proc.setPlayConfigDetails (2, 2, 48000.0, 512);
        proc.prepareToPlay (48000.0, 512);
        juce::AudioBuffer<float> buf (2, 512);
        juce::MidiBuffer midi;
        for (int b = 0; b < 40; ++b)
        {
            for (int i = 0; i < 512; ++i)
                buf.setSample (0, i, 0.2f * std::sin (0.05f * float (b * 512 + i))), buf.setSample (1, i, buf.getSample (0, i));
            proc.processBlock (buf, midi);
        }
        std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
        ed->setVisible (true);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (150);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.5f);
        auto f = outDir.getChildFile (juce::String (c.name) + ".png");
        f.deleteFile();
        juce::FileOutputStream out (f);
        juce::PNGImageFormat().writeImageToStream (img, out);
        std::cout << "wrote " << f.getFullPathName() << " (" << ed->getWidth() << "x" << ed->getHeight() << ")" << std::endl;
    }
    return 0;
}
