# Enjin Engine Architecture Documentation

## Overview

Enjin Engine is a proprietary, licensable 3D game engine built from scratch using C++20 and the Vulkan graphics API. It features a complete ImGui-based editor, an Entity-Component-System architecture, and modern rendering capabilities.

## System Architecture

```
┌─────────────────────────────────────────────────────────┐
│                    Application Layer                     │
│  (Editor, Game Runtime)                                 │
└─────────────────────────────────────────────────────────┘
                            │
┌─────────────────────────────────────────────────────────┐
│                    Engine Layer                          │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  │
│  │   Renderer   │  │   Physics    │  │    Audio     │  │
│  │   System     │  │   System     │  │   System     │  │
│  └──────────────┘  └──────────────┘  └──────────────┘  │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  │
│  │    ECS       │  │   Scene      │  │   Effects    │  │
│  │   System     │  │   System     │  │   System     │  │
│  └──────────────┘  └──────────────┘  └──────────────┘  │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  │
│  │   Editor     │  │   Assets     │  │  Procedural  │  │
│  │   System     │  │   System     │  │  Generation  │  │
│  └──────────────┘  └──────────────┘  └──────────────┘  │
└─────────────────────────────────────────────────────────┘
                            │
┌─────────────────────────────────────────────────────────┐
│                    Core Layer                            │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  │
│  │   Memory     │  │     Math     │  │   Logging    │  │
│  │  Management  │  │   Library    │  │   System     │  │
│  └──────────────┘  └──────────────┘  └──────────────┘  │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  │
│  │  Platform    │  │   Window     │  │    Input     │  │
│  │  Abstraction │  │   System     │  │   System     │  │
│  └──────────────┘  └──────────────┘  └──────────────┘  │
└─────────────────────────────────────────────────────────┘
                            │
┌─────────────────────────────────────────────────────────┐
│                 Vulkan API Layer                         │
│  (Instance, Device, Queues, Commands, Swapchain)        │
└─────────────────────────────────────────────────────────┘
```

