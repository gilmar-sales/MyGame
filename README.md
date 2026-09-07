# MyGame

Frigga project (3d template).

## Layout

- `frigga.project` — project metadata
- `input.json` — named Actions / Axes bindings
- `ecs.json` — ECS pipeline / system layout (created on first Editor open)
- `Scenes/main.json` — default scene
- `Resources/` — models, textures, prefabs, and fonts owned by this project
- `Modules/` — optional user-organized shared libraries
- `include/frigga_user_components.hpp` — FriSet / FriTryGet helpers

## Branding and publishing metadata

The `publish` section of `frigga.project` controls the executable identity: `displayName`, `executableName`, `publisher`, `copyright`, `version`, and `identifier`. Optional platform icons can be set with `iconWindows` (`.ico`), `iconLinux` (`.png`), and `iconMacOS` (`.icns`).

## Project components

1. Declare `struct Foo : fr::Component { float x; };`
2. In `FRI_MODULE`: `module.Component<Foo>()`
3. Build + **Reload Gameplay Module**.
4. In the Editor: Entity → Add Component → Gameplay → Foo.
5. In a Freyr `System::Update`: `CreateMutation()->Each([](fr::Entity, Foo& foo) { ... })` (Simulation pipeline — Play mode only).

## Gameplay systems

Inherit `fr::System` and register with `module.System<MySystem>()` (defaults to the **Simulation** pipeline).
Optional DI: `module.Singleton<T>()`, `.Scoped<T>()`, `.Transient<T>()`.
Host exposes `fg::Input`, `fg::Physics`, and `fg::AudioController` — inject them in system ctors (`IsDown`/`WasPressed`/`GetAxis`, `MoveCharacter`/`SetLinearVelocity`, `Play`/`Stop` on entities with `AudioSourceComponent`).
Host placement: new module systems append to **Simulation** (60 Hz, Play only); known labels are restored from `ecs.json` after attach. Edit pipelines in the **ECS** workflow.
Tick order: **Simulation** (gameplay + physics) → **Main** (audio + camera) → **Render** (animation preview + draw, always last). Edit mode keeps only Render; Simulation and Main tick in Play.

## Build modules

Requires a **C++26** compiler with reflection (GCC 16+ or Clang 22+), same as Frigga.

Point CMake at the packaged self-contained `Sdk/` next to the Editor (or the engine tree):

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DFRIGGA_SDK=/path/to/Sdk
cmake --build build
```

Alternatively set the `FRIGGA_SDK` environment variable, or use `cmake --preset default` after the Editor has written local `CMakeUserPresets.json` (gitignored).
The SDK contains `include/Frigga`, dependency headers, and the shared `cmake/FriggaSdk.cmake` module helper. Engine developers can point `FRIGGA_SDK` at the source tree and optionally pass `-DFRIGGA_BUILD=` (Editor binary dir with `_deps/`) for local dependency headers.

Or use **File → Build Gameplay Module** (Ctrl+B) in the Editor (passes SDK paths and forces `gnu++26` + `-freflection`).

## Publish the game

Use **Project → Publish Game...** in the Editor and choose an empty destination folder. The Editor configures a separate `build-release/` tree and compiles the project executable and enabled modules in Release. The result contains the project executable, scene, Resources, and modules without requiring the Editor, SDK, CMake, or source tree.

## Debug gameplay code

1. Keep the Frigga Editor open on this project.
2. Open this folder in VS Code with the Frigga extension.
3. Run **Frigga: Attach Debugger to Editor** (requires C/C++ extension / GDB).
4. Set breakpoints in your gameplay sources and hit Play in the Editor.
