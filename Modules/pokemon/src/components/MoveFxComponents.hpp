#pragma once

#include "components/PokemonComponents.hpp"
#include "data/PokemonCatalog.hpp"

#include <Frigga/Macro.hpp>

#include <Freyr/Freyr.hpp>

#include <string>

// Runtime VFX for scripted moves (Leech Seed anchor + drain orbs, ...)
struct LeechSeedAnchor: fr::Component
{
    std::int64_t target = -1;
    std::int64_t source = -1;
    /// Counts up to kDrainIntervalSec; each wrap spawns one drain orb visual.
    float orbTimer = 0.0f;
    float pulse    = 0.0f;
};

/// Transient homing orb: flies victim -> caster, purely visual.
/// Damage/heal is applied by the DoT owner (StatusEffectSystem); this only shows it.
struct LeechDrainOrb: fr::Component
{
    std::int64_t caster = -1;
    std::int64_t victim = -1;
    float        t      = 0.0f;
    float        duration = 0.45f;
};
