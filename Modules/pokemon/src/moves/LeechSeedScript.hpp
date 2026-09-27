#pragma once

#include "moves/MoveScript.hpp"

#include <memory>

/// Leech Seed execution script:
///   Cast      -> MoveCombatSystem spawns the seed projectile (legacy path).
///   Hit       -> sticks the seed: PokemonStatus link (drain/heal) + LeechSeedAnchor
///                visual (seed + roots) owned by LeechSeedFxSystem.
///   Tick      -> StatusEffectSystem applies damage/heal each second;
///                LeechSeedFxSystem spawns one green drain orb per tick flying
///                victim -> caster.
///   Expire/KO -> link cleared by StatusEffectSystem; anchor/orbs cleaned by FxSystem.
class LeechSeedScript final: public MoveScript
{
  public:
    [[nodiscard]] std::string_view Id() const override
    {
        return "leech_seed";
    }

    void OnProjectileHit(MoveHitCtx &ctx) override;
};
