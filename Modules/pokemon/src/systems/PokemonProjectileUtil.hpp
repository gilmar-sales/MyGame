#pragma once

#include "components/PokemonComponents.hpp"
#include "data/ElementType.hpp"
#include "data/PokemonCatalog.hpp"
#include "systems/PokemonCombatUtil.hpp"

#include <Frigga/Asset/AssetRegistry.hpp>
#include <Frigga/Asset/PrimitiveMeshFactory.hpp>
#include <Frigga/ECS/Components/MaterialComponent.hpp>
#include <Frigga/ECS/Components/MeshComponent.hpp>
#include <Frigga/ECS/Components/NameComponent.hpp>
#include <Frigga/ECS/Components/TransformComponent.hpp>
#include <Frigga/ECS/TransformUtil.hpp>

#include <Freya/Asset/Material.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <random>
#include <string>

namespace PokemonProjectileUtil
{
    [[nodiscard]] inline std::uint32_t LeafMaterial(fg::PrimitiveMeshFactory &primitives)
    {
        static std::uint32_t id = 0;
        if(id == 0)
        {
            fra::MaterialCreateInfo info {};
            info.albedoFactor     = {0.35f, 0.9f, 0.22f, 1.0f};
            info.emissiveFactor   = {0.12f, 0.4f, 0.05f};
            info.roughnessFactor  = 0.85f;
            info.metalnessFactor  = 0.0f;
            info.unlit            = true;
            info.receiveShadows   = false;
            id                    = primitives.CreateMaterial(info);
        }
        return id;
    }

    [[nodiscard]] inline std::uint32_t SeedMaterial(fg::PrimitiveMeshFactory &primitives)
    {
        static std::uint32_t id = 0;
        if(id == 0)
        {
            fra::MaterialCreateInfo info {};
            info.albedoFactor     = {0.55f, 0.82f, 0.18f, 1.0f};
            info.emissiveFactor   = {0.2f, 0.35f, 0.05f};
            info.roughnessFactor  = 0.7f;
            info.metalnessFactor  = 0.0f;
            info.unlit            = true;
            info.receiveShadows   = false;
            id                    = primitives.CreateMaterial(info);
        }
        return id;
    }

    [[nodiscard]] inline glm::vec3 SpreadDir(const glm::vec3 &forward, float spreadRad)
    {
        thread_local std::mt19937 rng {std::random_device {}()};
        if(spreadRad <= 1e-4f)
        {
            return forward;
        }

        glm::vec3 up {0.0f, 1.0f, 0.0f};
        glm::vec3 right = glm::cross(forward, up);
        if(glm::dot(right, right) < 1e-6f)
        {
            right = glm::cross(forward, glm::vec3 {1.0f, 0.0f, 0.0f});
        }
        right           = glm::normalize(right);
        const glm::vec3 bilat = glm::normalize(glm::cross(right, forward));

        std::uniform_real_distribution<float> dist(-spreadRad, spreadRad);
        return glm::normalize(forward + right * dist(rng) + bilat * dist(rng));
    }

    [[nodiscard]] inline bool TryResolveRazorLeaf(fg::AssetRegistry *assets,
                                                   std::uint32_t &outMeshId,
                                                   std::uint32_t &outMaterialId)
    {
        if(assets == nullptr)
        {
            return false;
        }
        const fg::ModelAsset *model = assets->FindModel("Models/razor_leaf.glb");
        if(model == nullptr)
        {
            // First use: load from Resources/ so later spawns hit the cache.
            if(auto loaded = assets->LoadModel("Models/razor_leaf.glb");
               loaded && !loaded->submeshes.empty())
            {
                outMeshId     = loaded->submeshes[0].meshId;
                outMaterialId = loaded->submeshes[0].materialId;
                return outMeshId != 0;
            }
            return false;
        }
        if(model->submeshes.empty() || model->submeshes[0].meshId == 0)
        {
            return false;
        }
        outMeshId     = model->submeshes[0].meshId;
        outMaterialId = model->submeshes[0].materialId;
        return true;
    }

