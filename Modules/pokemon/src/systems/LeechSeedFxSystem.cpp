#include "systems/LeechSeedFxSystem.hpp"

#include "components/MoveFxComponents.hpp"
#include "components/PokemonComponents.hpp"
#include "systems/PokemonProjectileUtil.hpp"

#include <Frigga/ECS/Components/MaterialComponent.hpp>
#include <Frigga/ECS/Components/MeshComponent.hpp>
#include <Frigga/ECS/Components/NameComponent.hpp>
#include <Frigga/ECS/Components/ParticleEmitterComponent.hpp>
#include <Frigga/ECS/Components/TransformComponent.hpp>
#include <Frigga/ECS/TransformUtil.hpp>

#include <Freya/Asset/Material.hpp>

#include <glm/glm.hpp>

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace
{
    constexpr float kDrainIntervalSec = 1.0f;
    constexpr float kOrbDurationSec   = 0.45f;
    constexpr float kSeedScale        = 0.22f;

    std::uint32_t SeedMat(fg::PrimitiveMeshFactory &primitives)
    {
        static std::uint32_t id = 0;
        if(id == 0)
        {
            fra::MaterialCreateInfo info {};
            info.albedoFactor    = {0.55f, 0.85f, 0.2f, 1.0f};
            info.emissiveFactor  = {0.25f, 0.45f, 0.06f};
            info.roughnessFactor = 0.6f;
            info.metalnessFactor = 0.0f;
            info.unlit           = true;
            info.receiveShadows  = false;
            id                   = primitives.CreateMaterial(info);
        }
        return id;
    }

    std::uint32_t DrainMat(fg::PrimitiveMeshFactory &primitives)
    {
        static std::uint32_t id = 0;
        if(id == 0)
        {
            fra::MaterialCreateInfo info {};
            info.albedoFactor    = {0.45f, 1.0f, 0.35f, 1.0f};
            info.emissiveFactor  = {0.3f, 0.8f, 0.15f};
            info.roughnessFactor = 0.4f;
            info.metalnessFactor = 0.0f;
            info.unlit           = true;
            info.receiveShadows  = false;
            id                   = primitives.CreateMaterial(info);
        }
        return id;
    }

    [[nodiscard]] bool CasterAlive(fr::Registry &registry, fr::Entity caster)
    {
        if(!registry.HasComponent<PokemonVitals>(caster))
        {
            return false;
        }
        bool alive = false;
        registry.TryGetComponents<PokemonVitals>(
            caster, [&](PokemonVitals &v) { alive = !v.knockedOut; });
        return alive;
    }

    [[nodiscard]] glm::vec3 BodyPos(fr::Registry &registry, fr::Entity entity, float height)
    {
        glm::vec3 pos {};
        if(registry.HasComponent<fg::TransformComponent>(entity))
        {
            pos = fg::TransformUtil::GetWorldPose(registry, entity).position;
        }
        pos.y += height;
        return pos;
    }

    fr::Entity SpawnVisual(fr::Registry &registry, std::uint32_t mesh, const char *name,
                           const glm::vec3 &pos, const glm::vec3 &scale, std::uint32_t material)
    {
        return registry.CreateEntity(fg::NameComponent {.name = name},
                                     fg::TransformComponent {.position = pos,
                                                             .scale    = scale,
                                                             .rotation = glm::quat {
                                                                 1.0f,
                                                                 0.0f,
                                                                 0.0f,
                                                                 0.0f,
                                                             }},
                                     fg::MeshComponent {.meshId = mesh, .castShadows = false},
                                     fg::MaterialComponent {.materialId = material});
    }

    /// Freya ParticleEmitter driving the "life being sucked back" stream from the
    /// victim to the caster. Frigga's RenderSystem ticks it each frame using the
    /// owning entity's world position, so only direction/speed need updating.
    [[nodiscard]] fg::ParticleEmitterComponent MakeDrainStream(const glm::vec3 &toCaster)
    {
        const float dist  = glm::length(toCaster);
        const float speed = std::clamp(dist / 0.9f, 1.5f, 12.0f);
        const glm::vec3 dir =
            dist > 1e-4f ? toCaster / dist : glm::vec3 {0.0f, 1.0f, 0.0f};

        fra::ParticleEmitter emitter {};
        emitter.velocity       = dir * speed;
        emitter.velocityJitter = glm::vec3 {0.22f};
        emitter.spawnRate      = 46.0f;
        emitter.lifetime       = std::clamp(dist / std::max(speed, 1e-3f), 0.25f, 1.3f);
        emitter.size0          = 0.1f;
        emitter.size1          = 0.02f;
        emitter.color0         = {0.40f, 1.0f, 0.32f, 1.0f};
        emitter.color1         = {0.06f, 0.40f, 0.10f, 0.0f};
        emitter.blend          = fra::BillboardBlend::Additive;
        emitter.maxParticles   = 96;
        emitter.gravity        = {0.0f, 0.4f, 0.0f};
        emitter.drag           = 0.1f;

        fg::ParticleEmitterComponent component {};
        component.playing = true;
        component.runtime = emitter;
        return component;
    }
} // namespace

