# Enjin Engine Tutorial Book

A comprehensive, hands-on guide to building games with the Enjin Engine. Each tutorial builds on concepts from previous ones, but can also be followed independently.

---

## Table of Contents

### Part I: Foundations
1. [Your First Scene](#tutorial-1-your-first-scene)
2. [Working with Entities and Components](#tutorial-2-working-with-entities-and-components)
3. [Materials, Textures, and Lighting](#tutorial-3-materials-textures-and-lighting)
4. [Camera Setup and Viewports](#tutorial-4-camera-setup-and-viewports)
5. [Importing 3D Models](#tutorial-5-importing-3d-models)

### Part II: 2D Game Development
6. [Building a 2D Platformer](#tutorial-6-building-a-2d-platformer)
7. [Sprite Animation and Atlases](#tutorial-7-sprite-animation-and-atlases)
8. [Tilemap Editing](#tutorial-8-tilemap-editing)
9. [2D Physics with Box2D](#tutorial-9-2d-physics-with-box2d)
10. [Top-Down 2D Movement](#tutorial-10-top-down-2d-movement)

### Part III: 3D Game Development
11. [Third-Person Character Controller](#tutorial-11-third-person-character-controller)
12. [First-Person Shooter Basics](#tutorial-12-first-person-shooter-basics)
13. [Terrain Sculpting and Vegetation](#tutorial-13-terrain-sculpting-and-vegetation)
14. [Skeletal Animation](#tutorial-14-skeletal-animation)
15. [Physics and Collision](#tutorial-15-physics-and-collision)

### Part IV: Scripting
16. [AngelScript Basics](#tutorial-16-angelscript-basics)
17. [Visual Scripting with Blueprints](#tutorial-17-visual-scripting-with-blueprints)
18. [Coroutines and Timing](#tutorial-18-coroutines-and-timing)
19. [Event System and Communication](#tutorial-19-event-system-and-communication)
20. [State Machines](#tutorial-20-state-machines)

### Part V: Game Systems
21. [Save System and Persistence](#tutorial-21-save-system-and-persistence)
22. [Dialogue Trees and Narrative](#tutorial-22-dialogue-trees-and-narrative)
23. [Quest and Objective System](#tutorial-23-quest-and-objective-system)
24. [Inventory with Pickups](#tutorial-24-inventory-with-pickups)
25. [HUD and UI Canvas](#tutorial-25-hud-and-ui-canvas)

### Part VI: AI and Pathfinding
26. [AI Behaviors: Patrol, Chase, Flee](#tutorial-26-ai-behaviors-patrol-chase-flee)
27. [Behavior Trees](#tutorial-27-behavior-trees)
28. [NavMesh Pathfinding](#tutorial-28-navmesh-pathfinding)

### Part VII: Visual Effects
29. [Particle Systems](#tutorial-29-particle-systems)
30. [Weather and Atmosphere](#tutorial-30-weather-and-atmosphere)
31. [Post-Processing and Retro Effects](#tutorial-31-post-processing-and-retro-effects)
32. [Water and Fluid Simulation](#tutorial-32-water-and-fluid-simulation)
33. [Ray Tracing](#tutorial-33-ray-tracing)

### Part VIII: Audio
34. [Sound Effects and Music](#tutorial-34-sound-effects-and-music)
35. [Audio Event Graphs](#tutorial-35-audio-event-graphs)

### Part IX: Procedural Generation
36. [Dungeon Generation](#tutorial-36-dungeon-generation)
37. [Terrain Heightmaps with fBm](#tutorial-37-terrain-heightmaps-with-fbm)
38. [L-Systems for Plants](#tutorial-38-l-systems-for-plants)
39. [Wave Function Collapse](#tutorial-39-wave-function-collapse)
40. [Reaction-Diffusion Patterns](#tutorial-40-reaction-diffusion-patterns)
41. [Cellular Automata as Geometry](#tutorial-41-cellular-automata-as-geometry)
42. [Physarum Slime Mold Networks](#tutorial-42-physarum-slime-mold-networks)

### Part X: Animation and Timeline
43. [Timeline Sequencer](#tutorial-43-timeline-sequencer)
44. [Cinematic Cameras](#tutorial-44-cinematic-cameras)
45. [Tweening and Easing](#tutorial-45-tweening-and-easing)

### Part XI: Networking
46. [LAN Multiplayer Basics](#tutorial-46-lan-multiplayer-basics)
47. [Newgrounds.io Integration](#tutorial-47-newgroundsio-integration)

### Part XII: Building and Distribution
48. [Building Your Game](#tutorial-48-building-your-game)
49. [HTML5 Export](#tutorial-49-html5-export)
50. [Accessibility Best Practices](#tutorial-50-accessibility-best-practices)

### Part XIII: Advanced Topics
51. [Shader Graph](#tutorial-51-shader-graph)
52. [Plugin Development](#tutorial-52-plugin-development)
53. [Data Assets and Schemas](#tutorial-53-data-assets-and-schemas)
54. [Splitscreen Multiplayer](#tutorial-54-splitscreen-multiplayer)
55. [Performance Optimization](#tutorial-55-performance-optimization)

---

## Tutorial 1: Your First Scene

**Goal:** Create a simple scene with a colored cube, a light, and a camera.

### Step 1: Launch the Editor

Run `EnjinEditor.exe`. The splash screen appears, followed by the **Template Selector** dialog. For this tutorial, select **Empty Scene** to start with a blank canvas.

### Step 2: Create a Cube

1. In the **Hierarchy** panel, right-click and select **Add Entity**.
2. A new entity named "Entity" appears. Click it to select it.
3. In the **Inspector** panel, click **Add Component** and select **MeshComponent**.
4. The mesh defaults to a unit cube. You should see a white cube in the viewport.
5. Rename the entity: double-click its name in the Hierarchy and type "MyCube".

### Step 3: Add Color with a Material

1. With "MyCube" selected, click **Add Component > MaterialComponent**.
2. In the Inspector, find the **Material** section.
3. Change **Base Color** to any color you like (e.g., red: `1.0, 0.2, 0.2`).
4. The cube in the viewport updates immediately.

### Step 4: Add a Light

1. Right-click in Hierarchy > **Add Entity**. Rename it "Sun".
2. Add a **LightComponent** to it.
3. Set **Type** to `Directional`.
4. Set **Direction** to `(-0.5, -1.0, -0.3)` for angled sunlight.
5. Set **Color** to warm white `(1.0, 0.95, 0.9)` and **Intensity** to `1.5`.

### Step 5: Add a Camera

1. Right-click in Hierarchy > **Add Entity**. Rename it "MainCamera".
2. Add a **CameraComponent**.
3. Set the entity's **Transform > Position** to `(0, 2, 5)`.
4. The Game View panel (View > Game View) now shows what this camera sees.

### Step 6: Play Mode

1. Click the **Play** button (or press the play button in the Game View).
2. The scene runs with your camera's perspective.
3. Click **Stop** to return to edit mode.

**Key Concept:** Every visible object in Enjin needs at minimum a `TransformComponent` (auto-added) and a `MeshComponent`. Add `MaterialComponent` for color/textures, and `LightComponent` to illuminate the scene.

---

## Tutorial 2: Working with Entities and Components

**Goal:** Understand the ECS (Entity-Component-System) architecture.

### What is ECS?

Enjin uses an Entity-Component-System architecture:

- **Entity** — A unique ID (just a number). Has no data or behavior by itself.
- **Component** — Data attached to an entity. `TransformComponent` holds position/rotation/scale. `MeshComponent` holds geometry. `MaterialComponent` holds visual properties.
- **System** — Logic that processes entities with specific components. `RenderSystem` draws entities with Mesh+Transform. `ControllerSystem` moves entities with CharacterController.

### Creating Entities

**From the Hierarchy panel:**
- Right-click > Add Entity
- Right-click an entity > Add Child Entity (creates a parented entity)
- Ctrl+D duplicates the selected entity

**Common component combinations:**

| Object Type | Components Needed |
|------------|-------------------|
| Static prop | Transform + Mesh + Material |
| Animated character | Transform + Mesh + Material + Animator + CharacterController |
| Light source | Transform + Light |
| Camera | Transform + Camera |
| 2D sprite | Transform + Sprite2D (**must have texture set!**) |
| Sound emitter | Transform + AudioSource |
| Physics object | Transform + Mesh + Material + Rigidbody + BoxCollider |
| UI overlay | Transform + UICanvas |

### Multi-Select Operations

- **Ctrl+Click** — Toggle entity in selection
- **Shift+Click** — Range select in hierarchy
- **Drag in viewport** — Marquee/rubber-band select
- **Delete** — Delete all selected entities
- **Ctrl+D** — Duplicate all selected
- **Drag any selected entity** — Reparent the whole selection at once
- **Right-click a selected entity** — "Group Selected (N) into New Entity" parents the selection under a new empty entity named "Group"

### Parent-Child Relationships

Drag an entity onto another in the Hierarchy to parent it. Child entities inherit their parent's transform — moving the parent moves all children.

### Transform Component

Every entity has a `TransformComponent`:
- **Position** — World-space XYZ position
- **Rotation** — Stored as a Quaternion internally; displayed as Euler angles in the Inspector
- **Scale** — XYZ scale factors (1,1,1 = normal size)
- **Visible** — Toggle to show/hide the entity and all children

### Gizmo Controls

Use number keys to switch gizmo modes:
- `1` — Translate (move)
- `2` — Rotate
- `3` — Scale
- `4` — Toggle local/world coordinate space

---

## Tutorial 3: Materials, Textures, and Lighting

**Goal:** Apply PBR materials and set up a multi-light scene.

### PBR Material Properties

Enjin uses a physically-based rendering (PBR) pipeline. Each `MaterialComponent` has:

| Property | Range | Description |
|----------|-------|-------------|
| Base Color | RGB (0-1) | Surface color |
| Metallic | 0-1 | 0 = dielectric (plastic/wood), 1 = metal |
| Roughness | 0-1 | 0 = mirror smooth, 1 = matte |
| Emissive Color | RGB | Self-illumination color |
| Emissive Strength | 0+ | Brightness of emission |
| Opacity | 0-1 | Transparency (1 = fully opaque) |
| Alpha Cutoff | 0-1 | For cutout transparency (foliage) |

### Applying Textures

1. Select an entity with a MaterialComponent.
2. In the Inspector, find **Base Color Texture** and enter the path to a `.png` or `.jpg` file.
3. Optional texture channels:
   - **Normal Map** — Adds surface detail without geometry
   - **Height Map** — Enables parallax mapping (4 modes: Simple, Steep, Occlusion, Relief)
   - **Metallic-Roughness Map** — Packed PBR texture

### Light Types

| Type | Description | Key Properties |
|------|-------------|----------------|
| **Directional** | Sun-like, infinite distance | Direction, Color, Intensity |
| **Point** | Omnidirectional, falls off with distance | Position, Range, Color, Intensity |
| **Spot** | Cone-shaped | Direction, Inner/Outer Cone Angle, Range |

### Shadow Setup

1. Go to **View > Rendering > Rendering** panel.
2. Under **Shadows**, enable shadow mapping.
3. Adjust **Shadow Bias** if you see shadow acne.
4. The engine uses 4-cascade CSM (Cascaded Shadow Maps) for directional lights.
5. Per-entity: each `MeshComponent` has a `castShadows` flag (default: true).

### Example: Three-Point Lighting

Create three lights for cinematic quality:
1. **Key Light** — Directional, brightest, main shadow caster
2. **Fill Light** — Point light opposite the key, softer, no shadows
3. **Rim Light** — Point light behind subject, highlights edges

---

## Tutorial 4: Camera Setup and Viewports

**Goal:** Configure game cameras with different projection modes.

### Camera Types

Add a `CameraComponent` to any entity:

- **Perspective** — 3D depth (FOV, near/far planes)
- **Orthographic** — 2D flat projection (ortho size controls zoom)

### Camera Properties

| Property | Description |
|----------|-------------|
| FOV | Field of view in degrees (perspective only) |
| Near Plane | Closest visible distance |
| Far Plane | Farthest visible distance |
| Ortho Size | Half-height of the visible area (orthographic only) |
| Priority | Higher priority cameras are used first |

### 2D Camera Follow

For 2D games, the `CameraFollow2DComponent` provides:
- **Target Entity** — Entity to follow
- **Smoothing** — Camera lag (0 = instant, 1 = very smooth)
- **Dead Zone** — Area where the target can move without camera movement
- **Look-Ahead** — Camera leads in the movement direction
- **Bounds** — Min/max camera position limits
- **Shake** — Camera shake intensity and duration

### Camera Presets

Enjin includes 9 built-in camera presets accessible via scripting:
- Side Scroller, Top Down, Isometric, Over Shoulder, Fixed Angle, Cinematic, Orbit, Security Cam, First Person

---

## Tutorial 5: Importing 3D Models

**Goal:** Import a glTF model with materials and animations.

### Supported Formats

| Format | Extension | Notes |
|--------|-----------|-------|
| glTF | .gltf, .glb | Native support, recommended |
| FBX | .fbx | Via Assimp |
| OBJ | .obj | Via Assimp, no animation |
| Collada | .dae | Via Assimp |
| PLY | .ply | Point clouds |
| VOX | .vox | MagicaVoxel voxels |

### Importing via Editor

1. Press **Ctrl+I** or go to **File > Import Model**.
2. Browse to your model file.
3. The **Import Options** dialog appears:
   - **Scale** — Model scale factor (default 1.0)
   - **Import Materials** — Extract PBR materials from the file
   - **Import Animations** — Import skeletal animation data
   - **Generate Colliders** — Auto-create box colliders for meshes
4. Click **Import**. The model appears as one or more entities in the hierarchy.

### Import via Script

```angelscript
ImportOptions options;
options.scale = 0.01;  // e.g., cm to m conversion
options.importMaterials = true;
options.importAnimations = true;
Import("assets/character.glb", options);
```

### Asset Metadata

Each imported model creates a `.enjinasset` sidecar file storing import settings. Re-importing uses these saved settings for consistency.

---

## Tutorial 6: Building a 2D Platformer

**Goal:** Create a playable 2D platformer with player movement, platforms, and jumping.

### Step 1: Project Mode

When creating a new scene, choose the **Platformer** template, or set your project to 2D mode:
1. Go to **View > Settings > Project Settings**
2. Set **Project Mode** to `2D`
3. The grid switches to the XY plane

### Step 2: Player Entity

1. Create an entity named "Player".
2. Add a **Sprite2DComponent** with a character texture (e.g., `player.png`).
3. Add a **CharacterController** and set **Type** to `Platformer2D`.

> **Important:** A `Sprite2DComponent` **must** have a texture set, or it will be invisible. For placeholder shapes, use `MeshComponent` + `MaterialComponent` without `Sprite2DComponent`.

### Step 3: Controller Settings

In the CharacterController Inspector:
- **Move Speed** — How fast the player moves (default: 5.0)
- **Jump Force** — Initial jump velocity (default: 10.0)
- **Gravity** — Downward acceleration (default: -20.0)
- **Wall Jump** — Enable wall-jumping with configurable wall slide speed
- **Double Jump** — Allow a second jump in mid-air
- **Coyote Time** — Grace period after leaving a platform where jumping is still allowed

### Step 4: Ground and Platforms

1. Create entities named "Ground", "Platform1", "Platform2".
2. For each, add `MeshComponent` (box shape) and `MaterialComponent`.
3. Add `BoxColliderComponent` for collision (or `Body2DComponent` if using Box2D).
4. Position them to form a level layout.
5. Set **categoryBits** and **collisionMask** so the player collides with platforms.

### Step 5: Play and Test

Press Play. Use **A/D** or **Arrow Keys** to move, **Space** to jump. Adjust controller parameters until the movement feels right.

### Collision Filtering

The bitmask collision system uses:
- `categoryBits` — Which groups this entity belongs to
- `collisionMask` — Which groups this entity collides with
- **Bilateral rule:** Both entities must accept each other: `(A.categoryBits & B.collisionMask) && (B.categoryBits & A.collisionMask)`

Default values: `categoryBits = 1`, `collisionMask = 0xFFFFFFFF` (collide with everything).

---

## Tutorial 7: Sprite Animation and Atlases

**Goal:** Set up animated sprites using sprite sheets.

### Sprite Sheet Setup

1. Add a `Sprite2DComponent` to your entity.
2. Set the **Texture Path** to your sprite sheet image.
3. Configure the sprite sheet grid:
   - **Frame Width/Height** — Size of each frame in pixels
   - **Columns/Rows** — Grid dimensions
4. Add an `AnimatedSprite2DComponent` for animation playback.

### Animation Properties

| Property | Description |
|----------|-------------|
| Frame Rate | Frames per second |
| Start Frame | First frame index |
| End Frame | Last frame index |
| Loop | Whether to loop the animation |
| Playing | Whether animation is currently active |

### Sprite Texture Atlas

Enjin automatically packs small sprites (<=512px) into a shared 4096x4096 texture atlas at runtime. This dramatically reduces draw calls — sprites sharing the atlas render in one instanced draw call instead of one per texture.

The atlas is transparent to you; it happens automatically. Textures >512px are excluded and drawn individually.

### Script Control

```angelscript
void OnStart() {
    Sprite2D_SetTexture(self, "sprites/hero_walk.png");
    AnimSprite_SetFrameRate(self, 12);
    AnimSprite_Play(self);
}

void OnUpdate(float dt) {
    if (Input_IsKeyPressed(KEY_RIGHT)) {
        Sprite2D_SetFlipX(self, false);
        AnimSprite_Play(self);
    } else if (Input_IsKeyPressed(KEY_LEFT)) {
        Sprite2D_SetFlipX(self, true);
        AnimSprite_Play(self);
    } else {
        AnimSprite_Stop(self);
    }
}
```

---

## Tutorial 8: Tilemap Editing

**Goal:** Build levels using the tilemap editor.

### Creating a Tilemap

1. Create an entity and add a `TilemapComponent`.
2. In the Inspector, set:
   - **Tileset Path** — Path to your tileset image
   - **Tile Width/Height** — Size of each tile in pixels
   - **Map Width/Height** — Grid dimensions in tiles
3. Open the **Tilemap Editor** panel to paint tiles.

### Tilemap Editor Tools

- **Paint** — Click or drag to place tiles
- **Erase** — Remove tiles
- **Fill** — Flood-fill an area
- **Select** — Select and move regions

### Tile Layers

Tilemaps support multiple layers for depth:
- **Background** — Behind the player
- **Midground** — Player level
- **Foreground** — In front of the player

### Auto-Tiling

Set up tile rules for automatic neighbor-aware tile selection. The editor matches tiles based on adjacent tile types (corners, edges, inner corners).

---

## Tutorial 9: 2D Physics with Box2D

**Goal:** Set up 2D physics simulation using the Box2D backend.

### Enabling Box2D

Box2D v3.0.0 is enabled by default (`ENJIN_PHYSICS_BOX2D=ON` in CMake). The engine auto-selects Box2D for 2D project modes.

### Adding Physics Bodies

1. Select an entity.
2. Add a `Body2DComponent`:
   - **Type** — `Static` (immovable), `Dynamic` (physics-driven), `Kinematic` (script-driven)
   - **Shape** — `Box`, `Circle`, `Polygon`
   - **Density** — Mass density (affects weight)
   - **Friction** — Surface friction (0-1)
   - **Restitution** — Bounciness (0-1)
   - **Fixed Rotation** — Prevent spinning

### 2D Joint Types

| Joint | Description | Use Case |
|-------|-------------|----------|
| **Revolute** | Pivot/hinge | Doors, wheels |
| **Prismatic** | Slide along axis | Pistons, elevators |
| **Distance** | Fixed distance | Chains, bridges |
| **Rope** | Distance with max limit | Swinging ropes |
| **Weld** | Rigid attachment | Composite bodies |

### 2D Raycasting

```angelscript
// Cast a ray downward to check for ground
bool onGround = Physics2D_Raycast(
    GetPosition(self),          // origin
    Vec2(0, -1),               // direction
    1.0,                        // distance
    hitPoint, hitNormal         // output
);
```

### Collision Events via Script

```angelscript
void OnCollisionEnter2D(Entity other) {
    string name = GetEntityName(other);
    Log("Hit: " + name);
}

void OnCollisionExit2D(Entity other) {
    Log("Left: " + GetEntityName(other));
}
```

---

## Tutorial 10: Top-Down 2D Movement

**Goal:** Create a top-down game with 4/8-directional movement.

### Setup

1. Create a "Player" entity with `Sprite2DComponent` (with texture!) and `CharacterController` set to `TopDown2D`.
2. Create wall entities with `MeshComponent`, `MaterialComponent`, and colliders.

### Controller Properties

| Property | Description |
|----------|-------------|
| Move Speed | Movement speed in all directions |
| Smoothing | Movement interpolation (0=instant, 1=very smooth) |
| Diagonal Normalization | Prevent faster diagonal movement |

### 8-Directional Animation

```angelscript
void OnUpdate(float dt) {
    Vec2 input = Vec2(GetAxis("Horizontal"), GetAxis("Vertical"));

    if (input.Length() > 0.1) {
        float angle = Atan2(input.y, input.x);
        // Select animation based on angle
        if (angle > -45 && angle <= 45) PlayAnim("walk_right");
        else if (angle > 45 && angle <= 135) PlayAnim("walk_up");
        else if (angle > -135 && angle <= -45) PlayAnim("walk_down");
        else PlayAnim("walk_left");
    } else {
        PlayAnim("idle");
    }
}
```

---

## Tutorial 11: Third-Person Character Controller

**Goal:** Set up a third-person camera and character in 3D.

### Setup

1. Use the **Third Person** template or create from scratch.
2. Create a "Player" entity with a character model, `CharacterController` set to `ThirdPerson`.
3. Create a "Camera" entity with `CameraComponent` (perspective).

### Key Properties

- **Move Speed** — Ground movement speed
- **Sprint Speed** — Speed when holding Shift
- **Jump Force** — Upward velocity on jump
- **Camera Distance** — How far the camera orbits behind the player
- **Camera Height** — Camera vertical offset
- **Look Sensitivity** — Mouse rotation speed

### Ground Detection

The ControllerSystem uses raycasting to detect ground. With Jolt Physics enabled, this uses the Jolt raycast system for accurate ground checks against complex 3D meshes.

---

## Tutorial 12: First-Person Shooter Basics

**Goal:** Create an FPS controller with mouse look.

### Setup

1. Use the **First Person** template.
2. The FPS controller combines character movement with mouse-look camera rotation.
3. Add weapons as child entities of the camera.

### FPS Controller Properties

| Property | Description |
|----------|-------------|
| Move Speed | Walking speed |
| Sprint Multiplier | Speed boost when sprinting |
| Jump Force | Jump height |
| Mouse Sensitivity | Look sensitivity |
| Head Bob | Enable/disable head bobbing |
| Weapon Sway | Camera-relative weapon movement |
| Dash | Optional dash (Shift/E or gamepad Right Bumper) with speed, duration, and cooldown fields |

### Shooting Mechanics

```angelscript
void OnUpdate(float dt) {
    if (Input_IsKeyPressed(MOUSE_LEFT)) {
        // Raycast from camera center
        Vec3 origin = GetPosition(self);
        Vec3 dir = GetForward(self);

        RaycastHit hit;
        if (Physics_Raycast(origin, dir, 100.0, hit)) {
            // Apply damage
            DamageComponent@ dmg = GetDamageComponent(hit.entity);
            if (dmg !is null) {
                dmg.damage += 10.0;
            }
        }
    }
}
```

---

## Tutorial 13: Terrain Sculpting and Vegetation

**Goal:** Create and sculpt a terrain with grass, shrubs, and trees.

### Creating Terrain

1. Create an entity and add `TerrainComponent`.
2. Set grid size (e.g., 128x128) and world scale.
3. Use the terrain sculpting tools:
   - **Raise/Lower** — Modify height
   - **Smooth** — Average nearby heights
   - **Flatten** — Set to a specific height
   - **Paint** — Apply texture layers

### Procedural Terrain

Use the **Procedural Generation** panel (View > Tools > Procedural) to generate terrain heightmaps:
- **Diamond-Square** — Natural-looking terrain
- **fBm** — Fractional Brownian Motion with ridged multifractal option
- **Hydraulic Erosion** — Realistic water erosion simulation
- **Thermal Erosion** — Talus angle material transport

### Vegetation

Add vegetation renderers as separate entities:
- `GrassRenderer` — Billboarded grass blades with wind animation
- `ShrubRenderer` — Low bushes with custom models
- `TreeRenderer` — Full trees with custom trunk/canopy models

Each renderer scatters instances across the terrain based on density maps and height ranges.

---

## Tutorial 14: Skeletal Animation

**Goal:** Import and play skeletal animations on a character model.

### Importing Animated Models

1. Import a `.glb` or `.fbx` file with **Import Animations** enabled.
2. The importer creates an `AnimatorComponent` with all embedded animations.

### Animator Component

| Property | Description |
|----------|-------------|
| Animation Name | Currently playing animation |
| Speed | Playback speed multiplier |
| Loop | Whether to loop |
| Playing | Playback state |

### Script Control

```angelscript
void OnStart() {
    Animator_Play(self, "idle");
}

void OnUpdate(float dt) {
    if (Input_IsKeyDown(KEY_W)) {
        Animator_Play(self, "run");
        Animator_SetSpeed(self, 1.5);
    } else {
        Animator_Play(self, "idle");
        Animator_SetSpeed(self, 1.0);
    }
}
```

### Animation Blending

Multiple animations can blend together using weights:
```angelscript
// Blend between walk and run based on speed
float speed = GetMoveSpeed();
float blend = Clamp(speed / maxSpeed, 0, 1);
Animator_SetBlendWeight(self, "walk", 1.0 - blend);
Animator_SetBlendWeight(self, "run", blend);
```

---

## Tutorial 15: Physics and Collision

**Goal:** Set up 3D physics with Jolt Physics.

### Physics Backends

Enjin supports three physics backends:
- **Jolt Physics** (default for 3D) — Production-grade, multi-threaded
- **Box2D** (default for 2D) — Industry-standard 2D physics
- **SimplePhysics** — Built-in lightweight fallback

### Adding 3D Colliders

| Component | Shape | Use Case |
|-----------|-------|----------|
| `BoxColliderComponent` | Axis-aligned box | Crates, walls, floors |
| `SphereColliderComponent` | Sphere | Balls, projectiles |
| `CapsuleColliderComponent` | Capsule | Characters |

### Rigidbody Properties

| Property | Description |
|----------|-------------|
| Mass | Object mass in kg |
| Linear Damping | Velocity decay |
| Angular Damping | Rotation decay |
| Is Kinematic | Script-controlled (no gravity) |
| Use Gravity | Affected by gravity |
| CCD | Continuous collision detection for fast objects |

### Joint Types (3D)

| Joint | Description |
|-------|-------------|
| `HingeJointComponent` | Single-axis rotation (doors) |
| `DistanceJointComponent` | Maintain distance (chains) |
| `SpringJointComponent` | Spring connection |
| `BallSocketJointComponent` | Free rotation (ragdoll limbs) |
| `SliderJointComponent` | Linear slide (pistons) |
| `FixedJointComponent` | Rigid connection |

### Collision Filtering

Use **categoryBits** and **collisionMask** for layer-based filtering:
```
Player:     categoryBits = 0x01, collisionMask = 0xFF
Enemy:      categoryBits = 0x02, collisionMask = 0xFF
Projectile: categoryBits = 0x04, collisionMask = 0x02  // Only hits enemies
Pickup:     categoryBits = 0x08, collisionMask = 0x01  // Only player can pick up
```

Name your collision groups in **View > Settings > Project Settings > Collision Groups**.

---

## Tutorial 16: AngelScript Basics

**Goal:** Write your first gameplay script.

### Script Component

1. Select an entity.
2. Add a `ScriptComponent`.
3. Set the **Script Path** to a `.as` file (e.g., `scripts/player.as`).

You can also click **Add Script** in the Inspector to create a new script. The New Script dialog offers four starter templates (Empty, Rotator, Interactable, Spawner) and creates the file in the project's `scripts/` folder.

The engine API scripts (`TegeBehavior.as`, `Timer.as`, `Tween.as`, `Math.as`, `StateMachine.as`) are embedded in the engine, so `#include "Timer.as"` resolves automatically with no `enjin_api` folder on disk. A project-local `scripts/enjin_api/` copy overrides the embedded versions, but it is optional.

### Script Lifecycle

```angelscript
// Called once when play mode starts
void OnStart() {
    Log("Hello from " + GetEntityName(self));
}

// Called every frame
void OnUpdate(float dt) {
    // dt is delta time in seconds
}

// Called at fixed intervals (physics rate)
void OnFixedUpdate(float dt) {
    // Physics-related updates here
}

// Called after all OnUpdate calls
void OnLateUpdate(float dt) {
    // Camera follow, post-processing, etc.
}
```

### Common API Functions

```angelscript
// Entity
Entity self;                        // Current entity (automatic)
string GetEntityName(Entity e);
Entity FindEntity(string name);     // Find by name (O(1) cached)

// Transform
Vec3 GetPosition(Entity e);
void SetPosition(Entity e, Vec3 pos);
Vec3 GetScale(Entity e);
void SetScale(Entity e, Vec3 scale);
Quat GetRotation(Entity e);
void SetRotation(Entity e, Quat rot);

// Input
bool Input_IsKeyPressed(int key);   // Just pressed this frame
bool Input_IsKeyDown(int key);      // Currently held
Vec2 Input_GetMousePosition();
float GetAxis(string name);         // "Horizontal", "Vertical"

// Math
float Sin(float x);
float Cos(float x);
float Lerp(float a, float b, float t);
Vec3 Vec3_Lerp(Vec3 a, Vec3 b, float t);

// Physics
bool Physics_Raycast(Vec3 origin, Vec3 dir, float dist, RaycastHit& hit);
void Physics_Teleport(uint64 entity, Vec3 pos); // Move a dynamic body and zero its velocities (respawns/resets)
```

### Hot-Reload

Scripts automatically hot-reload when you save them during play mode. The engine detects file changes and recompiles without restarting.

If a script fails to compile when you press Play, an error toast shows the compiler message and the Console panel opens automatically.

---

## Tutorial 17: Visual Scripting with Blueprints

**Goal:** Create gameplay logic without writing code.

### Opening the Visual Script Editor

1. Add a `VisualScriptComponent` to an entity.
2. Open **View > Tools > Visual Script** panel.
3. The node graph editor appears.

### Node Types (126+)

Nodes are organized by category:

| Category | Examples |
|----------|---------|
| **Events** | OnStart, OnUpdate, OnCollisionEnter |
| **Flow** | Branch, ForLoop, Sequence, Delay |
| **Math** | Add, Multiply, Lerp, Clamp, Sin, Random |
| **Transform** | GetPosition, SetPosition, Translate, LookAt |
| **Input** | IsKeyPressed, IsKeyDown, GetMousePosition, GetAxis |
| **Physics** | Raycast, ApplyForce, SetVelocity |
| **Scene** | LoadScene, FindEntity, SpawnEntity, DestroyEntity |
| **Audio** | PlaySound, StopSound, SetVolume |
| **Gameplay** | SaveToSlot, LoadFromSlot, QuestStart, QuestComplete |
| **AI** | SetAIState, GetBlackboardFloat, NavMeshFindPath |
| **Tween** | TweenPosition, TweenScale, TweenFloat |
| **Noise** | PerlinNoise2D, SimplexNoise3D, FractalNoise |

### Creating a Script

1. Right-click in the editor to open the node palette.
2. Search for "OnUpdate" and place it.
3. Add an "IsKeyPressed" node, connect the flow output of OnUpdate to its input.
4. Set the key to `KEY_SPACE`.
5. Add a "Branch" node connected to the IsKeyPressed output.
6. On the "True" branch, add "ApplyForce" with upward force.

### Debugger

- Click the **Debug** button to enable breakpoints.
- Click on any node connection to add a breakpoint (red dot).
- During play mode, execution pauses at breakpoints.
- Use **Step** to advance one node at a time.
- The **Execution Timeline** shows node execution order and timing.

---

## Tutorial 18: Coroutines and Timing

**Goal:** Use coroutines for timed sequences in scripts.

### What are Coroutines?

Coroutines let you write sequential logic that spans multiple frames:

```angelscript
void OnStart() {
    StartCoroutine("SpawnWave");
}

void SpawnWave() {
    for (int i = 0; i < 5; i++) {
        SpawnEnemy();
        WaitSeconds(0.5);  // Pause for 0.5 seconds
    }
    WaitSeconds(3.0);      // Wait 3 seconds
    StartCoroutine("SpawnWave");  // Restart
}

void SpawnEnemy() {
    // ... spawn logic
}
```

### Coroutine Functions

| Function | Description |
|----------|-------------|
| `StartCoroutine(name)` | Begin a coroutine |
| `StopCoroutine(name)` | Cancel a running coroutine |
| `StopAllCoroutines()` | Cancel all coroutines on this entity |
| `WaitSeconds(float)` | Pause for N seconds |
| `WaitFrames(int)` | Pause for N frames |

### Practical Example: Screen Shake

```angelscript
void ShakeCamera(float duration, float intensity) {
    float elapsed = 0;
    Vec3 originalPos = GetPosition(camera);

    while (elapsed < duration) {
        float x = RandomRange(-intensity, intensity);
        float y = RandomRange(-intensity, intensity);
        SetPosition(camera, originalPos + Vec3(x, y, 0));
        WaitFrames(1);
        elapsed += GetDeltaTime();
    }
    SetPosition(camera, originalPos);
}
```

---

## Tutorial 19: Event System and Communication

**Goal:** Decouple entity communication using events.

### Event Bus

The event system lets entities communicate without direct references:

```angelscript
// Entity A: Publish an event
void OnDeath() {
    Event_Fire("enemy_killed", "goblin");
}

// Entity B: Subscribe and react
void OnStart() {
    Event_Subscribe("enemy_killed", "OnEnemyKilled");
}

void OnEnemyKilled(string data) {
    int score = GetScore();
    SetScore(score + 100);
    Log("Enemy killed: " + data);
}
```

### Common Event Patterns

- **Damage System** — `"entity_damaged"` with entity ID and amount
- **Pickup Collection** — `"item_collected"` with item type
- **Level Complete** — `"level_complete"` triggers scene transition
- **UI Updates** — `"health_changed"` updates HUD display

---

## Tutorial 20: State Machines

**Goal:** Manage entity behavior states cleanly.

### State Machine Pattern

Enjin supports state machines with script callbacks:

```angelscript
void OnStart() {
    SM_AddState(self, "idle");
    SM_AddState(self, "patrol");
    SM_AddState(self, "chase");
    SM_AddState(self, "attack");

    SM_SetState(self, "idle");
}

void OnUpdate(float dt) {
    string state = SM_GetState(self);

    if (state == "idle") UpdateIdle(dt);
    else if (state == "patrol") UpdatePatrol(dt);
    else if (state == "chase") UpdateChase(dt);
    else if (state == "attack") UpdateAttack(dt);
}

void UpdateIdle(float dt) {
    if (DetectPlayer()) {
        SM_SetState(self, "chase");
    }
}
```

### Visual Script State Machine

In the Visual Script editor, use `StateMachine_SetState` and `StateMachine_GetState` nodes to manage states visually.

---

## Tutorial 21: Save System and Persistence

**Goal:** Save and load game progress.

### Save System Overview

Enjin uses a 3-tier persistence model:

| Tier | Scope | Example |
|------|-------|---------|
| **SceneState** | Current scene only | Door open/closed, chest looted |
| **RunState** | Current playthrough | Player health, inventory, quest progress |
| **MetaProgression** | Persists across runs | Unlocked characters, achievements, high scores |

### Setup

1. Add `SaveDataComponent` to entities that need persistence.
2. Set the **Persistence Tier** in the Inspector.
3. Add **Tags** and **Key-Value Data** for custom state.

### Saving and Loading via Script

```angelscript
// Save to slot 1
SaveGame_ToSlot(1);

// Load from slot 1
SaveGame_FromSlot(1);

// Delete a save
SaveGame_DeleteSlot(1);

// Create a checkpoint (auto-saves with scene state)
SaveGame_Checkpoint();

// Meta-progression (persists across saves)
Meta_SetFloat("high_score", 9999);
float best = Meta_GetFloat("high_score");
Meta_SetString("last_character", "warrior");
```

### Auto-Save

The system has 3 auto-save slots that cycle automatically. Configure auto-save interval and trigger conditions.

### In-Game Save/Load Menu

Add `SaveLoadMenuComponent` to an entity for a built-in save/load grid overlay:
- **Mode** — Save or Load
- **Columns** — Grid layout
- Shows slot previews with timestamps

---

## Tutorial 22: Dialogue Trees and Narrative

**Goal:** Create branching dialogue with the visual editor.

### Creating a Dialogue

1. Create an entity and add `DialogueComponent`.
2. Open the **Dialogue Editor** (View > Tools > Dialogue Editor).
3. Build your dialogue tree with nodes:

### Dialogue Node Types

| Type | Description |
|------|-------------|
| **Say** | NPC speaks a line of dialogue |
| **Choice** | Player chooses from options |
| **Condition** | Branch based on game state |
| **SetVariable** | Set a dialogue variable |
| **Event** | Fire a game event |
| **Jump** | Jump to another node |
| **End** | End the conversation |

### Dialogue Variables

Store state within a dialogue (e.g., has the player already asked about a topic):

```
[Set] askedAboutQuest = true
[Condition] askedAboutQuest == true → "You already know about the quest."
```

### Dialogue Box Component

Add `DialogueBoxComponent` for automatic UI rendering:
- Speaker name label
- Text display with typewriter effect
- Portrait image
- Choice buttons (up to 6)
- Continue indicator

### Localization

Dialogue text supports localization via string keys:
```angelscript
string greeting = Loc_Get("dialogue.innkeeper.greeting");
Loc_SetLocale("fr");  // Switch to French
```

---

## Tutorial 23: Quest and Objective System

**Goal:** Create quests with objectives and rewards.

### Quest Setup

Open the **Quest Flow Editor** (View > Tools > Quest Flow).

### Quest Structure

```
Quest: "The Lost Artifact"
├── Objective 1: "Talk to the Elder" (type: interact)
├── Objective 2: "Find 3 Crystal Shards" (type: collect, count: 3)
├── Objective 3: "Defeat the Guardian" (type: kill)
└── Reward: 500 gold, "Ancient Sword" item
```

### Script Integration

```angelscript
// Start a quest
Quest_Start("lost_artifact");

// Update progress
Quest_CompleteObjective("lost_artifact", "find_shards");

// Check status
if (Quest_IsActive("lost_artifact")) {
    // Show quest marker
}

// Complete quest
Quest_Complete("lost_artifact");
```

### Visual Script Integration

Use the Gameplay category nodes:
- `QuestStart`, `QuestComplete`, `QuestFail`
- `QuestIsActive`, `QuestIsCompleted`

---

## Tutorial 24: Inventory with Pickups

**Goal:** Create collectible items the player can pick up.

### Pickup Component

Add `PickupComponent` to items:

| Property | Type | Description |
|----------|------|-------------|
| `.type` | Enum | Health, Ammo, Coin, Key, PowerUp, Custom |
| `.customId` | String | Custom item identifier |
| `.value` | Float | Amount (e.g., 25 health, 50 coins) |

### Collection Script

```angelscript
void OnCollisionEnter(Entity other) {
    PickupComponent@ pickup = GetPickupComponent(other);
    if (pickup !is null) {
        if (pickup.type == PickupType::Health) {
            Heal(pickup.value);
        } else if (pickup.type == PickupType::Coin) {
            AddCoins(int(pickup.value));
        }
        DestroyEntity(other);
    }
}
```

### Object Pooling

For frequently spawned/despawned items, use the Object Pool:

```angelscript
// Acquire from pool (reuses existing inactive entity)
Entity coin = ObjectPool_Acquire("coin_pool");
SetPosition(coin, spawnPos);

// Release back to pool (deactivates but doesn't destroy)
ObjectPool_Release("coin_pool", coin);
```

---

## Tutorial 25: HUD and UI Canvas

**Goal:** Create an in-game heads-up display.

### UI Canvas Setup

1. Create an entity and add `UICanvasComponent`.
2. Set **Design Resolution** (e.g., 1920x1080).
3. Set **Scale Mode** — `ScaleWithScreen` for responsive layout.

### Adding UI Elements

```angelscript
void OnStart() {
    // Create elements programmatically
    UICanvas@ canvas = GetUICanvas(self);

    int panel = canvas.AddElement(UIWidgetType::Panel, "HealthPanel", 0);
    int label = canvas.AddElement(UIWidgetType::Label, "HealthLabel", panel);
    int bar = canvas.AddElement(UIWidgetType::ProgressBar, "HealthBar", panel);

    // Configure the health bar
    UIElement@ barEl = canvas.GetElement(bar);
    barEl.anchor = UIAnchor::TopLeft;
    barEl.position = Vec2(20, 20);
    barEl.size = Vec2(200, 30);
}
```

### Widget Types

| Type | Description |
|------|-------------|
| Panel | Container for grouping |
| Button | Clickable button |
| Label | Text display |
| Image | Texture display |
| ProgressBar | Fill bar (0-1) |
| Slider | Adjustable value |
| Checkbox | On/off toggle |
| Toggle | Switch toggle |

### Updating the HUD

```angelscript
void OnUpdate(float dt) {
    // Update health bar
    float healthPercent = currentHealth / maxHealth;
    UICanvas_SetProgress(self, "HealthBar", healthPercent);
    UICanvas_SetText(self, "HealthLabel", "HP: " + int(currentHealth));
}
```

### 9-Slice Sprites

For scalable UI backgrounds, use `NineSliceConfig`:
- Set `texturePath` to a panel texture
- Define `borderLeft/Right/Top/Bottom` (texels) to mark stretch zones
- The center stretches, corners stay fixed, edges tile

### Focus Navigation

UI Canvas supports keyboard/gamepad navigation:
- **Tab/Shift+Tab** — Cycle through focusable elements
- **Arrow Keys/DPad** — Directional navigation
- **Enter/Space/Gamepad-A** — Activate focused element
- **Left/Right** — Adjust sliders

Set `accessibleLabel` on each UIElement for screen reader support.

---

## Tutorial 26: AI Behaviors: Patrol, Chase, Flee

**Goal:** Create enemies with basic AI behaviors.

### AI Component Setup

1. Add `AIComponent` to an enemy entity.
2. Set the behavior type:
   - **Patrol** — Walk between waypoints
   - **Chase** — Follow the player when in range
   - **Flee** — Run away when health is low
   - **Wander** — Random movement within an area
   - **NavMesh** — Pathfind on navigation mesh

### Patrol Setup

Set patrol waypoints as child entities of the AI entity:
1. Create child entities named "Waypoint1", "Waypoint2", etc.
2. Position them along the patrol path.
3. Set **Patrol Speed** and **Wait Time** at each waypoint.

### Chase Behavior

```angelscript
void OnUpdate(float dt) {
    Entity player = FindEntity("Player");
    float dist = Distance(GetPosition(self), GetPosition(player));

    if (dist < detectionRange) {
        AI_SetState(self, "chase");
    } else {
        AI_SetState(self, "patrol");
    }
}
```

### Script Bindings

```angelscript
AI_SetState(entity, "patrol");     // Set behavior
AI_SetTarget(entity, player);       // Set chase target
AI_SetSpeed(entity, 5.0);           // Set movement speed
AI_SetDetectionRange(entity, 10.0); // Set detection radius
string state = AI_GetState(entity); // Query current state
```

---

## Tutorial 27: Behavior Trees

**Goal:** Create complex AI decision-making with behavior trees.

### Opening the BT Editor

1. Add a `BehaviorTreeComponent` to an entity.
2. Open **View > Tools > Behavior Tree** panel.
3. Build the tree top-down from the root.

### Node Types (20)

| Category | Nodes |
|----------|-------|
| **Composite** | Selector (OR), Sequence (AND), Parallel |
| **Decorator** | Inverter, Repeater, Succeeder, Failer, Cooldown |
| **Leaf** | MoveToTarget, Attack, Patrol, Wait, PlayAnimation |
| **Condition** | IsInRange, HasTarget, IsHealthLow, IsAlerted |
| **Action** | SetBlackboard, Log, FireEvent, SpawnEntity |

### Example: Guard AI

```
Root (Selector)
├── Sequence: "Combat"
│   ├── Condition: IsInRange(player, 15)
│   ├── Sequence: "Engage"
│   │   ├── Action: SetTarget(player)
│   │   ├── Action: MoveToTarget(speed: 6)
│   │   ├── Condition: IsInRange(player, 2)
│   │   └── Action: Attack
├── Sequence: "Patrol"
│   ├── Action: Patrol(waypoints)
│   └── Decorator: Repeater(infinite)
└── Action: Idle
```

### Blackboard

The blackboard is shared state for the behavior tree:

```angelscript
BT_SetBlackboardFloat(self, "alertLevel", 0.8);
float alert = BT_GetBlackboardFloat(self, "alertLevel");
BT_SetBlackboardBool(self, "hasSeenPlayer", true);
```

### Play-Mode Visualization

During play mode, the BT editor highlights the currently executing path in green, making it easy to debug AI decision-making.

---

## Tutorial 28: NavMesh Pathfinding

**Goal:** Use navigation meshes for intelligent pathfinding.

### NavMesh Setup

The navigation mesh is automatically generated from the scene's static geometry (entities with colliders marked as static).

### Pathfinding in Scripts

```angelscript
// Find a path from current position to target
Vec3 target = GetPosition(FindEntity("Destination"));
array<Vec3> path = NavMesh_FindPath(GetPosition(self), target);

// Follow the path
if (path.length() > 0) {
    MoveToward(self, path[0], speed * dt);
    if (Distance(GetPosition(self), path[0]) < 0.5) {
        path.removeAt(0);
    }
}
```

### NavMesh Properties

- **Cell Size** — Resolution of the nav mesh grid
- **Agent Radius** — Minimum clearance from walls
- **Agent Height** — Minimum ceiling clearance
- **Max Slope** — Maximum walkable incline angle
- **Step Height** — Maximum step-up height

---

## Tutorial 29: Particle Systems

**Goal:** Create fire, smoke, and magic spell effects.

### Particle Emitter Setup

1. Add `ParticleEmitterComponent` to an entity.
2. Configure emitter properties in the Inspector.

### Key Properties

| Property | Description |
|----------|-------------|
| Shape | Box, Sphere, Cone, Circle, Point |
| Max Particles | Maximum alive particles (up to 16384) |
| Emission Rate | Particles per second |
| Start Lifetime | How long particles live |
| Start Speed | Initial velocity |
| Start Size | Initial particle size |
| Start Color | RGB color |
| Gravity | Gravity vector applied to particles |
| Drag | Air resistance |

### Presets (12)

| Preset | Description |
|--------|-------------|
| Fire | Warm upward-rising flame |
| Smoke | Gray billowing smoke |
| Sparks | Fast small bright sparks |
| Rain | Downward streaks |
| Snow | Slow drifting flakes |
| Explosion | Burst outward |
| Magic | Colorful orbiting particles |
| Fireflies | Slow wandering points |
| Waterfall | Downward-flowing water drops |
| Steam | Rising translucent wisps |
| Blood | Red splash particles |
| LiquidDrip | Slow dripping drops |

### Script Control

```angelscript
Particle_Play(self);
Particle_Stop(self);
Particle_Burst(self, 50);           // Emit 50 particles instantly
Particle_SetEmissionRate(self, 100); // Change rate
Particle_SetColor(self, Vec3(1, 0.5, 0)); // Orange
Particle_SetGravity(self, Vec3(0, -5, 0));
```

### Particle Editor

Open the **Particle Editor** panel for interactive editing with:
- Real-time preview
- Color gradient editor
- Size/speed curves over lifetime
- Playback controls (play/pause/restart)

---

## Tutorial 30: Weather and Atmosphere

**Goal:** Add dynamic weather to your game.

### Weather System

1. The weather system runs automatically during play mode.
2. Configure via scripts or the **Project Settings > Environment** panel.

### Weather Types

| Type | Effects |
|------|---------|
| Clear | No precipitation, bright sky |
| Rain | Rain particles, wet surfaces, darker sky |
| Snow | Snow particles, accumulation |
| Fog | Distance-based fog |
| Storm | Rain + lightning + wind |

### Script Control

```angelscript
// Set weather type
Weather_SetType("rain");

// Adjust intensity
Weather_SetRainIntensity(0.8);
Weather_SetSnowIntensity(0.5);

// Fog
Weather_SetFogDensity(0.02);
Weather_SetFogColor(Vec3(0.5, 0.5, 0.6));

// Wind
Weather_SetWindDirection(Vec3(1, 0, 0.5));
Weather_SetWindStrength(2.0);

// Lightning
Weather_TriggerLightning();
```

### World Time and Seasons

The `WorldTimeComponent` provides a day/night cycle:
- **Time Scale** — Speed of time progression
- **Hour** — Current hour (0-24)
- **Season** — Spring, Summer, Autumn, Winter (affects weather probabilities and vegetation colors)

---

## Tutorial 31: Post-Processing and Retro Effects

**Goal:** Apply visual polish with post-processing.

### Post-Processing Stack

Open **View > Rendering > Post Processing** to configure:

| Effect | Description |
|--------|-------------|
| Bloom | Bright areas glow and bleed light |
| Vignette | Darken screen edges |
| FXAA | Fast anti-aliasing |
| Film Grain | Noise overlay for cinematic look |
| Color Grading | Adjust brightness, contrast, saturation |
| Depth of Field | Blur distant/near objects |
| Tilt-Shift | Miniature effect with blur bands |

### Retro Effects

Open **View > Rendering > Retro Effects** for classic aesthetics:

| Effect | Description |
|--------|-------------|
| CRT Scanlines | Horizontal/vertical scanline overlay |
| Pixelation | Reduce resolution for pixel-art look |
| Dithering | Floyd-Steinberg or ordered dithering |
| Color Quantization | Reduce color palette (4/8/16/32 colors) |
| Stipple/Dither | 8 combinable full-screen dither patterns |

### Combining Effects

Effects stack in order. A PS1-style look might use:
- Pixelation (320x240)
- Color quantization (32 colors)
- Vertex snapping (via material `ditherGradient` flag)
- CRT scanlines

---

## Tutorial 32: Water and Fluid Simulation

**Goal:** Add water planes and fluid simulations.

### 3D Water

Add `Water3DComponent` for a water plane with:
- Gerstner wave simulation
- Configurable wave height, speed, and direction
- Reflection and refraction (approximate)

### Fluid Simulation

The `FluidSimulation` system provides grid-based Navier-Stokes fluid dynamics:
- Density and velocity fields
- Real-time advection, diffusion, and pressure projection
- Visualized as a 2D/3D density field

### Fluid-Terrain Coupling

When both a `FluidSimulation` and `TerrainComponent` exist, enable `FluidTerrainCoupling` for:
- **Erosion Mode** — Fluid wears away terrain (rivers carving valleys)
- **Accumulate Mode** — Fluid deposits material (lava building formations)
- Bidirectional: terrain slope drives fluid flow direction

---

## Tutorial 33: Ray Tracing

**Goal:** Enable hardware ray tracing for realistic lighting.

### Requirements

- Vulkan-capable GPU with ray tracing extensions (NVIDIA RTX, AMD RX 6000+)
- The RT pipeline activates automatically when compiled SPIR-V shaders are available

### Enabling RT

1. Open **View > Rendering > Rendering** panel.
2. Under **Ray Tracing**, check "Supported" to verify GPU capability.
3. Enable **Ray Tracing** toggle.
4. Select **Mode**: Hybrid (raster + RT effects) or Path Trace (full path tracing).

### RT Effects

| Effect | Output | Description |
|--------|--------|-------------|
| RT Shadows | R16F | Soft shadows with penumbra |
| RT Reflections | RGBA16F | Accurate mirror/glossy reflections |
| RT Ambient Occlusion | R16F | Screen-space contact shadows |
| RT Global Illumination | RGBA16F | Bounced indirect lighting |
| Path Tracer | RGBA16F | Progressive accumulation (offline quality) |

### SVGF Denoiser

The SVGF (Spatiotemporal Variance-Guided Filtering) denoiser smooths noisy RT output:
- Temporal accumulation (blends with previous frames)
- Variance estimation (3x3 box)
- A-trous wavelet filtering (5 iterations)

---

## Tutorial 34: Sound Effects and Music

**Goal:** Add audio to your game.

### Audio Source Setup

1. Add `AudioSourceComponent` to an entity.
2. Set the **Audio File** path (.wav, .mp3, .ogg, .flac).
3. Configure properties:
   - **Volume** — 0 to 1
   - **Pitch** — Playback speed
   - **Loop** — Repeat playback
   - **is3D** — Enable 3D spatialization (falloff based on distance)
   - **Min/Max Distance** — 3D audio range

### Script Control

```angelscript
Audio_Play(self);
Audio_Stop(self);
Audio_SetVolume(self, 0.5);
Audio_SetPitch(self, 1.2);
```

### Background Music

Create a dedicated entity with a non-3D AudioSource for background music. Set `is3D = false` so it plays at full volume regardless of camera position.

---

## Tutorial 35: Audio Event Graphs

**Goal:** Create complex audio behaviors with node graphs.

### Opening the Audio Graph

Go to **View > Tools > Audio Event Graph**. The node-based editor lets you create audio logic.

### Key Concepts

- **Events** — Named triggers (e.g., "footstep", "explosion")
- **Parameters** — Runtime-adjustable values (e.g., "danger_level")
- **Thresholds** — Parameter values that trigger different audio paths

### Script Integration

```angelscript
AudioGraph_TriggerEvent("footstep");
AudioGraph_SetParameter("danger_level", 0.8);
float value = AudioGraph_GetParameter("danger_level");
AudioGraph_StopAll();
```

### File Format

Audio event packages save as `.enjaudiopkg` files.

---

## Tutorial 36: Dungeon Generation

**Goal:** Procedurally generate dungeon layouts.

### Using the Procedural Panel

1. Open **View > Tools > Procedural Generation**.
2. Select an algorithm.
3. Configure parameters.
4. Click **Generate** to preview.
5. Click **Apply** to create entities from the result.

### Algorithms for Dungeons

**Cellular Automata** — Organic caves:
```
Width: 64, Height: 64
Fill Percent: 45%
Birth Limit: 4, Death Limit: 3
Iterations: 5
```

**BSP (Binary Space Partition)** — Rectangular rooms:
```
Width: 64, Height: 64
Min Room: 5, Max Room: 15
Split Depth: 5
Corridor Width: 2
```

**Random Walker** — Carved corridors:
```
Width: 64, Height: 64
Steps: 2000
Turn Chance: 0.5
```

**Prefab Assembler** — Snap-together rooms:
- Define room prefabs with connection points
- Algorithm places rooms that connect at matching ports

---

## Tutorial 37: Terrain Heightmaps with fBm

**Goal:** Generate realistic terrain using noise functions.

### fBm (Fractional Brownian Motion)

Standard fBm stacks multiple octaves of Perlin noise:

```
Octaves: 6 (more = more detail)
Lacunarity: 2.0 (frequency multiplier per octave)
Gain: 0.5 (amplitude multiplier per octave, aka persistence)
Frequency: 1.0 (base frequency)
```

### Ridged Multifractal

For mountain ridges, use the Ridged Multifractal mode:
- Takes absolute value of noise, then inverts
- Produces sharp ridge lines
- `ridgedPower` controls sharpness (higher = sharper)

### Erosion

After generating a heightmap, apply erosion for realism:

**Hydraulic Erosion** — Simulates rain droplets:
```
Iterations: 50000 (number of droplets)
Sediment Capacity: 4.0
Erosion Rate: 0.3
Evaporate Rate: 0.01
```

**Thermal Erosion** — Material slides downhill:
```
Iterations: 50
Talus Angle: 0.8
Erosion Rate: 0.5
```

---

## Tutorial 38: L-Systems for Plants

**Goal:** Generate procedural vegetation using L-systems.

### 2D L-System (Trees on a Plane)

Classic string-rewriting turtle graphics:

```
Axiom: "F"
Rules: F → "F[+F]F[-F]F"
Iterations: 4
Angle: 25.7°
```

Commands: `F` = draw forward, `+` = turn left, `-` = turn right, `[` = push state, `]` = pop state.

### 3D L-System (Full Trees)

The 3D L-system adds pitch and roll to the turtle:

```
Commands:
F = Move forward + draw
f = Move forward without drawing
+ = Yaw left,  - = Yaw right
^ = Pitch up,  & = Pitch down
/ = Roll left,  \ = Roll right
[ = Push state, ] = Pop state
```

### Stochastic Rules

Add variation with weighted random rules:
```
F → "F[+F][-F]" (weight: 0.6)
F → "F[+F]"     (weight: 0.3)
F → "FF"         (weight: 0.1)
```

### Branch Radius

Branches thin naturally via `radiusDecay` (0.7 = 30% thinner per branch level).

---

## Tutorial 39: Wave Function Collapse

**Goal:** Generate tile-based worlds with constraint propagation.

### WFC Basics

Wave Function Collapse generates grids where each tile respects adjacency constraints.

### Setup

1. Define your tileset with allowed neighbors:
```
Tile 0 (Grass): can have [0,1] north, [0,1] south, [0,1] east, [0,1] west
Tile 1 (Path):  can have [0,1] north, [0,1] south, [0,1] east, [0,1] west
Tile 2 (Water): can have [2,3] north, [2,3] south, [2,3] east, [2,3] west
Tile 3 (Shore): can have [0,2,3] all directions
```

2. Set grid size and run generation.
3. The algorithm collapses cells one at a time, propagating constraints.

### Backtracking

If the algorithm reaches a contradiction (a cell with no valid tiles), it backtracks. Set `maxBacktracks` to control how many retries before declaring failure.

---

## Tutorial 40: Reaction-Diffusion Patterns

**Goal:** Generate Turing patterns using Gray-Scott reaction-diffusion.

### What is Reaction-Diffusion?

Two chemicals (U and V) diffuse and react on a 2D grid:
- U is consumed to produce V
- V decays naturally
- The interplay creates complex patterns

### Presets

| Preset | Feed Rate | Kill Rate | Pattern |
|--------|-----------|-----------|---------|
| Mitosis Spots | 0.0367 | 0.0649 | Cell-like splitting dots |
| Coral Growth | 0.0545 | 0.062 | Branching coral structures |
| Fingerprints | 0.0545 | 0.062 | Ridged fingerprint lines |
| Leopard | 0.026 | 0.051 | Leopard spot patterns |
| Labyrinth | 0.029 | 0.057 | Maze-like connected paths |
| Worm Holes | 0.039 | 0.058 | Isolated holes |
| Bubble Packing | 0.012 | 0.047 | Tightly packed circles |
| Spirals | 0.014 | 0.045 | Rotating spirals |

### Usage

1. Configure grid size and preset.
2. Run the simulation (auto-steps per frame).
3. **Bake to Texture** — Export the pattern as an RGBA8 texture for use on meshes.
4. **Bake to Heightmap** — Use as terrain displacement or normal map source.

### Scripting

```angelscript
// Create and step the simulation
RD_Initialize("leopard", 256, 256);
RD_Step(10);  // Run 10 steps

// Seed additional spots
RD_SeedCircle(128, 128, 10);

// Export
RD_BakeToTexture("patterns/leopard.png");
```

---

## Tutorial 41: Cellular Automata as Geometry

**Goal:** Turn cellular automata grids into 3D geometry.

### Overview

Cellular automata rules (Game of Life, Brian's Brain, etc.) generate evolving 2D/3D grids. This system converts live cells into visible geometry in real-time.

### Rules Available

| Rule | Type | Description |
|------|------|-------------|
| Game of Life | B3/S23 | Classic Conway |
| HighLife | B36/S23 | Replicating patterns |
| Day and Night | B3678/S34678 | Symmetric |
| Seeds | B2/S | Explosive growth |
| Brian's Brain | 3-state | On → Dying → Dead |
| Rule 110 | 1D | Turing-complete elementary automaton |
| Diamoeba | B35678/S5678 | Diamond amoebas |

### Mesh Generation Modes

| Mode | Description | Performance |
|------|-------------|-------------|
| **Voxels** | Each live cell = cube (internal faces culled) | Fast for sparse grids |
| **Marching Cubes** | Smooth isosurface | Better visuals, slower |
| **Point Cloud** | Each cell = small sphere | Fastest |

### Patterns

Stamp classic patterns:
- **Glider** — Moves diagonally
- **Pulsar** — Period-3 oscillator
- **Gosper Glider Gun** — Continuously produces gliders

---

## Tutorial 42: Physarum Slime Mold Networks

**Goal:** Simulate organic network formation with Physarum agents.

### How It Works

Thousands of simple agents follow chemical trails:
1. Each agent senses the trail map ahead (3 sensors)
2. Turns toward the strongest trail signal
3. Deposits trail chemical at its new position
4. Trail evaporates and diffuses over time

The result: organic, self-organizing network structures.

### Presets

| Preset | Description |
|--------|-------------|
| Classic Slime | Standard Physarum behavior |
| Branching Network | More branching, less merging |
| Dense Web | Thick interconnected mesh |
| Tendrils | Thin extending filaments |
| Pulsating | Oscillating density |

### Key Parameters

| Parameter | Effect |
|-----------|--------|
| Agent Count | More agents = denser networks (50K typical) |
| Sensor Angle | Wider = more branching |
| Sensor Distance | Farther = smoother paths |
| Turn Speed | Higher = more responsive agents |
| Trail Decay | Higher = shorter-lived trails (more dynamic) |
| Trail Deposit | Higher = stronger trails (more persistent) |

### Food Sources

Place "food" locations that attract agents, creating networks that connect food sources (similar to how real slime mold finds optimal paths between food).

---

## Tutorial 43: Timeline Sequencer

**Goal:** Create keyframed animations using the timeline editor.

### Opening the Timeline

The **Timeline Editor** is a Flash-style keyframe animation tool. Open via **View > Tools > Timeline**.

### Concepts

- **Layers** — Group tracks (like Flash layers)
- **Tracks** — Animate a specific property (position, rotation, scale, opacity, etc.)
- **Keyframes** — Value snapshots at specific times
- **Interpolation** — How values blend between keyframes (Constant, Linear, Bezier, Catmull-Rom)

### Workflow

1. Select an entity.
2. Add tracks for the properties you want to animate (e.g., "transform.position").
3. Move the playhead to a time.
4. Set the property value (position, rotation, etc.) in the Inspector.
5. Click **Auto-Key** to record a keyframe at the current time.
6. Move the playhead and set a new value. Repeat.

### Curve Editor

Toggle the **Curve** view to see and edit Bezier curves:
- Drag keyframes to adjust timing
- Pull tangent handles for smooth curves
- Break tangents for sharp transitions

### Dopesheet View

The default **Dopesheet** view shows keyframes as diamonds on a timeline, similar to Flash's frame view. Useful for timing adjustments.

### Onion Skinning

Enable **Onion Skin** to see ghost frames before and after the current time, helping you visualize motion.

---

## Tutorial 44: Cinematic Cameras

**Goal:** Create camera sequences for cutscenes.

### Cinematic System

```angelscript
// Play a named cinematic sequence
Cinematic_Play("intro_cutscene");

// Stop playback
Cinematic_Stop("intro_cutscene");
```

### Camera Tracks

Use the Timeline to animate camera properties:
- Position (camera dolly/crane moves)
- Rotation (pan, tilt)
- FOV (zoom effects)
- Focus distance (rack focus)

### Camera Presets

9 built-in camera presets for quick setup:
- **Side Scroller** — Fixed horizontal view
- **Top Down** — Overhead view
- **Isometric** — 45-degree angled view
- **Over Shoulder** — Behind character, offset
- **Fixed Angle** — Security camera style
- **Cinematic** — Smooth follow with offset
- **Orbit** — Orbiting around target
- **Security Cam** — Fixed with panning
- **First Person** — Locked to character head

---

## Tutorial 45: Tweening and Easing

**Goal:** Smoothly animate values over time.

### Tween Functions

```angelscript
// Move entity to target over 2 seconds with ease-out
Tween_Position(self, targetPos, 2.0, EaseOutCubic);

// Scale entity with bounce
Tween_Scale(self, Vec3(2, 2, 2), 0.5, EaseOutBounce);

// Fade opacity
Tween_Float(self, "material.opacity", 0.0, 1.0, EaseInOutQuad);
```

### 25 Easing Functions

| Category | Functions |
|----------|-----------|
| Linear | Linear |
| Quad | EaseInQuad, EaseOutQuad, EaseInOutQuad |
| Cubic | EaseInCubic, EaseOutCubic, EaseInOutCubic |
| Quart | EaseInQuart, EaseOutQuart, EaseInOutQuart |
| Quint | EaseInQuint, EaseOutQuint, EaseInOutQuint |
| Sine | EaseInSine, EaseOutSine, EaseInOutSine |
| Expo | EaseInExpo, EaseOutExpo, EaseInOutExpo |
| Circ | EaseInCirc, EaseOutCirc, EaseInOutCirc |
| Elastic | EaseInElastic, EaseOutElastic, EaseInOutElastic |
| Bounce | EaseOutBounce |
| Back | EaseInBack, EaseOutBack |

### Visual Script Tween Nodes

Use the Tween category nodes:
- `TweenPosition`, `TweenScale`, `TweenFloat`, `TweenColor`

---

## Tutorial 46: LAN Multiplayer Basics

**Goal:** Set up local network multiplayer.

### Architecture

Enjin uses a **host-authoritative UDP** model:
- One player hosts the game (acts as server)
- Other players connect as clients
- Host has authority over game state
- Clients use prediction for responsiveness

### Setting Up

1. Open **View > Tools > Network** panel.
2. Choose **Host** or **Join**.
3. For hosting: set port (default 7777).
4. For joining: enter host IP address.

### Entity Ownership

Each networked entity has an owner (the player who controls it). Only the owner can modify the entity; changes sync to other players.

### RPC (Remote Procedure Calls)

```angelscript
// Call a function on all clients
RPC_Send("OnPlayerScored", playerName + "," + score);

// Handle incoming RPC
void OnRPC(string functionName, string data) {
    if (functionName == "OnPlayerScored") {
        // Parse data and update scoreboard
    }
}
```

### State Synchronization

The system syncs at 20Hz with interpolation buffer, providing smooth movement even with network latency.

---

## Tutorial 47: Newgrounds.io Integration

**Goal:** Publish your game on Newgrounds with medals and scoreboards.

### Setup

1. Create a project on Newgrounds.com.
2. Get your App ID and Encryption Key.
3. Configure in editor settings.

### Medals (Achievements)

```angelscript
// Unlock a medal
NG_UnlockMedal(12345);  // Medal ID from Newgrounds

// Check if unlocked
if (NG_IsMedalUnlocked(12345)) {
    // Show unlocked badge
}
```

### Scoreboards

```angelscript
// Submit a score
NG_SubmitScore(67890, playerScore);  // Board ID, score value

// Get scores
NG_GetScores(67890, 10);  // Board ID, count
```

### Cloud Saves

Use the Newgrounds save backend:
```angelscript
// Saves sync to Newgrounds cloud
SaveGame_ToSlot(1);  // Uses NewgroundsSaveBackend
```

---

## Tutorial 48: Building Your Game

**Goal:** Export a standalone game package.

### Build Pipeline

1. Go to **View > Settings > Project Settings > Build Config**.
2. Configure:
   - **Project Path** — Root of your project
   - **Output Directory** — Where to export
   - **Window Title** — Game window title
   - **Window Size** — Default resolution
3. Click **Build**, or **Build & Run** to save the scene, build, and launch the game in one step. After any successful build, a **Run** button launches the built game.

### What Happens

1. **Scan** — Finds all scenes, scripts, assets
2. **Validate** — Checks for missing references
3. **Pack** — Creates `game.enjpak` (compressed asset pack)
4. **Copy** — Copies `EnjinPlayer.exe` alongside the pack, plus the project's loose `scripts/` folder (including `scripts/enjin_api/`) and the whole `assets/` folder

### Asset Pack Format

Assets are packed into `.enjpak` files with:
- Per-file CRC32 integrity checks
- XOR obfuscation (basic protection)
- All asset types: scenes, scripts (.as), audio, textures, models (.gltf/.glb/.fbx/.obj), dialogue (.enjdlg), prefabs (.enjprefab), data assets, SVG, icons

### Running Your Game

The built game is a standalone folder:
```
MyGame/
├── EnjinPlayer.exe
├── game.enjpak
├── scripts/          (loose scripts, including scripts/enjin_api/)
├── assets/
└── icon.png (optional)
```

Double-click `EnjinPlayer.exe` to play.

---

## Tutorial 49: HTML5 Export

**Goal:** Export your game for web browsers.

### HTML5 Export

1. In Build Config, select **HTML5** as the target platform.
2. The export generates:
   - `index.html` — Game page with canvas
   - `game.js` — Game logic
   - `game.enjpak` — Asset pack
3. Includes preloader and responsive scaling.

### Newgrounds Embed

The HTML5 export is compatible with Newgrounds embedding. Upload the files to your Newgrounds project for instant web play.

### Considerations

- Audio requires user interaction before playing (browser policy)
- File system access is sandboxed
- WebGL is used for rendering (subset of Vulkan features)

---

## Tutorial 50: Accessibility Best Practices

**Goal:** Make your game accessible to all players.

### Built-in Accessibility Features

| Feature | Description |
|---------|-------------|
| Colorblind Modes | 8 modes (Protanopia through Achromatopsia) |
| Subtitles | Speaker names, direction indicators |
| Font Scaling | 0.5x to 3.0x text size |
| Dyslexia Mode | Increased letter/word/line spacing |
| Reduced Motion | Disable screen shake, weather particles |
| High Contrast | WCAG AAA 7:1+ contrast themes |
| Switch Access | One-button auto-scan navigation |
| Screen Reader | Announcer system for UI elements |

### Implementation Checklist

1. **Set accessible labels** on all UICanvas elements
2. **Test with colorblind modes** enabled (8 types)
3. **Provide subtitles** for all dialogue
4. **Test keyboard-only** navigation (Tab, Arrow keys, Enter)
5. **Avoid flashing lights** or provide a toggle
6. **Support font scaling** (test at 3x)
7. **Test with reduced motion** enabled
8. **Use high contrast themes** for UI

### Script Integration

```angelscript
// Subtitles
Subtitle_Show("Welcome, adventurer!", 3.0);
Subtitle_ShowWithSpeaker("Elder", "The artifact lies deep within.", 4.0);

// Announcer (screen reader)
Announcer_Announce("Menu opened", "polite");
Announcer_AnnounceUrgent("Health critical!");

// Query settings
bool reduced = Accessibility_GetReducedMotion();
float scale = Accessibility_GetFontScale();
```

---

## Tutorial 51: Shader Graph

**Goal:** Create custom shaders visually.

### Opening Shader Graph

Go to **View > Tools > Shader Graph**.

### Node Types (54)

Categories include:
- **Math** — Add, Multiply, Lerp, Clamp, Sin, Power, etc.
- **Texture** — Sample2D, UV coordinates, tiling
- **Color** — HSV conversion, blend modes, gradients
- **Vector** — Split, combine, normalize, transform
- **Noise** — Perlin, Simplex, Voronoi, cellular
- **Time** — Animated time, sine wave
- **Output** — Albedo, normal, metallic, roughness, emission

### Workflow

1. Start with the **Output** node (always present).
2. Right-click to add nodes from the palette.
3. Connect outputs to inputs by dragging wires.
4. The shader compiles to GLSL in real-time.
5. Save as `.enjshader` for reuse.

### Full GLSL Code Generation

The graph performs topological sort and generates complete vertex/fragment shader pairs. View the generated code in the preview panel.

---

## Tutorial 52: Plugin Development

**Goal:** Extend the engine with custom C++ plugins.

### Plugin SDK

Create a new C++ project that links against `PluginSDK.h`:

```cpp
#include "Enjin/Plugin/PluginSDK.h"

class MyPlugin : public Enjin::Plugin::IPlugin {
public:
    const char* GetName() override { return "MyPlugin"; }
    const char* GetVersion() override { return "1.0.0"; }

    bool Initialize(Enjin::Plugin::PluginContext* ctx) override {
        // Access ECS world, renderer, etc. via context
        return true;
    }

    void Update(float dt) override {
        // Per-frame logic
    }

    void Shutdown() override {
        // Cleanup
    }
};

ENJIN_PLUGIN_ENTRY(MyPlugin)
```

### Hot-Reload

Plugins support hot-reload: modify your plugin DLL, and the engine reloads it automatically during development. State is saved/restored across reloads.

### Script Bindings

```angelscript
bool loaded = Plugin_IsLoaded("MyPlugin");
string ver = Plugin_GetVersion("MyPlugin");
Plugin_Load("path/to/myplugin.dll");
Plugin_Unload("MyPlugin");
```

---

## Tutorial 53: Data Assets and Schemas

**Goal:** Create reusable data configurations.

### What are Data Assets?

Data assets are structured data files (JSON) with schemas for validation. Use them for item databases, enemy stats, level configurations, etc.

### Schema Definition

Create a `.enjschema` file:
```json
{
    "name": "WeaponData",
    "fields": [
        {"name": "damage", "type": "float", "default": 10.0},
        {"name": "fireRate", "type": "float", "default": 0.5},
        {"name": "ammoType", "type": "string", "default": "bullet"},
        {"name": "maxAmmo", "type": "int", "default": 30}
    ]
}
```

### Data Instances

Create `.enjdata` files that conform to the schema:
```json
{
    "schema": "WeaponData",
    "data": {
        "damage": 25.0,
        "fireRate": 0.8,
        "ammoType": "shotgun",
        "maxAmmo": 8
    }
}
```

### Script Access

```angelscript
// Load data asset
DataAsset@ weapon = DataAsset_Load("weapons/shotgun.enjdata");
float damage = weapon.GetFloat("damage");
int ammo = weapon.GetInt("maxAmmo");
```

---

## Tutorial 54: Splitscreen Multiplayer

**Goal:** Set up local splitscreen for 2-4 players.

### Setup

1. Create multiple camera entities (one per player).
2. Set each camera's **Viewport** to a screen region:
   - 2P horizontal: Camera1 (0, 0, 1, 0.5) + Camera2 (0, 0.5, 1, 0.5)
   - 2P vertical: Camera1 (0, 0, 0.5, 1) + Camera2 (0.5, 0, 0.5, 1)
   - 4P: Four quadrants

### Input Mapping

Each player needs separate input bindings. Use player indices for gamepad assignment.

### Performance

Splitscreen renders the scene multiple times. Tips:
- Reduce shadow quality per viewport
- Use LOD aggressively
- Consider lower resolution per viewport

---

## Tutorial 55: Performance Optimization

**Goal:** Identify and fix performance bottlenecks.

### Profiler

Open the **Stats Overlay** to monitor:
- FPS and frame time
- Draw calls and triangle count
- Physics step time
- Script execution time

### Common Bottlenecks

| Problem | Solution |
|---------|----------|
| Too many draw calls | Use sprite atlas, batch materials |
| Slow physics | Reduce collider count, use simpler shapes |
| Script overhead | Cache entity lookups, avoid per-frame allocations |
| Shadow rendering | Reduce cascade count, lower shadow resolution |
| Too many particles | Reduce max particles, increase emission rate instead |

### Best Practices

1. **Use LOD** for distant 3D objects
2. **Enable frustum culling** (automatic in Enjin)
3. **Use object pooling** instead of spawn/destroy
4. **Cache entity references** — `FindEntity()` is O(1) but still has overhead
5. **Use component queries** — `GetEntitiesWithComponent<T>()` instead of iterating all entities
6. **Reserve vectors** — Call `.reserve()` before large fills
7. **Minimize per-frame allocations** — Reuse containers as member variables
8. **Profile before optimizing** — Measure, don't guess

### Frame Rate Limiting

The engine defaults to 60 FPS. Configure in **Project Settings > Frame Rate**:
- `FPS30`, `FPS60`, `FPS120`, `FPS144`, `FPS240`, `Uncapped`

---

## Cross-Feature Interactions

### Combining Systems

The real power of Enjin comes from combining multiple systems. Here are some advanced patterns:

### Pattern: Procedural Dungeon with AI

1. Generate dungeon layout with **BSP** or **Cellular Automata**
2. Place enemy entities with **AI + Behavior Trees**
3. Add **NavMesh** for pathfinding
4. Create **Pickups** with `PickupComponent`
5. Track progress with **Quest System**
6. Save state with **Save System**

### Pattern: Living World

1. **World Time** drives day/night cycle
2. **Seasonal Weather** changes biomes
3. **Fluid Simulation** for rivers/lava
4. **Terrain** eroded by fluid over time
5. **Vegetation** responds to seasons
6. **AI** behaviors change by time of day

### Pattern: Retro Flash Game

1. Use **2D project mode** with pixel art sprites
2. Apply **Retro Effects** (pixelation, dithering)
3. Add **Sprite Animations** with the sheet importer
4. Score tracking with **HUD/UICanvas**
5. **Newgrounds** integration for medals/scoreboards
6. **HTML5 Export** for web play

### Pattern: Narrative RPG

1. **Dialogue Trees** for conversations
2. **Quest System** for story progression
3. **Save System** (RunState tier) for game progress
4. **Inventory** with `PickupComponent`
5. **Behavior Trees** for NPC decision-making
6. **Cinematic** cameras for cutscenes
7. **Timeline** animations for dramatic moments

### Pattern: Physics Puzzle

1. **Jolt Physics** for 3D simulation
2. **Joint Components** for mechanisms
3. **Destructible** environments for chain reactions
4. **Particle Effects** for visual feedback
5. **Audio Events** for satisfying sounds
6. **Object Pooling** for debris management

---

## Appendix A: File Formats

| Extension | Type | Description |
|-----------|------|-------------|
| `.enjscene` | Scene | Scene file (JSON) |
| `.enjprefab` | Prefab | Reusable entity template |
| `.enjpak` | Pack | Compiled asset package |
| `.enjdlg` | Dialogue | Dialogue tree |
| `.enjdata` | Data | Data asset instance |
| `.enjschema` | Schema | Data asset schema |
| `.enjshader` | Shader | Shader graph |
| `.enjaudiopkg` | Audio | Audio event package |
| `.enjparticle` | Particle | Particle graph |
| `.enjinasset` | Meta | Import settings sidecar |
| `.enjinproject` | Project | Project configuration |
| `.enjinlock` | Lock | Advisory entity/scene lock |
| `.as` | Script | AngelScript source |

## Appendix B: Key Constants

| Constant | Value |
|----------|-------|
| Max Entities | ~4 billion (u64 IDs) |
| Max Particles | 16,384 per emitter |
| Max Lights | 128 simultaneous |
| Max Shadow Cascades | 4 (CSM) |
| Max Collision Groups | 32 |
| Save Slots | 20 (17 manual + 3 auto) |
| Sprite Atlas Size | 4096x4096 |
| Sprite Atlas Threshold | 512px max dimension |
| Pack Obfuscation Key | `enjin_default_pack_key_2025` |

## Appendix C: Troubleshooting

| Problem | Solution |
|---------|----------|
| Sprite invisible | Ensure `Sprite2DComponent` has a texture set. Without texture, it renders through neither pipeline. |
| Shadows missing | Check `castShadows` on MeshComponent, enable shadows in Rendering panel |
| Physics not working | Ensure entities have both a collider AND rigidbody component |
| Script not running | Check ScriptComponent path, look for compile errors in Console |
| Audio silent | Verify file path, check volume > 0, ensure `is3D` positioning is correct |
| Model too small/large | Adjust import scale or entity transform scale |
| Poor performance | Check Stats Overlay, profile draw calls and physics time |
| 2D gameplay not working in Player | Previously `if (!m_Physics)` guard killed all 2D gameplay. Ensure Player is updated. |