    inline void SpawnForMove(fr::Registry &registry, fg::PrimitiveMeshFactory &primitives,
                             fg::AssetRegistry *assets, fr::Entity owner, const MoveDef &move,
                             const glm::vec3 &origin, const glm::vec3 &forwardFlat)
    {
        const int count = std::max(1, move.projectileCount);
        const bool leech =
            move.delivery == MoveDelivery::StatusRanged || move.id == "leech_seed";
        const std::uint32_t fallbackMesh = primitives.GetMesh(fg::PrimitiveType::Sphere);
        const std::uint32_t fallbackLeafMat = LeafMaterial(primitives);
        const std::uint32_t seedMat         = SeedMaterial(primitives);

        std::uint32_t leafMesh = 0;
        std::uint32_t leafMat  = 0;
        const bool useLeafPrefab =
            !leech && TryResolveRazorLeaf(assets, leafMesh, leafMat);
        if(!useLeafPrefab)
        {
            leafMesh = fallbackMesh;
            leafMat  = fallbackLeafMat;
        }
        else if(leafMat == 0)
        {
            leafMat = fallbackLeafMat;
        }

        const std::uint32_t seedMesh = fallbackMesh;
        const float scale =
            move.projectileScale > 0.0f ? move.projectileScale : (leech ? 0.28f : 0.1f);
        const float speed =
            move.projectileSpeed > 0.0f ? move.projectileSpeed : 10.0f;
        const float life =
            move.projectileLife > 0.0f ? move.projectileLife : 1.0f;
        const float radius =
            move.projectileRadius > 0.0f ? move.projectileRadius : 0.2f;
        const float damageScale = leech ? 1.0f : (1.35f / static_cast<float>(count));
        const std::int64_t kind = leech ? 1 : 0;

        const glm::vec3 spawnBase =
            origin + glm::vec3 {0.0f, 0.75f, 0.0f} + forwardFlat * 0.55f;

        for(int i = 0; i < count; ++i)
        {
            glm::vec3 dir = SpreadDir(forwardFlat, move.projectileSpread);
            // Slight vertical bias so the volley reads as a leaf burst.
            if(!leech)
            {
                thread_local std::mt19937 rng {std::random_device {}()};
                std::uniform_real_distribution<float> lift(-0.12f, 0.18f);
                dir = glm::normalize(dir + glm::vec3 {0.0f, lift(rng), 0.0f});
            }

            const glm::vec3 vel = dir * speed;
            glm::vec3       pos = spawnBase;
            if(!leech)
            {
                // Stagger flakes a bit so the burst isn't a single blob.
                thread_local std::mt19937 rng {std::random_device {}()};
                std::uniform_real_distribution<float> jitter(-0.12f, 0.12f);
                pos += glm::vec3 {jitter(rng), jitter(rng) * 0.5f, jitter(rng)};
            }

            const glm::quat baseRot =
                glm::quatLookAt(-dir, glm::vec3 {0.0f, 1.0f, 0.0f});

            // Razor leaves spin in flight; seeds keep a stable facing.
            glm::vec3 spinAxis {0.0f, 1.0f, 0.0f};
            float     spinSpeed = 0.0f;
            float     spinAngle = 0.0f;
            glm::quat rot       = baseRot;
            if(!leech)
            {
                thread_local std::mt19937 rng {std::random_device {}()};
                std::uniform_real_distribution<float> axisDist(-1.0f, 1.0f);
                std::uniform_real_distribution<float> speedDist(9.0f, 18.0f);
                std::uniform_real_distribution<float> angleDist(0.0f, 6.2831853f);
                glm::vec3 axis {axisDist(rng), axisDist(rng), axisDist(rng)};
                if(glm::dot(axis, axis) < 1e-4f)
                {
                    axis = glm::vec3 {0.0f, 1.0f, 0.0f};
                }
                spinAxis  = glm::normalize(axis);
                spinSpeed = speedDist(rng);
                spinAngle = angleDist(rng);
                rot = baseRot * glm::angleAxis(spinAngle, spinAxis);
            }

            const std::uint32_t meshId = leech ? seedMesh : leafMesh;
            const std::uint32_t matId  = leech ? seedMat : leafMat;

            registry.CreateEntity(
                fg::NameComponent {.name = leech ? "LeechSeed" : "RazorLeaf"},
                fg::TransformComponent {.position = pos,
                                        .scale    = {scale, scale, scale},
                                        .rotation = rot},
                fg::MeshComponent {.meshId = meshId, .castShadows = !leech},
                fg::MaterialComponent {.materialId = matId},
                PokemonProjectile {.owner       = static_cast<std::int64_t>(owner),
                                   .moveId      = std::string(move.id),
                                   .velX        = vel.x,
                                   .velY        = vel.y,
                                   .velZ        = vel.z,
                                   .life        = life,
                                   .radius      = radius,
                                   .damageScale = damageScale,
                                   .kind        = kind,
                                   .consumed    = false,
                                   .spinAxisX   = spinAxis.x,
                                   .spinAxisY   = spinAxis.y,
                                   .spinAxisZ   = spinAxis.z,
                                   .spinSpeed   = spinSpeed,
                                   .spinAngle   = spinAngle});
        }
    }

    // Back-compat for callers without an AssetRegistry (falls back to spheres).
    inline void SpawnForMove(fr::Registry &registry, fg::PrimitiveMeshFactory &primitives,
                             fr::Entity owner, const MoveDef &move, const glm::vec3 &origin,
                             const glm::vec3 &forwardFlat)
    {
        SpawnForMove(registry, primitives, nullptr, owner, move, origin, forwardFlat);
    }
} // namespace PokemonProjectileUtil
