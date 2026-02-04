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
  - `BoxColliderComponent` / `SphereColliderComponent` / `CapsuleColliderComponent` - Colliders with `categoryBits` (u32 bitmask, default `1` = "Default" group) and `collisionMask` (u32, default `0xFFFFFFFF` = collide with all). Old `layer` field was renamed to `categoryBits`.

### Collision Filtering

- **Bitmask system:** Each collider has `categoryBits` (which groups it belongs to) and `collisionMask` (which groups it collides with). Up to 32 groups, one per bit.
- **Bilateral rule:** `(A.categoryBits & B.collisionMask) && (B.categoryBits & A.collisionMask)` — both entities must agree to collide
- **Defaults:** `categoryBits = 1` (bit 0, "Default"), `collisionMask = 0xFFFFFFFF` (all). New entities collide with everything.
- **Named groups:** Stored in `SceneManager::m_CollisionGroupNames` (32-entry vector, index 0 = "Default"). Serialized in `.enjinproject` as `"collisionGroups"` array.
- **PhysicsWorld integration:** `RigidBody` carries `categoryBits`/`collisionMask`, copied from ECS colliders in `SyncFromECS()`. `DetectCollisions()` applies bilateral filter before shape tests.
- **SimplePhysics integration:** `Raycast`, `RaycastAll`, `CheckGround`, `MoveAndSlide`, `GetCollidersInRadius` accept optional `u32 layerMask` param (default `0xFFFFFFFF`). Ground check loop uses bilateral filter.
- **Inspector UI:** `DrawCollisionFilteringUI(categoryBits, collisionMask)` shows named checkbox lists for "Category (belongs to)" and "Collides with", plus hex display. Called from all 3 collider inspectors.
- **Project Settings:** "Collision Groups" section with editable group name text fields (group 0 "Default" is read-only).
- **Serialization:** Saved as `"categoryBits"` in scene JSON. Deserializer migrates old `"layer"` field: `layer 0 → categoryBits 1`, `layer N → categoryBits (1 << N)`.
- **Script bindings:** Masked overloads `Physics_Raycast(..., uint)`, `Physics_RaycastHit(..., uint, ...)`, `Physics_CheckSphere(..., uint)`, `Physics_CheckBox(..., uint)`. AngelScript resolves by param count.

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
Binding 4: Shadow map array sampler - 2D array for CSM cascades (fragment shader)
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

### Flower System

- **`FlowerSystem`** (`Engine/include/Enjin/ECS/Systems/FlowerSystem.h`) — Play-mode system for interactive flower plucking gameplay
  - Manages click-drag petal/leaf plucking via `GrabbableComponent` (ray-sphere picking in game view)
  - `TetherComponent` defines connection graph: `connectedEntity` (physics joint target) + `stemEntity` (for scoring). Petals connect to crown, crown and leaves connect to stem
  - Physics joints created at play-mode start by `SetupJointsIfNeeded()`: adds RigidbodyComponents (Kinematic for stem, Dynamic for parts) and SpringJointComponents with per-type tuning (Petal: k=120/break=25, Crown: k=200/break=60, Leaf: k=100/break=20)
  - Connected plucking: pulling a petal tugs the crown via spring force transfer, pulling the crown tugs the stem. Organic tug chain
  - Break detection via `UpdateJointTracking()`: monitors SpringJointComponent existence — when physics solver destroys a joint (stress exceeds breakForce), spawns particles at cached `junctionWorldPos`
  - `ProcessGrabForces()` applies cursor pull force and wind sway to rigidbody velocity
  - `JellyMeshComponent` per-vertex spring deformation for organic feel. Mesh data cleared on break to prevent GPU buffer churn
  - `FlowerStemComponent` tracks score (partsRemoved, healthyRemoved, witheredRemoved), `liquidIntensity` (0=off, 1=normal, 2=extra gush)
  - `FlowerParticle` lightweight internal particles (no ECS entities — avoids Vulkan buffer race conditions). Rendered as projected ImGui shapes in game view overlay
  - Liquid particles: green sap streaks (`isLiquid=true`) rendered as thick lines + head blobs. Squirt direction follows pull vector
  - Break flow: entity hidden offscreen (scale=0, y=-100) on mouse release — NOT destroyed (keeps GPU buffers valid)
  - Evaluate: computes final score from stem counters, locks display via `stem->evaluated` flag
  - Inspector: `DrawFlowerStemComponent` with Healthy Bonus, Withered Penalty, Liquid Intensity slider + Off button. TetherComponent shows Connected entity, spring params on SpringJointComponent
  - Template: Flower Garden (stem + crown + 10 petals + 5 leaves + game camera + score display + sun light). Crown created before petals so petals can reference it as connectedEntity. Collision groups: "Petals" (bit 1) and "Leaves" (bit 2) prevent same-type collisions

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
  - Supports two render modes: Billboard (default) and VelocityStretch (elongates particles along velocity)
