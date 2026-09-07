#pragma once

#include "components/PokemonComponents.hpp"
#include "data/ElementType.hpp"
#include "data/PokemonCatalog.hpp"
#include "systems/PokemonCombatUtil.hpp"

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

    inline void SpawnForMove(fr::Registry &registry, fg::PrimitiveMeshFactory &primitives,
                             fr::Entity owner, const MoveDef &move, const glm::vec3 &origin,
                             const glm::vec3 &forwardFlat)
    {
        const int count = std::max(1, move.projectileCount);
        const bool leech =
            move.delivery == MoveDelivery::StatusRanged || move.id == "leech_seed";
        const std::uint32_t meshId = primitives.GetMesh(fg::PrimitiveType::Sphere);
        const std::uint32_t matId =
            leech ? SeedMaterial(primitives) : LeafMaterial(primitives);
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

            const glm::quat rot =
                glm::quatLookAt(-dir, glm::vec3 {0.0f, 1.0f, 0.0f});

            registry.CreateEntity(
                fg::NameComponent {.name = leech ? "LeechSeed" : "RazorLeaf"},
                fg::TransformComponent {.position = pos,
                                        .scale    = {scale, scale, scale},
                                        .rotation = rot},
                fg::MeshComponent {.meshId = meshId, .castShadows = false},
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
                                   .consumed    = false});
        }
    }
} // namespace PokemonProjectileUtil
