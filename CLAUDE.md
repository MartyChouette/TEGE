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
│   │   ├── GUI/            # ImGui integration, UICanvas, UISystem, GameMenus, DialogueTree
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

### UI System (Runtime + Editor)

- **`UICanvasComponent`** (`Engine/include/Enjin/GUI/UICanvas.h`) — ECS component (namespace `Enjin::GUI`). Holds element tree, design resolution, scale mode, theme
- **`UIElement`** (`Engine/include/Enjin/GUI/UIElement.h`) — Single UI element with `UIAnchor` layout, `UIStyleOverride`, `UIWidgetData`, `computedRect`
- **`UIWidgetType`** enum: Panel, Button, Label, Image, ProgressBar, Slider, Checkbox, Toggle (+ Phase 2 placeholders)
- **`UIAnchor`** — Unity RectTransform-style: anchorMin/Max (0-1), pivot, offsetLeft/Right/Top/Bottom (pixels)
- **`UISystem`** (`Engine/include/Enjin/GUI/UISystem.h`) — Layout + render + input processing
  - `Update(world, vpW, vpH, deltaTime)` — processes all canvases in the world
  - `ComputeLayoutForCanvas(canvas, vpW, vpH)` — editor API: compute layout for a single canvas
  - `RenderCanvasPreview(canvas)` — editor API: render a single canvas via ImGui foreground draw list
- **`UIEventBus`** (`Engine/include/Enjin/GUI/UIEvents.h`) — String-named callbacks, `Listen(name, callback)`, `Dispatch(eventData)`
- **`UITheme`** (`Engine/include/Enjin/GUI/UITheme.h`) — Presets: Dark, Light, RetroGreen, Fantasy. Per-element style overrides
- **`UITemplates`** (`Engine/include/Enjin/GUI/UITemplates.h`) — Factory functions: `CreateMainMenu`, `CreatePauseMenu`, `CreateOptionsMenu`
- **UI Editor** — Viewport WYSIWYG editing mode in EditorLayer (`m_UIEditMode`):
  - Click to select elements, drag to move, 8 resize handles (corners + edges)
  - Right-click context menu to add new elements at click position
  - Inspector tree synced via `m_UIEditSelectedElementId`
  - Mutual exclusion with terrain/tilemap edit modes

### Effects Systems

- **`WeatherSystem`** - Rain, snow, fog, storm with lightning (global scene effect)
- **`Water3D`** - 3D water plane with waves (global scene effect)
- **`RetroEffects`** - CRT, pixelation, dithering post-processing
- **`WorldTimeSystem`** - Day/night cycle with configurable speed
- **`SeasonalWeatherSystem`** - Season-based weather transitions
- **`ParticleSystem`** (`Engine/include/Enjin/Effects/ParticleSystem.h`) - CPU particle simulation for `ParticleEmitterComponent` entities
  - Spawns from 5 shapes: Point, Sphere, Hemisphere, Cone, Box
  - Piecewise-linear size and speed curves over lifetime, color/alpha interpolation
  - Gravity, drag, rotation, burst spawning, accumulator-based continuous emission
- **`ParticleRenderer`** (`Engine/include/Enjin/Effects/ParticleRenderer.h`) - GPU instanced billboard renderer (same pipeline as WeatherRenderer)
  - Gathers all emitter pools into single instanced draw call (up to 16384 particles)
