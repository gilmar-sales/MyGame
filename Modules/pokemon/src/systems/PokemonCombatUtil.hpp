#pragma once

#include "components/PokemonComponents.hpp"
#include "data/ElementType.hpp"
#include "data/PokemonCatalog.hpp"
#include "data/StatFormulas.hpp"
#include "data/TypeChart.hpp"
#include "data/WildTypes.hpp"

#include <CharacterControllerComponent.hpp>

#include <Frigga/Animation/AnimationController.hpp>
#include <Frigga/ECS/Components/AnimatorComponent.hpp>
#include <Frigga/ECS/Components/HierarchyComponent.hpp>
#include <Frigga/ECS/Components/NameComponent.hpp>
#include <Frigga/ECS/Components/RigidBodyComponent.hpp>
#include <Frigga/ECS/Components/TransformComponent.hpp>
#include <Frigga/ECS/TransformUtil.hpp>
#include <Frigga/Physics/Physics.hpp>

#include <Freyr/Freyr.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cmath>
#include <random>
#include <string>
#include <string_view>
#include <vector>

namespace PokemonCombat
{
    inline constexpr std::int64_t kPhaseIdle       = 0;
    inline constexpr std::int64_t kPhaseCharging   = 1;
    inline constexpr std::int64_t kPhaseRecovering = 2;
    inline constexpr std::int64_t kPhaseLunging    = 3;
    inline constexpr std::int64_t kPhaseRecoiling  = 4;

