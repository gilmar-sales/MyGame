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

    inline void BeginPhysicsCharge(fr::Registry &registry, const skr::Arc<fg::Physics> &physics,
                                   fr::Entity entity, PokemonCombatState &combat)
    {
        if(!physics)
        {
            return;
        }
        float maxStrength = 100.0f;
        if(registry.HasComponent<CharacterControllerComponent>(entity))
        {
            registry.TryGetComponents<CharacterControllerComponent>(
                entity, [&](CharacterControllerComponent &cc) {
                    maxStrength         = cc.maxStrength;
                    cc.locomotionLocked = true;
                });
        }
        if(combat.savedMaxStrength < 0.0f)
        {
            combat.savedMaxStrength = maxStrength;
        }
        // CharacterVirtual push force during the charge — target reaction is pure physics.
        physics->SetCharacterMaxStrength(entity, 8000.0f);
    }

    inline void RestorePhysicsCharge(fr::Registry &, const skr::Arc<fg::Physics> &physics,
                                     fr::Entity entity, PokemonCombatState &combat)
    {
        if(!physics)
        {
            combat.savedMaxStrength = -1.0f;
            return;
        }
        const float restore =
            combat.savedMaxStrength > 0.0f ? combat.savedMaxStrength : 100.0f;
        physics->SetCharacterMaxStrength(entity, restore);
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

    /// Starts a move slot if idle, off cooldown, and has stamina. Returns true on success.
    inline bool TryStartMove(fr::Registry &registry, fr::Entity entity, PokemonVitals &vitals,
                             PokemonMoveset &moves, PokemonCombatState &combat, int slot,
                             const glm::vec3 &forwardOverride = {})
    {
        if(vitals.knockedOut || combat.phase != kPhaseIdle || combat.busyTimer > 0.0f)
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

    inline bool ApplyDamage(fr::Registry &registry,
                            const skr::Arc<fg::AnimationController> &animation, fr::Entity attacker,
                            fr::Entity defender, const MoveDef &move, float powerScale = 1.0f)
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
            ApplyDamage(registry, animation, attacker, target, move);
        }
    }
} // namespace PokemonCombat