- **Particle Editor** (`View > Particle Editor`, `EditorPanel::ParticleEditor = 1 << 13`)
  - Operates on selected entity's `ParticleEmitterComponent`
  - 7 presets: Fire, Smoke, Sparks, Snow, Rain, Magic, Explosion
  - Color gradient bar with alpha, piecewise-linear size/speed curve visualizations
  - 2D wireframe shape preview, emission/rotation/forces editors
  - Play/Pause/Restart playback controls, active particle stats
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
- **`AssimpLoader`** - Loads FBX, OBJ, DAE, 3DS files via Assimp v5.4.3 into AssimpScene
- **`SceneImporter`** - Converts GLTFScene or AssimpScene to ECS entities, auto-generates BoxColliders, sets up skeleton/animation for skinned meshes. `Import()` auto-detects format from extension.
- **`PrefabManager`** (singleton) - Create prefabs from entities (`CreateFromEntity`), instantiate with overrides, save/load `.enjprefab` files, apply changes to all instances, unpack instances
- **`PrefabInstanceComponent`** - Marks an entity as a prefab instance with `prefabId`, `prefabPath`, and per-instance property overrides

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
- glTF/FBX/OBJ/DAE model import (GLTFLoader native + Assimp v5.4.3 for FBX/OBJ/DAE/3DS)
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
- AI ping-pong patrol mode (reversing direction at patrol endpoints)
- 2D sprite rendering (dirty-flag mesh generation, sorted sprite pass, UV normalization)
- 2D tilemap rendering (tile mesh generation, tileset texture binding)
- 2D sprite animation advancement (frame timer, looping, playback speed)
- 2D camera follow and bounds clamping in ControllerSystem
- Sprite texture preview in inspector (thumbnail with source rect overlay)
- Sprite sheet frame picker (clickable grid, auto-sets source rectangle)
- Animation preview widget (live current-frame display with progress bar)
- Sprite atlas auto-slicer (grid-based frame generation from sprite sheets)
- Tilemap visual grid editor in inspector (clickable tile grid with palette)
- Tilemap viewport brush tool (ray-plane intersection painting/erasing)
- Runtime UI system (UICanvasComponent, UIElement hierarchy, anchor-based layout, 8 widget renderers)
- UI event bus (string-named callbacks, C++ std::function listeners)
- UI theme system (4 presets: Dark, Light, RetroGreen, Fantasy, per-element style overrides)
- UI templates (CreateMainMenu, CreatePauseMenu, CreateOptionsMenu factory functions)
- UI canvas inspector (element tree, theme editor, widget-specific fields, template insertion)
- UI Editor (viewport WYSIWYG: click-select, drag-move, resize handles, right-click add, inspector sync)
- Script component workflow (class name prompt, TegeBehavior boilerplate generation, auto-fill attachment, open in IDE)
- External IDE configuration (VS Code, Visual Studio, Rider, Custom with persistent settings)
- Editor Settings vs Project Settings separation (dedicated Project Settings panel for rendering/physics)
- Prefab system (save/load .enjprefab, instantiate, unpack, per-instance property overrides, inspector badge)
- Security hardening: vector deserialization bounds checks, AssetReader size caps and I/O validation, GLTFLoader attribute count clamping, animation keyframe bounds validation, script execution timeout (1M instruction limit)
- Particle system runtime (CPU simulation: 5 emitter shapes, size/speed curves, gravity/drag/rotation, burst spawning)
- GPU instanced particle renderer (billboard quads, alpha-blended, depth-tested, up to 16384 particles)
- Particle Editor panel (7 presets, color gradient bar, size/speed curve visualization, shape preview, playback controls)
- Per-scene rendering settings (SceneRenderSettings config struct, project-level defaults in .enjinproject, per-scene overrides in .enjin, play mode save/restore, Project Settings UI)

## AngelScript API Reference

All functions below are callable from AngelScript via `TegeBehavior` scripts. ~150 functions across all categories.

### Math Types

- **Vector2**: `x`, `y`, `Length()`, `Normalized()`, `Dot()`, operators `+`, `-`, `*`, `/`, unary `-`
- **Vector3**: `x`, `y`, `z`, `Length()`, `Normalized()`, `Dot()`, `Cross()`, operators
- **Vector4**: `x`, `y`, `z`, `w`
- **Quaternion**: `x`, `y`, `z`, `w`, `Rotate(Vector3)`, `Normalized()`, `Inverse()`, `ToEuler()`, operators. Statics: `Quaternion_Identity()`, `Quaternion_FromEuler(Vector3)`, `Quaternion_Slerp(q1, q2, t)`

