// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>

class GreedyLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    GreedyLookAndFeel();
    void drawRotarySlider(juce::Graphics&, int x, int y, int width, int height,
                          float position, float start, float end, juce::Slider&) override;
};

class GreedyAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                        private juce::Timer
{
public:
    explicit GreedyAudioProcessorEditor(GreedyAudioProcessor&);
    ~GreedyAudioProcessorEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    GreedyAudioProcessor& processor;
    GreedyLookAndFeel lookAndFeel;
    juce::TooltipWindow tooltips { this, 600 };
    std::array<juce::Slider, 6> knobs;
    std::array<juce::Label, 6> labels;
    std::array<juce::ComboBox, 3> noteSelectors;
    std::array<juce::Label, 3> noteLabels;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 6> knobAttachments;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>, 3> noteAttachments;
    GreedyAudioProcessor::PatternDisplay displayedPattern;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GreedyAudioProcessorEditor)
};
