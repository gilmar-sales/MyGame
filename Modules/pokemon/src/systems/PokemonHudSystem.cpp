#include "systems/PokemonHudSystem.hpp"

#include "components/PokemonComponents.hpp"
#include "data/PokemonCatalog.hpp"
#include "systems/PokemonCombatUtil.hpp"

#include <Frigga/ECS/Components/HealthBarComponent.hpp>
#include <Frigga/ECS/Components/NameComponent.hpp>

#include <Freya/Advanced.hpp>
#include <Freya/Core/UiContext.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

namespace
{
    fra::TextureHandle MakeSolidTexture(fra::TexturePool &pool, std::uint8_t r, std::uint8_t g,
                                        std::uint8_t b, std::uint32_t size = 64)
    {
        std::vector<std::uint8_t> pixels(size * size * 4);
        for(std::uint32_t i = 0; i < size * size; ++i)
        {
            pixels[i * 4 + 0] = r;
            pixels[i * 4 + 1] = g;
            pixels[i * 4 + 2] = b;
            pixels[i * 4 + 3] = 255;
        }
        return pool.CreateTextureFromMemory(pixels.data(), size, size, 4, 1);
    }

    void ElementRgb(ElementType type, std::uint8_t &r, std::uint8_t &g, std::uint8_t &b)
    {
        switch(type)
        {
        case ElementType::Fire:
            r = 220;
            g = 70;
            b = 30;
            break;
        case ElementType::Water:
            r = 60;
            g = 120;
            b = 220;
            break;
        case ElementType::Grass:
            r = 70;
            g = 180;
            b = 80;
            break;
        case ElementType::Electric:
            r = 230;
            g = 200;
            b = 40;
            break;
        case ElementType::Ice:
            r = 120;
            g = 200;
            b = 230;
            break;
        case ElementType::Poison:
            r = 160;
            g = 80;
            b = 200;
            break;
        case ElementType::Fighting:
            r = 180;
            g = 70;
            b = 50;
            break;
        case ElementType::Psychic:
            r = 220;
            g = 90;
            b = 160;
            break;
        case ElementType::Ground:
            r = 180;
            g = 140;
            b = 70;
            break;
        case ElementType::Flying:
            r = 140;
            g = 170;
            b = 220;
            break;
        case ElementType::Bug:
            r = 140;
            g = 180;
            b = 40;
            break;
        case ElementType::Rock:
            r = 160;
            g = 140;
            b = 90;
            break;
        case ElementType::Ghost:
            r = 100;
            g = 80;
            b = 140;
            break;
        case ElementType::Dragon:
            r = 90;
            g = 60;
            b = 200;
            break;
        case ElementType::Normal:
        default:
            r = 180;
            g = 170;
            b = 150;
            break;
        }
    }

    [[nodiscard]] const char *HotkeyForSlot(int slot)
    {
        switch(slot)
        {
        case 1:
            return "2";
        case 2:
            return "3";
        case 3:
            return "4";
        default:
            return "1";
        }
    }

    [[nodiscard]] std::string PrettyMoveName(std::string_view id)
    {
        std::string out;
        out.reserve(id.size());
        bool cap = true;
        for(const char ch : id)
        {
            if(ch == '_')
            {
                out.push_back(' ');
                cap = true;
                continue;
            }
            if(cap && ch >= 'a' && ch <= 'z')
            {
                out.push_back(static_cast<char>(ch - 'a' + 'A'));
                cap = false;
            }
            else
            {
                out.push_back(ch);
                cap = false;
            }
        }
        return out;
    }
} // namespace

PokemonHudSystem::PokemonHudSystem(const skr::Arc<fr::Registry> &registry,
                                   const skr::Arc<fg::Scene> &scene,
                                   const skr::Arc<fra::TexturePool> &textures,
                                   const skr::Arc<fg::AssetRegistry> &assets,
                                   const skr::Arc<fg::SceneSimulationState> &simulation)
    : fr::System(registry), mScene(scene), mTextures(textures), mAssets(assets),
      mSimulation(simulation)
{
}

