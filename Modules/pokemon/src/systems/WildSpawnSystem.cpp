#include "systems/WildSpawnSystem.hpp"

#include "components/PokemonComponents.hpp"
#include "data/ElementType.hpp"
#include "data/PokemonCatalog.hpp"
#include "data/WildTypes.hpp"

#include "systems/WildPhysicsUtil.hpp"

#include <Frigga/Asset/AssetRegistry.hpp>
#include <Frigga/ECS/Components/AnimatorComponent.hpp>
#include <Frigga/ECS/Components/HealthBarComponent.hpp>
#include <Frigga/ECS/Components/MaterialComponent.hpp>
#include <Frigga/ECS/Components/MeshComponent.hpp>
#include <Frigga/ECS/Components/NameComponent.hpp>
#include <Frigga/ECS/Components/TransformComponent.hpp>
#include <Frigga/ECS/TransformUtil.hpp>
#include <Frigga/Scene/Prefab.hpp>

#include <Freya/Asset/Material.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <random>
#include <string>
#include <vector>

namespace
{
    constexpr std::string_view kBulbasaurPrefab = "Prefabs/Bulbasaur.prefab";

    thread_local std::mt19937 rng {std::random_device {}()};

    [[nodiscard]] float Rand01()
    {
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        return dist(rng);
    }

    [[nodiscard]] std::int64_t RandLevel(std::int64_t lo, std::int64_t hi)
    {
        if(hi < lo)
        {
            std::swap(lo, hi);
        }
        std::uniform_int_distribution<int> dist(static_cast<int>(lo), static_cast<int>(hi));
        return dist(rng);
    }

    [[nodiscard]] std::uint32_t WildMaterial(fg::PrimitiveMeshFactory &primitives)
    {
        static std::uint32_t id = 0;
        if(id == 0)
        {
            fra::MaterialCreateInfo info {};
            info.albedoFactor    = {0.28f, 0.72f, 0.32f, 1.0f};
            info.emissiveFactor  = {0.05f, 0.15f, 0.04f};
            info.roughnessFactor = 0.8f;
            info.unlit           = false;
            id                   = primitives.CreateMaterial(info);
        }
        return id;
    }

    void ResetLocalTransform(fr::Registry &registry, fr::Entity entity)
    {
        if(!registry.HasComponent<fg::TransformComponent>(entity))
        {
            return;
        }
        registry.TryGetComponents<fg::TransformComponent>(entity, [&](fg::TransformComponent &t) {
            t.position = {};
            t.scale    = {1.0f, 1.0f, 1.0f};
            t.rotation = glm::quat {1.0f, 0.0f, 0.0f, 0.0f};
        });
    }
} // namespace

WildSpawnSystem::WildSpawnSystem(const skr::Arc<fr::Registry> &registry,
                                 const skr::Arc<fg::PrimitiveMeshFactory> &primitives,
                                 const skr::Arc<fg::AssetRegistry> &assets,
                                 const skr::Arc<fg::Scene> &scene,
                                 const skr::Arc<fg::IPhysicsWorld> &world)
    : fr::System(registry), mPrimitives(primitives), mAssets(assets), mScene(scene), mWorld(world)
{
}

