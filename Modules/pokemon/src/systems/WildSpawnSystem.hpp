#pragma once

#include "components/PokemonComponents.hpp"

#include <Frigga/Macro.hpp>
#include <Frigga/Asset/AssetRegistry.hpp>
#include <Frigga/Asset/PrimitiveMeshFactory.hpp>
#include <Frigga/Physics/Physics.hpp>
#include <Frigga/Scene/Scene.hpp>

#include <Freyr/Freyr.hpp>
#include <Skirnir/Skirnir.hpp>

#include <string>

class WildSpawnSystem: public fr::System
{
  public:
    WildSpawnSystem(const skr::Arc<fr::Registry> &registry,
                    const skr::Arc<fg::PrimitiveMeshFactory> &primitives,
                    const skr::Arc<fg::AssetRegistry> &assets, const skr::Arc<fg::Scene> &scene,
                    const skr::Arc<fg::Physics> &physics);

    void Update(float deltaTime) override;

  private:
    [[nodiscard]] bool SpawnOne(fr::Entity areaEntity, WildSpawnArea &area, const glm::vec3 &center);
    [[nodiscard]] const std::string *CachedBulbasaurPrefabJson();

    skr::Arc<fg::PrimitiveMeshFactory> mPrimitives;
    skr::Arc<fg::AssetRegistry>        mAssets;
    skr::Arc<fg::Scene>                mScene;
    skr::Arc<fg::Physics>              mPhysics;

    std::string mPrefabJson;
    bool        mPrefabJsonLoaded = false;
    bool        mPrefabJsonMissing = false;
};
