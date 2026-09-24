#include "systems/MoveCombatSystem.hpp"

#include "components/PokemonComponents.hpp"
#include "data/PokemonCatalog.hpp"
#include "systems/PokemonCombatUtil.hpp"
#include "systems/PokemonProjectileUtil.hpp"

#include <Frigga/ECS/Components/TransformComponent.hpp>
#include <Frigga/ECS/TransformUtil.hpp>

#include <algorithm>
#include <utility>
#include <vector>

MoveCombatSystem::MoveCombatSystem(const skr::Arc<fr::Registry> &registry,
                                   const skr::Arc<fg::Physics> &physics,
                                   const skr::Arc<fg::AnimationController> &animation,
                                   const skr::Arc<fg::PrimitiveMeshFactory> &primitives)
    : fr::System(registry), mPhysics(physics), mAnimation(animation), mPrimitives(primitives)
{
}

void MoveCombatSystem::Update(float deltaTime)
{
    std::vector<std::pair<fr::Entity, glm::vec3>> candidates;
    mRegistry->CreateMutation()->Each(
        [&](fr::Entity entity, PokemonVitals &vitals, fg::TransformComponent &) {
            if(vitals.knockedOut)
            {
                return;
            }
            candidates.emplace_back(entity,
                                    fg::TransformUtil::GetWorldPose(*mRegistry, entity).position);
        });

    // Physics contacts from the last fixed step (caster CharacterVirtual vs target presence RB).
    const std::vector<fg::PhysicsContactEvent> contacts =
        mPhysics ? mPhysics->DrainContactEvents() : std::vector<fg::PhysicsContactEvent> {};

    struct PendingSpawn
    {
        fr::Entity owner = fr::NullEntity;
        const MoveDef *def = nullptr;
        glm::vec3 origin {};
        glm::vec3 forward {};
    };
    std::vector<PendingSpawn> pendingSpawns;

    mRegistry->CreateMutation()->Each(
        [&](fr::Entity entity, PokemonVitals &vitals, PokemonMoveset &moves,
            PokemonCombatState &combat) {
            if(vitals.knockedOut)
            {
                // Keep movement locked for the whole KO / respawn window.
                if(combat.savedMaxStrength >= 0.0f)
                {
                    PokemonCombat::RestorePhysicsCharge(*mRegistry, mPhysics, entity, combat);
                }
                PokemonCombat::SetLocomotionLocked(*mRegistry, entity, true);
                PokemonCombat::ZeroPlanarVelocity(mPhysics, entity);
                // Force faint even for deaths that bypassed ApplyDamage (Leech Seed
                // DoT) or raced a locomotion write on the KO frame. No-op when the
                // faint clip is already playing.
                PokemonCombat::EnsureKoAnim(*mRegistry, mAnimation, entity);
                combat.phase         = PokemonCombat::kPhaseIdle;
                combat.pendingSlot   = -1;
                combat.hitTarget     = -1;
                combat.damageApplied = false;
                combat.activeMove.clear();
                return;
            }

            if(combat.busyTimer > 0.0f)
            {
                combat.busyTimer = std::max(0.0f, combat.busyTimer - deltaTime);
            }

            if(combat.phase == PokemonCombat::kPhaseIdle)
            {
                return;
            }

            const MoveDef *def = FindMove(combat.activeMove);
            if(def == nullptr)
            {
                combat.phase       = PokemonCombat::kPhaseIdle;
                combat.pendingSlot = -1;
                combat.activeMove.clear();
                return;
            }

            const glm::vec3 forward {combat.forwardX, 0.0f, combat.forwardZ};
            const glm::vec3 origin {combat.originX, combat.originY, combat.originZ};

            auto finishAndCooldown = [&]() {
                if(def->chargeSpeed > 0.0f || combat.savedMaxStrength >= 0.0f)
                {
                    PokemonCombat::EndPhysicsCharge(*mRegistry, mPhysics, entity, combat);
                }
                else
                {
                    PokemonCombat::SetLocomotionLocked(*mRegistry, entity, false);
                }
                if(combat.pendingSlot >= 0 && combat.pendingSlot < 4)
                {
                    PokemonCombat::CooldownAt(moves, static_cast<int>(combat.pendingSlot)) =
                        def->cooldown;
                }
                combat.phase         = PokemonCombat::kPhaseIdle;
                combat.pendingSlot   = -1;
                combat.hitTarget     = -1;
                combat.damageApplied = false;
                combat.activeMove.clear();
                combat.busyTimer = 0.25f;
            };

            auto applyHitIfNeeded = [&](const glm::vec3 &probePos) -> bool {
                if(combat.damageApplied)
                {
                    return false;
                }
                const glm::vec3 probe =
                    probePos + glm::vec3 {0.0f, 0.6f, 0.0f} + forward * 0.35f;
                const fr::Entity target = PokemonCombat::FindTarget(
                    *mRegistry, mPhysics, entity, probe, forward, *def, candidates);
                if(target == fr::NullEntity)
                {
                    return false;
                }
                PokemonCombat::ApplyDamage(*mRegistry, mPhysics, mAnimation, entity, target, *def);
                combat.hitTarget     = static_cast<std::int64_t>(target);
                combat.damageApplied = true;
                return true;
            };

            // --- Charge (Solar Beam) ---
            if(combat.phase == PokemonCombat::kPhaseCharging)
            {
                if(combat.phaseTimer >= def->chargeSec - deltaTime * 0.5f)
                {
                    PokemonCombat::PlayMoveAnim(*mRegistry, mAnimation, entity, def->animClip);
                }
                combat.phaseTimer -= deltaTime;
                if(combat.phaseTimer > 0.0f)
                {
                    return;
                }
                PokemonCombat::ResolveHit(*mRegistry, mPhysics, mAnimation, entity, *def,
                                          candidates);
                finishAndCooldown();
                return;
            }

            // --- Lunge (Tackle scripted / Headbutt physics charge) ---
            if(combat.phase == PokemonCombat::kPhaseLunging)
            {
                if(combat.motionT <= 0.0f)
                {
                    PokemonCombat::PlayMoveAnim(*mRegistry, mAnimation, entity, def->animClip);
                    if(def->chargeSpeed > 0.0f)
                    {
                        PokemonCombat::BeginPhysicsCharge(*mRegistry, mPhysics, entity, combat);
                    }
                }

                // Physics charge: drive caster; hit + knockback only on real body contact.
                if(def->chargeSpeed > 0.0f && mPhysics)
                {
                    const float duration = std::max(def->lungeDuration, 0.05f);
                    combat.motionT =
                        std::min(1.0f, combat.motionT + deltaTime / duration);
                    combat.phaseTimer = std::max(0.0f, combat.phaseTimer - deltaTime);

                    const glm::vec3 pos =
                        fg::TransformUtil::GetWorldPose(*mRegistry, entity).position;
                    const float traveled =
                        glm::length(glm::vec3 {pos.x - origin.x, 0.0f, pos.z - origin.z});

                    glm::vec3 chargeVel = forward * def->chargeSpeed;
                    const glm::vec3 current = mPhysics->GetCharacterVelocity(entity);
                    chargeVel.y            = current.y;
                    mPhysics->MoveCharacter(entity, chargeVel);

                    if(!combat.damageApplied)
                    {
                        const fr::Entity target = PokemonCombat::FindHostileContactTarget(
                            *mRegistry, entity, contacts, candidates);
                        if(target != fr::NullEntity)
                        {
                            PokemonCombat::ApplyDamage(*mRegistry, mPhysics, mAnimation, entity,
                                                       target, *def, 1.0f,
                                                       /*applyKnockback=*/true);
                            combat.hitTarget     = static_cast<std::int64_t>(target);
                            combat.damageApplied = true;
                            finishAndCooldown();
                            return;
                        }
                    }

                    const bool timedOut =
                        combat.motionT >= 1.0f || combat.phaseTimer <= 0.0f ||
                        traveled >= def->lungeDistance;
                    if(timedOut)
                    {
                        finishAndCooldown();
                    }
                    return;
                }

                // Scripted tackle dash (teleport along facing).
                PokemonCombat::ZeroPlanarVelocity(mPhysics, entity);
                const float duration = std::max(def->lungeDuration, 0.05f);
                combat.motionT =
                    std::min(1.0f, combat.motionT + deltaTime / duration);
                const float     t    = PokemonCombat::SmoothStep(combat.motionT);
                const glm::vec3 pos  = origin + forward * (def->lungeDistance * t);
                PokemonCombat::SetEntityWorldPos(*mRegistry, mPhysics, entity, pos);
                applyHitIfNeeded(pos);

                if(combat.motionT < 1.0f && !combat.damageApplied)
                {
                    return;
                }

                applyHitIfNeeded(origin + forward * def->lungeDistance);

                combat.originX = pos.x;
                combat.originY = pos.y;
                combat.originZ = pos.z;
                combat.motionT = 0.0f;

                if(def->recoilDistance > 0.0f)
                {
                    combat.phase      = PokemonCombat::kPhaseRecoiling;
                    combat.phaseTimer = std::max(def->recoilDuration, 0.05f);
                    return;
                }

                finishAndCooldown();
                return;
            }

            // --- Recoil (Tackle bounce-back) ---
            if(combat.phase == PokemonCombat::kPhaseRecoiling)
            {
                PokemonCombat::ZeroPlanarVelocity(mPhysics, entity);
                const float duration = std::max(def->recoilDuration, 0.05f);
                combat.motionT =
                    std::min(1.0f, combat.motionT + deltaTime / duration);
                const float     t   = PokemonCombat::SmoothStep(combat.motionT);
                const glm::vec3 pos = origin - forward * (def->recoilDistance * t);
                PokemonCombat::SetEntityWorldPos(*mRegistry, mPhysics, entity, pos);
                if(combat.motionT < 1.0f)
                {
                    return;
                }
                finishAndCooldown();
                return;
            }

            // --- Projectile / status orb / instant ---
            if(combat.phase == PokemonCombat::kPhaseRecovering)
            {
                PokemonCombat::PlayMoveAnim(*mRegistry, mAnimation, entity, def->animClip);
                if(def->projectileCount > 0 && mPrimitives)
                {
                    pendingSpawns.push_back(
                        PendingSpawn {entity, def, origin, forward});
                }
                else
                {
                    PokemonCombat::ResolveHit(*mRegistry, mPhysics, mAnimation, entity, *def,
                                              candidates);
                }
                finishAndCooldown();
            }
        });

    for(const PendingSpawn &spawn : pendingSpawns)
    {
        if(spawn.def != nullptr)
        {
            PokemonProjectileUtil::SpawnForMove(*mRegistry, *mPrimitives, spawn.owner, *spawn.def,
                                                spawn.origin, spawn.forward);
        }
    }
}
