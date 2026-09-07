#pragma once

#include <algorithm>
#include <cmath>

struct StatBlock
{
    int hp      = 0;
    int attack  = 0;
    int defense = 0;
    int spAttack = 0;
    int spDefense = 0;
    int speed   = 0;
};

/// Mainline-inspired formulas (no Nature / EVs).
[[nodiscard]] inline int CalcHp(int base, int iv, int level)
{
    level = std::clamp(level, 1, 100);
    iv    = std::clamp(iv, 0, 31);
    base  = std::max(base, 1);
    return ((2 * base + iv) * level) / 100 + level + 10;
}

[[nodiscard]] inline int CalcOther(int base, int iv, int level)
{
    level = std::clamp(level, 1, 100);
    iv    = std::clamp(iv, 0, 31);
    base  = std::max(base, 1);
    return ((2 * base + iv) * level) / 100 + 5;
}

[[nodiscard]] inline float CalcMaxStamina(float baseStamina, int level)
{
    level = std::clamp(level, 1, 100);
    return baseStamina + static_cast<float>(level) * 1.5f;
}

[[nodiscard]] inline int CalcDamage(float power, int offense, int defense, float stab,
                                    float typeMult, float random)
{
    if(power <= 0.0f || typeMult <= 0.0f)
    {
        return 0;
    }
    offense = std::max(offense, 1);
    defense = std::max(defense, 1);
    const float raw =
        ((power * static_cast<float>(offense) / static_cast<float>(defense)) * 0.5f + 2.0f) *
        stab * typeMult * random;
    return std::max(1, static_cast<int>(std::floor(raw)));
}
