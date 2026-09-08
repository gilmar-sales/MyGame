#include "systems/PlayerRespawnSystem.hpp"

#include "components/PokemonComponents.hpp"
#include "systems/PokemonCombatUtil.hpp"

#include <Frigga/ECS/Components/NameComponent.hpp>
#include <Frigga/ECS/Components/TransformComponent.hpp>
#include <Frigga/ECS/TransformUtil.hpp>

namespace
{
    constexpr float kPlayerRespawnDelay = 2.0f;
}

PlayerRespawnSystem::PlayerRespawnSystem(const skr::Arc<fr::Registry> &registry,
                                         const skr::Arc<fg::Physics> &physics,
                                         const skr::Arc<fg::AnimationController> &animation)
    : fr::System(registry), mPhysics(physics), mAnimation(animation)
{
}

void PlayerRespawnSystem::CaptureSpawn(fr::Entity player)
{
    if(mHasSpawn || !mRegistry->HasComponent<fg::TransformComponent>(player))
    {
        return;
    }
    const auto pose = fg::TransformUtil::WorldPose(*mRegistry, player);
    mSpawnPos       = pose.position;
    mSpawnRot       = pose.rotation;
    mHasSpawn       = true;
}

void PlayerRespawnSystem::Respawn(fr::Entity player)
{
    if(mRegistry->HasComponent<PokemonVitals>(player))
    {
        mRegistry->TryGetComponents<PokemonVitals>(player, [&](PokemonVitals &vitals) {
            vitals.knockedOut = false;
            vitals.hp         = vitals.maxHp;
            vitals.stamina    = vitals.maxStamina;
        });
    }

    if(mRegistry->HasComponent<PokemonCombatState>(player))
    {
        mRegistry->TryGetComponents<PokemonCombatState>(player, [&](PokemonCombatState &combat) {
            combat.phase         = PokemonCombat::kPhaseIdle;
            combat.pendingSlot   = -1;
            combat.hitTarget     = -1;
            combat.damageApplied = false;
            combat.phaseTimer    = 0.0f;
            combat.busyTimer     = 0.0f;
            combat.motionT       = 0.0f;
            combat.koRespawnTimer = 0.0f;
            combat.activeMove.clear();
            if(combat.savedMaxStrength >= 0.0f)
            {
                PokemonCombat::EndPhysicsCharge(*mRegistry, mPhysics, player, combat);
            }
            else
            {
                PokemonCombat::SetLocomotionLocked(*mRegistry, player, false);
            }
        });
    }

    if(mRegistry->HasComponent<PokemonStatus>(player))
    {
        mRegistry->TryGetComponents<PokemonStatus>(player, [&](PokemonStatus &status) {
            status.leechSeeded       = false;
            status.leechSeedTimer    = 0.0f;
            status.leechSeedTick     = 0.0f;
            status.leechSeedSource   = -1;
            status.leechSeeding      = false;
            status.leechSeedingTimer = 0.0f;
            status.leechSeedTarget   = -1;
        });
    }

    if(mRegistry->HasComponent<PokemonMoveset>(player))
    {
        mRegistry->TryGetComponents<PokemonMoveset>(player, [&](PokemonMoveset &moves) {
            moves.cd0 = 0.0f;
            moves.cd1 = 0.0f;
            moves.cd2 = 0.0f;
            moves.cd3 = 0.0f;
        });
    }

    PokemonCombat::SetEntityWorldPos(*mRegistry, mPhysics, player, mSpawnPos);
    if(mRegistry->HasComponent<fg::TransformComponent>(player))
    {
        fg::TransformUtil::SetWorldPose(*mRegistry, player, mSpawnPos, mSpawnRot);
    }
    if(mPhysics)
    {
        mPhysics->SetCharacterFacing(player, mSpawnRot);
        PokemonCombat::ZeroPlanarVelocity(mPhysics, player);
    }

    PokemonCombat::PlayMoveAnim(*mRegistry, mAnimation, player, "001aidle");
}

void PlayerRespawnSystem::Update(float deltaTime)
{
    mRegistry->CreateMutation()->Each(
        [&](fr::Entity entity, fg::NameComponent &name, PokemonVitals &vitals,
            PokemonCombatState &combat, fg::TransformComponent &) {
            if(name.name != "Player")
            {
                return;
            }

            if(!vitals.knockedOut)
            {
                CaptureSpawn(entity);
                combat.koRespawnTimer = 0.0f;
                return;
            }

            // Stay frozen on the ground until respawn.
            PokemonCombat::SetLocomotionLocked(*mRegistry, entity, true);
            PokemonCombat::ZeroPlanarVelocity(mPhysics, entity);

            if(combat.koRespawnTimer <= 0.0f)
            {
                combat.koRespawnTimer = kPlayerRespawnDelay;
                return;
            }

            combat.koRespawnTimer -= deltaTime;
            if(combat.koRespawnTimer > 0.0f)
            {
                return;
            }

            Respawn(entity);
        });
}
