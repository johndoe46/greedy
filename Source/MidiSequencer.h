// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "PatternEngine.h"
#include <array>
#include <limits>

namespace greedy
{
struct Transport
{
    bool playing = false;
    double bpm = 120.0;
    double ppq = 0.0;
};

struct NoteEvent
{
    int sample = 0;
    int note = 0;
    std::uint8_t velocity = 0;
    bool on = false;
};

class MidiSequencer
{
public:
    using Emit = void (*)(void*, const NoteEvent&);
    MidiSequencer() noexcept { prepare(44100.0); }
    void prepare(double newSampleRate) noexcept;
    void process(int samples, Transport transport, const Settings& settings,
                 void* context, Emit emit) noexcept;
    int currentStep() const noexcept { return displayedStep; }
    std::int64_t currentAbsoluteStep() const noexcept { return lastStep; }

private:
    void flush(int sample, void* context, Emit emit) noexcept;
    void releaseUntil(int sample, bool inclusive, void* context, Emit emit) noexcept;
    static constexpr auto noStep = std::numeric_limits<std::int64_t>::min();
    // One deadline per MIDI pitch avoids premature offs when parts share a note.
    std::array<int, 128> noteOffAt {};
    std::array<int, 3> previousNotes { 36, 38, 42 };
    double sampleRate = 44100.0;
    double expectedPpq = 0.0;
    std::int64_t lastStep = noStep;
    bool wasPlaying = false;
    int displayedStep = -1;
};
}
