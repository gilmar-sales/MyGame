#include "systems/WildAISystem.hpp"

#include "components/PokemonComponents.hpp"
#include "data/PokemonCatalog.hpp"
#include "data/WildTypes.hpp"
#include "systems/PokemonCombatUtil.hpp"
#include "systems/WildPhysicsUtil.hpp"

#include <Frigga/ECS/Components/NameComponent.hpp>
#include <Frigga/ECS/Components/TransformComponent.hpp>
#include <Frigga/ECS/TransformUtil.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cmath>
#include <random>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace
{
    constexpr std::string_view kClipIdle = "001aidle";
    constexpr std::string_view kClipWalk = "001walk";
    constexpr std::string_view kClipRun  = "001run";

    thread_local std::mt19937 rng {std::random_device {}()};

    [[nodiscard]] float RandRange(float a, float b)
    {
        std::uniform_real_distribution<float> dist(a, b);
        return dist(rng);
    }

    void FaceFlat(fr::Registry &, const skr::Arc<fg::Physics> &physics, fr::Entity entity,
                  const glm::vec3 &dirFlat)
    {
        if(glm::dot(dirFlat, dirFlat) < 1e-6f || !physics)
        {
            return;
        }
        const glm::vec3 n   = glm::normalize(glm::vec3 {dirFlat.x, 0.0f, dirFlat.z});
        const glm::quat rot = glm::quatLookAt(-n, glm::vec3 {0.0f, 1.0f, 0.0f});
        physics->SetCharacterFacing(entity, rot);
    }

    void MoveFlat(fr::Registry &registry, const skr::Arc<fg::Physics> &physics, fr::Entity entity,
                  const glm::vec3 &planarVel, float dt)
    {
        if(!physics)
        {
            return;
        }

        WildPhysics::AttachCharacter(registry, physics, entity);
        FaceFlat(registry, physics, entity, planarVel);

        // Gameplay drives XZ only; Y stays with physics gravity.
        const glm::vec3 current = physics->GetCharacterVelocity(entity);
        glm::vec3       desired {planarVel.x, current.y, planarVel.z};
        physics->MoveCharacter(entity, desired);
        (void)dt;
    }

    void PlayLoco(fr::Registry &registry, const skr::Arc<fg::AnimationController> &animation,
                  fr::Entity entity, WildPokemonAI &ai, std::string_view clip)
    {
        if(!animation || clip.empty() || ai.locoClip == clip)
        {
            return;
        }
        PokemonCombat::PlayMoveAnim(registry, animation, entity, clip);
        ai.locoClip = std::string(clip);
    }

    [[nodiscard]] int PickMoveSlot(PokemonMoveset &moves, float distanceToTarget)
    {
        struct Candidate
        {
            int   slot  = 0;
            float score = 0.0f;
        };
        std::vector<Candidate> options;
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
            options.push_back({slot, score});
        }
        if(options.empty())
        {
            return -1;
        }
        std::sort(options.begin(), options.end(),
                  [](const Candidate &a, const Candidate &b) { return a.score > b.score; });
        return options.front().slot;
    }
} // namespace

WildAISystem::WildAISystem(const skr::Arc<fr::Registry> &registry,
                           const skr::Arc<fg::Physics> &physics,
                           const skr::Arc<fg::AnimationController> &animation)
    : fr::System(registry), mPhysics(physics), mAnimation(animation)
{
}

