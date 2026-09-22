// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "MidiSequencer.h"

class GreedyAudioProcessor final : public juce::AudioProcessor,
                                   private juce::AsyncUpdater
{
public:
    struct PatternDisplay
    {
        std::array<std::array<std::uint8_t, 32>, 3> velocities {};
        std::array<std::array<bool, 32>, 3> accents {};
        std::array<int, 3> notes {};
        int currentStep = -1;
    };

    GreedyAudioProcessor();
    ~GreedyAudioProcessor() override;
    void prepareToPlay(double sampleRate, int maximumBlockSize) override;
    void releaseResources() override {}
    using juce::AudioProcessor::processBlock;
    using juce::AudioProcessor::processBlockBypassed;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    bool isBusesLayoutSupported(const BusesLayout& layout) const override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Greedy"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return true; }
    double getTailLengthSeconds() const override { return 0.010; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;
    PatternDisplay getPatternDisplay() const noexcept;
    void storeSlot(int slot) noexcept;
    void recallSlot(int slot);
    bool isSlotStored(int slot) const noexcept;
    int getCurrentSlot() const noexcept { return currentSlot.load(std::memory_order_relaxed); }

    juce::AudioProcessorValueTreeState parameters;
    static constexpr int midiChannel = 10;
    static constexpr int slotCount = 12;
    static constexpr int firstSlotMidiNote = 36;
    static constexpr int slotMidiNote(int slot) noexcept { return firstSlotMidiNote + slot; }

private:
    static constexpr int storedParameterCount = 11;
    struct StoredSlot
    {
        StoredSlot() noexcept;
        std::array<std::atomic<float>, storedParameterCount> values;
        std::atomic<bool> stored { false };
    };

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameters();
    greedy::Settings readSettings() const noexcept;
    greedy::Settings readSlotSettings(int slot) const noexcept;
    static greedy::Settings settingsFromValues(
        const std::array<float, storedParameterCount>& values) noexcept;
    void requestMidiRecall(int slot) noexcept;
    void handleAsyncUpdate() override;
    static void emitMidi(void* context, const greedy::NoteEvent& event);
    std::array<std::atomic<float>*, storedParameterCount> storedParameters {};
    std::array<StoredSlot, slotCount> slots;
    std::atomic<int> pendingRecall { -1 };
    std::atomic<int> midiOverrideSlot { -1 };
    std::atomic<int> currentSlot { -1 };
    greedy::MidiSequencer sequencer;
    std::atomic<std::int64_t> displayedAbsoluteStep { std::numeric_limits<std::int64_t>::min() };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GreedyAudioProcessor)
};
