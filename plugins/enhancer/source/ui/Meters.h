// Craft Enhancer - two-channel level meter with peak readouts (spec r1 section 9).
#pragma once

#include <array>

#include <juce_gui_basics/juce_gui_basics.h>

namespace craft::enhancer::ui
{

class StereoMeter : public juce::Component
{
public:
    void push (std::array<float, 2> peaks);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { hold = { -120.0f, -120.0f }; repaint(); }
    float getMaxHoldDb() const noexcept { return juce::jmax (hold[0], hold[1]); }

private:
    std::array<float, 2> level { -120.0f, -120.0f }, hold { -120.0f, -120.0f };
};

} // namespace craft::enhancer::ui
