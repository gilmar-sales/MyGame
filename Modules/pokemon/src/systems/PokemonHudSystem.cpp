#include "systems/PokemonHudSystem.hpp"

#include "components/PokemonComponents.hpp"

#include <Frigga/ECS/Components/HealthBarComponent.hpp>
#include <Frigga/ECS/Components/HierarchyComponent.hpp>

#include <algorithm>

PokemonHudSystem::PokemonHudSystem(const skr::Arc<fr::Registry> &registry)
    : fr::System(registry)
{
}

void PokemonHudSystem::Update(float)
{
    mRegistry->CreateMutation()->Each([&](fr::Entity entity, PokemonVitals &vitals) {
        const float fill =
            vitals.maxHp > 0.0f ? std::clamp(vitals.hp / vitals.maxHp, 0.0f, 1.0f) : 0.0f;

        if(mRegistry->HasComponent<fg::HealthBarComponent>(entity))
        {
            mRegistry->TryGetComponents<fg::HealthBarComponent>(
                entity, [&](fg::HealthBarComponent &bar) { bar.fill = fill; });
        }

        if(mRegistry->HasComponent<fg::HierarchyComponent>(entity))
        {
            mRegistry->TryGetComponents<fg::HierarchyComponent>(
                entity, [&](fg::HierarchyComponent &hierarchy) {
                    for(const fr::Entity child : hierarchy.children)
                    {
                        if(!mRegistry->HasComponent<fg::HealthBarComponent>(child))
                        {
                            continue;
                        }
                        mRegistry->TryGetComponents<fg::HealthBarComponent>(
                            child, [&](fg::HealthBarComponent &bar) { bar.fill = fill; });
                    }
                });
        }
    });
}
