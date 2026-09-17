// SPDX-License-Identifier: GPL-3.0-or-later
#include "MidiSequencer.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <vector>

namespace
{
void require(bool result, const char* message)
{
    if (!result)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

struct Capture
{
    int base = 0;
    int blockSize = 0;
    std::vector<greedy::NoteEvent> events;
    static void emit(void* context, const greedy::NoteEvent& event)
    {
        auto& capture = *static_cast<Capture*>(context);
        require(event.sample >= 0 && event.sample < capture.blockSize,
                "MIDI event belongs to its processing block");
        auto timed = event;
        timed.sample += capture.base;
        capture.events.push_back(timed);
    }
};

void render(greedy::MidiSequencer& engine, Capture& capture, int samples,
            greedy::Transport transport, const greedy::Settings& settings)
{
    capture.blockSize = samples;
    engine.process(samples, transport, settings, &capture, Capture::emit);
    capture.base += samples;
}

std::vector<greedy::NoteEvent> sequence(int blockSize, double bpm, double sampleRate)
{
    greedy::MidiSequencer engine;
    engine.prepare(sampleRate);
    greedy::Settings settings;
    settings.chaos = 230;
    settings.density = { 255, 180, 210 };
    Capture capture;
    constexpr int length = 150001;
    for (int frame = 0; frame < length;)
    {
        const auto size = std::min(blockSize, length - frame);
        render(engine, capture, size, { true, bpm, frame * bpm / (60.0 * sampleRate) }, settings);
        frame += size;
    }
    render(engine, capture, 1, {}, settings);
    return capture.events;
}

void checkBalanced(const std::vector<greedy::NoteEvent>& events)
{
    std::array<bool, 128> active {};
    int lastSample = -1;
    for (const auto& event : events)
    {
        require(event.sample >= lastSample, "events are chronological");
        lastSample = event.sample;
        auto& note = active[static_cast<std::size_t>(event.note)];
        require(event.on != note, "note-ons and note-offs alternate for each pitch");
        note = event.on;
    }
    require(std::none_of(active.begin(), active.end(), [](bool value) { return value; }),
            "no hanging notes");
}

void patternTests()
{
    require(greedy::PatternEngine::readMap(0, 0, 0, 0) == 143,
            "original AVR interpolation rounding is preserved at node 10");
    require(greedy::PatternEngine::readMap(12, 0, 0, 0) == 253,
            "original maximum is attenuated by both interpolation stages");
    greedy::Settings settings;
    for (int chaos : { 0, 255 })
        for (int step = -32; step < 96; ++step)
        {
            settings.chaos = static_cast<std::uint8_t>(chaos);
            settings.density.fill(0);
            require(greedy::PatternEngine::evaluate(step, settings)
                    == std::array<std::uint8_t, 3> {}, "zero density always mutes");
        }

    settings.x = 0;
    settings.y = 0;
    settings.chaos = 0;
    settings.density.fill(255);
    const auto normal = greedy::PatternEngine::evaluate(0, settings);
    require(normal[0] == 90, "ordinary Grids hits use velocity 90");
    require(greedy::PatternEngine::evaluate(12, settings)[0] == 120,
            "Grids accents use velocity 120");
    require(greedy::PatternEngine::evaluate(-32, settings) == normal,
            "negative host positions wrap correctly");

    for (int step = 0; step < 32; ++step)
    {
        std::array<std::uint8_t, 3> previous {};
        for (int density = 0; density < 256; ++density)
        {
            settings.density.fill(static_cast<std::uint8_t>(density));
            const auto hits = greedy::PatternEngine::evaluate(step, settings);
            for (std::size_t part = 0; part < 3; ++part)
                require(previous[part] == 0 || hits[part] != 0,
                        "raising density never removes a hit");
            previous = hits;
        }
    }

    bool mapsDiffer = false;
    bool chaosDiffers = false;
    settings.density.fill(180);
    for (int step = 0; step < 256; ++step)
    {
        settings.x = settings.y = settings.chaos = 0;
        const auto baseline = greedy::PatternEngine::evaluate(step, settings);
        settings.x = settings.y = 255;
        mapsDiffer |= baseline != greedy::PatternEngine::evaluate(step, settings);
        settings.x = settings.y = 0;
        settings.chaos = 255;
        chaosDiffers |= baseline != greedy::PatternEngine::evaluate(step, settings);
    }
    require(mapsDiffer, "map controls change the sequence");
    require(chaosDiffers, "chaos changes the sequence");
}

void timingTests()
{
    for (double rate : { 44100.0, 48000.0, 96000.0 })
        for (double bpm : { 120.0, 123.45, 480.0 })
        {
            const auto reference = sequence(150001, bpm, rate);
            require(!reference.empty(), "playing produces notes");
            checkBalanced(reference);
            for (int blockSize : { 1, 17, 127, 512, 2048 })
            {
                const auto chunked = sequence(blockSize, bpm, rate);
                require(reference.size() == chunked.size(), "block size does not change hit count");
                for (std::size_t i = 0; i < reference.size(); ++i)
                    require(reference[i].sample == chunked[i].sample
                            && reference[i].note == chunked[i].note
                            && reference[i].velocity == chunked[i].velocity
                            && reference[i].on == chunked[i].on,
                            "block size does not change sample timing, notes, or chaos");
            }
        }
}

void lifecycleTests()
{
    greedy::Settings settings;
    settings.density.fill(255);
    settings.x = settings.y = 0;
    greedy::MidiSequencer engine;
    engine.prepare(48000);
    Capture capture;
    render(engine, capture, 16, { true, 120, 0 }, settings);
    const auto beforeStop = capture.events.size();
    render(engine, capture, 16, {}, settings);
    require(capture.events.size() > beforeStop, "stop immediately releases held notes");
    checkBalanced(capture.events);
    const auto afterStop = capture.events.size();
    render(engine, capture, 16, {}, settings);
    require(capture.events.size() == afterStop, "stopped host generates no new events");

    render(engine, capture, 16, { true, 120, 0 }, settings);
    settings.notes = { 60, 61, 62 };
    render(engine, capture, 16, { true, 120, 16.0 / 24000.0 }, settings);
    render(engine, capture, 16, { true, 120, 2.0 }, settings);
    render(engine, capture, 16, { true, 120, 0.0 }, settings);
    render(engine, capture, 16, {}, settings);
    checkBalanced(capture.events);
    require(std::any_of(capture.events.begin(), capture.events.end(),
                [](const auto& event) { return event.on && event.note == 60; }),
            "changed note mapping is used after transport seek");

    engine.prepare(48000);
    capture = {};
    settings.notes.fill(36);
    render(engine, capture, 16, { true, 120, 0 }, settings);
    require(capture.events.size() == 1, "parts sharing a pitch produce one note-on");
    render(engine, capture, 512, { true, 120, 16.0 / 24000.0 }, settings);
    checkBalanced(capture.events);

    engine.prepare(48000);
    capture = {};
    render(engine, capture, 16, { true, 120, 0 }, settings);
    render(engine, capture, 16, { true, 120, std::numeric_limits<double>::quiet_NaN() }, settings);
    checkBalanced(capture.events);

    engine.prepare(48000);
    capture = {};
    render(engine, capture, 3000, { true, 120, 0 }, settings);
    // Continuous PPQ with a tempo change must not retrigger the previous step.
    render(engine, capture, 3000, { true, 180, 0.125 }, settings);
    render(engine, capture, 16, {}, settings);
    checkBalanced(capture.events);
}
}

int main()
{
    patternTests();
    timingTests();
    lifecycleTests();
    std::cout << "All Greedy pattern, MIDI timing, transport, and note lifecycle tests passed.\n";
}