void WildAISystem::Update(float deltaTime)
{
    if(deltaTime <= 0.0f)
    {
        return;
    }

    fr::Entity player = fg::kInvalidEntity;
    glm::vec3  playerPos {};
    bool       playerAlive = false;
    mRegistry->CreateMutation()->Each(
        [&](fr::Entity entity, fg::NameComponent &name, PokemonVitals &vitals,
            fg::TransformComponent &) {
            if(name.name != "Player")
            {
                return;
            }
            player      = entity;
            playerPos  = fg::TransformUtil::WorldPose(*mRegistry, entity).position;
            playerAlive = !vitals.knockedOut;
        });

    std::unordered_map<std::string, std::pair<fr::Entity, float>> escapes;
    mRegistry->CreateMutation()->Each(
        [&](fr::Entity entity, fg::NameComponent &name, WildEscapeArea &area) {
            escapes[name.name] = {entity, area.radius};
        });

    std::unordered_map<std::int64_t, std::string> spawnEscapeNames;
    mRegistry->CreateMutation()->Each(
        [&](fr::Entity entity, WildSpawnArea &area) {
            spawnEscapeNames[static_cast<std::int64_t>(entity)] = area.escapeAreaName;
        });

    std::vector<fr::Entity> destroyList;

    mRegistry->CreateMutation()->Each(
        [&](fr::Entity entity, WildPokemonAI &ai, PokemonVitals &vitals, PokemonMoveset &moves,
            PokemonCombatState &combat, fg::TransformComponent &) {
            const auto pose = fg::TransformUtil::WorldPose(*mRegistry, entity);
            const glm::vec3 pos = pose.position;

            // --- Fainted: stay on ground, then despawn for respawn ---
            if(ai.state == WildAIState::kFainted || vitals.knockedOut)
            {
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
                    PokemonCombat::ZeroPlanarVelocity(mPhysics, entity);
                    combat.phase = PokemonCombat::kPhaseIdle;
                    combat.activeMove.clear();
                }

                ai.faintTimer -= deltaTime;
                PokemonCombat::ZeroPlanarVelocity(mPhysics, entity);
                if(ai.faintTimer <= 0.0f)
                {
                    destroyList.push_back(entity);
                }
                return;
            }

            if(PokemonCombat::IsStunned(*mRegistry, entity))
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

            const bool hasPlayer = playerAlive && player != fg::kInvalidEntity;
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
                    ai.target = static_cast<std::int64_t>(player);
                }
                else if(ai.personality == WildPersonality::kSkittish)
                {
                    ai.state  = WildAIState::kFleeing;
                    ai.target = static_cast<std::int64_t>(player);
                }
            }

            if(ai.state == WildAIState::kFleeing)
            {
                glm::vec3 escapePos = {ai.homeX, pos.y, ai.homeZ};
                float     escapeR   = 2.0f;
                const auto nameIt = spawnEscapeNames.find(ai.spawnArea);
                const std::string escName =
                    nameIt != spawnEscapeNames.end() ? nameIt->second : "WildEscape";
                const auto escIt = escapes.find(escName);
                if(escIt != escapes.end())
                {
                    escapePos =
                        fg::TransformUtil::WorldPose(*mRegistry, escIt->second.first).position;
                    escapeR = escIt->second.second;
                }
                glm::vec3 toEscape {escapePos.x - pos.x, 0.0f, escapePos.z - pos.z};
                const float distEsc = glm::length(toEscape);
                if(distEsc <= escapeR)
                {
                    destroyList.push_back(entity);
                    return;
                }
                if(distEsc > 1e-4f)
                {
                    toEscape = glm::normalize(toEscape);
                    PlayLoco(*mRegistry, mAnimation, entity, ai, kClipRun);
                    MoveFlat(*mRegistry, mPhysics, entity, toEscape * (ai.moveSpeed * 1.85f),
                             deltaTime);
                }
                else
                {
                    PlayLoco(*mRegistry, mAnimation, entity, ai, kClipIdle);
                }
                return;
            }

            if(ai.state == WildAIState::kCombat)
            {
                fr::Entity target = static_cast<fr::Entity>(ai.target);
                if(ai.target < 0 || !mRegistry->HasComponent<PokemonVitals>(target))
                {
                    if(hasPlayer)
                    {
                        target    = player;
                        ai.target = static_cast<std::int64_t>(player);
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

                const glm::vec3 tpos = fg::TransformUtil::WorldPose(*mRegistry, target).position;
                glm::vec3       toT {tpos.x - pos.x, 0.0f, tpos.z - pos.z};
                const float     dist = glm::length(toT);
                if(dist > 1e-4f)
                {
                    toT = glm::normalize(toT);
                    FaceFlat(*mRegistry, mPhysics, entity, toT);
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
                        MoveFlat(*mRegistry, mPhysics, entity, toT * combatRunSpeed, deltaTime);
                    }
                    else
                    {
                        PlayLoco(*mRegistry, mAnimation, entity, ai, kClipIdle);
                        PokemonCombat::ZeroPlanarVelocity(mPhysics, entity);
                        PokemonCombat::TryStartMove(*mRegistry, entity, vitals, moves, combat,
                                                    slot, toT);
                    }
                }
                else if(dist > 1.2f)
                {
                    PlayLoco(*mRegistry, mAnimation, entity, ai, kClipRun);
                    MoveFlat(*mRegistry, mPhysics, entity, toT * combatRunSpeed, deltaTime);
                }
                else
                {
                    PlayLoco(*mRegistry, mAnimation, entity, ai, kClipIdle);
                    PokemonCombat::ZeroPlanarVelocity(mPhysics, entity);
                }
                return;
            }

            ai.wanderTimer -= deltaTime;
            if(ai.wanderTimer <= 0.0f)
            {
                const float angle = RandRange(0.0f, 6.2831853f);
                ai.wanderDirX     = std::cos(angle);
                ai.wanderDirZ     = std::sin(angle);
                ai.wanderTimer    = RandRange(1.2f, 3.5f);
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
            const glm::vec3 vel {ai.wanderDirX * ai.moveSpeed, 0.0f, ai.wanderDirZ * ai.moveSpeed};
            if(glm::length(glm::vec3 {vel.x, 0.0f, vel.z}) > 1e-4f)
            {
                PlayLoco(*mRegistry, mAnimation, entity, ai, kClipWalk);
                MoveFlat(*mRegistry, mPhysics, entity, vel, deltaTime);
            }
            else
            {
                PlayLoco(*mRegistry, mAnimation, entity, ai, kClipIdle);
                PokemonCombat::ZeroPlanarVelocity(mPhysics, entity);
            }
        });

    for(const fr::Entity entity : destroyList)
    {
        if(mRegistry->HasComponent<WildPokemonAI>(entity))
        {
            WildPhysics::DestroyCharacter(*mRegistry, mPhysics, entity);
            fg::TransformUtil::DestroySubtree(*mRegistry, entity);
        }
    }
}
