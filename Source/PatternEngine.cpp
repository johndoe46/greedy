// SPDX-License-Identifier: GPL-3.0-or-later
// Adapted from Mutable Instruments Grids, Copyright 2011 Emilie Gillet.
#include "PatternEngine.h"
#include "DrumMaps.h"
#include <algorithm>

namespace greedy
{
namespace
{
constexpr int map[5][5] {
    { 10, 8, 0, 9, 11 }, { 15, 7, 13, 12, 6 }, { 18, 14, 4, 5, 3 },
    { 23, 16, 21, 1, 2 }, { 24, 19, 17, 20, 22 }
};

// Grids' U8Mix uses weights summing to 255 and divides by 256.
std::uint8_t mix(std::uint8_t a, std::uint8_t b, std::uint8_t balance) noexcept
{
    return static_cast<std::uint8_t>((a * (255 - balance) + b * balance) >> 8);
}

std::uint8_t randomByte(std::int64_t cycle, int part) noexcept
{
    // A cycle-derived seed keeps chaos independent of host block size and seeks.
    auto value = static_cast<std::uint64_t>(cycle)
               + 0x9e3779b97f4a7c15ULL * static_cast<std::uint64_t>(part + 1);
    value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
    return static_cast<std::uint8_t>((value ^ (value >> 31)) >> 56);
}
}

std::uint8_t PatternEngine::readMap(int step, int part, std::uint8_t x,
                                   std::uint8_t y) noexcept
{
    const auto i = x >> 6;
    const auto j = y >> 6;
    const auto offset = part * 32 + step;
    const auto a = drumNodes[map[i][j]][offset];
    const auto b = drumNodes[map[i + 1][j]][offset];
    const auto c = drumNodes[map[i][j + 1]][offset];
    const auto d = drumNodes[map[i + 1][j + 1]][offset];
    return mix(mix(a, b, static_cast<std::uint8_t>(x << 2)),
               mix(c, d, static_cast<std::uint8_t>(x << 2)),
               static_cast<std::uint8_t>(y << 2));
}

std::array<std::uint8_t, 3> PatternEngine::evaluate(std::int64_t absoluteStep,
                                                 const Settings& settings) noexcept
{
    const auto step = static_cast<int>((absoluteStep % 32 + 32) % 32);
    const auto cycle = (absoluteStep - step) / 32;
    std::array<std::uint8_t, 3> velocities {};
    for (int part = 0; part < 3; ++part)
    {
        const auto index = static_cast<std::size_t>(part);
        const auto perturbation = (randomByte(cycle, part) * (settings.chaos >> 2)) >> 8;
        const auto level = std::min(255, readMap(step, part, settings.x, settings.y)
                                       + perturbation);
        if (level > 255 - settings.density[index])
            velocities[index] = static_cast<std::uint8_t>(level > 192 ? 120 : 90);
    }
    return velocities;
}
}
