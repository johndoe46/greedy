// SPDX-License-Identifier: GPL-3.0-or-later
#include "MidiSequencer.h"
#include <algorithm>
#include <cmath>

namespace greedy
{
void MidiSequencer::prepare(double newSampleRate) noexcept
{
    sampleRate = newSampleRate > 0 ? newSampleRate : 44100.0;
    noteOffAt.fill(-1);
    wasPlaying = false;
    lastStep = noStep;
    displayedStep = -1;
}

void MidiSequencer::flush(int sample, void* context, Emit emit) noexcept
{
    for (int note = 0; note < 128; ++note)
    {
        auto& deadline = noteOffAt[static_cast<std::size_t>(note)];
        if (deadline >= 0)
            emit(context, { sample, note, 0, false });
        deadline = -1;
    }
}

void MidiSequencer::releaseUntil(int sample, bool inclusive, void* context, Emit emit) noexcept
{
    // Deliver chronologically even if notes were started at different times.
    while (true)
    {
        const auto first = std::min_element(noteOffAt.begin(), noteOffAt.end(),
            [](int a, int b) { return (a < 0 ? std::numeric_limits<int>::max() : a)
                                   < (b < 0 ? std::numeric_limits<int>::max() : b); });
        const auto deadline = *first;
        if (deadline < 0 || deadline > sample || (!inclusive && deadline == sample))
            break;
        emit(context, { deadline, static_cast<int>(first - noteOffAt.begin()), 0, false });
        *first = -1;
    }
}

void MidiSequencer::process(int samples, Transport transport, const Settings& settings,
                            void* context, Emit emit) noexcept
{
    if (samples <= 0)
        return;

    const bool valid = transport.playing && std::isfinite(transport.ppq)
                    && std::abs(transport.ppq) < 1.0e12
                    && std::isfinite(transport.bpm) && transport.bpm >= 1.0
                    && transport.bpm <= 1000.0;
    if (!valid)
    {
        flush(0, context, emit);
        wasPlaying = false;
        lastStep = noStep;
        displayedStep = -1;
        return;
    }

    const auto ppqPerSample = transport.bpm / (60.0 * sampleRate);
    const bool jumped = !wasPlaying
        || std::abs(transport.ppq - expectedPpq) > std::max(1.0e-7, ppqPerSample * 1.5);
    if (jumped || settings.notes != previousNotes)
        flush(0, context, emit);
    if (jumped)
        lastStep = noStep;
    previousNotes = settings.notes;

    // Include a boundary less than one sample before this block: its rounded
    // event belongs at sample zero. lastStep prevents duplicate boundary hits.
    auto absoluteStep = static_cast<std::int64_t>(std::floor(transport.ppq * 8.0));
    const auto duration = std::max(1, static_cast<int>(std::lround(sampleRate * 0.010)));
    for (;; ++absoluteStep)
    {
        const auto offset = (static_cast<double>(absoluteStep) / 8.0 - transport.ppq)
                          / ppqPerSample;
        if (offset >= samples)
            break;
        if (offset <= -1.0 + 1.0e-7)
            continue;
        const auto sample = std::max(0, static_cast<int>(std::ceil(offset - 1.0e-7)));
        if (sample >= samples)
            break;
        if (absoluteStep == lastStep)
            continue;

        releaseUntil(sample, true, context, emit);
        const auto velocities = PatternEngine::evaluate(absoluteStep, settings);
        // Combine coincident hits assigned to the same pitch, choosing the accent.
        std::array<std::uint8_t, 128> hits {};
        for (std::size_t part = 0; part < velocities.size(); ++part)
        {
            const auto note = static_cast<std::size_t>(std::clamp(settings.notes[part], 0, 127));
            hits[note] = std::max(hits[note], velocities[part]);
        }
        for (std::size_t note = 0; note < hits.size(); ++note)
        {
            if (hits[note] == 0)
                continue;
            if (noteOffAt[note] >= 0)
                emit(context, { sample, static_cast<int>(note), 0, false });
            emit(context, { sample, static_cast<int>(note), hits[note], true });
            noteOffAt[note] = sample + duration;
        }
        lastStep = absoluteStep;
        displayedStep = static_cast<int>((absoluteStep % 32 + 32) % 32);
    }

    releaseUntil(samples, false, context, emit);
    for (auto& deadline : noteOffAt)
        if (deadline >= 0)
            deadline -= samples;
    expectedPpq = transport.ppq + samples * ppqPerSample;
    wasPlaying = true;
}
}
