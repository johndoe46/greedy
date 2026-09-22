// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>

class GreedySlotButton final : public juce::TextButton,
                               private juce::Timer
{
public:
    GreedySlotButton(GreedyAudioProcessor&, int slot);
    void mouseDown(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void refresh();

private:
    void timerCallback() override;
    GreedyAudioProcessor& processor;
    const int slot;
    bool longPressTriggered = false;
    bool displayedStored = true;
    bool displayedActive = false;
};

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
    juce::Component controls;
    std::array<std::unique_ptr<GreedySlotButton>, GreedyAudioProcessor::slotCount> slotButtons;
    juce::ToggleButton midiRecallToggle;
    std::array<juce::Slider, 6> knobs;
    std::array<juce::Label, 6> labels;
    std::array<juce::ComboBox, 3> noteSelectors;
    std::array<juce::Label, 3> noteLabels;
    std::array<juce::Slider, 2> velocitySliders;
    std::array<juce::Label, 2> velocityLabels;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 6> knobAttachments;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>, 3> noteAttachments;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 2> velocityAttachments;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> midiRecallAttachment;
    GreedyAudioProcessor::PatternDisplay displayedPattern;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GreedyAudioProcessorEditor)
};
