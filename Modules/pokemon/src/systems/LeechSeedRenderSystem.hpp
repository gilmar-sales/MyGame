#pragma once

#include <Frigga/Macro.hpp>

#include <Frigga/Scene/Scene.hpp>
#include <Freya/Core/SplineRope.hpp>

#include <Freyr/Freyr.hpp>
#include <Skirnir/Skirnir.hpp>

#include <array>
#include <unordered_map>

/// Render-side Leech Seed VFX (runs in the "Render" pipeline):
///  - SplineRope helical roots wrapping the seeded victim (see FreyaExamples
///    CellBulbasaur's Leech Seed roots for the reference effect).
///  - Reuses the anchor entity created by LeechSeedFxSystem as the visual key.
/// Gameplay + the returning drain orbs stay in LeechSeedFxSystem (Simulation);
/// the green stream back to the caster is a ParticleEmitterComponent on the
/// anchor, ticked by Frigga's RenderSystem.
///
/// Takes fg::Scene (not Scoped fra::Renderer): Freyr constructs systems from a
/// fresh scope, so injecting scoped Freya services here crashes at startup.
class LeechSeedRenderSystem: public fr::System
{
  public:
    LeechSeedRenderSystem(const skr::Arc<fr::Registry> &registry,
                          const skr::Arc<fg::Scene> &scene);

    void PostUpdate(float deltaTime) override;

  private:
    struct RootFx
    {
        std::array<fra::SplineRope, 3> ropes;
    };

    /// Camera right/up/forward from the active CameraComponent entity.
    /// Falls back to world axes when no camera is present.
    void CameraBasis(glm::vec3 &outRight, glm::vec3 &outUp, glm::vec3 &outForward) const;

    skr::Arc<fg::Scene>                    mScene;
    std::unordered_map<fr::Entity, RootFx> mRoots;
};
