# Framework

Reusable **C++** game framework: windowing / OpenGL, ECS scenes, Luau scripting, audio (FMOD), match/session, and networking.

Games consume this as a library (e.g. git submodule, then `add_subdirectory`, then link static `Framework`). The engine owns `main`; the game supplies content and hooks (`GameParseArgs`, `GameRegisterContent`, `CreateApplication`). This repo is the **engine only** - sample games stay elsewhere.

## Build

```bash
cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug -DFW_ENABLE_LUAU=ON -B build -S .
cmake --build build --target Framework BlankHost
```

`BlankHost` (`samples/blank`) links Framework only.

### Windows run note

BlankHost needs runtime DLLs on `PATH` or next to the exe:

- SDL2: `vendor/SDL2/bin/` (`SDL2.dll`, `SDL2_image.dll`, `SDL2_ttf.dll`, etc.)
- FMOD: from your local SDK (`vendor/FMOD/api/core/lib/x64/fmod.dll`)

## Layout

| Module | Role |
|--------|------|
| **Core** | App window, paths, assets, settings, GL context |
| **Render** | 2D renderer, targets, atlas, present |
| **Scene** | ECS scene and systems |
| **Script** | Luau host + engine bindings |
| **Audio** | FMOD music / debug |
| **Events** | Input / app events |
| **Match** | Session, input record/playback |
| **Net** | Sockets, session, lobby |
| **Utils / Debug** | Helpers, HUD |

## Vendor

Open deps under `vendor/` (SDL2, Box2D, EnTT, spdlog, json, Luau, etc.).

**FMOD** is proprietary - place the SDK at `vendor/FMOD/` locally (gitignored). Get a license from Firelight.

## Paths

`Paths::Init` uses `FW_ASSET_ROOT`, else walks up from the executable until it finds a `res/` folder, else cwd.

## Game hooks

Engine `main` calls into game-provided hooks such as:

- `GameParseArgs` - identity / CLI
- `GameRegisterContent` - atlas, extra Lua binds, etc.
- `CreateApplication` - app loop

Product saves, modes, and content lists stay in the game repo.
