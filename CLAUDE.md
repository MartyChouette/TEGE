# CLAUDE.md - Enjin Engine Project Context

## Git Commit Rules

- **NEVER include a Co-Authored-By line in commits.** No byline, no attribution footer. Just the commit message.

## Overview

Enjin is a proprietary, licensable game engine built from scratch using C++20 and the Vulkan graphics API. It features a complete editor with ImGui, an Entity-Component-System architecture, and modern rendering capabilities.

## Build Commands

```bash
# Build on Windows (Visual Studio)
cd build && cmake .. && cmake --build . --config Release

# Build on Linux/Mac
cd build && cmake .. && make -j$(nproc)

# Clean rebuild (Linux/Mac)
cd build && make clean && make -j$(nproc)

# Reconfigure CMake (needed after adding new source files)
cd build && cmake ..

# Compile shaders (GLSL to SPIR-V)
glslangValidator -V Engine/shaders/triangle.vert -o Engine/shaders/triangle.vert.spv
glslangValidator -V Engine/shaders/triangle.frag -o Engine/shaders/triangle.frag.spv

# Run the editor
./build/bin/Release/EnjinEditor.exe  # Windows
./build/bin/EnjinEditor              # Linux/Mac
```

## Project Architecture

```
enjin/
├── Core/                    # Foundation layer (no engine dependencies)
│   ├── include/Enjin/
│   │   ├── Core/           # Application, Window, Input
│   │   ├── Logging/        # Thread-safe categorized logging
│   │   ├── Math/           # Vector, Matrix, Quaternion, Spline
│   │   ├── Memory/         # Custom allocators (Stack, Pool, Linear)
│   │   └── Platform/       # Platform abstraction, types
│   └── src/
│
├── Engine/                  # Engine layer
│   ├── include/Enjin/
│   │   ├── AI/             # AIBehaviors, Navmesh, A* Pathfinding
│   │   ├── Animation/      # Sprite + skeletal animation, Timeline/Sequencer
│   │   ├── Assets/         # GLTFLoader, SceneImporter, Prefab
│   │   ├── Audio/          # AudioSystem, SimpleAudio (miniaudio backend)
│   │   ├── Debug/          # Profiler, ScopeTimer, FrameData
│   │   ├── ECS/            # Entity-Component-System
│   │   │   ├── Components/ # 60+ component types (incl. joints, ragdoll, script, LOD)
│   │   │   │   ├── Controllers/  # 5 character controller types + Vehicle
│   │   │   │   └── ...
│   │   │   └── Systems/    # RenderSystem, ControllerSystem
│   │   ├── Accessibility/  # ColorblindFilter, SubtitleSystem, ContentWarning
│   │   ├── Editor/         # EditorLayer, PlayMode, EditorSettings, PerformanceStats
│   │   ├── Effects/        # Weather, Water, RetroEffects, WorldTime, SeasonalWeather
│   │   ├── Input/          # InputAction (remappable input action map)
│   │   ├── GUI/            # ImGui integration
│   │   ├── Build/          # BuildPipeline, AssetPacker, AssetReader
│   │   ├── Gameplay/       # SaveSystem, HUDSystem, QuestSystem, FootstepSystem, ObjectPool, CinematicSystem
│   │   ├── Physics/        # SimplePhysics, PhysicsWorld, ConstraintSolver
│   │   ├── Platform/       # FileDialog
│   │   ├── Plugin/         # PluginSystem, HotReload
│   │   ├── Procedural/     # LevelGenerator
│   │   ├── Renderer/       # Vulkan renderer, RenderBackend abstraction
│   │   │   └── Vulkan/     # VulkanContext, Pipeline, Buffer, etc.
│   │   ├── Scene/          # SceneSerializer, SceneManager, LevelStreaming
│   │   └── Scripting/      # ScriptEngine, ScriptBindings, TegeBehavior
│   ├── shaders/            # GLSL shaders (triangle.vert/frag)
│   └── src/
│
├── Editor/                  # Editor application (main.cpp entry point)
│
├── Player/                  # Standalone game player (no editor/ImGui)
│
├── third_party/            # External dependencies
│   ├── imgui/              # Dear ImGui (UI)
│   └── imguizmo/           # Transform gizmos
│
└── build/                  # Build output (bin/, lib/)
```

