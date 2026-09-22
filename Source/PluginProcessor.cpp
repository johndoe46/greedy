// SPDX-License-Identifier: GPL-3.0-or-later
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

namespace
{
const std::array<const char*, 11> storedParameterIds {
    "kickDensity", "snareDensity", "hatDensity", "mapX", "mapY", "chaos",
    "kickNote", "snareNote", "hatNote", "normalVelocity", "accentVelocity"
};
const juce::Identifier slotSettingsType { "SlotSettings" };
const juce::Identifier slotType { "Slot" };
}

GreedyAudioProcessor::StoredSlot::StoredSlot() noexcept
{
    for (auto& value : values)
        value.store(0.0f, std::memory_order_relaxed);
}

juce::AudioProcessorValueTreeState::ParameterLayout GreedyAudioProcessor::createParameters()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    const std::array<const char*, 6> ids { "kickDensity", "snareDensity", "hatDensity",
                                         "mapX", "mapY", "chaos" };
    const std::array<const char*, 6> names { "Kick Density", "Snare Density", "Hi-Hat Density",
                                           "Map X", "Map Y", "Chaos" };
    for (std::size_t i = 0; i < ids.size(); ++i)
        layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { ids[i], 1 },
            names[i], juce::NormalisableRange<float> { 0.0f, 100.0f, 0.1f },
            i == 5 ? 0.0f : 50.0f,
            juce::AudioParameterFloatAttributes().withLabel("%")));
    const std::array<const char*, 3> noteIds { "kickNote", "snareNote", "hatNote" };
    const std::array<const char*, 3> noteNames { "Kick MIDI Note", "Snare MIDI Note", "Hi-Hat MIDI Note" };
    const std::array<int, 3> defaults { 36, 38, 42 };
    for (std::size_t i = 0; i < noteIds.size(); ++i)
        layout.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID { noteIds[i], 1 },
            noteNames[i], 0, 127, defaults[i]));
    layout.add(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID { "normalVelocity", 1 }, "Normal Velocity", 1, 127, 90));
    layout.add(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID { "accentVelocity", 1 }, "Accent Velocity", 1, 127, 120));
    return layout;
}

GreedyAudioProcessor::GreedyAudioProcessor()
    : AudioProcessor(BusesProperties()),
      parameters(*this, nullptr, "GreedyState", createParameters())
{
    for (std::size_t i = 0; i < storedParameterIds.size(); ++i)
        storedParameters[i] = parameters.getRawParameterValue(storedParameterIds[i]);
}

GreedyAudioProcessor::~GreedyAudioProcessor()
{
    cancelPendingUpdate();
}

bool GreedyAudioProcessor::isBusesLayoutSupported(const BusesLayout& layout) const
{
    return layout.getMainInputChannelSet().isDisabled()
        && layout.getMainOutputChannelSet().isDisabled();
}

void GreedyAudioProcessor::prepareToPlay(double sampleRate, int maximumBlockSize)
{
    juce::ignoreUnused(maximumBlockSize);
    sequencer.prepare(sampleRate);
    displayedAbsoluteStep.store(std::numeric_limits<std::int64_t>::min(), std::memory_order_relaxed);
}

greedy::Settings GreedyAudioProcessor::settingsFromValues(
    const std::array<float, storedParameterCount>& values) noexcept
{
    greedy::Settings settings;
    const auto byte = [&values](std::size_t i)
    {
        return static_cast<std::uint8_t>(std::lround(
            juce::jlimit(0.0f, 100.0f, values[i]) * 2.55f));
    };
    for (std::size_t i = 0; i < 3; ++i)
    {
        settings.density[i] = byte(i);
        settings.notes[i] = static_cast<int>(values[i + 6]);
    }
    settings.x = byte(3);
    settings.y = byte(4);
    settings.chaos = byte(5);
    settings.normalVelocity = static_cast<std::uint8_t>(values[9]);
    settings.accentVelocity = static_cast<std::uint8_t>(values[10]);
    return settings;
}

greedy::Settings GreedyAudioProcessor::readSettings() const noexcept
{
    std::array<float, storedParameterCount> values {};
    for (std::size_t i = 0; i < values.size(); ++i)
        values[i] = storedParameters[i]->load(std::memory_order_relaxed);
    return settingsFromValues(values);
}

