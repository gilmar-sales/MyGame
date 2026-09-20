#pragma once

#include <Frigga/ECS/Components/EntityRef.hpp>
#include <Freyr/Freyr.hpp>

#include <cstdint>
#include <string>

struct PokemonIdentity: fr::Component
{
    std::string speciesId = "bulbasaur";
    std::string nickname;
    std::int64_t level = 10;
};

struct PokemonIVs: fr::Component
{
    std::int64_t hp        = 15;
    std::int64_t attack    = 15;
    std::int64_t defense   = 15;
    std::int64_t spAttack  = 15;
    std::int64_t spDefense = 15;
    std::int64_t speed     = 15;
};

struct PokemonStats: fr::Component
{
    std::int64_t hp        = 0;
    std::int64_t attack    = 0;
    std::int64_t defense   = 0;
    std::int64_t spAttack  = 0;
    std::int64_t spDefense = 0;
    std::int64_t speed     = 0;
    /// When true, PokemonStatsSystem recalculates from identity/IVs/species.
    bool dirty = true;
};

struct PokemonTypes: fr::Component
{
    /// See ElementType (Grass=3, Poison=7, Fire=1, Water=2, None=-1).
    std::int64_t primary   = 3;
    std::int64_t secondary = 7;
};

struct PokemonVitals: fr::Component
{
    float hp                 = 0.0f;
    float maxHp              = 0.0f;
    float stamina            = 0.0f;
    float maxStamina         = 100.0f;
    float staminaRegenPerSec = 10.0f;
    bool  initialized        = false;
    bool  knockedOut         = false;
};

struct PokemonMoveset: fr::Component
{
    std::string move0 = "tackle";
    std::string move1 = "headbutt";
    std::string move2 = "razor_leaf";
    std::string move3 = "leech_seed";
    float       cd0   = 0.0f;
    float       cd1   = 0.0f;
    float       cd2   = 0.0f;
    float       cd3   = 0.0f;
};

struct PokemonInventory: fr::Component
{
    std::int64_t potions        = 3;
    std::int64_t oranBerries    = 2;
    std::int64_t staminaBerries = 3;
};

/// phase: 0 idle, 1 charging, 2 recovering, 3 lunging, 4 recoiling
struct PokemonCombatState: fr::Component
{
    std::int64_t phase       = 0;
    std::string  activeMove;
    float        phaseTimer  = 0.0f;
    float        busyTimer   = 0.0f;
    std::int64_t pendingSlot = -1;

    float        forwardX    = 0.0f;
    float        forwardZ    = 1.0f;
    float        originX     = 0.0f;
    float        originY     = 0.0f;
    float        originZ     = 0.0f;
    float        motionT     = 0.0f;
    std::int64_t hitTarget   = -1;
    bool         damageApplied = false;
    /// Sentinel while a physics charge is active (cleanup / interrupt bookkeeping).
    float        savedMaxStrength = -1.0f;
    /// Player-only: countdown after KO before respawn (0 = not counting).
    float        koRespawnTimer = 0.0f;
};

struct PokemonStatus: fr::Component
{
    bool         leechSeeded     = false;
    float        leechSeedTimer  = 0.0f;
    float        leechSeedTick   = 0.0f;
    float        leechSeedDps    = 0.0f;
    float        leechSeedHeal   = 1.0f;
    std::int64_t leechSeedSource = -1;

    /// Caster side of an active Leech Seed link.
    bool         leechSeeding      = false;
    float        leechSeedingTimer = 0.0f;
    std::int64_t leechSeedTarget   = -1;

    /// Headbutt (and similar): brief lock so physics repulsion is not overridden by loco/AI.
    float        stunTimer = 0.0f;
};

/// Runtime damaging / status orbs (razor leaf flakes, leech seed).
struct PokemonProjectile: fr::Component
{
    std::int64_t owner       = -1;
    std::string  moveId;
    float        velX        = 0.0f;
    float        velY        = 0.0f;
    float        velZ        = 0.0f;
    float        life        = 1.0f;
    float        radius      = 0.2f;
    float        damageScale = 1.0f;
    /// 0 = razor leaf flake, 1 = leech seed orb
    std::int64_t kind        = 0;
    bool         consumed    = false;
};

/// 0 = player, 1 = wild (same-team entities do not target each other).
struct PokemonTeam: fr::Component
{
    std::int64_t team = 0;
};

/// Marks the unique controllable player entity (WildAI and future systems).
struct PlayerTag: fr::Component
{
};

/// Personality: 0 aggressive, 1 peaceful, 2 skittish, 3 cowardly
/// AI state: 0 passive, 1 combat, 2 fleeing, 3 fainted
struct WildPokemonAI: fr::Component
{
    std::int64_t spawnArea   = -1;
    std::int64_t personality = 1;
    std::int64_t state       = 0;
    std::int64_t target      = -1;
    std::int64_t lastAttacker = -1;
    float        wanderTimer = 0.0f;
    float        wanderDirX  = 1.0f;
    float        wanderDirZ  = 0.0f;
    float        faintTimer  = 0.0f;
    float        homeX       = 0.0f;
    float        homeZ       = 0.0f;
    float        alertRadius = 5.0f;
    float        spawnRadius = 8.0f;
    float        moveSpeed   = 2.2f;
    /// Last locomotion clip name (avoids restarting CrossFade every frame).
    std::string  locoClip;
    /// Cached animator entity for loco CrossFade (-1 = unresolved).
    std::int64_t animatorEntity = -1;
    /// Last applied planar facing (unit XZ); skip SetCharacterFacing when unchanged.
    float        faceDirX = 0.0f;
    float        faceDirZ = 0.0f;
};

struct WildSpawnArea: fr::Component
{
    std::string  speciesId         = "bulbasaur";
    /// 0 aggressive, 1 peaceful, 2 skittish, 3 cowardly
    std::int64_t personality       = 1;
    float        spawnRadius       = 8.0f;
    std::int64_t maxCount          = 8;
    float        faintRespawnDelay = 30.0f;
    float        alertRadius       = 5.0f;
    std::int64_t levelMin          = 8;
    std::int64_t levelMax          = 12;
    fg::EntityRef escapeArea {};
    /// Runtime bookkeeping so deferred CreateEntity cannot overshoot maxCount.
    std::int64_t pendingSpawns     = 0;
    std::int64_t lastLiveCount     = 0;
};

struct WildEscapeArea: fr::Component
{
    float radius = 3.0f;
};
