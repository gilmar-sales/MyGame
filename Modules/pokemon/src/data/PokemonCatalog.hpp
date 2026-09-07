#pragma once

#include "data/ElementType.hpp"

#include <cstddef>
#include <string_view>

struct MoveDef
{
    std::string_view id;
    ElementType      element   = ElementType::Normal;
    MoveCategory     category  = MoveCategory::Physical;
    MoveDelivery     delivery  = MoveDelivery::Melee;
    float            power     = 0.0f;
    float            stamina   = 10.0f;
    float            cooldown  = 0.5f;
    float            range     = 1.5f;
    float            radius    = 0.6f;
    float            chargeSec = 0.0f;
    float            statusDuration = 0.0f;
    float            statusDps = 0.0f;
    float            healRatio = 0.0f; // portion of status dps healed to caster
    /// Dash toward facing before the hit resolves (melee body blows).
    float            lungeDistance   = 0.0f;
    float            lungeDuration   = 0.0f;
    /// After hit: pull attacker back (tackle).
    float            recoilDistance  = 0.0f;
    float            recoilDuration  = 0.0f;
    /// Physics charge (headbutt): drive CharacterController velocity into the target.
    /// Knockback on hit is left entirely to the physics contact response.
    float            chargeSpeed       = 0.0f;
    /// Homing/ballistic projectiles (razor leaf burst / leech seed orb).
    int              projectileCount = 0;
    float            projectileSpeed = 0.0f;
    float            projectileLife  = 0.0f;
    float            projectileRadius = 0.15f;
    float            projectileSpread = 0.0f; // radians cone half-angle
    float            projectileScale  = 0.12f;
    std::string_view animClip;
};

struct SpeciesDef
{
    std::string_view id;
    ElementType      primary   = ElementType::Normal;
    ElementType      secondary = ElementType::None;
    int              baseHp    = 45;
    int              baseAtk   = 45;
    int              baseDef   = 45;
    int              baseSpAtk = 45;
    int              baseSpDef = 45;
    int              baseSpeed = 45;
    float            baseStamina = 100.0f;
    /// 0 aggressive, 1 peaceful, 2 skittish, 3 cowardly
    int              defaultPersonality = 1;
};

[[nodiscard]] const MoveDef *FindMove(std::string_view id);
[[nodiscard]] const SpeciesDef *FindSpecies(std::string_view id);

[[nodiscard]] inline std::size_t MoveCatalogSize();
[[nodiscard]] const MoveDef *MoveAt(std::size_t index);
