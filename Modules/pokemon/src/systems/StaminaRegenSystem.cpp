#include "systems/StaminaRegenSystem.hpp"

#include "components/PokemonComponents.hpp"

#include <algorithm>

StaminaRegenSystem::StaminaRegenSystem(const skr::Arc<fr::Registry> &registry)
    : fr::System(registry)
{
}

void StaminaRegenSystem::Update(float deltaTime)
{
    if(deltaTime <= 0.0f)
    {
        return;
    }

    mRegistry->CreateMutation()->Each(
        [&](fr::Entity, PokemonVitals &vitals, PokemonCombatState &combat) {
            if(vitals.knockedOut)
            {
                return;
            }
            // Pause regen while casting / body-blow motion is active.
            if(combat.phase != 0)
            {
                return;
            }
            vitals.stamina = std::min(vitals.maxStamina,
                                      vitals.stamina + vitals.staminaRegenPerSec * deltaTime);
        });
}
