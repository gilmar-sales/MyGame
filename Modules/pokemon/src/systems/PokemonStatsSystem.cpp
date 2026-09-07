#include "systems/PokemonStatsSystem.hpp"

#include "components/PokemonComponents.hpp"
#include "data/PokemonCatalog.hpp"
#include "data/StatFormulas.hpp"

#include <algorithm>

PokemonStatsSystem::PokemonStatsSystem(const skr::Arc<fr::Registry> &registry)
    : fr::System(registry)
{
}

void PokemonStatsSystem::Update(float)
{
    mRegistry->CreateMutation()->Each(
        [&](fr::Entity, PokemonIdentity &identity, PokemonIVs &ivs, PokemonStats &stats,
            PokemonTypes &types, PokemonVitals &vitals) {
            const SpeciesDef *species = FindSpecies(identity.speciesId);
            if(species == nullptr)
            {
                return;
            }

            if(!stats.dirty && vitals.initialized)
            {
                return;
            }

            const int level = static_cast<int>(std::clamp<std::int64_t>(identity.level, 1, 100));
            stats.hp = CalcHp(species->baseHp, static_cast<int>(ivs.hp), level);
            stats.attack =
                CalcOther(species->baseAtk, static_cast<int>(ivs.attack), level);
            stats.defense =
                CalcOther(species->baseDef, static_cast<int>(ivs.defense), level);
            stats.spAttack =
                CalcOther(species->baseSpAtk, static_cast<int>(ivs.spAttack), level);
            stats.spDefense =
                CalcOther(species->baseSpDef, static_cast<int>(ivs.spDefense), level);
            stats.speed =
                CalcOther(species->baseSpeed, static_cast<int>(ivs.speed), level);
            stats.dirty = false;

            types.primary   = static_cast<std::int64_t>(species->primary);
            types.secondary = static_cast<std::int64_t>(species->secondary);

            const float newMaxHp = static_cast<float>(stats.hp);
            const float newMaxStamina = CalcMaxStamina(species->baseStamina, level);
            if(!vitals.initialized)
            {
                vitals.maxHp      = newMaxHp;
                vitals.hp         = newMaxHp;
                vitals.maxStamina = newMaxStamina;
                vitals.stamina    = newMaxStamina;
                vitals.initialized = true;
                vitals.knockedOut  = false;
            }
            else
            {
                const float hpRatio =
                    vitals.maxHp > 0.0f ? vitals.hp / vitals.maxHp : 1.0f;
                const float staminaRatio =
                    vitals.maxStamina > 0.0f ? vitals.stamina / vitals.maxStamina : 1.0f;
                vitals.maxHp      = newMaxHp;
                vitals.maxStamina = newMaxStamina;
                vitals.hp         = std::clamp(newMaxHp * hpRatio, 0.0f, newMaxHp);
                vitals.stamina =
                    std::clamp(newMaxStamina * staminaRatio, 0.0f, newMaxStamina);
            }
        });
}
