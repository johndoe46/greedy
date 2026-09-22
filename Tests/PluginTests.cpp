// SPDX-License-Identifier: GPL-3.0-or-later
#include "PluginProcessor.h"
#include <cstdlib>
#include <cmath>
#include <functional>
#include <iostream>

namespace
{
void require(bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

class TestPlayHead final : public juce::AudioPlayHead
{
public:
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo info;
        info.setBpm(120.0);
        info.setPpqPosition(ppq);
        info.setIsPlaying(playing);
        return info;
    }
    double ppq = 0;
    bool playing = true;
};

void setParameter(GreedyAudioProcessor& processor, const char* id, float value)
{
    auto* parameter = processor.parameters.getParameter(id);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
}

int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI initialiseJuce;
    GreedyAudioProcessor processor;
    require(processor.isMidiEffect() && processor.producesMidi() && processor.acceptsMidi(),
            "plugin declares MIDI input for slot recall and generated MIDI output");
    require(processor.getTotalNumInputChannels() == 0 && processor.getTotalNumOutputChannels() == 0,
            "plugin exposes no audio buses");
    require(processor.getParameters().size() == 11,
            "knobs, note selectors, and velocity levels are parameters");

    setParameter(processor, "kickDensity", 100);
    setParameter(processor, "snareDensity", 0);
    setParameter(processor, "hatDensity", 0);
    setParameter(processor, "mapX", 0);
    setParameter(processor, "mapY", 0);
    setParameter(processor, "kickNote", 60);
    setParameter(processor, "normalVelocity", 47);
    setParameter(processor, "accentVelocity", 107);
    processor.storeSlot(0);
    require(processor.isSlotStored(0) && processor.getCurrentSlot() == 0,
            "storing captures a slot and marks it active");
    processor.prepareToPlay(48000, 512);
    TestPlayHead host;
    processor.setPlayHead(&host);
    juce::AudioBuffer<float> audio(0, 128);
    juce::MidiBuffer midi;
    // Input MIDI must be discarded rather than mixed into the generated output.
    midi.addEvent(juce::MidiMessage::noteOn(1, 99, static_cast<juce::uint8>(100)), 0);
    processor.processBlock(audio, midi);
    require(midi.getNumEvents() == 1, "only the selected kick generates a note");
    const auto first = (*midi.begin()).getMessage();
    require(first.isNoteOn() && first.getNoteNumber() == 60 && first.getChannel() == 10,
            "note selection and MIDI channel reach actual JUCE output");
    require(first.getVelocity() == 47, "configured normal velocity reaches actual JUCE output");

    host.playing = false;
    processor.processBlock(audio, midi);
    require(midi.getNumEvents() == 1 && (*midi.begin()).getMessage().isNoteOff(),
            "host stop releases the actual MIDI note");
    processor.processBlock(audio, midi);
    require(midi.isEmpty(), "stopped processor stays silent");
    require(processor.getPatternDisplay().currentStep == -1,
            "stopped pattern has no playback marker");

    juce::MemoryBlock state;
    processor.getStateInformation(state);
    GreedyAudioProcessor restored;
    restored.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    for (auto* parameter : processor.getParameters())
    {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(parameter);
        require(ranged != nullptr, "parameters are ranged");
        require(std::abs(restored.parameters.getParameter(ranged->getParameterID())->getValue()
                - ranged->getValue()) < 1.0e-6f, "state round-trip restores each parameter");
    }
    require(restored.isSlotStored(0) && restored.getCurrentSlot() == 0,
            "state round-trip restores stored slots and the active slot");
    setParameter(restored, "normalVelocity", 88);
    restored.recallSlot(0);
    require(std::abs(restored.parameters.getRawParameterValue("normalVelocity")->load() - 47.0f)
            < 0.01f, "recalling a slot restores its parameter values");

    host.playing = true;
    processor.processBlock(audio, midi);
    require(!midi.isEmpty(), "restart generates a fresh note");
    processor.processBlockBypassed(audio, midi);
    require(midi.getNumEvents() == 1 && (*midi.begin()).getMessage().isNoteOff(),
            "bypass releases held MIDI notes");

    processor.setPlayHead(nullptr);
    processor.processBlock(audio, midi);
    require(midi.isEmpty(), "missing host transport produces no notes");

