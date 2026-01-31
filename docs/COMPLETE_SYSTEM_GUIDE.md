# Complete System Guide

## Overview

This guide covers all major systems in Enjin Engine and how to use them from the editor and from code.

## Editor Overview

### Startup Flow

When you launch the editor, you will see:

1. **Splash Screen** - Engine logo, fades after 3 seconds
2. **Template Selector** - Choose a project template or open an existing scene
3. **Editor** - Full editor with all panels

### Template Selector

The template selector appears after the splash screen and offers:

| Template | Description |
|----------|-------------|
| Blank | Empty scene with just a directional light |
| 2D Platformer | Side-scrolling setup with Platformer2D controller |
| 2D Top-Down | Overhead camera with TopDown2D controller |
| 3D Isometric | Fixed-angle 3D camera with TopDown3D controller |
| 3D Third Person | Over-the-shoulder camera with ThirdPerson controller |
| 3D First Person | FPS-style camera with FirstPerson controller |

Each template creates a ground plane, directional light, player entity with the appropriate controller, and a camera configured for that game type.

You can also:
- **Open an existing scene** from disk
- **Skip** to an empty scene
- Load **custom templates** saved from `File > Save as Template...`

### Editor Panels

Toggle panels from `View` menu:

- **Hierarchy** - Entity tree view, right-click to add/delete/duplicate
- **Inspector** - Component editor for selected entity
- **Console** - Log output and command input
- **Asset Browser** - Browse project files
- **Settings** - Grid, gizmo, and editor settings
- **Post Processing** - Bloom, vignette, color grading, FXAA, film grain
- **Effects (Retro)** - CRT, pixelation, dithering, color quantization
- **Game View** - Rendered game camera preview with play/pause/stop
- **Scene List** - Project scene management (add, reorder, load scenes)
- **Stats Overlay** - FPS, frame time graph, draw calls

### Keyboard Shortcuts

| Key | Action |
|-----|--------|
| `1` | Translate gizmo |
| `2` | Rotate gizmo |
| `3` | Scale gizmo |
| `4` | Toggle local/world space |
| `WASD` | Fly camera movement |
| `Space`/`E` | Move up |
| `Q`/`Ctrl` | Move down |
| `Shift` | Sprint |
| Hold RMB + Mouse | Look around |
| Left-click | Select entity |
| Double-click | Focus on entity |
| Scroll | Adjust move speed |
| `Ctrl+N` | New scene |
| `Ctrl+O` | Open scene |
| `Ctrl+S` | Save scene |
| `Ctrl+I` | Import model |
| `Ctrl+X/C/V` | Cut/Copy/Paste entity |

## Scene Management

### Project Files

Enjin uses `.enjinproject` files to manage multi-scene projects. A project file is a JSON manifest listing all scenes with their build indices.

```json
{
  "projectName": "My Game",
  "version": "1.0",
  "scenes": [
    { "name": "Main Menu", "path": "scenes/menu.enjin", "buildIndex": 0, "isStartScene": true },
    { "name": "Level 1", "path": "scenes/level1.enjin", "buildIndex": 1, "isStartScene": false },
    { "name": "Level 2", "path": "scenes/level2.enjin", "buildIndex": 2, "isStartScene": false }
  ]
}
```

### Using the Scene List Panel

1. Open `View > Scene List`
2. **Add scenes**: Click `+ Add Current Scene` or `+ Add Scene File...`
3. **Reorder**: Right-click a scene > Move Up/Down
4. **Set start scene**: Right-click > Set as Start Scene
5. **Load a scene**: Double-click or right-click > Load
6. **Load additive**: Right-click > Load Additive (keeps existing entities)
7. **Save project**: Click Save Project button or `File > Save Project`

### Scene Transitions

The Scene List panel includes transition controls:

- **Instant** - Immediate scene swap
- **Fade Black** - Fade to black, load, fade in
- **Fade White** - Fade to white, load, fade in
- **Cross Fade** - Overlap old and new scenes