greedy::Settings GreedyAudioProcessor::readSlotSettings(int slot) const noexcept
{
    std::array<float, storedParameterCount> values {};
    const auto& storedSlot = slots[static_cast<std::size_t>(slot)];
    for (std::size_t i = 0; i < values.size(); ++i)
        values[i] = storedSlot.values[i].load(std::memory_order_relaxed);
    return settingsFromValues(values);
}

bool GreedyAudioProcessor::isSlotStored(int slot) const noexcept
{
    return slot >= 0 && slot < slotCount
        && slots[static_cast<std::size_t>(slot)].stored.load(std::memory_order_acquire);
}

void GreedyAudioProcessor::storeSlot(int slot) noexcept
{
    if (slot < 0 || slot >= slotCount)
        return;
    auto& storedSlot = slots[static_cast<std::size_t>(slot)];
    for (std::size_t i = 0; i < storedParameters.size(); ++i)
        storedSlot.values[i].store(
            storedParameters[i]->load(std::memory_order_relaxed), std::memory_order_relaxed);
    storedSlot.stored.store(true, std::memory_order_release);
    currentSlot.store(slot, std::memory_order_relaxed);
}

void GreedyAudioProcessor::recallSlot(int slot)
{
    if (!isSlotStored(slot))
        return;
    const auto& storedSlot = slots[static_cast<std::size_t>(slot)];
    for (std::size_t i = 0; i < storedParameterIds.size(); ++i)
    {
        if (auto* parameter = parameters.getParameter(storedParameterIds[i]))
        {
            const auto value = storedSlot.values[i].load(std::memory_order_relaxed);
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
            parameter->endChangeGesture();
        }
    }
    currentSlot.store(slot, std::memory_order_relaxed);
}

void GreedyAudioProcessor::requestMidiRecall(int slot) noexcept
{
    if (!isSlotStored(slot))
        return;
    midiOverrideSlot.store(slot, std::memory_order_release);
    pendingRecall.store(slot, std::memory_order_release);
    triggerAsyncUpdate();
}

void GreedyAudioProcessor::handleAsyncUpdate()
{
    for (;;)
    {
        const auto slot = pendingRecall.exchange(-1, std::memory_order_acq_rel);
        if (slot < 0)
            break;
        recallSlot(slot);
        auto expected = slot;
        midiOverrideSlot.compare_exchange_strong(expected, -1, std::memory_order_acq_rel);
    }
}

GreedyAudioProcessor::PatternDisplay GreedyAudioProcessor::getPatternDisplay() const noexcept
{
    const auto settings = readSettings();
    const auto absoluteStep = displayedAbsoluteStep.load(std::memory_order_relaxed);
    PatternDisplay display;
    display.notes = settings.notes;
    std::int64_t barStart = 0;
    if (absoluteStep != std::numeric_limits<std::int64_t>::min())
    {
        display.currentStep = static_cast<int>((absoluteStep % 32 + 32) % 32);
        barStart = absoluteStep - display.currentStep;
    }
    // Evaluate on the UI thread; the audio thread only publishes its position.
    for (std::size_t step = 0; step < 32; ++step)
    {
        std::array<bool, 3> accents {};
        const auto velocities = greedy::PatternEngine::evaluate(
            barStart + static_cast<std::int64_t>(step), settings, &accents);
        for (std::size_t part = 0; part < velocities.size(); ++part)
        {
            display.velocities[part][step] = velocities[part];
            display.accents[part][step] = accents[part];
        }
    }
    return display;
}

void GreedyAudioProcessor::emitMidi(void* context, const greedy::NoteEvent& event)
{
    auto& midi = *static_cast<juce::MidiBuffer*>(context);
    const auto message = event.on
        ? juce::MidiMessage::noteOn(midiChannel, event.note, event.velocity)
        : juce::MidiMessage::noteOff(midiChannel, event.note);
    midi.addEvent(message, event.sample);
}

