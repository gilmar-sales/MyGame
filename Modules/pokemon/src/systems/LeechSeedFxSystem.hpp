#pragma once

#include <Frigga/Asset/AssetRegistry.hpp>
#include <Frigga/Asset/PrimitiveMeshFactory.hpp>
#include <Frigga/Macro.hpp>

#include <Freyr/Freyr.hpp>
#include <Skirnir/Skirnir.hpp>

/// Visuals for scripted Leech Seed (moves/LeechSeedScript):
///  - LeechSeedAnchor: stuck seed (Models/seed.glb) + the green particle stream
///    that returns from the victim to the caster (Freya ParticleEmitterComponent,
///    ticked by Frigga's RenderSystem).
///  - LeechDrainOrb: green seed/orb flying victim -> caster once per drain tick.
/// The helical wrapping roots are rendered by LeechSeedRenderSystem (SplineRope).
/// Gameplay (damage/heal/link lifetime) stays in StatusEffectSystem.
class LeechSeedFxSystem: public fr::System
{
  public:
    LeechSeedFxSystem(const skr::Arc<fr::Registry> &registry,
                      const skr::Arc<fg::PrimitiveMeshFactory> &primitives,
                      const skr::Arc<fg::AssetRegistry> &assets);

    void Update(float deltaTime) override;

  private:
    skr::Arc<fg::PrimitiveMeshFactory> mPrimitives;
    skr::Arc<fg::AssetRegistry>        mAssets;
};
