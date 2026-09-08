#pragma once

#include "components/PokemonComponents.hpp"

#include <Frigga/Macro.hpp>
#include <Frigga/Asset/AssetRegistry.hpp>
#include <Frigga/Asset/PrimitiveMeshFactory.hpp>
#include <Frigga/Physics/IPhysicsWorld.hpp>
#include <Frigga/Scene/Scene.hpp>

#include <Freyr/Freyr.hpp>
#include <Skirnir/Skirnir.hpp>

class WildSpawnSystem: public fr::System
{
  public:
    WildSpawnSystem(const skr::Arc<fr::Registry> &registry,
                    const skr::Arc<fg::PrimitiveMeshFactory> &primitives,
                    const skr::Arc<fg::AssetRegistry> &assets, const skr::Arc<fg::Scene> &scene,
                    const skr::Arc<fg::IPhysicsWorld> &world);

    void Update(float deltaTime) override;

  private:
    void SpawnOne(fr::Entity areaEntity, WildSpawnArea &area, const glm::vec3 &center);

    skr::Arc<fg::PrimitiveMeshFactory> mPrimitives;
    skr::Arc<fg::AssetRegistry>        mAssets;
    skr::Arc<fg::Scene>                mScene;
    skr::Arc<fg::IPhysicsWorld>        mWorld;
};