## File Structure

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
│   │   ├── AI/             # AIBehaviors, Navmesh
│   │   ├── Animation/      # Sprite + skeletal animation framework
│   │   ├── Assets/         # GLTFLoader, SceneImporter, Prefab
│   │   ├── Audio/          # AudioSystem, SimpleAudio
│   │   ├── ECS/            # Entity-Component-System
│   │   │   ├── Components/ # 40+ component types
│   │   │   │   ├── Controllers/  # 5 character controller types
│   │   │   │   └── ...
│   │   │   └── Systems/    # RenderSystem, ControllerSystem
│   │   ├── Editor/         # EditorLayer, PlayMode, UndoRedo
│   │   ├── Effects/        # Weather, Water, Wind, RetroEffects
│   │   ├── GUI/            # ImGui integration
│   │   ├── Physics/        # SimplePhysics
│   │   ├── Platform/       # FileDialog
│   │   ├── Procedural/     # LevelGenerator
│   │   ├── Renderer/       # Vulkan renderer
│   │   │   └── Vulkan/     # VulkanContext, Pipeline, Buffer, etc.
│   │   └── Scene/          # SceneSerializer, SceneManager
│   ├── shaders/            # GLSL shaders (triangle.vert/frag, grass.vert/frag)
│   └── src/
│
├── Editor/                  # Editor application (main.cpp entry point)
│
├── third_party/            # External dependencies
│   ├── imgui/              # Dear ImGui (UI)
│   └── imguizmo/           # Transform gizmos
│
└── build/                  # Build output (bin/, lib/)
```

## Key Systems

### Rendering System

**Components**:
- `VulkanRenderer` - Main renderer, swapchain management
- `VulkanContext` - Vulkan instance, device, queues
- `VulkanPipeline` - Graphics pipeline with descriptor sets
- `VulkanBuffer` - GPU buffers (vertex, index, uniform, storage)
- `RenderSystem` - ECS system that renders entities with Mesh+Transform
- `PostProcessing` - Bloom, vignette, color grading, FXAA, film grain
- `RenderTarget` - Offscreen rendering for Game View

**Features**:
- Blinn-Phong lighting with multi-light support
- PBR material system with base color, normal, and height maps
- Shadow mapping with PCF filtering
- Skeletal animation with GPU skinning (bone SSBO)
- Instanced grass rendering
- Retro rendering effects (per-material)
- Wireframe rendering mode
- Text-to-texture rasterization (stb_truetype)

### ECS System

**Components**:
- `World` - Main ECS container managing entities and components
- `Entity` - ID-based entities (u64)
- 40+ component types across categories:
  - Core (Transform, Mesh, Material, Light, Camera, Name, Notes, Text)
  - Controllers (Platformer2D, TopDown2D, TopDown3D, ThirdPerson, FirstPerson)
  - Physics (Rigidbody, BoxCollider, SphereCollider, CapsuleCollider, TriggerZone)
  - Environment (WeatherZone, WaterVolume, GrassVolume, Vegetation, Temperature, Gravity, CameraTrigger)
  - Gameplay (Health, Damage, Interactable, Pickup, Inventory, Timer, Audio, Tag, SpawnPoint)
  - AI (AIController, FollowTarget, LookAtTarget, Waypoint)
  - Visual (Billboard, ParticleEmitter, Sprite2D, AnimatedSprite2D, Tilemap, Camera2DBounds)
  - Other (StateMachine, Dialogue, Skeleton, Animator)

### Editor System

**Components**:
- `EditorLayer` - Main editor with ImGui panels and menus
- `PlayMode` - Play/Pause/Stop game preview with state save/restore
- `SceneManager` - Multi-scene project management with transitions
- Template Selector - Startup project templates

**Features**:
- 10 editor panels (Hierarchy, Inspector, Console, Asset Browser, Settings, Post Processing, Effects, Game View, Scene List, Stats Overlay)
- Transform gizmos (translate, rotate, scale) via ImGuizmo
- Entity selection via ray casting (click-to-select)
- Entity clipboard (Cut/Copy/Paste via JSON serialization)
- Scene management with project manifests and scene transitions
- Startup template selector with 6 built-in templates + custom templates

### Scene System

**Components**:
- `SceneSerializer` - Save/load scenes as JSON (.enjin files)
- `SceneManager` - Project manifests, scene lists, runtime loading, transitions

**Features**:
- Full serialization of all 40+ component types
- Project manifest format (.enjinproject)
- Scene build indices and start scene designation
- Scene transitions (Instant, Fade Black, Fade White, Cross Fade)
- Additive scene loading
- Save/load to string (for clipboard operations)

### Effects System

**Components**:
- `WindSystem` - Global wind affecting weather, vegetation, and grass
- `WeatherSystem` - Rain, snow, fog, storm with lightning
- `Water3D` - 3D water plane with Gerstner wave simulation
- `RetroEffects` - CRT, pixelation, dithering post-processing
- `GrassRenderer` - Instanced grass blades with wind sway
- `WeatherRenderer` - 3D weather particle rendering

### Physics System

- Collision detection (sphere-sphere, AABB-AABB, sphere-AABB)
- Rigidbody dynamics with gravity, drag, and constraints
- Raycast-based ground detection
- Auto-generated box colliders on model import
- Gravity zones (directional, point, zero-G overrides)

### Assets System

- `GLTFLoader` - Loads .gltf/.glb files (meshes, materials, skins, animations)
- `SceneImporter` - Converts loaded models to ECS entities
- `MeshFactory` - Primitive mesh generation (cube, sphere, plane, cylinder, cone, quad)

## Descriptor Bindings

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

## Push Constants (128 bytes, per-object)

```cpp
struct PushConstants {
    Matrix4 model;          // 64 bytes
    Vector3 baseColor;      // + metallic = 16 bytes
    Vector3 emissiveColor;  // + roughness = 16 bytes
    f32 emissiveStrength, opacity, alphaCutoff;
    i32 flags;              // bit field: render/alpha/texture/retro flags
    f32 parallaxScale;      // + padding = 16 bytes
};
```

## Design Patterns

### Component-Based Architecture
- Entities are u64 IDs
- Components are plain data structs
- Systems contain logic and operate on components
- Data-oriented design for cache efficiency

### Data-Driven Design
- Scenes saved as JSON
- Project manifests as JSON
- Material properties via component fields
- Room prefabs defined in JSON

## Code Conventions

- **Types:** `u8, u16, u32, u64, i8, i16, i32, i64, f32, f64, usize`
- **Namespaces:** `Enjin::Core`, `Enjin::Math`, `Enjin::Renderer`, `Enjin::ECS`, `Enjin::Editor`, `Enjin::Scene`, `Enjin::Effects`
- **Logging:** `ENJIN_LOG_INFO/WARN/ERROR/FATAL(Category, format, ...)`
- **Log Categories:** Core, Renderer, Physics, Audio, Asset, Script, Editor, Game, AI, Assets, Procedural, Animation
- **API export:** `ENJIN_API` macro for DLL export
