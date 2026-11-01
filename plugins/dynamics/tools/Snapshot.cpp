// Renders the editor of each variant to PNG (development aid, not shipped).
// Usage: dynamics_snapshot <output directory>
#include <functional>
#include <iostream>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "plugin/PluginProcessor.h"

int main (int argc, char** argv)
{
    using namespace craft::dynamics;
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File outDir (argc > 1 ? juce::String (argv[1]) : juce::File::getCurrentWorkingDirectory().getFullPathName());
    outDir.createDirectory();

    auto set = [] (juce::AudioProcessorValueTreeState& s, const char* id, float physical) {
        if (auto* p = s.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (physical));
    };
    struct Case
    {
        dsp::Variant v;
        const char* name;
        std::function<void (juce::AudioProcessorValueTreeState&)> setup;
    };
    const Case cases[] = {
        { dsp::Variant::Full, "dynamics-default", [] (auto&) {} },
        { dsp::Variant::Full, "dynamics-set",
          [&] (auto& s) {
              set (s, ids::compThreshold, -30.0f);
              set (s, ids::ratio, 0.5f);
              set (s, ids::makeup, 6.0f);
              set (s, ids::gateOpen, -55.0f);
              set (s, ids::gateClose, -62.0f);
              set (s, ids::floor, -30.0f);
              set (s, ids::compEq, 2.0f);
              set (s, ids::filterType, 2.0f);
              set (s, ids::filterFreq, 2500.0f);
              set (s, ids::filterQ, 0.4f);
              set (s, ids::linkAttack, 1.0f);
              set (s, ids::lookahead, 1.0f);
          } },
        { dsp::Variant::Compressor, "dynamics-compressor", [&] (auto& s) { set (s, ids::compThreshold, -24.0f); set (s, ids::ratio, 0.9f); } },
        { dsp::Variant::Gate, "dynamics-gate", [&] (auto& s) { set (s, ids::gateOpen, -40.0f); set (s, ids::gateMode, 1.0f); set (s, ids::floor, -20.0f); } },
        { dsp::Variant::CompressorFilter, "dynamics-compressor-sc", [&] (auto& s) { set (s, ids::compThreshold, -20.0f); set (s, ids::ratio, 0.1f); set (s, ids::compEq, 1.0f); } },
    };

    for (const auto& c : cases)
    {
        DynamicsProcessor proc (c.v);
        c.setup (proc.getState());
        proc.setRateAndBufferSizeDetails (48000.0, 512);
        proc.prepareToPlay (48000.0, 512);
        juce::AudioBuffer<float> buf (2, 512);
        juce::MidiBuffer midi;
        for (int b = 0; b < 60; ++b)
        {
            for (int i = 0; i < 512; ++i)
            {
                const float v = 0.3f * std::sin (float (b * 512 + i) * 0.13f);
                buf.setSample (0, i, v);
                buf.setSample (1, i, 0.8f * v);
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