void PokemonHudSystem::ensureIcons()
{
    if(mIconsReady || !mTextures)
    {
        return;
    }

    for(std::size_t i = 0; i < mElementIcons.size(); ++i)
    {
        std::uint8_t r = 180;
        std::uint8_t g = 170;
        std::uint8_t b = 150;
        ElementRgb(static_cast<ElementType>(i), r, g, b);
        mElementIcons[i] = MakeSolidTexture(*mTextures, r, g, b);
    }
    mIconsReady = true;
}

fra::TextureHandle PokemonHudSystem::iconForElement(std::int64_t element) const
{
    if(element < 0 || element >= static_cast<std::int64_t>(mElementIcons.size()))
    {
        return mElementIcons[0];
    }
    return mElementIcons[static_cast<std::size_t>(element)];
}

void PokemonHudSystem::syncWorldHealthBars()
{
    mRegistry->CreateMutation()->Each([&](fr::Entity entity, PokemonVitals &vitals) {
        const float fill =
            vitals.maxHp > 0.0f ? std::clamp(vitals.hp / vitals.maxHp, 0.0f, 1.0f) : 0.0f;

        if(mRegistry->HasComponent<fg::HealthBarComponent>(entity))
        {
            mRegistry->TryGetComponents<fg::HealthBarComponent>(
                entity, [&](fg::HealthBarComponent &bar) { bar.fill = fill; });
        }

        mRegistry->ForEachChild(entity, [&](fr::Entity child) {
            mRegistry->TryGetComponents<fg::HealthBarComponent>(
                child, [&](fg::HealthBarComponent &bar) { bar.fill = fill; });
        });
    });
}

