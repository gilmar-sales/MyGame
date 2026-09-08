#pragma once

#include <CharacterControllerComponent.hpp>

#include <Frigga/ECS/Components/RigidBodyComponent.hpp>
#include <Frigga/ECS/Components/TransformComponent.hpp>
#include <Frigga/ECS/TransformUtil.hpp>
#include <Frigga/Physics/CharacterPhysics.hpp>
#include <Frigga/Physics/IPhysicsWorld.hpp>
#include <Frigga/Physics/PhysicsTypes.hpp>

#include <Freyr/Freyr.hpp>
#include <Skirnir/Skirnir.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace WildPhysics
{
    // Match Player CharacterController + RigidBody capsule (Scenes/main.json).
    inline constexpr float kRadius = 0.5f;
    inline constexpr float kHeight = 0.05f;
    inline constexpr float kMass   = 70.0f;

    inline void EnsureRigidBody(fr::Registry &registry, fr::Entity entity)
    {
        if(!registry.HasComponent<fg::RigidBodyComponent>(entity))
        {
            registry.AddComponents(entity,
                                   fg::RigidBodyComponent {
                                       .motion            = fg::BodyMotionType::Kinematic,
                                       .shape             = fg::ColliderShape::Capsule,
                                       .radius            = kRadius,
                                       .height            = kHeight,
                                       .mass              = kMass,
                                       .collisionLayer    = 1,
                                       .collideWithLayers = 0xffff,
                                   });
            registry.ExecuteTasks();
            return;
        }

        registry.TryGetComponents<fg::RigidBodyComponent>(entity, [&](fg::RigidBodyComponent &rb) {
            rb.motion            = fg::BodyMotionType::Kinematic;
            rb.shape             = fg::ColliderShape::Capsule;
            rb.radius            = kRadius;
            rb.height            = kHeight;
            rb.mass              = kMass;
            rb.collisionLayer    = 1;
            rb.collideWithLayers = 0xffff;
        });
    }

    inline void AttachCharacter(fr::Registry &registry, const skr::Arc<fg::IPhysicsWorld> &world,
                                fr::Entity entity)
    {
        if(!world || !registry.HasComponent<fg::TransformComponent>(entity))
        {
            return;
        }

        if(world->FindCharacter(static_cast<std::uint64_t>(entity)).IsValid())
        {
            return;
        }

        EnsureRigidBody(registry, entity);

        if(!registry.HasComponent<CharacterControllerComponent>(entity))
        {
            registry.AddComponents(entity,
                                   CharacterControllerComponent {
                                       .maxSlopeDegrees      = 45.0f,
                                       .maxStrength          = 100.0f,
                                       .stickToFloorDistance = 0.5f,
                                       .walkStairsStepHeight = 0.4f,
                                       .locomotionLocked     = false,
                                   });
            registry.ExecuteTasks();
        }

        const auto pose = fg::TransformUtil::WorldPose(registry, entity);

        fg::PhysicsBodyHandle presenceBody {};
        registry.TryGetComponents<fg::RigidBodyComponent>(entity, [&](fg::RigidBodyComponent &rb) {
            if(rb.body.IsValid())
            {
                world->DestroyBody(rb.body);
                rb.body.Reset();
            }
            fg::PhysicsBodyDesc bodyDesc {};
            bodyDesc.motion            = fg::BodyMotionType::Kinematic;
            bodyDesc.shape             = fg::ColliderShape::Capsule;
            bodyDesc.position          = pose.position;
            bodyDesc.rotation          = pose.rotation;
            bodyDesc.radius            = rb.radius;
            bodyDesc.height            = rb.height;
            bodyDesc.centerOffset      = rb.centerOffset;
            bodyDesc.mass              = rb.mass;
            bodyDesc.friction          = rb.friction;
            bodyDesc.restitution       = rb.restitution;
            bodyDesc.collisionLayer    = rb.collisionLayer;
            bodyDesc.collideWithLayers = rb.collideWithLayers;
            bodyDesc.entityId          = static_cast<std::uint64_t>(entity);
            rb.body                    = world->CreateBody(bodyDesc);
            presenceBody               = rb.body;
        });

        fg::PhysicsCharacterDesc desc {};
        desc.position              = pose.position;
        desc.rotation              = pose.rotation;
        desc.maxSlopeDegrees       = 45.0f;
        desc.maxStrength           = 100.0f;
        desc.stickToFloorDistance  = 0.5f;
        desc.walkStairsStepHeight  = 0.4f;

        registry.TryGetComponents<fg::RigidBodyComponent>(
            entity, [&](fg::RigidBodyComponent &rb) { fg::ApplyRigidBodyToCharacterDesc(desc, rb); });

        if(registry.HasComponent<CharacterControllerComponent>(entity))
        {
            registry.TryGetComponents<CharacterControllerComponent>(
                entity, [&](CharacterControllerComponent &cc) {
                    desc.maxSlopeDegrees      = cc.maxSlopeDegrees;
                    desc.maxStrength          = cc.maxStrength;
                    desc.stickToFloorDistance = cc.stickToFloorDistance;
                    desc.walkStairsStepHeight = cc.walkStairsStepHeight;
                });
        }

        const auto handle = world->CreateCharacter(desc);
        if(handle.IsValid())
        {
            world->BindCharacter(static_cast<std::uint64_t>(entity), handle, presenceBody);
        }
    }

    inline void DestroyCharacter(fr::Registry &registry, const skr::Arc<fg::IPhysicsWorld> &world,
                                 fr::Entity entity)
    {
        if(!world)
        {
            return;
        }
        const auto handle = world->FindCharacter(static_cast<std::uint64_t>(entity));
        if(handle.IsValid())
        {
            world->DestroyCharacter(handle);
        }
        world->UnbindCharacter(static_cast<std::uint64_t>(entity));

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
