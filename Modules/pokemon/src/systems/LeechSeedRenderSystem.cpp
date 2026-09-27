#include "systems/LeechSeedRenderSystem.hpp"

#include "components/MoveFxComponents.hpp"

#include <Frigga/ECS/Components/CameraComponent.hpp>
#include <Frigga/ECS/Components/WorldTransformComponent.hpp>
#include <Frigga/ECS/TransformUtil.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cmath>
#include <iterator>
#include <unordered_set>
#include <utility>
#include <vector>

namespace
{
    constexpr float kTwoPi   = 6.28318530f;
    constexpr float kGrowSec = 0.8f;
    constexpr float kRootsHeight = 0.9f;
    constexpr float kRootsRadius = 0.42f;

    /// Helical root path around `base`, matching the CellBulbasaur reference:
    /// 1.5 turns, radius tapering toward the tip.
    [[nodiscard]] std::vector<glm::vec3> MakeHelix(const glm::vec3 &base, float startAngle)
    {
        constexpr int          kPts = 16;
        std::vector<glm::vec3> pts;
        pts.reserve(kPts);
        for(int j = 0; j < kPts; ++j)
        {
            const float t = static_cast<float>(j) / static_cast<float>(kPts - 1);
            const float a = startAngle + t * 1.5f * kTwoPi;
            const float r = kRootsRadius * (1.0f - 0.25f * t);
            pts.push_back({base.x + r * std::cos(a), base.y + t * kRootsHeight,
                           base.z + r * std::sin(a)});
        }
        return pts;
    }
} // namespace

LeechSeedRenderSystem::LeechSeedRenderSystem(const skr::Arc<fr::Registry> &registry,
                                             const skr::Arc<fg::Scene> &scene)
    : fr::System(registry), mScene(scene)
{
}

void LeechSeedRenderSystem::CameraBasis(glm::vec3 &outRight, glm::vec3 &outUp,
                                        glm::vec3 &outForward) const
{
    // Freya/OpenGL convention: the camera looks along local -Z.
    outForward = {0.0f, 0.0f, -1.0f};
    outRight   = {1.0f, 0.0f, 0.0f};
    outUp      = {0.0f, 1.0f, 0.0f};

    fr::Entity camera = fr::NullEntity;
    mRegistry->CreateMutation()->Each(
        [&](fr::Entity entity, fg::WorldTransformComponent &, fg::CameraComponent &component) {
            if(camera == fr::NullEntity && component.primary)
            {
                camera = entity;
            }
        });
    if(camera == fr::NullEntity)
    {
        mRegistry->CreateMutation()->Each(
            [&](fr::Entity entity, fg::WorldTransformComponent &, fg::CameraComponent &component) {
                if(camera == fr::NullEntity && component.locked)
                {
                    camera = entity;
                }
            });
    }
    if(camera == fr::NullEntity)
    {
        mRegistry->CreateMutation()->Each(
            [&](fr::Entity entity, fg::WorldTransformComponent &, fg::CameraComponent &) {
                if(camera == fr::NullEntity)
                {
                    camera = entity;
                }
            });
    }
    if(camera == fr::NullEntity)
    {
        return;
    }

    const glm::quat rot = fg::TransformUtil::GetWorldPose(*mRegistry, camera).rotation;
    const auto      normalizeOr = [](const glm::vec3 &v, const glm::vec3 &fallback) {
        return glm::dot(v, v) > 1e-6f ? glm::normalize(v) : fallback;
    };
    outForward = normalizeOr(rot * glm::vec3 {0.0f, 0.0f, -1.0f}, outForward);
    outRight   = normalizeOr(rot * glm::vec3 {1.0f, 0.0f, 0.0f}, outRight);
    outUp      = normalizeOr(rot * glm::vec3 {0.0f, 1.0f, 0.0f}, outUp);
}

void LeechSeedRenderSystem::PostUpdate(float deltaTime)
{
    if(deltaTime <= 0.0f || !mScene)
    {
        return;
    }
    const auto &renderer = mScene->GetRenderer();
    if(!renderer)
    {
        return;
    }

    glm::vec3 right {};
    glm::vec3 up {};
    glm::vec3 forward {};
    CameraBasis(right, up, forward);
    (void)forward;

    // Snapshot live anchors (entity, pulse) — LeechSeedFxSystem owns lifetime.
    std::vector<std::pair<fr::Entity, float>> anchors;
    mRegistry->CreateMutation()->Each(
        [&](fr::Entity entity, LeechSeedAnchor &anchor, fg::TransformComponent &) {
            anchors.emplace_back(entity, anchor.pulse);
        });

    std::unordered_set<fr::Entity> live;
    live.reserve(anchors.size());
    for(const auto &[entity, pulse] : anchors)
    {
        live.insert(entity);
    }
    for(auto it = mRoots.begin(); it != mRoots.end();)
    {
        it = live.contains(it->first) ? std::next(it) : mRoots.erase(it);
    }

    auto &bb = renderer->GetBillboardDraw();

    for(const auto &[entity, pulse] : anchors)
    {
        const glm::vec3 anchorPos =
            fg::TransformUtil::GetWorldPose(*mRegistry, entity).position;
        // Anchor sits at body + 0.55 (see LeechSeedFxSystem); roots grow from the feet.
        const glm::vec3 base = anchorPos - glm::vec3 {0.0f, 0.55f, 0.0f};

        RootFx &fx = mRoots[entity];
        const float growT = std::clamp(pulse / kGrowSec, 0.0f, 1.0f);

        for(int k = 0; k < 3; ++k)
        {
            fra::SplineRope &rope = fx.ropes[static_cast<std::size_t>(k)];
            // Gentle sway while the roots are holding the victim.
            const float  sway = std::sin(pulse * 2.2f + static_cast<float>(k) * 1.7f) * 0.03f;
            glm::vec3    rootBase = base;
            rootBase.y += sway;
            rope.controlPoints = MakeHelix(rootBase, kTwoPi / 3.0f * static_cast<float>(k));
            rope.baseRadius    = 0.045f;
            rope.tipRadius     = 0.014f;
            rope.color0        = {0.22f, 0.68f, 0.15f, 1.0f};
            rope.color1        = {0.10f, 0.40f, 0.08f, 1.0f};
            rope.blend         = fra::BillboardBlend::Alpha;
            rope.segments      = 24;
            rope.growT         = growT;
            rope.Submit(bb, right, up);
        }
    }
}