Set the duration with the slider (0.1s to 3.0s). Click a scene's Quick Load button to load with the selected transition.

### Scene Management from Code

```cpp
// Get the scene manager from EditorLayer
Scene::SceneManager& manager = editor.GetSceneManager();

// Load a project
manager.LoadProject("path/to/project.enjinproject");

// Load scenes
manager.LoadScene("Level 1");
manager.LoadSceneByIndex(2);
manager.LoadStartScene();
manager.LoadSceneAdditive("HUD Overlay");

// Transitions
manager.LoadSceneWithTransition("Level 2", Scene::TransitionType::FadeBlack, 0.5f);

// Update transition each frame
manager.UpdateTransition(deltaTime);

// Callbacks
manager.SetOnSceneLoaded([](const std::string& name) {
    // Scene loaded
});
manager.SetOnSceneUnloaded([](const std::string& name) {
    // Scene unloaded
});
```

## ECS Components

### Core Components

| Component | Description |
|-----------|-------------|
| TransformComponent | Position, rotation (Euler), scale |
| NameComponent | Entity display name |
| MeshComponent | Vertex/index data for rendering |
| MaterialComponent | PBR material properties and textures |
| LightComponent | Directional, point, or spot light |
| CameraComponent | In-game camera with projection settings |
| NotesComponent | Text annotations |
| TextComponent | 3D text rendered to texture via stb_truetype |

### Controller Components

| Component | Description |
|-----------|-------------|
| Platformer2DController | Side-scrolling movement (jump, run, wall slide) |
| TopDown2DController | Overhead 2D movement |
| TopDown3DController | Isometric/top-down 3D movement |
| ThirdPersonController | Over-the-shoulder camera and movement |
| FirstPersonController | FPS-style camera and movement |

Adding a controller via `Entity > Add Component` auto-creates a configured camera entity.

### Physics Components

| Component | Description |
|-----------|-------------|
| RigidbodyComponent | Mass, velocity, gravity, drag, constraints |
| BoxColliderComponent | Axis-aligned box collision |
| SphereColliderComponent | Sphere collision with radius |
| CapsuleColliderComponent | Capsule collision (radius + height) |
| TriggerZoneComponent | Non-physical trigger volume with callbacks |

### Environment Components

| Component | Description |
|-----------|-------------|
| WeatherZoneComponent | Local weather override (rain, snow, fog) |
| WaterVolumeComponent | Water plane with wave parameters |
| GrassVolumeComponent | Instanced grass rendering volume |
| VegetationComponent | Vegetation sway affected by wind |
| TemperatureZoneComponent | Heat/cold zone with damage over time |
| GravityZoneComponent | Gravity override (directional, point, zero-G) |
| CameraTriggerComponent | Camera override when player enters volume |

### Gameplay Components

| Component | Description |
|-----------|-------------|
| HealthComponent | HP, max HP, invincibility, regeneration |
| DamageComponent | Damage amount, type, cooldown |
| InteractableComponent | Interact prompt, range, callback type |
| PickupComponent | Collectible item (health, ammo, key, coin, custom) |
| InventoryComponent | Item slots with max capacity |
| TimerComponent | Countdown/stopwatch with auto-reset |
| AudioSourceComponent | 3D audio with volume, pitch, spatial settings |
| AudioListenerComponent | Audio listener position |
| TagComponent | String tags and layer for entity classification |
| SpawnPointComponent | Respawn location with team/priority |

### AI Components

| Component | Description |
|-----------|-------------|
| AIControllerComponent | Behavior type (idle, patrol, chase, flee, wander) |
| FollowTargetComponent | Follow a target entity with speed/distance |
| LookAtTargetComponent | Rotate to face a target entity |
| WaypointComponent | Patrol waypoints with wait times |

### Visual Components

