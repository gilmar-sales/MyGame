#include "systems/CharacterMovementSystem.hpp"

#include "components/CharacterControllerComponent.hpp"

#include <Frigga/ECS/Components/AnimatorComponent.hpp>
#include <Frigga/ECS/Components/NameComponent.hpp>
#include <Frigga/ECS/Components/RigidBodyComponent.hpp>
#include <Frigga/ECS/Components/TransformComponent.hpp>
#include <Frigga/ECS/TransformUtil.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cmath>
#include <string>
#include <string_view>
#include <vector>

namespace
{
    constexpr float kMoveSpeed     = 4.0f;
    constexpr float kRunSpeed      = 7.0f;
    constexpr float kJumpSpeed     = 5.0f;
    constexpr float kIdleThreshold = 0.15f;
    constexpr float kRunThreshold  = 5.0f;

    constexpr std::string_view kClipIdle  = "001aidle";
    constexpr std::string_view kClipWalk  = "001walk";
    constexpr std::string_view kClipRun   = "001run";
    constexpr std::string_view kClipJumpS = "001jump_s";
    constexpr std::string_view kClipJumpL = "001jump_l";
} // namespace

CharacterMovementSystem::CharacterMovementSystem(
    const skr::Arc<fr::Registry> &registry, const skr::Arc<fg::Input> &input,
    const skr::Arc<fg::Physics> &physics, const skr::Arc<fg::AnimationController> &animation)
    : fr::System(registry), mInput(input), mPhysics(physics), mAnimation(animation)
{
}

fr::Entity CharacterMovementSystem::FindAnimator(fr::Entity player) const
{
    if(mRegistry->HasComponent<fg::AnimatorComponent>(player))
    {
        return player;
    }

    // Prefab instances may nest the Animator one or more levels down.
    const auto              rootChildren = mRegistry->Children(player);
    std::vector<fr::Entity> queue(rootChildren.begin(), rootChildren.end());

    for(std::size_t i = 0; i < queue.size(); ++i)
    {
        const fr::Entity node = queue[i];
        if(mRegistry->HasComponent<fg::AnimatorComponent>(node))
        {
            return node;
        }
        for(const auto child : mRegistry->Children(node))
        {
            queue.push_back(child);
        }
    }
    return fr::NullEntity;
}

void CharacterMovementSystem::PlayLocomotion(fr::Entity animator, std::string_view clip,
                                             float crossFadeSeconds)
{
    if(animator == fr::NullEntity || !mAnimation || clip.empty())
    {
        return;
    }

    // Combat (and other systems) can change the clip without updating mCurrentClip.
    // Re-drive when the Animator no longer matches the requested loco clip.
    bool animatorMatches = false;
    bool oneShotPlaying  = false;
    if(mRegistry->HasComponent<fg::AnimatorComponent>(animator))
    {
        mRegistry->TryGetComponents<fg::AnimatorComponent>(
            animator, [&](fg::AnimatorComponent &anim) {
                animatorMatches = anim.clipName == clip ||
                                  anim.clipName.find(clip) != std::string::npos;
                // One-shot clips (KO/faint, played with loop=false) own the
                // animator until another system hands it back with loop=true.
                // Never let walk/run/idle stomp them.
                oneShotPlaying = !anim.loop;
            });
    }
    if(oneShotPlaying)
    {
        mCurrentClip.clear();
        return;
    }
    if(mCurrentClip == clip && animatorMatches)
    {
        return;
    }

    if(mAnimation->CrossFade(animator, clip, crossFadeSeconds))
    {
        mCurrentClip = std::string(clip);
    }
}

