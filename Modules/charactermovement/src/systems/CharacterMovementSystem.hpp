#pragma once

#include <Frigga/Macro.hpp>
#include <Frigga/Animation/AnimationController.hpp>
#include <Frigga/Input/Input.hpp>
#include <Frigga/Physics/Physics.hpp>

#include <Freyr/Freyr.hpp>
#include <Skirnir/Skirnir.hpp>

#include <string>

class CharacterMovementSystem: public fr::System
{
  public:
    CharacterMovementSystem(const skr::Arc<fr::Registry> &registry,
                            const skr::Arc<fg::Input> &input,
                            const skr::Arc<fg::Physics> &physics,
                            const skr::Arc<fg::AnimationController> &animation);

    void Update(float deltaTime) override;

  private:
    [[nodiscard]] fr::Entity FindAnimator(fr::Entity player) const;
    void                     PlayLocomotion(fr::Entity animator, std::string_view clip,
                                            float crossFadeSeconds = 0.15f);

    skr::Arc<fg::Input>               mInput;
    skr::Arc<fg::Physics>             mPhysics;
    skr::Arc<fg::AnimationController> mAnimation;
    std::string                       mCurrentClip;
    float                             mJumpStartTimer = 0.0f;
};
