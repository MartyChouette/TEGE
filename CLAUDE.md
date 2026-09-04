# CLAUDE.md - Enjin Engine Project Context

## Git Commit Rules

- **NEVER include a Co-Authored-By line in commits.** No byline, no attribution footer. Just the commit message.

## Overview

Enjin is an open-source (BSL 1.1) game engine built from scratch using C++20. It features a complete editor with ImGui, an Entity-Component-System architecture, and modern rendering with multi-backend support (Vulkan, WebGPU, Metal planned).

## Traps & Rules

These are hard-won lessons. Violating any of these will cause bugs.

### Naming & API
- **`InputSystem` namespace**, not `Input` — `Enjin::Input` is an existing class
- **`NotesComponent` field is `.notes`**, not `.text`
- **`LightComponent` has no direction field** — extract direction from `TransformComponent` rotation
- **Log categories:** `Network` (not `Networking`), `Script` (not `Scripting`), `Build`, `Player`
- **Math headers:** `Enjin/Math/Vector.h` (not `Vector3.h`). `Matrix4` uses flat `f32 m[16]`
- **`SceneSerializer`** requires `World*` in constructor
- **File extensions:** `.enjinproject` = project manifest (JSON), `.enjin` = scene (JSON). There is no `.enjscene`
- **Scene JSON script key is `scriptComponent`**, not `script` — unknown entity keys are silently ignored on load and erased by the next save (no warning anywhere)
- **Scene JSON top-level `"version"` MUST be the STRING `"1.0"`**, not the number `1.0` — a bare number fails the ENTIRE scene load with `type must be string, but is number` (this broke two hand-authored probe scenes before being diagnosed 2026-08-23). Also: the editor resolves a launch project path against its own CWD — pass ABSOLUTE paths to `EnjinEditor <project> --play --golden ...` or it silently opens no project and captures the empty hub
- **The C++ facade is `Enjin::App` / `Enjin::App2D`** (`Enjin/App.h`, source in `Engine/src/App/`) — renamed from SimpleApp. `ENJIN_SIMPLE_MAIN` emits WinMain on Windows
- **Path validation:** use `Platform::IsSafeRelativePath` / `IsSafeFileName` / `ResolveWithinRoot` from `Enjin/Platform/Paths.h` — never hand-roll `find("..")` checks

### ECS
- **Structural mutation is MAIN-THREAD ONLY (adr-0004):** `World::GetComponent`/`HasComponent` are lock-free reads. That is only safe because `Add`/`Remove`/`Create`/`Destroy`/`Clear` run solely on the owner thread (the one that constructed the World) and every parallel region is fork-join (the owner is parked at the join while workers read). NEVER add/remove components or create/destroy entities from a worker thread — worker threads may only READ. `World::AssertOwnerThread()` catches violations in all builds (debug aborts, release logs a loud error once). If you add a job system that must mutate components off-thread, this invariant is broken and you must redesign it (do not just silence the guard). `IsValid`/`IsPendingDestruction`/`FindEntityByName` keep their locks; the read hot path does not
- **`DestroyEntity()` is deferred** — flushed at `World::Update()` start. `IsValid()` returns false for pending-destruction entities
- **Entity IDs are generational:** destroying recycles the slot index but bumps the generation, so a recreated entity never equals the old handle. Compare slots with `EntityIndex(e)`
- **`MeshComponent.subMeshes`** array for multi-material meshes. `MaterialSlotsComponent` holds per-sub-mesh materials

### UI / Scenes
- **UICanvas anchors are Unity-style:** `edge = anchor*parent + offset`. A centered 200-wide element needs `offsetLeft=-100, offsetRight=+100` — writing `+100/-100` produces negative width and falls into a legacy center-on-anchor fallback that puts top-anchored elements off-screen
- **`isStartScene` is the only start-scene authority** — `SceneEntry.buildIndex` is ordering/inclusion only (`-1` = not in build). `SceneManager::NormalizeSceneList()` repairs invariants on project load/save
- **TopDown3D camera:** `cameraAngle` is pitch from horizontal, NOT yaw — never rotate movement input by it. The follow camera sits at +Z looking -Z, so up input = -Z

