#include "systems/WildAISystem.hpp"

#include "components/PokemonComponents.hpp"
#include "data/PokemonCatalog.hpp"
#include "data/WildTypes.hpp"
#include "systems/PokemonCombatUtil.hpp"
#include "systems/WildPhysicsUtil.hpp"

#include <Frigga/ECS/Components/AnimatorComponent.hpp>
#include <Frigga/ECS/Components/RigidBodyComponent.hpp>
#include <Frigga/ECS/Components/TransformComponent.hpp>
#include <Frigga/ECS/TransformUtil.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cmath>
#include <random>
#include <string>

namespace
{
    constexpr std::string_view kClipIdle = "001aidle";
    constexpr std::string_view kClipWalk = "001walk";
    constexpr std::string_view kClipRun  = "001run";

    thread_local std::mt19937 rng {std::random_device {}()};

    [[nodiscard]] float RandomAngle()
    {
        thread_local std::uniform_real_distribution<float> dist(0.0f, 6.2831853f);
        return dist(rng);
    }
    [[nodiscard]] float RandomTimer()
    {
        thread_local std::uniform_real_distribution<float> dist(1.2f, 3.5f);
        return dist(rng);
    }

    /// Skip physics facing writes when planar direction is unchanged (~cos 5°).
    constexpr float kFaceDotEps = 0.995f;

    void FaceFlat(const skr::Arc<fg::Physics> &physics, fr::Entity entity, WildPokemonAI &ai,
                  const glm::vec3 &dirFlat)
    {
        if(!physics || glm::dot(dirFlat, dirFlat) < 1e-6f)
        {
            return;
        }
        const glm::vec3 n = glm::normalize(glm::vec3 {dirFlat.x, 0.0f, dirFlat.z});
        const float     facingLen2 = ai.faceDirX * ai.faceDirX + ai.faceDirZ * ai.faceDirZ;
        if(facingLen2 > 0.5f &&
           (ai.faceDirX * n.x + ai.faceDirZ * n.z) > kFaceDotEps)
        {
            return;
        }

        FREYR_TRACE("APP", "WildAI.FaceFlat");
        ai.faceDirX           = n.x;
        ai.faceDirZ           = n.z;
        const glm::quat rot = glm::quatLookAt(-n, glm::vec3 {0.0f, 1.0f, 0.0f});
        physics->SetCharacterFacing(entity, rot);
    }

    /// Parallel-safe locomotion: never AttachCharacter / ExecuteTasks (spawn owns that).
    void MoveFlat(const skr::Arc<fg::Physics> &physics, fr::Entity entity, WildPokemonAI &ai,
                  fg::RigidBodyComponent &rb, const glm::vec3 &planarVel)
    {
        FREYR_TRACE("APP", "WildAI.MoveFlat");
        if(!physics || !rb.body.IsValid())
        {
            return;
        }

        FaceFlat(physics, entity, ai, planarVel);

        // Gameplay drives XZ only; Y stays with physics gravity.
        const glm::vec3 current = physics->GetCharacterVelocity(entity);
        physics->MoveCharacter(entity, {planarVel.x, current.y, planarVel.z});
    }

    void PlayLoco(fr::Registry &registry, const skr::Arc<fg::AnimationController> &animation,
                  fr::Entity entity, WildPokemonAI &ai, std::string_view clip)
    {
        if(!animation || clip.empty() || ai.locoClip == clip)
        {
            return;
        }

        FREYR_TRACE("APP", "WildAI.PlayLoco");
        fr::Entity animator = fg::kInvalidEntity;
        if(ai.animatorEntity >= 0)
        {
            animator = static_cast<fr::Entity>(ai.animatorEntity);
            if(!registry.HasComponent<fg::AnimatorComponent>(animator))
            {
                animator          = fg::kInvalidEntity;
                ai.animatorEntity = -1;
            }
        }
        if(animator == fg::kInvalidEntity)
        {
            FREYR_TRACE("APP", "WildAI.FindAnimator");
            animator = PokemonCombat::FindAnimator(registry, entity);
            ai.animatorEntity =
                animator != fg::kInvalidEntity ? static_cast<std::int64_t>(animator) : -1;
        }
        if(animator != fg::kInvalidEntity)
        {
            animation->CrossFade(animator, clip, 0.08f);
            animation->SetLoop(animator, true);
        }
        ai.locoClip.assign(clip.data(), clip.size());
    }

