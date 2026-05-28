# Enjin Engine Manual

A lightweight game engine targeting retro aesthetics (PS1/PS2/GameCube/Dreamcast era), built with C++20 and Vulkan.

---

## Table of Contents

1. [Getting Started](#getting-started)
2. [Editor Interface](#editor-interface)
3. [Camera Controls](#camera-controls)
4. [Entity-Component-System](#entity-component-system)
5. [Components Reference](#components-reference)
6. [Effects Systems](#effects-systems)
7. [Keyboard Shortcuts](#keyboard-shortcuts)

---

## Getting Started

### Building the Engine

```bash
# From the project root
cd build && cmake .. && make -j$(nproc)

# Run the editor
./build/bin/EnjinEditor
```

### Compiling Shaders

```bash
glslangValidator -V Engine/shaders/triangle.vert -o Engine/shaders/triangle.vert.spv
glslangValidator -V Engine/shaders/triangle.frag -o Engine/shaders/triangle.frag.spv
```

---

## Editor Interface

The editor opens with a splash screen displaying "THE ENJIN ENGINE", then fades into the main interface.

### Panels

| Panel | Description |
|-------|-------------|
| **Hierarchy** | Shows all entities in the scene as a tree. Click to select, right-click for context menu. |
| **Inspector** | Displays and edits components of the selected entity. |
| **Viewport** | 3D scene view with gizmos for manipulating objects. |
| **Console** | Log output and debug messages. |
| **Asset Browser** | Browse and import assets (models, textures). |
| **Settings** | Editor and rendering settings, keyboard shortcuts reference. |
| **Post-Processing** | Configure post-processing effects (bloom, vignette, etc.). |
| **Effects** | Weather and water effect controls. |
| **Game View** | Live feed from the active game camera (play mode preview). |
| **Profiler** | Per-frame breakdown, FPS graph, scope-based profiling. |
| **Scene List** | Manage multiple scenes within a project. |
| **Skybox** | Configure procedural, cubemap, or solid color skybox. |
| **Vector Drawing** | Built-in vector art editor with 7 shape types. |
| **Pixel Editor** | Sprite pixel art editor with layers and retro presets. |
| **Bug Reports** | Report bugs and submit feedback with auto-captured diagnostics. |
| **Behavior Tree** | Visual node-based AI behavior tree editor. |
| **Procedural Gen** | 9 procedural generation algorithms with preview canvas. |

### Menu Bar

- **File**: New Scene, Open Scene, Save Scene, Import Model, Export Prefab
- **Edit**: Undo, Redo, Duplicate, Delete, Cut, Copy, Paste
- **Entity**: Create Empty, Create primitives (Cube, Sphere, Plane, Cylinder, Cone), Create lights, Create 2D (Sprite, Tilemap, Camera2D)
- **View**: Panels (toggle all editor panels), Settings, Effects, Tools sub-menus, Game View, Scene List
- **Build**: Build Game, Export HTML5
- **Tools**: Pixel Editor, Vector Drawing, Behavior Tree Editor, Procedural Generation, Sprite Sheet Importer
- **Help**: About, User Manual, Report Bug... (Ctrl+Shift+B), Send Feedback..., Bug Reports & Feedback

---

## Camera Controls

The editor uses FPS-style camera controls for intuitive navigation.

### Movement (Fly Mode)

| Key | Action |
|-----|--------|
| **W** | Move forward |
| **S** | Move backward |
| **A** | Strafe left |
| **D** | Strafe right |
| **Q** | Move down |
| **E** / **Space** | Move up |
| **Shift** | Sprint (2.5x speed) |
| **Scroll Wheel** | Adjust movement speed |

### Look Controls

| Input | Action |
|-------|--------|
| **Right Mouse + Drag** | Look around (free look) |
| **Middle Mouse + Drag** | Orbit around target point |

### Notes
- All WASD movement is on the horizontal plane (FPS-style)
- Vertical movement is handled separately with Q/E/Space
- Hold Shift for faster movement

---

## Entity-Component-System

Enjin uses an ECS architecture where:

- **Entity**: A unique ID (u64) representing a game object
- **Component**: Data attached to entities (Transform, Mesh, Material, etc.)
- **System**: Logic that processes entities with specific components

### Creating Entities

1. Use **Entity > Create Empty** for a blank entity
2. Use **Entity > Create [Primitive]** for pre-configured shapes
3. Use **File > Import Model** to load glTF/GLB files

### Entity Operations

- **Select**: Click in Hierarchy or Viewport
- **Duplicate**: Ctrl+D or Edit > Duplicate
- **Delete**: Delete key or Edit > Delete
- **Focus**: Press F to center camera on selected entity

---

## Components Reference

### Core Components

#### TransformComponent
Controls position, rotation, and scale of an entity in 3D space.

| Property | Type | Description |
|----------|------|-------------|
| Position | Vector3 | World position (X, Y, Z) |
| Rotation | Vector3 | Euler rotation in degrees |
| Scale | Vector3 | Scale multiplier per axis |

#### NameComponent
Assigns a human-readable name to an entity for identification in the editor.

| Property | Type | Description |
|----------|------|-------------|
| Name | String | Display name in Hierarchy |

#### NotesComponent
Attach developer notes to entities for documentation purposes.

| Property | Type | Description |
|----------|------|-------------|
| Notes | String | Multiline text notes |

---

### Rendering Components

#### MeshComponent
Defines the 3D geometry of an entity.

| Property | Type | Description |
|----------|------|-------------|
| Vertices | Array | Position, normal, UV per vertex |
| Indices | Array | Triangle indices |

#### MaterialComponent
PBR (Physically Based Rendering) material properties.

| Property | Type | Description |
|----------|------|-------------|
| Base Color | Vector3 | Albedo color (RGB 0-1) |
| Metallic | Float | Metal vs dielectric (0-1) |
| Roughness | Float | Surface roughness (0-1) |
| Emissive Color | Vector3 | Self-illumination color |
| Emissive Strength | Float | Emission intensity |
| Opacity | Float | Transparency (0-1) |
| Alpha Cutoff | Float | Alpha test threshold |

#### LightComponent
Adds a light source to the entity.

| Property | Type | Description |
|----------|------|-------------|
| Type | Enum | Directional, Point, Spot |
| Color | Vector3 | Light color (RGB) |
| Intensity | Float | Brightness multiplier |
| Range | Float | Attenuation distance (Point/Spot) |
| Spot Angle | Float | Cone angle in degrees (Spot only) |
| Cast Shadows | Bool | Enable shadow casting |

#### CameraComponent
Makes an entity a camera that can render the scene.

| Property | Type | Description |
|----------|------|-------------|
| Field of View | Float | Vertical FOV in degrees |
| Near Plane | Float | Near clipping distance |
| Far Plane | Float | Far clipping distance |
| Is Active | Bool | Whether this camera is active |
| Priority | Int | Higher priority cameras render first |

---

### 2D Rendering Components

#### Sprite2DComponent
Renders a 2D sprite from a texture atlas.

| Property | Type | Description |
|----------|------|-------------|
| Texture Path | String | Path to sprite texture |
| Src X/Y/W/H | Float | Source rectangle in texture |
| Size | Vector2 | Display size in world units |
| Pivot | Vector2 | Origin point (0-1, default center) |
| Tint | Vector3 | Color multiply |
| Alpha | Float | Transparency |
| Flip X/Y | Bool | Mirror the sprite |
| Sorting Layer | Int | Render order (higher = front) |
| Order In Layer | Int | Sub-order within layer |

#### AnimatedSprite2DComponent
Sprite with frame-based animation.

| Property | Type | Description |
|----------|------|-------------|
| Frame Width/Height | Float | Size of each frame |
| Frame Count | Int | Total frames in animation |
| Current Frame | Int | Active frame index |
| FPS | Float | Animation speed |
| Looping | Bool | Restart when finished |
| Playing | Bool | Animation is running |

#### TilemapComponent
Grid-based tile rendering for levels.

| Property | Type | Description |
|----------|------|-------------|
| Tileset Path | String | Tile atlas texture |
| Tile Width/Height | Float | Size of each tile |
| Map Width/Height | Int | Grid dimensions |
| Tiles | Array | Tile indices per cell |

---

### Physics Components

#### RigidbodyComponent
Adds physics simulation to an entity.

| Property | Type | Description |
|----------|------|-------------|
| Mass | Float | Object mass in kg |
| Drag | Float | Linear damping |
| Angular Drag | Float | Rotational damping |
| Use Gravity | Bool | Affected by gravity |
| Is Kinematic | Bool | Moved by code only |
| Velocity | Vector3 | Current linear velocity |
| Angular Velocity | Vector3 | Current rotation speed |

#### BoxColliderComponent
Axis-aligned box collision shape.

| Property | Type | Description |
|----------|------|-------------|
| Size | Vector3 | Box dimensions |
| Offset | Vector3 | Center offset from transform |
| Is Trigger | Bool | Overlap events only (no physics) |

---

### Gameplay Components

#### HealthComponent
Tracks entity health for damage systems.

| Property | Type | Description |
|----------|------|-------------|
| Current Health | Float | Current HP |
| Max Health | Float | Maximum HP |
| Is Invulnerable | Bool | Ignore damage |
| Regen Rate | Float | HP per second |

#### AudioSourceComponent
Plays sound effects and music.

| Property | Type | Description |
|----------|------|-------------|
| Clip Path | String | Audio file path |
| Volume | Float | Playback volume (0-1) |
| Pitch | Float | Playback speed |
| Loop | Bool | Repeat when finished |
| Spatial | Bool | 3D positioned audio |
| Min/Max Distance | Float | Attenuation range |

---

### Controller Components

Pre-built movement controllers for common game types.

#### Platformer2DController
Side-scrolling platformer movement.

| Property | Type | Description |
|----------|------|-------------|
| Move Speed | Float | Horizontal movement speed |
| Jump Force | Float | Initial jump velocity |
| Gravity | Float | Downward acceleration |
| Max Jumps | Int | Air jump count (1 = no double jump) |
| Coyote Time | Float | Jump grace period after leaving ground |

#### TopDown2DController
Overhead view movement (Zelda-style).

| Property | Type | Description |
|----------|------|-------------|
| Move Speed | Float | Movement speed |
| Dash Speed | Float | Dash ability speed |
| Dash Duration | Float | How long dash lasts |
| Dash Cooldown | Float | Time between dashes |

#### TopDown3DController
3D overhead view (RTS/ARPG style).

| Property | Type | Description |
|----------|------|-------------|
| Move Speed | Float | Movement speed |
| Sprint Multiplier | Float | Sprint speed bonus |
| Turn Speed | Float | Rotation speed |

#### ThirdPersonController
Over-the-shoulder camera control.

| Property | Type | Description |
|----------|------|-------------|
| Move Speed | Float | Movement speed |
| Camera Distance | Float | Follow distance |
| Camera Height | Float | Camera offset Y |
| Look Sensitivity | Float | Mouse look speed |
| Sprint Multiplier | Float | Sprint speed bonus |

#### FirstPersonController
FPS-style controls.

| Property | Type | Description |
|----------|------|-------------|
| Move Speed | Float | Movement speed |
| Look Sensitivity | Float | Mouse sensitivity |
| Jump Force | Float | Jump velocity |
| Sprint Multiplier | Float | Sprint speed bonus |
| Head Bob Amount | Float | View bob intensity |

---

### Logic Components

#### StateMachineComponent
Finite state machine for AI and game logic.

| Property | Type | Description |
|----------|------|-------------|
| Current State | String | Active state name |
| Previous State | String | Last state |
| State Timer | Float | Time in current state |
| Parameters | Map | Named values (float, int, bool, string) |

**Usage Example:**
```cpp
auto& sm = world.GetComponent<StateMachineComponent>(entity);
sm.SetBool("isJumping", true);
sm.SetFloat("speed", 5.0f);
if (sm.currentState == "idle" && sm.GetBool("isJumping")) {
    sm.currentState = "jump";
}
```

#### DialogueComponent
Interactive dialogue system.

| Property | Type | Description |
|----------|------|-------------|
| Dialogue Lines | Array | Text to display |
| Current Line | Int | Active line index |
| Is Active | Bool | Dialogue is showing |
| Auto Advance | Bool | Progress automatically |
| Typewriter Speed | Float | Characters per second |
| Speaker Name | String | Character name display |
| Speaker Portrait | String | Portrait image path |
| Choices | Array | Response options |

---

## Effects Systems

### How to Access Effects

1. Open the editor
2. Go to **View > Effects** or find the **Effects (Retro)** panel
3. Use the preset buttons or customize individual settings

### Retro Effects System

Transform your game's visuals to match classic console aesthetics.

#### Quick Presets

Click any preset button to instantly apply that era's visual style:

| Preset | Resolution | Key Features |
|--------|------------|--------------|
| **PS1** | 320x240 | Vertex jitter, affine warping, 16-bit color, dithering |
| **N64** | 320x240 | Bilinear filtering, heavy fog, subtle jitter |
| **PS2** | 512x448 | Clean rendering, light fog, 24-bit color |
| **GameCube** | 640x480 | Vibrant, clean, no artifacts |
| **SNES** | 256x224 | 15-bit color, pixel-perfect, 4:3 aspect |
| **Dreamcast** | 640x480 | Clean, slightly cool colors |

#### Individual Settings

**Resolution:**
- Render Width/Height: Internal resolution before upscaling
- Point Filtering: Nearest-neighbor (crispy pixels) vs bilinear
- Integer Scaling: Only scale by whole numbers (prevents blurring)
- Aspect Ratio: 4:3 for authentic CRT look

**Dithering (PS1 style):**
- None, Bayer 2x2, Bayer 4x4, Bayer 8x8, Blue Noise, Ordered
- Use Bayer 4x4 for authentic PS1 look

**Color Mode:**
- True Color (24-bit): Modern
- High Color (16-bit): PS1/Saturn
- 256 Colors: Retro PC
- 16 Colors: SNES
- Monochrome: Game Boy

**Vertex Jitter:**
- Emulates PS1's lack of sub-pixel precision
- Amount: How much vertices wobble (1.0 = PS1 authentic)
- Snap to Grid: Vertices snap to integer positions
- Grid Resolution: Virtual pixel grid (160 = PS1)

**Affine Texture Warping:**
- Emulates PS1's non-perspective-correct texturing
- Warp Strength: How much textures wobble
- Vertex Snapping: Combined with jitter for full PS1 effect

**CRT Filter:**
- Scanline Intensity: Darkness of scan lines
- Curved Screen: Barrel distortion
- Phosphor Glow: RGB subpixel bleeding
- Vignette: Corner darkening

**Fog:**
- Start/End Distance: Where fog begins and becomes opaque
- Color: Fog tint
- Hard Cutoff: Objects pop in (N64 style)

### Weather System

Create atmospheric weather effects with minimal performance impact.

#### Weather Types

| Type | Rain | Snow | Fog | Lightning |
|------|------|------|-----|-----------|
| Clear | - | - | - | - |
| Cloudy | - | - | Light | - |
| Rain | Medium | - | Light | - |
| Heavy Rain | Heavy | - | Medium | - |
| Snow | - | Heavy | Medium | - |
| Fog | - | - | Heavy | - |
| Storm | Heavy | - | Medium | Yes |

#### How to Use

In the Effects panel under "Weather":
1. Select a weather type from the dropdown
2. Adjust intensity sliders for fine-tuning
3. Set fog color and range for atmosphere
4. Configure wind direction and strength

#### Settings Reference

**Particle Settings:**
- Rain Intensity: 0-1 (particles per second)
- Snow Intensity: 0-1
- Spawn Radius: Area around camera for particles
- Spawn Height: How high above camera to spawn

**Fog:**
- Density: Overall fog thickness
- Start Distance: Where fog begins
- End Distance: Where fog is fully opaque
- Color: RGB fog color

**Wind:**
- Direction: X/Z vector for wind
- Strength: Multiplier for particle drift

**Lightning (Storm only):**
- Triggers randomly during storms
- Flash intensity varies
- Use `IsLightningActive()` and `GetLightningIntensity()` in code

### Water System

Create retro-style water surfaces (PS1/N64/PS2/GameCube era).

#### Water Styles

| Style | Era | Description |
|-------|-----|-------------|
| Flat | Very Retro | Solid color plane |
| Animated | SNES | UV scrolling texture |
| Vertex Wave | PS1/N64 | Sine wave vertex displacement |
| Reflective | PS2/GC | Simple planar reflections |
| Refractive | Late PS2 | Reflection + refraction |

#### 3D Water Settings

**Geometry:**
- Position: World position of water plane
- Width/Depth: Size of water area
- Tile Size: Mesh tessellation (smaller = more detail)

**Colors:**
- Shallow Color: Color near edges
- Deep Color: Color in center/deep areas
- Opacity: Transparency level

**Waves:**
- Wave Speed: Animation speed
- Wave Height: Amplitude of displacement
- Wave Frequency: How many waves fit in the area
- Wave Direction: Primary wave direction

**UV Animation:**
- UV Scroll Speed X/Y: Texture movement speed

**Advanced:**
- Reflection Strength: Planar reflection intensity
- Fresnel Power: Edge reflection boost
- Foam: Enable foam at edges

#### 2D Water Settings (Side-scrollers)

- Wavy Top Edge: Animated sine wave at water surface
- Reflection: Flip sprites above water with distortion
- Parallax Layers: Multiple depth layers (SNES style)
- Caustics: Light patterns underwater

#### Code Example

```cpp
// In your game code:
Effects::Water3D water;
Effects::Water3DSettings settings;
settings.position = Math::Vector3(0, 0, 0);
settings.width = 100.0f;
settings.depth = 100.0f;
settings.style = Effects::WaterStyle::VertexWave;
settings.waveSpeed = 1.0f;
settings.waveHeight = 0.5f;
water.Initialize(settings);

// In update loop:
water.Update(deltaTime);
f32 heightAtPoint = water.GetWaveHeight(x, z);
```

---

## Keyboard Shortcuts

### General

| Key | Action |
|-----|--------|
| **Ctrl+N** | New Scene |
| **Ctrl+O** | Open Scene |
| **Ctrl+S** | Save Scene |
| **Ctrl+Z** | Undo |
| **Ctrl+Y** | Redo |
| **Delete** | Delete selected entity |
| **Ctrl+D** | Duplicate selected entity |
| **F** | Focus camera on selected entity |
| **Ctrl+Shift+B** | Report bug |
| **Ctrl+P** | Command palette |
| **Ctrl+1-5** | Focus panel (Hierarchy, Inspector, Console, Asset Browser, Settings) |

### Gizmo Controls

| Key | Action |
|-----|--------|
| **W** | Translate mode (move) |
| **E** | Rotate mode |
| **R** | Scale mode |
| **G** | Toggle local/world space |

### Play Mode

| Key | Action |
|-----|--------|
| **F5** | Play |
| **F6** | Pause |
| **F7** | Stop |

---

## Importing Assets

### 3D Models

Supported formats: `.gltf`, `.glb`, `.fbx`, `.obj`, `.dae`, `.3ds`, `.ply` (ASCII/binary), `.vox` (MagicaVoxel)

1. **File > Import Model**
2. Browse to your model file
3. Adjust import settings:
   - Scale factor
   - Combine meshes
   - Import materials
4. Click Import

The model hierarchy is preserved as child entities.

### Textures

Supported formats: `.png`, `.jpg`, `.tga`, `.bmp`

Textures are loaded automatically when referenced by materials or sprites.

---

## Scene Serialization

Scenes are saved as JSON files (`.enjin` extension).

### Save Scene
1. **File > Save Scene** (or Ctrl+S)
2. Enter filename
3. All entities, components, and settings are preserved

### Load Scene
1. **File > Open Scene** (or Ctrl+O)
2. Browse to `.enjin` file
3. Current scene is replaced

---

## Tips and Best Practices

1. **Use the Hierarchy** for organization - rename entities descriptively
2. **Notes Component** - document complex setups for future reference
3. **Game View Panel** - preview how the game camera sees the scene
4. **Focus (F key)** - quickly navigate to entities
5. **Middle Mouse Orbit** - rotate around a point for detailed inspection
6. **Sorting Layers** - for 2D games, use layers to control draw order
7. **State Machines** - keep state names simple: "idle", "walk", "jump", "attack"

---

## Troubleshooting

### Camera Not Moving
- Check if a UI panel has focus (click in Viewport first)
- Verify camera controller is enabled in code

### Model Not Visible
- Check scale (import scale might be too small/large)
- Verify material is assigned
- Check if entity has TransformComponent

### Dark Scene
- Add a directional light (Entity > Create Light > Directional)
- Increase light intensity
- Check ambient light settings

---

## Spline System

Create smooth paths for camera rails, enemy patrols, and procedural placement.

### Spline Types

| Type | Description | Use Case |
|------|-------------|----------|
| Linear | Straight lines between points | Simple paths |
| Bezier | Cubic Bezier with tangent control | Precise artistic paths |
| Catmull-Rom | Smooth curve through all points | Natural-looking paths |
| B-Spline | Smooth curve (doesn't pass through points) | Very smooth motion |

### Basic Usage

```cpp
#include "Enjin/Math/Spline.h"

// Create a patrol path
Math::Spline path(Math::SplineType::CatmullRom);
path.AddPoint(Math::Vector3(0, 0, 0));
path.AddPoint(Math::Vector3(10, 0, 5));
path.AddPoint(Math::Vector3(15, 0, 15));
path.AddPoint(Math::Vector3(5, 0, 20));
path.SetClosed(true);  // Loop the path

// Evaluate position along path (t = 0 to 1)
Math::Vector3 pos = path.Evaluate(0.5f);  // Halfway point
Math::Vector3 tangent = path.EvaluateTangent(0.5f);  // Direction

// Get complete transform for camera/character
Math::Vector3 position, forward, up, right;
path.EvaluateFrame(0.5f, position, forward, up, right);

// Distance-based movement (constant speed)
f32 distance = 10.0f;  // Travel 10 units along path
f32 t = path.DistanceToT(distance);
Math::Vector3 posAtDistance = path.Evaluate(t);
```

### Properties

| Property | Description |
|----------|-------------|
| Closed | Whether the spline loops back to start |
| Tension | Catmull-Rom tightness (0=tight, 1=loose) |
| Roll | Rotation around the tangent at each point |

### 2D Splines

For side-scrollers and top-down games:

```cpp
Math::Spline2D path2d;
path2d.AddPoint(Math::Vector2(0, 0));
path2d.AddPoint(Math::Vector2(100, 50));
path2d.AddPoint(Math::Vector2(200, 0));

Math::Vector2 pos = path2d.Evaluate(0.5f);
Math::Vector2 normal = path2d.EvaluateNormal(0.5f);  // Perpendicular
```

---

## AI System

ECS-based enemy AI with state machine, navmesh pathfinding, and behavior trees.

### AIControllerComponent

Add an `AIControllerComponent` to any entity in the editor (Physics > AI Controller) to give it AI behavior. Configure in the inspector:

| Field | Description |
|-------|-------------|
| `state` | Current AI state: Idle, Patrol, Chase, Attack, Flee, Dead |
| `detectionRange` | How far the AI can see targets |
| `attackRange` | Distance at which the AI will attack |
| `moveSpeed` | Movement speed |
| `patrolPoints` | Vector of patrol waypoint positions |
| `useNavmesh` | Enable navmesh-based pathfinding |
| `repathInterval` | How often to recalculate path (seconds) |
| `is2D` | Use 2D movement for side-scrollers / top-down |

### AngelScript Bindings

```angelscript
// Set/get AI state
AI_SetState(entityId, 2);          // 0=Idle, 1=Patrol, 2=Chase, 3=Attack, 4=Flee, 5=Dead
int state = AI_GetState(entityId);

// Set target
AI_SetTarget(entityId, playerEntityId);

// Configure detection
AI_SetDetectionRange(entityId, 20.0f);
AI_SetAttackRange(entityId, 2.5f);
AI_SetMoveSpeed(entityId, 5.0f);
```

### Behavior Trees

For complex AI, use `BehaviorTreeComponent` with the visual behavior tree editor (Window > Behavior Tree Editor). Supports 20 node types including Sequence, Selector, Parallel, decorators, and gameplay actions (MoveTo, PlayAnimation, Wait).

---

## Animation System

### 2D Sprite Animation

```cpp
#include "Enjin/Animation/Animation.h"

using namespace Enjin::Animation;

// Create animation from sprite sheet
SpriteAnimation walk = AnimationUtils::CreateFromSpriteSheet(
    "walk",                    // Name
    "textures/player.png",     // Texture path
    32, 32,                    // Frame width, height
    8,                         // Frame count
    12.0f,                     // FPS
    4                          // Columns in sheet
);
walk.playMode = PlayMode::Loop;

// Create animator and add animation
SpriteAnimator animator;
animator.AddAnimation(walk);

// Play animation
animator.Play("walk");

// Update each frame
animator.Update(deltaTime);

// Get current frame info for rendering
const SpriteFrame* frame = animator.GetCurrentFrame();
if (frame) {
    // Use frame->srcX, srcY, srcWidth, srcHeight for UV coordinates
}
```

### 3D Skeletal Animation

```cpp
#include "Enjin/Animation/Animation.h"

using namespace Enjin::Animation;

// Create skeleton
auto skeleton = std::make_shared<Skeleton>();
skeleton->name = "humanoid";
// Add bones (typically loaded from file)
Bone root;
root.name = "root";
root.parentIndex = -1;
skeleton->bones.push_back(root);

// Create animator
SkeletalAnimator animator;
animator.SetSkeleton(skeleton);

// Add animations
SkeletalAnimation idleAnim;
idleAnim.name = "idle";
idleAnim.duration = 2.0f;
// Add animation tracks for each bone
animator.AddAnimation(idleAnim);

// Play with blending
animator.Play("idle", 0.0f);      // No blend
animator.CrossFade("walk", 0.3f); // Blend over 0.3 seconds

// Update and get skinning matrices for GPU
animator.Update(deltaTime);
const auto& matrices = animator.GetSkinningMatrices();
```

### Animation State Machine

```cpp
// Create states
AnimationState idle;
idle.name = "Idle";
idle.animationName = "idle";

AnimationState walk;
walk.name = "Walk";
walk.animationName = "walk";

// Create state machine
AnimationStateMachine stateMachine;
stateMachine.SetAnimator(&animator);
stateMachine.AddState(idle);
stateMachine.AddState(walk);
stateMachine.SetDefaultState("Idle");

// Define transition
AnimationTransition toWalk;
toWalk.fromState = "Idle";
toWalk.toState = "Walk";
toWalk.blendTime = 0.2f;
TransitionCondition cond;
cond.parameterName = "speed";
cond.type = TransitionCondition::Type::Float;
cond.comparison = TransitionCondition::Comparison::Greater;
cond.value.floatValue = 0.1f;
toWalk.conditions.push_back(cond);
stateMachine.AddTransition(toWalk);

// Update parameters to drive transitions
stateMachine.SetFloat("speed", currentSpeed);
stateMachine.Update(deltaTime);
```

---

## Audio System

### Basic Usage

```cpp
#include "Enjin/Audio/AudioSystem.h"

using namespace Enjin::Audio;

// Initialize with default backend
AudioManager::Get().Initialize();

// Load a sound
SoundSettings settings;
settings.type = SoundType::SoundEffect;
SoundHandle sfx = AudioManager::Get().LoadSound("sounds/explosion.wav", settings);

// Play sound
ChannelHandle channel = AudioManager::Get().PlaySound(sfx);

// Play at 3D position
ChannelHandle channel3D = AudioManager::Get().PlaySoundAt(sfx, Math::Vector3(10, 0, 5));

// One-shot helper (loads, plays, manages lifetime)
AudioManager::Get().PlayOneShot("sounds/click.wav");
AudioManager::Get().PlayOneShotAt("sounds/impact.wav", position, 0.8f);

// Music with fade
AudioManager::Get().PlayMusic("music/level1.ogg", 2.0f);  // 2 second fade in
AudioManager::Get().StopMusic(1.5f);                       // 1.5 second fade out

// Volume controls
AudioManager::Get().SetMasterVolume(0.8f);
AudioManager::Get().SetSFXVolume(1.0f);
AudioManager::Get().SetMusicVolume(0.5f);

// Update listener for 3D audio
AudioManager::Get().SetListenerPosition(cameraPos, cameraForward, cameraUp);
```

### FMOD/Wwise Integration

```cpp
// Using FMOD backend
auto fmodBackend = std::make_unique<FMODBackend>();
AudioManager::Get().Initialize(std::move(fmodBackend));

// FMOD-specific features
auto* fmod = static_cast<FMODBackend*>(AudioManager::Get().GetBackend());
fmod->SetParameter("intensity", 0.8f);
fmod->TriggerEvent("event:/Music/Combat");

// Using Wwise backend
auto wwiseBackend = std::make_unique<WwiseBackend>();
AudioManager::Get().Initialize(std::move(wwiseBackend));

// Wwise-specific features
auto* wwise = static_cast<WwiseBackend*>(AudioManager::Get().GetBackend());
wwise->PostEvent("Play_Explosion", gameObjectId);
wwise->SetRTPC("Health", healthPercent, gameObjectId);
wwise->SetState("MusicState", "Combat");
```

---

## Procedural Level Generation

### 3D Dungeon Generation

```cpp
#include "Enjin/Procedural/LevelGenerator.h"

using namespace Enjin::Procedural;

LevelGenerator generator;

// Add room prefabs
RoomPrefab room = PrefabHelpers::CreateRectangularRoom(
    "basic_room",
    Math::Vector3(10, 4, 10),  // Size
    true, true, false, false    // N, S, E, W doors
);
room.category = "normal";
generator.AddPrefab(room);

// Add corridors
generator.AddPrefab(PrefabHelpers::CreateCorridor("corridor_h", 8, 3, 3, true));
generator.AddPrefab(PrefabHelpers::CreateTJunction("t_junction", 3, 3));

// Configure generation
LevelGenSettings settings;
settings.minRooms = 5;
settings.maxRooms = 15;
settings.targetRooms = 10;
settings.branchChance = 0.3f;
settings.seed = 12345;  // 0 for random

// Generate!
if (generator.Generate(settings)) {
    // Get results
    for (const auto& room : generator.GetPlacedRooms()) {
        // room.position, room.rotation, room.prefab->name
    }

    // Instantiate in ECS world
    generator.InstantiateInWorld(&world);
}
```

### 2D Platformer Generation

```cpp
LevelGenerator2D generator2D;

// Create 2D room templates
Room2D room = PrefabHelpers::CreateRoom2D("room", 10, 8, true, true, false, true);
generator2D.AddRoom(room);

LevelGen2DSettings settings2D;
settings2D.minRooms = 5;
settings2D.maxRooms = 10;
settings2D.tileSize = 16;

if (generator2D.Generate(settings2D)) {
    const auto& level = generator2D.GetLevel();
    // Access level.tiles for the complete tilemap
}
```

---

## Navmesh & Pathfinding

### Generating a Navmesh

```cpp
#include "Enjin/AI/Navmesh.h"

using namespace Enjin::AI;

NavmeshGenerator generator;

// Generate from geometry
NavmeshGenSettings settings;
settings.agentHeight = 2.0f;
settings.agentRadius = 0.5f;
settings.agentMaxSlope = 45.0f;

generator.Generate(vertices, indices, settings);

// Or generate a simple grid
generator.GenerateGrid(
    Math::Vector3(-50, 0, -50),  // Min
    Math::Vector3(50, 0, 50),    // Max
    1.0f                          // Cell size
);

Navmesh& navmesh = generator.GetNavmesh();
```

### Pathfinding

```cpp
Pathfinder pathfinder(&navmesh);

// Find path
PathResult result = pathfinder.FindPath(startPos, endPos);

if (result.success) {
    for (const auto& waypoint : result.waypoints) {
        // Follow the path
    }
}

// Configure pathfinder
pathfinder.SetSmoothPath(true);      // Smooth corners
pathfinder.SetStringPulling(true);   // Remove unnecessary waypoints
pathfinder.SetAreaCost(1, 2.0f);     // Make area type 1 cost double
```

### Path Following

```cpp
PathFollowerComponent follower;
follower.speed = 5.0f;
follower.turnSpeed = 180.0f;
follower.arrivalRadius = 0.5f;
follower.onPathComplete = []() { /* Arrived! */ };

PathFollower::SetPath(follower, pathResult);

// In update loop
PathFollower::Update(entity.position, entity.forward, follower, deltaTime);

// Check progress
float remaining = PathFollower::GetRemainingDistance(follower, entity.position);
```

---

## Undo/Redo System

### Basic Usage

```cpp
#include "Enjin/Editor/UndoRedo.h"

using namespace Enjin::Editor;

UndoRedoManager undoManager;

// Execute a move command
auto moveCmd = std::make_unique<MoveCommand>(
    &world, entity,
    oldPosition,
    newPosition
);
undoManager.Execute(std::move(moveCmd));

// Undo/Redo
if (undoManager.CanUndo()) undoManager.Undo();
if (undoManager.CanRedo()) undoManager.Redo();
```

### Compound Commands

```cpp
// Group multiple operations into one undo step
{
    UndoTransaction transaction(undoManager, "Move Multiple Objects");

    for (auto entity : selectedEntities) {
        undoManager.Execute(std::make_unique<MoveCommand>(
            &world, entity, oldPositions[entity], newPositions[entity]
        ));
    }
} // Automatically creates compound command when scope exits
```

---

## Prefab System

### Creating Prefabs

```cpp
#include "Enjin/Assets/Prefab.h"

using namespace Enjin::Assets;

// Create prefab from existing entity
auto prefab = PrefabManager::Get().CreateFromEntity(&world, entity, "MyPrefab");

// Save to file
PrefabManager::Get().SavePrefab(*prefab, "prefabs/myprefab.json");

// Load from file
auto loadedPrefab = PrefabManager::Get().LoadPrefab("prefabs/myprefab.json");
```

### Instantiating Prefabs

```cpp
// Basic instantiation
ECS::Entity instance = PrefabManager::Get().Instantiate(&world, *prefab);

// With transform offset
ECS::Entity instance = PrefabManager::Get().Instantiate(
    &world, *prefab,
    Math::Vector3(10, 0, 5),      // Position
    Math::Vector3(0, 45, 0),      // Rotation (euler degrees)
    Math::Vector3(1, 1, 1)        // Scale
);

// Check if entity is prefab instance
if (PrefabUtils::IsPrefabInstance(&world, entity)) {
    u64 prefabId = PrefabUtils::GetPrefabId(&world, entity);
}

// Unpack instance (remove prefab link)
PrefabManager::Get().UnpackInstance(&world, entity);
```

---

## Completed Features (Previously Planned)

- **Scripting Language**: AngelScript with ~170 bindings, hot-reload, coroutines, event system
- **Shadow Mapping**: 4-cascade CSM, point/spot shadows, Poisson disk soft shadows
- **Texture Support**: Base color, normal, height, metallic-roughness, emissive maps
- **Asset Hot-Reloading**: File watcher for textures and shaders with automatic reload
- **Ray Tracing Pipeline**: RT shadows, reflections, AO, GI, path tracing, SVGF denoiser

## Planned Features

- **Networking**: Client-server multiplayer with prediction and interpolation
- **Platform Ports**: Linux, macOS (MoltenVK), consoles, mobile, WebAssembly

---

*Enjin Engine - Collaborate. Compromise. Create.*