### Physics
- **STRICT 2D/3D separation:** Box2D for 2D scenes only, Jolt for 3D only. Never mix. 2D controllers use `CheckGround2D`/`CheckWall2D`, 3D use `CheckGround`/`CharacterVirtual`
- **Collider sizes are WORLD SPACE:** Jolt/Box2D do NOT multiply by transform scale. `BoxColliderComponent.size = (50, 0.1, 50)` means 50 world units regardless of entity scale
- **Capsule height convention:** `height` = cylinder only (between hemispheres). Total = `height + 2*radius`. For `CharacterVirtual`: `totalHalfH = height/2 + radius`
- **Box2D kinematic bodies:** Use `b2Body_SetLinearVelocity` (not `SetTransform`) — teleporting doesn't trigger sensor events
- **Box2D sensor events:** `enableSensorEvents` must be `true` even on static shapes. Formula: `enableSensorEvents = isSensor || !isStatic`
- **2D raycasts skip sensors:** `Box2DBackend::Raycast` filters out sensor bodies by design
- **Hazard sensors** must be `isKinematic = true` (not `isStatic`). Box2D v3 doesn't fire events between static sensors and kinematic visitors. Use `gravityScale = 0`
- **`CheckHazardOverlaps`** is a manual AABB check each frame — workaround for Box2D v3 kinematic-kinematic sensor limitation
- **3D character controllers:** Use `JPH::CharacterVirtual` (not manual raycasts). Self-excluded from own raycasts via `EnjinBodyFilter.ignoreBodyID`

### Renderer
- **Web draw order: the procedural sky must come after the opaque meshes and BEFORE anything alpha-blended.** It is a fullscreen triangle at z=1 with `LessEqual`, and particles/sprites do not write depth, so a sky drawn last paints over everything silhouetted against open sky. Any new transparent pass goes after the sky block in `RenderSystem::Update`'s web path
- **Shaders that load from a FILE (compute, visibility buffer, upscalers) must go through `Renderer::ShaderSearchPaths()`** (`Enjin/Renderer/ShaderPaths.h`) — never a hand-written array of candidate paths. Nineteen hand-written copies existed and not one looked in `../share/enjin/shaders/`, where `cmake --install` and the DEB package put them, so an installed build found no compute shaders at all
- **Descriptor set binding 2** is a plain `STORAGE_BUFFER` (adr-0003) — the material SSBO is a runtime array indexed per draw by `firstInstance` → `gl_InstanceIndex` → `v_MaterialIndex`. ALL set-0 `vkCmdBindDescriptorSets` calls pass `dynamicOffsetCount=0, nullptr`; direct entity draws MUST pass `GetMaterialIndex(entity)` as the draw's `firstInstance`. Set 0 uses `UPDATE_AFTER_BIND` on bindings 2-23 (pool + layout flags must stay in sync); this is what legalizes the per-entity bone/morph/sprite descriptor writes mid-recording
- **WGSL is compiled by the BROWSER, so a green web build proves nothing about it.** Run `cd tools && npm install && node check_wgsl.mjs` after ANY edit to `WebShaderData.h` — it compiles all 12 shaders through Dawn, the same compiler Chrome uses, and reports errors with line numbers. Without it a syntax or type error reaches a player as a black canvas. CI runs it in the web job. (Headless Chrome on this machine exposes `navigator.gpu` but returns no adapter, so a browser cannot be used for this)
- **WGSL rule:** `textureSample` must be called from uniform control flow — never inside `if` branches that depend on per-vertex/per-fragment data. A branch on a UNIFORM-buffer value is fine, and the implicit form compiles there — measured against Dawn, after a comment in WebShaderData.h claimed otherwise for years
- **Render pass formats:** Swapchain = `B8G8R8A8_SRGB` with MRT. Offscreen `RenderTarget`s = `B8G8R8A8_UNORM`, single color + depth, `colorAttachmentCount=1`, `SAMPLE_COUNT_1_BIT` (no MSAA)
- **WebGPU depth-only pass:** `Depth32Float`, no stencil — stencil ops MUST be `Undefined`. Shadow pipeline has no fragment shader
- **WebGPU tangent fallback:** PBR shader checks `dot(tangent,tangent) > 0.001` — without this, `normalize(vec3(0))` produces NaN and kills all lighting
- **`MaterialGPU` = 144 bytes** — struct alignment matters for SSBO offsets (guarded by `static_assert` in TestMaterial). Was 80 (pre-SSS), then 112 (SSS + bindless), then 128 (scrolling-reflection row), now 144 (F1 UV-animation row: uvScrollU/V + flipbookGrid + flipbookFps). Update BOTH TestMaterial checks (static_assert AND the runtime test) — updating only one left CI red for two days. Keep this in lockstep across THREE places: the C++ `MaterialGPU` struct, the shader `MaterialEntry` struct (triangle.frag), and the `static_assert` in TestMaterial — then regen ShaderData.h and verify a golden capture is byte-identical (a mismatch corrupts ALL materials)
- **Any pipeline used in the swapchain main pass needs a 2-attachment blend state** (MRT color + velocity, VUID-07609) — extra attachment gets `colorWriteMask = 0`. `third_party/imgui` is now tracked normally in the repo (the old "untracked nested git clone" state is gone), but its backend carries the MRT patch: re-apply `third_party/patches/imgui-mrt-colorattachmentcount.patch` after any imgui update (ImGuiLayer guards the field behind `IMGUI_IMPL_VULKAN_HAS_COLOR_ATTACHMENT_COUNT`)