### Global Math Functions

`Abs`, `Sin`, `Cos`, `Tan`, `Asin`, `Acos`, `Atan2`, `Sqrt`, `Pow`, `Floor`, `Ceil`, `Round`, `Min`, `Max`, `Clamp`, `Lerp`, `MoveTowards`, `Sign`, `Random()`, `RandomRange(min, max)`, `RandomInt(min, max)`, `Radians`, `Degrees`, `PI()`

### Entity & Transform

- `Entity_GetPosition/SetPosition(uint64, Vector3)`
- `Entity_GetRotation/SetRotation(uint64, Vector3)` — degrees
- `Entity_GetScale/SetScale(uint64, Vector3)`
- `Entity_GetName(uint64)`
- **EntityHandle** class: `IsValid()`, `GetID()`, `GetPosition/SetPosition()`, `GetRotation/SetRotation()`, `GetScale/SetScale()`, `GetName()`, `HasTag(string)`
- **TransformProxy**: `position`, `rotation`, `scale`, `forward`, `right`, `up` (read-only)

### Scene Management

- `Scene_FindEntity(string name)`, `Scene_FindEntityByTag(string tag)`
- `Scene_DestroyEntity(uint64)`, `Scene_Instantiate()`, `Scene_InstantiateNamed(string)`, `Scene_InstantiateAt(Vector3)`
- `Scene_IsValid(uint64)`, `Scene_GetEntityCount()`
- `Scene_GetEntityName/SetEntityName(uint64, string)`
- `Scene_AddTag/RemoveTag/HasTag(uint64, string)`
- `Scene_LoadScene(string)`, `Scene_GetCurrentScene()`

### Time

`Time_GetDeltaTime()`, `Time_GetFixedDeltaTime()`, `Time_GetTime()`, `Time_GetTimeScale()`, `Time_SetTimeScale(float)`, `Time_GetFrameCount()`

### Debug

`Debug_Log(string)`, `Debug_LogWarning(string)`, `Debug_LogError(string)`

### Input — Keyboard

`Input_GetKey(int)`, `Input_GetKeyDown(int)`, `Input_GetKeyUp(int)` — Key enum: A-Z, Num0-9, F1-F12, Space, Escape, Enter, Tab, Backspace, arrows, Shift, Control, Alt, etc.

### Input — Mouse

`Input_GetMouseButton/Down/Up(int)` — MouseBtn: Left, Right, Middle
`Input_GetMousePosition()`, `Input_GetMouseDelta()`, `Input_GetScrollDelta()`
`Input_IsMouseCaptured()`, `Input_SetMouseCaptured(bool)`

### Input — Gamepad

`Input_IsGamepadConnected(int)`, `Input_GetGamepadButton/ButtonDown(int, int)` — GamepadBtn: A, B, X, Y, bumpers, back, start, D-pad
`Input_GetGamepadAxis(int, int)` — GamepadAx: LeftX/Y, RightX/Y, triggers
`Input_GetGamepadLeftStick/RightStick(int)`, `Input_GetGamepadLeftTrigger/RightTrigger(int)`

### Physics

- `Physics_Raycast(origin, dir, maxDist)`, `Physics_RaycastHit(origin, dir, maxDist, &hit)` — RaycastHit: `point`, `normal`, `distance`, `entity`
- `Physics_CheckSphere(center, radius)`, `Physics_CheckBox(center, halfExtents)`
- `Physics_AddForce/AddImpulse(uint64, Vector3)`, `Physics_SetVelocity/GetVelocity(uint64)`, `Physics_SetGravityScale(uint64, float)`

### Audio

`Audio_Play(uint64)`, `Audio_PlayAtPosition(string, Vector3)`, `Audio_Stop(uint64)`, `Audio_StopAll()`
`Audio_SetVolume/SetPitch(uint64, float)`, `Audio_IsPlaying(uint64)`
`Audio_SetMasterVolume/GetMasterVolume(float)`