- **Particle Editor** (`View > Particle Editor`, `EditorPanel::ParticleEditor = 1 << 13`)
  - Operates on selected entity's `ParticleEmitterComponent`
  - 12 presets: Fire, Smoke, Sparks, Snow, Rain, Magic, Explosion + liquid presets (Water Splash, Blood/Sap, Lava, Fountain, Drip)
  - Color gradient bar with alpha, piecewise-linear size/speed curve visualizations
  - 2D wireframe shape preview, emission/rotation/forces editors
  - Rendering section: render mode combo (Billboard/Velocity Stretch), stretch scale slider
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
- Cascaded shadow maps (4-cascade CSM with PCF filtering, texel stabilization, distance fade, per-cascade bias)
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
- Script bindings: rendering (Render_Set/Get for shadows, ambient, fog, snow, rain, curvature, wireframe — 28 functions)
- Script bindings: post-processing (PostProcess_Set/Get for tone mapping, exposure, bloom, vignette, chromatic aberration, color grading, film grain, FXAA — 36 functions)
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
- Particle Editor panel (12 presets incl. 5 liquid presets, velocity stretch render mode, color gradient bar, size/speed curve visualization, shape preview, playback controls)
- Per-scene rendering settings (SceneRenderSettings config struct, project-level defaults in .enjinproject, per-scene overrides in .enjin, play mode save/restore, Project Settings UI)
- Shadow quality settings (resolution 512-4096, shadow distance 10-500, shadow strength 0-1, serialized per-scene)
- Flower system (FlowerSystem: SpringJointComponent-based connected plucking — petals connect to crown, crown/leaves connect to stem via physics joints. Click-drag plucking, jelly mesh deformation, joint break detection via physics solver stress, green sap liquid particles with streak rendering, configurable liquidIntensity, evaluate scoring, ImGui game view particle projection and button overlay)
- Flower Garden startup template (stem + crown + 10 petals + 5 leaves + game camera + score display)
- Particle velocity stretch render mode (VelocityStretch elongates particles along velocity vector, configurable scale, serialized per-emitter)
- Particle liquid presets (Water Splash, Blood/Sap, Lava, Fountain, Drip with tuned velocity stretch, gravity, drag)
- Drag-and-drop file import (GLFW drop callback, imports FBX/OBJ/glTF/GLB/DAE/3DS models and opens .enjin scene files)
- Per-entity collision filtering (categoryBits/collisionMask bitmask system, bilateral filter rule, 32 named groups in project manifest, inspector checkbox UI, Project Settings group editor, backward-compat migration from old layer field, SimplePhysics + PhysicsWorld filtering, masked script bindings)
- Entity visibility toggle (TransformComponent::visible bool, RenderSystem skip in all passes incl. shadows/sprites/tilemaps/grass/shrubs/trees/particles, inspector checkbox, hierarchy eye icon, scene serialization, script bindings Entity_SetVisible/Entity_IsVisible)
- Node graph editor framework (generic NodeGraphEditor widget with typed pins, Bezier links, drag-to-connect, pan/zoom, minimap, keyboard nav, box select, context menus, theme-aware colors, JSON serialization)
- Animation graph panel (visual state machine editor for StateMachineComponent, Entry pseudo-node, state/transition inspector, parameter editor, play mode highlighting, auto layout, editorPosition serialization)

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
- Masked overloads (filter by collision group): `Physics_Raycast(origin, dir, maxDist, layerMask)`, `Physics_RaycastHit(origin, dir, maxDist, layerMask, &hit)`, `Physics_CheckSphere(center, radius, layerMask)`, `Physics_CheckBox(center, halfExtents, layerMask)`
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

### Rendering