void PokemonHudSystem::drawPlayerHud(float deltaTime)
{
    if(!mScene)
    {
        return;
    }
    const auto &renderer = mScene->GetRenderer();
    if(!renderer)
    {
        return;
    }

    ensureIcons();

    fr::Entity playerEntity = fr::NullEntity;
    PokemonVitals *playerVitals = nullptr;
    PokemonMoveset *playerMoves = nullptr;
    PokemonCombatState *playerCombat = nullptr;
    PokemonIdentity *playerIdentity = nullptr;

    mRegistry->CreateMutation()->Each(
        [&](fr::Entity entity, fg::NameComponent &name, PokemonVitals &vitals,
            PokemonMoveset &moves, PokemonCombatState &combat, PokemonIdentity &identity) {
            if(name.name != "Player" || playerEntity != fr::NullEntity)
            {
                return;
            }
            playerEntity   = entity;
            playerVitals   = &vitals;
            playerMoves    = &moves;
            playerCombat   = &combat;
            playerIdentity = &identity;
        });

    if(playerEntity == fr::NullEntity || playerVitals == nullptr || playerMoves == nullptr)
    {
        return;
    }

    const auto viewport = fra::Advanced(*renderer).GetViewportImage();
    std::uint32_t fbW = viewport.valid ? viewport.width : 0;
    std::uint32_t fbH = viewport.valid ? viewport.height : 0;
    // Published Runtime presents to the swapchain (no Editor offscreen viewport).
    if((fbW == 0 || fbH == 0) && mScene->GetWindow())
    {
        fbW = mScene->GetWindow()->GetWidth();
        fbH = mScene->GetWindow()->GetHeight();
    }
    if(fbW == 0 || fbH == 0)
    {
        return;
    }

    auto &ui = renderer->GetUiContext();
    if(mAssets)
    {
        if(const auto *font = mAssets->FindFont("Fonts/OpenSans.ttf"))
        {
            ui.Style().font = font;
        }
        else if(const auto *fallback = mAssets->FindFontById(mAssets->DefaultBillboardFontId()))
        {
            ui.Style().font = fallback;
        }
    }

    ui.Begin(deltaTime, {fbW, fbH});

    const float hp01 =
        playerVitals->maxHp > 0.0f
            ? std::clamp(playerVitals->hp / playerVitals->maxHp, 0.0f, 1.0f)
            : 0.0f;
    const float stamina01 =
        playerVitals->maxStamina > 0.0f
            ? std::clamp(playerVitals->stamina / playerVitals->maxStamina, 0.0f, 1.0f)
            : 0.0f;

    ui.BeginAnchor(fra::UiAnchor::TopLeft, {24.f, 24.f});
    {
        const std::string title =
            playerIdentity && !playerIdentity->nickname.empty()
                ? playerIdentity->nickname
                : (playerIdentity ? std::string(playerIdentity->speciesId) : "Player");
        ui.Label(title, ui.Style().Var(fra::UiVar::FontSize));
        ui.Label("HP", ui.Style().Var(fra::UiVar::FontSizeSmall));
        ui.ProgressBar(hp01, {220.f, 18.f});
        ui.Label("Stamina", ui.Style().Var(fra::UiVar::FontSizeSmall));
        ui.ProgressBar(stamina01, {220.f, 14.f});
    }
    ui.EndAnchor();

    constexpr float kCell  = 68.f;
    constexpr float kGap   = 8.f;
    constexpr int   kCount = 4;
    const float     barW   = kCount * kCell + (kCount - 1) * kGap + 24.f;
    ui.BeginAnchor(fra::UiAnchor::Bottom, {-barW * 0.5f, -110.f});
    if(ui.BeginPanel("player_moves", {barW, kCell + 28.f}))
    {
        ui.BeginGrid("player_moves_grid", kCount, {kCell, kCell}, kGap);
        for(int slot = 0; slot < kCount; ++slot)
        {
            const std::string &moveId = PokemonCombat::MoveIdAt(*playerMoves, slot);
            const MoveDef *def        = FindMove(moveId);
            const float cooldownMax   = def ? def->cooldown : 1.0f;
            const float remaining     = PokemonCombat::CooldownAt(*playerMoves, slot);
            const float rem01 =
                cooldownMax > 0.0f ? std::clamp(remaining / cooldownMax, 0.0f, 1.0f) : 0.0f;
            const auto icon =
                def ? iconForElement(static_cast<std::int64_t>(def->element)) : mElementIcons[0];
            const auto id = std::string("move_") + std::to_string(slot);

            if(ui.AbilitySlot(id, icon, HotkeyForSlot(slot), rem01, {kCell, kCell}))
            {
                if(playerCombat && !playerVitals->knockedOut)
                {
                    PokemonCombat::TryStartMove(*mRegistry, playerEntity, *playerVitals,
                                                *playerMoves, *playerCombat, slot);
                }
            }

            if(ui.IsItemHovered() && ui.BeginTooltip("move_tip", 0.2f))
            {
                if(def)
                {
                    if(!def->name.empty())
                    {
                        ui.Label(def->name, ui.Style().Var(fra::UiVar::FontSizeTitle));
                    }
                    else
                    {
                        ui.Label(PrettyMoveName(moveId),
                                 ui.Style().Var(fra::UiVar::FontSizeTitle));
                    }
                    if(!def->description.empty())
                    {
                        ui.TextWrapped(def->description, 240.f);
                    }
                    char meta[96];
                    std::snprintf(meta, sizeof(meta), "%s  ·  CD %.1fs  ·  STA %.0f",
                                  ElementTypeName(def->element).data(), def->cooldown,
                                  def->stamina);
                    ui.Label(meta, ui.Style().Var(fra::UiVar::FontSizeSmall));
                }
                else
                {
                    ui.Label(moveId.empty() ? "(empty)" : PrettyMoveName(moveId),
                             ui.Style().Var(fra::UiVar::FontSize));
                }
                ui.EndTooltip();
            }
        }
        ui.EndGrid();
        ui.EndPanel();
    }
    ui.EndAnchor();

    ui.End();
}

void PokemonHudSystem::Update(float deltaTime)
{
    syncWorldHealthBars();

    if(!mSimulation || !mSimulation->IsPlaying() || mSimulation->IsPaused())
    {
        return;
    }

    drawPlayerHud(deltaTime);
}
