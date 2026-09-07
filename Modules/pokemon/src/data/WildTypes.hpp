#pragma once

#include "components/PokemonComponents.hpp"
#include "data/ElementType.hpp"

namespace WildPersonality
{
    inline constexpr std::int64_t kAggressive = 0;
    inline constexpr std::int64_t kPeaceful   = 1;
    inline constexpr std::int64_t kSkittish   = 2;
    inline constexpr std::int64_t kCowardly   = 3;
} // namespace WildPersonality

namespace WildAIState
{
    inline constexpr std::int64_t kPassive  = 0;
    inline constexpr std::int64_t kCombat   = 1;
    inline constexpr std::int64_t kFleeing  = 2;
    inline constexpr std::int64_t kFainted  = 3;
} // namespace WildAIState

namespace PokemonTeamId
{
    inline constexpr std::int64_t kPlayer = 0;
    inline constexpr std::int64_t kWild   = 1;
} // namespace PokemonTeamId