- **Shadows**: `Render_SetShadowsEnabled(bool)` / `Render_IsShadowsEnabled()`, `Render_SetShadowDistance/GetShadowDistance(float)`, `Render_SetShadowStrength/GetShadowStrength(float)`
- **Ambient**: `Render_SetAmbientIntensity/GetAmbientIntensity(float)`, `Render_SetAmbientColor/GetAmbientColor(Vector3)`
- **Fog**: `Render_SetFogDensity/GetFogDensity(float)`, `Render_SetFogColor/GetFogColor(Vector3)`, `Render_SetFogStart/GetFogStart(float)`, `Render_SetFogEnd/GetFogEnd(float)`, `Render_SetFogHeightFalloff/GetFogHeightFalloff(float)`
- **Weather**: `Render_SetSnowIntensity/GetSnowIntensity(float)`, `Render_SetRainActive/IsRainActive(bool)`
- **Effects**: `Render_SetWorldCurvature/GetWorldCurvature(float)`, `Render_SetWireframeEnabled/IsWireframeEnabled(bool)`

### Post-Processing

- **Tone Mapping**: `PostProcess_SetToneMapping/GetToneMapping(int)`, `PostProcess_SetExposure/GetExposure(float)`, `PostProcess_SetGamma/GetGamma(float)`
- **Bloom**: `PostProcess_SetBloomEnabled/IsBloomEnabled(bool)`, `PostProcess_SetBloomThreshold/GetBloomThreshold(float)`, `PostProcess_SetBloomIntensity/GetBloomIntensity(float)`
- **Vignette**: `PostProcess_SetVignetteEnabled/IsVignetteEnabled(bool)`, `PostProcess_SetVignetteIntensity/GetVignetteIntensity(float)`, `PostProcess_SetVignetteSmoothness/GetVignetteSmoothness(float)`
- **Chromatic Aberration**: `PostProcess_SetChromaticAberrationEnabled/IsChromaticAberrationEnabled(bool)`, `PostProcess_SetChromaticAberrationIntensity/GetChromaticAberrationIntensity(float)`
- **Color Grading**: `PostProcess_SetColorFilter/GetColorFilter(Vector3)`, `PostProcess_SetSaturation/GetSaturation(float)`, `PostProcess_SetContrast/GetContrast(float)`, `PostProcess_SetBrightness/GetBrightness(float)`
- **Film Grain**: `PostProcess_SetFilmGrainEnabled/IsFilmGrainEnabled(bool)`, `PostProcess_SetFilmGrainIntensity/GetFilmGrainIntensity(float)`
- **FXAA**: `PostProcess_SetFXAAEnabled/IsFXAAEnabled(bool)`
- Note: PostProcess_ functions return sensible defaults if PostProcessing is unavailable (e.g. in Player app)

## Common Tasks

### Adding a new ECS Component

1. Create header in `Engine/include/Enjin/ECS/Components/`
2. Include in relevant systems
3. Optionally add inspector UI in `EditorLayer::DrawInspectorPanel()`

### Adding a new shader uniform

1. Update the UBO struct (`Light.h` for LightingUBO, `VulkanPipeline.h` for ViewProjectionUBO)
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
- **Particle Editor** — ~~DONE (Phase 2)~~ CPU particle simulation + GPU instanced renderer + editor panel with 12 presets (7 standard + 5 liquid), velocity stretch render mode, color gradient, size/speed curves, shape preview, playback controls. Future work: sub-emitter support, curve key editors, texture atlas animation, GPU particle simulation.
- **Node/Graph Editor** — ~~PARTIAL (Phase 1)~~ Generic node graph framework (`NodeGraphEditor` widget) with typed pin system (9 types, Wong colorblind-safe colors), Bezier curve links, drag-to-connect, pan/zoom, minimap, keyboard nav, box select, context menus, theme-aware colors, JSON serialization. First consumer: Animation Graph panel (`View > Animation Graph`) — visual state machine editor for `StateMachineComponent` with Entry pseudo-node, state/transition inspector sidebar, parameter editor, play mode highlighting, auto layout. "Open in Graph Editor" button in SM inspector. `editorPosition` serialized per SM state. Future consumers:
  - Visual scripting (blueprint-style alternative to AngelScript)
  - Shader graph (node-based shader authoring, generates GLSL/SPIR-V)