### Component Access

- **Health**: `Health_Get/GetMax/SetCurrent(uint64)`, `Health_Damage(uint64, float)`
- **Material**: `Material_SetBaseColor/GetBaseColor(uint64, Vector3)`, `Material_SetMetallic/SetRoughness(uint64, float)`
- **Light**: `Light_SetColor/SetIntensity(uint64, ...)`
- **Camera**: `Camera_SetFOV/GetFOV(uint64, float)`
- **AudioSource**: `AudioSource_Play/Stop/SetClip/SetVolume(uint64, ...)`
- **Animator**: `Animator_Play(uint64, string)`, `Animator_SetSpeed(uint64, float)`
- **Controller**: `Controller_SetMoveSpeed/GetVelocity(uint64, ...)` — works with all 5 controller types
- **Existence checks**: `HasComponent_Health/Light/Camera/Material/AudioSource/Rigidbody/BoxCollider/Animator(uint64)`

### Coroutines

`StartCoroutine(string)`, `YieldSeconds(float)`, `YieldFrames(uint)`, `YieldEndOfFrame()`

### Event System

- **EventData** class: `SetFloat/GetFloat`, `SetInt/GetInt`, `SetString/GetString`, `SetEntity/GetEntity`
- `Events_Listen(string, EventCallback@)` — returns listener ID
- `Events_Send(string, EventData@)`, `Events_Broadcast(EventData@)`

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

## Security Considerations

When modifying or extending the engine, keep these security notes in mind:

### Input Validation
- **Scene files (JSON):** All deserialization functions must validate array sizes and check field existence with `.contains()` before accessing JSON keys. Vector deserializers now return safe defaults for malformed arrays.
- **glTF/GLB import:** Attribute counts may differ across vertex attributes. Always clamp loop bounds to the allocated buffer size. Validate that animation keyframe value arrays have enough elements for the declared keyframe count.
- **Asset pack (.enjpak):** All sizes and offsets read from pack files must be bounds-checked against file size and capped to reasonable maximums. Check `file.good()` after every seek/read.

### Script Execution
- AngelScript scripts are sandboxed from filesystem and network access (no bindings exposed).
- A per-context instruction limit (1M instructions) is enforced via `SetLineCallback()` to prevent infinite loop DoS. If a script exceeds this limit, execution is aborted.
- Script `#include` paths are resolved via `lexically_normal()` but are not yet restricted to the script directory — avoid exposing script compilation to untrusted input.

### Asset Pack Obfuscation
- The `.enjpak` XOR obfuscation uses a repeating key and is **not cryptographically secure**. It deters casual inspection but is trivially broken via known-plaintext attack (e.g., JSON files start with `{`).
- CRC32 is used for **integrity detection**, not authentication. It catches accidental corruption but not intentional tampering.
- For commercial releases requiring real asset protection, replace XOR with authenticated encryption (e.g., AES-GCM).

### General Guidelines
- Always validate enum casts from deserialized integers against valid ranges.
- Sanitize file paths loaded from scene/project files — prevent `..` traversal.
- Cap allocation sizes from untrusted input (pack files, scene files, model files).

## Roadmap (Planned Features)

This is the long-term feature roadmap for Enjin to compete with Unity/Unreal. Items are grouped by category. Each session should pick one item to plan and implement — do NOT try to plan the whole list at once.

### Editor Tools & UX

- **UI Editor** — ~~DONE (Phase 1)~~ Runtime UI system + viewport WYSIWYG editor implemented. Future work: snap-to-grid, alignment guides, undo/redo for UI edits, nested element drag reparenting.
- **Particle Editor** — ~~DONE (Phase 1)~~ CPU particle simulation + GPU instanced renderer + editor panel with 7 presets, color gradient, size/speed curves, shape preview, playback controls. Future work: sub-emitter support, curve key editors, texture atlas animation, GPU particle simulation.
- **Node/Graph Editor** — generic node graph framework powering three systems:
  - Visual scripting (blueprint-style alternative to AngelScript)
  - Shader graph (node-based shader authoring, generates GLSL/SPIR-V)
  - Animation state machine (visual state/transition editor replacing manual AnimatorComponent setup)
