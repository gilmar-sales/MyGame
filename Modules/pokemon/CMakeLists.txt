frigga_add_module(pokemon
  src/PokemonModule.cpp
  src/data/TypeChart.cpp
  src/data/PokemonCatalog.cpp
  src/systems/PokemonStatsSystem.cpp
  src/systems/StaminaRegenSystem.cpp
  src/systems/StatusEffectSystem.cpp
  src/systems/WildSpawnSystem.cpp
  src/systems/WildAISystem.cpp
  src/systems/MoveInputSystem.cpp
  src/systems/MoveCombatSystem.cpp
  src/systems/ProjectileSystem.cpp
  src/systems/ItemUseSystem.cpp
  src/systems/PokemonHudSystem.cpp
)
target_include_directories(pokemon PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/../charactermovement/src/components"
)