- **Script Component Workflow** — ~~DONE~~ Class name prompt, TegeBehavior boilerplate generation, auto-fill ScriptAttachment, open in configured IDE. Future work: open-file-at-line for script errors.
- **IDE Integration** — ~~DONE (Phase 1)~~ Editor settings for external IDE selection (Auto/VS Code, Visual Studio, Rider, Custom). Persistent settings, Browse for custom path, Test Open button. Future work: open-file-at-line support for script errors.
- **Undo/Redo Across All Operations** — extend existing UndoRedoManager to cover every inspector edit, hierarchy change, component add/remove, tilemap paint, terrain sculpt, etc.
- **Drag and Drop** — ~~PARTIAL~~ File drop from OS (Explorer/Finder) imports models (FBX/OBJ/glTF/GLB/DAE/3DS) and opens scenes (.enjin) via GLFW drop callback. Future work: drag assets from asset browser into viewport/inspector fields, drag entities in hierarchy for reparenting.
- **Hot-Swap Shaders** — edit shaders at runtime and see changes live without restarting. File watcher on .vert/.frag files, recompile GLSL to SPIR-V, recreate pipeline.
- **Improved Asset Import Pipeline** — on model/texture import, auto-process like Unity does: generate thumbnails, extract materials, set up serialization metadata, configure import settings (scale, axis, compression). Clean .enjinasset metadata files.
- **Extended Model Format Support** — add PLY (point cloud/mesh) and VOX (MagicaVoxel voxel) import via Assimp or custom loaders. PLY is useful for photogrammetry/scan data; VOX enables voxel art workflows. Also verify Assimp's existing FBX/OBJ/DAE import paths are robust (fix any crash-on-malformed-input issues).
- **Improved Icon/Window Inspector** — better entity icons in hierarchy, component icons in inspector, custom window icon picker in project settings.
- **Editor Settings vs Scene Settings** — ~~DONE~~ Separated into Settings (editor prefs) and Project Settings (rendering, physics) panels.
- **Template Rebuild & Demo Scenes** — Update and rebuild all 15 startup templates to use the latest engine features (per-scene render settings, particle system, UI system, etc.). For each template, create a small demo scene that showcases the template's intended gameplay. Add a "Demo" button on each template card in the selector that loads the demo scene instead of the blank template. Template card UI improvements: larger font for template titles, better visual hierarchy.
- **Planet Gravity 3D Third-Person Platformer Template** — New startup template: spherical/planetoid gravity third-person platformer (Super Mario Galaxy-style). Player walks on the surface of small planetoids with gravity that always points toward the planet center. Requires: a `PlanetGravityZone` component (sphere collider that overrides gravity direction toward its center, configurable radius and strength), integration with the existing gravity zone system (the `GravityZoneComponent` already supports directional/point/zero-G modes — planet gravity is a point-mode specialization with surface-aligned orientation), third-person camera that orbits relative to the planet surface normal rather than world up, and a `SurfaceAlignedController` that rotates the character to match the local gravity vector. Template scene: 3-4 small planetoids at different positions, player spawns on the largest one, can jump between them. Camera auto-adjusts "up" to match the planet the player is standing on.
- **Component Search Bar** — When clicking "Add Component" in the inspector, show a searchable dropdown/popup instead of the current flat list. Fuzzy text filter that narrows results as you type (like Unity's Add Component menu). Categorize components into groups (Rendering, Physics, Audio, Gameplay, Controllers, UI, Effects, etc.) with collapsible headers. Recently-used components pinned at the top. Keyboard navigation (arrow keys + Enter to select). This becomes essential as component count grows past 60+.
- **2D/3D Project & Component Separation** — Distinguish between 2D and 3D workflows at both the project and component level. Project-level: startup templates already hint at 2D vs 3D, but formalize this with a project mode setting (2D, 3D, or Mixed) stored in `.enjinproject`. Component-level: tag each component as 2D-specific (SpriteComponent, TilemapComponent, Platformer2DController, TopDown2DController), 3D-specific (MeshComponent, SkeletonComponent, FPSController, TPSController, GrassVolume, TreeVolume, TerrainComponent), or universal (Transform, Name, Tags, Light, AudioSource, Script, Health, etc.). In the Add Component menu and inspector, filter or visually separate components by project mode — a 2D project hides 3D-specific components by default (with an "Show All" toggle), and vice versa. The editor viewport should also adapt: 2D projects default to orthographic camera, snap to XY plane, show 2D grid. 3D projects default to perspective with 3D grid. Mixed mode shows everything. This reduces clutter and makes the engine less overwhelming for users who only need one dimension.
- **Curved Grid Snapping** — The curved/spherical grid (used in planet gravity and terrain contexts) should guide placed objects and land them on the surface. When placing or dropping entities near a curved grid, snap their position to the grid surface and align their orientation to the surface normal. This enables intuitive object placement on planetoids, terrain, and other non-flat surfaces without manual transform tweaking.
- **Entity Visibility Toggle** — ~~DONE~~ `TransformComponent::visible` bool (default true). RenderSystem skips invisible entities in all render passes (main, shadow, render-to-target, splitscreen, sprites, tilemaps, grass, shrubs, trees, particles). Inspector checkbox, hierarchy eye icon toggle, scene serialization (saved when false), script bindings `Entity_SetVisible`/`Entity_IsVisible`.
- **Project Hub & Creation Wizard** — Replace the current startup template selector with a full project management hub. Three tabs on launch: **Recent Projects** (thumbnails + last-modified, click to open), **New Project** (creation wizard), **Demos** (playable demo scenes showcasing engine features). New Project wizard flow: (1) Enter project name and choose save location (folder picker), (2) Browse templates with search bar and category filters — filter/sort by genre (Action, RPG, Strategy, Sim, Sports, Puzzle, Narrative), dimension (2D, 3D), player count (Single Player, Local Multiplayer, Online), and tags. Template cards show preview image, description, and feature list. (3) Select template, (4) Name the first scene (default: "Main"), (5) Engine auto-creates the project folder structure: `.enjinproject` manifest, `scenes/` dir with the named `.enjin` scene file, `scripts/` dir, `assets/` dir, `templates/` dir. All paths and references are set up correctly so the project is immediately runnable. The project folder is the single source of truth — self-contained, portable, and git-friendly. Collaboration considerations: project structure must support multiple people working simultaneously. Scene files should have a lock/checkout mechanism (advisory locks stored in `.enjinproject` or a `.enjinlock` file) so two people don't edit the same scene. Entity-level locking within a scene for finer-grained collaboration (lock entities you're editing, others see them as read-only). Integration with the planned Git panel — on project creation, optionally `git init` the project folder with a sensible `.gitignore` (excluding build output, `.enjpak` files, editor cache). For live collaboration (future): operational transform or CRDT-based scene sync where multiple editors see each other's changes in real-time, with conflict resolution at the entity/component level rather than file level.
- **Editor Accent Color & Theming** — Replace the current blue accent color with TEGE brand color `#c7dac4` (soft sage green). Use this as the primary accent throughout the editor (selected items, active tabs, buttons, progress bars, focus indicators). Complement with the existing blue as a secondary accent for links/info. Make the overall editor more aesthetically pleasant, cute, and inviting while maintaining readability. Consider: rounded corners on panels, softer panel borders, warmer background tones, subtle hover animations. The goal is a distinct visual identity — not Unity grey, not Unreal dark, not generic dev-tool blue.

