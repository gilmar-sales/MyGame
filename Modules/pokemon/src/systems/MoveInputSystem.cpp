#include "systems/MoveInputSystem.hpp"

#include "components/PokemonComponents.hpp"
#include "systems/PokemonCombatUtil.hpp"

#include <Frigga/ECS/Components/NameComponent.hpp>
#include <Frigga/ECS/Components/TransformComponent.hpp>

#include <algorithm>

MoveInputSystem::MoveInputSystem(const skr::Arc<fr::Registry> &registry,
                                 const skr::Arc<fg::Input> &input)
    : fr::System(registry), mInput(input)
{
}

void MoveInputSystem::Update(float deltaTime)
{
    if(!mInput)
    {
        return;
    }

    mRegistry->CreateMutation()->Each([&](fr::Entity, PokemonMoveset &moves) {
        moves.cd0 = std::max(0.0f, moves.cd0 - deltaTime);
        moves.cd1 = std::max(0.0f, moves.cd1 - deltaTime);
        moves.cd2 = std::max(0.0f, moves.cd2 - deltaTime);
        moves.cd3 = std::max(0.0f, moves.cd3 - deltaTime);
    });

    const bool pressed[4] = {
        mInput->WasPressed("Move1"),
        mInput->WasPressed("Move2"),
        mInput->WasPressed("Move3"),
        mInput->WasPressed("Move4"),
    };

    bool any = false;
    for(bool p : pressed)
    {
        any = any || p;
    }
    if(!any)
    {
        return;
    }

    mRegistry->CreateMutation()->Each(
        [&](fr::Entity entity, fg::NameComponent &name, PokemonVitals &vitals,
            PokemonMoveset &moves, PokemonCombatState &combat, fg::TransformComponent &) {
            if(name.name != "Player" || vitals.knockedOut)
            {
                return;
            }

            for(int slot = 0; slot < 4; ++slot)
            {
                if(!pressed[slot])
                {
                    continue;
                }
                if(PokemonCombat::TryStartMove(*mRegistry, entity, vitals, moves, combat, slot))
                {
                    return;
                }
            }
        });
}
