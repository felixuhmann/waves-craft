// Craft Dynamics - editor (spec r1 section 12; all artwork drawn in code).
#pragma once

#include <map>
#include <memory>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>

#include "Controls.h"
#include "DynamicsLookAndFeel.h"
#include "FilterGraph.h"
#include "Meters.h"
#include "TransferGraph.h"
#include "plugin/PluginProcessor.h"

namespace craft::dynamics
{

class DynamicsEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit DynamicsEditor (DynamicsProcessor&);
    ~DynamicsEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Panel
    {
        juce::Rectangle<int> bounds;
        juce::String title;
        juce::Colour colour;
    };

    void timerCallback() override;
    ui::ValueBox* box (const char* id, juce::Colour c, const juce::String& tooltip);
    ui::SegmentedChoice* segments (const char* id, juce::StringArray labels, juce::Colour c, const juce::String& tooltip);
    ui::ParamToggle* toggle (const char* id, const juce::String& text, juce::Colour c, const juce::String& tooltip,
                             ui::ParamToggle::Icon icon = ui::ParamToggle::Icon::None);

    // layout helpers: a caption above a control
    juce::Rectangle<int> captioned (juce::Rectangle<int> area, const juce::String& caption);
    void layoutComp (juce::Rectangle<int> panel);
    void layoutGate (juce::Rectangle<int> panel);
    void layoutFilter (juce::Rectangle<int> panel);
    void layoutGlobal (juce::Rectangle<int> panel);
    void layoutOutput (juce::Rectangle<int> panel);
    static juce::Rectangle<int> content (juce::Rectangle<int> panel);

    DynamicsProcessor& proc;
    const dsp::VariantInfo info;
    ui::CraftLookAndFeel lnf;

    std::vector<std::unique_ptr<ui::ValueBox>> boxes;
    std::vector<std::unique_ptr<juce::Component>> others;
    std::map<juce::String, juce::Component*> byId;

    std::unique_ptr<ui::TransferGraph> transfer;
    std::unique_ptr<ui::FilterGraph> filterGraph;
    std::unique_ptr<ui::ModuleMeters> compMeters, gateMeters;
    std::unique_ptr<ui::IoMeters> ioMeters;
    std::unique_ptr<ui::Fader> outFader;
    juce::TooltipWindow tooltips { this, 600 };

    std::vector<Panel> panels;
    std::vector<std::pair<juce::Rectangle<int>, juce::String>> captions;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DynamicsEditor)
};

} // namespace craft::dynamics
