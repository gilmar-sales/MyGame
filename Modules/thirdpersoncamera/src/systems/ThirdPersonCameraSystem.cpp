#include "systems/ThirdPersonCameraSystem.hpp"

#include "components/ThirdPersonCameraComponent.hpp"

#include <Frigga/ECS/Components/NameComponent.hpp>
#include <Frigga/ECS/Components/TransformComponent.hpp>
#include <Frigga/ECS/TransformUtil.hpp>
#include <Frigga/Physics/PhysicsTypes.hpp>

#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_map>

namespace
{
    struct NamedTarget
    {
        fr::Entity entity = fg::kInvalidEntity;
        glm::vec3  position {0.0f};
    };
} // namespace

ThirdPersonCameraSystem::ThirdPersonCameraSystem(const skr::Arc<fr::Registry> &registry,
                                                 const skr::Arc<fg::Input> &input,
                                                 const skr::Arc<fg::Physics> &physics)
    : fr::System(registry), mInput(input), mPhysics(physics)
{
}

void ThirdPersonCameraSystem::Update(float)
{
    if(!mInput)
    {
        return;
    }

    if(mInput->WasPressed("ToggleCursorLock"))
    {
        mInput->ToggleCursorLocked();
    }

    std::unordered_map<std::string, NamedTarget> namedTargets;
    mRegistry->CreateMutation()->Each(
        [&](fr::Entity entity, fg::NameComponent &name, fg::TransformComponent &) {
            namedTargets[name.name] = NamedTarget {
                .entity   = entity,
                .position = fg::TransformUtil::WorldPose(*mRegistry, entity).position,
            };
        });

    mRegistry->CreateMutation()->Each(
        [&](fr::Entity entity, fg::TransformComponent &, ThirdPersonCameraComponent &orbit) {
            orbit.yaw -= mInput->GetAxis(orbit.lookXAxis);
            orbit.pitch += mInput->GetAxis(orbit.lookYAxis);
            orbit.pitch = std::clamp(orbit.pitch, orbit.minPitch, orbit.maxPitch);

            orbit.distance -= mInput->GetAxis(orbit.zoomAxis);
            orbit.distance = std::clamp(orbit.distance, orbit.minDistance, orbit.maxDistance);

            glm::vec3  targetPos   = fg::TransformUtil::WorldPose(*mRegistry, entity).position;
            fr::Entity targetEntity = fg::kInvalidEntity;
            if(const auto found = namedTargets.find(orbit.targetName);
               found != namedTargets.end())
            {
                targetPos    = found->second.position;
                targetEntity = found->second.entity;
            }

            const glm::vec3 pivot = targetPos + orbit.pivotOffset;
            const float yawRad    = glm::radians(orbit.yaw);
            const float pitchRad  = glm::radians(orbit.pitch);
            const float cosPitch  = std::cos(pitchRad);

            const glm::vec3 offsetDir = glm::normalize(glm::vec3 {
                cosPitch * std::sin(yawRad), std::sin(pitchRad), cosPitch * std::cos(yawRad)});

            float cameraDistance = orbit.distance;
            if(orbit.collideWithWorld && mPhysics && cameraDistance > 1e-4f)
            {
                fg::QueryFilter filter {};
                if(targetEntity != fg::kInvalidEntity)
                {
                    filter.ignoreEntity = static_cast<std::uint64_t>(targetEntity);
                }

                const float probeRadius = std::max(orbit.collisionRadius, 0.05f);
                const auto  hit =
                    mPhysics->SphereCast(pivot, offsetDir, probeRadius, cameraDistance, filter);
                if(hit.hit)
                {
                    // Leave a small skin so the near plane does not bite into the surface.
                    constexpr float kSkin = 0.05f;
                    cameraDistance =
                        std::clamp(hit.distance - kSkin, 0.05f, orbit.distance);
                }
            }

            const glm::vec3 worldPos = pivot + offsetDir * cameraDistance;
            const glm::vec3 toPivot  = pivot - worldPos;
            if(glm::dot(toPivot, toPivot) < 1e-8f)
            {
                fg::TransformUtil::SetWorldPose(*mRegistry, entity, worldPos,
                                                glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
                return;
            }
            fg::TransformUtil::SetWorldPose(*mRegistry, entity, worldPos,
                                            glm::quatLookAt(glm::normalize(toPivot),
                                                            {0.0f, 1.0f, 0.0f}));
        });
}
