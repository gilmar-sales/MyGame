#pragma once

#include <Frigga/Macro.hpp>
#include <Frigga/Animation/AnimationController.hpp>
#include <Frigga/Physics/IPhysicsWorld.hpp>
#include <Frigga/Physics/Physics.hpp>

#include <Freyr/Freyr.hpp>
#include <Skirnir/Skirnir.hpp>

class WildAISystem: public fr::System
{
  public:
    WildAISystem(const skr::Arc<fr::Registry> &registry, const skr::Arc<fg::Physics> &physics,
                 const skr::Arc<fg::AnimationController> &animation,
                 const skr::Arc<fg::IPhysicsWorld> &world);

    void Update(float deltaTime) override;

  private:
    skr::Arc<fg::Physics>             mPhysics;
    skr::Arc<fg::AnimationController> mAnimation;
    skr::Arc<fg::IPhysicsWorld>       mWorld;
};
