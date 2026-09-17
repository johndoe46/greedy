// SPDX-License-Identifier: GPL-3.0-or-later
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

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
    return layout;
}

GreedyAudioProcessor::GreedyAudioProcessor()
    : AudioProcessor(BusesProperties()),
      parameters(*this, nullptr, "GreedyState", createParameters())
{
    const std::array<const char*, 6> ids { "kickDensity", "snareDensity", "hatDensity",
                                         "mapX", "mapY", "chaos" };
    for (std::size_t i = 0; i < ids.size(); ++i)
        controls[i] = parameters.getRawParameterValue(ids[i]);
    const std::array<const char*, 3> noteIds { "kickNote", "snareNote", "hatNote" };
    for (std::size_t i = 0; i < noteIds.size(); ++i)
        notes[i] = parameters.getRawParameterValue(noteIds[i]);
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

greedy::Settings GreedyAudioProcessor::readSettings() const noexcept
{
    greedy::Settings settings;
    const auto byte = [this](std::size_t i)
    {
        return static_cast<std::uint8_t>(std::lround(
            juce::jlimit(0.0f, 100.0f, controls[i]->load(std::memory_order_relaxed)) * 2.55f));
    };
    for (std::size_t i = 0; i < 3; ++i)
    {
        settings.density[i] = byte(i);
        settings.notes[i] = static_cast<int>(notes[i]->load(std::memory_order_relaxed));
    }
    settings.x = byte(3);
    settings.y = byte(4);
    settings.chaos = byte(5);
    return settings;
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
        const auto velocities = greedy::PatternEngine::evaluate(
            barStart + static_cast<std::int64_t>(step), settings);
        for (std::size_t part = 0; part < velocities.size(); ++part)
            display.velocities[part][step] = velocities[part];
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
    midi.clear();
    greedy::Transport transport;
    if (auto* hostPlayHead = getPlayHead())
        if (const auto position = hostPlayHead->getPosition())
            if (const auto ppq = position->getPpqPosition())
                if (const auto bpm = position->getBpm())
                    transport = { position->getIsPlaying(), *bpm, *ppq };
    sequencer.process(audio.getNumSamples(), transport, readSettings(), &midi, emitMidi);
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
    if (const auto xml = parameters.copyState().createXml())
        copyXmlToBinary(*xml, dest);
}

void GreedyAudioProcessor::setStateInformation(const void* data, int size)
{
    if (const auto xml = getXmlFromBinary(data, size))
        if (xml->hasTagName(parameters.state.getType()))
            parameters.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessorEditor* GreedyAudioProcessor::createEditor()
{
    return new GreedyAudioProcessorEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GreedyAudioProcessor();
}