### Shaders (CRITICAL)
- After ANY change to `LightingUBO`, `UniformBufferObject`, `MaterialGPU`, or other UBO/SSBO structs: **recompile ALL affected shaders AND regenerate `ShaderData.h`**. Stale SPIR-V = GPU reading wrong offsets (dark scenes, wrong colors, crashes)
- **If a shader edit visibly does NOTHING, suspect a FOSSIL BAKED ARRAY**: PostProcessing.cpp carried its own `static const u32 PostProcessFragmentShader[]` copy that shadowed the generated `ShaderData::PostProcessFragmentShaderData` for months — postprocess.frag edits compiled into ShaderData.h but never ran, and as PostProcessSettings grew the fossil read the UBO at stale offsets (ghost/wash/offset artifacts, panel toggles inert; fixed 2026-08-07). Verify with a byte search: the exe must contain the .spv's bytes. **OITManager.cpp's two baked arrays (OITFullscreenVertexShader, OITCompositeFragmentShader) were migrated to `ShaderData::FullscreenVertexShaderData` / `OitCompositeFragmentShaderData` on 2026-08-20 — no known fossils remain, but keep watching for new `static const u32 …Shader[]` copies**
- Shader edit workflow: edit GLSL → `glslangValidator -V` → `python _gen_all.py` → rebuild. No shortcuts
- **RT shaders:** compile with `glslc --target-env=vulkan1.2 -I.` (they `#include rt_common.glsl`; glslangValidator rejects the include) → `python _gen_rt.py` regenerates `RTShaderData.h`
- **GLSL cannot pass unsized arrays as function parameters** — helpers that loop over an SSBO array must be inlined in each shader that declares the SSBO (see rt_reflect.rgen SDF loop)