## Key Classes and Concepts

### Core Layer

- **`Math::Vector3/4`** - 3D/4D vectors with SIMD-friendly layout
- **`Math::Matrix4`** - 4x4 matrix with LookAt, Perspective, Orthographic, Inverse
- **`Math::Quaternion`** - Rotation representation
- **`Log`** - Categorized logging: `ENJIN_LOG_INFO(Category, "msg", ...)`

### ECS (Entity-Component-System)

- **`ECS::World`** - Manages entities and components
- **`ECS::Entity`** - Just a u64 ID
- **Components:**
  - `TransformComponent` - position, rotation (Euler), scale
  - `MeshComponent` - vertices (position, normal, UV, color, tangent, boneWeights, boneIndices), indices
  - `MaterialComponent` - PBR properties, textures (base color, normal, height), retro flags (flatShading, vertexSnapping, vertexSnapResolution, affineTexturing, stippleTransparency)
  - `LightComponent` - Light data (direction, color, intensity)
  - `NameComponent` - Entity name string
  - `CameraComponent` - In-game cameras with projection, weather/water settings
  - `NotesComponent` - Text annotations for entities (field: `.notes`, not `.text`)
  - `SkeletonComponent` - Shared skeleton data for skinned meshes
  - `AnimatorComponent` - Skeletal animation playback (SkeletalAnimator + AnimationStateMachine)
  - `CharacterController` - Various movement controllers (Platformer2D, TopDown2D/3D, FPS, TPS)

### Renderer

- **`VulkanContext`** - Vulkan instance, device, queues
- **`VulkanRenderer`** - Main renderer, swapchain management
- **`VulkanPipeline`** - Graphics pipeline with descriptor sets
- **`VulkanBuffer`** - GPU buffers (vertex, index, uniform, storage)
- **`RenderSystem`** - ECS system that renders all entities with Mesh+Transform, drives skeletal animation

### Descriptor Bindings

```
Binding 0: View/Projection UBO (vertex shader)
Binding 1: Lighting UBO with multi-light arrays (vertex + fragment)
Binding 2: Material UBO (fragment shader)
Binding 3: Base color texture sampler (fragment shader)
Binding 4: Shadow map sampler (fragment shader)
Binding 5: Height map for parallax mapping (fragment shader)
Binding 6: Normal map (fragment shader)
Binding 7: Bone matrix SSBO for skeletal animation (vertex shader)
```

### Push Constants (128 bytes, per-object)

```cpp
struct PushConstants {
    Matrix4 model;          // 64 bytes
    Vector3 baseColor;      // + metallic = 16 bytes
    Vector3 emissiveColor;  // + roughness = 16 bytes
    f32 emissiveStrength, opacity, alphaCutoff;
    i32 flags;              // bit field: render/alpha/texture/retro flags
    f32 parallaxScale;      // + padding = 16 bytes
};
// flags layout: bits 0-2 render, 3 skinned, 8-9 alpha mode, 10 height tex,
//   16-19 texture flags, 20-23 retro flags, 24-31 snap resolution
```

### Editor

- **`EditorLayer`** - Main editor class with ImGui panels
- **`ScenePicker`** - Ray casting for entity selection (click-to-select, rect-pick for marquee)
- **`PlayMode`** - Play/Pause/Stop game preview controls
- **Multi-select system:**
  - `m_SelectedEntities` (`std::unordered_set<ECS::Entity>`) — all currently selected entities
  - `m_PrimarySelected` — last-clicked entity, used by inspector/gizmo
  - Helper methods: `SelectEntity()`, `DeselectEntity()`, `ClearSelection()`, `IsSelected()`, `SelectRange()`, `SelectEntitiesInRect()`
  - Backward-compatible API: `GetSelectedEntity()` returns primary, `SetSelectedEntity()` clears and selects one
