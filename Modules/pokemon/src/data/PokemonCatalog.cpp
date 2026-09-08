#include "data/PokemonCatalog.hpp"

#include <array>
#include <cstring>

namespace
{
    constexpr std::array<MoveDef, 5> kMoves {{
        MoveDef {.id        = "tackle",
                 .element   = ElementType::Normal,
                 .category  = MoveCategory::Physical,
                 .delivery  = MoveDelivery::Melee,
                 .power     = 22.0f,
                 .stamina   = 10.0f,
                 .cooldown  = 0.95f,
                 .range     = 1.7f,
                 .radius    = 0.7f,
                 .lungeDistance  = 1.15f,
                 .lungeDuration  = 0.14f,
                 .recoilDistance = 0.45f,
                 .recoilDuration = 0.12f,
                 .animClip  = "001fight_b"},
        MoveDef {.id        = "headbutt",
                 .element   = ElementType::Normal,
                 .category  = MoveCategory::Physical,
                 .delivery  = MoveDelivery::Melee,
                 .power     = 36.0f,
                 .stamina   = 26.0f,
                 .cooldown  = 1.45f,
                 .range     = 2.1f,
                 .radius    = 0.85f,
                 .lungeDistance    = 2.5f,
                 .lungeDuration    = 0.34f,
                 .chargeSpeed      = 10.0f,
                 .animClip  = "001fight_b"},
        MoveDef {.id        = "razor_leaf",
                 .element   = ElementType::Grass,
                 .category  = MoveCategory::Special,
                 .delivery  = MoveDelivery::Projectile,
                 .power     = 42.0f,
                 .stamina   = 18.0f,
                 .cooldown  = 1.0f,
                 .range     = 10.0f,
                 .radius    = 0.35f,
                 .projectileCount  = 9,
                 .projectileSpeed  = 14.0f,
                 .projectileLife   = 0.85f,
                 .projectileRadius = 0.18f,
                 .projectileSpread = 0.28f,
                 .projectileScale  = 0.1f,
                 .animClip  = "001fight_d"},
        MoveDef {.id        = "leech_seed",
                 .element   = ElementType::Grass,
                 .category  = MoveCategory::Status,
                 .delivery  = MoveDelivery::StatusRanged,
                 .power     = 0.0f,
                 .stamina   = 25.0f,
                 .cooldown  = 1.2f,
                 .range     = 9.0f,
                 .radius    = 0.4f,
                 .statusDuration   = 8.0f,
                 .statusDps        = 3.0f,
                 .healRatio        = 1.0f,
                 .projectileCount  = 1,
                 .projectileSpeed  = 9.0f,
                 .projectileLife   = 1.4f,
                 .projectileRadius = 0.28f,
                 .projectileSpread = 0.0f,
                 .projectileScale  = 0.28f,
                 .animClip  = "001fight_d"},
        MoveDef {.id        = "solar_beam",
                 .element   = ElementType::Grass,
                 .category  = MoveCategory::Special,
                 .delivery  = MoveDelivery::Beam,
                 .power     = 95.0f,
                 .stamina   = 45.0f,
                 .cooldown  = 2.8f,
                 .range     = 12.0f,
                 .radius    = 0.45f,
                 .chargeSec = 0.8f,
                 .animClip  = "001fight_d"},
    }};

    constexpr SpeciesDef kBulbasaur {
        .id         = "bulbasaur",
        .primary    = ElementType::Grass,
        .secondary  = ElementType::Poison,
        .baseHp     = 45,
        .baseAtk    = 49,
        .baseDef    = 49,
        .baseSpAtk  = 65,
        .baseSpDef  = 65,
        .baseSpeed  = 45,
        .baseStamina = 100.0f,
        .defaultPersonality = 1,
    };

    constexpr SpeciesDef kTrainingDummy {
        .id         = "training_dummy",
        .primary    = ElementType::Fire,
        .secondary  = ElementType::None,
        .baseHp     = 80,
        .baseAtk    = 30,
        .baseDef    = 40,
        .baseSpAtk  = 30,
        .baseSpDef  = 40,
        .baseSpeed  = 20,
        .baseStamina = 50.0f,
        .defaultPersonality = 1,
    };

    constexpr std::array<SpeciesDef, 2> kSpecies {{kBulbasaur, kTrainingDummy}};
} // namespace

const MoveDef *FindMove(std::string_view id)
{
    for(const MoveDef &move : kMoves)
    {
        if(move.id == id)
        {
            return &move;
        }
    }
    return nullptr;
}

const SpeciesDef *FindSpecies(std::string_view id)
{
    for(const SpeciesDef &species : kSpecies)
    {
        if(species.id == id)
        {
            return &species;
        }
    }
    return nullptr;
}

std::size_t MoveCatalogSize()
{
    return kMoves.size();
}

const MoveDef *MoveAt(std::size_t index)
{
    if(index >= kMoves.size())
    {
        return nullptr;
    }
    return &kMoves[index];
}