void WildSpawnSystem::SpawnOne(fr::Entity areaEntity, WildSpawnArea &area, const glm::vec3 &center)
{
    const SpeciesDef *species = FindSpecies(area.speciesId);
    if(species == nullptr)
    {
        species = FindSpecies("bulbasaur");
    }
    if(species == nullptr || !mPrimitives)
    {
        return;
    }

    const float angle = Rand01() * 6.2831853f;
    const float dist  = std::sqrt(Rand01()) * std::max(0.5f, area.spawnRadius * 0.85f);
    const glm::vec3 pos {center.x + std::cos(angle) * dist, center.y,
                         center.z + std::sin(angle) * dist};

    std::int64_t personality = area.personality;
    if(personality < 0 || personality > 3)
    {
        personality = species->defaultPersonality;
    }

    const std::int64_t level = RandLevel(area.levelMin, area.levelMax);

    PokemonIdentity identity {};
    identity.speciesId = std::string(species->id);
    identity.nickname  = "Wild " + identity.speciesId;
    identity.level     = level;

    PokemonTypes types {};
    types.primary   = static_cast<std::int64_t>(species->primary);
    types.secondary = static_cast<std::int64_t>(species->secondary);

    WildPokemonAI ai {};
    ai.spawnArea   = static_cast<std::int64_t>(areaEntity);
    ai.personality = personality;
    ai.state       = WildAIState::kPassive;
    ai.homeX       = center.x;
    ai.homeZ       = center.z;
    ai.alertRadius = area.alertRadius;
    ai.spawnRadius = area.spawnRadius;
    ai.wanderTimer = 0.5f + Rand01();

    // Combat root — visuals come from the Bulbasaur prefab (mesh + textures + animator).
    const fr::Entity root = mRegistry->CreateEntity(
        fg::NameComponent {.name = "WildBulbasaur"},
        fg::TransformComponent {.position = pos},
        fg::HealthBarComponent {.fill = 1.0f, .offset = {0.0f, 1.4f, 0.0f}}, identity,
        PokemonIVs {}, PokemonStats {.dirty = true}, types, PokemonVitals {}, PokemonMoveset {},
        PokemonCombatState {}, PokemonStatus {}, PokemonTeam {.team = PokemonTeamId::kWild}, ai);
    mRegistry->ExecuteTasks();

    bool spawnedVisual = false;
    if(mScene)
    {
        const auto prefabPath = fg::AssetRegistry::ToAbsoluteResourcePath(kBulbasaurPrefab);
        fr::Entity visualRoot = fg::kInvalidEntity;
        if(std::filesystem::is_regular_file(prefabPath) &&
           fg::Prefab::Load(*mScene, prefabPath, root, visualRoot) &&
           visualRoot != fg::kInvalidEntity)
        {
            // Prefab was saved with the player's world offset; keep it local under the wild root.
            ResetLocalTransform(*mRegistry, visualRoot);
            if(mRegistry->HasComponent<fg::NameComponent>(visualRoot))
            {
                mRegistry->TryGetComponents<fg::NameComponent>(
                    visualRoot, [&](fg::NameComponent &n) { n.name = "WildBulbasaurMesh"; });
            }
            if(mRegistry->HasComponent<fg::AnimatorComponent>(visualRoot))
            {
                mRegistry->TryGetComponents<fg::AnimatorComponent>(
                    visualRoot, [&](fg::AnimatorComponent &anim) {
                        anim.playing = true;
                        anim.loop    = true;
                        if(anim.clipName.empty())
                        {
                            anim.clipName = "model_skeleton|001aidle";
                        }
                    });
            }
            spawnedVisual = true;
        }
    }

    if(!spawnedVisual)
    {
        const std::uint32_t meshId = mPrimitives->GetMesh(fg::PrimitiveType::Capsule);
        const std::uint32_t matId  = WildMaterial(*mPrimitives);
        const fr::Entity visual    = mRegistry->CreateEntity(
            fg::NameComponent {.name = "WildBulbasaurMesh"},
            fg::TransformComponent {.scale = {0.55f, 0.55f, 0.55f}},
            fg::MeshComponent {.meshId = meshId, .castShadows = true},
            fg::MaterialComponent {.materialId = matId});
        mRegistry->ExecuteTasks();
        fg::TransformUtil::SetParent(*mRegistry, visual, root, false);
    }

    // Character: Dynamic Sphere matching Player RigidBody (centerOffset lifts feet).
    WildPhysics::AttachCharacter(*mRegistry, mWorld, root);
}

void WildSpawnSystem::Update(float)
{
    if(!mPrimitives)
    {
        return;
    }

    struct AreaInfo
    {
        fr::Entity    entity = fg::kInvalidEntity;
        WildSpawnArea area {};
        glm::vec3     center {};
        int           count = 0;
    };

    std::vector<AreaInfo> areas;
    mRegistry->CreateMutation()->Each(
        [&](fr::Entity entity, WildSpawnArea &area, fg::TransformComponent &) {
            AreaInfo info;
            info.entity = entity;
            info.area   = area;
            info.center = fg::TransformUtil::WorldPose(*mRegistry, entity).position;
            areas.push_back(info);
        });

    if(areas.empty())
    {
        return;
    }

    mRegistry->CreateMutation()->Each([&](fr::Entity, WildPokemonAI &ai) {
        for(AreaInfo &info : areas)
        {
            if(ai.spawnArea == static_cast<std::int64_t>(info.entity))
            {
                ++info.count;
            }
        }
    });

    for(AreaInfo &info : areas)
    {
        mRegistry->TryGetComponents<WildSpawnArea>(info.entity, [&](WildSpawnArea &area) {
            const int live = info.count;
            if(live >= static_cast<int>(area.lastLiveCount))
            {
                const int appeared = live - static_cast<int>(area.lastLiveCount);
                area.pendingSpawns =
                    std::max<std::int64_t>(0, area.pendingSpawns - appeared);
            }
            area.lastLiveCount = live;

            const int need = static_cast<int>(area.maxCount) - live -
                             static_cast<int>(area.pendingSpawns);
            // Cap per frame so prefab instantiate cannot hitch the sim.
            constexpr int kMaxSpawnPerFrame = 8;
            const int     toSpawn = std::min(need, kMaxSpawnPerFrame);
            for(int i = 0; i < toSpawn; ++i)
            {
                SpawnOne(info.entity, area, info.center);
                ++area.pendingSpawns;
            }
        });
    }
}
