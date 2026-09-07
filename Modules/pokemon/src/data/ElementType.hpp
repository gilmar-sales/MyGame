#pragma once

#include <cstdint>
#include <string_view>

enum class ElementType : std::int64_t
{
    None = -1,
    Normal = 0,
    Fire,
    Water,
    Grass,
    Electric,
    Ice,
    Fighting,
    Poison,
    Ground,
    Flying,
    Psychic,
    Bug,
    Rock,
    Ghost,
    Dragon,
    Count
};

enum class MoveCategory : std::int8_t
{
    Physical = 0,
    Special,
    Status
};

enum class MoveDelivery : std::int8_t
{
    Melee = 0,
    Projectile,
    Beam,
    StatusRanged
};

[[nodiscard]] inline std::string_view ElementTypeName(ElementType type)
{
    switch(type)
    {
    case ElementType::Normal:
        return "Normal";
    case ElementType::Fire:
        return "Fire";
    case ElementType::Water:
        return "Water";
    case ElementType::Grass:
        return "Grass";
    case ElementType::Electric:
        return "Electric";
    case ElementType::Ice:
        return "Ice";
    case ElementType::Fighting:
        return "Fighting";
    case ElementType::Poison:
        return "Poison";
    case ElementType::Ground:
        return "Ground";
    case ElementType::Flying:
        return "Flying";
    case ElementType::Psychic:
        return "Psychic";
    case ElementType::Bug:
        return "Bug";
    case ElementType::Rock:
        return "Rock";
    case ElementType::Ghost:
        return "Ghost";
    case ElementType::Dragon:
        return "Dragon";
    default:
        return "None";
    }
}

[[nodiscard]] inline ElementType ElementTypeFromInt(std::int64_t value)
{
    if(value < 0 || value >= static_cast<std::int64_t>(ElementType::Count))
    {
        return ElementType::None;
    }
    return static_cast<ElementType>(value);
}
