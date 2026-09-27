#include "moves/MoveScript.hpp"

#include "systems/PokemonCombatUtil.hpp"

#include <mutex>

namespace
{
    std::unordered_map<std::string, std::shared_ptr<MoveScript>> &Registry()
    {
        static std::unordered_map<std::string, std::shared_ptr<MoveScript>> instance;
        return instance;
    }

    std::mutex &RegistryMutex()
    {
        static std::mutex instance;
        return instance;
    }
} // namespace

void MoveScript::OnProjectileHit(MoveHitCtx &ctx)
{
    PokemonCombat::ApplyDamage(ctx.registry, ctx.physics, ctx.animation, ctx.caster, ctx.target,
                               ctx.move, 1.0f);
}

void RegisterMoveScript(std::shared_ptr<MoveScript> script)
{
    if(!script)
    {
        return;
    }
    std::lock_guard<std::mutex> lock(RegistryMutex());
    Registry()[std::string(script->Id())] = std::move(script);
}

MoveScript *FindMoveScript(std::string_view id)
{
    std::lock_guard<std::mutex> lock(RegistryMutex());
    auto it = Registry().find(std::string(id));
    return it != Registry().end() ? it->second.get() : nullptr;
}