### Scripting Runtime
- **A context created before a binding is registered does not know that binding's ABI.** AngelScript decides, per registered native function, how the host returns its value (for a float: floating-point register or integer register) inside `PrepareEngine()`, whose only public trigger is `CreateContext()`. Every later registration marks that work stale and NOTHING redoes it, because redoing it is tied to creating a context and one already exists. `ScriptEngine` therefore fills its context pool on FIRST USE, never in `Initialize()` — the engine registers its bindings after `Initialize()` returns. If you register bindings later than that, call `ScriptEngine::InvalidateContextPool()`. Symptom when this is wrong: every float-returning binding hands back a leftover address, so the value is nonsense, changes with every process start, and is stable within a run — and turning off address randomisation makes it look like a compiler bug. Cost: the Linux CI job was red on every run for weeks (fixed 2026-09-04, regression tests in TestScriptEngine)
- **ALL AngelScript binding registrations MUST use the `ENJIN_AS_*` macros from `Enjin/Scripting/ASCallConv.h`** (`ENJIN_AS_FN`/`ENJIN_AS_MFN`/`ENJIN_AS_OBJ_FIRST`/`ENJIN_AS_OBJ_LAST` + matching `ENJIN_AS_CALL_*`), never raw `asFUNCTION`+`asCALL_CDECL`. On WASM AngelScript forces `AS_MAX_PORTABILITY` (no native calling conventions) and raw registrations fail with asNOT_SUPPORTED (-7) — before 2026-08-09 the web player registered ZERO bindings so every script failed to compile. On desktop the macros expand to the native forms unchanged. Overloaded functions need `_PR` variants — extend the header first. `SetMessageCallback`/`SetLineCallback` are engine-invoked and stay native
- **Web edge-input trap:** browser key/mouse events land BETWEEN frames — Emscripten callbacks in `Core/src/Platform/Input.cpp` must write the `s_Web*Latest`/`s_Web*DownLatch` pending state, never `s_KeysDown`/`s_MouseButtonsDown` directly, or `Input::Update()`'s current→previous copy erases the pressed edge and `IsKeyPressed`/`IsMouseButtonPressed` NEVER fire (Tab dead, all UI unclickable on web — fixed 2026-08-09)
- **web_main.cpp must call `Logger::Get().Initialize(...)`** (web forgot until 2026-08-09 — every `ENJIN_LOG_*` silently dropped; stdout DOES reach the browser console) **and `m_ScriptEngine.SetAssetReader(&m_AssetReader)`** (web has no loose script files; script sources come from the pak)
- **TegeBehavior + the enjin_api scripts are EMBEDDED in the engine** (`EnjinApiEmbedded.cpp`, regenerate with `python _gen_api.py` after editing `enjin_api/*.as`). TegeBehavior is auto-injected into every module unless the source mentions `TegeBehavior.as`; `#include "Timer.as"` etc. fall back to embedded copies. Project `scripts/enjin_api/` overrides
- **Script module names are `parentDir_stem`** (`scripts/Foo.as` → `scripts_Foo`) — anything creating instances must derive the name identically or CreateInstance fails
- **The process CWD is NEVER reliable** (editor/player CWD = exe dir). All relative paths resolve via roots set at play/boot: `ScriptSystem::SetScriptRoot`, `ScriptEngine::SetScriptDirectory`, `SimpleAudio::SetAssetRoot` — new path consumers must follow this pattern, never bare relative file access
- **Exported games read scripts from loose DISK files by default** — BuildPipeline emits loose `scripts/`, `scripts/enjin_api/`, and `assets/` next to the exe (`EmitLooseRuntimeFiles`), and that is what the runtime loads. Pak-side script loading now EXISTS as a fallback (`ScriptEngine::SetAssetReader` + `ReadScriptSource`/`IncludeCallback` read from the `.enjpak`), but loose files still ship and take precedence, so the pak path is not exercised in practice yet
- **The build copies a PREBUILT `EnjinPlayer.exe`** — after engine changes, rebuild the `EnjinPlayer` target too or exported games ship a stale engine

### Frame Safety (crash class: mid-frame GPU resource destruction)
- **`RenderSystem::FlushPendingChanges` is the ONLY safe home** for destroying/recreating GPU resources or updating descriptor sets. It early-returns when `m_SkipMainPassRendering` is set (the editor records offscreen binds BEFORE `World::Update`) — destroying/updating anything bound in the recording command buffer invalidates it and the driver access-violates at submit
- **Parallel shadow path activates at ≥32 shadow casters.** With a render pass begun for SECONDARY_COMMAND_BUFFERS, the primary may record ONLY `vkCmdExecuteCommands`. Per-command-buffer state (e.g. merged-pool VB/IB bound) must never live in shared members — pass it per-CB (`RenderEntityShadow`'s `bool& poolBound`)
- **Editor auto-saves the open scene on a timer** — never modify scene files out-of-band while an editor instance is running; close it first
- **The "parented entities render eulers with Y/Z swapped" bug is FIXED (2026-08-23)** — root cause was never the render path (ComputeWorldMatrix/FromEuler/script bindings all proven correct by tests): the viewport GIZMO write-back rebuilt rotations from ImGuizmo-decomposed angles with a hand-rolled Y*X*Z product instead of the engine's ZYX `FromEuler` (= ImGuizmo's exact convention), silently corrupting any compound LOCAL rotation (typically a child under a rotated parent) on every gizmo drag, including pure translations. Do NOT put yaw in the Z slot anymore; content authored with that workaround needs its rotations re-authored. Euler convention everywhere: `Quaternion::FromEuler` ZYX intrinsic (x=pitch applied first, then y=yaw, then z=roll); any new euler→quat site MUST use FromEuler, never hand-rolled axis products (test: GizmoEulerConvention)