- **Script Component Workflow** — ~~DONE~~ Class name prompt, TegeBehavior boilerplate generation, auto-fill ScriptAttachment, open in configured IDE. Future work: open-file-at-line for script errors.
- **IDE Integration** — ~~DONE (Phase 1)~~ Editor settings for external IDE selection (Auto/VS Code, Visual Studio, Rider, Custom). Persistent settings, Browse for custom path, Test Open button. Future work: open-file-at-line support for script errors.
- **Undo/Redo Across All Operations** — extend existing UndoRedoManager to cover every inspector edit, hierarchy change, component add/remove, tilemap paint, terrain sculpt, etc.
- **Drag and Drop** — drag assets (textures, models, scenes, scripts) from asset browser into viewport/inspector fields. Drag entities in hierarchy for reparenting.
- **Hot-Swap Shaders** — edit shaders at runtime and see changes live without restarting. File watcher on .vert/.frag files, recompile GLSL to SPIR-V, recreate pipeline.
- **Improved Asset Import Pipeline** — on model/texture import, auto-process like Unity does: generate thumbnails, extract materials, set up serialization metadata, configure import settings (scale, axis, compression). Clean .enjinasset metadata files.
- **Extended Model Format Support** — add PLY (point cloud/mesh) and VOX (MagicaVoxel voxel) import via Assimp or custom loaders. PLY is useful for photogrammetry/scan data; VOX enables voxel art workflows. Also verify Assimp's existing FBX/OBJ/DAE import paths are robust (fix any crash-on-malformed-input issues).
- **Improved Icon/Window Inspector** — better entity icons in hierarchy, component icons in inspector, custom window icon picker in project settings.
- **Editor Settings vs Scene Settings** — ~~DONE~~ Separated into Settings (editor prefs) and Project Settings (rendering, physics) panels.
- **Template Rebuild & Demo Scenes** — Update and rebuild all 15 startup templates to use the latest engine features (per-scene render settings, particle system, UI system, etc.). For each template, create a small demo scene that showcases the template's intended gameplay. Add a "Demo" button on each template card in the selector that loads the demo scene instead of the blank template. Template card UI improvements: larger font for template titles, better visual hierarchy.
- **Editor Accent Color & Theming** — Replace the current blue accent color with TEGE brand color `#c7dac4` (soft sage green). Use this as the primary accent throughout the editor (selected items, active tabs, buttons, progress bars, focus indicators). Complement with the existing blue as a secondary accent for links/info. Make the overall editor more aesthetically pleasant, cute, and inviting while maintaining readability. Consider: rounded corners on panels, softer panel borders, warmer background tones, subtle hover animations. The goal is a distinct visual identity — not Unity grey, not Unreal dark, not generic dev-tool blue.

### Runtime Systems

