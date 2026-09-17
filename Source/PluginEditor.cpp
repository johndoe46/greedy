// SPDX-License-Identifier: GPL-3.0-or-later
#include "PluginEditor.h"

namespace
{
const juce::Colour background { 0xff151b23 };
const juce::Colour panel { 0xff202934 };
const juce::Colour ink { 0xffecf1f5 };
const juce::Colour muted { 0xff8b9cac };
const std::array<juce::Colour, 6> accents {
    juce::Colour(0xffffa760), juce::Colour(0xfff17d99), juce::Colour(0xfff4d878),
    juce::Colour(0xff6dcac1), juce::Colour(0xff6dcac1), juce::Colour(0xffa79aef)
};
}

GreedyLookAndFeel::GreedyLookAndFeel()
{
    setColour(juce::Slider::textBoxTextColourId, ink);
    setColour(juce::Slider::textBoxBackgroundColourId, background);
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::ComboBox::backgroundColourId, background);
    setColour(juce::ComboBox::textColourId, ink);
    setColour(juce::ComboBox::outlineColourId, muted.withAlpha(0.3f));
    setColour(juce::PopupMenu::backgroundColourId, panel);
    setColour(juce::PopupMenu::textColourId, ink);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xff365a64));
}

void GreedyLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                       float position, float start, float end, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y),
                                               static_cast<float>(width), static_cast<float>(height)).reduced(12.0f);
    const auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const auto angle = start + position * (end - start);
    juce::Path track;
    track.addCentredArc(centre.x, centre.y, radius, radius, 0.0f, start, end, true);
    g.setColour(muted.withAlpha(0.2f));
    g.strokePath(track, juce::PathStrokeType(4.0f));
    juce::Path value;
    value.addCentredArc(centre.x, centre.y, radius, radius, 0.0f, start, angle, true);
    g.setColour(slider.findColour(juce::Slider::rotarySliderFillColourId));
    g.strokePath(value, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved,
                                          juce::PathStrokeType::rounded));
    g.setColour(background);
    g.fillEllipse(centre.x - radius + 9, centre.y - radius + 9,
                  (radius - 9) * 2, (radius - 9) * 2);
    juce::Path pointer;
    pointer.addRoundedRectangle(-2.0f, -radius + 17.0f, 4.0f, radius * 0.37f, 2.0f);
    g.setColour(ink);
    g.fillPath(pointer, juce::AffineTransform::rotation(angle).translated(centre.x, centre.y));
}

GreedyAudioProcessorEditor::GreedyAudioProcessorEditor(GreedyAudioProcessor& p)
    : AudioProcessorEditor(p), processor(p)
{
    setLookAndFeel(&lookAndFeel);
    const std::array<const char*, 6> ids { "kickDensity", "snareDensity", "hatDensity", "mapX", "mapY", "chaos" };
    const std::array<const char*, 6> names { "KICK", "SNARE", "HI-HAT", "MAP X", "MAP Y", "CHAOS" };
    const std::array<const char*, 6> tips {
        "Kick density. Double-click to reset to 50%.",
        "Snare density. Double-click to reset to 50%.",
        "Hi-hat density. Double-click to reset to 50%.",
        "Horizontal position in the Grids drum map.",
        "Vertical position in the Grids drum map.",
        "Per-pattern variation in the drum hit thresholds."
    };
    for (std::size_t i = 0; i < knobs.size(); ++i)
    {
        knobs[i].setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        knobs[i].setTextBoxStyle(juce::Slider::TextBoxBelow, false, 76, 24);
        knobs[i].setTextValueSuffix(" %");
        knobs[i].setNumDecimalPlacesToDisplay(1);
        knobs[i].setDoubleClickReturnValue(true, i == 5 ? 0.0 : 50.0);
        knobs[i].setColour(juce::Slider::rotarySliderFillColourId, accents[i]);
        knobs[i].setTooltip(tips[i]);
        knobs[i].setName(names[i]);
        addAndMakeVisible(knobs[i]);
        labels[i].setText(names[i], juce::dontSendNotification);
        labels[i].setJustificationType(juce::Justification::centred);
        labels[i].setColour(juce::Label::textColourId, accents[i]);
        labels[i].setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::bold)));
        addAndMakeVisible(labels[i]);
        knobAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processor.parameters, ids[i], knobs[i]);
    }

    const std::array<const char*, 3> noteIds { "kickNote", "snareNote", "hatNote" };
    for (std::size_t i = 0; i < noteSelectors.size(); ++i)
    {
        for (int note = 0; note < 128; ++note)
            noteSelectors[i].addItem(juce::String(note) + "  "
                + juce::MidiMessage::getMidiNoteName(note, true, true, 3), note + 1);
        noteSelectors[i].setJustificationType(juce::Justification::centred);
        noteSelectors[i].setName(juce::String(names[i]) + " MIDI note");
        noteSelectors[i].setTooltip("MIDI note number (0-127), sent on channel 10.");
        addAndMakeVisible(noteSelectors[i]);
        noteLabels[i].setText("MIDI NOTE", juce::dontSendNotification);
        noteLabels[i].setJustificationType(juce::Justification::centred);
        noteLabels[i].setColour(juce::Label::textColourId, muted);
        noteLabels[i].setFont(juce::Font(juce::FontOptions(10.0f)));
        addAndMakeVisible(noteLabels[i]);
        noteAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            processor.parameters, noteIds[i], noteSelectors[i]);
    }
    displayedPattern = processor.getPatternDisplay();
    setSize(640, 670);
    startTimerHz(30);
}