### Input / Touch
- **`GameAction` is APPEND-ONLY** (ordinals persist in `bindings.json` and mirror the script enum). Every action is one row in `kActionInfo` (`InputActionMap.cpp`): name, category, defaults, touch hint, hint verb. `LoadDefaults`, menus, touch presets and the controls hint all read that table; a new action = a new row + a script enum value
- **Touch presets live in Engine (`TouchActionBridge`), Core knows nothing about actions.** Core owns hit-zones/geometry/state only. Presets are lists of consumed actions per controller type and drive BOTH the touch buttons and the bottom-left controls hint, so a scene only shows controls it has. Apply via `ApplyTouchPresetForWorld` BEFORE scripts tick (OnStart additions must survive)
- **Project input settings live in `.enjinproject` under `input`** (`SceneManager::GetInputSettings`, struct `InputSystem::InputProjectSettings`): custom action names/bindings + touch layout + touch accessibility (left-handed mirror, button scale). BuildPipeline carries the block verbatim into the game manifest (like `startupFlow`) and all three runtimes load it BEFORE the player's `bindings.json`, so a project sets defaults and a rebind still wins. Editor UI = Project Settings > Input & Touch
- **`ActionTriggerComponent` is the components-only control path** (no script): pick an action, pick an effect (time scale / show-hide / event / subtitle). `ActionTriggerSystem` runs it in all three runtimes and `Reset()` must be called on stop or a bullet-time trigger strands the editor's time scale. A trigger with `touchButton` also CONTRIBUTES its own on-screen button
- **The touch scheme is rebuilt on a FINGERPRINT, not just a preset change** (`ApplyTouchPresetForWorld`): controller preset + every scene ActionTrigger button + project overrides. That is what makes dropping a component into a scene show its touch button immediately
- **One input focus, one pointer-capture flag (`Enjin::Input`)**: `SetInputFocus(Gameplay|Menu|Dialogue|Console)` is enforced inside `InputActionMap` (non-UI actions read inactive off Gameplay; UI-category actions always pass), and `SetUIConsumedPointer` makes mouse-bound actions + the camera-drag paths ignore a click the UI took. Set BOTH once per frame per runtime; do not add another per-system boolean
- **Touch hits UI FIRST** (`Input::SetUIHitTestResolver`, installed via `InputSystem::SetUIHitTestSystem(&uiSystem)`): a finger on an interactive element becomes a real pointer (press/drag/release, touch role 3), so sliders work and the move-stick zone no longer swallows the left half of the screen. `UISystem::Update` runs layout -> input (topmost canvas first, stops at the first interactive hit) -> draw, which is also what finally makes `UIWidgetType::Modal` block instead of only dimming
- **ONE `InputActionMap` in the editor** — `EditorLayer` owns it and `PlayMode` BORROWS it (`PlayMode::SetInputActionMap`, injected before `PlayMode::Initialize`). It used to keep its own, so ControllerSystem + the Controls menu read one map while script bindings, `SetTouchActionMap` and ActionTriggers read another, and a rebind moved only half of them. **PlayMode must never call `m_InputMap->Update()`** — its owner ticks it once per frame, and a second tick in the same frame eats every pressed edge (Toggle actions and one-shot presses silently stop working)
- **Sprint/crouch mode, mouse sensitivity and invert-Y live ONLY on `InputActionMap`** (persisted in `bindings.json`). They were duplicated into `RuntimeAccessibilitySettings` and `EditorSettings` where nothing read them, so the Controls menu and the Accessibility menu edited different state. Old `accessibility.json` files migrate once, only when no `bindings.json` exists yet. Do not re-add copies
- **Dialogue and menu navigation read ACTIONS** (`DialogueAdvance`, `UIConfirm`, `UINav*`), wired via `DialogueSystem::SetInputActionMap` / `UISystem::SetInputActionMap` in all three runtimes. Both fall back to the historic keys when no map is attached (headless tests). Do not add hardcoded Space/Enter/W/S reads back
- **An exported game ships the PROJECT's accessibility defaults** (`accessibilityDefaults` in `.enjinproject` -> BuildPipeline -> `accessibility.json`), authored from Project Settings > Accessibility Defaults. `RuntimeAccessibilitySettings::ToJson/FromJson` is the ONE serializer for these; do not hand-roll another key list
- **`AlternativeInputManager` is ticked by its owner only** (EditorLayer, or the Player). PlayMode must not tick it too: during editor play both ran, so switch scanning advanced at double the configured speed. Its enable flag and scan speed come from `RuntimeAccessibilitySettings` via `ApplyAccessibilitySettings`, the same values `UISystem` gets; the two scan-target lists stay separate on purpose (app chrome vs game UI)
- **`Accessibility::ApplyTextScale` is the one place that knows which systems draw text** (UI + subtitles + announcer). Push font scale through it, never `uiSystem.SetFontScale` alone, or subtitles and the screen-reader bar silently stay unscaled
- **The touch overlay compiles on all platforms.** `EnjinPlayer --touch` and editor View > Simulate Touch Controls make the mouse one touch. Game-specific buttons (Playground SLO-MO) are `Custom0..7` actions named+bound from script plus `Touch_AddActionButton`, never hardcoded ImGui windows in a player