- **Keyboard shortcuts:**
  - `1` - Translate gizmo
  - `2` - Rotate gizmo
  - `3` - Scale gizmo
  - `4` - Toggle local/world space
  - `WASD` - Fly camera movement
  - `Space`/`E` - Move up, `Q`/`Ctrl` - Move down
  - `Shift` - Sprint
  - Hold RMB + Mouse - Look around
  - Left-click - Select entity, Double-click - Focus on entity
  - Ctrl+click - Toggle entity in/out of selection (hierarchy + viewport)
  - Shift+click - Range select in hierarchy (from primary to clicked)
  - Drag in viewport - Marquee/rubber-band selection (adds enclosed entities)
  - `Delete` - Delete all selected entities
  - `Ctrl+D` - Duplicate all selected entities
  - `F` - Focus camera on selection centroid
  - Scroll - Adjust move speed
- **Inspector multi-select:** When multiple entities selected, shows entity list + batch transform editing (position offset, rotation offset, scale multiplier with Apply buttons)
- **Gizmo multi-select:** Single entity = direct manipulation; multiple = gizmo at centroid, delta applied to all

### Skybox

- **`Skybox`** (`Engine/include/Enjin/Renderer/Skybox.h`) - Cubemap-based skybox rendering
- **`SkyboxConfig`** - Configuration struct with type, colors, sun direction, cubemap paths, rotation
- **`SkyboxType`** enum: `None`, `Cubemap`, `Procedural`, `SolidColor`
- **Editor panel:** Dedicated Skybox panel (`View > Skybox`, `EditorPanel::Skybox = 1 << 10`)
  - `DrawSkyboxPanel()` in `EditorLayer` — type combo, procedural presets, color pickers, sun direction, cubemap face paths, rotation slider
  - Procedural presets: Midday, Sunset, Dawn, Night, Overcast (set colors + sun direction)
- **Config fields:**
  - `topColor`, `horizonColor`, `bottomColor` — procedural gradient colors
  - `sunDirection` — `Vector3` for procedural sun position
  - `solidColor` — flat fill color
  - `cubemapPaths` — `std::array<std::string, 6>` face paths (Right +X, Left -X, Top +Y, Bottom -Y, Front +Z, Back -Z)
  - `rotation` — Y-axis rotation in degrees (0-360)
- **Serialization:** All fields including `sunDirection` are saved/loaded in `SceneSerializer` (both file and string paths)
- **API:** `RenderSystem::SetSkybox(config)` / `RenderSystem::GetSkyboxConfig()`

### Effects Systems

- **`WeatherSystem`** - Rain, snow, fog, storm with lightning (global scene effect)
- **`Water3D`** - 3D water plane with waves (global scene effect)
- **`RetroEffects`** - CRT, pixelation, dithering post-processing
- **`WorldTimeSystem`** - Day/night cycle with configurable speed
- **`SeasonalWeatherSystem`** - Season-based weather transitions
- Effects are configured globally and rendered only in Game View (not editor camera)

### Accessibility

- **`EditorSettings`** (`Engine/include/Enjin/Editor/EditorSettings.h`) - Persistent settings (theme, scale, colorblind, motion, input)
- **`InputActionMap`** (`Engine/include/Enjin/Input/InputAction.h`) - Remappable input system (namespace: `Enjin::InputSystem`)
- **`SubtitleSystem`** (`Engine/include/Enjin/Accessibility/SubtitleSystem.h`) - Subtitle/caption overlay
- **`ContentWarningSystem`** (`Engine/include/Enjin/Accessibility/ContentWarning.h`) - Scene content warnings
- **`RuntimeAccessibilitySettings`** (`Engine/include/Enjin/Accessibility/AccessibilitySettings.h`) - Runtime accessibility config
- **Important:** The `InputSystem` namespace was chosen to avoid collision with the existing `Enjin::Input` class. Always use `InputSystem::` not `Input::` for action-map types.

### Assets

- **`GLTFLoader`** - Loads .gltf/.glb files into GLTFScene (meshes, materials, skins, animations)
- **`SceneImporter`** - Converts GLTFScene to ECS entities, auto-generates BoxColliders, sets up skeleton/animation for skinned meshes

### Window Icon

- **`WindowDesc::iconPath`** (`const char* iconPath = nullptr`) - Optional path to a PNG icon file
- `Application.cpp` sets `windowDesc.iconPath = "icon.png"` at startup
- The engine loads the PNG via `stb_image` and calls `glfwSetWindowIcon()` to set the window icon
- Place `icon.png` next to the executable (32x32 or 64x64 recommended)
- If the file is missing, the OS default icon is used silently

