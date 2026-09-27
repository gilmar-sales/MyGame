#pragma once

#include <Frigga/Macro.hpp>
#include <Frigga/Animation/AnimationController.hpp>
#include <Frigga/Asset/AssetRegistry.hpp>
#include <Frigga/Asset/PrimitiveMeshFactory.hpp>
#include <Frigga/Physics/Physics.hpp>

#include <Freyr/Freyr.hpp>
#include <Skirnir/Skirnir.hpp>

class MoveCombatSystem: public fr::System
{
  public:
    MoveCombatSystem(const skr::Arc<fr::Registry> &registry, const skr::Arc<fg::Physics> &physics,
                     const skr::Arc<fg::AnimationController> &animation,
                     const skr::Arc<fg::PrimitiveMeshFactory> &primitives,
                     const skr::Arc<fg::AssetRegistry> &assets);

    void Update(float deltaTime) override;

  private:
    skr::Arc<fg::Physics>              mPhysics;
    skr::Arc<fg::AnimationController>  mAnimation;
    skr::Arc<fg::PrimitiveMeshFactory> mPrimitives;
    skr::Arc<fg::AssetRegistry>        mAssets;
};