- **UI Runtime** — ~~DONE (Phase 1)~~ Anchored layout, 8 widget types, event bus, theme system implemented. Future work: flex/grid layout, text input widget, scrollable panels, runtime texture loading for Image widgets.
- **9-Slice / Text Box System** — Scalable UI backgrounds from sprite sheets using 9-slice (9-patch) rendering. A `NineSliceConfig` struct (texture path + 4 border insets in pixels) defines how a sprite is split into 9 regions: corners stay fixed-size, edges stretch in one axis, center stretches in both. The renderer generates a 9-quad mesh per element. This replaces flat-color Panel backgrounds with customizable, artist-friendly frames (dialogue boxes, buttons, tooltips, health bars). Should be simple to configure: drop in a sprite, set 4 inset values, done. Per-theme 9-slice defaults so all Panels/Buttons in a theme share the same frame style. Alternative considered: SDF rounded rectangles (resolution-independent but less artist-customizable). 9-slice is the better fit for a game engine UI.
- **2D Camera System** — follow targets, camera bounds/clamping, smooth follow, look-ahead, screen shake, zoom, dead zones, multi-target framing.
- **Particle System Runtime** — ~~DONE (Phase 1)~~ CPU simulation with 5 emitter shapes, piecewise-linear size/speed curves, color/alpha interpolation, gravity, drag, rotation. GPU instanced billboard rendering. Future work: GPU compute particle simulation, sub-emitters, particle collision, attractors, force fields.
- **Improved Physics** — 2D physics (Box2D-style), 2D joints, continuous collision detection, more shape types, physics materials (friction, bounce), trigger callbacks from scripts.
- **Basic Networking** — client-server architecture, state synchronization, entity ownership, lobbies, RPCs, lag compensation. Start with LAN/direct connect, then relay servers later.
- **Destructible Environments** — extend DestructibleComponent to work as a prefab-level setting. When enabled on a prefab, all instances inherit destructibility. Add fracture/shatter visual effects (mesh splitting into fragments on destroy), debris physics, chain destruction propagation. Editor toggle: "Destructible" checkbox on prefab inspector.
- **Improved Shadow System** — cascaded shadow maps (CSM) for large outdoor scenes, shadow distance fade, per-light shadow quality settings, soft shadows with PCSS, transparent shadow receivers. Fix shadow acne edge cases and improve shadow bias auto-tuning.
- **Per-Scene Rendering Settings** — ~~DONE~~ `SceneRenderSettings` struct captures all RenderSystem + PostProcessSettings state (~60 fields). Serialized in scene files as `"renderSettings"` JSON section. Project-level defaults stored in `.enjinproject` manifest. Applied on scene load/new scene, saved/restored around play mode. Project Settings panel has "Use Project Defaults" checkbox + "Set Current as Project Default" / "Reset to Project Default" buttons. Old scenes without `renderSettings` gracefully default.

### Rendering Pipeline & Performance

- **3D/2D Pipeline Audit & Safety Rails** — Currently 2D sprites and 3D meshes share the same Vulkan pipeline. Sprites generate per-entity quads via MeshComponent and are sorted/rendered in a separate `RenderSprites()` pass. Audit needed:
  - **Costs of cross-use:** 2D sprites incur per-entity draw calls like 3D meshes (no batching). Shadow pass runs even in 2D-only scenes. Ortho/perspective projection mixing can produce unexpected depth results.
  - **Smart safety rails:** Auto-disable shadow pass when no 3D geometry exists. Warn users when mixing ortho and perspective cameras in the same scene. Auto-skip 3D lighting calculations for sprite-only entities (flat shading fast path). Detect and warn when sprite count exceeds batching threshold (100+ individual draw calls).
  - **Sprite batching:** Group sprites by texture atlas into single instanced draw calls (like particle renderer). This is the biggest 2D performance win — reduces 100 sprite draw calls to 1-5 batched calls.
  - **Frame slog scenarios to address:** Many unbatched sprites (100+), sprite meshes regenerating every frame without dirty flags (already mitigated), 3D shadow pass on 2D-only scenes, per-entity uniform buffer updates for static sprites.
  - **What breaks:** Mixing 2D sprites with 3D depth testing causes z-fighting. 2D entities with 3D physics colliders work but waste collision broadphase cycles. Sprite transparency sorting conflicts with 3D depth buffer.

- **Rendering Pipeline Investigation & Optimization** — Frame rate erratically drops (200fps dives). Investigate the rendering pipeline for bottlenecks. Potential optimizations:
  - Multi-threaded command buffer recording (record per-viewport or per-material-group in parallel)
  - GPU payload batching (sort by pipeline/material to minimize state changes)
  - Indirect rendering (VkCmdDrawIndexedIndirect for single draw call with GPU-side culling)
  - Asynchronous compute for culling, particle simulation, and post-processing passes
  - Frame graph resource scheduling (avoid unnecessary GPU barriers/transitions)
  - LOD selection on compute shader, occlusion queries, Hi-Z culling
  - Profile with GPU timestamps and CPU profiler to identify actual bottleneck (CPU-bound submission vs GPU-bound fragment)

