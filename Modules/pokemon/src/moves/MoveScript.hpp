#pragma once

/// Move execution scripts: per-move behaviour hooks.
///
/// Como criar um script novo:
///   1. Crie uma classe herdando de MoveScript (veja LeechSeedScript como exemplo).
///   2. Implemente OnProjectileHit / OnCast conforme o delivery do golpe.
///   3. Registre em RegisterCoreMoveScripts() (moves/MoveScripts.cpp).
///   4. Se precisar de visuais persistentes, crie componentes em
///      components/MoveFxComponents.hpp e um FxSystem dedicado (veja LeechSeedFxSystem).
///
/// O ProjectileSystem despacha o hit para o script quando existe um registrado
/// para aquele move id; caso contrario mantem o comportamento legado
/// (dano direto ou ApplyLeechSeedLink).

#include "components/PokemonComponents.hpp"
#include "data/PokemonCatalog.hpp"

#include <Frigga/Animation/AnimationController.hpp>
#include <Frigga/Asset/AssetRegistry.hpp>
#include <Frigga/Asset/PrimitiveMeshFactory.hpp>
#include <Frigga/Physics/Physics.hpp>

#include <Freyr/Freyr.hpp>
#include <Skirnir/Skirnir.hpp>

#include <glm/glm.hpp>

#include <memory>
#include <string_view>
#include <unordered_map>

struct MoveHitCtx
{
    fr::Registry &registry;
    skr::Arc<fg::Physics> physics;
    skr::Arc<fg::AnimationController> animation;
    fr::Entity caster = fr::NullEntity;
    fr::Entity target = fr::NullEntity;
    const MoveDef &move;
    glm::vec3 hitPos {};
};

struct MoveCastCtx
{
    fr::Registry &registry;
    skr::Arc<fg::Physics> physics;
    skr::Arc<fg::AnimationController> animation;
    skr::Arc<fg::PrimitiveMeshFactory> primitives;
    fg::AssetRegistry *assets = nullptr;
    fr::Entity caster = fr::NullEntity;
    const MoveDef &move;
    glm::vec3 origin {};
    glm::vec3 forward {};
};

class MoveScript
{
  public:
    virtual ~MoveScript() = default;
    [[nodiscard]] virtual std::string_view Id() const = 0;

    /// Called when a projectile of this move touches a hostile target.
    /// Default: direct damage via PokemonCombat::ApplyDamage.
    virtual void OnProjectileHit(MoveHitCtx &ctx);

    /// Called when the caster finishes the cast animation (non-projectile spawn point).
    /// Default: no-op (MoveCombatSystem keeps its legacy lunge/recover logic).
    virtual void OnCast(MoveCastCtx & /*ctx*/)
    {
    }
};

/// Global registry keyed by move id (e.g. "leech_seed").
void RegisterMoveScript(std::shared_ptr<MoveScript> script);
[[nodiscard]] MoveScript *FindMoveScript(std::string_view id);

/// Registers all built-in scripts. Called once from PokemonModule startup
/// (idempotent, safe under hot-reload).
void RegisterCoreMoveScripts();