### Platform Integration
- **A change inside a platform guard is UNVERIFIED until that platform builds it, and there are THREE.** Windows, Linux (WSL Ubuntu reproduces CI exactly) and web all have to compile. A broken string literal inside a `#else` built clean on MSVC four times before Linux caught it; a `#include <execinfo.h>` under `ENJIN_PLATFORM_LINUX` broke the web build, because **Emscripten reports itself as Linux** and takes the POSIX branch. Anything POSIX-only needs `#if !defined(__EMSCRIPTEN__)` around it
- **Opening a file, a folder, a URL or a built game goes through `Enjin/Platform/Desktop.h`** (`OpenInDesktop` / `RevealInFileManager` / `OpenUrlPreferChromium` / `LaunchDetached`) — never a fresh `ShellExecuteA` / `fork`+`execlp` / `posix_spawnp` at the call site. Five hand-rolled variants existed and most had a Windows branch with an empty `#else`, so on Linux Open Folder, Run in Browser, Show in Explorer and Launch game all silently did nothing (one of them printed "Launching game..." while launching nothing). Every function returns whether the action started — report failure, do not imply success
- **A file dialog returning `""` does NOT mean the user cancelled.** On Linux it also means no dialog helper is installed. Call `FileDialog::IsAvailable()` before offering one. Supported helpers: zenity, qarma, yad, matedialog, kdialog
- **Linux saves live under `XDG_DATA_HOME`** (`~/.local/share/enjin/saves`), not `~/.config`. The legacy config path is still read when it exists and the new one does not, so do not "clean that up"

### Windows C++ Gotchas
- `near` and `far` are reserved macros (`windef.h`) — don't use as variable names

### Build System
- **`file(GLOB_RECURSE ... "src/**/*.cpp")` silently drops depth-1 files.** CMake does not read `**` as "zero or more directories" — it requires at least one. `GLOB_RECURSE` already recurses, so the pattern is `src/*.cpp`. Both engine and core use that plus `CONFIGURE_DEPENDS`; a `.cpp` placed directly in `src/` under the old pattern would never have compiled, even after a clean reconfigure
- **Release debug info is opt-in per target** via `${ENJIN_PDB_COMPILE_OPTIONS}` / `${ENJIN_PDB_LINK_OPTIONS}`, applied to EnjinCore, EnjinEngine, EnjinEditor and EnjinPlayer. Do NOT put `/Zi` back at root scope: it applied to the vendored libraries and all 113 test executables, each writing a PDB spanning the whole engine archive (~3 GB per build). Add it to a target only when a user can be holding that binary when it crashes
- **`/MP` is required on EnjinEngine and EnjinCore.** MSBuild's `--parallel` spans projects, not the files inside one, and the engine is a single ~390-file project — without `/MP` the largest target in the build never uses more than one core

## Build Commands

```bash
# Build on Windows (Visual Studio)
cd build && cmake .. && cmake --build . --config Release

# Build on Linux/Mac
cd build && cmake .. && make -j$(nproc)

# Build for Web (WebAssembly + WebGPU)
# Requires Emscripten SDK at D:/emsdk
export EMSDK=/d/emsdk && export EMSDK_PYTHON="$EMSDK/python/3.13.3_64bit/python.exe"
"$EMSDK_PYTHON" "$EMSDK/upstream/emscripten/emcmake.py" cmake -B build-web -S . -DENJIN_PLATFORM_WEB=ON
"$EMSDK_PYTHON" "$EMSDK/upstream/emscripten/emmake.py" cmake --build build-web
# Output: build-web/bin/EnjinPlayer.{js,wasm}

# Reconfigure CMake (needed after adding new source files)
cd build && cmake ..

# Run the editor
./build/bin/Release/EnjinEditor.exe  # Windows
./build/bin/EnjinEditor              # Linux/Mac

# Serve web demo
cd web-demo && python serve.py  # http://localhost:9090
```