| Component | Description |
|-----------|-------------|
| BillboardComponent | Always-face-camera sprite |
| ParticleEmitterComponent | Particle system (burst/continuous, gravity, color) |
| Sprite2DComponent | 2D sprite with atlas support |
| AnimatedSprite2DComponent | Animated sprite with frame timing |
| TilemapComponent | Tile-based level with collision |
| Camera2DBoundsComponent | 2D camera boundary constraints |

### Other Components

| Component | Description |
|-----------|-------------|
| StateMachineComponent | Named states with transition rules |
| DialogueComponent | Branching dialogue with NPC name and choices |
| SkeletonComponent | Skeletal animation bone hierarchy |
| AnimatorComponent | Animation playback and state machine |

## Effects Systems

### Wind System

The wind system runs globally and affects weather particles, vegetation sway, and instanced grass. It is always active.

### Weather System

Configured globally in the Effects panel or per-zone via WeatherZoneComponent:

- **Rain** - Particle-based with configurable density
- **Snow** - Slower particles with drift
- **Fog** - Distance-based fog with density control
- **Storm** - Rain + lightning flashes

### Water System

3D water planes with Gerstner wave simulation. Configure via WaterVolumeComponent on entities or the global Water3D system.

### Retro Effects

Per-material and post-processing retro rendering:
- Flat shading (removes smooth interpolation)
- Affine texture mapping (PS1-style warping)
- Vertex snapping (low-poly jitter)
- Stipple transparency
- CRT scanlines, dithering, color quantization

### Post-Processing

Available in the Post Processing panel:
- Bloom (threshold, intensity, radius)
- Vignette (intensity, radius)
- Color grading (exposure, contrast, saturation, temperature)
- FXAA anti-aliasing
- Film grain (intensity, speed)

## Scene Serialization

Scenes are saved as `.enjin` JSON files. All component types are serialized including transforms, meshes, materials, lights, controllers, gameplay components, environment zones, and more.

```cpp
// Save
Scene::SceneSerializer serializer(world);
Scene::SerializationOptions opts;
opts.includeVertexData = true;
serializer.Save("scene.enjin", opts);

// Load (clears world)
auto result = serializer.Load("scene.enjin", true);

// Load additive (keeps existing entities)
auto result = serializer.LoadAdditive("scene.enjin");

// Serialize to/from string (for clipboard)
std::string json = serializer.SaveToString(opts);
auto result = serializer.LoadFromString(json, false);
```

## Procedural Generation

The `LevelGenerator` supports room-based procedural levels:

```cpp
Procedural::LevelGenerator gen;
gen.LoadPrefabsFromFile("prefabs/rooms.json");

// Generate a level
gen.Generate(seed, roomCount);

// Save/load prefab definitions
gen.SavePrefabsToFile("prefabs/rooms.json");
```

Room prefabs are defined in JSON with connection points, size constraints, weights, and tags.

## System Dependencies

```
Application
    ├── Renderer (VulkanRenderer)
    │   ├── RenderSystem (ECS rendering)
    │   ├── PostProcessing (bloom, vignette, etc.)
    │   └── RenderTarget (offscreen Game View)
    ├── ECS World
    │   ├── TransformComponent + MeshComponent + MaterialComponent
    │   ├── LightComponent (multi-light UBO)
    │   ├── CameraComponent (in-game cameras)
    │   ├── CharacterControllers (5 types)
    │   └── Gameplay/Physics/AI/Environment components
    ├── Editor
    │   ├── EditorLayer (ImGui panels)
    │   ├── PlayMode (play/pause/stop)
    │   ├── SceneManager (multi-scene projects)
    │   └── Template Selector (startup)
    ├── Effects
    │   ├── WindSystem (global wind)
    │   ├── WeatherSystem (rain, snow, fog, storm)
    │   ├── Water3D (Gerstner waves)
    │   └── RetroEffects (CRT, dithering)
    └── Scene
        ├── SceneSerializer (save/load)
        └── SceneManager (project manifests, transitions)
```
