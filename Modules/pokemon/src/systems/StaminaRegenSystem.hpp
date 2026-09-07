#pragma once

#include <Frigga/Macro.hpp>
#include <Freyr/Freyr.hpp>
#include <Skirnir/Skirnir.hpp>

class StaminaRegenSystem: public fr::System
{
  public:
    explicit StaminaRegenSystem(const skr::Arc<fr::Registry> &registry);

    void Update(float deltaTime) override;
};