### Procedural Generation

- **Noise Library** — Implement a variety of useful noise types for procgen and terrain:
  - Perlin noise (2D/3D)
  - Simplex noise (2D/3D)
  - Worley/cellular noise (F1, F2, F1-F2 variants)
  - Value noise
  - Fractal Brownian motion (fBm) — octaved layering of any base noise
  - Ridged multifractal noise
  - Domain warping (noise fed into noise coordinates)
  - Billow noise
  - All noise types should support configurable frequency, amplitude, octaves, lacunarity, persistence. Expose as both C++ API and AngelScript bindings.

- **Procedural Generation Algorithms** — Expand LevelGenerator with modular algorithm support. Each algorithm should work as a pluggable generator that produces 2D grid data or 3D room/corridor layouts. Include bag/piece-pull options (weighted randomized selection from pools) where applicable:
  - **Cellular Automata** — cave generation, organic shapes (configurable birth/death thresholds, iteration count, bag of initial fill patterns)
  - **Random Walkers** — dungeon carving with drunkard's walk, directional bias, tunnel width options (bag of walker behaviors: straight, wobbly, branching)
  - **Wave Function Collapse (WFC)** — tile-based generation from example patterns, adjacency constraints, backtracking solver (bag of tile sets, weight per tile)
  - **BSP (Binary Space Partitioning)** — room-corridor dungeons, min/max room sizes, corridor placement (bag of room templates, piece-pull for room shapes and decoration)
  - **L-Systems** — rule-based recursive generation for trees, plants, branching structures, river networks (bag of production rules, stochastic rule selection)
  - **Voronoi Diagrams** — region-based world generation, biome placement, city districts (bag of region types with weighted pull for biome assignment)
  - **Diamond-Square Algorithm** — heightmap terrain generation, midpoint displacement (configurable roughness, seed, corner initialization)
  - **Grammar-Based Generation** — shape grammars for building layouts, procedural architecture, interior room decoration (bag of grammar rules with weighted selection)
  - **Modular/Prefab Assembly** — snap-together room/corridor pieces from a prefab library with connection points (bag of pieces per socket type, weighted pull, uniqueness constraints)
  - Editor UI: Generator panel with algorithm selection, parameter sliders, live preview, seed control, "Generate" button. Results stored as TilemapComponent data or TerrainComponent heightmaps.

### Custom Flora Assets

- **Flora Asset Drop-In** — Allow users to drop in custom images or 3D models to replace stock flora in the vegetation/grass/shrub/tree systems:
  - For 2D flora (grass, shrubs): drop a sprite/image into the vegetation volume inspector to replace the stock billboard texture. The system textures the existing instanced quads with the custom image.
  - For 3D flora (trees): drop a `.gltf`/`.fbx`/`.obj` model file. The engine auto-creates a prefab if one doesn't exist, and the TreeRenderer uses the prefab mesh instead of the procedural tree. Wind sway, LOD, and seasonal color changes continue to work via vertex shader wind and material tinting.
  - Inspector UI: "Custom Asset" field on GrassVolumeComponent, ShrubVolumeComponent, TreeVolumeComponent. Browse button + drag-drop from asset browser. Clear button to revert to stock.

### Scripting & Extensibility

