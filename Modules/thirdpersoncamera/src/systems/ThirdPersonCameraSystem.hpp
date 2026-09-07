#pragma once

#include <Frigga/Macro.hpp>
#include <Frigga/Input/Input.hpp>
#include <Frigga/Physics/Physics.hpp>

#include <Freyr/Freyr.hpp>
#include <Skirnir/Skirnir.hpp>

class ThirdPersonCameraSystem: public fr::System
{
  public:
    ThirdPersonCameraSystem(const skr::Arc<fr::Registry> &registry, const skr::Arc<fg::Input> &input,
                            const skr::Arc<fg::Physics> &physics);
    ~ThirdPersonCameraSystem() override = default;

    void Update(float deltaTime) override;

  private:
    skr::Arc<fg::Input>   mInput;
    skr::Arc<fg::Physics> mPhysics;
};