## Testing

- **Framework:** Custom — `ENJIN_TEST(Suite, Name)`, `ENJIN_EXPECT_*`, `ENJIN_ASSERT_*`
- **No `ENJIN_ASSERT_GT`** — use `ENJIN_ASSERT_TRUE(x > y)` instead
- **Tests are gated behind `ENJIN_BUILD_TESTS` (default OFF).** If `ctest` runs but counts look stale, the cache lost the flag and you're running frozen binaries: `cmake -DENJIN_BUILD_TESTS=ON ..`
- **Run all:** `cd build && ctest --output-on-failure`
- **Run one suite:** `cd build && ctest -R TestPhysics --output-on-failure`
- **~80 CTest targets, ~1100+ test cases** across 18 subdirectories
- **4 tests require environment:** TestAudio, TestAudioTypes, TestAssetPack, TestAssetLoaders (may show "Not Run")

## Code Conventions

- **Types:** `u8, u16, u32, u64, i8, i16, i32, i64, f32, f64, usize`
- **Namespaces:** `Enjin::Core`, `Enjin::Math`, `Enjin::Renderer`, `Enjin::ECS`, `Enjin::Editor`, `Enjin::Effects`, `Enjin::Accessibility`, `Enjin::InputSystem`, `Enjin::Build`, `Enjin::Gameplay`
- **Logging:** `ENJIN_LOG_INFO/WARN/ERROR/FATAL(Category, format, ...)`
- **API export:** `ENJIN_API` macro for DLL export

## Common Tasks

### Adding a new ECS Component
1. Create header in `Engine/include/Enjin/ECS/Components/`
2. Include in relevant systems
3. Optionally add inspector UI in `EditorLayer::DrawInspectorPanel()`

### Adding a new shader uniform
1. Update C++ UBO struct (`Light.h` for LightingUBO, `VulkanPipeline.h` for ViewProjectionUBO)
2. Update the matching GLSL struct in the shader
3. Compile: `glslangValidator -V Engine/shaders/triangle.frag -o Engine/shaders/triangle.frag.spv` (and all affected shaders)
4. Regenerate: `python _gen_all.py`
5. Update the system that uploads the uniform
6. Rebuild engine

### Building a game for export
1. Configure `BuildConfig` with project path, output directory, window settings
2. Run `BuildPipeline::Execute(config)` — packs into `.enjpak`
3. Player loads `game.enjpak` from its own directory at startup

## Project Architecture

```
enjin/
├── Core/                    # Foundation layer (no engine dependencies)
│   ├── include/Enjin/
│   │   ├── Core/           # Application, Window, Input
│   │   ├── Logging/        # Thread-safe categorized logging
│   │   ├── Math/           # Vector, Matrix, Quaternion, Spline, Noise
│   │   ├── Memory/         # Custom allocators (Stack, Pool, Linear)
│   │   └── Platform/       # Platform abstraction, types
│   └── src/
├── Engine/                  # Engine layer
│   ├── include/Enjin/
│   │   ├── AI/             # AIBehaviors, Navmesh, A* Pathfinding
│   │   ├── Animation/      # Sprite + skeletal animation, BlendTree, Retargeting
│   │   ├── Assets/         # GLTFLoader, AssimpLoader, SceneImporter, Prefab
│   │   ├── Audio/          # SimpleAudio (miniaudio), SteamAudioProcessor
│   │   ├── ECS/            # Entity-Component-System (140+ component types)
│   │   │   ├── Components/ # Transform, Mesh, Material, Light, Camera, etc.
│   │   │   └── Systems/    # RenderSystem, ControllerSystem
│   │   ├── Editor/         # EditorLayer, PlayMode, Settings, Tools
│   │   ├── Effects/        # Weather, Water, RetroEffects, Particles
│   │   ├── GUI/            # UICanvas, UISystem, DialogueTree
│   │   ├── Build/          # BuildPipeline, AssetPacker, AssetReader
│   │   ├── Gameplay/       # SaveSystem, QuestSystem, HUD, Cinematics
│   │   ├── Networking/     # LAN Multiplayer, HTTPClient
│   │   ├── Physics/        # IPhysicsBackend (Jolt/Box2D)
│   │   ├── Renderer/       # Multi-backend renderer (Vulkan/WebGPU/Metal)
│   │   ├── Scene/          # SceneSerializer, SceneManager, LevelStreaming
│   │   ├── Scripting/      # AngelScript engine, ScriptBindings
│   │   └── VisualScript/   # Blueprint-style visual scripting
│   ├── shaders/            # GLSL + WGSL shaders
│   └── src/
├── Editor/                  # Editor application (main.cpp entry point)
├── Player/                  # Standalone game player (no editor/ImGui)
├── third_party/            # External dependencies (imgui, imguizmo)
└── build/                  # Build output (bin/, lib/)
```

