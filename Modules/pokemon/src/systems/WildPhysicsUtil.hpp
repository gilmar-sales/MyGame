#pragma once

#include <CharacterControllerComponent.hpp>

#include <Frigga/ECS/Components/RigidBodyComponent.hpp>
#include <Frigga/ECS/Components/TransformComponent.hpp>
#include <Frigga/Physics/Physics.hpp>

#include <Freyr/Freyr.hpp>
#include <Skirnir/Skirnir.hpp>

#include <glm/glm.hpp>

namespace WildPhysics
{
    // Match Player RigidBody (Scenes/main.json / editor dump).
    inline constexpr float     kRadius       = 0.5f;
    inline constexpr float     kHeight       = 0.05f;
    inline constexpr float     kMass         = 70.0f;
    inline constexpr float     kFriction     = 0.5f;
    inline constexpr float     kRestitution  = 0.0f;
    inline constexpr glm::vec3 kCenterOffset {0.0f, 0.5f, 0.0f};
    inline constexpr glm::vec3 kHalfExtents {0.5f, 0.5f, 0.5f};

    inline void EnsureRigidBody(fr::Registry &registry, fr::Entity entity)
    {
        if(!registry.HasComponent<fg::RigidBodyComponent>(entity))
        {
            registry.AddComponents(entity,
                                   fg::RigidBodyComponent {
                                       .motion            = fg::BodyMotionType::Dynamic,
                                       .shape             = fg::ColliderShape::Sphere,
                                       .halfExtents       = kHalfExtents,
                                       .radius            = kRadius,
                                       .height            = kHeight,
                                       .centerOffset      = kCenterOffset,
                                       .mass              = kMass,
                                       .friction          = kFriction,
                                       .restitution       = kRestitution,
                                       .collisionLayer    = 1,
                                       .collideWithLayers = 0xffff,
                                       .isSensor          = false,
                                   });
            registry.ExecuteTasks();
            return;
        }

        registry.TryGetComponents<fg::RigidBodyComponent>(entity, [&](fg::RigidBodyComponent &rb) {
            rb.motion            = fg::BodyMotionType::Dynamic;
            rb.shape             = fg::ColliderShape::Sphere;
            rb.halfExtents       = kHalfExtents;
            rb.radius            = kRadius;
            rb.height            = kHeight;
            rb.centerOffset      = kCenterOffset;
            rb.mass              = kMass;
            rb.friction          = kFriction;
            rb.restitution       = kRestitution;
            rb.collisionLayer    = 1;
            rb.collideWithLayers = 0xffff;
            rb.isSensor          = false;
        });
    }

    /// Ensure Dynamic Sphere RB + CharacterController + live Jolt body (mid-play safe).
    inline bool AttachCharacter(fr::Registry &registry, const skr::Arc<fg::Physics> &physics,
                                fr::Entity entity)
    {
        if(!registry.HasComponent<fg::TransformComponent>(entity))
        {
            return false;
        }

        EnsureRigidBody(registry, entity);

        if(!registry.HasComponent<CharacterControllerComponent>(entity))
        {
            registry.AddComponents(entity,
                                   CharacterControllerComponent {
                                       .maxSlopeDegrees  = 45.0f,
                                       .locomotionLocked = false,
                                   });
            registry.ExecuteTasks();
        }

        if(!physics)
        {
            return false;
        }
        return physics->EnsureBody(entity, /*lockRotation=*/true);
    }

    inline void DestroyCharacter(fr::Registry &, const skr::Arc<fg::Physics> &physics,
                                 fr::Entity entity)
    {
        if(physics)
        {
            physics->DestroyBody(entity);
        }
    }
} // namespace WildPhysics