- **Script Rendering Bindings** — Expose RenderSystem and PostProcessSettings to AngelScript so scripts can automate camera movement and rendering changes at runtime. Bindings like `Render_SetFogDensity(float)`, `Render_SetFogColor(Vector3)`, `Render_SetAmbientIntensity(float)`, `Render_SetShadowsEnabled(bool)`, `PostProcess_SetExposure(float)`, `PostProcess_SetBloomEnabled(bool)`, `PostProcess_SetVignetteIntensity(float)`, `PostProcess_SetToneMapping(int)`, etc. Enables cinematic sequences that change lighting/fog/post-processing over time, day/night transitions driven by scripts, and gameplay-reactive visual effects. Combined with existing `CinematicCameraComponent` spline waypoints and coroutines (`YieldSeconds`), this gives full cutscene authoring capability from scripts.
- **Component/Plugin DLL Repositories** — load gameplay components from external DLLs/shared libraries. Package format for distributing reusable components. Local repository system (marketplace comes later).
- **Documentation Generator** — auto-generate docs from component definitions, script API, project structure. HTML or markdown output for game teams.
- **ScriptableObject / DataAsset System** — Unity-like reusable data containers that are NOT entities. Serialized JSON assets for game configuration: weapon stats, enemy tables, dialogue databases, item definitions, skill trees. Create/edit in inspector, reference from components. Extends the current component-only model with standalone data assets.

### Platform & Export

- **Mobile Support** — touch input, gyroscope, screen density handling, mobile-optimized render paths. Android (Vulkan) and iOS (MoltenVK) targets.
- **Console Support** — platform abstraction for console input, certification requirements, console-specific render backends.
- **VR/XR Support** — OpenXR integration, stereo rendering, hand tracking, spatial input, roomscale. Head-mounted display render paths.
- **WebAssembly Export** — target WebGPU (NOT WebGL). Vulkan-aligned API, better future-proofing. Compile to WASM with Emscripten or wasm-bindgen. Accept that browser support is limited now but growing.

### Accessibility (Engine-Level)

Accessibility is not just for games made with Enjin — the editor itself must be fully accessible.

- **Screen Reader Support** — expose editor UI hierarchy to OS accessibility APIs (UI Automation on Windows, AT-SPI on Linux, NSAccessibility on macOS). All panels, buttons, fields must have accessible names/roles.
- **Keyboard-Only Navigation** — full editor operation without a mouse. Tab-order through panels, keyboard shortcuts for every action, focus indicators on all interactive elements.
- **Alternative Input Devices** — support for switch access, eye tracking, sip-and-puff. Configurable input mapping at the editor level (not just in-game InputActionMap).
- **Motor Accessibility** — adjustable click/drag thresholds, sticky keys, dwell-click support, one-handed editor presets.
- **Visual Accessibility** — already have colorblind modes and high-contrast themes. Add: configurable font sizes across all editor panels, icon scaling, custom accent colors, reduced transparency option.
- **Audio Accessibility** — visual indicators for all audio feedback in the editor (build complete, error notifications, etc.). Captions for any tutorial/onboarding audio.
- **Blind-Accessible Workflow** — investigate whether a blind developer can create a game using only screen reader + keyboard. Identify and fix gaps. Consider an alternative text-based/CLI interface for core operations (create entity, add component, set properties, build).

### UI/UX Design Philosophy

Enjin's editor design should be:
- **Aesthetically accessible** — clean, forward-thinking, timeless design
- **Not a clone** — not Apple-style, not Unity grey, not Unreal dark. Our own identity.
- **Information-dense but not cluttered** — show what matters, collapse what doesn't
- **Consistent** — same patterns everywhere (context menus, drag behavior, property editing)

### Version Control & Collaboration

- **Git Integration** — built-in git panel in editor. Stage, commit, push, pull, branch, merge. Visual diff for scenes (structured JSON diff, not raw text).
- **Session Sharing / Collaborative Editing** — real-time or turn-based collaboration with clear session locks showing who is editing which entity/scene. Prevents merge conflicts at the editor level.
- **Clean Git Serialization** — scene files must serialize deterministically (sorted keys, stable ordering, no floating-point drift noise). Unity's random YAML reordering and index GUIDs are the anti-pattern. Our JSON scenes should diff cleanly and merge predictably.

### NOT Planned Yet (Future)

- Asset marketplace / template exchange (needs servers, not now)
- Cloud build pipelines
- Analytics dashboard