    void ZeroPlanar(const skr::Arc<fg::Physics> &physics, fr::Entity entity)
    {
        FREYR_TRACE("APP", "WildAI.ZeroPlanar");
        PokemonCombat::ZeroPlanarVelocity(physics, entity);
    }

    [[nodiscard]] int PickMoveSlot(PokemonMoveset &moves, float distanceToTarget)
    {
        int   bestSlot  = -1;
        float bestScore = -1e9f;
        for(int slot = 0; slot < 4; ++slot)
        {
            const MoveDef *def = FindMove(PokemonCombat::MoveIdAt(moves, slot));
            if(def == nullptr || PokemonCombat::CooldownAt(moves, slot) > 0.0f)
            {
                continue;
            }
            float score = def->power + 1.0f;
            if(distanceToTarget <= def->range + 0.5f)
            {
                score += 40.0f;
            }
            else if(def->projectileCount > 0 || def->chargeSec > 0.0f)
            {
                score += 10.0f;
            }
            else
            {
                score -= 20.0f;
            }
            if(score > bestScore)
            {
                bestScore = score;
                bestSlot  = slot;
            }
        }
        return bestSlot;
    }
} // namespace

WildAISystem::WildAISystem(const skr::Arc<fr::Registry> &registry,
                           const skr::Arc<fg::Physics> &physics,
                           const skr::Arc<fg::AnimationController> &animation)
    : fr::System(registry), mPhysics(physics), mAnimation(animation)
{
}

void WildAISystem::drainPendingDestroys()
{
    FREYR_TRACE("APP", "WildAI.DrainDestroys");
    fr::Entity entity = fg::kInvalidEntity;
    while(mPendingDestroy.try_pop(entity))
    {
        if(!mRegistry->HasComponent<WildPokemonAI>(entity))
        {
            continue;
        }
        WildPhysics::DestroyCharacter(*mRegistry, mPhysics, entity);
        fg::TransformUtil::DestroySubtree(*mRegistry, entity);
    }
}