    [[nodiscard]] inline float SmoothStep(float t)
    {
        t = std::clamp(t, 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    }

    inline void SetEntityWorldPos(fr::Registry &registry, const skr::Arc<fg::Physics> &physics,
                                  fr::Entity entity, const glm::vec3 &position)
    {
        glm::quat rotation {1.0f, 0.0f, 0.0f, 0.0f};
        if(registry.HasComponent<fg::TransformComponent>(entity))
        {
            rotation = fg::TransformUtil::WorldPose(registry, entity).rotation;
            fg::TransformUtil::SetWorldPose(registry, entity, position, rotation);
        }

        if(!physics)
        {
            return;
        }

        // Character controllers ignore SetTransform; teleport handles that path.
        physics->TeleportCharacter(entity, position);
        physics->SetLinearVelocity(entity, {0.0f, 0.0f, 0.0f});
        // Rigid-body dummies / props: snap the physics body too.
        physics->SetKinematicPose(entity, position, rotation);
    }

    inline void ZeroPlanarVelocity(const skr::Arc<fg::Physics> &physics, fr::Entity entity)
    {
        if(!physics)
        {
            return;
        }
        const glm::vec3 v = physics->GetCharacterVelocity(entity);
        physics->MoveCharacter(entity, {0.0f, v.y, 0.0f});
        physics->SetLinearVelocity(entity, {0.0f, 0.0f, 0.0f});
        physics->SetAngularVelocity(entity, {});
    }

    inline void SetLocomotionLocked(fr::Registry &registry, fr::Entity entity, bool locked)
    {
        if(!registry.HasComponent<CharacterControllerComponent>(entity))
        {
            return;
        }
        registry.TryGetComponents<CharacterControllerComponent>(
            entity, [&](CharacterControllerComponent &cc) { cc.locomotionLocked = locked; });
    }

    inline void BeginPhysicsCharge(fr::Registry &registry, const skr::Arc<fg::Physics> &,
                                   fr::Entity entity, PokemonCombatState &combat)
    {
        SetLocomotionLocked(registry, entity, true);
        // Sentinel: still charging (used by MoveCombat / StatusEffect cleanup).
        if(combat.savedMaxStrength < 0.0f)
        {
            combat.savedMaxStrength = 1.0f;
        }
    }

    inline void RestorePhysicsCharge(fr::Registry &, const skr::Arc<fg::Physics> &physics,
                                     fr::Entity entity, PokemonCombatState &combat)
    {
        combat.savedMaxStrength = -1.0f;
        ZeroPlanarVelocity(physics, entity);
    }

    inline void EndPhysicsCharge(fr::Registry &registry, const skr::Arc<fg::Physics> &physics,
                                 fr::Entity entity, PokemonCombatState &combat)
    {
        SetLocomotionLocked(registry, entity, false);
        RestorePhysicsCharge(registry, physics, entity, combat);
    }

    [[nodiscard]] inline fr::Entity FindAnimator(fr::Registry &registry, fr::Entity root)
    {
        if(registry.HasComponent<fg::AnimatorComponent>(root))
        {
            return root;
        }
        std::vector<fr::Entity> queue;
        if(registry.HasComponent<fg::HierarchyComponent>(root))
        {
            registry.TryGetComponents<fg::HierarchyComponent>(
                root, [&](fg::HierarchyComponent &hierarchy) {
                    queue.insert(queue.end(), hierarchy.children.begin(),
                                 hierarchy.children.end());
                });
        }
        for(std::size_t i = 0; i < queue.size(); ++i)
        {
            const fr::Entity node = queue[i];
            if(registry.HasComponent<fg::AnimatorComponent>(node))
            {
                return node;
            }
            if(!registry.HasComponent<fg::HierarchyComponent>(node))
            {
                continue;
            }
            registry.TryGetComponents<fg::HierarchyComponent>(
                node, [&](fg::HierarchyComponent &hierarchy) {
                    queue.insert(queue.end(), hierarchy.children.begin(),
                                 hierarchy.children.end());
                });
        }
        return fg::kInvalidEntity;
    }

    [[nodiscard]] inline std::string &MoveIdAt(PokemonMoveset &moves, int slot)
    {
        switch(slot)
        {
        case 1:
            return moves.move1;
        case 2:
            return moves.move2;
        case 3:
            return moves.move3;
        default:
            return moves.move0;
        }
    }

    [[nodiscard]] inline float &CooldownAt(PokemonMoveset &moves, int slot)
    {
        switch(slot)
        {
        case 1:
            return moves.cd1;
        case 2:
            return moves.cd2;
        case 3:
            return moves.cd3;
        default:
            return moves.cd0;
        }
    }

    [[nodiscard]] inline glm::vec3 ForwardFlat(const glm::quat &rotation)
    {
        // Match CharacterMovement facing: mesh forward is +Z after quatLookAt(-moveDir).
        const glm::vec3 forward = rotation * glm::vec3 {0.0f, 0.0f, 1.0f};
        glm::vec3       flat {forward.x, 0.0f, forward.z};
        if(glm::dot(flat, flat) < 1e-8f)
        {
            return {0.0f, 0.0f, 1.0f};
        }
        return glm::normalize(flat);
    }

    [[nodiscard]] inline float RandomFactor()
    {
        thread_local std::mt19937 rng {std::random_device {}()};
        std::uniform_real_distribution<float> dist(0.85f, 1.0f);
        return dist(rng);
    }

    inline void PlayMoveAnim(fr::Registry &registry,
                             const skr::Arc<fg::AnimationController> &animation, fr::Entity root,
                             std::string_view clip)
    {
        if(!animation || clip.empty())
        {
            return;
        }
        const fr::Entity animator = FindAnimator(registry, root);
        if(animator != fg::kInvalidEntity)
        {
            animation->CrossFade(animator, clip, 0.08f);
            // Loco / fight clips loop; KO explicitly disables loop in PlayKoAnim.
            animation->SetLoop(animator, true);
        }
    }

    inline void PlayKoAnim(fr::Registry &registry,
                           const skr::Arc<fg::AnimationController> &animation, fr::Entity root)
    {
        if(!animation)
        {
            return;
        }
        const fr::Entity animator = FindAnimator(registry, root);
        if(animator == fg::kInvalidEntity)
        {
            return;
        }
        animation->CrossFade(animator, "001ko", 0.08f);
        animation->SetLoop(animator, false);
        // Stop CharacterMovement from replacing KO with idle/walk.
        SetLocomotionLocked(registry, root, true);
    }

    inline void ApplyLeechSeedLink(fr::Registry &registry, fr::Entity attacker, fr::Entity defender,
                                   const MoveDef &move)
    {
        if(!registry.HasComponent<PokemonStatus>(attacker) ||
           !registry.HasComponent<PokemonStatus>(defender))
        {
            return;
        }

        registry.TryGetComponents<PokemonStatus>(defender, [&](PokemonStatus &defStatus) {
            defStatus.leechSeeded     = true;
            defStatus.leechSeedTimer  = move.statusDuration;
            defStatus.leechSeedTick   = 0.0f;
            defStatus.leechSeedDps    = move.statusDps;
            defStatus.leechSeedHeal   = move.healRatio;
            defStatus.leechSeedSource = static_cast<std::int64_t>(attacker);
        });

        registry.TryGetComponents<PokemonStatus>(attacker, [&](PokemonStatus &atkStatus) {
            atkStatus.leechSeeding      = true;
            atkStatus.leechSeedingTimer = move.statusDuration;
            atkStatus.leechSeedTarget   = static_cast<std::int64_t>(defender);
        });
    }

    [[nodiscard]] inline bool HasTeam(fr::Registry &registry, fr::Entity entity)
    {
        return registry.HasComponent<PokemonTeam>(entity);
    }

    [[nodiscard]] inline std::int64_t TeamOf(fr::Registry &registry, fr::Entity entity)
    {
        std::int64_t team = PokemonTeamId::kPlayer;
        if(registry.HasComponent<PokemonTeam>(entity))
        {
            registry.TryGetComponents<PokemonTeam>(entity,
                                                   [&](PokemonTeam &t) { team = t.team; });
        }
        return team;
    }

    [[nodiscard]] inline bool IsHostile(fr::Registry &registry, fr::Entity a, fr::Entity b)
    {
        // Unmarked entities (e.g. TrainingDummy) remain valid targets for everyone.
        if(!HasTeam(registry, a) || !HasTeam(registry, b))
        {
            return true;
        }
        return TeamOf(registry, a) != TeamOf(registry, b);
    }

    inline void NoteAttacker(fr::Registry &registry, fr::Entity attacker, fr::Entity defender)
    {
        if(!registry.HasComponent<WildPokemonAI>(defender))
        {
            return;
        }
        registry.TryGetComponents<WildPokemonAI>(defender, [&](WildPokemonAI &ai) {
            ai.lastAttacker = static_cast<std::int64_t>(attacker);
        });
    }

    [[nodiscard]] inline bool IsStunned(fr::Registry &registry, fr::Entity entity)
    {
        bool stunned = false;
        if(registry.HasComponent<PokemonStatus>(entity))
        {
            registry.TryGetComponents<PokemonStatus>(
                entity, [&](PokemonStatus &status) { stunned = status.stunTimer > 0.0f; });
        }
        return stunned;
    }

    inline void ApplyStun(fr::Registry &registry, const skr::Arc<fg::Physics> &physics,
                          fr::Entity entity, float duration)
    {
        if(duration <= 0.0f || !registry.HasComponent<PokemonStatus>(entity))
        {
            return;
        }

        registry.TryGetComponents<PokemonStatus>(entity, [&](PokemonStatus &status) {
            status.stunTimer = std::max(status.stunTimer, duration);
        });
        SetLocomotionLocked(registry, entity, true);

        // Interrupt any in-progress move so AI/player control does not resume mid-knockback.
        if(registry.HasComponent<PokemonCombatState>(entity))
        {
            registry.TryGetComponents<PokemonCombatState>(
                entity, [&](PokemonCombatState &combat) {
                    if(combat.savedMaxStrength >= 0.0f)
                    {
                        RestorePhysicsCharge(registry, physics, entity, combat);
                    }
                    combat.phase         = kPhaseIdle;
                    combat.pendingSlot   = -1;
                    combat.hitTarget     = -1;
                    combat.damageApplied = false;
                    combat.phaseTimer    = 0.0f;
                    combat.busyTimer     = 0.0f;
                    combat.motionT       = 0.0f;
                    combat.activeMove.clear();
                });
        }
    }

    inline void ApplyKnockback(const skr::Arc<fg::Physics> &physics, fr::Entity defender,
                               const glm::vec3 &fromAttacker, float speed)
    {
        if(!physics || speed <= 0.0f)
        {
            return;
        }
        glm::vec3 dir {fromAttacker.x, 0.0f, fromAttacker.z};
        const float len = glm::length(dir);
        if(len < 1e-4f)
        {
            dir = {0.0f, 0.0f, 1.0f};
        }
        else
        {
            dir /= len;
        }

        float mass = 70.0f;
        // Impulse ≈ mass * desired planar Δv so AddImpulse works for Dynamic characters.
        physics->AddImpulse(defender, {dir.x * speed * mass, 0.0f, dir.z * speed * mass});
    }

    /// Starts a move slot if idle, off cooldown, and has stamina. Returns true on success.
    inline bool TryStartMove(fr::Registry &registry, fr::Entity entity, PokemonVitals &vitals,
                             PokemonMoveset &moves, PokemonCombatState &combat, int slot,
                             const glm::vec3 &forwardOverride = {})
    {
        if(vitals.knockedOut || combat.phase != kPhaseIdle || combat.busyTimer > 0.0f ||
           IsStunned(registry, entity))
        {
            return false;
        }
        if(slot < 0 || slot > 3)
        {
            return false;
        }
        const std::string &moveId = MoveIdAt(moves, slot);
        const MoveDef *    def    = FindMove(moveId);
        if(def == nullptr || CooldownAt(moves, slot) > 0.0f || vitals.stamina < def->stamina)
        {
            return false;
        }

        const auto pose = fg::TransformUtil::WorldPose(registry, entity);
        glm::vec3  forward =
            glm::dot(forwardOverride, forwardOverride) > 1e-6f ? forwardOverride
                                                               : ForwardFlat(pose.rotation);
        if(glm::dot(forward, forward) < 1e-6f)
        {
            forward = {0.0f, 0.0f, 1.0f};
        }
        else
        {
            forward = glm::normalize(glm::vec3 {forward.x, 0.0f, forward.z});
        }

        combat.pendingSlot   = slot;
        combat.activeMove    = moveId;
        combat.hitTarget     = -1;
        combat.damageApplied = false;
        combat.motionT       = 0.0f;
        combat.forwardX      = forward.x;
        combat.forwardZ      = forward.z;
        combat.originX       = pose.position.x;
        combat.originY       = pose.position.y;
        combat.originZ       = pose.position.z;
        vitals.stamina -= def->stamina;
        // Keep CharacterMovement from stomping fight clips / fighting move motion.
        SetLocomotionLocked(registry, entity, true);

        if(def->chargeSec > 0.0f)
        {
            combat.phase      = kPhaseCharging;
            combat.phaseTimer = def->chargeSec;
        }
        else if(def->lungeDistance > 0.0f)
        {
            combat.phase      = kPhaseLunging;
            combat.phaseTimer = std::max(def->lungeDuration, 0.05f);
        }
        else
        {
            combat.phase      = kPhaseRecovering;
            combat.phaseTimer = 0.0f;
            combat.busyTimer  = 0.25f;
        }
        return true;
    }

    [[nodiscard]] inline float PresenceRadius(fr::Registry &registry, fr::Entity entity)
    {
        float radius = 0.5f;
        if(registry.HasComponent<fg::RigidBodyComponent>(entity))
        {
            registry.TryGetComponents<fg::RigidBodyComponent>(
                entity, [&](fg::RigidBodyComponent &rb) {
                    radius = std::max(rb.radius, 0.05f);
                    if(rb.shape == fg::ColliderShape::Box)
                    {
                        radius = std::max({rb.halfExtents.x, rb.halfExtents.z, radius});
                    }
                });
        }
        return radius;
    }

    /// Prefer physics contact events; fall back to linked RigidBody capsule overlap (CC vs CC).
    [[nodiscard]] inline fr::Entity FindHostileContactTarget(
        fr::Registry &registry, fr::Entity caster,
        const std::vector<fg::PhysicsContactEvent> &contacts,
        const std::vector<std::pair<fr::Entity, glm::vec3>> &candidates = {})
    {
        for(const auto &contact : contacts)
        {
            fr::Entity other = fg::kInvalidEntity;
            if(contact.entityA == static_cast<std::uint64_t>(caster))
            {
                other = static_cast<fr::Entity>(contact.entityB);
            }
            else if(contact.entityB == static_cast<std::uint64_t>(caster))
            {
                other = static_cast<fr::Entity>(contact.entityA);
            }
            else
            {
                continue;
            }

            if(other == fg::kInvalidEntity || other == caster)
            {
                continue;
            }
            if(!registry.HasComponent<PokemonVitals>(other) || !IsHostile(registry, caster, other))
            {
                continue;
            }

            bool ko = false;
            registry.TryGetComponents<PokemonVitals>(other,
                                                     [&](PokemonVitals &v) { ko = v.knockedOut; });
            if(!ko)
            {
                return other;
            }
        }

        if(candidates.empty() || !registry.HasComponent<fg::TransformComponent>(caster))
        {
            return fg::kInvalidEntity;
        }

        const glm::vec3 casterPos = fg::TransformUtil::WorldPose(registry, caster).position;
        const float     casterR   = PresenceRadius(registry, caster);
        fr::Entity      best      = fg::kInvalidEntity;
        float           bestDist  = 1e9f;
        for(const auto &[entity, pos] : candidates)
        {
            if(entity == caster || !IsHostile(registry, caster, entity))
            {
                continue;
            }
            const float limit = casterR + PresenceRadius(registry, entity) + 0.08f;
            const float dist =
                glm::length(glm::vec3 {pos.x - casterPos.x, 0.0f, pos.z - casterPos.z});
            if(dist <= limit && dist < bestDist)
            {
                bestDist = dist;
                best     = entity;
            }
        }
        return best;
    }

    inline bool ApplyDamage(fr::Registry &registry, const skr::Arc<fg::Physics> &physics,
                            const skr::Arc<fg::AnimationController> &animation, fr::Entity attacker,
                            fr::Entity defender, const MoveDef &move, float powerScale = 1.0f,
                            bool applyKnockback = false)
    {
        if(!registry.HasComponent<PokemonVitals>(defender) ||
           !registry.HasComponent<PokemonStats>(attacker) ||
           !registry.HasComponent<PokemonStats>(defender) ||
           !registry.HasComponent<PokemonTypes>(attacker) ||
           !registry.HasComponent<PokemonTypes>(defender))
        {
            return false;
        }
        if(!IsHostile(registry, attacker, defender))
        {
            return false;
        }

        bool dealt = false;
        registry.TryGetComponents<PokemonStats, PokemonTypes>(
            attacker, [&](PokemonStats &atkStats, PokemonTypes &atkTypes) {
                registry.TryGetComponents<PokemonVitals, PokemonStats, PokemonTypes, PokemonStatus>(
                    defender,
                    [&](PokemonVitals &defVitals, PokemonStats &defStats, PokemonTypes &defTypes,
                        PokemonStatus &) {
                        if(defVitals.knockedOut)
                        {
                            return;
                        }

                        if(move.category == MoveCategory::Status)
                        {
                            ApplyLeechSeedLink(registry, attacker, defender, move);
                            NoteAttacker(registry, attacker, defender);
                            dealt = true;
                            return;
                        }

                        const int offense = move.category == MoveCategory::Special
                                                ? static_cast<int>(atkStats.spAttack)
                                                : static_cast<int>(atkStats.attack);
                        const int defense = move.category == MoveCategory::Special
                                                ? static_cast<int>(defStats.spDefense)
                                                : static_cast<int>(defStats.defense);

                        float stab = 1.0f;
                        if(move.element == ElementTypeFromInt(atkTypes.primary) ||
                           move.element == ElementTypeFromInt(atkTypes.secondary))
                        {
                            stab = 1.5f;
                        }

                        const float typeMult = TypeChart::Multiplier(
                            move.element, ElementTypeFromInt(defTypes.primary),
                            ElementTypeFromInt(defTypes.secondary));

                        const float scaledPower =
                            std::max(1.0f, move.power * std::max(0.05f, powerScale));
                        const int dmg = CalcDamage(scaledPower, offense, defense, stab, typeMult,
                                                   RandomFactor());
                        defVitals.hp  = std::max(0.0f, defVitals.hp - static_cast<float>(dmg));
                        if(defVitals.hp <= 0.0f)
                        {
                            defVitals.knockedOut = true;
                            PlayKoAnim(registry, animation, defender);
                        }
                        else
                        {
                            if(move.stunDuration > 0.0f)
                            {
                                ApplyStun(registry, physics, defender, move.stunDuration);
                            }
                            // Knockback only when the caller confirmed a physics collision.
                            if(applyKnockback && move.knockbackSpeed > 0.0f)
                            {
                                const auto atkPose =
                                    fg::TransformUtil::WorldPose(registry, attacker);
                                const auto defPose =
                                    fg::TransformUtil::WorldPose(registry, defender);
                                ApplyKnockback(physics, defender,
                                               defPose.position - atkPose.position,
                                               move.knockbackSpeed);
                            }
                        }
                        NoteAttacker(registry, attacker, defender);
                        dealt = dmg > 0 || typeMult > 0.0f;
                    });
            });
        return dealt;
    }

    [[nodiscard]] inline fr::Entity FindClosestTarget(
        fr::Registry &registry,
        const std::vector<std::pair<fr::Entity, glm::vec3>> &candidates, fr::Entity self,
        const glm::vec3 &origin, const glm::vec3 &forward, float range, float radius)
    {
        fr::Entity best      = fg::kInvalidEntity;
        float      bestScore = range + 1.0f;

        for(const auto &[entity, pos] : candidates)
        {
            if(entity == self || !IsHostile(registry, self, entity))
            {
                continue;
            }
            const glm::vec3 to   = pos - origin;
            const float     dist = glm::length(to);
            if(dist > range || dist < 1e-4f)
            {
                continue;
            }
            const glm::vec3 dir   = to / dist;
            const float     along = glm::dot(dir, forward);
            if(along < 0.15f)
            {
                continue;
            }
            const float lateral = glm::length(to - forward * glm::dot(to, forward));
            if(lateral > radius + 0.75f)
            {
                continue;
            }
            if(dist < bestScore)
            {
                bestScore = dist;
                best      = entity;
            }
        }
        return best;
    }

    [[nodiscard]] inline fr::Entity FindTarget(
        fr::Registry &registry, const skr::Arc<fg::Physics> &physics, fr::Entity attacker,
        const glm::vec3 &origin, const glm::vec3 &forward, const MoveDef &move,
        const std::vector<std::pair<fr::Entity, glm::vec3>> &candidates)
    {
        fr::Entity target = fg::kInvalidEntity;
        if(physics)
        {
            fg::QueryFilter filter {};
            filter.ignoreEntity = static_cast<std::uint64_t>(attacker);
            const auto hit =
                physics->SphereCast(origin, forward, move.radius, move.range, filter);
            if(hit.hit && hit.entityId != 0)
            {
                const auto hitEntity = static_cast<fr::Entity>(hit.entityId);
                if(registry.HasComponent<PokemonVitals>(hitEntity) &&
                   IsHostile(registry, attacker, hitEntity))
                {
                    target = hitEntity;
                }
            }
        }
        if(target == fg::kInvalidEntity)
        {
            target = FindClosestTarget(registry, candidates, attacker, origin, forward, move.range,
                                       move.radius);
        }
        return target;
    }

    inline void ResolveHit(fr::Registry &registry, const skr::Arc<fg::Physics> &physics,
                           const skr::Arc<fg::AnimationController> &animation, fr::Entity attacker,
                           const MoveDef &move,
                           const std::vector<std::pair<fr::Entity, glm::vec3>> &candidates)
    {
        const auto pose = fg::TransformUtil::WorldPose(registry, attacker);
        const glm::vec3 forward = ForwardFlat(pose.rotation);
        const glm::vec3 origin  = pose.position + glm::vec3 {0.0f, 0.6f, 0.0f} + forward * 0.4f;
        const fr::Entity target =
            FindTarget(registry, physics, attacker, origin, forward, move, candidates);
        if(target != fg::kInvalidEntity)
        {
            ApplyDamage(registry, physics, animation, attacker, target, move);
        }
    }
} // namespace PokemonCombat
