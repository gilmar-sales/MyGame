#include "systems/StatusEffectSystem.hpp"

#include "components/PokemonComponents.hpp"
#include "systems/PokemonCombatUtil.hpp"

#include <algorithm>

namespace
{
    void ClearLeechSeeding(PokemonStatus &status)
    {
        status.leechSeeding      = false;
        status.leechSeedingTimer = 0.0f;
        status.leechSeedTarget   = -1;
    }

    void ClearLeechSeeded(PokemonStatus &status)
    {
        status.leechSeeded     = false;
        status.leechSeedTimer  = 0.0f;
        status.leechSeedTick   = 0.0f;
        status.leechSeedSource = -1;
    }
} // namespace

StatusEffectSystem::StatusEffectSystem(const skr::Arc<fr::Registry> &registry)
    : fr::System(registry)
{
}

void StatusEffectSystem::Update(float deltaTime)
{
    if(deltaTime <= 0.0f)
    {
        return;
    }

    mRegistry->CreateMutation()->Each(
        [&](fr::Entity entity, PokemonVitals &vitals, PokemonStatus &status) {
            // Headbutt stun: brief lock so locomotion/AI do not cancel physics shove.
            if(status.stunTimer > 0.0f)
            {
                status.stunTimer -= deltaTime;
                if(status.stunTimer <= 0.0f)
                {
                    status.stunTimer = 0.0f;
                    if(!vitals.knockedOut)
                    {
                        bool keepLocked = false;
                        if(mRegistry->HasComponent<PokemonCombatState>(entity))
                        {
                            mRegistry->TryGetComponents<PokemonCombatState>(
                                entity, [&](PokemonCombatState &combat) {
                                    keepLocked = combat.phase != PokemonCombat::kPhaseIdle ||
                                                 combat.savedMaxStrength >= 0.0f;
                                });
                        }
                        if(!keepLocked)
                        {
                            PokemonCombat::SetLocomotionLocked(*mRegistry, entity, false);
                        }
                    }
                }
            }

            // Caster-side Leech Seed status (linked heal source).
            if(status.leechSeeding)
            {
                status.leechSeedingTimer -= deltaTime;
                const auto targetId = static_cast<fr::Entity>(status.leechSeedTarget);
                const bool targetGone =
                    status.leechSeedTarget < 0 ||
                    !mRegistry->HasComponent<PokemonVitals>(targetId) ||
                    [&]() {
                        bool ko = false;
                        mRegistry->TryGetComponents<PokemonVitals>(
                            targetId, [&](PokemonVitals &v) { ko = v.knockedOut; });
                        return ko;
                    }();

                if(status.leechSeedingTimer <= 0.0f || targetGone || vitals.knockedOut)
                {
                    if(!targetGone && mRegistry->HasComponent<PokemonStatus>(targetId))
                    {
                        mRegistry->TryGetComponents<PokemonStatus>(
                            targetId, [&](PokemonStatus &t) {
                                if(t.leechSeedSource == static_cast<std::int64_t>(entity))
                                {
                                    ClearLeechSeeded(t);
                                }
                            });
                    }
                    ClearLeechSeeding(status);
                }
            }

            if(!status.leechSeeded || vitals.knockedOut)
            {
                if(vitals.knockedOut && status.leechSeeded)
                {
                    const auto sourceId = static_cast<fr::Entity>(status.leechSeedSource);
                    if(status.leechSeedSource >= 0 &&
                       mRegistry->HasComponent<PokemonStatus>(sourceId))
                    {
                        mRegistry->TryGetComponents<PokemonStatus>(
                            sourceId, [&](PokemonStatus &src) {
                                if(src.leechSeedTarget == static_cast<std::int64_t>(entity))
                                {
                                    ClearLeechSeeding(src);
                                }
                            });
                    }
                    ClearLeechSeeded(status);
                }
                return;
            }

            status.leechSeedTimer -= deltaTime;
            status.leechSeedTick += deltaTime;
            while(status.leechSeedTick >= 1.0f)
            {
                status.leechSeedTick -= 1.0f;
                const float dmg = status.leechSeedDps;
                vitals.hp       = std::max(0.0f, vitals.hp - dmg);
                if(vitals.hp <= 0.0f)
                {
                    vitals.knockedOut = true;
                }

                const auto sourceId = static_cast<fr::Entity>(status.leechSeedSource);
                if(status.leechSeedSource >= 0 &&
                   mRegistry->HasComponent<PokemonVitals>(sourceId))
                {
                    mRegistry->TryGetComponents<PokemonVitals>(
                        sourceId, [&](PokemonVitals &src) {
                            if(!src.knockedOut)
                            {
                                src.hp = std::min(src.maxHp,
                                                  src.hp + dmg * status.leechSeedHeal);
                            }
                        });
                }
            }

            if(status.leechSeedTimer <= 0.0f || vitals.knockedOut)
            {
                const auto sourceId = static_cast<fr::Entity>(status.leechSeedSource);
                if(status.leechSeedSource >= 0 &&
                   mRegistry->HasComponent<PokemonStatus>(sourceId))
                {
                    mRegistry->TryGetComponents<PokemonStatus>(
                        sourceId, [&](PokemonStatus &src) {
                            if(src.leechSeedTarget == static_cast<std::int64_t>(entity))
                            {
                                ClearLeechSeeding(src);
                            }
                        });
                }
                ClearLeechSeeded(status);
            }
        });
}
