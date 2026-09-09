#include "systems/PokemonStatsSystem.hpp"
#include "systems/StaminaRegenSystem.hpp"
#include "systems/StatusEffectSystem.hpp"
#include "systems/WildSpawnSystem.hpp"
#include "systems/WildAISystem.hpp"
#include "systems/MoveInputSystem.hpp"
#include "systems/MoveCombatSystem.hpp"
#include "systems/ProjectileSystem.hpp"
#include "systems/ItemUseSystem.hpp"
#include "systems/PokemonHudSystem.hpp"
#include "systems/PlayerRespawnSystem.hpp"

#include "components/PokemonComponents.hpp"
#include "data/WildTypes.hpp"

#include <Frigga/Module/FriModule.hpp>

#include <cstdio>

namespace
{
    void DrawIdentity(PokemonIdentity &c, fg::FriComponentInspector &ui)
    {
        ui.InputText("Species", c.speciesId);
        ui.InputText("Nickname", c.nickname);
        int level = static_cast<int>(c.level);
        if(ui.SliderInt("Level", level, 1, 100))
        {
            c.level = level;
        }
    }

    void DrawIVs(PokemonIVs &c, fg::FriComponentInspector &ui)
    {
        auto slider = [&](const char *label, std::int64_t &value) {
            int v = static_cast<int>(value);
            if(ui.SliderInt(label, v, 0, 31))
            {
                value = v;
            }
        };
        slider("HP IV", c.hp);
        slider("Atk IV", c.attack);
        slider("Def IV", c.defense);
        slider("SpA IV", c.spAttack);
        slider("SpD IV", c.spDefense);
        slider("Spe IV", c.speed);
    }

    void DrawStats(PokemonStats &c, fg::FriComponentInspector &ui)
    {
        ui.BeginDisabled(true);
        char buf[64];
        std::snprintf(buf, sizeof(buf), "HP %lld  Atk %lld  Def %lld",
                      static_cast<long long>(c.hp), static_cast<long long>(c.attack),
                      static_cast<long long>(c.defense));
        ui.TextDisabled(buf);
        std::snprintf(buf, sizeof(buf), "SpA %lld  SpD %lld  Spe %lld",
                      static_cast<long long>(c.spAttack), static_cast<long long>(c.spDefense),
                      static_cast<long long>(c.speed));
        ui.TextDisabled(buf);
        ui.EndDisabled();
        if(!ui.playing)
        {
            // Mark dirty via toggling — user edits IVs/level; force recalc button-like.
            c.dirty = true;
        }
    }

    void DrawTypes(PokemonTypes &c, fg::FriComponentInspector &ui)
    {
        int primary   = static_cast<int>(c.primary);
        int secondary = static_cast<int>(c.secondary);
        if(ui.SliderInt("Primary Type", primary, -1, 14))
        {
            c.primary = primary;
        }
        if(ui.SliderInt("Secondary Type", secondary, -1, 14))
        {
            c.secondary = secondary;
        }
        ui.TextDisabled("0 Normal .. 3 Grass .. 7 Poison .. 1 Fire");
    }

    void DrawVitals(PokemonVitals &c, fg::FriComponentInspector &ui)
    {
        ui.DragFloat("HP", c.hp, 1.0f, 0.0f, 9999.0f);
        ui.DragFloat("Max HP", c.maxHp, 1.0f, 1.0f, 9999.0f);
        ui.DragFloat("Stamina", c.stamina, 1.0f, 0.0f, 9999.0f);
        ui.DragFloat("Max Stamina", c.maxStamina, 1.0f, 1.0f, 9999.0f);
        ui.DragFloat("Stamina Regen/s", c.staminaRegenPerSec, 0.1f, 0.0f, 100.0f);
    }

    void DrawMoveset(PokemonMoveset &c, fg::FriComponentInspector &ui)
    {
        ui.InputText("Move 1", c.move0);
        ui.InputText("Move 2", c.move1);
        ui.InputText("Move 3", c.move2);
        ui.InputText("Move 4", c.move3);
        ui.TextDisabled("Catalog: tackle, headbutt, razor_leaf, leech_seed, solar_beam");
        if(ui.playing)
        {
            char buf[96];
            std::snprintf(buf, sizeof(buf), "CD: %.1f / %.1f / %.1f / %.1f", c.cd0, c.cd1, c.cd2,
                          c.cd3);
            ui.TextDisabled(buf);
        }
    }