void WildAISystem::Update(float deltaTime)
{
    FREYR_TRACE("APP", "WildAI.Update");
    if(deltaTime <= 0.0f)
    {
        return;
    }

    {
        FREYR_TRACE("APP", "WildAI.SnapshotPlayer");
        if(mPlayer == fg::kInvalidEntity || !mRegistry->HasComponent<PlayerTag>(mPlayer))
        {
            const auto player = mRegistry->CreateQuery()->First<PlayerTag>();
            mPlayer = player.has_value() ? player.value() : fg::kInvalidEntity;
        }
    }

    glm::vec3 playerPos {};
    bool      playerAlive = false;
    if(mPlayer != fg::kInvalidEntity &&
       mRegistry->HasComponent<PokemonVitals>(mPlayer) &&
       mRegistry->HasComponent<fg::TransformComponent>(mPlayer))
    {
        mRegistry->TryGetComponents<fg::TransformComponent>(
            mPlayer, [&](fg::TransformComponent &t) { playerPos = t.position; });
        mRegistry->TryGetComponents<PokemonVitals>(
            mPlayer, [&](PokemonVitals &vitals) { playerAlive = !vitals.knockedOut; });
    }
    else
    {
        mPlayer = fg::kInvalidEntity;
    }

    const bool hasPlayer = playerAlive && mPlayer != fg::kInvalidEntity;

    mRegistry->CreateMutation()
        ->WithLabel("WildAI")
        .EachAsync(
        [&](fr::Entity entity, WildPokemonAI &ai, PokemonVitals &vitals, PokemonMoveset &moves,
            PokemonCombatState &combat, PokemonStatus &status, fg::RigidBodyComponent &rb,
            fg::TransformComponent &transform) {
            const glm::vec3& pos = transform.position;

            if(ai.state == WildAIState::kFainted || vitals.knockedOut)
            {
                FREYR_TRACE("APP", "WildAI.Fainted");
                if(ai.state != WildAIState::kFainted)
                {
                    ai.state      = WildAIState::kFainted;
                    float delay = 30.0f;
                    if(ai.spawnArea >= 0 &&
                       mRegistry->HasComponent<WildSpawnArea>(
                           static_cast<fr::Entity>(ai.spawnArea)))
                    {
                        mRegistry->TryGetComponents<WildSpawnArea>(
                            static_cast<fr::Entity>(ai.spawnArea), [&](WildSpawnArea &area) {
                                delay = std::max(0.5f, area.faintRespawnDelay);
                            });
                    }
                    ai.faintTimer = delay;
                    PokemonCombat::PlayKoAnim(*mRegistry, mAnimation, entity);
                    ZeroPlanar(mPhysics, entity);
                    combat.phase = PokemonCombat::kPhaseIdle;
                    combat.activeMove.clear();
                }

                ai.faintTimer -= deltaTime;
                ZeroPlanar(mPhysics, entity);
                if(ai.faintTimer <= 0.0f)
                {
                    mPendingDestroy.emplace(entity);
                }
                return;
            }

            if(status.stunTimer > 0.0f)
            {
                // Let physics knockback resolve — do not zero velocity or steer.
                ai.locoClip.clear();
                return;
            }

            if(combat.phase != PokemonCombat::kPhaseIdle)
            {
                // Fight anim owns the animator while a move is resolving.
                ai.locoClip.clear();
                return;
            }

            const glm::vec3 toPlayer =
                hasPlayer ? glm::vec3 {playerPos.x - pos.x, 0.0f, playerPos.z - pos.z}
                          : glm::vec3 {};
            const float distPlayer = hasPlayer ? glm::length(toPlayer) : 1e9f;

            if(ai.lastAttacker >= 0)
            {
                if(ai.personality == WildPersonality::kCowardly)
                {
                    ai.state  = WildAIState::kFleeing;
                    ai.target = ai.lastAttacker;
                }
                else if(ai.personality == WildPersonality::kPeaceful ||
                        ai.personality == WildPersonality::kAggressive)
                {
                    ai.state  = WildAIState::kCombat;
                    ai.target = ai.lastAttacker;
                }
                ai.lastAttacker = -1;
            }

            if(ai.state == WildAIState::kPassive && hasPlayer && distPlayer <= ai.alertRadius)
            {
                if(ai.personality == WildPersonality::kAggressive)
                {
                    ai.state  = WildAIState::kCombat;
                    ai.target = static_cast<std::int64_t>(mPlayer);
                }
                else if(ai.personality == WildPersonality::kSkittish)
                {
                    ai.state  = WildAIState::kFleeing;
                    ai.target = static_cast<std::int64_t>(mPlayer);
                }
            }

            if(ai.state == WildAIState::kFleeing)
            {
                FREYR_TRACE("APP", "WildAI.Flee");
                glm::vec3 escapePos = {ai.homeX, pos.y, ai.homeZ};
                float     escapeR   = 2.0f;
                if(ai.spawnArea >= 0)
                {
                    const auto spawn = static_cast<fr::Entity>(ai.spawnArea);
                    if(mRegistry->HasComponent<WildSpawnArea>(spawn))
                    {
                        mRegistry->TryGetComponents<WildSpawnArea>(
                            spawn, [&](WildSpawnArea &area) {
                                const fr::Entity escape = area.escapeArea.id;
                                if(escape == fg::kInvalidEntity ||
                                   !mRegistry->HasComponent<WildEscapeArea>(escape) ||
                                   !mRegistry->HasComponent<fg::TransformComponent>(escape))
                                {
                                    return;
                                }
                                mRegistry->TryGetComponents<fg::TransformComponent>(
                                    escape, [&](fg::TransformComponent &t) {
                                        escapePos = t.position;
                                    });
                                mRegistry->TryGetComponents<WildEscapeArea>(
                                    escape, [&](WildEscapeArea &esc) { escapeR = esc.radius; });
                            });
                    }
                }
                glm::vec3 toEscape {escapePos.x - pos.x, 0.0f, escapePos.z - pos.z};
                const float distEsc = glm::length(toEscape);
                if(distEsc <= escapeR)
                {
                    mPendingDestroy.emplace(entity);
                    return;
                }
                if(distEsc > 1e-4f)
                {
                    toEscape = glm::normalize(toEscape);
                    PlayLoco(*mRegistry, mAnimation, entity, ai, kClipRun);
                    MoveFlat(mPhysics, entity, ai, rb, toEscape * (ai.moveSpeed * 1.85f));
                }
                else
                {
                    PlayLoco(*mRegistry, mAnimation, entity, ai, kClipIdle);
                }
                return;
            }

            if(ai.state == WildAIState::kCombat)
            {
                FREYR_TRACE("APP", "WildAI.Combat");
                fr::Entity target = static_cast<fr::Entity>(ai.target);
                if(ai.target < 0 || !mRegistry->HasComponent<PokemonVitals>(target))
                {
                    if(hasPlayer)
                    {
                        target    = mPlayer;
                        ai.target = static_cast<std::int64_t>(mPlayer);
                    }
                    else
                    {
                        ai.state  = WildAIState::kPassive;
                        ai.target = -1;
                        PlayLoco(*mRegistry, mAnimation, entity, ai, kClipIdle);
                        return;
                    }
                }

                bool targetKo = false;
                mRegistry->TryGetComponents<PokemonVitals>(
                    target, [&](PokemonVitals &tv) { targetKo = tv.knockedOut; });
                if(targetKo)
                {
                    ai.state  = WildAIState::kPassive;
                    ai.target = -1;
                    PlayLoco(*mRegistry, mAnimation, entity, ai, kClipIdle);
                    return;
                }

                glm::vec3 tpos = pos;
                if(mRegistry->HasComponent<fg::TransformComponent>(target))
                {
                    mRegistry->TryGetComponents<fg::TransformComponent>(
                        target, [&](fg::TransformComponent &t) { tpos = t.position; });
                }
                glm::vec3   toT {tpos.x - pos.x, 0.0f, tpos.z - pos.z};
                const float dist = glm::length(toT);
                if(dist > 1e-4f)
                {
                    toT = glm::normalize(toT);
                    FaceFlat(mPhysics, entity, ai, toT);
                }

                const float combatRunSpeed = ai.moveSpeed * 1.75f;
                const int   slot           = PickMoveSlot(moves, dist);
                if(slot >= 0)
                {
                    const MoveDef *def = FindMove(PokemonCombat::MoveIdAt(moves, slot));
                    if(def != nullptr && dist > def->range * 0.85f && def->projectileCount == 0 &&
                       def->chargeSec <= 0.0f)
                    {
                        PlayLoco(*mRegistry, mAnimation, entity, ai, kClipRun);
                        MoveFlat(mPhysics, entity, ai, rb, toT * combatRunSpeed);
                    }
                    else
                    {
                        PlayLoco(*mRegistry, mAnimation, entity, ai, kClipIdle);
                        ZeroPlanar(mPhysics, entity);
                        {
                            FREYR_TRACE("APP", "WildAI.TryStartMove");
                            PokemonCombat::TryStartMove(*mRegistry, entity, vitals, moves, combat,
                                                        slot, toT);
                        }
                    }
                }
                else if(dist > 1.2f)
                {
                    PlayLoco(*mRegistry, mAnimation, entity, ai, kClipRun);
                    MoveFlat(mPhysics, entity, ai, rb, toT * combatRunSpeed);
                }
                else
                {
                    PlayLoco(*mRegistry, mAnimation, entity, ai, kClipIdle);
                    ZeroPlanar(mPhysics, entity);
                }
                return;
            }

            {
                FREYR_TRACE("APP", "WildAI.Wander");
                ai.wanderTimer -= deltaTime;
                if(ai.wanderTimer <= 0.0f)
                {
                    const float angle = RandomAngle();
                    ai.wanderDirX     = std::cos(angle);
                    ai.wanderDirZ     = std::sin(angle);
                    ai.wanderTimer    = RandomTimer();
                }

                const glm::vec3 wanderVel {ai.wanderDirX * ai.moveSpeed, 0.0f,
                                           ai.wanderDirZ * ai.moveSpeed};
                glm::vec3       next = pos + wanderVel * deltaTime;
                const glm::vec3 home {ai.homeX, 0.0f, ai.homeZ};
                glm::vec3       fromHome {next.x - home.x, 0.0f, next.z - home.z};
                const float     homeDist = glm::length(fromHome);
                if(homeDist > ai.spawnRadius)
                {
                    fromHome      = glm::normalize(fromHome);
                    ai.wanderDirX = -fromHome.x;
                    ai.wanderDirZ = -fromHome.z;
                }
                const glm::vec3 vel {ai.wanderDirX * ai.moveSpeed, 0.0f,
                                     ai.wanderDirZ * ai.moveSpeed};
                if(vel.x * vel.x + vel.z * vel.z > 1e-8f)
                {
                    PlayLoco(*mRegistry, mAnimation, entity, ai, kClipWalk);
                    MoveFlat(mPhysics, entity, ai, rb, vel);
                }
                else
                {
                    PlayLoco(*mRegistry, mAnimation, entity, ai, kClipIdle);
                    ZeroPlanar(mPhysics, entity);
                }
            }
        });
}

void WildAISystem::PostUpdate(float /*deltaTime*/)
{
    FREYR_TRACE("APP", "WildAI.PostUpdate");
    {
        FREYR_TRACE("APP", "WildAI.ExecuteTasks");
        mRegistry->ExecuteTasks();
    }
    drainPendingDestroys();
}
