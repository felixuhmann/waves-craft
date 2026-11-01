// Renders the editor to PNG (development aid, not shipped).
// Usage: dynamic_distortion_snapshot <output directory>
#include <iostream>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "plugin/PluginProcessor.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File outDir (argc > 1 ? juce::String (argv[1]) : juce::File::getCurrentWorkingDirectory().getFullPathName());
    outDir.createDirectory();

    using craft::ddp::DynamicDistortionProcessor;
    namespace ids = craft::ddp::ids;
    auto set = [] (DynamicDistortionProcessor& p, const juce::String& id, float physical) {
        if (auto* prm = p.getState().getParameter (id))
            prm->setValueNotifyingHost (prm->convertTo0to1 (physical));
    };

    struct Case
    {
        const char* name;
        std::function<void (DynamicDistortionProcessor&)> setup;
    };
    const Case cases[] = {
        { "default", [] (auto&) {} },
        { "tone", [&] (auto& p) {
              set (p, ids::bass, 20.0f);
              set (p, ids::mid, -35.0f);
              set (p, ids::midFreq, 600.0f);
              set (p, ids::treble, 30.0f);
              set (p, ids::lowPass, 4000.0f);
              set (p, ids::drive, 80.0f);
              set (p, ids::direct, 40.0f);
          } },
    };

    for (const auto& c : cases)
    {
        DynamicDistortionProcessor proc;
        c.setup (proc);
        proc.setPlayConfigDetails (2, 2, 48000.0, 512);
        proc.prepareToPlay (48000.0, 512);
        juce::AudioBuffer<float> buf (2, 512);
        juce::MidiBuffer midi;
        for (int b = 0; b < 40; ++b)
        {
            for (int i = 0; i < 512; ++i)
            {
                const float v = 0.2f * std::sin (0.05f * float (b * 512 + i));
                buf.setSample (0, i, v);
                buf.setSample (1, i, v);
            }
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