### Build Pipeline & Player

- **`BuildPipeline`** (`Engine/include/Enjin/Build/BuildPipeline.h`) - Orchestrates full game export: scan project → validate assets → pack → copy player → write manifest → verify CRC32s
- **`AssetPacker`** (`Engine/include/Enjin/Build/AssetPacker.h`) - Writes `.enjpak` archives (compression + XOR obfuscation + CRC32 integrity)
- **`AssetReader`** (`Engine/include/Enjin/Build/AssetReader.h`) - Reads `.enjpak` archives at runtime (decompress + deobfuscate + verify)
- **`BuildConfig`** / **`BuildResult`** (`Engine/include/Enjin/Build/BuildReport.h`) - Build configuration (project path, output dir, window settings) and result messages
- **`VulkanImage::LoadFromMemory()`** - Loads textures from packed in-memory data (PNG/JPG via stb_image)
- **Editor integration:** `DrawBuildDialog()` in `EditorLayer` with progress tracking
- **Player app** (`Player/src/main.cpp`) - Standalone executable that loads `game.enjpak`, reads build manifest for window config, runs game loop without editor/ImGui
- **Pack format:** `.enjpak` with magic header `ENJPAK10`, per-file CRC32, XOR obfuscation with configurable key
- **Build manifest:** `_build/manifest.json` inside the pack (windowTitle, windowWidth, windowHeight, fullscreen, startScene)
- **Default pack key:** `enjin_default_pack_key_2025`

## Shader Workflow

Shaders are in `Engine/shaders/` as GLSL, compiled to SPIR-V, then embedded in `ShaderData.h`:

1. Edit `triangle.vert` or `triangle.frag`
2. Compile: `glslangValidator -V triangle.frag -o triangle.frag.spv`
3. Convert to C++ array and update `ShaderData.h`
4. Rebuild engine

## Code Conventions

- **Types:** `u8, u16, u32, u64, i8, i16, i32, i64, f32, f64, usize`
- **Namespaces:** `Enjin::Core`, `Enjin::Math`, `Enjin::Renderer`, `Enjin::ECS`, `Enjin::Editor`, `Enjin::Effects`, `Enjin::Accessibility`, `Enjin::InputSystem`, `Enjin::Build`, `Enjin::Gameplay`
- **Logging:** `ENJIN_LOG_INFO/WARN/ERROR/FATAL(Category, format, ...)` — categories include Build, Player
- **API export:** `ENJIN_API` macro for DLL export

## Current Feature Status

