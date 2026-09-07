#include "data/TypeChart.hpp"

#include <array>

namespace
{
    constexpr int kCount = static_cast<int>(ElementType::Count);

    // Rows = attack type, cols = defend type.
    // Compact codes: 0=immune, 1=neutral, 2=super-effective, 5=not-very (0.5x).
    constexpr std::uint8_t kNeutral         = 1;
    constexpr std::uint8_t kNotVeryEffective = 5;
    constexpr std::uint8_t kSuperEffective  = 2;
    constexpr std::uint8_t kImmune          = 0;

    // clang-format off
    //              Nor Fir Wat Grs Ele Ice Fgt Poi Gnd Fly Psy Bug Roc Gho Dra
    constexpr std::array<std::array<std::uint8_t, kCount>, kCount> kChart {{
        /*Normal*/   {{kNeutral, kNeutral, kNeutral, kNeutral, kNeutral, kNeutral, kNeutral, kNeutral, kNeutral, kNeutral, kNeutral, kNeutral, kNotVeryEffective, kImmune, kNeutral}},
        /*Fire*/     {{kNeutral, kNotVeryEffective, kNotVeryEffective, kSuperEffective, kNeutral, kSuperEffective, kNeutral, kNeutral, kNeutral, kNeutral, kNeutral, kSuperEffective, kNotVeryEffective, kNeutral, kNotVeryEffective}},
        /*Water*/    {{kNeutral, kSuperEffective, kNotVeryEffective, kNotVeryEffective, kNeutral, kNeutral, kNeutral, kNeutral, kSuperEffective, kNeutral, kNeutral, kNeutral, kSuperEffective, kNeutral, kNotVeryEffective}},
        /*Grass*/    {{kNeutral, kNotVeryEffective, kSuperEffective, kNotVeryEffective, kNeutral, kNeutral, kNeutral, kNotVeryEffective, kSuperEffective, kNotVeryEffective, kNeutral, kNotVeryEffective, kSuperEffective, kNeutral, kNotVeryEffective}},
        /*Electric*/ {{kNeutral, kNeutral, kSuperEffective, kNotVeryEffective, kNotVeryEffective, kNeutral, kNeutral, kNeutral, kImmune, kSuperEffective, kNeutral, kNeutral, kNeutral, kNeutral, kNotVeryEffective}},
        /*Ice*/      {{kNeutral, kNotVeryEffective, kNotVeryEffective, kSuperEffective, kNeutral, kNotVeryEffective, kNeutral, kNeutral, kSuperEffective, kSuperEffective, kNeutral, kNeutral, kNeutral, kNeutral, kSuperEffective}},
        /*Fighting*/ {{kSuperEffective, kNeutral, kNeutral, kNeutral, kNeutral, kSuperEffective, kNeutral, kNotVeryEffective, kNeutral, kNotVeryEffective, kNotVeryEffective, kNotVeryEffective, kSuperEffective, kImmune, kNeutral}},
        /*Poison*/   {{kNeutral, kNeutral, kNeutral, kSuperEffective, kNeutral, kNeutral, kNeutral, kNotVeryEffective, kNotVeryEffective, kNeutral, kNeutral, kSuperEffective, kNotVeryEffective, kNotVeryEffective, kNeutral}},
        /*Ground*/   {{kNeutral, kSuperEffective, kNeutral, kNotVeryEffective, kSuperEffective, kNeutral, kNeutral, kSuperEffective, kNeutral, kImmune, kNeutral, kNotVeryEffective, kSuperEffective, kNeutral, kNeutral}},
        /*Flying*/   {{kNeutral, kNeutral, kNeutral, kSuperEffective, kNotVeryEffective, kNeutral, kSuperEffective, kNeutral, kNeutral, kNeutral, kNeutral, kSuperEffective, kNotVeryEffective, kNeutral, kNeutral}},
        /*Psychic*/  {{kNeutral, kNeutral, kNeutral, kNeutral, kNeutral, kNeutral, kSuperEffective, kSuperEffective, kNeutral, kNeutral, kNotVeryEffective, kNeutral, kNeutral, kNeutral, kNeutral}},
        /*Bug*/      {{kNeutral, kNotVeryEffective, kNeutral, kSuperEffective, kNeutral, kNeutral, kNotVeryEffective, kSuperEffective, kNeutral, kNotVeryEffective, kSuperEffective, kNeutral, kNeutral, kNotVeryEffective, kNeutral}},
        /*Rock*/     {{kNeutral, kSuperEffective, kNeutral, kNeutral, kNeutral, kSuperEffective, kNotVeryEffective, kNeutral, kNotVeryEffective, kSuperEffective, kNeutral, kSuperEffective, kNeutral, kNeutral, kNeutral}},
        /*Ghost*/    {{kImmune, kNeutral, kNeutral, kNeutral, kNeutral, kNeutral, kNeutral, kNeutral, kNeutral, kNeutral, kSuperEffective, kNeutral, kNeutral, kSuperEffective, kNeutral}},
        /*Dragon*/   {{kNeutral, kNeutral, kNeutral, kNeutral, kNeutral, kNeutral, kNeutral, kNeutral, kNeutral, kNeutral, kNeutral, kNeutral, kNeutral, kNeutral, kSuperEffective}},
    }};
    // clang-format on

    [[nodiscard]] float Decode(std::uint8_t code)
    {
        switch(code)
        {
        case kImmune:
            return 0.0f;
        case kNotVeryEffective:
            return 0.5f;
        case kSuperEffective:
            return 2.0f;
        default:
            return 1.0f;
        }
    }
} // namespace

float TypeChart::Multiplier(ElementType attack, ElementType defend)
{
    if(attack == ElementType::None || defend == ElementType::None)
    {
        return 1.0f;
    }
    const int a = static_cast<int>(attack);
    const int d = static_cast<int>(defend);
    if(a < 0 || a >= kCount || d < 0 || d >= kCount)
    {
        return 1.0f;
    }
    return Decode(kChart[static_cast<std::size_t>(a)][static_cast<std::size_t>(d)]);
}

float TypeChart::Multiplier(ElementType attack, ElementType defendA, ElementType defendB)
{
    float mult = 1.0f;
    if(defendA != ElementType::None)
    {
        mult *= Multiplier(attack, defendA);
    }
    if(defendB != ElementType::None && defendB != defendA)
    {
        mult *= Multiplier(attack, defendB);
    }
    return mult;
}
