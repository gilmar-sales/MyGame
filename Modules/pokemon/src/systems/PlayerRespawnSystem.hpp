#pragma once

#include <Frigga/Macro.hpp>
#include <Frigga/Animation/AnimationController.hpp>
#include <Frigga/Physics/Physics.hpp>

#include <Freyr/Freyr.hpp>
#include <Skirnir/Skirnir.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

class PlayerRespawnSystem: public fr::System
{
  public:
    PlayerRespawnSystem(const skr::Arc<fr::Registry> &registry,
                        const skr::Arc<fg::Physics> &physics,
                        const skr::Arc<fg::AnimationController> &animation);

    void Update(float deltaTime) override;

  private:
    void CaptureSpawn(fr::Entity player);
    void Respawn(fr::Entity player);

    skr::Arc<fg::Physics>             mPhysics;
    skr::Arc<fg::AnimationController> mAnimation;
    bool                              mHasSpawn = false;
    glm::vec3                         mSpawnPos {0.0f, 0.0f, 2.0f};
    glm::quat                         mSpawnRot {1.0f, 0.0f, 0.0f, 0.0f};
};
