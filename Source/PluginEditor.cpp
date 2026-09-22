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
constexpr int baseWidth = 744;
constexpr int baseHeight = 790;
constexpr int mainOffset = 104;

juce::AffineTransform contentTransform(int width, int height)
{
    const auto scale = juce::jmin(static_cast<float>(width) / baseWidth,
                                  static_cast<float>(height) / baseHeight);
    const auto x = (static_cast<float>(width) - baseWidth * scale) * 0.5f;
    const auto y = (static_cast<float>(height) - baseHeight * scale) * 0.5f;
    return juce::AffineTransform::scale(scale).translated(x, y);
}
}

GreedySlotButton::GreedySlotButton(GreedyAudioProcessor& p, int slotIndex)
    : processor(p), slot(slotIndex)
{
    const auto note = GreedyAudioProcessor::slotMidiNote(slot);
    setButtonText(juce::String(slot + 1).paddedLeft('0', 2) + "  "
        + juce::MidiMessage::getMidiNoteName(note, true, true, 3));
    setName("SLOT " + juce::String(slot + 1));
    setTooltip("Short press: recall. Hold for 600 ms: store current settings. MIDI note "
        + juce::String(note) + " recalls this slot.");
    onClick = [this]
    {
        if (!longPressTriggered)
            processor.recallSlot(slot);
        refresh();
    };
    refresh();
}

void GreedySlotButton::mouseDown(const juce::MouseEvent& event)
{
    longPressTriggered = false;
    startTimer(600);
    juce::TextButton::mouseDown(event);
}

void GreedySlotButton::mouseUp(const juce::MouseEvent& event)
{
    stopTimer();
    juce::TextButton::mouseUp(event);
    longPressTriggered = false;
}

void GreedySlotButton::timerCallback()
{
    stopTimer();
    if (getState() == juce::Button::buttonDown)
    {
        longPressTriggered = true;
        processor.storeSlot(slot);
        refresh();
    }
}

void GreedySlotButton::refresh()
{
    const auto stored = processor.isSlotStored(slot);
    const auto active = processor.getCurrentSlot() == slot;
    if (stored == displayedStored && active == displayedActive)
        return;
    displayedStored = stored;
    displayedActive = active;
    const auto colour = active ? juce::Colour(0xff365a64)
                               : stored ? juce::Colour(0xff293744) : background;
    setColour(juce::TextButton::buttonColourId, colour);
    setColour(juce::TextButton::buttonOnColourId, colour.brighter(0.08f));
    setColour(juce::TextButton::textColourOffId, active ? accents[3] : stored ? ink : muted);
    setColour(juce::TextButton::textColourOnId, ink);
    repaint();
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
    addAndMakeVisible(controls);
    for (int slot = 0; slot < GreedyAudioProcessor::slotCount; ++slot)
    {
        auto& button = slotButtons[static_cast<std::size_t>(slot)];
        button = std::make_unique<GreedySlotButton>(processor, slot);
        controls.addAndMakeVisible(*button);
    }
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
        controls.addAndMakeVisible(knobs[i]);
        labels[i].setText(names[i], juce::dontSendNotification);
        labels[i].setJustificationType(juce::Justification::centred);
        labels[i].setColour(juce::Label::textColourId, accents[i]);
        labels[i].setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::bold)));
        controls.addAndMakeVisible(labels[i]);
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
        controls.addAndMakeVisible(noteSelectors[i]);
        noteLabels[i].setText("MIDI NOTE", juce::dontSendNotification);
        noteLabels[i].setJustificationType(juce::Justification::centred);
        noteLabels[i].setColour(juce::Label::textColourId, muted);
        noteLabels[i].setFont(juce::Font(juce::FontOptions(10.0f)));
        controls.addAndMakeVisible(noteLabels[i]);
        noteAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            processor.parameters, noteIds[i], noteSelectors[i]);
    }
    const std::array<const char*, 2> velocityIds { "normalVelocity", "accentVelocity" };
    const std::array<const char*, 2> velocityNames { "NORMAL VELOCITY", "ACCENT VELOCITY" };
    for (std::size_t i = 0; i < velocitySliders.size(); ++i)
    {
        velocitySliders[i].setSliderStyle(juce::Slider::LinearHorizontal);
        velocitySliders[i].setTextBoxStyle(juce::Slider::TextBoxRight, false, 48, 24);
        velocitySliders[i].setRange(1, 127, 1);
        velocitySliders[i].setDoubleClickReturnValue(true, i == 0 ? 90.0 : 120.0);
        velocitySliders[i].setColour(juce::Slider::trackColourId, accents[i == 0 ? 3 : 5]);
        velocitySliders[i].setName(velocityNames[i]);
        velocitySliders[i].setTooltip("MIDI note-on velocity (1-127). Double-click to reset.");
        controls.addAndMakeVisible(velocitySliders[i]);
        velocityLabels[i].setText(velocityNames[i], juce::dontSendNotification);
        velocityLabels[i].setJustificationType(juce::Justification::centredLeft);
        velocityLabels[i].setColour(juce::Label::textColourId, accents[i == 0 ? 3 : 5]);
        velocityLabels[i].setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
        controls.addAndMakeVisible(velocityLabels[i]);
        velocityAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processor.parameters, velocityIds[i], velocitySliders[i]);
    }
    displayedPattern = processor.getPatternDisplay();
    setSize(baseWidth, baseHeight);
    setResizable(true, true);
    setResizeLimits(558, 593, 1488, 1580);
    getConstrainer()->setFixedAspectRatio(static_cast<double>(baseWidth) / baseHeight);
    startTimerHz(30);
}

