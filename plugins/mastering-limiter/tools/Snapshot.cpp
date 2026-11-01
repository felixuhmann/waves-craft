// Renders the editor to PNG (development aid, not shipped).
// Usage: mastering_limiter_snapshot <output directory>
#include <iostream>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "plugin/PluginProcessor.h"

int main (int argc, char** argv)
{
    using namespace craft::mastering_limiter;
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File outDir (argc > 1 ? juce::String (argv[1]) : juce::File::getCurrentWorkingDirectory().getFullPathName());
    outDir.createDirectory();

    auto set = [] (juce::AudioProcessorValueTreeState& s, const char* id, float physical) {
        if (auto* p = s.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (physical));
    };
    struct Case
    {
        const char* name;
        std::function<void (MasteringLimiterProcessor&)> setup;
        int blocks;
        bool mono;
    };
    const Case cases[] = {
        { "mastering-limiter-default", [] (auto&) {}, 0, false },
        { "mastering-limiter-set",
          [&] (auto& p) {
              auto& s = p.getState();
              set (s, ids::threshold, -12.0f);
              set (s, ids::ceiling, -0.3f);
              set (s, ids::adaptive, 0.0f);
              set (s, ids::release, 31.6f);
              set (s, ids::wordLength, 4.0f);
              set (s, ids::dither, 2.0f);
              set (s, ids::shaping, 3.0f);
              s.state.setProperty (linkMarkersProperty, true, nullptr);
          },
          60, false },
        { "mastering-limiter-mono", [&] (auto& p) { set (p.getState(), ids::threshold, -6.0f); }, 30, true },
    };

    for (const auto& c : cases)
    {
        MasteringLimiterProcessor proc;
        if (c.mono)
        {
            auto layout = proc.getBusesLayout();
            layout.inputBuses.getReference (0) = juce::AudioChannelSet::mono();
            layout.outputBuses.getReference (0) = juce::AudioChannelSet::mono();
            proc.setBusesLayout (layout);
        }
        c.setup (proc);
        proc.setRateAndBufferSizeDetails (48000.0, 512);
        proc.prepareToPlay (48000.0, 512);
        std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
        ed->setVisible (true);
        juce::AudioBuffer<float> buf (2, 512);
        juce::MidiBuffer midi;
        for (int b = 0; b < c.blocks; ++b)
        {
            for (int i = 0; i < 512; ++i)
            {
                const float t = float (b * 512 + i);
                buf.setSample (0, i, 0.9f * std::sin (t * 0.031f) * (0.6f + 0.4f * std::sin (t * 0.0007f)));
                buf.setSample (1, i, 0.7f * std::sin (t * 0.017f));
            }
            proc.processBlock (buf, midi);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (5);
        }
        juce::MessageManager::getInstance()->runDispatchLoopUntil (150);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
        auto f = outDir.getChildFile (juce::String (c.name) + ".png");
        f.deleteFile();
        juce::FileOutputStream out (f);
        juce::PNGImageFormat().writeImageToStream (img, out);
        std::cout << "wrote " << f.getFullPathName() << " (" << ed->getWidth() << "x" << ed->getHeight() << ")" << std::endl;
    }
    return 0;
}