### Runtime Systems

- **UI Runtime** — ~~DONE (Phase 1)~~ Anchored layout, 8 widget types, event bus, theme system implemented. Future work: flex/grid layout, text input widget, scrollable panels, runtime texture loading for Image widgets.
- **9-Slice / Text Box System** — Scalable UI backgrounds from sprite sheets using 9-slice (9-patch) rendering. A `NineSliceConfig` struct (texture path + 4 border insets in pixels) defines how a sprite is split into 9 regions: corners stay fixed-size, edges stretch in one axis, center stretches in both. The renderer generates a 9-quad mesh per element. This replaces flat-color Panel backgrounds with customizable, artist-friendly frames (dialogue boxes, buttons, tooltips, health bars). Should be simple to configure: drop in a sprite, set 4 inset values, done. Per-theme 9-slice defaults so all Panels/Buttons in a theme share the same frame style. Alternative considered: SDF rounded rectangles (resolution-independent but less artist-customizable). 9-slice is the better fit for a game engine UI.
- **2D Camera System** — follow targets, camera bounds/clamping, smooth follow, look-ahead, screen shake, zoom, dead zones, multi-target framing.
- **Particle System Runtime** — ~~DONE (Phase 1)~~ CPU simulation with 5 emitter shapes, piecewise-linear size/speed curves, color/alpha interpolation, gravity, drag, rotation. GPU instanced billboard rendering. Future work: GPU compute particle simulation, sub-emitters, particle collision, attractors, force fields.
- **Improved Physics** — 2D physics (Box2D-style), 2D joints, continuous collision detection, more shape types, physics materials (friction, bounce), trigger callbacks from scripts.
- **Basic Networking** — client-server architecture, state synchronization, entity ownership, lobbies, RPCs, lag compensation. Start with LAN/direct connect, then relay servers later.
- **Destructible Environments** — extend DestructibleComponent to work as a prefab-level setting. When enabled on a prefab, all instances inherit destructibility. Add fracture/shatter visual effects (mesh splitting into fragments on destroy), debris physics, chain destruction propagation. Editor toggle: "Destructible" checkbox on prefab inspector.
- **Improved Shadow System** — ~~DONE (Phase 1)~~ 4-cascade CSM with practical split scheme, texel-size stabilization (anti-swimming), per-cascade scaled bias, distance fade, configurable shadow distance/resolution/strength, Project Settings UI, per-scene serialization. Future work: soft shadows with PCSS, transparent shadow receivers, per-light shadow quality settings, point/spot light shadow maps.
- **Per-Scene Rendering Settings** — ~~DONE~~ `SceneRenderSettings` struct captures all RenderSystem + PostProcessSettings state (~60 fields). Serialized in scene files as `"renderSettings"` JSON section. Project-level defaults stored in `.enjinproject` manifest. Applied on scene load/new scene, saved/restored around play mode. Project Settings panel has "Use Project Defaults" checkbox + "Set Current as Project Default" / "Reset to Project Default" buttons. Old scenes without `renderSettings` gracefully default.
- **Simple Fluid Simulation** — Lightweight 2D/3D fluid system for water, lava, and gas effects. Grid-based Eulerian approach (not SPH — simpler, more predictable, easier to integrate with existing rendering). Core features: velocity field advection, pressure solve (Jacobi iteration), density transport, boundary conditions from colliders. Rendering via particle injection into existing particle system or a dedicated screen-space fluid surface renderer. Use cases: flowing water in platformers, lava pools, gas/smoke volumes, potion/liquid physics. Editor integration: FluidVolumeComponent (defines simulation bounds, resolution, viscosity, gravity response), real-time preview in viewport, preset configs (Water, Lava, Smoke, Steam). Performance target: 64x64 2D grid or 32x32x32 3D grid at 60fps on mid-range GPU via compute shader. Potential extensions: two-way coupling with rigidbodies, buoyancy forces, heat-driven convection, surface tension.
- **Tweening System** — ~~DONE~~ TweenComponent with 25 easing functions, 6 tweeneable properties (Position, Rotation, Scale, BaseColor, EmissiveColor, Opacity), Once/Loop/PingPong modes. TweenSystem ticks during play mode, auto-cleans completed tweens. Inspector UI with per-entry editors. AngelScript bindings: `Tween_Position`, `Tween_Scale`, `Tween_Rotation`, `Tween_Color`, `Tween_Opacity`, `Tween_StopAll` with `EasingType` enum constants. Scene serialization. Future work: `Tween_Float` with callback, `OnComplete` callback support, Timeline/Sequencer integration, chainable API from scripts.
- **State Machine System** — Built-in finite state machine as a first-class ECS component (`StateMachineComponent`). Define states with enter/update/exit callbacks, transitions with conditions (bool, float threshold, trigger), and a current state. Works for AI behavior (idle → patrol → chase → attack), game flow (menu → playing → paused → game over), animation (idle → walk → run → jump — complementing the existing AnimationStateMachine), UI state (hidden → appearing → visible → disappearing), and any entity-level logic that follows a state pattern. C++ API: `StateMachine::AddState(name, onEnter, onUpdate, onExit)`, `AddTransition(from, to, condition)`, `SetState(name)`, `SendTrigger(name)`. AngelScript bindings: `SM_AddState(entity, name)`, `SM_AddTransition(entity, from, to, conditionName)`, `SM_SetState(entity, name)`, `SM_SendTrigger(entity, name)`, `SM_GetCurrentState(entity)`. Editor inspector: visual state list with transition arrows, click to add states/transitions, test triggers in play mode. Optional node-graph editor integration (from the planned Node/Graph Editor) for visual state machine authoring.
- **Dialogue System** — Runtime dialogue tree system for NPC conversations, cutscene text, branching narratives, and interactive fiction. Core: `DialogueTreeAsset` (serialized JSON/binary) with nodes (Text, Choice, Branch, Event, Random) connected by edges. `DialogueComponent` on an entity references a tree asset and tracks playback state (current node, visited nodes, variable state). `DialogueRunner` processes nodes: displays text with speaker name/portrait, presents choices, evaluates conditions (inventory checks, quest flags, stat thresholds via script callbacks), fires events (start quest, give item, play animation, change relationship). Integrates with: SubtitleSystem (accessibility captions), UICanvasComponent (runtime dialogue box rendering), AngelScript (condition/event callbacks), SaveSystem (dialogue progress persistence). Editor: dedicated Dialogue Editor panel with node graph (drag to connect, preview text, test branches), import/export for external tools (Yarn Spinner JSON, Twine). Script bindings: `Dialogue_Start(entity, treeName)`, `Dialogue_Advance(entity)`, `Dialogue_Choose(entity, choiceIndex)`, `Dialogue_SetVariable(name, value)`, `Dialogue_GetVariable(name)`. Localization-ready: text nodes reference string keys, actual text pulled from a locale table.

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
  - **Wave Function Collapse (WFC)** — tile-based generation from example patterns, adjacency constraints, backtracking solver (bag of tile sets, weight per tile). Key references to study before implementation:
    - Maxim Gumin's original WFC repo and algorithm description: https://github.com/mxgmn/WaveFunctionCollapse
    - Oskar Stalberg (Bad North, Townscaper) — "Wave Function Collapse in Bad North" EPC2018 talk: https://www.youtube.com/watch?v=0bcZb-SsnrA — covers practical WFC tile assembly for procedural island dioramas, marching cubes on irregular grids
    - Anastasia Opara (Tiny Glade, ex-SEED/Embark) — "Creativity of Rules and Patterns" GDC2018: https://www.ea.com/seed/news/seed-gdc-2018-presentation-slides-creativity-of-rules-and-patterns — procedural systems design philosophy. Also her texture synthesis work (example-based generation closely related to WFC): https://medium.com/embarkstudios/texture-synthesis-and-remixing-from-a-single-example-faf5f4e8a5b8
    - Isaac Karth & Adam M. Smith — "WaveFunctionCollapse is Constraint Solving in the Wild" (2017 workshop paper formalizing WFC as ASP) and follow-up on backtracking heuristics
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

