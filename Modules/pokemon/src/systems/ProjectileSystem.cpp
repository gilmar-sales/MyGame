#include "systems/ProjectileSystem.hpp"

#include "components/PokemonComponents.hpp"
#include "data/PokemonCatalog.hpp"
#include "systems/PokemonCombatUtil.hpp"

#include <Frigga/ECS/Components/TransformComponent.hpp>
#include <Frigga/ECS/TransformUtil.hpp>

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

ProjectileSystem::ProjectileSystem(const skr::Arc<fr::Registry> &registry,
                                   const skr::Arc<fg::Physics> &physics,
                                   const skr::Arc<fg::AnimationController> &animation)
    : fr::System(registry), mPhysics(physics), mAnimation(animation)
{
}

void ProjectileSystem::Update(float deltaTime)
{
    if(deltaTime <= 0.0f)
    {
        return;
    }

    std::vector<std::pair<fr::Entity, glm::vec3>> candidates;
    mRegistry->CreateMutation()->Each(
        [&](fr::Entity entity, PokemonVitals &vitals, fg::TransformComponent &) {
            if(vitals.knockedOut)
            {
                return;
            }
            candidates.emplace_back(entity,
                                    fg::TransformUtil::WorldPose(*mRegistry, entity).position);
        });

    std::vector<fr::Entity> toDestroy;

    mRegistry->CreateMutation()->Each(
        [&](fr::Entity entity, PokemonProjectile &proj, fg::TransformComponent &transform) {
            if(proj.consumed)
            {
                toDestroy.push_back(entity);
                return;
            }

            proj.life -= deltaTime;
            if(proj.life <= 0.0f)
            {
                toDestroy.push_back(entity);
                return;
            }

            const glm::vec3 vel {proj.velX, proj.velY, proj.velZ};
            const float     speed = glm::length(vel);
            if(speed < 1e-4f)
            {
                toDestroy.push_back(entity);
                return;
            }

            const glm::vec3 dir      = vel / speed;
            const glm::vec3 oldPos   = transform.position;
            const glm::vec3 newPos   = oldPos + vel * deltaTime;
            transform.position       = newPos;
            transform.rotation =
                glm::quatLookAt(-dir, glm::vec3 {0.0f, 1.0f, 0.0f});

            const auto owner = static_cast<fr::Entity>(proj.owner);
            const MoveDef *def = FindMove(proj.moveId);
            if(def == nullptr)
            {
                toDestroy.push_back(entity);
                return;
            }

            fr::Entity hit = fg::kInvalidEntity;

            if(mPhysics)
            {
                fg::QueryFilter filter {};
                filter.ignoreEntity = static_cast<std::uint64_t>(owner);
                const float castDist = std::max(speed * deltaTime, 0.05f) + proj.radius;
                const auto  cast =
                    mPhysics->SphereCast(oldPos, dir, proj.radius, castDist, filter);
                if(cast.hit && cast.entityId != 0)
                {
                    const auto hitEntity = static_cast<fr::Entity>(cast.entityId);
                    if(hitEntity != owner &&
                       mRegistry->HasComponent<PokemonVitals>(hitEntity) &&
                       PokemonCombat::IsHostile(*mRegistry, owner, hitEntity))
                    {
                        hit = hitEntity;
                    }
                }
            }

            if(hit == fg::kInvalidEntity)
            {
                float bestDist = proj.radius + 0.65f;
                for(const auto &[cand, pos] : candidates)
                {
                    if(cand == owner || !PokemonCombat::IsHostile(*mRegistry, owner, cand))
                    {
                        continue;
                    }
                    const glm::vec3 to = pos + glm::vec3 {0.0f, 0.55f, 0.0f} - newPos;
                    const float     d  = glm::length(to);
                    if(d < bestDist)
                    {
                        bestDist = d;
                        hit      = cand;
                    }
                }
            }

            if(hit == fg::kInvalidEntity)
            {
                return;
            }

            if(proj.kind == 1 || def->category == MoveCategory::Status)
            {
                PokemonCombat::ApplyLeechSeedLink(*mRegistry, owner, hit, *def);
                PokemonCombat::NoteAttacker(*mRegistry, owner, hit);
            }
            else
            {
                PokemonCombat::ApplyDamage(*mRegistry, mAnimation, owner, hit, *def,
                                           proj.damageScale);
            }

            proj.consumed = true;
            toDestroy.push_back(entity);
        });

    for(const fr::Entity entity : toDestroy)
    {
        if(mRegistry->HasComponent<PokemonProjectile>(entity))
        {
            fg::TransformUtil::DestroySubtree(*mRegistry, entity);
        }
    }
}
