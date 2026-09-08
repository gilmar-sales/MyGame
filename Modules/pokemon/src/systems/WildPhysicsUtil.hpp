#pragma once

#include <CharacterControllerComponent.hpp>

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
    // Match Player CharacterControllerComponent (Scenes/main.json).
    inline constexpr float kRadius = 0.5f;
    inline constexpr float kHeight = 0.05f;
    inline constexpr float kMass   = 70.0f;

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

        if(!registry.HasComponent<CharacterControllerComponent>(entity))
        {
            registry.AddComponents(entity,
                                   CharacterControllerComponent {
                                       .radius               = kRadius,
                                       .height               = kHeight,
                                       .maxSlopeDegrees      = 45.0f,
                                       .mass                 = kMass,
                                       .maxStrength          = 100.0f,
                                       .centerOffset         = {0.0f, 0.0f, 0.0f},
                                       .stickToFloorDistance = 0.5f,
                                       .walkStairsStepHeight = 0.4f,
                                       .collisionLayer       = 1,
                                       .collideWithLayers    = 0xffff,
                                       .locomotionLocked     = false,
                                   });
            registry.ExecuteTasks();
        }

        const auto pose = fg::TransformUtil::WorldPose(registry, entity);
        fg::PhysicsCharacterDesc desc {};
        desc.position              = pose.position;
        desc.rotation              = pose.rotation;
        desc.radius                = kRadius;
        desc.height                = kHeight;
        desc.maxSlopeDegrees       = 45.0f;
        desc.mass                  = kMass;
        desc.maxStrength           = 100.0f;
        desc.centerOffset          = {};
        desc.stickToFloorDistance  = 0.5f;
        desc.walkStairsStepHeight  = 0.4f;
        desc.collisionLayer        = 1;
        desc.collideWithLayers     = 0xffff;

        if(registry.HasComponent<CharacterControllerComponent>(entity))
        {
            registry.TryGetComponents<CharacterControllerComponent>(
                entity, [&](CharacterControllerComponent &cc) {
                    desc.radius               = cc.radius;
                    desc.height               = cc.height;
                    desc.maxSlopeDegrees      = cc.maxSlopeDegrees;
                    desc.mass                 = cc.mass;
                    desc.maxStrength          = cc.maxStrength;
                    desc.centerOffset         = cc.centerOffset;
                    desc.stickToFloorDistance = cc.stickToFloorDistance;
                    desc.walkStairsStepHeight = cc.walkStairsStepHeight;
                    desc.collisionLayer       = cc.collisionLayer;
                    desc.collideWithLayers    = cc.collideWithLayers;
                });
        }

        const auto handle = world->CreateCharacter(desc);
        if(handle.IsValid())
        {
            world->BindCharacter(static_cast<std::uint64_t>(entity), handle);
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
        (void)registry;
    }
} // namespace WildPhysics