    setParameter(processor, "kickDensity", 47);
    setParameter(processor, "snareDensity", 67);
    setParameter(processor, "hatDensity", 83);
    setParameter(processor, "mapX", 73);
    setParameter(processor, "mapY", 29);
    setParameter(processor, "chaos", 100);
    setParameter(processor, "kickNote", 36);
    setParameter(processor, "normalVelocity", 115);
    setParameter(processor, "accentVelocity", 35);
    processor.setPlayHead(&host);
    juce::AudioBuffer<float> stepAudio(0, 3000); // One step at 120 BPM / 48 kHz.
    bool sawNormal = false;
    bool sawAccent = false;
    for (const auto bar : { -2, 3, 7 })
    {
        for (int step = 0; step < 32; ++step)
        {
            host.ppq = bar * 4.0 + step / 8.0;
            midi.clear();
            processor.processBlock(stepAudio, midi);
            const auto display = processor.getPatternDisplay();
            require(display.currentStep == step, "pattern marker follows transport, including seeks");
            std::array<int, 3> emitted {};
            for (const auto metadata : midi)
            {
                const auto message = metadata.getMessage();
                if (message.isNoteOn())
                    for (std::size_t part = 0; part < display.notes.size(); ++part)
                        if (message.getNoteNumber() == display.notes[part])
                            emitted[part] = message.getVelocity();
            }
            for (std::size_t part = 0; part < emitted.size(); ++part)
            {
                require(emitted[part] == display.velocities[part][static_cast<std::size_t>(step)],
                        "each displayed drum step and accent matches generated MIDI with chaos");
                if (emitted[part] != 0)
                {
                    sawAccent |= display.accents[part][static_cast<std::size_t>(step)]
                                 && emitted[part] == 35;
                    sawNormal |= !display.accents[part][static_cast<std::size_t>(step)]
                                 && emitted[part] == 115;
                }
            }
        }
    }
    require(sawNormal && sawAccent, "configured normal and accent velocities both reach MIDI output");
    processor.processBlockBypassed(audio, midi);
    require(processor.getPatternDisplay().currentStep == -1, "bypass removes playback marker");
    processor.setPlayHead(nullptr);
    setParameter(processor, "snareNote", 65);
    setParameter(processor, "hatDensity", 0);
    const auto stoppedDisplay = processor.getPatternDisplay();
    require(stoppedDisplay.notes[1] == 65, "lane note mapping updates while stopped");
    for (const auto velocity : stoppedDisplay.velocities[2])
        require(velocity == 0, "zero density clears the corresponding preview lane while stopped");
    setParameter(processor, "hatDensity", 83);

    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
    require(editor->getWidth() == 744 && editor->getHeight() == 790,
            "editor has its intended dimensions");
    int knobs = 0;
    int selectors = 0;
    int slotButtonCount = 0;
    juce::Slider* normalSlider = nullptr;
    juce::Slider* accentSlider = nullptr;
    std::function<void(juce::Component&)> inspectControls = [&](juce::Component& component)
    {
        for (int i = 0; i < component.getNumChildComponents(); ++i)
        {
            auto* child = component.getChildComponent(i);
            if (auto* slider = dynamic_cast<juce::Slider*>(child))
            {
                ++knobs;
                if (slider->getName() == "NORMAL VELOCITY")
                    normalSlider = slider;
                if (slider->getName() == "ACCENT VELOCITY")
                    accentSlider = slider;
            }
            selectors += dynamic_cast<juce::ComboBox*>(child) != nullptr ? 1 : 0;
            if (child->getName().startsWith("SLOT "))
                ++slotButtonCount;
            inspectControls(*child);
        }
    };
    inspectControls(*editor);
    require(knobs == 8 && selectors == 3,
            "editor has six pattern knobs, two velocity sliders, and three note selectors");
    require(slotButtonCount == 12, "editor has twelve settings slot buttons");
    require(normalSlider != nullptr && accentSlider != nullptr
            && std::abs(normalSlider->getValue() - 115.0) < 0.01
            && std::abs(accentSlider->getValue() - 35.0) < 0.01,
            "velocity sliders show their saved parameter values");
    normalSlider->setValue(72, juce::sendNotificationSync);
    require(std::abs(processor.parameters.getRawParameterValue("normalVelocity")->load() - 72.0f)
            < 0.01f,
            "moving the normal velocity slider updates the plugin parameter");
    normalSlider->setValue(115, juce::sendNotificationSync);
    require(editor->isResizable() && editor->getConstrainer() != nullptr
            && std::abs(editor->getConstrainer()->getFixedAspectRatio() - 744.0 / 790.0) < 1.0e-6,
            "editor advertises proportional resizing to the host");
    editor->setSize(1116, 1185);
    require(editor->getWidth() == 1116 && editor->getHeight() == 1185,
            "editor accepts a larger proportional size");
    const auto scaledSliderBounds = editor->getLocalArea(normalSlider, normalSlider->getLocalBounds());
    require(std::abs(scaledSliderBounds.getWidth() - 396) <= 1,
            "controls scale with the resized editor");
    editor->setSize(744, 790);
    if (argc == 2)
    {
        juce::Image preview(juce::Image::RGB, editor->getWidth(), editor->getHeight(), true);
        juce::Graphics graphics(preview);
        editor->paintEntireComponent(graphics, true);
        juce::FileOutputStream stream { juce::File(juce::String(argv[1])) };
        require(stream.openedOk() && stream.setPosition(0) && stream.truncate().wasOk()
                && juce::PNGImageFormat().writeImageToStream(preview, stream),
                "editor preview can be saved");
    }

    // Slot note-ons are consumed and apply the stored settings to this block.
    editor.reset();
    setParameter(processor, "kickNote", 77);
    setParameter(processor, "normalVelocity", 99);
    processor.prepareToPlay(48000, 512);
    processor.setPlayHead(&host);
    host.playing = true;
    host.ppq = 0.0;
    midi.addEvent(juce::MidiMessage::noteOn(1, GreedyAudioProcessor::slotMidiNote(0),
                                           static_cast<juce::uint8>(100)), 0);
    processor.processBlock(audio, midi);
    require(midi.getNumEvents() == 1, "slot trigger note is consumed rather than passed through");
    const auto recalledHit = (*midi.begin()).getMessage();
    require(recalledHit.isNoteOn() && recalledHit.getNoteNumber() == 60
            && recalledHit.getVelocity() == 47,
            "MIDI slot recall affects generated notes in the triggering block");
    std::cout << "All JUCE processor MIDI, slots, pattern display, state, stop, and bypass tests passed.\n";
}