- **Script Rendering Bindings** — ~~DONE~~ 64 AngelScript wrapper functions exposing RenderSystem (shadows, ambient, fog, snow, rain, curvature, wireframe — 28 functions) and PostProcessSettings (tone mapping, exposure, bloom, vignette, chromatic aberration, color grading, film grain, FXAA — 36 functions). Null-safe: PostProcess_ functions return defaults in Player app. Wired through PlayMode (editor) and directly in Player app. Future work: additional bindings for retro effects (CRT, dithering, VHS), skybox control from scripts.
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

- **Git Integration** — built-in git panel in editor. Stage, commit, push, pull, branch, merge. Visual diff for scenes (structured JSON diff, not raw text). On new project creation (via Project Hub wizard), optionally `git init` with a sensible `.gitignore`. Branch-per-scene workflow support.
- **Scene & Entity Locking** — advisory lock system for multi-user workflows. Scene-level locks (`.enjinlock` file or manifest entry) prevent two people from editing the same scene. Entity-level locks within a scene for finer-grained collaboration — lock the entities you're editing, others see them as read-only with a visual indicator (lock icon in hierarchy, dimmed in inspector). Lock ownership tracked by user identity (git username or configured display name). Locks are advisory — can be force-released by project admin. Integrates with the Project Hub's collaboration features.
- **Session Sharing / Collaborative Editing** — real-time or turn-based collaboration with clear session locks showing who is editing which entity/scene. Prevents merge conflicts at the editor level. Long-term: operational transform or CRDT-based scene sync where multiple editors see each other's changes in real-time, with conflict resolution at the entity/component level rather than file level. Presence indicators (colored cursors/selection outlines per user in viewport).
- **Clean Git Serialization** — scene files must serialize deterministically (sorted keys, stable ordering, no floating-point drift noise). Unity's random YAML reordering and index GUIDs are the anti-pattern. Our JSON scenes should diff cleanly and merge predictably.

### NOT Planned Yet (Future)

- Asset marketplace / template exchange (needs servers, not now)
- Cloud build pipelines
- Analytics dashboard
