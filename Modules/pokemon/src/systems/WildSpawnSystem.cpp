#include "systems/WildSpawnSystem.hpp"

#include "components/PokemonComponents.hpp"
#include "data/ElementType.hpp"
#include "data/PokemonCatalog.hpp"
#include "data/WildTypes.hpp"

#include "systems/WildPhysicsUtil.hpp"

#include <Frigga/Asset/AssetRegistry.hpp>
#include <Frigga/ECS/Components/AnimatorComponent.hpp>
#include <Frigga/ECS/Components/HealthBarComponent.hpp>
#include <Frigga/ECS/Components/BillboardTextComponent.hpp>
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
#include <fstream>
#include <random>
#include <string>
#include <vector>

namespace
{
    constexpr std::string_view kBulbasaurPrefab = "Prefabs/Bulbasaur.prefab";
    /// Global budget: Prefab::Instantiate deserializes + scans named entities (O(scene)).
    /// Per-area caps used to explode to dozens of loads in one Simulation tick.
    constexpr int kMaxSpawnPerFrame = 32;

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
        fg::TransformUtil::MarkDirty(registry, entity);
    }
} // namespace

WildSpawnSystem::WildSpawnSystem(const skr::Arc<fr::Registry> &registry,
                                 const skr::Arc<fg::PrimitiveMeshFactory> &primitives,
                                 const skr::Arc<fg::AssetRegistry> &assets,
                                 const skr::Arc<fg::Scene> &scene,
                                 const skr::Arc<fg::Physics> &physics)
    : fr::System(registry), mPrimitives(primitives), mAssets(assets), mScene(scene),
      mPhysics(physics)
{
}

const std::string *WildSpawnSystem::CachedBulbasaurPrefabJson()
{
    if(mPrefabJsonMissing)
    {
        return nullptr;
    }
    if(mPrefabJsonLoaded)
    {
        return &mPrefabJson;
    }

    const auto prefabPath = fg::AssetRegistry::ToAbsoluteResourcePath(kBulbasaurPrefab);
    std::ifstream file(prefabPath, std::ios::binary);
    if(!file)
    {
        mPrefabJsonMissing = true;
        return nullptr;
    }
    mPrefabJson.assign((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    mPrefabJsonLoaded = true;
    if(mPrefabJson.empty())
    {
        mPrefabJsonMissing = true;
        return nullptr;
    }
    return &mPrefabJson;
}

bool WildSpawnSystem::SpawnOne(fr::Entity areaEntity, WildSpawnArea &area, const glm::vec3 &center)
{
    const SpeciesDef *species = FindSpecies(area.speciesId);
    if(species == nullptr)
    {
        species = FindSpecies("bulbasaur");
    }
    if(species == nullptr || !mPrimitives)
    {
        return false;
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
    {
        const float wanderAngle = Rand01() * 6.2831853f;
        ai.wanderDirX           = std::cos(wanderAngle);
        ai.wanderDirZ           = std::sin(wanderAngle);
    }

    const fr::Entity root = mRegistry->CreateEntity(
        fg::NameComponent {.name = "WildBulbasaur"},
        fg::TransformComponent {.position = pos},
        fg::HealthBarComponent {.fill = 1.0f, .offset = {0.0f, 1.1f, 0.0f}},
        fg::BillboardTextComponent {.text = "Bulbasaur", .heightMeters = 0.2f, .borderWidth = 1.1f, .offset = {0.0f, 1.25f, 0.0f}},
         identity,
        PokemonIVs {}, PokemonStats {.dirty = true}, types, PokemonVitals {}, PokemonMoveset {},
        PokemonCombatState {}, PokemonStatus {}, PokemonTeam {.team = PokemonTeamId::kWild}, ai);
    // One flush for the combat root before parenting the visual under it.
    mRegistry->ExecuteTasks();

    bool spawnedVisual = false;
    if(mScene)
    {
        if(const auto *json = CachedBulbasaurPrefabJson())
        {
            fr::Entity visualRoot = fr::NullEntity;
            if(fg::Prefab::Instantiate(*mScene, *json, root, visualRoot) &&
               visualRoot != fr::NullEntity)
            {
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
        fg::TransformUtil::Reparent(*mRegistry, visual, root, false);
    }

    WildPhysics::AttachCharacter(*mRegistry, mPhysics, root);
    return true;
}

void WildSpawnSystem::Update(float)
{
    if(!mPrimitives)
    {
        return;
    }

    struct AreaInfo
    {
        fr::Entity    entity = fr::NullEntity;
        glm::vec3     center {};
        int           count = 0;
    };

    std::vector<AreaInfo> areas;
    mRegistry->CreateMutation()->Each(
        [&](fr::Entity entity, WildSpawnArea &, fg::TransformComponent &) {
            AreaInfo info;
            info.entity = entity;
            info.center = fg::TransformUtil::GetWorldPose(*mRegistry, entity).position;
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

    int spawnedThisFrame = 0;
    for(AreaInfo &info : areas)
    {
        if(spawnedThisFrame >= kMaxSpawnPerFrame)
        {
            break;
        }

        mRegistry->TryGetComponents<WildSpawnArea>(info.entity, [&](WildSpawnArea &area) {
            if(spawnedThisFrame >= kMaxSpawnPerFrame)
            {
                return;
            }

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
            if(need <= 0)
            {
                return;
            }

            if(SpawnOne(info.entity, area, info.center))
            {
                ++area.pendingSpawns;
                ++spawnedThisFrame;
            }
        });
    }
}