void CharacterMovementSystem::Update(float deltaTime)
{
    if(!mInput || !mPhysics)
    {
        return;
    }

    if(mJumpStartTimer > 0.0f)
    {
        mJumpStartTimer = std::max(0.0f, mJumpStartTimer - deltaTime);
    }

    const float horizontal = mInput->GetAxis("Horizontal");
    const float vertical   = mInput->GetAxis("Vertical");
    const bool  jump       = mInput->WasPressed("Jump");
    const bool  sprint     = mInput->IsDown("Sprint");
    const float speed      = sprint ? kRunSpeed : kMoveSpeed;

    glm::quat cameraRotation {1.0f, 0.0f, 0.0f, 0.0f};
    bool      hasCamera = false;
    mRegistry->CreateMutation()->Each(
        [&](fr::Entity entity, fg::NameComponent &name, fg::TransformComponent &) {
            if(name.name != "Main Camera")
            {
                return;
            }
            cameraRotation = fg::TransformUtil::GetWorldPose(*mRegistry, entity).rotation;
            hasCamera      = true;
        });

    mRegistry->CreateMutation()->Each(
        [&](fr::Entity entity, fg::NameComponent &name, CharacterControllerComponent &controller,
            fg::RigidBodyComponent &) {
            if(name.name != "Player")
            {
                return;
            }

            if(controller.locomotionLocked)
            {
                // Combat / stun / KO own velocity — do not zero here or physics knockback dies.
                mCurrentClip.clear();
                return;
            }

            glm::vec3 desired;
            if(hasCamera)
            {
                const glm::vec3 forward = cameraRotation * glm::vec3 {0.0f, 0.0f, -1.0f};
                glm::vec3       flatForward {forward.x, 0.0f, forward.z};
                if(glm::dot(flatForward, flatForward) < 1e-8f)
                {
                    desired = {horizontal, 0.0f, -vertical};
                }
                else
                {
                    flatForward = glm::normalize(flatForward);
                    const glm::vec3 right =
                        glm::normalize(glm::cross(flatForward, glm::vec3 {0.0f, 1.0f, 0.0f}));
                    desired = right * horizontal + flatForward * vertical;
                }
            }
            else
            {
                desired = {horizontal, 0.0f, -vertical};
            }

            const glm::vec3 planarInput {desired.x, 0.0f, desired.z};
            const float     planarLenSq = glm::dot(planarInput, planarInput);
            if(planarLenSq > 1e-8f)
            {
                desired = glm::normalize(planarInput) * speed;
            }
            else
            {
                desired = {};
            }

            const auto  ground      = mPhysics->GetCharacterGroundInfo(entity);
            const bool  grounded    = ground.grounded;
            const float planarSpeed =
                std::sqrt(desired.x * desired.x + desired.z * desired.z);

            // Only touch Y on jump — leave gravity / fall velocity to the Dynamic body.
            desired.y = mPhysics->GetCharacterVelocity(entity).y;
            if(jump && grounded)
            {
                desired.y       = kJumpSpeed;
                mJumpStartTimer = 0.2f;
            }
            else if(grounded)
            {
                mJumpStartTimer = 0.0f;
            }

            const glm::vec3 planar {desired.x, 0.0f, desired.z};
            if(glm::dot(planar, planar) > 1e-6f)
            {
                // quatLookAt aims local -Z; Bulbasaur's mesh forward is +Z, so flip.
                const glm::vec3 dir = glm::normalize(-planar);
                mPhysics->SetCharacterFacing(
                    entity, glm::quatLookAt(dir, glm::vec3 {0.0f, 1.0f, 0.0f}));
            }

            mPhysics->MoveCharacter(entity, desired);

            const fr::Entity animator = FindAnimator(entity);
            if(animator == fr::NullEntity || !mAnimation)
            {
                return;
            }

            const bool airborne = !grounded || mJumpStartTimer > 0.0f;
            if(airborne)
            {
                if(mJumpStartTimer > 0.05f)
                {
                    PlayLocomotion(animator, kClipJumpS, 0.05f);
                }
                else
                {
                    PlayLocomotion(animator, kClipJumpL, 0.1f);
                }
                mAnimation->SetSpeed(animator, 1.0f);
                return;
            }

            if(planarSpeed < kIdleThreshold)
            {
                PlayLocomotion(animator, kClipIdle);
                mAnimation->SetSpeed(animator, 1.0f);
            }
            else if(planarSpeed >= kRunThreshold)
            {
                PlayLocomotion(animator, kClipRun);
                mAnimation->SetSpeed(animator, planarSpeed / kRunSpeed);
            }
            else
            {
                PlayLocomotion(animator, kClipWalk);
                mAnimation->SetSpeed(animator, planarSpeed / kMoveSpeed);
            }
        });
}
