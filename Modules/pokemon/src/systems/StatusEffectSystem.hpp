#pragma once

#include <Frigga/Macro.hpp>
#include <Freyr/Freyr.hpp>
#include <Skirnir/Skirnir.hpp>

class StatusEffectSystem: public fr::System
{
  public:
    explicit StatusEffectSystem(const skr::Arc<fr::Registry> &registry);

    void Update(float deltaTime) override;
};