GreedyAudioProcessorEditor::~GreedyAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void GreedyAudioProcessorEditor::resized()
{
    controls.setTransform({});
    controls.setBounds(0, 0, baseWidth, baseHeight);
    controls.setTransform(contentTransform(getWidth(), getHeight()));
    for (int slot = 0; slot < GreedyAudioProcessor::slotCount; ++slot)
        slotButtons[static_cast<std::size_t>(slot)]->setBounds(28, 62 + slot * 54, 72, 44);
    for (int column = 0; column < 3; ++column)
    {
        const auto i = static_cast<std::size_t>(column);
        const auto x = mainOffset + 30 + column * 196;
        labels[i].setBounds(x, 184, 188, 24);
        knobs[i].setBounds(x + 22, 208, 144, 154);
        noteLabels[i].setBounds(x, 370, 188, 16);
        noteSelectors[i].setBounds(x + 34, 390, 120, 28);
        labels[i + 3].setBounds(x, 444, 188, 24);
        knobs[i + 3].setBounds(x + 22, 468, 144, 154);
    }
    for (int column = 0; column < 2; ++column)
    {
        const auto i = static_cast<std::size_t>(column);
        const auto x = mainOffset + 40 + column * 288;
        velocityLabels[i].setBounds(x, 654, 264, 20);
        velocitySliders[i].setBounds(x, 684, 264, 36);
    }
}

void GreedyAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(background);
    g.addTransform(contentTransform(getWidth(), getHeight()));
    g.setColour(panel);
    g.fillRoundedRectangle(16.0f, 16.0f, 96.0f, 730.0f, 14.0f);
    g.setColour(ink);
    g.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
    g.drawText("SLOTS", 24, 27, 80, 20, juce::Justification::centred);
    g.setColour(muted);
    g.setFont(juce::Font(juce::FontOptions(8.5f)));
    g.drawFittedText("SHORT: RECALL\nHOLD: STORE", 23, 713, 82, 26,
                     juce::Justification::centred, 2);
    g.saveState();
    g.addTransform(juce::AffineTransform::translation(static_cast<float>(mainOffset), 0.0f));
    g.setColour(panel);
    g.fillRoundedRectangle(24.0f, 62.0f, 592.0f, 104.0f, 14.0f);
    g.fillRoundedRectangle(24.0f, 176.0f, 592.0f, 254.0f, 14.0f);
    g.fillRoundedRectangle(24.0f, 436.0f, 592.0f, 194.0f, 14.0f);
    g.fillRoundedRectangle(24.0f, 638.0f, 592.0f, 108.0f, 14.0f);
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
            g.setColour(velocity > 0 ? accents[part].withAlpha(
                                      displayedPattern.accents[part][static_cast<std::size_t>(step)] ? 1.0f : 0.65f)
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
    g.drawText("32 STEPS / 4 BEATS", 30, 758, 220, 20, juce::Justification::centredLeft);
    g.drawText(displayedPattern.currentStep < 0 ? "HOST STOPPED / MIDI CH 10" : "HOST SYNC / MIDI CH 10",
               270, 758, 340, 20, juce::Justification::centredRight);
    g.restoreState();
}

void GreedyAudioProcessorEditor::timerCallback()
{
    for (auto& button : slotButtons)
        button->refresh();
    const auto pattern = processor.getPatternDisplay();
    if (displayedPattern.currentStep != pattern.currentStep
        || displayedPattern.velocities != pattern.velocities
        || displayedPattern.accents != pattern.accents
        || displayedPattern.notes != pattern.notes)
    {
        displayedPattern = pattern;
        repaint();
    }
}
