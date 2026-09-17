// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <array>
#include <cstdint>

namespace greedy
{
struct Settings
{
    std::array<std::uint8_t, 3> density { 128, 128, 128 };
    std::uint8_t x = 128;
    std::uint8_t y = 128;
    std::uint8_t chaos = 0;
    std::array<int, 3> notes { 36, 38, 42 };
};

class PatternEngine
{
public:
    // 32 thirty-second notes = four quarter notes. Accent levels become velocity.
    static std::array<std::uint8_t, 3> evaluate(std::int64_t absoluteStep,
                                               const Settings& settings) noexcept;
    static std::uint8_t readMap(int step, int part, std::uint8_t x,
                               std::uint8_t y) noexcept;
};
}