GreedyAudioProcessorEditor::~GreedyAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void GreedyAudioProcessorEditor::resized()
{
    for (int column = 0; column < 3; ++column)
    {
        const auto i = static_cast<std::size_t>(column);
        const auto x = 30 + column * 196;
        labels[i].setBounds(x, 184, 188, 24);
        knobs[i].setBounds(x + 22, 208, 144, 154);
        noteLabels[i].setBounds(x, 370, 188, 16);
        noteSelectors[i].setBounds(x + 34, 390, 120, 28);
        labels[i + 3].setBounds(x, 444, 188, 24);
        knobs[i + 3].setBounds(x + 22, 468, 144, 154);
    }
}

void GreedyAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(background);
    g.setColour(panel);
    g.fillRoundedRectangle(24.0f, 62.0f, 592.0f, 104.0f, 14.0f);
    g.fillRoundedRectangle(24.0f, 176.0f, 592.0f, 254.0f, 14.0f);
    g.fillRoundedRectangle(24.0f, 436.0f, 592.0f, 194.0f, 14.0f);
    g.setColour(ink);
    g.setFont(juce::Font(juce::FontOptions(30.0f, juce::Font::bold)));
    g.drawText("greedy", 30, 16, 200, 40, juce::Justification::centredLeft);
    g.setFont(juce::Font(juce::FontOptions(12.0f)));
    g.setColour(muted);
    g.drawText("TOPOGRAPHIC MIDI SEQUENCER", 240, 27, 370, 22, juce::Justification::centredRight);
    const std::array<const char*, 3> laneNames { "KICK", "SNARE", "HI-HAT" };
    g.setFont(juce::Font(juce::FontOptions(9.0f)));
    for (int step = 0; step < 32; ++step)
    {
        g.setColour(step % 8 == 0 ? ink : muted);
        g.drawText(juce::String(step + 1), 126 + step * 15, 67, 15, 14,
                   juce::Justification::centred);
    }
    for (std::size_t part = 0; part < laneNames.size(); ++part)
    {
        const auto y = 84 + static_cast<int>(part) * 24;
        g.setColour(accents[part]);
        g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
        g.drawText(juce::String(laneNames[part]) + " " + juce::String(displayedPattern.notes[part]),
                   32, y, 88, 18, juce::Justification::centredLeft);
        for (int step = 0; step < 32; ++step)
        {
            const auto velocity = displayedPattern.velocities[part][static_cast<std::size_t>(step)];
            const juce::Rectangle<float> cell { 128.0f + static_cast<float>(step) * 15.0f,
                                               static_cast<float>(y), 11.0f, 18.0f };
            g.setColour(velocity > 0 ? accents[part].withAlpha(velocity > 90 ? 1.0f : 0.65f)
                                    : muted.withAlpha(step % 8 == 0 ? 0.25f : 0.10f));
            g.fillRoundedRectangle(cell, 2.0f);
            if (step == displayedPattern.currentStep)
            {
                g.setColour(ink);
                g.drawRoundedRectangle(cell.expanded(1.0f), 3.0f, 1.5f);
            }
        }
    }
    g.setColour(muted);
    g.setFont(juce::Font(juce::FontOptions(11.0f)));
    g.drawText("32 STEPS / 4 BEATS", 30, 638, 220, 20, juce::Justification::centredLeft);
    g.drawText(displayedPattern.currentStep < 0 ? "HOST STOPPED / MIDI CH 10" : "HOST SYNC / MIDI CH 10",
               270, 638, 340, 20, juce::Justification::centredRight);
}

void GreedyAudioProcessorEditor::timerCallback()
{
    const auto pattern = processor.getPatternDisplay();
    if (displayedPattern.currentStep != pattern.currentStep
        || displayedPattern.velocities != pattern.velocities
        || displayedPattern.notes != pattern.notes)
    {
        displayedPattern = pattern;
        repaint();
    }
}
