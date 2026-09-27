#include "moves/LeechSeedScript.hpp"
#include "moves/MoveScript.hpp"

#include "systems/PokemonCombatUtil.hpp"

void LeechSeedScript::OnProjectileHit(MoveHitCtx &ctx)
{
    // Gameplay state: DoT on the victim + heal link on the caster.
    // Visuals (stuck seed, roots, drain orbs) are owned by LeechSeedFxSystem,
    // which observes PokemonStatus and spawns/cleans them automatically.
    PokemonCombat::ApplyLeechSeedLink(ctx.registry, ctx.caster, ctx.target, ctx.move);
    PokemonCombat::NoteAttacker(ctx.registry, ctx.caster, ctx.target);
}

void RegisterCoreMoveScripts()
{
    static bool registered = false;
    if(registered)
    {
        return;
    }
    registered = true;
    RegisterMoveScript(std::make_shared<LeechSeedScript>());
    // Novos scripts: RegisterMoveScript(std::make_shared<MeuScript>());
}