LeechSeedFxSystem::LeechSeedFxSystem(const skr::Arc<fr::Registry> &registry,
                                     const skr::Arc<fg::PrimitiveMeshFactory> &primitives,
                                     const skr::Arc<fg::AssetRegistry> &assets)
    : fr::System(registry), mPrimitives(primitives), mAssets(assets)
{
}

void LeechSeedFxSystem::Update(float deltaTime)
{
    if(deltaTime <= 0.0f || !mPrimitives)
    {
        return;
    }

    // --- 1. Collect live seeded victims (source of truth: PokemonStatus). ---
    std::unordered_map<fr::Entity, std::int64_t> seeded;
    mRegistry->CreateMutation()->Each(
        [&](fr::Entity entity, PokemonVitals &vitals, PokemonStatus &status) {
            if(status.leechSeeded && !vitals.knockedOut && status.leechSeedSource >= 0)
            {
                seeded.emplace(entity, status.leechSeedSource);
            }
        });

    // --- 2. Index existing anchors by target. ---
    std::unordered_map<fr::Entity, fr::Entity> anchorByTarget;
    mRegistry->CreateMutation()->Each([&](fr::Entity entity, LeechSeedAnchor &anchor) {
        anchorByTarget.emplace(static_cast<fr::Entity>(anchor.target), entity);
    });

    std::vector<fr::Entity> toDestroy;

    // --- 3. Destroy stale anchors (expired link, KO, missing entities). ---
    for(const auto &[target, anchorEntity] : anchorByTarget)
    {
        auto it = seeded.find(target);
        const bool stale =
            it == seeded.end() || !mRegistry->HasComponent<fg::TransformComponent>(target) ||
            !CasterAlive(*mRegistry, static_cast<fr::Entity>(it->second));
        if(stale)
        {
            toDestroy.push_back(anchorEntity);
        }
    }

    // --- 4. Ensure one anchor per seeded victim. ---
    for(const auto &[target, source] : seeded)
    {
        if(anchorByTarget.contains(target))
        {
            continue;
        }
        if(!mRegistry->HasComponent<fg::TransformComponent>(target))
        {
            continue;
        }
        const glm::vec3   at     = BodyPos(*mRegistry, target, 0.55f);
        const glm::vec3   caster = BodyPos(*mRegistry, static_cast<fr::Entity>(source), 1.0f);

        // Stuck seed (Models/seed.glb, sphere fallback): sits on top of the victim.
        std::uint32_t seedMesh = mPrimitives->GetMesh(fg::PrimitiveType::Sphere);
        std::uint32_t seedMat  = SeedMat(*mPrimitives);
        std::uint32_t modelMat = 0;
        if(PokemonProjectileUtil::TryResolveSeed(mAssets.get(), seedMesh, modelMat) && modelMat != 0)
        {
            seedMat = modelMat;
        }

        // Parent first, then rewrite the local so the seed lands on top of the victim
        // (SetParent keeps the old local, so without this it would sit at world*2).
        const fr::Entity seedEntity =
            SpawnVisual(*mRegistry, seedMesh, "LeechSeedCore", at + glm::vec3 {0.0f, 0.55f, 0.0f},
                        glm::vec3 {kSeedScale, kSeedScale, kSeedScale}, seedMat);

        const fr::Entity anchor = mRegistry->CreateEntity(
            fg::NameComponent {.name = "LeechSeedAnchor"},
            fg::TransformComponent {.position = at}, LeechSeedAnchor {
                .target   = static_cast<std::int64_t>(target),
                .source   = source,
                .orbTimer = 0.0f,
                .pulse    = 0.0f,
            },
            MakeDrainStream(caster - at));

        mRegistry->SetParent(seedEntity, anchor);
        mRegistry->TryGetComponents<fg::TransformComponent>(
            seedEntity, [&](fg::TransformComponent &ct) { ct.position = glm::vec3 {0.0f, 0.55f, 0.0f}; });
        fg::TransformUtil::MarkDirty(*mRegistry, seedEntity);
        fg::TransformUtil::MarkDirty(*mRegistry, anchor);
    }

    // --- 5. Tick anchors: follow victim, aim the drain stream, spawn drain orbs. ---
    struct OrbSpawn
    {
        fr::Entity caster;
        fr::Entity victim;
    };
    std::vector<OrbSpawn> orbSpawns;

    mRegistry->CreateMutation()->Each(
        [&](fr::Entity entity, LeechSeedAnchor &anchor, fg::TransformComponent &transform) {
            const auto target = static_cast<fr::Entity>(anchor.target);
            const auto source = static_cast<fr::Entity>(anchor.source);
            if(!seeded.contains(target))
            {
                return;
            }
            const glm::vec3 at = BodyPos(*mRegistry, target, 0.55f);
            transform.position = at;
            anchor.pulse += deltaTime;
            fg::TransformUtil::MarkDirty(*mRegistry, entity);

            // Keep the Freya particle stream aimed at the (moving) caster. Frigga's
            // RenderSystem ticks the emitter from this entity's world position.
            if(mRegistry->HasComponent<fg::ParticleEmitterComponent>(entity) &&
               CasterAlive(*mRegistry, source))
            {
                const glm::vec3 toCaster = BodyPos(*mRegistry, source, 1.0f) - at;
                const float     dist     = glm::length(toCaster);
                const glm::vec3 dir =
                    dist > 1e-4f ? toCaster / dist : glm::vec3 {0.0f, 1.0f, 0.0f};
                const float speed = std::clamp(dist / 0.9f, 1.5f, 12.0f);
                mRegistry->TryGetComponents<fg::ParticleEmitterComponent>(
                    entity, [&](fg::ParticleEmitterComponent &component) {
                        component.playing            = true;
                        component.runtime.velocity   = dir * speed;
                        component.runtime.lifetime =
                            std::clamp(dist / std::max(speed, 1e-3f), 0.25f, 1.3f);
                    });
            }

            anchor.orbTimer += deltaTime;
            if(anchor.orbTimer >= kDrainIntervalSec)
            {
                anchor.orbTimer = std::fmod(anchor.orbTimer, kDrainIntervalSec);
                if(CasterAlive(*mRegistry, source))
                {
                    orbSpawns.push_back(OrbSpawn {source, target});
                }
            }
        });

    for(const OrbSpawn &spawn : orbSpawns)
    {
        const glm::vec3 from = BodyPos(*mRegistry, spawn.victim, 0.9f);

        fra::ParticleEmitter trail {};
        trail.velocity       = {0.0f, 0.6f, 0.0f};
        trail.velocityJitter = glm::vec3 {0.18f};
        trail.spawnRate      = 55.0f;
        trail.lifetime       = 0.32f;
        trail.size0          = 0.09f;
        trail.size1          = 0.015f;
        trail.color0         = {0.5f, 1.0f, 0.38f, 1.0f};
        trail.color1         = {0.06f, 0.4f, 0.1f, 0.0f};
        trail.blend          = fra::BillboardBlend::Additive;
        trail.maxParticles   = 48;

        fg::ParticleEmitterComponent orbEmitter {};
        orbEmitter.playing = true;
        orbEmitter.runtime = trail;

        const fr::Entity orb =
            mRegistry->CreateEntity(fg::NameComponent {.name = "LeechDrainOrb"},
                                    fg::TransformComponent {.position = from,
                                                            .scale = glm::vec3 {0.14f}},
                                    fg::MeshComponent {.meshId = mPrimitives->GetMesh(
                                                           fg::PrimitiveType::Sphere),
                                                       .castShadows = false},
                                    fg::MaterialComponent {.materialId = DrainMat(*mPrimitives)},
                                    orbEmitter,
                                    LeechDrainOrb {
                                        .caster   = static_cast<std::int64_t>(spawn.caster),
                                        .victim   = static_cast<std::int64_t>(spawn.victim),
                                        .t        = 0.0f,
                                        .duration = kOrbDurationSec,
                                    });
        fg::TransformUtil::MarkDirty(*mRegistry, orb);
    }

    // --- 6. Fly drain orbs victim -> caster, destroy on arrival. ---
    mRegistry->CreateMutation()->Each(
        [&](fr::Entity entity, LeechDrainOrb &orb, fg::TransformComponent &transform) {
            const auto caster = static_cast<fr::Entity>(orb.caster);
            const auto victim = static_cast<fr::Entity>(orb.victim);
            if(!mRegistry->HasComponent<fg::TransformComponent>(caster) ||
               !mRegistry->HasComponent<fg::TransformComponent>(victim) || !CasterAlive(*mRegistry, caster))
            {
                toDestroy.push_back(entity);
                return;
            }
            orb.t += deltaTime / std::max(orb.duration, 0.05f);
            const float t = std::clamp(orb.t, 0.0f, 1.0f);
            const float s = t * t * (3.0f - 2.0f * t);
            const glm::vec3 from = BodyPos(*mRegistry, victim, 0.9f);
            const glm::vec3 to   = BodyPos(*mRegistry, caster, 1.0f);
            // Arc upward so the stream reads as sucked back, not a straight line.
            glm::vec3 pos = from + (to - from) * s;
            pos.y += std::sin(s * 3.14159265f) * 0.5f;
            transform.position = pos;
            const float shrink = 1.0f - 0.5f * s;
            transform.scale    = glm::vec3 {0.14f * shrink};

            // Trail streams out behind the orb as it homes back to the caster.
            if(mRegistry->HasComponent<fg::ParticleEmitterComponent>(entity))
            {
                glm::vec3 back = from - to;
                back.y        = 0.0f;
                const float backLen = glm::length(back);
                back = backLen > 1e-4f ? back / backLen : glm::vec3 {0.0f, 0.0f, 0.0f};
                mRegistry->TryGetComponents<fg::ParticleEmitterComponent>(
                    entity, [&](fg::ParticleEmitterComponent &component) {
                        component.runtime.velocity = back * 1.4f + glm::vec3 {0.0f, 0.5f, 0.0f};
                    });
            }
            fg::TransformUtil::MarkDirty(*mRegistry, entity);
            if(orb.t >= 1.0f)
            {
                toDestroy.push_back(entity);
            }
        });

    // --- 7. Flush destroys (anchors cascade to their visual children). ---
    std::unordered_set<fr::Entity> seen;
    for(const fr::Entity entity : toDestroy)
    {
        if(!seen.insert(entity).second)
        {
            continue;
        }
        if(mRegistry->HasComponent<LeechSeedAnchor>(entity) ||
           mRegistry->HasComponent<LeechDrainOrb>(entity))
        {
            mRegistry->DestroyEntity(entity);
        }
    }
}
