#pragma once

#include <CharacterControllerComponent.hpp>

#include <Frigga/ECS/Components/RigidBodyComponent.hpp>
#include <Frigga/ECS/Components/TransformComponent.hpp>
#include <Frigga/ECS/TransformUtil.hpp>
#include <Frigga/Physics/IPhysicsWorld.hpp>
#include <Frigga/Physics/PhysicsTypes.hpp>

#include <Freyr/Freyr.hpp>
#include <Skirnir/Skirnir.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

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

    /// Ensure Dynamic RigidBody + CharacterController; create the body if Play already started.
    inline void AttachCharacter(fr::Registry &registry, const skr::Arc<fg::IPhysicsWorld> &world,
                                fr::Entity entity)
    {
        if(!registry.HasComponent<fg::TransformComponent>(entity))
        {
            return;
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

        if(!world)
        {
            return;
        }

        bool hasBody = false;
        registry.TryGetComponents<fg::RigidBodyComponent>(entity, [&](fg::RigidBodyComponent &rb) {
            hasBody = rb.body.IsValid();
        });
        if(hasBody)
        {
            return;
        }

        const auto pose = fg::TransformUtil::WorldPose(registry, entity);
        registry.TryGetComponents<fg::RigidBodyComponent>(entity, [&](fg::RigidBodyComponent &rb) {
            fg::PhysicsBodyDesc bodyDesc {};
            bodyDesc.motion            = fg::BodyMotionType::Dynamic;
            bodyDesc.shape             = fg::ColliderShape::Sphere;
            bodyDesc.position          = pose.position;
            bodyDesc.rotation          = pose.rotation;
            bodyDesc.halfExtents       = rb.halfExtents;
            bodyDesc.radius            = rb.radius;
            bodyDesc.height            = rb.height;
            bodyDesc.centerOffset      = rb.centerOffset;
            bodyDesc.mass              = rb.mass;
            bodyDesc.friction          = rb.friction;
            bodyDesc.restitution       = rb.restitution;
            bodyDesc.collisionLayer    = rb.collisionLayer;
            bodyDesc.collideWithLayers = rb.collideWithLayers;
            bodyDesc.isSensor          = rb.isSensor;
            bodyDesc.entityId          = static_cast<std::uint64_t>(entity);
            bodyDesc.lockRotationX     = true;
            bodyDesc.lockRotationY     = true;
            bodyDesc.lockRotationZ     = true;
            rb.body                    = world->CreateBody(bodyDesc);
        });
    }

    inline void DestroyCharacter(fr::Registry &registry, const skr::Arc<fg::IPhysicsWorld> &world,
                                 fr::Entity entity)
    {
        if(!world)
        {
            return;
        }

        if(registry.HasComponent<fg::RigidBodyComponent>(entity))
        {
            registry.TryGetComponents<fg::RigidBodyComponent>(entity, [&](fg::RigidBodyComponent &rb) {
                if(rb.body.IsValid())
                {
                    world->DestroyBody(rb.body);
                    rb.body.Reset();
                }
            });
        }
    }
} // namespace WildPhysics
