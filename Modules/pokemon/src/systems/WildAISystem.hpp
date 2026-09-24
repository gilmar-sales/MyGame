#pragma once

#include <Frigga/Animation/AnimationController.hpp>
#include <Frigga/Physics/Physics.hpp>

#include <Freyr/Containers/UnboundedMPMCQueue.hpp>
#include <Freyr/Freyr.hpp>
#include <Skirnir/Skirnir.hpp>

class WildAISystem: public fr::System
{
  public:
    WildAISystem(const skr::Arc<fr::Registry> &registry, const skr::Arc<fg::Physics> &physics,
                 const skr::Arc<fg::AnimationController> &animation);

    void Update(float deltaTime) override;
    void PostUpdate(float deltaTime) override;

  private:
    void drainPendingDestroys();

    skr::Arc<fg::Physics>             mPhysics;
    skr::Arc<fg::AnimationController> mAnimation;
    fr::Entity                        mPlayer = fr::NullEntity;

    /// Filled from EachAsync workers; drained serially after ExecuteTasks.
    rigtorp::UnboundedMPMCQueue<fr::Entity> mPendingDestroy;
};