    void DrawInventory(PokemonInventory &c, fg::FriComponentInspector &ui)
    {
        int potions = static_cast<int>(c.potions);
        int oran    = static_cast<int>(c.oranBerries);
        int stamina = static_cast<int>(c.staminaBerries);
        if(ui.SliderInt("Potions", potions, 0, 99))
        {
            c.potions = potions;
        }
        if(ui.SliderInt("Oran Berries", oran, 0, 99))
        {
            c.oranBerries = oran;
        }
        if(ui.SliderInt("Stamina Berries", stamina, 0, 99))
        {
            c.staminaBerries = stamina;
        }
        ui.TextDisabled("Q potion, G oran, F stamina berry");
    }

    void DrawWildSpawn(WildSpawnArea &c, fg::FriComponentInspector &ui)
    {
        ui.InputText("Species", c.speciesId);
        int personality = static_cast<int>(c.personality);
        if(ui.SliderInt("Personality", personality, 0, 3))
        {
            c.personality = personality;
        }
        ui.TextDisabled("0 agg, 1 peaceful, 2 skittish, 3 cowardly");
        ui.DragFloat("Spawn Radius", c.spawnRadius, 0.1f, 1.0f, 100.0f);
        ui.DragFloat("Alert Radius", c.alertRadius, 0.1f, 0.5f, 50.0f);
        int maxCount = static_cast<int>(c.maxCount);
        if(ui.SliderInt("Max Count", maxCount, 0, 1000))
        {
            c.maxCount = maxCount;
        }
        ui.DragFloat("Faint Respawn Delay", c.faintRespawnDelay, 0.5f, 1.0f, 120.0f);
        int lo = static_cast<int>(c.levelMin);
        int hi = static_cast<int>(c.levelMax);
        if(ui.SliderInt("Level Min", lo, 1, 100))
        {
            c.levelMin = lo;
        }
        if(ui.SliderInt("Level Max", hi, 1, 100))
        {
            c.levelMax = hi;
        }
        ui.EntityField("Escape Area", c.escapeArea);
    }

    void DrawWildEscape(WildEscapeArea &c, fg::FriComponentInspector &ui)
    {
        ui.DragFloat("Radius", c.radius, 0.1f, 0.5f, 50.0f);
    }

    void DrawPlayerTag(PlayerTag &, fg::FriComponentInspector &ui)
    {
        ui.TextDisabled("Marks the unique player entity");
    }

    void DrawTeam(PokemonTeam &c, fg::FriComponentInspector &ui)
    {
        int team = static_cast<int>(c.team);
        if(ui.SliderInt("Team", team, 0, 1))
        {
            c.team = team;
        }
        ui.TextDisabled("0 player, 1 wild");
    }
} // namespace

FRI_MODULE(module)
{
    module.Component<PokemonIdentity>("PokemonIdentity", "Pokemon Identity", DrawIdentity)
          .Component<PokemonIVs>("PokemonIVs", "Pokemon IVs", DrawIVs)
          .Component<PokemonStats>("PokemonStats", "Pokemon Stats", DrawStats)
          .Component<PokemonTypes>("PokemonTypes", "Pokemon Types", DrawTypes)
          .Component<PokemonVitals>("PokemonVitals", "Pokemon Vitals", DrawVitals)
          .Component<PokemonMoveset>("PokemonMoveset", "Pokemon Moveset", DrawMoveset)
          .Component<PokemonInventory>("PokemonInventory", "Pokemon Inventory", DrawInventory)
          .Component<PokemonCombatState>("PokemonCombatState", "Pokemon Combat State")
          .Component<PokemonStatus>("PokemonStatus", "Pokemon Status")
          .Component<PokemonProjectile>("PokemonProjectile", "Pokemon Projectile")
          .Component<PokemonTeam>("PokemonTeam", "Pokemon Team", DrawTeam)
          .Component<PlayerTag>("PlayerTag", "Player Tag", DrawPlayerTag)
          .Component<WildPokemonAI>("WildPokemonAI", "Wild Pokemon AI")
          .Component<WildSpawnArea>("WildSpawnArea", "Wild Spawn Area", DrawWildSpawn)
          .Component<WildEscapeArea>("WildEscapeArea", "Wild Escape Area", DrawWildEscape)
          .System<PokemonStatsSystem>()
          .System<StaminaRegenSystem>()
          .System<StatusEffectSystem>()
          .System<WildSpawnSystem>()
          .System<WildAISystem>()
          .System<MoveInputSystem>()
          .System<MoveCombatSystem>()
          .System<ProjectileSystem>()
          .System<ItemUseSystem>()
          .System<PokemonHudSystem>()
          .System<PlayerRespawnSystem>();
}
