#pragma once

#include <Frigga/Macro.hpp>
#include <Freyr/Freyr.hpp>
#include <Skirnir/Skirnir.hpp>

class PokemonHudSystem: public fr::System
{
  public:
    explicit PokemonHudSystem(const skr::Arc<fr::Registry> &registry);

    void Update(float deltaTime) override;
};
