#pragma once

#include <Frigga/Macro.hpp>
#include <Frigga/Input/Input.hpp>

#include <Freyr/Freyr.hpp>
#include <Skirnir/Skirnir.hpp>

class MoveInputSystem: public fr::System
{
  public:
    MoveInputSystem(const skr::Arc<fr::Registry> &registry, const skr::Arc<fg::Input> &input);

    void Update(float deltaTime) override;

  private:
    skr::Arc<fg::Input> mInput;
};
