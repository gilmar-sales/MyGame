#include "systems/ItemUseSystem.hpp"

#include "components/PokemonComponents.hpp"

#include <Frigga/ECS/Components/NameComponent.hpp>

#include <algorithm>

namespace
{
    constexpr float kPotionHeal       = 40.0f;
    constexpr float kOranHeal         = 20.0f;
    constexpr float kStaminaBerryRest = 35.0f;
} // namespace

ItemUseSystem::ItemUseSystem(const skr::Arc<fr::Registry> &registry,
                             const skr::Arc<fg::Input> &input)
    : fr::System(registry), mInput(input)
{
}

void ItemUseSystem::Update(float)
{
    if(!mInput)
    {
        return;
    }

    const bool usePotion = mInput->WasPressed("UsePotion");
    const bool useBerry  = mInput->WasPressed("UseStaminaBerry");
    const bool useOran   = mInput->WasPressed("UseOranBerry");
    if(!usePotion && !useBerry && !useOran)
    {
        return;
    }

    mRegistry->CreateMutation()->Each(
        [&](fr::Entity, fg::NameComponent &name, PokemonVitals &vitals,
            PokemonInventory &inventory) {
            if(name.name != "Player" || vitals.knockedOut)
            {
                return;
            }

    if(usePotion && inventory.potions > 0)
            {
                inventory.potions -= 1;
                vitals.hp = std::min(vitals.maxHp, vitals.hp + kPotionHeal);
            }
            if(useOran && inventory.oranBerries > 0)
            {
                inventory.oranBerries -= 1;
                vitals.hp = std::min(vitals.maxHp, vitals.hp + kOranHeal);
            }
            if(useBerry && inventory.staminaBerries > 0)
            {
                inventory.staminaBerries -= 1;
                vitals.stamina = std::min(vitals.maxStamina, vitals.stamina + kStaminaBerryRest);
            }
        });
}
