#pragma once

#include "data/ElementType.hpp"

/// Gen1-style effectiveness. Returns 0, 0.25, 0.5, 1, 2, or 4 for dual defense.
namespace TypeChart
{
    [[nodiscard]] float Multiplier(ElementType attack, ElementType defend);
    [[nodiscard]] float Multiplier(ElementType attack, ElementType defendA, ElementType defendB);
} // namespace TypeChart