## Key Subsystem Notes

### Renderer
- **Multi-backend:** `IRenderBackend` with sub-interfaces (`IGPUBufferManager`, `IGPUTextureManager`, `IGPUPipelineManager`, etc.). Typed opaque handles in `GPUTypes.h`. Feature detection in `GPUCapabilities.h`
- **Backends:** Vulkan (Windows/Linux, full features), WebGPU (browser via Emscripten, PBR + shadows working), Metal (stubs)
- **Scene classification:** `Scene2D` (sprites only, shadows skipped), `Scene2_5D` (sprites+lights), `Scene3D` (full pipeline)
- **Ray tracing:** Full RT pipeline (shadows/reflections/AO/GI/path tracing, denoisers). Auto-activates on capable hardware

### Physics
- `IPhysicsBackend`/`IPhysicsBackend2D` interfaces. Jolt v5.2.0 (3D), Box2D v3.0.0 (2D)
- `PhysicsBackendFactory` creates via `PhysicsBackendType` enum (`Auto`/`Jolt`/`Box2D`)
- CMake: `ENJIN_PHYSICS_JOLT` (ON), `ENJIN_PHYSICS_BOX2D` (ON)
- Collision filtering: bilateral bitmask `(A.categoryBits & B.collisionMask) && (B.categoryBits & A.collisionMask)`. Defaults: `categoryBits = 1`, `collisionMask = 0xFFFFFFFF`

### Editor
- Settings: 3-tab window (System/Project/Scene). `OpenSettings(tab)` for programmatic tab selection
- PlayMode: Play/Pause/Stop. Scene changes persist on Stop. `PlayModeDiff` shows what changed
- Shortcuts: `1/2/3` gizmo modes, `WASD` fly cam, `Delete` delete, `Ctrl+D` duplicate, `F` focus, `Ctrl+P` command palette, `F1` Game Debug, `F2` Debug Workstation, `` ` `` console

## Security

- **Scene files:** Validate array sizes, `.contains()` before access. Vertex/index caps (10M), strings via `SafeStr()`
- **Scripts:** AngelScript sandboxed, 1M instruction limit. `#include` paths restricted to the script directory (`ScriptEngine::IncludeCallback`: canonical + `MakeRelativeToRoot` containment, depth limit 16)
- **Asset packs:** XOR obfuscation (not crypto-secure), CRC32 integrity. Path traversal rejected
- **Process execution:** No `std::system()` — use `ShellExecuteA` (Win), `fork`/`execlp` (Linux/macOS)
- **Thread safety:** See `docs/AUDIT_2026_04_12.md` for open issues
- **Trust zones:** Documented in `.enjin-boundaries.json`

## Further Reading

- `docs/CLAUDE_REFERENCE.md` - Detailed subsystem docs (component catalog, binding tables, RT pipeline, physics, audio, scripting, editor details, perf history)
- `docs/ARCHITECTURE.md` - System architecture and diagrams
- `docs/SCRIPTING_API.md` - Complete AngelScript API reference
- `docs/USER_MANUAL.md` - Component details and user guide
- `docs/ROADMAP.md` - Planned work and progress tracking
- `docs/BUILD.md` - Build guide with dependencies
- `docs/AUDIT_2026_04_12.md` - Full engine audit (55 findings)