**Completed:**
- Vulkan renderer with Blinn-Phong lighting
- ECS architecture
- glTF/FBX model import
- ImGui editor (hierarchy, inspector, viewport, effects panels)
- Transform gizmos (ImGuizmo)
- Entity selection via ray casting
- PBR material system
- Fly camera controller
- Scene serialization (save/load JSON)
- Post-processing effects (bloom, vignette, color grading, FXAA, film grain)
- Weather effects (rain, snow, fog, storm with toggleable lightning)
- Water effects (3D water plane with Gerstner waves)
- Camera component for in-game cameras
- Camera frustum visualization in editor
- Play mode (play/pause/stop)
- Native file dialogs (Windows, macOS, Linux)
- Multiple light sources support (directional, point, spot)
- Shadow mapping with PCF filtering
- Render-to-texture for Game View (offscreen rendering)
- Retro rendering effects (per-material flat shading, affine texturing, vertex snapping, stipple transparency)
- Retro post-processing (dithering, color quantization, resolution downscaling, CRT scanlines)
- Vertex colors for baked lighting/shadows
- Normal mapping (tangent-space, per-material)
- Parallax occlusion mapping (height map ray marching)
- Physics collision detection (sphere-sphere, AABB-AABB, sphere-AABB)
- Render graph with topological sorting
- Deferred rendering framework
- GPU-driven frustum culling
- Material system with file watching and hot-reload
- Cross-platform file dialogs (Win32, macOS osascript, Linux zenity/kdialog)
- Render scripting system (command-based DSL)
- GLSL runtime shader compilation
- Wireframe rendering (fillModeNonSolid + wideLines support)
- Physics-integrated ground detection (raycast-based with Y=0 fallback)
- Auto-generated box colliders on model import (AABB from mesh vertices)
- Ground plane entity creation (Entity > Ground Plane menu)
- Editor input locking during play mode (NoInputs on panels, shortcut suppression)
- Skeletal animation (glTF skin/joint/animation import, GPU skinning via bone SSBO, auto-play)
- Wind system with instanced grass and vegetation sway
- Character controllers (Platformer 2D, Top-Down 2D/3D, Third Person, First Person)
- Camera presets auto-configured per controller type
- Gravity zones (per-entity gravity override with directional/point/zero-G modes)
- Temperature zones (heat/cold environmental effects)
- Camera trigger zones (camera override volumes)
- In-game text rendering (TextComponent with stb_truetype rasterization to texture)
- Entity clipboard (Cut/Copy/Paste via JSON serialization)
- Startup template selector (Blank, 2D Platformer, 2D Top-Down, 3D Isometric, 3D Third Person, 3D First Person)
- Custom template save/load from templates/ directory
- Scene management system (SceneManager with project manifests, scene lists, build indices)
- Scene transitions (Instant, Fade Black, Fade White, Cross Fade with configurable duration)
- Project file format (.enjinproject JSON manifest)
- Full inspector UI for all gameplay components (40+ component types)
- Procedural level generation with room prefab system (JSON load/save, weighted selection)
- Accessibility: persistent editor settings (EditorSettings save/load to %APPDATA%/enjin/)
- Accessibility: 4 editor themes (Dark, Light, High Contrast Dark, High Contrast Light)
- Accessibility: GPU colorblind correction (8 modes: protanopia, deuteranopia, tritanopia, anomalous variants, achromatopsia) via Daltonization in postprocess.frag
- Accessibility: remappable input via InputActionMap (semantic GameActions, hold/toggle modes, one-handed presets, JSON persistence)
- Accessibility: reduced motion support (weather particle reduction, head-bob disable)
- Accessibility: subtitle/caption overlay system (SubtitleSystem with configurable font size, background, speaker names, direction indicators)
- Accessibility: content warning system (per-scene warning flags with dismissable overlay)
- Accessibility: quick presets (Low Vision, Motor Impaired, Photosensitive, Reset All)
- Scene serialization of content warning flags
- 15 startup templates: Blank, 2D Platformer, 2D Top-Down, 3D Isometric, 3D Third Person, 3D First Person, Visual Novel, RPG Village, Survival, Game Manager, 3D Narrative, 4P Racing, Arena Fighter, PS1 RPG, City Builder
- World time and seasonal weather systems
- Terrain editing with brush tools (viewport sculpting: raise, lower, flatten, smooth, paint with ray-heightmap intersection)
- 2D terrain control point drag-to-edit in viewport
- Shrub/Tree vegetation rendering
- Audio system (miniaudio cross-platform backend, 3D spatialization, multi-channel mixing)
- Audio scene serialization (AudioSourceComponent, AudioListenerComponent)
- Game camera offscreen rendering with separate uniform buffers (fixes editor/game camera conflict)
- Standalone game player (Player/ app, loads .enjpak asset packs)
- Asset pack build pipeline (.enjpak packaging)
- Splitscreen rendering (2P/4P viewport subdivision, per-viewport uniform buffers, 4P Racing template)
- Multi-select system (Ctrl+click toggle, Shift+click range, viewport marquee/rubber-band selection)
- Multi-entity gizmo (centroid-based transform, delta applied to all selected)
- Batch transform inspector (position offset, rotation offset, scale multiplier for multiple entities)
- Raw mouse input (GLFW_RAW_MOUSE_MOTION, bypasses OS acceleration) + temporal smoothing
- C++ Entity Event Bus (decoupled entity communication with deferred dispatch)
- Damage resistance/weakness system (per-type multipliers: physical, fire, ice, electric, poison, magic)
- Stamina/Resource system (generic ResourceComponent with regen, depletion, action costs integrated into controllers)
- Footstep system (surface-based audio with walk/run intervals and pitch variance)
- Object pooling (entity recycling with lifetime-based auto-release)
- Quest/Objective system (QuestStateComponent with objective flags, QuestSystem with start/complete/fail)
- HUD overlay system (health bars, resource bars, labels, crosshair during play mode)
- Game state save/load system (SaveSystem with 10 slots, quick save/load, disk persistence)
- Cinematic camera system (spline-based waypoint sequences with easing, hold times, loop)
- Window icon support (PNG via stb_image, glfwSetWindowIcon)
- AngelScript scripting system (ScriptEngine, TegeBehavior base class, hot-reload)
- Script bindings: entity transform access (Get/Set Position/Rotation/Scale/Name)
- Script bindings: physics (Raycast, CheckSphere, CheckBox, AddForce, AddImpulse, SetVelocity, SetGravityScale)
- Script bindings: audio (Play, Stop, SetVolume, SetPitch, PlayAtPosition, MasterVolume)
- Script bindings: component access (Health, Material, Light, Camera, AudioSource, Animator, Controller — 40+ functions)
- Script coroutines (StartCoroutine, YieldSeconds, YieldFrames, YieldEndOfFrame)
- Script event system (Events_Listen, Events_Send, Events_Broadcast with EventData)
- Scene management from scripts (Scene_LoadScene, Scene_GetCurrentScene)
- Physics constraint solver (sequential impulse, 8 iterations, warm starting, Baumgarte stabilization)
- 6 physics joint types (Distance, Hinge, BallSocket, Spring, Fixed, Slider) with breakable mode
- Ragdoll component (bone-to-joint mapping, animation-to-ragdoll blend, auto-settle)
- PhysicsWorld-ECS integration bridge (automatic sync between RigidBody objects and ECS components)
- Joint and ragdoll serialization + inspector UI
- Cubemap skybox loading (stb_image 6-face loading with fallback)
- Quest log overlay rendering (ImGui overlay with objective checkmarks)
- LOD system with distance-based mesh swapping
- RenderSystem hot path optimizations (single-pass rendering, cached player entity, iterator reuse)
- Profiler system (ENJIN_PROFILE_SCOPE macro, per-frame breakdown, FPS graph, ImGui panel)
- Plugin/extension system (IPlugin interface, DLL/SO loading, manifest JSON, editor panel)
- Animation timeline/sequencer (property/event/animation tracks, easing, loop/ping-pong)
- C++ gameplay hot-reload (file watching, DLL reload, state save/restore)
- Mobile/console export foundations (IRenderBackend interface, PlatformInput, BuildTarget enum)
- Level streaming (chunk-based, distance-based loading, priority queue, async, StreamingVolume/Portal components)
- AI/Navmesh A* pathfinding with debug visualization

