#pragma once

#include <Frigga/Asset/AssetRegistry.hpp>
#include <Frigga/Macro.hpp>
#include <Frigga/Scene/Scene.hpp>
#include <Frigga/Scene/SceneSimulationState.hpp>

#include <Freya/Asset/TexturePool.hpp>

#include <Freyr/Freyr.hpp>
#include <Skirnir/Skirnir.hpp>

#include <array>
#include <cstdint>

class PokemonHudSystem: public fr::System
{
  public:
    /// Only singleton / Scene-held services — Freyr Update uses an unseeded scope, so
    /// Scoped Freya types (Renderer / Window / FreyaOptions) must come from Scene.
    PokemonHudSystem(const skr::Arc<fr::Registry> &registry, const skr::Arc<fg::Scene> &scene,
                     const skr::Arc<fra::TexturePool> &textures,
                     const skr::Arc<fg::AssetRegistry> &assets,
                     const skr::Arc<fg::SceneSimulationState> &simulation);

    void Update(float deltaTime) override;

  private:
    void ensureIcons();
    void syncWorldHealthBars();
    void drawPlayerHud(float deltaTime);

    [[nodiscard]] fra::TextureHandle iconForElement(std::int64_t element) const;

    skr::Arc<fg::Scene> mScene;
    skr::Arc<fra::TexturePool> mTextures;
    skr::Arc<fg::AssetRegistry> mAssets;
    skr::Arc<fg::SceneSimulationState> mSimulation;

    bool mIconsReady = false;
    std::array<fra::TextureHandle, 15> mElementIcons {};
};