void GreedyAudioProcessor::processBlock(juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    audio.clear();
    for (const auto metadata : midi)
    {
        const auto message = metadata.getMessage();
        if (message.isNoteOn())
        {
            const auto slot = message.getNoteNumber() - firstSlotMidiNote;
            if (slot >= 0 && slot < slotCount)
                requestMidiRecall(slot);
        }
    }
    midi.clear();
    greedy::Transport transport;
    if (auto* hostPlayHead = getPlayHead())
        if (const auto position = hostPlayHead->getPosition())
            if (const auto ppq = position->getPpqPosition())
                if (const auto bpm = position->getBpm())
                    transport = { position->getIsPlaying(), *bpm, *ppq };
    const auto overrideSlot = midiOverrideSlot.load(std::memory_order_acquire);
    const auto settings = isSlotStored(overrideSlot) ? readSlotSettings(overrideSlot) : readSettings();
    sequencer.process(audio.getNumSamples(), transport, settings, &midi, emitMidi);
    displayedAbsoluteStep.store(sequencer.currentAbsoluteStep(), std::memory_order_relaxed);
}

void GreedyAudioProcessor::processBlockBypassed(juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi)
{
    audio.clear();
    midi.clear();
    sequencer.process(audio.getNumSamples(), {}, readSettings(), &midi, emitMidi);
    displayedAbsoluteStep.store(std::numeric_limits<std::int64_t>::min(), std::memory_order_relaxed);
}

void GreedyAudioProcessor::getStateInformation(juce::MemoryBlock& dest)
{
    auto state = parameters.copyState();
    juce::ValueTree slotSettings(slotSettingsType);
    slotSettings.setProperty("current", currentSlot.load(std::memory_order_relaxed), nullptr);
    for (int slot = 0; slot < slotCount; ++slot)
    {
        if (!isSlotStored(slot))
            continue;
        juce::ValueTree child(slotType);
        child.setProperty("index", slot, nullptr);
        const auto& storedSlot = slots[static_cast<std::size_t>(slot)];
        for (int parameter = 0; parameter < storedParameterCount; ++parameter)
            child.setProperty("p" + juce::String(parameter),
                storedSlot.values[static_cast<std::size_t>(parameter)].load(std::memory_order_relaxed), nullptr);
        slotSettings.addChild(child, -1, nullptr);
    }
    state.addChild(slotSettings, -1, nullptr);
    if (const auto xml = state.createXml())
        copyXmlToBinary(*xml, dest);
}

void GreedyAudioProcessor::setStateInformation(const void* data, int size)
{
    if (const auto xml = getXmlFromBinary(data, size))
        if (xml->hasTagName(parameters.state.getType()))
        {
            cancelPendingUpdate();
            pendingRecall.store(-1, std::memory_order_relaxed);
            midiOverrideSlot.store(-1, std::memory_order_relaxed);
            auto state = juce::ValueTree::fromXml(*xml);
            const auto slotSettings = state.getChildWithName(slotSettingsType);
            for (auto& slot : slots)
                slot.stored.store(false, std::memory_order_relaxed);
            if (slotSettings.isValid())
            {
                for (int i = 0; i < slotSettings.getNumChildren(); ++i)
                {
                    const auto child = slotSettings.getChild(i);
                    const auto slot = static_cast<int>(child.getProperty("index", -1));
                    if (slot < 0 || slot >= slotCount)
                        continue;
                    auto& storedSlot = slots[static_cast<std::size_t>(slot)];
                    for (int parameter = 0; parameter < storedParameterCount; ++parameter)
                        storedSlot.values[static_cast<std::size_t>(parameter)].store(
                            static_cast<float>(child.getProperty("p" + juce::String(parameter), 0.0f)),
                            std::memory_order_relaxed);
                    storedSlot.stored.store(true, std::memory_order_release);
                }
                const auto restoredCurrent = static_cast<int>(
                    slotSettings.getProperty("current", -1));
                currentSlot.store(isSlotStored(restoredCurrent) ? restoredCurrent : -1,
                                  std::memory_order_relaxed);
                state.removeChild(slotSettings, nullptr);
            }
            else
            {
                currentSlot.store(-1, std::memory_order_relaxed);
            }
            parameters.replaceState(state);
        }
}

juce::AudioProcessorEditor* GreedyAudioProcessor::createEditor()
{
    return new GreedyAudioProcessorEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GreedyAudioProcessor();
}