## Common Tasks

### Adding a new ECS Component

1. Create header in `Engine/include/Enjin/ECS/Components/`
2. Include in relevant systems
3. Optionally add inspector UI in `EditorLayer::DrawInspectorPanel()`

### Adding a new shader uniform

1. Update the UBO struct in `VulkanPipeline.h`
2. Update the GLSL shader
3. Recompile shader to SPIR-V
4. Update `ShaderData.h` with new bytecode
5. Update the system that uploads the uniform (e.g., `RenderSystem::UpdateUniformBuffer`)

### Importing a 3D model at runtime

```cpp
// In EditorLayer or game code:
SceneImporter::ImportOptions options;
options.scale = 1.0f;
auto result = SceneImporter::ImportGLTF("path/to/model.gltf", m_World, options);
if (result.success) {
    // result.entities contains all created entities
    // result.rootEntity is the root of the hierarchy
}
```

### Building a game for export

1. Configure `BuildConfig` with project path, output directory, and window settings
2. Run `BuildPipeline::Execute(config)` — scans scenes, validates assets, packs into `.enjpak`
3. The pipeline copies `EnjinPlayer` executable alongside the pack
4. Player loads `game.enjpak` from its own directory at startup

### Setting the window icon

Place an `icon.png` file (32x32 or 64x64 PNG recommended) next to the executable. The engine loads it via stb_image and calls `glfwSetWindowIcon()` at startup. If the file is missing, the OS default icon is used.
