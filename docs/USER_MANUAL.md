# Enjin Engine User Manual

Enjin is a proprietary, licensable game engine built from scratch using C++20 and the Vulkan graphics API. It features a complete editor with Dear ImGui, an Entity-Component-System (ECS) architecture, PBR materials, skeletal animation, AngelScript scripting, and modern rendering capabilities.

This manual covers everything you need to get started and build games with Enjin.

> **Tutorials:** For hands-on, step-by-step tutorials covering all engine features (55 tutorials from basics to advanced topics), see [TUTORIALS.md](TUTORIALS.md).

---

## Table of Contents

1. [Getting Started](#1-getting-started)
2. [Editor Overview](#2-editor-overview)
3. [Creating Entities](#3-creating-entities)
4. [Templates](#4-templates)
5. [Components Reference](#5-components-reference)
6. [Scene Management](#6-scene-management)
7. [Play Mode](#7-play-mode)
8. [Effects and Environment](#8-effects-and-environment)
9. [Accessibility](#9-accessibility)
10. [Building and Distributing Games](#10-building-and-distributing-games)
11. [Scripting (AngelScript)](#11-scripting-angelscript)
12. [Procedural Generation](#12-procedural-generation)
13. [Splitscreen](#13-splitscreen)
14. [Physics Joints and Constraints](#14-physics-joints-and-constraints)
15. [Profiler and Debug Tools](#15-profiler-and-debug-tools)
16. [Plugin System](#16-plugin-system)
17. [Animation Timeline](#17-animation-timeline)
18. [Level Streaming](#18-level-streaming)
19. [Terrain Editing](#19-terrain-editing)
20. [AI and Pathfinding](#20-ai-and-pathfinding)
21. [Quest Flow Editor](#21-quest-flow-editor)
22. [Visual Scripting](#22-visual-scripting)
23. [Behavior Tree Editor](#23-behavior-tree-editor)
24. [Pixel Editor](#24-pixel-editor)
25. [Sprite Sheet Importer](#25-sprite-sheet-importer)
26. [Asset Browser](#26-asset-browser)
27. [Ray Tracing](#27-ray-tracing)
28. [Bug Reporting & Feedback](#28-bug-reporting--feedback)
29. [Vector Drawing Editor](#29-vector-drawing-editor)
30. [HTML5 Export](#30-html5-export)
31. [Newgrounds.io Integration](#31-negroundsio-integration)
32. [Networking & Security Settings](#32-networking--security-settings)

---

## 1. Getting Started

### Prerequisites

Before building Enjin, ensure the following tools are installed:

| Requirement | Minimum Version |
|-------------|-----------------|
| CMake | 3.20+ |
| C++20 Compiler | MSVC 2019+ / GCC 10+ / Clang 12+ |
| Vulkan SDK | Latest LunarG release |

### Build Instructions

#### Windows (Visual Studio)

```bash
cd build
cmake ..
cmake --build . --config Release
```

The editor executable will be located at `build/bin/Release/EnjinEditor.exe`.

#### Linux

```bash
cd build
cmake ..
make -j$(nproc)
```

The editor executable will be located at `build/bin/EnjinEditor`.

#### macOS

```bash
cd build
cmake ..
make -j$(nproc)
```

The editor executable will be located at `build/bin/EnjinEditor`.

#### Clean Rebuild (Linux/macOS)

```bash
cd build
make clean
make -j$(nproc)
```

#### Reconfiguring CMake

After adding new source files to the project, re-run CMake to pick them up:

```bash
cd build
cmake ..
```

### First Launch

When you launch the editor for the first time:

1. A splash screen appears briefly.
2. The **Template Selector** dialog opens, letting you choose a starting template (see [Templates](#4-templates)).
3. After selecting a template, the editor opens with the scene pre-populated.

### Window Icon

To set a custom window icon, place an `icon.png` file next to the editor executable. The engine will load it automatically on startup.

You can also set a custom icon from within the editor via **View > Settings > Project Settings > Window Icon** (or via the Build Config section in Project Settings). Browse for any PNG file, click **Apply**, and the window icon updates immediately. The path is saved in editor settings and auto-applied on startup. Use **Clear** to revert to the OS default.

---

## 2. Editor Overview

The Enjin editor is a panel-based workspace. All panels can be toggled from the **View** menu in the top menu bar. Panels are dockable and can be rearranged freely.

### Panels

| Panel | Description |
|-------|-------------|
| **Hierarchy** | Entity tree view. Right-click to add, delete, or duplicate entities. Supports drag-and-drop reparenting. |
| **Inspector** | Component editor for the selected entity. Displays and edits all attached components (50+ component types). Includes an "Add Component" button. |
| **Console** | Log output for engine messages, warnings, and errors. |
| **Asset Browser** | Browse and manage project files with grid/list view, thumbnails, search, and drag-and-drop. |
| **Editor Settings** | Grid display, gizmo settings, accent colors, and editor preferences. |
| **Post Processing** | Configure bloom, vignette, color grading, FXAA, and film grain. |
| **Retro Effects** | CRT scanlines, pixelation, dithering, and color quantization post-processing. |
| **Game View** | Rendered game camera output with Play/Pause/Stop controls. |
| **Scene List** | Multi-scene project management. Add, reorder, load scenes, and set the start scene. |
| **Rendering** | Skybox configuration, shadow settings, ambient lighting, cel shading, ray tracing, and fog. |
| **Stats Overlay** | Real-time performance metrics: FPS, frame time, draw calls, and triangle count. |
| **Visual Script** | Blueprint-style visual scripting editor with 76+ node types and debugger. |
| **Behavior Tree** | AI behavior tree editor with 20 node types, blackboard editor, and play-mode visualization. |
| **Quest Flow** | Visual quest designer with objectives, branches, conditions, and rewards. |
| **Pixel Editor** | Pixel art creation tool with layers, 8 drawing tools, undo/redo, and retro presets. |
| **Sprite Sheet Importer** | Import and slice sprite sheets with grid or auto-detect modes. |

### Entity and Component Icons

Entities in the **Hierarchy** panel display bracket-tag icons based on their primary component type, making it easy to identify entity roles at a glance:

`[C]` Camera, `[L]` Light, `[M]` Mesh, `[S]` Sprite, `[T]` Tilemap, `[P]` Particle, `[A]` Audio, `[R]` Rigidbody, `[D]` Dialogue, `[V]` Visual Script, `[U]` UI Canvas, `[AI]` AI, `[BT]` Behavior Tree.

Similarly, component headers in the **Inspector** panel show bracket-tag icons (e.g., `[T] Transform`, `[M] Mesh`, `[L] Light`).

### Empty States

Panels display helpful empty-state messages when there is nothing to show:

- **Hierarchy** — "No World Loaded" or "No Entities" (with a "Create Entity" button)
- **Inspector** — "No Entity Selected"
- **Asset Browser** — "Directory Not Found"
- **Dialogue** — "No DialogueComponent"
- **Plugin Browser** — "No Plugins Found"

### Keyboard Shortcuts

| Key | Action |
|-----|--------|
| `1` | Translate gizmo |
| `2` | Rotate gizmo |
| `3` | Scale gizmo |
| `4` | Toggle local/world space |
| `W` `A` `S` `D` | Fly camera movement |
| `Space` / `E` | Camera up |
| `Q` / `Ctrl` | Camera down |
| `Shift` | Sprint (faster fly camera) |
| Hold `RMB` + Mouse | Look around |
| Left-click | Select entity |
| Double-click | Focus on entity |
| `Ctrl` + click | Toggle entity in/out of multi-selection |
| `Shift` + click | Range select in hierarchy (from primary to clicked) |
| Drag in viewport | Marquee / rubber-band selection |
| `Delete` | Delete all selected entities |
| `Ctrl` + `D` | Duplicate all selected entities |
| `F` | Focus camera on selection centroid |
| Scroll wheel | Adjust fly camera speed |
| `Ctrl` + `S` | Save scene |
| `Ctrl` + `O` | Open scene |
| `Ctrl` + `I` | Import model |
| `Ctrl` + `X` | Cut entity |
| `Ctrl` + `C` | Copy entity |
| `Ctrl` + `V` | Paste entity |
| `Ctrl` + `Z` | Undo |
| `Ctrl` + `Y` | Redo |

### Multi-Select

Enjin supports selecting multiple entities at once for batch operations:

- **Ctrl+click** toggles an individual entity in or out of the current selection, both in the hierarchy and the viewport.
- **Shift+click** in the hierarchy performs a range select from the current primary selected entity to the clicked entity.
- **Drag in viewport** draws a marquee rectangle; all entities enclosed by the rectangle are added to the selection on release.
- **Primary entity**: The last entity clicked becomes the primary selection, which is used by the inspector and gizmo for reference.

When multiple entities are selected:

- The **Inspector** shows a list of all selected entities and provides batch transform editing:
  - **Position offset** -- apply a positional offset to all selected entities.
  - **Rotation offset** -- apply a rotational offset to all selected entities.
  - **Scale multiplier** -- multiply the scale of all selected entities.
  - Each has an **Apply** button to commit the change.
- The **Gizmo** appears at the centroid of all selected entities. Dragging the gizmo applies the delta transform to every selected entity.

---

## 3. Creating Entities

### Entity Menu

Use the **Entity** menu in the top menu bar to create new entities:

| Category | Options |
|----------|---------|
| **General** | Create Empty |
| **3D Objects** | Cube, Sphere, Plane, Cylinder, Cone |
| **2D Objects** | Sprite, Animated Sprite, Tilemap |
| **Lights** | Directional Light, Point Light, Spot Light |
| **Camera** | Camera |
| **Environment** | Ground Plane |

### Adding Components

Select an entity in the hierarchy or viewport, then use the **Add Component** button at the bottom of the Inspector panel. A searchable dropdown lists all available component types.

### Importing 3D Models

To import an external 3D model:

1. Go to **File > Import Model** (or press `Ctrl+I`).
2. Select a `.gltf` or `.glb` file from the file dialog.
3. The importer creates ECS entities for all meshes, materials, and hierarchy nodes.
4. Box colliders are auto-generated from mesh bounding boxes.
5. If the model contains skeletal animation data (skins, joints, animations), the importer sets up `SkeletonComponent` and `AnimatorComponent` automatically.

Import options include a configurable scale factor.

---

## 4. Templates

Enjin provides 23 built-in startup templates organized into 5 categories. When you create a new project or scene, the template selector offers these options. All templates start with a minimal 5-panel layout (Hierarchy, Inspector, Viewport, Console, Asset Browser) for a clean first impression:

**Foundations**

| Template | Description |
|----------|-------------|
| **Blank** | Empty scene with directional light, procedural skybox, and FXAA. |
| **2D Platformer** | Side-scrolling with wall jump, floating platforms, coin tween, torch particles. |
| **2D Top-Down Action** | Overhead 2D with dash, health, AI patrol enemy, health pickup. |
| **3D Third Person** | Over-the-shoulder camera with shadows, obstacle cubes, point light, bloom. |
| **3D First Person** | FPS camera in an L-shaped corridor with warm point light and vignette. |

**Genre Showcases**

| Template | Description |
|----------|-------------|
| **Sokoban Puzzle** | Pushable crates with grid snap, 3 goal plates, switch door, top-down 3D camera. |
| **Survival** | Temperature zones, weather zones, campfire particles, stamina, fog, hazard zone. |
| **RPG Village** | NPC dialogue, chest pickup, house/fences, lantern point light. |
| **Horror** | Flashlight (spot light with follow), fog, dark ambient, collectible notes, door switch. |
| **Vehicle Racing** | VehicleController with chase camera, track barriers, checkpoint/finish goal zones, cinematic camera. |
| **PS1 RPG** | Retro effects (pixelation, dither, color quantization, 320x240), flat shading, save point with magic particles. |
| **Arena Fighter** | 2-player splitscreen with per-player cameras, health + stamina, arena walls. |

**Systems Deep-Dives**

| Template | Description |
|----------|-------------|
| **Physics Playground** | Ramp, 5 rigidbodies (spheres/cubes/capsule), gravity zone (point mode), conveyor, moving platform. |
| **Dialogue & Narrative** | 3 NPCs with dialogue, quest state entity, dialogue box component, branching conversation notes. |
| **Save System Demo** | 3-tier persistence demo: RunState collectibles, SceneState checkpoint, MetaProgression stats, save/load menu. |
| **Visual Scripting** | 3 entities with VisualScriptComponent, switch, particle effect, guide notes for node editor. |
| **UI Canvas Demo** | UICanvasComponent, HUD health bar widget, guide notes for the UI editor. |

**Retro & Flash**

| Template | Description |
|----------|-------------|
| **Point & Click** | Adventure game with background, 3 click hotspots, inventory UI canvas, dialogue descriptions. |
| **Bullet Hell** | Fast top-down 2D, enemy spawner with particles, bullet pool, boundary walls. |
| **Idle/Clicker** | Click target with scale tween feedback, UI canvas, MetaProgression save data. |

**Advanced**

| Template | Description |
|----------|-------------|
| **Planet Gravity** | Spherical planet with point gravity zone, surface-aligned controller, dark space skybox. |
| **Dungeon Crawler** | Grid-based FPS with snap turns, L-shaped corridor walls, skeleton enemy, treasure, torch lights. |
| **Accessibility Menu** | In-game accessibility settings: subtitle toggle + size, colorblind toggle, reduced motion, input sensitivity. All controls use UICanvas focus navigation (Tab/Arrow/Gamepad). |

Each template creates the appropriate entities (ground, lights, player entity with controller, camera) and pre-configures component values for that genre. Every template includes NotesComponent hints explaining the featured systems.

### Custom Templates

You can save your own templates:

1. Set up a scene the way you want it.
2. Go to **File > Save as Template**.
3. Give it a name. The template is saved to the `templates/` directory.
4. Your custom template will appear in the template selector alongside the built-in ones.

---

## 5. Components Reference

This section documents every component type available in Enjin. Components are added to entities via the Inspector panel's **Add Component** button.

### 5.1 Core Components

#### TransformComponent

Every entity has a transform. Controls the entity's position, rotation, and scale in the scene.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `position` | Vector3 | (0, 0, 0) | World position. |
| `rotation` | Vector3 | (0, 0, 0) | Euler rotation in degrees. |
| `scale` | Vector3 | (1, 1, 1) | Scale on each axis. |

#### NameComponent

Gives the entity a human-readable display name shown in the hierarchy.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `name` | string | "" | Entity display name. |

#### MeshComponent

Stores the vertex and index data used by the renderer to draw geometry. Vertices contain position, normal, UV, color, tangent, bone weights, and bone indices.

Typically populated by importing a 3D model or by using a built-in primitive (Cube, Sphere, etc.).

#### MaterialComponent

Controls the visual surface properties of a mesh. Supports PBR rendering, texture maps, and retro rendering flags.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `baseColor` | Vector3 | (1, 1, 1) | Albedo color (RGB 0-1). |
| `opacity` | f32 | 1.0 | Transparency (0 = invisible, 1 = opaque). |
| `metallic` | f32 | 0.0 | Metallic factor (0 = dielectric, 1 = metal). |
| `roughness` | f32 | 0.5 | Surface roughness (0 = mirror, 1 = diffuse). |
| `emissiveColor` | Vector3 | (0, 0, 0) | Emission color. |
| `emissiveStrength` | f32 | 0.0 | Emission intensity multiplier. |
| `baseColorTexturePath` | string | "" | Path to base color/albedo texture. |
| `normalTexturePath` | string | "" | Path to tangent-space normal map. |
| `heightTexturePath` | string | "" | Path to height map for parallax mapping. |
| `parallaxScale` | f32 | 0.05 | Parallax occlusion mapping depth. |
| `doubleSided` | bool | false | Render both front and back faces. |
| `castShadows` | bool | true | Whether this mesh casts shadows. |
| `receiveShadows` | bool | true | Whether this mesh receives shadows. |
| `alphaMode` | enum | Opaque | Alpha mode: `Opaque`, `Mask`, `Blend`. |
| `alphaCutoff` | f32 | 0.5 | Cutoff threshold for `Mask` alpha mode. |

**Retro rendering flags** (per-material):

| Flag | Default | Description |
|------|---------|-------------|
| `flatShading` | false | Disables smooth shading, faceted look. |
| `affineTexturing` | false | PS1-style texture warping (no perspective correction). |
| `vertexSnapping` | false | PS1-style vertex jittering. |
| `vertexSnapResolution` | 160 | Grid resolution for vertex snapping (80-320). |
| `stippleTransparency` | false | Dithered transparency pattern instead of alpha blending. |
| `uvQuantize` | false | Quantize UV coordinates for retro look. |
| `gouraudOnly` | false | Force Gouraud shading (no per-pixel lighting). |

#### LightComponent

Adds a light source to the entity. The engine supports multiple simultaneous lights.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `type` | enum | Directional | Light type: `Directional`, `Point`, `Spot`. |
| `color` | Vector3 | (1, 1, 1) | Light color (RGB). |
| `intensity` | f32 | 1.0 | Brightness multiplier. |
| `direction` | Vector3 | (0, -1, 0) | Light direction (for directional and spot lights). |

#### CameraComponent

Attaches a game camera to an entity. The editor has its own camera; this component is for in-game cameras used during play mode. The highest-priority active camera is used for rendering.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `projectionType` | enum | Perspective | `Perspective` or `Orthographic`. |
| `fieldOfView` | f32 | 60.0 | Vertical FOV in degrees (perspective only). |
| `nearPlane` | f32 | 0.1 | Near clipping plane distance. |
| `farPlane` | f32 | 1000.0 | Far clipping plane distance. |
| `orthoSize` | f32 | 10.0 | Half-height of orthographic view. |
| `priority` | i32 | 0 | Higher priority cameras take precedence. |
| `isActive` | bool | true | Whether this camera is eligible for activation. |
| `backgroundColor` | Vector3 | (0.1, 0.1, 0.15) | Clear color. |
| `viewportX/Y` | f32 | 0.0 | Viewport position (normalized 0-1). |
| `viewportWidth/Height` | f32 | 1.0 | Viewport dimensions (normalized 0-1). |
| `cullingMask` | u32 | 0xFFFFFFFF | Layer bitmask for what to render. |

Camera frustum visualization is shown in the editor viewport for game cameras.

#### NotesComponent

Attach text annotations to entities for documentation or design notes. Visible only in the editor.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `notes` | string | "" | Free-form text annotation. Note: the field name is `.notes`, not `.text`. |

#### TextComponent

Renders 3D text in the world using stb_truetype font rasterization to a texture.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `text` | string | "" | The text string to render. |
| `fontPath` | string | "" | Path to a .ttf font file. |
| `fontSize` | f32 | 32.0 | Font size in pixels. |
| `color` | Vector3 | (1, 1, 1) | Text color. |

---

### 5.2 Character Controllers

Character controllers provide pre-built movement behaviors. Adding a controller to an entity **auto-creates a configured camera entity** appropriate for that controller type.

All controllers share a common base with these fields:

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `moveSpeed` | f32 | 5.0 | Base movement speed. |
| `sprintMultiplier` | f32 | 2.0 | Speed multiplier when sprinting. |
| `isEnabled` | bool | true | Whether the controller processes input. |
| `useWASD` | bool | true | Use WASD keys for movement. |
| `useArrowKeys` | bool | false | Use arrow keys for movement. |
| `useGamepad` | bool | false | Use gamepad input. |
| `gamepadIndex` | i32 | 0 | Gamepad index (0-3) for splitscreen. |
| `gamepadLookSensitivity` | f32 | 2.0 | Right stick camera sensitivity. |
| `disableMouseLook` | bool | false | Disable mouse/stick camera control. |
| `gridMovement` | bool | false | Snap movement to grid cells. |
| `gridCellSize` | f32 | 1.0 | Grid cell size in world units. |
| `gridMoveSpeed` | f32 | 8.0 | Speed of lerp between grid cells. |

**Note on stamina**: FPS and TPS controllers integrate with `ResourceComponent`. If a `ResourceComponent` is present on the same entity, sprinting consumes stamina at the rate defined by `sprintCostPerSec`, and jumping consumes `jumpCost`. Sprint is disabled while the resource is depleted.

#### Platformer2DController

Side-scrolling movement with gravity, jumping, and optional wall mechanics.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `jumpForce` | f32 | 8.0 | Upward force applied on jump. |
| `gravity` | f32 | 20.0 | Downward acceleration. |
| `maxJumps` | i32 | 2 | Maximum jumps (supports double jump). |
| `acceleration` | f32 | 50.0 | Horizontal acceleration. |
| `deceleration` | f32 | 40.0 | Horizontal deceleration. |
| `airControl` | f32 | 0.5 | Movement control multiplier while airborne. |
| `coyoteTime` | f32 | 0.1 | Grace period (seconds) after leaving a platform where jump is still allowed. |
| `jumpBufferTime` | f32 | 0.1 | Input buffer (seconds) for pressing jump slightly before landing. |
| `enableWallJump` | bool | false | Allow jumping off walls. |
| `enableWallSlide` | bool | false | Slide down walls slowly. |
| `wallSlideSpeed` | f32 | 2.0 | Descent speed while wall sliding. |
| `wallJumpForce` | f32 | 6.0 | Force applied on wall jump. |

#### TopDown2DController

Overhead 2D movement with 8-directional input and optional dash.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `acceleration` | f32 | 30.0 | Movement acceleration. |
| `deceleration` | f32 | 25.0 | Movement deceleration. |
| `rotateToFaceMovement` | bool | true | Entity rotates to face movement direction. |
| `rotationSpeed` | f32 | 720.0 | Degrees per second rotation. |
| `enableDash` | bool | false | Enable dash/dodge ability. |
| `dashSpeed` | f32 | 15.0 | Dash velocity. |
| `dashDuration` | f32 | 0.2 | Dash length in seconds. |
| `dashCooldown` | f32 | 1.0 | Cooldown between dashes. |

#### TopDown3DController

Isometric or overhead 3D movement, similar to Diablo-style games. Includes optional click-to-move.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `acceleration` | f32 | 30.0 | Movement acceleration. |
| `deceleration` | f32 | 25.0 | Movement deceleration. |
| `rotateToFaceMovement` | bool | true | Entity rotates to face movement direction. |
| `rotationSpeed` | f32 | 720.0 | Degrees per second rotation. |
| `cameraAngle` | f32 | 45.0 | Fixed camera angle from horizontal. |
| `cameraDistance` | f32 | 15.0 | Camera distance from player. |
| `cameraHeight` | f32 | 10.0 | Camera height above player. |
| `lockCameraToPlayer` | bool | true | Camera follows the player. |
| `enableClickToMove` | bool | false | Click on ground to move (Diablo-style). |
| `arrivalThreshold` | f32 | 0.5 | Distance at which click-to-move stops. |
| `enableDash` | bool | false | Enable dash/dodge ability. |

#### ThirdPersonController

Over-the-shoulder camera that orbits the player. Supports lock-on targeting.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `acceleration` | f32 | 30.0 | Movement acceleration. |
| `deceleration` | f32 | 25.0 | Movement deceleration. |
| `jumpForce` | f32 | 8.0 | Jump strength. |
| `gravity` | f32 | 20.0 | Downward acceleration. |
| `rotateToFaceMovement` | bool | true | Character faces movement direction. |
| `rotateToFaceCamera` | bool | false | Character always faces camera direction. |
| `rotationSpeed` | f32 | 720.0 | Degrees per second rotation. |
| `cameraDistance` | f32 | 5.0 | Default distance from player to camera. |
| `cameraHeight` | f32 | 2.0 | Camera height above player. |
| `cameraMinDistance` | f32 | 2.0 | Minimum zoom distance. |
| `cameraMaxDistance` | f32 | 15.0 | Maximum zoom distance. |
| `cameraMinPitch` | f32 | -30.0 | Minimum vertical look angle. |
| `cameraMaxPitch` | f32 | 60.0 | Maximum vertical look angle. |
| `cameraSensitivity` | f32 | 0.15 | Mouse sensitivity for orbit. |
| `cameraLerpSpeed` | f32 | 20.0 | Smooth camera follow speed. |
| `enableCameraCollision` | bool | true | Camera avoids clipping through geometry. |
| `enableLockOn` | bool | false | Enable lock-on targeting system. |
| `lockOnRange` | f32 | 20.0 | Maximum lock-on distance. |

#### FirstPersonController

FPS-style camera and movement with mouse look, crouching, and optional head bob.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `acceleration` | f32 | 50.0 | Movement acceleration. |
| `deceleration` | f32 | 40.0 | Movement deceleration. |
| `jumpForce` | f32 | 7.0 | Jump strength. |
| `gravity` | f32 | 20.0 | Downward acceleration. |
| `mouseSensitivity` | f32 | 2.0 | Mouse look sensitivity. |
| `minPitch` | f32 | -89.0 | Minimum vertical look angle. |
| `maxPitch` | f32 | 89.0 | Maximum vertical look angle. |
| `invertY` | bool | false | Invert vertical mouse axis. |
| `enableHeadBob` | bool | false | Subtle camera bob while walking. |
| `headBobFrequency` | f32 | 8.0 | Bob oscillation speed. |
| `headBobAmplitude` | f32 | 0.05 | Bob vertical displacement. |
| `enableCrouch` | bool | true | Allow crouching. |
| `standingHeight` | f32 | 1.8 | Camera height when standing. |
| `crouchingHeight` | f32 | 1.0 | Camera height when crouching. |
| `crouchSpeed` | f32 | 0.5 | Movement speed multiplier when crouching. |
| `sprintFOVIncrease` | f32 | 10.0 | FOV increase while sprinting. |
| `dungeonCrawlerMode` | bool | false | SMT-style movement: snap turns and facing-relative movement. |
| `snapTurnAngle` | f32 | 90.0 | Degrees per snap turn (A/D keys) in dungeon crawler mode. |

#### VehicleController

Car-like physics with steering, acceleration, braking, and drift mechanics.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `maxSpeed` | f32 | 30.0 | Top forward speed (units/sec). |
| `reverseMaxSpeed` | f32 | 10.0 | Top reverse speed. |
| `acceleration` | f32 | 15.0 | Engine acceleration force. |
| `brakeForce` | f32 | 25.0 | Brake deceleration force. |
| `engineBrake` | f32 | 5.0 | Deceleration when no input (engine drag). |
| `maxSteerAngle` | f32 | 35.0 | Maximum wheel turn angle (degrees). |
| `steerSpeed` | f32 | 120.0 | Steering input speed (degrees/sec). |
| `steerReturnSpeed` | f32 | 200.0 | Auto-center speed (degrees/sec). |
| `wheelBase` | f32 | 2.5 | Distance between front and rear axles. |
| `grip` | f32 | 1.0 | Tire grip multiplier (lower = more sliding). |
| `driftFactor` | f32 | 0.9 | Lateral velocity retention (1 = no drift, 0 = full drift). |
| `downforceMultiplier` | f32 | 0.5 | Speed-dependent downforce. |
| `mass` | f32 | 1000.0 | Vehicle mass in kg. |
| `bodyRollAmount` | f32 | 5.0 | Degrees of body roll in turns (visual). |
| `bodyPitchAmount` | f32 | 3.0 | Degrees of body pitch on accel/brake (visual). |

#### PossessableComponent

Allows the player to switch which entity they control at runtime.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `isPossessed` | bool | false | Currently controlled by the player. |
| `autoDetect` | bool | true | Auto-detect controller type on possess. |
| `playerIndex` | i32 | 0 | Which player (0-3) can possess this entity. |
| `possessRange` | f32 | 5.0 | Maximum distance to possess (0 = unlimited). |
| `promptText` | string | "Press E to enter" | UI prompt shown when in range. |
| `transitionDuration` | f32 | 0.3 | Camera blend time on possess/unpossess. |
| `disableOnUnpossess` | bool | true | Disable controller when not possessed. |

---

### 5.3 Physics Components

#### RigidbodyComponent

Adds physics simulation to an entity. Controls mass, velocity, gravity, and constraints.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `mass` | f32 | 1.0 | Object mass. |
| `drag` | f32 | 0.0 | Linear drag (air resistance). |
| `angularDrag` | f32 | 0.05 | Angular drag. |
| `useGravity` | bool | true | Apply gravity. |
| `gravityScale` | f32 | 1.0 | Gravity strength multiplier. |
| `velocity` | Vector3 | (0, 0, 0) | Current linear velocity. |
| `angularVelocity` | Vector3 | (0, 0, 0) | Current angular velocity. |
| `freezePositionX/Y/Z` | bool | false | Lock position on individual axes. |
| `freezeRotationX/Y/Z` | bool | false | Lock rotation on individual axes. |
| `bodyType` | enum | Dynamic | `Dynamic` (fully simulated), `Kinematic` (moved by code), or `Static` (never moves). |
| `collisionMode` | enum | Discrete | `Discrete`, `Continuous`, or `ContinuousSpeculative`. Continuous modes prevent fast objects from passing through geometry. |

#### BoxColliderComponent

Axis-aligned box collision shape.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `center` | Vector3 | (0, 0, 0) | Collider center offset from entity origin. |
| `size` | Vector3 | (1, 1, 1) | Box dimensions. |
| `isTrigger` | bool | false | If true, does not block movement (fires events only). |
| `friction` | f32 | 0.5 | Surface friction. |
| `bounciness` | f32 | 0.0 | Restitution (0 = no bounce, 1 = full bounce). |
| `categoryBits` | u32 | 1 | Bitmask of collision groups this object belongs to. |
| `collisionMask` | u32 | 0xFFFFFFFF | Bitmask of groups this object collides with. |

#### SphereColliderComponent

Spherical collision shape.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `center` | Vector3 | (0, 0, 0) | Collider center offset. |
| `radius` | f32 | 0.5 | Sphere radius. |
| `isTrigger` | bool | false | Trigger mode. |
| `friction` | f32 | 0.5 | Surface friction. |
| `bounciness` | f32 | 0.0 | Restitution. |
| `categoryBits` | u32 | 1 | Collision group bitmask. |
| `collisionMask` | u32 | 0xFFFFFFFF | Collision mask. |

#### CapsuleColliderComponent

Capsule collision shape, commonly used for characters.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `center` | Vector3 | (0, 0, 0) | Collider center offset. |
| `radius` | f32 | 0.5 | Capsule radius. |
| `height` | f32 | 2.0 | Capsule total height. |
| `direction` | enum | Y | Capsule orientation axis: `X`, `Y`, or `Z`. |
| `isTrigger` | bool | false | Trigger mode. |
| `friction` | f32 | 0.5 | Surface friction. |
| `bounciness` | f32 | 0.0 | Restitution. |
| `categoryBits` | u32 | 1 | Collision group bitmask. |
| `collisionMask` | u32 | 0xFFFFFFFF | Collision mask. |

#### TriggerZoneComponent

Fires events when entities enter, exit, or stay inside the zone.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `shape` | enum | Box | `Box` or `Sphere`. |
| `boxSize` | Vector3 | (2, 2, 2) | Box dimensions (when shape is Box). |
| `sphereRadius` | f32 | 1.0 | Sphere radius (when shape is Sphere). |
| `triggerMask` | u32 | 0xFFFFFFFF | Which collision layers can trigger this zone. |
| `triggerOnce` | bool | false | Only fire the first time (one-shot). |
| `onEnterNotify` | Entity | 0 | Entity to notify when something enters. |
| `onExitNotify` | Entity | 0 | Entity to notify when something exits. |
| `onStayNotify` | Entity | 0 | Entity to notify while something stays inside. |

---

### 5.4 Audio Components

#### AudioSourceComponent

Plays audio clips with 3D spatialization support.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `clipPath` | string | "" | Path to the audio file. |
| `volume` | f32 | 1.0 | Playback volume (0-1). |
| `pitch` | f32 | 1.0 | Playback pitch (1.0 = normal). |
| `minDistance` | f32 | 1.0 | Distance at which sound is at full volume. |
| `maxDistance` | f32 | 500.0 | Distance at which sound reaches minimum volume. |
| `playOnAwake` | bool | false | Automatically start playing when play mode begins. |
| `loop` | bool | false | Loop the clip. |
| `is3D` | bool | true | Enable 3D spatial audio. |
| `spatialBlend` | f32 | 1.0 | Blend between 2D (0) and 3D (1) spatialization. |
| `rolloff` | enum | Logarithmic | Volume falloff curve: `Logarithmic`, `Linear`, or `Custom`. |
| `priority` | i32 | 128 | Playback priority (lower = higher priority when too many sounds are playing). |

#### AudioListenerComponent

Defines the "ears" of the scene. Typically attached to the player or camera entity. Only one active listener should exist at a time.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `isActive` | bool | true | Whether this listener receives audio. |
| `volumeScale` | f32 | 1.0 | Master volume scale for this listener. |

---

### 5.5 Interaction and Item Components

#### InteractableComponent

Makes an entity interactable by the player (doors, NPCs, switches, etc.).

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `promptText` | string | "Press E to interact" | Text shown when player is in range. |
| `interactionRange` | f32 | 2.0 | Maximum interaction distance. |
| `requiresLookAt` | bool | true | Player must be facing the object. |
| `lookAtAngle` | f32 | 45.0 | Cone angle (degrees) for look-at check. |
| `isEnabled` | bool | true | Whether interaction is currently available. |
| `singleUse` | bool | false | Disable after first interaction. |
| `highlightOnHover` | bool | true | Visual highlight when player is in range. |
| `highlightColor` | Vector3 | (1, 1, 0) | Highlight color (default: yellow). |
| `onInteractNotify` | Entity | 0 | Entity to notify on interaction. |

#### PickupComponent

Collectible items that the player can pick up.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `type` | enum | Coin | `Health`, `Ammo`, `Coin`, `Key`, `Powerup`, `Custom`. |
| `value` | f32 | 1.0 | Amount to give on pickup. |
| `customId` | string | "" | Identifier for custom item types. |
| `pickupRange` | f32 | 1.0 | Distance at which pickup is collected. |
| `destroyOnPickup` | bool | true | Remove entity after collection. |
| `magnetToPlayer` | bool | false | Auto-attract toward the player. |
| `magnetRange` | f32 | 3.0 | Distance at which magnet effect begins. |
| `magnetSpeed` | f32 | 10.0 | Speed of magnet attraction. |
| `canRespawn` | bool | false | Respawn after being collected. |
| `respawnTime` | f32 | 10.0 | Seconds before respawning. |
| `bobSpeed` | f32 | 2.0 | Visual floating bob animation speed. |
| `bobHeight` | f32 | 0.2 | Bob vertical displacement. |
| `rotationSpeed` | f32 | 90.0 | Visual spin speed (degrees/sec). |

#### InventoryComponent

Manages an item inventory with slots, currency, and keys.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `slots` | list | [] | List of inventory slots, each with `itemId`, `quantity`, `maxStack`. |
| `maxSlots` | usize | 20 | Maximum number of inventory slots. |
| `coins` | i32 | 0 | Coin currency count. |
| `gems` | i32 | 0 | Gem currency count. |
| `keys` | list | [] | List of key ID strings (used by LockComponent). |

---

### 5.6 AI Components

#### AIControllerComponent

Simple AI with state-based behavior: idle, patrol, chase, attack, flee, and dead states.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `currentState` | enum | Idle | AI state: `Idle`, `Patrol`, `Chase`, `Attack`, `Flee`, `Dead`. |
| `targetEntity` | Entity | 0 | Entity to chase/attack. |
| `detectionRange` | f32 | 10.0 | Distance at which the AI detects targets. |
| `attackRange` | f32 | 2.0 | Distance at which the AI can attack. |
| `loseTargetRange` | f32 | 15.0 | Distance at which the AI gives up chasing. |
| `fieldOfView` | f32 | 120.0 | Detection cone angle in degrees. |
| `moveSpeed` | f32 | 3.0 | Movement speed. |
| `turnSpeed` | f32 | 180.0 | Rotation speed (degrees/sec). |
| `stoppingDistance` | f32 | 1.0 | Stop moving when within this distance of target. |
| `attackCooldown` | f32 | 1.0 | Seconds between attacks. |
| `attackDamage` | f32 | 10.0 | Damage per attack. |
| `patrolPoints` | list | [] | List of Vector3 positions for patrol routes. |
| `patrolWaitTime` | f32 | 2.0 | Seconds to wait at each patrol point. |

#### FollowTargetComponent

Makes an entity follow another entity with smooth movement.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `target` | Entity | 0 | Entity to follow. |
| `followDistance` | f32 | 3.0 | Desired distance from target. |
| `minDistance` | f32 | 1.0 | Stop if closer than this. |
| `maxDistance` | f32 | 20.0 | Give up if farther than this. |
| `moveSpeed` | f32 | 5.0 | Follow movement speed. |
| `smoothTime` | f32 | 0.3 | Movement smoothing. |
| `matchTargetRotation` | bool | false | Also match the target's rotation. |
| `offset` | Vector3 | (0, 0, 0) | Positional offset from target. |
| `useLocalOffset` | bool | false | Offset relative to target's rotation. |

#### LookAtTargetComponent

Rotates an entity to face a target entity or world position.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `target` | Entity | 0 | Entity to face. |
| `worldTarget` | Vector3 | (0, 0, 0) | Alternative: world position to face. |
| `useWorldTarget` | bool | false | Use world position instead of target entity. |
| `rotationSpeed` | f32 | 180.0 | Degrees per second. |
| `instant` | bool | false | Snap instantly to face target. |
| `constrainX/Y/Z` | bool | varies | Lock rotation on individual axes (Z defaults to true). |
| `minYaw` / `maxYaw` | f32 | -180 / 180 | Yaw angle limits. |
| `minPitch` / `maxPitch` | f32 | -89 / 89 | Pitch angle limits. |

#### WaypointComponent

Marks a position in the world as a waypoint for AI pathfinding or patrol routes.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `waypointId` | string | "" | Unique identifier for this waypoint. |
| `index` | i32 | 0 | Order in path sequence. |
| `nextWaypoint` | Entity | 0 | Next waypoint in a linked list. |
| `waitTime` | f32 | 0.0 | Time to wait at this waypoint. |
| `radius` | f32 | 0.5 | Arrival threshold distance. |

---

### 5.7 Spawning

#### SpawnPointComponent

Defines where and how entities are spawned in the scene.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `spawnId` | string | "" | Identifier for this spawn point. |
| `prefabToSpawn` | string | "" | Prefab name to instantiate. |
| `spawnOnStart` | bool | false | Spawn immediately when play begins. |
| `spawnDelay` | f32 | 0.0 | Delay before first spawn (seconds). |
| `respawnTime` | f32 | 0.0 | Time between respawns (0 = no respawn). |
| `maxSpawns` | i32 | -1 | Maximum spawn count (-1 = unlimited). |
| `spawnRadius` | f32 | 0.0 | Random position variance within radius. |
| `randomRotation` | bool | false | Randomize spawned entity rotation. |

---

### 5.8 Timers

#### TimerComponent

A general-purpose countdown timer that can notify another entity on completion.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `duration` | f32 | 1.0 | Timer duration in seconds. |
| `elapsed` | f32 | 0.0 | Current elapsed time. |
| `isRunning` | bool | false | Whether the timer is active. |
| `loop` | bool | false | Restart automatically on completion. |
| `autoStart` | bool | false | Begin counting when play starts. |
| `onCompleteNotify` | Entity | 0 | Entity to notify when timer finishes. |

Helper queries: `GetProgress()` (0-1), `GetRemaining()`, `IsComplete()`.

---

### 5.9 Environment Components

#### GravityZoneComponent

Overrides gravity for entities within the zone. Supports directional gravity (e.g., sideways), point gravity (toward a center), and zero-G.

#### TemperatureZoneComponent

Applies heat or cold environmental effects to entities within the zone. Can deal damage over time.

#### CameraTriggerComponent

A camera override volume. When the player enters this zone, the game camera switches to the specified camera configuration.

---

### 5.10 Visual Components

#### BillboardComponent

Makes the entity always face the camera, useful for sprites, health bars, and labels in 3D.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `faceCamera` | bool | true | Enable billboard behavior. |
| `lockY` | bool | true | Only rotate on Y axis (vertical lock, like trees). |
| `rotationOffset` | f32 | 0.0 | Additional rotation in degrees. |

#### ParticleEmitterComponent

Emits particles with configurable shape, lifetime, color, and forces.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `isPlaying` | bool | false | Whether particles are being emitted. |
| `playOnAwake` | bool | true | Start emitting when play begins. |
| `loop` | bool | true | Loop emission. |
| `emissionRate` | f32 | 10.0 | Particles emitted per second. |
| `burstCount` | i32 | 0 | Instant burst of particles. |
| `burstInterval` | f32 | 0.0 | Time between bursts. |
| `lifetime` | f32 | 2.0 | Particle lifetime in seconds. |
| `lifetimeVariance` | f32 | 0.5 | Random lifetime variation. |
| `startSpeed` | f32 | 5.0 | Initial particle speed. |
| `speedVariance` | f32 | 1.0 | Random speed variation. |
| `startSize` / `endSize` | f32 | 0.5 / 0.1 | Size at birth and death. |
| `startColor` / `endColor` | Vector3 | (1,1,1) | Color at birth and death. |
| `startAlpha` / `endAlpha` | f32 | 1.0 / 0.0 | Opacity at birth and death. |
| `shape` | enum | Cone | Emitter shape: `Point`, `Sphere`, `Hemisphere`, `Cone`, `Box`. |
| `shapeRadius` | f32 | 0.1 | Emitter shape radius. |
| `coneAngle` | f32 | 30.0 | Cone emission angle (degrees). |
| `gravity` | Vector3 | (0, -9.8, 0) | Gravity force on particles. |
| `drag` | f32 | 0.0 | Air resistance on particles. |
| `texturePath` | string | "" | Particle texture. |
| `textureSheetX/Y` | i32 | 1 / 1 | Animation frame grid dimensions. |

#### Sprite2DComponent

2D sprite rendering for 2D games.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `texturePath` | string | "" | Sprite texture path. |
| `srcX`, `srcY` | f32 | 0 | Source rectangle position in texture (for sprite sheets). |
| `srcWidth`, `srcHeight` | f32 | 0 | Source rectangle size (0 = full texture). |
| `size` | Vector2 | (1, 1) | Display size in world units. |
| `pivot` | Vector2 | (0.5, 0.5) | Pivot point (0-1, center by default). |
| `tint` | Vector3 | (1, 1, 1) | Color tint. |
| `alpha` | f32 | 1.0 | Opacity. |
| `sortingLayer` | i32 | 0 | Render order (layer). |
| `orderInLayer` | i32 | 0 | Render order within layer. |
| `flipX` / `flipY` | bool | false | Flip sprite horizontally/vertically. |
| `visible` | bool | true | Visibility toggle. |

#### AnimatedSprite2DComponent

Sprite sheet animation with per-frame timing.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `frames` | list | [] | List of frames, each with `srcX`, `srcY`, `duration` (seconds). |
| `currentFrame` | u32 | 0 | Current frame index. |
| `playing` | bool | true | Whether animation is playing. |
| `loop` | bool | true | Loop animation. |
| `playbackSpeed` | f32 | 1.0 | Speed multiplier. |

#### TilemapComponent

Grid-based tile rendering for retro-style 2D games.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `tiles` | list | [] | Flat array of tile indices (-1 = empty). |
| `width` / `height` | u32 | 0 | Grid dimensions in tiles. |
| `tilesetPath` | string | "" | Path to tileset texture. |
| `tileWidth` / `tileHeight` | f32 | 16 | Tile size in pixels within the tileset. |
| `tilesetColumns` | u32 | 16 | Number of tile columns in tileset. |
| `worldTileWidth` / `worldTileHeight` | f32 | 1.0 | Size of each tile in world units. |
| `hasCollision` | bool | false | Enable tile collision. |
| `collisionMask` | list | [] | Per-tile boolean: which tiles are solid. |

Helper methods: `GetTile(x, y)` and `SetTile(x, y, tileIndex)`.

#### Camera2DBoundsComponent

Constrains a 2D camera within boundaries, with smooth follow and zoom.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `useBounds` | bool | false | Enable boundary constraints. |
| `minBounds` / `maxBounds` | Vector2 | (0,0) | World-space boundaries. |
| `boundsPadding` | f32 | 0.0 | Padding inside bounds. |
| `followTarget` | Entity | 0 | Entity for the camera to follow. |
| `followSmoothing` | f32 | 5.0 | Follow smoothing speed (higher = faster). |
| `followOffset` | Vector2 | (0, 0) | Offset from follow target. |
| `minZoom` / `maxZoom` | f32 | 0.5 / 3.0 | Zoom range. |
| `currentZoom` | f32 | 1.0 | Current zoom level. |

---

### 5.11 State and Dialogue Components

#### StateMachineComponent

A general-purpose state machine for game logic, AI, or animation.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `currentState` | string | "idle" | Name of the current state. |
| `previousState` | string | "" | Name of the previous state. |
| `stateTimer` | f32 | 0.0 | Time spent in the current state. |
| `stateJustChanged` | bool | false | True on the first frame of a new state. |
| `floatParams` | list | [] | Named float parameters (key-value pairs). |
| `intParams` | list | [] | Named integer parameters. |
| `boolParams` | list | [] | Named boolean parameters. |

Methods: `SetState(name)`, `SetFloat(name, value)`, `GetFloat(name)`, `SetBool(name, value)`, `GetBool(name)`.

#### DialogueComponent

Retro RPG-style dialogue system with typewriter effect and branching choices.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `dialogueLines` | list | [] | List of text strings for sequential display. |
| `charDelay` | f32 | 0.05 | Seconds between characters (typewriter speed). |
| `speakerName` | string | "" | Name of the speaking character. |
| `portraitPath` | string | "" | Path to character portrait image. |
| `typeSound` | string | "" | Audio clip for typewriter effect. |
| `playTypeSound` | bool | true | Play sound per character. |
| `choices` | list | [] | Branching choices, each with `text` and `nextDialogueId`. |

Methods: `IsComplete()`, `StartDialogue(lines)`, `GetVisibleText()`.

#### DialogueBoxComponent

Automatically builds a UICanvas-based dialogue box overlay for displaying dialogue. Attach this alongside a `DialogueComponent` and a `UICanvasComponent` — the system creates and syncs all UI elements automatically.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `boxHeight` | f32 | 200.0 | Height of the dialogue panel in pixels. |
| `boxMargin` | f32 | 20.0 | Margin from screen edges. |
| `boxPadding` | f32 | 16.0 | Interior padding. |
| `boxColor` | Vector3 | (0.05, 0.05, 0.08) | Background color of the panel. |
| `boxAlpha` | f32 | 0.92 | Panel opacity. |
| `boxBorderRadius` | f32 | 8.0 | Corner rounding radius. |
| `speakerFontSize` | f32 | 20.0 | Font size for the speaker name. |
| `defaultSpeakerColor` | Vector3 | (0.9, 0.85, 0.5) | Default color for speaker name text. |
| `textFontSize` | f32 | 17.0 | Font size for dialogue text. |
| `textColor` | Vector3 | (0.9, 0.9, 0.9) | Dialogue text color. |
| `showPortrait` | bool | true | Show character portrait image. |
| `portraitSize` | f32 | 96.0 | Portrait dimensions in pixels. |
| `choiceFontSize` | f32 | 16.0 | Font size for choice buttons. |
| `choiceColor` | Vector3 | (0.7, 0.7, 0.7) | Default choice text color. |
| `choiceHighlightColor` | Vector3 | (1.0, 0.9, 0.3) | Highlighted choice color. |
| `showContinueIndicator` | bool | true | Show blinking "continue" indicator. |
| `continueText` | string | ">" | Text for the continue indicator. |
| `continueBlinkRate` | f32 | 2.0 | Blink speed in Hz. |

The inspector groups settings into collapsible sections: **Box Layout**, **Text Style**, **Portrait**, **Choices**, and **Continue Indicator**.

---

### 5.12 Puzzle and Level Design Components

#### LockComponent

Represents a locked entity (door, gate, chest) that requires a key to open.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `requiredKey` | string | "" | Key ID that unlocks this (matches keys in InventoryComponent). |
| `isLocked` | bool | true | Whether the lock is currently locked. |
| `consumeKey` | bool | false | Remove key from inventory on use. |
| `autoOpen` | bool | false | Open automatically when a player with the key enters range. |
| `interactRange` | f32 | 2.0 | Interaction distance. |
| `openMode` | enum | Toggle | `Toggle` (open/close), `OpenOnly` (stays open), or `Timed` (closes after duration). |
| `openDuration` | f32 | 5.0 | Duration for Timed mode. |
| `closedPosition` / `openPosition` | Vector3 | varies | Position lerp targets for animation. |
| `closedRotation` / `openRotation` | Vector3 | (0,0,0) | Rotation lerp targets for animation. |
| `openSpeed` | f32 | 3.0 | Lerp speed for open/close animation. |
| `lockedPrompt` | string | "Requires key" | UI prompt when locked. |
| `unlockedPrompt` | string | "Press E to open" | UI prompt when unlocked. |

#### PushableComponent

Makes an entity pushable by the player or other forces. Supports Sokoban-style grid snapping.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `mass` | f32 | 1.0 | Heavier objects are slower to push. |
| `pushSpeed` | f32 | 3.0 | Movement speed when being pushed. |
| `friction` | f32 | 0.9 | Velocity damping per frame. |
| `gridSnap` | bool | false | Snap to grid cells (Sokoban-style). |
| `gridCellSize` | f32 | 1.0 | Grid cell size. |
| `gridMoveSpeed` | f32 | 6.0 | Speed of cell-to-cell lerp. |
| `pushableX` / `pushableZ` | bool | true | Allow pushing on each axis. |
| `pushableY` | bool | false | Allow vertical pushing. |
| `canBePushedOff` | bool | false | Allow pushing off ledges. |

#### SwitchComponent

Pressure plates, toggles, and timed switches that activate linked entities.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `type` | enum | PressurePlate | `PressurePlate`, `Toggle`, `OneShot`, `Timed`, or `Sequence`. |
| `isActive` | bool | false | Current activation state. |
| `requiredTag` | string | "" | Only entities with this tag can activate it. |
| `activationWeight` | f32 | 0.0 | Minimum mass for pressure plates (0 = any). |
| `activeDuration` | f32 | 5.0 | Duration for Timed mode. |
| `sequenceIndex` | i32 | 0 | Position in sequence (for Sequence mode). |
| `sequenceGroup` | i32 | 0 | Which sequence group this belongs to. |
| `linkedEntities` | list | [] | Entities controlled by this switch. |
| `offPosition` / `onPosition` | Vector3 | varies | Visual position transition. |
| `transitionSpeed` | f32 | 8.0 | Visual transition speed. |
| `promptText` | string | "Press E" | Interaction prompt. |

#### GoalZoneComponent

Target area for puzzle completion: Sokoban goals, checkpoints, level exits.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `type` | enum | PushTarget | `PushTarget`, `StandOn`, `ItemDeposit`, `Checkpoint`, or `LevelExit`. |
| `requiredTag` | string | "" | Entity tag that satisfies this goal (e.g., "crate"). |
| `requiredItem` | string | "" | Item ID for ItemDeposit type. |
| `isSatisfied` | bool | false | Whether the goal condition is met. |
| `goalGroup` | i32 | 0 | Group ID (all goals in a group must be satisfied). |
| `inactiveColor` / `activeColor` | Vector3 | gray / green | Visual feedback colors. |
| `nextScene` | string | "" | Scene to transition to (for LevelExit type). |

#### ConveyorComponent

Moves entities along a direction, like a conveyor belt.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `direction` | Vector3 | (1, 0, 0) | Movement direction (normalized). |
| `speed` | f32 | 3.0 | Movement speed. |
| `affectsPlayer` | bool | true | Whether the conveyor moves the player. |
| `affectsPushables` | bool | true | Whether the conveyor moves pushable objects. |
| `isActive` | bool | true | Toggle conveyor on/off. |

#### TeleporterComponent

Instantly moves entities to a target position when they enter the teleporter.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `targetPosition` | Vector3 | (0,0,0) | Destination position. |
| `targetRotation` | Vector3 | (0,0,0) | Destination rotation (Euler angles). |
| `linkedTeleporter` | Entity | 0 | For bidirectional teleporters. |
| `cooldown` | f32 | 1.0 | Seconds before teleporter can be used again. |
| `preserveVelocity` | bool | false | Keep the entity's velocity after teleporting. |
| `requiredTag` | string | "" | Only entities with this tag can teleport (empty = any). |

#### DestructibleComponent

Entity that can be destroyed by damage or interaction.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `health` | f32 | 1.0 | Hit points. |
| `destroyOnHit` | bool | true | One-hit destroy. |
| `spawnPickup` | bool | false | Drop an item on destruction. |
| `pickupId` | string | "" | Item to drop. |
| `pickupCount` | i32 | 1 | Number of items to drop. |
| `canRespawn` | bool | false | Respawn after destruction. |
| `respawnTime` | f32 | 10.0 | Seconds before respawn. |
| `shakeOnHit` | f32 | 0.1 | Screen/entity shake intensity on hit. |

#### MovingPlatformComponent

A platform that moves between waypoints, carrying entities standing on it.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `waypoints` | list | [] | List of Vector3 positions the platform visits. |
| `speed` | f32 | 2.0 | Movement speed. |
| `waitTime` | f32 | 1.0 | Pause duration at each waypoint. |
| `mode` | enum | PingPong | `Loop` (A-B-C-A-B), `PingPong` (A-B-C-B-A), `OneWay` (A-B-C stop), or `Triggered` (moves only when activated by a switch). |
| `carryEntities` | bool | true | Entities standing on the platform move with it. |

---

### 5.13 Combat Components

#### HealthComponent

Tracks hit points, shield, regeneration, and invulnerability.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `maxHealth` | f32 | 100.0 | Maximum HP. |
| `currentHealth` | f32 | 100.0 | Current HP. |
| `regenRate` | f32 | 0.0 | HP regenerated per second (0 = no regen). |
| `regenDelay` | f32 | 3.0 | Seconds after taking damage before regeneration starts. |
| `isInvulnerable` | bool | false | Immune to damage. |
| `invulnerabilityTime` | f32 | 0.0 | Seconds of invulnerability after taking a hit. |
| `maxShield` | f32 | 0.0 | Maximum shield points (absorbs damage before health). |
| `currentShield` | f32 | 0.0 | Current shield points. |
| `shieldRegenRate` | f32 | 0.0 | Shield regenerated per second. |
| `shieldRegenDelay` | f32 | 5.0 | Seconds after damage before shield regeneration. |
| `onDamageNotify` | Entity | 0 | Entity to notify when damaged. |
| `onDeathNotify` | Entity | 0 | Entity to notify on death. |
| `onHealNotify` | Entity | 0 | Entity to notify on heal. |

Helper methods: `GetHealthPercent()`, `GetShieldPercent()`, `IsFullHealth()`.

#### DamageComponent

Attach to projectiles, hazards, or weapons to deal damage to entities with HealthComponent.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `damage` | f32 | 10.0 | Damage amount. |
| `knockbackForce` | f32 | 0.0 | Knockback force on hit. |
| `destroyOnHit` | bool | true | Destroy this entity after dealing damage. |
| `damageOnce` | bool | true | Only damage each entity once. |
| `damageInterval` | f32 | 0.0 | For continuous damage (lava, poison): seconds between ticks. |
| `type` | enum | Physical | Damage type: `Physical`, `Fire`, `Ice`, `Electric`, `Poison`, `Magic`. |

#### DamageResistanceComponent

Per-type damage multipliers. Pair with HealthComponent to create resistances and weaknesses.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `physicalMult` | f32 | 1.0 | Physical damage multiplier. |
| `fireMult` | f32 | 1.0 | Fire damage multiplier. |
| `iceMult` | f32 | 1.0 | Ice damage multiplier. |
| `electricMult` | f32 | 1.0 | Electric damage multiplier. |
| `poisonMult` | f32 | 1.0 | Poison damage multiplier. |
| `magicMult` | f32 | 1.0 | Magic damage multiplier. |

Values: `1.0` = normal damage, `0.0` = immune, `2.0` = double damage (weakness). Any value is valid.

---

### 5.14 Resource System

#### ResourceComponent

A generic resource bar for stamina, mana, energy, or any depletable value. Integrates with character controllers for sprint and jump costs.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `resourceName` | string | "Stamina" | Display name for the resource. |
| `maxValue` | f32 | 100.0 | Maximum resource capacity. |
| `currentValue` | f32 | 100.0 | Current resource amount. |
| `regenRate` | f32 | 10.0 | Regeneration per second. |
| `regenDelay` | f32 | 1.0 | Seconds after use before regeneration begins. |
| `depleted` | bool | false | True when value reaches 0. Stays true until `depletedThreshold` is reached. |
| `depletedThreshold` | f32 | 20.0 | Must regenerate to this value before `depleted` clears. |
| `sprintCostPerSec` | f32 | 15.0 | Resource consumed per second while sprinting. |
| `jumpCost` | f32 | 20.0 | Resource consumed per jump. |
| `dashCost` | f32 | 25.0 | Resource consumed per dash. |
| `attackCost` | f32 | 0.0 | Resource consumed per attack. |

Methods: `GetPercent()`, `TryConsume(amount)`, `Regenerate(deltaTime)`.

Auto-regeneration happens automatically each frame. Controllers check `TryConsume()` before allowing sprint/jump actions.

---

### 5.15 Save System

#### SaveDataComponent

Marks an entity for persistence in the tiered save system. Controls which transform fields are saved, the persistence tier, custom tags for filtering, and key-value data.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `savePosition` | bool | true | Save entity position. |
| `saveRotation` | bool | true | Save entity rotation. |
| `saveScale` | bool | false | Save entity scale. |
| `saveEnabled` | bool | true | Whether saving is enabled for this entity. |
| `tier` | PersistenceTier | RunState | **SceneState** (per-scene, resets on new run), **RunState** (per-run, resets on new game), or **MetaProgression** (permanent across runs). |
| `tags` | list | [] | Custom string tags for filtering entities during save/load. |
| `customData` | list | [] | Key-value string pairs for game-specific data. |

Methods: `SetData(key, value)`, `GetData(key, defaultValue)`, `HasTag(tag)`.

#### SaveLoadMenuComponent

In-game save/load grid overlay. Add to any entity to enable a pause-menu-style save/load UI during play mode.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `showOnPause` | bool | true | Auto-show when game is paused. |
| `allowManualSave` | bool | true | Show "Save" buttons on slots. |
| `allowManualLoad` | bool | true | Show "Load" buttons on slots. |
| `allowDelete` | bool | true | Show "Delete" buttons on occupied slots. |
| `showAutoSaves` | bool | true | Show auto-save slots (17-19). |
| `columnsPerRow` | i32 | 4 | Number of columns in the slot grid. |
| `headerText` | string | "Save / Load Game" | Header text above the grid. |

#### TieredSaveSystem

The engine provides a 20-slot tiered save system with 3 persistence tiers:

| Tier | Lifetime | Example Data |
|------|----------|-------------|
| **SceneState** | Per-scene within a run. Resets on new run. | Enemies dead, doors opened, chests looted. |
| **RunState** | Per-run. Resets on new game. | Player health, inventory, quest progress. |
| **MetaProgression** | Permanent across all runs. | Unlocks, achievements, meta-currencies. |

**Slot layout:** 17 manual save slots (0-16) + 3 rotating auto-save slots (17-19).

**Auto-save:** Configurable timed interval (default 5 minutes), on scene transition, and on checkpoint calls.

**Meta-progression:** Separate `meta.enjsave` file stores permanent key-value data (float, int, bool, string) that survives across runs and save slot deletion.

**Cloud sync:** Pluggable backends via `ISaveBackend` interface. Built-in: `LocalSaveBackend` (filesystem), `NewgroundsSaveBackend` (wraps NG.io cloud saves), `SteamSaveBackend` (Steam Cloud via ISteamRemoteStorage, requires `ENJIN_STEAM` CMake flag).

**Save Debug Panel:** Open from **View > Tools > Save Debug** to inspect all 20 save slots, view meta-progression key-value tables, configure auto-save, and trigger manual cloud sync.

#### QuestStateComponent

Tracks quest progress for RPG and narrative games.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `questId` | string | "" | Unique quest identifier. |
| `status` | enum | NotStarted | `NotStarted`, `Active`, `Completed`, or `Failed`. |
| `currentObjective` | i32 | 0 | Index of the current objective. |
| `objectiveFlags` | list | [] | Named boolean flags for objective completion. |
| `timeElapsed` | f32 | 0.0 | Time since quest was started. |

#### QuestFlowComponent

Visual node-graph-based quest authoring for complex, branching quests. While `QuestStateComponent` works for simple linear quests, `QuestFlowComponent` provides a visual editor for designing quest flows with conditions, branching, delays, rewards, and events.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `questId` | string | "" | Unique quest identifier. |
| `questTitle` | string | "" | Display name for the quest. |
| `questDescription` | string | "" | Quest description text. |
| `enabled` | bool | true | Whether the quest flow is active. |
| `startNodeId` | NodeId | 0 | Entry point node (created automatically). |
| `graph` | NodeGraphData | -- | Visual graph layout (nodes, pins, links). |
| `nodeMeta` | map | {} | Per-node metadata and properties. |

**Runtime state** (not serialized, reset on play/stop):

| Field | Type | Description |
|-------|------|-------------|
| `status` | enum | `Inactive`, `Active`, `Completed`, or `Failed`. |
| `activeNodes` | set | Nodes currently being processed. |
| `completedNodes` | set | Nodes that have finished. |
| `nodeTimers` | map | Accumulated time for Delay nodes. |
| `nodeCounters` | map | Progress counters for Objective nodes. |

See [Section 21: Quest Flow Editor](#21-quest-flow-editor) for full editor usage.

---

### 5.16 HUD System

#### HUDWidgetComponent

Displays UI elements like health bars, resource bars, labels, and markers.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `type` | enum | HealthBar | `HealthBar`, `ResourceBar`, `Label`, `ObjectiveMarker`, `Crosshair`, `Minimap`. |
| `visible` | bool | true | Widget visibility. |
| `screenSpace` | bool | true | True = fixed screen position; false = world-space billboard. |
| `anchorX` / `anchorY` | f32 | 0.05 | Screen position (normalized 0-1). |
| `width` / `height` | f32 | 0.2 / 0.03 | Widget dimensions (normalized 0-1). |
| `fillColor` | Vector3 | (0.2, 0.8, 0.2) | Bar fill color. |
| `bgColor` | Vector3 | (0.2, 0.2, 0.2) | Bar background color. |
| `textColor` | Vector3 | (1, 1, 1) | Text color. |
| `fontSize` | f32 | 16.0 | Font size for text. |
| `text` | string | "" | Label text content. |
| `sourceEntity` | Entity | 0 | Entity to read data from (0 = self). |
| `bindField` | string | "" | Field to bind: `"health"`, `"stamina"`, `"custom"`. |
| `worldOffset` | Vector3 | (0, 2, 0) | Offset for world-space widgets. |
| `maxRenderDistance` | f32 | 50.0 | Maximum distance for world-space widget visibility. |

---

### 5.17 Cinematic Camera

#### CinematicCameraComponent

Scripted camera sequences with waypoints, easing curves, and hold times.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `waypoints` | list | [] | List of camera waypoints (see below). |
| `loop` | bool | false | Loop the sequence. |
| `autoPlay` | bool | false | Start playing when play mode begins. |
| `hideHUD` | bool | true | Hide HUD widgets during cinematic. |
| `disableInput` | bool | true | Disable player input during cinematic. |
| `onCompleteNotify` | Entity | 0 | Entity to notify when sequence completes. |
| `onWaypointReachNotify` | Entity | 0 | Entity to notify when each waypoint is reached. |

**Waypoint fields:**

| Field | Type | Description |
|-------|------|-------------|
| `position` | Vector3 | Camera position at this waypoint. |
| `lookAt` | Vector3 | Point the camera looks at. |
| `fov` | f32 | Field of view at this waypoint (default: 60). |
| `duration` | f32 | Time in seconds to reach this waypoint from the previous one. |
| `holdTime` | f32 | Time to pause at this waypoint before moving on. |
| `easing` | enum | Interpolation curve: `Linear`, `EaseIn`, `EaseOut`, `EaseInOut`, `SmashCut`. |

---

### 5.18 Object Pooling

#### PoolableComponent

Marks an entity as part of an object pool for efficient reuse (e.g., bullets, particles, enemies).

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `poolId` | string | "" | Pool identifier. |
| `isActive` | bool | false | Whether this pooled object is currently active. |
| `lifetime` | f32 | 0.0 | Auto-return to pool after this many seconds (0 = infinite, manually returned). |
| `activeTime` | f32 | 0.0 | Time since activation. |
| `spawnedBy` | Entity | 0 | Entity that spawned this object. |

---

### 5.19 Footstep Audio

#### FootstepComponent

Surface-aware footstep sounds. Plays different audio clips based on the surface the character is walking on.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `surfaceSounds` | list | [] | List of surface-sound mappings (see below). |
| `defaultWalkSound` | string | "" | Fallback walk sound when no surface matches. |
| `defaultRunSound` | string | "" | Fallback run sound. |
| `walkStepInterval` | f32 | 0.5 | Seconds between steps when walking. |
| `runStepInterval` | f32 | 0.3 | Seconds between steps when running. |
| `volume` | f32 | 0.8 | Footstep volume. |
| `pitchVariance` | f32 | 0.1 | Random pitch variation for natural sound. |

**SurfaceSound fields:**

| Field | Type | Description |
|-------|------|-------------|
| `surfaceTag` | string | Surface identifier (e.g., "grass", "stone", "wood", "metal", "water"). |
| `walkSound` | string | Audio clip path for walking. |
| `runSound` | string | Audio clip path for running. |
| `volumeScale` | f32 | Per-surface volume multiplier. |

---

### 5.20 Tags and Organization

#### TagComponent

String-based tags for filtering and identification. An entity can have multiple tags.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `tags` | list | [] | List of tag strings. |

Methods: `HasTag(tag)`, `AddTag(tag)`, `RemoveTag(tag)`.

#### LayerComponent

Numeric layer assignment for collision filtering and rendering.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `layer` | u32 | 0 | Layer number. |
| `layerName` | string | "" | Human-readable layer name. |

---

### 5.21 Terrain Components

#### TerrainComponent

A grid-based 3D heightmap terrain with multi-layer texture splatting. The terrain mesh is auto-regenerated by the RenderSystem whenever `meshDirty` is set.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `gridWidth` | u32 | 64 | Number of grid cells along X. |
| `gridHeight` | u32 | 64 | Number of grid cells along Z. |
| `cellSize` | f32 | 1.0 | World-space size of each grid cell. |
| `maxHeight` | f32 | 20.0 | Maximum terrain height. |
| `heightmap` | vector\<f32\> | [] | Height values (`gridWidth * gridHeight`). |
| `splatmap` | vector\<f32\> | [] | Texture blend weights (`gridWidth * gridHeight * 4`, RGBA per cell). |
| `layers[0..3]` | TextureLayer | -- | Four texture layers, each with `texturePath` and `tileScale`. |
| `meshDirty` | bool | true | When true, the RenderSystem regenerates the terrain mesh. |

**Methods:**

| Method | Description |
|--------|-------------|
| `GetHeight(x, z)` | Returns the height at grid coordinates (x, z). Returns 0 if out of bounds. |
| `SetHeight(x, z, h)` | Sets the height at (x, z) and sets `meshDirty = true`. |
| `InitializeFlat(height)` | Fills the heightmap with a uniform height and initializes the splatmap (layer 0 = 100%). |

#### Terrain2DComponent

A polyline-based 2D terrain for side-scrolling games. Control points define the surface profile; a filled mesh is generated below the surface to a configurable depth.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `controlPoints` | vector\<Vector2\> | [] | XY positions defining the terrain surface, auto-sorted by X. |
| `depth` | f32 | 5.0 | Fill depth below the surface. |
| `uvScale` | f32 | 1.0 | UV coordinate scaling. |
| `texturePath` | string | "" | Surface texture path. |
| `autoColliders` | bool | true | Automatically generate collision from the terrain shape. |
| `meshDirty` | bool | true | When true, the mesh is regenerated. |

**Methods:**

| Method | Description |
|--------|-------------|
| `AddPoint(p)` | Adds a control point and re-sorts by X. |
| `SortPoints()` | Sorts control points by X coordinate. |

---

### 5.22 Animation

#### SkeletonComponent

Stores the bone hierarchy for skinned meshes. Populated automatically when importing glTF models with skeletal data.

#### AnimatorComponent

Drives skeletal animation playback with a state machine. Supports animation blending and transitions.

---

## 6. Scene Management

### Scene Files

Scenes are saved as `.enjin` JSON files. Use `Ctrl+S` to save and `Ctrl+O` to open scenes.

### Project Files

A project is defined by a `.enjinproject` JSON manifest that contains:

- The project name.
- A list of all scenes in the project.
- Build indices for each scene (determining load order in builds).
- The designated start scene.

### Scene List Panel

The **Scene List** panel (View > Scene List) provides multi-scene project management:

- **Add** new scenes to the project.
- **Reorder** scenes by dragging.
- **Load** a scene by clicking on it.
- **Set start scene** for the built game.

### Scene Transitions

When switching between scenes at runtime, four transition types are available:

| Transition | Description |
|------------|-------------|
| **Instant** | Immediate cut to the new scene. |
| **Fade Black** | Fade to black, then fade in to the new scene. |
| **Fade White** | Fade to white, then fade in to the new scene. |
| **Cross Fade** | Blend the old scene into the new scene. |

All transitions except Instant have a configurable duration.

### Additive Loading

Scenes can be loaded additively (on top of the existing scene) for layering, such as loading a UI scene over a gameplay scene.

---

## 7. Play Mode

### Controls

The **Game View** panel contains three buttons for controlling play mode:

| Button | Action |
|--------|--------|
| **Play** | Enter play mode. Activates controllers, physics, and gameplay systems. Saves the current editor state for restoration. |
| **Pause** | Freeze game simulation. The scene remains in play mode but time stops advancing. |
| **Stop** | Exit play mode, compute a diff of all entity changes, and restore the editor state. If changes were made, a **Play Mode Changes** dialog appears allowing you to cherry-pick which changes to keep. |

### Behavior During Play

When play mode is active:

- **Editor input is locked.** Panels receive `NoInputs` flags to prevent accidental edits. Keyboard shortcuts are suppressed.
- **The game camera** renders in the Game View panel using the highest-priority active `CameraComponent`.
- **The editor camera** continues to be visible in the main viewport so you can observe the scene from any angle.

### Active Systems During Play

The following systems are updated each frame during play mode:

- **ControllerSystem** -- processes all character controllers (movement, jumping, camera orbit).
- **SimplePhysics** -- rigidbody simulation, collision detection, gravity.
- **ScriptSystem** -- AngelScript lifecycle callbacks (OnUpdate, OnFixedUpdate, OnLateUpdate).
- **CoroutineScheduler** -- resumes suspended script coroutines.
- **FootstepSystem** -- surface-aware footstep audio.
- **QuestSystem** -- quest state tracking and objective updates.
- **Quest Flow** -- visual quest graph processing (advances active `QuestFlowComponent` entities).
- **HUDSystem** -- HUD widget rendering and data binding.
- **CinematicSystem** -- cinematic camera sequence playback.
- **ObjectPool** -- object lifetime management and recycling.
- **EntityEventBus** -- deferred event dispatch for decoupled entity communication.
- **ScriptEventBus** -- script-to-script event communication.
- **Resource regeneration** -- ResourceComponent auto-regen each frame.
- **FlowerSystem** -- flower/vegetation animation.
- **TieredSaveSystem** -- auto-save timer, play time tracking, checkpoint management.
- **VisualScriptSystem** -- visual script graph execution (includes save/load/checkpoint/meta nodes).
- **BehaviorTreeSystem** -- AI behavior tree tick execution.
- **NetworkSystem** -- LAN multiplayer state sync (if connected).

### Play Mode Diff Dialog

When you press **Stop**, the engine compares the scene state before and after play. If any entities were created, deleted, or modified, a **Play Mode Changes** dialog appears:

- **Tree view** of all changed entities, expandable to component and property level
- **Color-coded** actions: green (Created), red (Deleted), yellow (Modified)
- **Checkboxes** at entity, component, and property level for selective apply
- **Apply Selected** — re-applies checked changes to the restored scene
- **Discard All** — closes dialog, no changes kept

---

## 8. Effects and Environment

### Skybox

Configure the skybox from the **Rendering** panel (View > Rendering > Rendering).

#### Skybox Types

| Type | Description |
|------|-------------|
| **None** | No skybox rendered. |
| **Procedural** | Gradient sky generated from top, horizon, and bottom colors plus sun direction. |
| **Solid Color** | Single flat color fills the background. |
| **Cubemap** | Six-face cubemap textures for realistic sky imagery. |

#### Procedural Presets

Five built-in presets that set colors and sun direction:

| Preset | Description |
|--------|-------------|
| **Midday** | Bright blue sky with high sun. |
| **Sunset** | Orange/red horizon with low sun. |
| **Dawn** | Soft pink/purple with early sun. |
| **Night** | Dark blue/black sky. |
| **Overcast** | Gray, even lighting. |

#### Cubemap Configuration

When using the Cubemap type, provide paths to six face textures:

| Face | Direction |
|------|-----------|
| 1 | Right (+X) |
| 2 | Left (-X) |
| 3 | Top (+Y) |
| 4 | Bottom (-Y) |
| 5 | Front (+Z) |
| 6 | Back (-Z) |

#### Rotation

All skybox types support a **Y-axis rotation** slider (0-360 degrees) to orient the sky.

### Weather

Configure weather effects from the **Project Settings** panel (View > Settings > Project Settings > Environment).

| Effect | Description |
|--------|-------------|
| **Rain** | Falling rain particles with configurable intensity. |
| **Snow** | Falling snowflakes. |
| **Fog** | Distance-based fog with color and density. |
| **Storm** | Rain with toggleable lightning flashes. |

Per-zone weather overrides are possible via `WeatherZoneComponent` on entities.

### Water

3D water plane rendering with:

- **Gerstner waves** -- realistic wave simulation with configurable amplitude and frequency.
- **Shore foam** -- foam effect at water edges.
- **Freeze system** -- water can freeze over time.
- **Ocean mode** -- extended water plane for open-water scenes.

### Post-Processing

Available from the **Post Processing** panel:

| Effect | Parameters |
|--------|------------|
| **Bloom** | Threshold, intensity, radius. |
| **Vignette** | Intensity, smoothness. |
| **Color Grading** | Exposure, contrast, saturation, temperature. |
| **FXAA** | Anti-aliasing toggle. |
| **Film Grain** | Grain intensity. |

#### Screen-Space Effects

Five raster-tier screen-space effects run in the post-process shader using only the scene color and depth buffer. They provide ambient occlusion, contact shadows, volumetric light, caustics, and fog without requiring ray tracing hardware. All are configurable from the Post Processing panel and persist per-scene.

| Effect | Description | Key Parameters |
|--------|-------------|---------------|
| **God Rays** | Screen-space radial blur from the projected sun position (GPU Gems 3 style). | Intensity, decay, density, samples (default 64), weight. |
| **SSAO** | Depth-only hemisphere-sampled ambient occlusion with reconstructed normals. | Radius, intensity, bias, samples (default 16). |
| **Contact Shadows** | Screen-space ray march toward the light source in the depth buffer for fine shadow detail. | Ray length, march steps (default 16), intensity. |
| **Fake Caustics** | Procedural animated Voronoi pattern projected below a configurable water plane Y height. | Intensity, scale, speed, water Y. |
| **Fog Shafts** | Noisy volumetric-look fog via ray marching through the depth buffer. | Intensity, density, decay, samples (default 16), max distance. |

**Effect chain order** (all run in HDR before tone mapping): SSAO (multiply) -> Contact Shadows (multiply) -> Caustics (additive) -> God Rays (additive) -> Fog Shafts (blend) -> DoF -> Tilt-Shift -> Tone Mapping.

**Performance:** All sample counts are tunable. Estimated total cost is ~3 ms at 1080p on a GTX 1060-class GPU with default settings. Effects can be enabled/disabled individually.

**PostProcessVolume blending:** All 5 effects support spatial blending via `PostProcessVolumeComponent` override bits (19-23).

### Retro Effects

#### Per-Material

These are set on individual `MaterialComponent` instances:

- **Flat shading** -- faceted, low-poly look.
- **Affine texturing** -- PS1-style texture warping without perspective correction.
- **Vertex snapping** -- PS1-style vertex jittering with configurable grid resolution (80-320).
- **Stipple transparency** -- dithered transparency pattern.

#### Post-Process

These are global effects from the **Retro Effects** panel (View > Rendering > Retro Effects):

- **CRT scanlines** -- horizontal scanline overlay.
- **Dithering** -- ordered dithering pattern.
- **Color quantization** -- reduce color palette.
- **Resolution downscaling** -- render at lower resolution for a retro look.

### World Time

A day/night cycle system with configurable speed. As time advances, the sun position and sky colors change accordingly. Configure from Project Settings > Environment.

### Wind

A global wind system that affects:

- Weather particles (rain, snow direction and speed).
- Vegetation sway (trees, shrubs, grass).
- Instanced grass rendering with wind-driven animation.

---

## 9. Accessibility

Enjin includes comprehensive accessibility features, configurable from the **Editor Settings** panel (View > Settings > Editor Settings). Settings are saved persistently to disk (JSON format in `%APPDATA%/enjin/` on Windows).

### Editor Themes

Eleven themes are available, including four standard themes and seven retro console-inspired themes:

| Theme | Description |
|-------|-------------|
| **Dark** | Default dark theme with sage green accents. |
| **Light** | Light background theme. |
| **High Contrast Dark** | High-contrast dark for low vision. |
| **High Contrast Light** | High-contrast light for low vision. |
| **SNES** | Super Nintendo inspired -- deep purple and indigo tones with lavender accents. |
| **PS2** | PlayStation 2 inspired -- dark blue with PS2-signature blue accents. |
| **Xbox** | Xbox inspired -- dark charcoal with Xbox green accents. |
| **Dreamcast** | Dreamcast inspired -- warm grey-blues with Dreamcast orange-red accents. |
| **Sega Saturn** | Sega Saturn inspired -- dark blue-greys with Saturn blue accents. |
| **GBA** | Game Boy Advance inspired -- dark teal-grey with GBA purple accents. |
| **DS** | Nintendo DS inspired -- slate grey with DS red accents. |

### Customizable Accent Colors

Beyond the built-in themes, you can fully customize the editor's accent colors from **Editor Settings > Accent Colors**:

- **Enable Custom Colors** checkbox activates per-color overrides.
- 11 accent color fields are available: Button, Button Hover, Button Active, Check Mark, Slider Grab, Slider Grab Active, Resize Grip, Text Selected, Drag Drop Target, Tab Active, Tab Hovered.
- Each field has a color picker (with alpha).
- **Reset to Defaults** restores the default accent colors for the current theme.
- Custom accent colors are saved persistently and apply to any base theme.

### Colorblind Modes

GPU-accelerated colorblind correction via Daltonization in the post-process shader. Eight modes are supported:

| Mode | Description |
|------|-------------|
| **Off** | No correction. |
| **Protanopia** | Red-blind (full). |
| **Protanomaly** | Red-weak (partial). |
| **Deuteranopia** | Green-blind (full). |
| **Deuteranomaly** | Green-weak (partial). |
| **Tritanopia** | Blue-blind (full). |
| **Tritanomaly** | Blue-weak (partial). |
| **Achromatopsia** | Total color blindness. |

A strength slider (0-1) controls the intensity of the correction.

### Remappable Input

The `InputActionMap` system provides semantic game actions that can be rebound:

- Actions are mapped to keys/buttons (not hard-coded).
- **Hold/Toggle modes** for sprint and crouch.
- **One-handed presets**: left-hand only, right-hand only, or gamepad only.
- Input mappings are saved as JSON and persist between sessions.

**18 Game Actions:** MoveForward, MoveBack, MoveLeft, MoveRight, Jump, Sprint, Crouch, Dash, Interact, Attack, Block, Pause, LookUp, LookDown, LookLeft, LookRight, CameraZoomIn, CameraZoomOut.

**AngelScript API:** 22 bindings are available for scripting input actions at runtime -- see the Scripting API reference for the full `InputAction_*` function list (query, sensitivity, toggle, rebinding, display, presets).

### Reduced Motion

- **Weather particle reduction** -- fewer particles for rain, snow, and effects.
- **Disable head bob** -- removes the camera bob from FirstPersonController.
- **Disable screen shake** -- suppresses all screen shake effects.
- **Disable FOV effects** -- prevents FOV changes (sprint zoom, etc.).

### Subtitles and Captions

The `SubtitleSystem` provides an overlay for dialogue and environmental audio:

- Configurable **font size** (16-48 px).
- Adjustable **background opacity** for readability.
- **Speaker names** toggle.
- **Direction indicators** showing where sound is coming from.
- Separate toggle for **closed captions** (environmental sounds).

### Content Warnings

The `ContentWarningSystem` provides per-scene content flags:

- Scenes can be tagged with content warning flags.
- A dismissable overlay appears before the scene loads.
- Players can acknowledge warnings before continuing.

### Quick Presets

Four one-click presets that configure multiple accessibility settings at once:

| Preset | What it does |
|--------|-------------|
| **Low Vision** | Large UI, high contrast theme, large subtitles. |
| **Motor Impaired** | Toggle modes for sprint/crouch, reduced input requirements. |
| **Photosensitive** | Reduced motion, disabled screen shake, disabled FOV effects. |
| **Reset All** | Restores all settings to defaults. |

### UICanvas Focus Navigation

UICanvas supports full keyboard and gamepad navigation for in-game UI, enabling accessible menu control without a mouse:

| Input | Action |
|-------|--------|
| **Tab / Shift+Tab** | Move focus forward/backward through focusable elements (ordered by `tabOrder`). |
| **Arrow Keys / D-Pad** | Navigate between elements with key repeat support. |
| **Enter / Space / Gamepad A** | Activate the focused element (button press, checkbox toggle). |
| **Left / Right** | Adjust slider values on focused slider elements. |

- **Focus indicators** are rendered as an outset rounded-rect border around the focused element, using the theme's `inputFocused` color or a per-element `focusColor` override.
- Elements can be marked as focusable/unfocusable via the `focusable` flag and ordered with `tabOrder` (0 = auto from element order).
- AngelScript bindings: `UI_SetFocus()`, `UI_ClearFocus()`, `UI_GetFocusedElement()`, `UI_IsFocused()`, `UI_SetTabOrder()`, `UI_SetFocusable()`.

### Mouse Input Settings

| Setting | Description |
|---------|-------------|
| **Raw mouse input** | Bypasses OS mouse acceleration for 1:1 input (toggle, default: on). |
| **Mouse smoothing** | Temporal smoothing for mouse movement (0.0 = none, 1.0 = heavy). |
| **Mouse sensitivity** | Global mouse sensitivity multiplier. |

---

## 10. Building and Distributing Games

Enjin includes a complete build pipeline for packaging your game into a standalone, distributable executable.

### Build Dialog

Open the build dialog from the editor menu. Configure:

| Setting | Description |
|---------|-------------|
| **Project Path** | Path to the `.enjinproject` file. |
| **Output Directory** | Where to write the built game. |
| **Window Title** | Title for the game window. |
| **Window Width / Height** | Default window resolution (e.g., 1280x720). |
| **Fullscreen** | Whether the game launches in fullscreen. |
| **Build Key** | Obfuscation key for the asset pack (a default is used if empty). |

### Build Pipeline Phases

When you press Build, the pipeline executes these phases:

1. **Scan Project** -- Reads the `.enjinproject` manifest and lists all scenes.
2. **Validate Assets** -- Parses each scene JSON, collects all referenced assets (textures, models, scripts), and verifies they exist on disk.
3. **Pack Assets** -- Compresses and packs all assets into a `.enjpak` archive.
4. **Copy Player** -- Copies the standalone `EnjinPlayer` executable to the output directory.
5. **Write Build Manifest** -- Writes `_build/manifest.json` into the pack with window title, resolution, fullscreen flag, and start scene.
6. **Verify Build** -- Reads back the `.enjpak` archive and verifies all CRC32 checksums for data integrity.

A progress callback displays the current phase and completion percentage in the editor.

### .enjpak Archive Format

The `.enjpak` format is a custom archive:

- **Magic header**: `ENJPAK10`
- **Compression**: Per-file data compression.
- **XOR obfuscation**: Per-file obfuscation with a configurable key.
- **CRC32 integrity**: Each file has a checksum verified on read.
- **Default pack key**: `enjin_default_pack_key_2025` (used when no custom key is specified).

### Standalone Player

The built game consists of two files:

| File | Description |
|------|-------------|
| `EnjinPlayer.exe` | Standalone executable (no editor, no ImGui). |
| `game.enjpak` | Packed asset archive. |

The player executable:

1. Loads `game.enjpak` from its own directory.
2. Reads the build manifest for window configuration (title, resolution, fullscreen, start scene).
3. Initializes Vulkan and runs the game loop.
4. No editor UI is loaded -- only game systems run.

To distribute: ship both files together. The player expects `game.enjpak` in the same directory.

### Distribution

Enjin supports multiple distribution formats via CMake/CPack:

| Format | Platform | Command |
|--------|----------|---------|
| **ZIP** | Windows | `cd build && cpack -G ZIP` |
| **NSIS Installer** | Windows | `cd build && cpack -G NSIS` |
| **TGZ** | macOS/Linux | `cd build && cpack -G TGZ` |
| **DEB** | Linux | `cd build && cpack -G DEB` |

The **NSIS installer** provides:
- Start Menu shortcuts for Enjin Editor and Enjin Player.
- Desktop shortcut for the Editor.
- File associations (`.enjin` opens in Editor, `.enjpak` opens in Player).
- Standard Windows uninstaller via Add/Remove Programs.
- Default install to `Program Files/Enjin`.

To create both a ZIP and installer in one step: `cd build && cpack` (uses all configured generators).

---

## 11. Scripting (AngelScript)

Enjin uses **AngelScript** as its scripting language. Scripts are attached to entities via `ScriptComponent` and receive lifecycle callbacks similar to Unity's MonoBehaviour.

### ScriptComponent

Each entity can have multiple scripts attached. Each script attachment specifies:

- **Script path**: e.g., `scripts/PlayerController.as`
- **Class name**: the AngelScript class to instantiate.
- **Properties**: exposed values editable in the Inspector (see below).
- **Enabled**: toggle script on/off.

### Lifecycle Callbacks

Scripts can implement any of these methods:

| Method | When it is called |
|--------|-------------------|
| `void OnCreate()` | When the script instance is first created (entering play mode). |
| `void OnStart()` | Once, on the first frame after creation. |
| `void OnUpdate(float dt)` | Every frame. |
| `void OnFixedUpdate(float dt)` | At a fixed timestep (60 Hz). |
| `void OnLateUpdate(float dt)` | After all OnUpdate calls. |
| `void OnDestroy()` | When the entity is destroyed or play mode stops. |
| `void OnEnable()` | When the script is enabled. |
| `void OnDisable()` | When the script is disabled. |
| `void OnCollisionEnter(Entity other)` | When a collision begins. |
| `void OnCollisionStay(Entity other)` | While a collision persists. |
| `void OnCollisionExit(Entity other)` | When a collision ends. |
| `void OnTriggerEnter(Entity other)` | When entering a trigger zone. |
| `void OnTriggerExit(Entity other)` | When leaving a trigger zone. |

### Execution Order Per Frame

1. Hot reload check (polls for changed script files every 30 frames).
2. New scripts: `CreateInstance` -> `OnCreate` -> `OnEnable`.
3. Unstarted scripts: `OnStart`.
4. Fixed timestep loop: `OnFixedUpdate(fixedDt)`.
5. `OnUpdate(deltaTime)`.
6. Coroutine scheduler update.
7. `OnLateUpdate(deltaTime)`.

### Script Properties

Properties marked with metadata attributes are exposed in the editor Inspector. Supported types:

- `Int`, `Float`, `Bool`, `String`
- `Vector2`, `Vector3`, `Vector4`
- `Entity` (entity reference)
- `Enum`

Properties can have range hints and tooltips defined in metadata.

### Engine Bindings

Scripts have access to engine functionality through registered bindings:

| Binding Category | Description |
|-----------------|-------------|
| **Math types** | Vector2, Vector3, Vector4, Quaternion, Matrix4. |
| **Entity types** | Entity handles, component access. |
| **Input** | Key/button state, mouse position. |
| **Physics** | Raycasting, collision queries. |
| **Audio** | Play/stop sounds, set volume. |
| **Scene** | Load scenes, get entities by name/tag. |
| **Time** | Delta time, total time, time scale. |
| **Debug** | Log messages, draw debug lines. |

### Coroutines

Scripts can use coroutines for time-based logic:

| Yield Function | Description |
|----------------|-------------|
| `WaitForSeconds(float seconds)` | Resume after a time delay. |
| `WaitForFrames(int frames)` | Resume after a number of frames. |
| `WaitForEndOfFrame()` | Resume at the end of the current frame. |

### ScriptEventBus

Scripts can communicate with each other through a named event system:

- **Listen(eventName, callback)** -- register a listener for events with a given name.
- **Send(eventName, data)** -- dispatch an event to all listeners for that name.
- **Broadcast(data)** -- dispatch to all listeners regardless of name.
- **EventData** payload supports floats, ints, strings, and entity references as key-value pairs.

### EntityEventBus (C++)

A C++ event bus for decoupled entity communication:

- Supports named events with key-value payloads (floats, ints, strings, entity references).
- **Send** for immediate dispatch, **SendDeferred** for end-of-frame dispatch.
- **Broadcast** sends to all listeners.
- Listeners are automatically cleaned up when entities are destroyed.

### Hot Reload

During play mode, the script engine polls the script directory for file changes every 30 frames. Modified scripts are automatically recompiled, and running instances are updated without stopping play mode.

---

## 12. Procedural Generation

Enjin includes a procedural level generation system based on room prefabs.

### Room Prefab System

Rooms are defined as JSON files with the following properties:

- **Size** -- room dimensions.
- **Connection points** -- doorways/openings where rooms can connect to each other.
- **Tags** -- categorization tags (e.g., "start", "boss", "corridor", "treasure").
- **Weight** -- probability weight for random selection (higher weight = more likely to be chosen).

### Generation Parameters

| Parameter | Description |
|-----------|-------------|
| **Seed** | Random seed for reproducible generation. |
| **Room count** | Target number of rooms to generate. |
| **Room prefab set** | Which set of JSON room definitions to use. |

### Workflow

1. Create room prefab JSON files in your project.
2. Define connection points and weights.
3. Call `LevelGenerator` with a seed and room count.
4. The generator places rooms by matching connection points and using weighted random selection.
5. The result is a connected level layout.

---

## 13. Splitscreen

Enjin supports splitscreen rendering for local multiplayer games.

### Supported Configurations

| Mode | Layout |
|------|--------|
| **2-player** | Screen split horizontally (top/bottom) or vertically (left/right). |
| **4-player** | Screen divided into four quadrants. |

### How It Works

- Each player's camera uses `CameraComponent` viewport fields (`viewportX`, `viewportY`, `viewportWidth`, `viewportHeight`) to define its portion of the screen.
- Per-viewport uniform buffers ensure each camera renders with its own view/projection matrices.
- Character controllers use the `gamepadIndex` field (0-3) to assign each player to a different gamepad.

### Example: 4-Player Setup

The **4P Racing** template demonstrates splitscreen with four viewport cameras and four vehicle controllers, each bound to a different gamepad index.

For a 4-player quadrant layout:

| Player | viewportX | viewportY | viewportWidth | viewportHeight |
|--------|-----------|-----------|---------------|----------------|
| 1 | 0.0 | 0.0 | 0.5 | 0.5 |
| 2 | 0.5 | 0.0 | 0.5 | 0.5 |
| 3 | 0.0 | 0.5 | 0.5 | 0.5 |
| 4 | 0.5 | 0.5 | 0.5 | 0.5 |

---

## 14. Physics Joints and Constraints

Enjin's constraint solver provides 6 joint types for connecting entities with physical relationships.

### Joint Types

| Joint | Description | Key Properties |
|-------|-------------|----------------|
| **Distance** | Maintains fixed distance between two entities | `restDistance`, `stiffness`, `tolerance` |
| **Hinge** | Rotation around one axis (like a door) | `axis`, `lowerLimit`/`upperLimit` (degrees), motor |
| **Ball Socket** | Free rotation around a point (like a shoulder) | `coneAngleLimit`, twist limits |
| **Spring** | Elastic connection between entities | `springConstant`, `dampingCoefficient`, min/max distance |
| **Fixed** | Rigid connection (breakable under force) | Stores relative position/rotation |
| **Slider** | Translation along one axis (like a piston) | `slideAxis`, limits, motor |

### Common Joint Properties

All joints share:
- **entityA / entityB** (u64) - The two connected entities
- **anchorA / anchorB** (Vector3) - Local-space anchor points on each entity
- **breakable** (bool) - Whether the joint can break under force
- **breakForce** (f32) - Force threshold for breaking
- **currentStress** (f32) - Runtime stress value (read-only in inspector)

### Ragdoll

The `RagdollComponent` maps skeleton bones to physics joints:
- Each `BoneJoint` defines a bone name, joint type, mass, and collider radius
- `blendWeight` (0.0-1.0) transitions between animation and ragdoll
- `autoDisableAfterSettle` stops simulation when the ragdoll settles

### Constraint Solver

The solver runs 8 iterations per frame (configurable) using sequential impulse with warm starting. Baumgarte stabilization prevents position drift over time.

---

## 15. Profiler and Debug Tools

### Built-in Profiler

Open via **View > Profiler** in the editor menu.

The profiler displays:
- **FPS** and average frame time
- **Frame time graph** (240-frame rolling window)
- **System breakdown**: Render, Physics, Scripting, ECS, Audio (progress bars showing % of frame)
- **Counters**: Draw calls, entity count, triangle count, memory usage
- **Detailed scopes**: Expandable table showing per-scope last/avg/max times and call counts

### Adding Profile Scopes

In C++ code, use the `ENJIN_PROFILE_SCOPE` macro:

```cpp
#include "Enjin/Debug/Profiler.h"

void MySystem::Update(f32 deltaTime) {
    ENJIN_PROFILE_SCOPE("MySystem::Update");
    // ... your code
}
```

Use `ENJIN_PROFILE_FUNCTION()` to automatically use the function name.

---

## 16. Plugin System

Enjin supports dynamic plugins loaded at runtime.

### Creating a Plugin

Implement the `IPlugin` interface:

```cpp
class MyPlugin : public Enjin::Plugin::IPlugin {
public:
    const char* GetName() override { return "My Plugin"; }
    const char* GetVersion() override { return "1.0"; }
    void OnLoad() override { /* init */ }
    void OnUnload() override { /* cleanup */ }
    void OnUpdate(f32 deltaTime) override { /* per-frame */ }
};
```

### Plugin Manifest

Create a `plugin.json` alongside the shared library:

```json
{
    "name": "My Plugin",
    "version": "1.0",
    "dependencies": []
}
```

### Editor Panel

The Plugin Manager panel (**View > Plugins**) shows loaded plugins, their status, and provides load/unload controls.

---

## 17. Animation Timeline

The timeline system allows keyframing entity properties over time.

### Track Types

| Track Type | Description |
|------------|-------------|
| **Property** | Animate any component field (position, rotation, scale, material properties) |
| **Event** | Fire callbacks at specific timestamps |
| **Animation** | Play or blend skeletal animations |

### TimelineComponent

Add a `TimelineComponent` to any entity. Configure tracks with keyframes, then control playback:
- **Play / Pause / Stop** - Playback controls
- **Loop** - Restart when reaching the end
- **Ping-Pong** - Play forward then backward
- **Speed** - Playback rate multiplier

### Easing Functions

Keyframes support: `Linear`, `EaseIn`, `EaseOut`, `EaseInOut`, `Step`.

---

## 18. Level Streaming

For large worlds, the streaming system loads and unloads chunks based on camera distance.

### StreamingChunk

Each chunk defines:
- **center** and **halfExtents** - Spatial bounds
- **loadDistance** - Distance at which the chunk starts loading
- **unloadDistance** - Distance at which the chunk is released
- **scenePath** - Path to the `.enjin` scene file for this chunk

### Components

- **StreamingVolumeComponent** - Placed on entities to define chunk boundaries
- **StreamingPortalComponent** - Connects two chunks (doorways, corridors) for seamless transitions

### Priority System

Chunks are loaded in priority order: `Critical` > `High` > `Normal` > `Low`. The system limits concurrent loads to prevent frame drops.

### Debug Overlay

Enable the streaming debug overlay to visualize chunk states (Unloaded, Loading, Loaded, Unloading) and the load queue.

---

## 19. Terrain Editing

Enjin provides interactive terrain sculpting directly in the editor viewport for both 3D heightmap terrain and 2D polyline terrain.

### 3D Terrain Workflow

1. Create an entity and add a **TerrainComponent** via the Inspector's Add Component button.
2. Set the desired **Grid Width**, **Grid Height**, and **Cell Size** in the Inspector.
3. Click **Initialize Flat** to allocate the heightmap.
4. Enable **Edit Mode** in the Terrain Brush section of the Inspector.
5. Select a **Brush Mode** and adjust parameters.
6. Click and drag on the terrain in the viewport to sculpt.

### Brush Modes

| Mode | Effect |
|------|--------|
| **Raise** | Increases height under the brush. Hold the mouse to continuously raise terrain. |
| **Lower** | Decreases height under the brush. |
| **Flatten** | Blends terrain height toward a target value (set via the Flatten Height slider). |
| **Smooth** | Blends each cell toward its neighbor average, reducing sharp peaks and valleys. |
| **Paint** | Paints splatmap weights for the selected texture layer (0-3). Weights are automatically normalized so they sum to 1.0. |

### Brush Parameters

| Parameter | Range | Description |
|-----------|-------|-------------|
| **Radius** | 0.5 - 50.0 | Brush size in world units. |
| **Strength** | 0.01 - 10.0 | Intensity of the brush effect per frame. |
| **Falloff** | 0.0 - 1.0 | How quickly the effect fades from center to edge. 0 = uniform, 1 = full smoothstep falloff. |
| **Flatten Height** | 0.0 - maxHeight | Target height for Flatten mode. |
| **Paint Layer** | 0 - 3 | Which splatmap layer to paint (corresponds to the 4 texture layers). |

### Brush Cursor Feedback

When Edit Mode is active and the mouse is over the terrain, the Inspector displays the brush world coordinates: `Brush: (X, Y, Z)`. This helps with precise placement.

### How Raycasting Works

The editor casts a ray from the mouse position into the scene using `ScenePicker::ScreenToRay()`. The ray is marched along in steps of half a cell size, checking at each step whether the ray Y is below the interpolated terrain height. When an intersection is found, a binary search refines the hit point to sub-cell accuracy.

### Texture Layers

Each terrain supports 4 texture layers. Configure them in the Inspector under **Texture Layers**:

- **Load Texture** -- select a PNG/JPG/BMP/TGA file for the layer.
- **Tile Scale** -- how many times the texture repeats across the terrain.

Use the **Paint** brush mode to blend between layers. By default, layer 0 covers the entire terrain at 100%.

### 2D Terrain Workflow

1. Create an entity and add a **Terrain2DComponent**.
2. Use the Inspector to add control points (click **Add Point**), or edit existing point positions with the drag fields.
3. Enable **Edit Mode (Drag Points)** in the Inspector.
4. Click near a control point in the viewport and drag to reposition it.
5. Points are automatically re-sorted by X after each move.

### Behavior During Edit Mode

While terrain Edit Mode is active:

- **Viewport picking is disabled.** Left-click in the viewport sculpts terrain instead of selecting entities. This prevents accidentally deselecting the terrain entity while painting.
- **Gizmos are unaffected.** The terrain entity's transform gizmo still works normally.
- **Disable Edit Mode** by unchecking the Edit Mode checkbox to return to normal entity selection.

---

## 20. AI and Pathfinding

Enjin includes AI behavior systems and navigation mesh pathfinding for NPC and enemy logic.

### AI Controller

Add an `AIControllerComponent` to an entity for state-based AI behavior. The AI cycles through states based on target detection:

| State | Behavior |
|-------|----------|
| **Idle** | Stands still, scans for targets within detection range and field of view. |
| **Patrol** | Moves between patrol point positions in order, waiting at each for a configurable duration. |
| **Chase** | Moves toward the target entity. Transitions to Attack when within attack range. |
| **Attack** | Deals damage at a configurable rate and amount. Returns to Chase if target moves out of range. |
| **Flee** | Moves away from the target (triggered by game logic or low health). |
| **Dead** | No longer active. |

### Pathfinding (A*)

The navmesh system provides A* pathfinding:

1. **Navmesh generation** -- automatically builds a navigation mesh from scene geometry.
2. **Path queries** -- find a path from point A to point B on the navmesh.
3. **Debug visualization** -- toggle navmesh and path rendering in the editor.

AI entities use pathfinding results to navigate around obstacles instead of moving in straight lines.

### Support Components

| Component | Purpose |
|-----------|---------|
| `FollowTargetComponent` | Smooth following of another entity with configurable distance and offset. |
| `LookAtTargetComponent` | Rotates to face a target entity or world position with angle limits. |
| `WaypointComponent` | Defines linked waypoint chains for patrol routes. |

---

## 21. Quest Flow Editor

The Quest Flow Editor is a visual node-graph tool for designing complex, branching quests. It builds on the same `NodeGraphEditor` framework used by the Behavior Tree and Visual Script editors.

### Opening the Editor

1. Select an entity and add the **Quest Flow** component from **Add Component > Gameplay > Quest Flow**.
2. In the inspector, click **Open Editor** to open the Quest Flow panel.
3. Alternatively, open it from **View > Tools > Quest Flow**.

The editor auto-targets the selected entity when it has a `QuestFlowComponent`.

### Node Types

The Quest Flow system provides 8 node types organized into 3 categories:

#### Flow Nodes

| Node | Color | Pins | Description |
|------|-------|------|-------------|
| **Start** | Green | 1 output (Next) | Entry point. Created automatically. Cannot be deleted. Activates when the quest begins. |
| **End** | Red | 1 input (In) | Terminates the quest. Set `endStatus` to `completed` or `failed`. |
| **Delay** | Grey | 1 input (In), 1 output (Next) | Waits for `duration` seconds before continuing. |
| **Event** | Teal | 1 input (In), 1 output (Next) | Fires a named event string (`eventName`) for script integration, then continues. |

#### Objective Nodes

| Node | Color | Pins | Description |
|------|-------|------|-------------|
| **Objective** | Blue | 1 input (In), 1 output (Done) | A task to complete. Tracks progress via `nodeCounters` against `targetCount`. |

Objective properties:

| Property | Description |
|----------|-------------|
| `description` | Human-readable objective text. |
| `objectiveType` | `kill`, `collect`, `reach`, `interact`, or `custom`. |
| `targetCount` | Number of completions required (default: 1). |
| `targetTag` | Tag to match against for auto-tracking. |

#### Logic Nodes

| Node | Color | Pins | Description |
|------|-------|------|-------------|
| **Condition** | Orange | 1 input, 2 outputs (True/False) | Checks a game state condition and routes accordingly. |
| **Branch** | Purple | 1 input, 2 outputs (True/False) | Identical to Condition; use for readability when branching quest paths. |
| **Reward** | Gold | 1 input (In), 1 output (Next) | Grants a reward (logged to console), then continues. |

Condition/Branch properties:

| Property | Description |
|----------|-------------|
| `conditionType` | `hasItem`, `questComplete`, `variable`, or `custom`. |
| `key` | The variable or item key to check. |
| `operator` | Comparison operator (`==`, `!=`, `<`, `>`, `<=`, `>=`). |
| `value` | The value to compare against. |

Reward properties:

| Property | Description |
|----------|-------------|
| `rewardType` | `xp`, `item`, `currency`, or `custom`. |
| `amount` | Quantity to grant. |
| `itemId` | Item identifier (for `item` type). |

### Editor Layout

The editor window has three areas:

- **Toolbar** (top) -- Auto Layout and Fit All buttons.
- **Graph Canvas** (left) -- The node graph where you create and connect nodes.
- **Inspector Panel** (right, 280px) -- Quest info fields (ID, title, description) and selected node properties.

### Creating Nodes

Right-click on the canvas to open the context menu with three categories:

- **Flow** -- Start, End, Delay, Event
- **Objectives** -- Objective
- **Logic** -- Condition, Branch, Reward

### Connecting Nodes

Drag from an output pin to an input pin to create a link. Validation rules:

- Links connect Flow output to Flow input only.
- Each input pin accepts at most 1 incoming link.
- The Start node allows at most 1 outgoing link.
- Self-links are not allowed.

### Editing Properties

Select a node in the canvas to see its properties in the inspector panel. Edit property values directly in the text fields.

### Play Mode

When you enter Play mode:

1. All `QuestFlowComponent` runtime state is reset.
2. Enabled quests automatically start processing from their Start node.
3. The graph walker advances each frame:
   - **Start** immediately completes and activates its output.
   - **Objective** waits until its counter reaches `targetCount`.
   - **Condition/Branch** evaluates and routes to True or False output.
   - **Reward** logs the grant and continues.
   - **Delay** accumulates time and continues when elapsed.
   - **Event** fires the named event and continues.
   - **End** sets the quest status and stops processing.
4. Active nodes show a yellow dot, completed nodes show a green dot, and unreached nodes show grey.
5. The inspector shows live status, active node count, and objective progress.

When Play mode stops, all runtime state is cleared.

### Auto Layout

Click **Auto Layout** to arrange nodes in a top-down tree layout starting from the Start node. Orphaned (unconnected) nodes are placed below the tree.

### Example: Simple Fetch Quest

```
Start --> Objective ("Collect 5 herbs", targetCount=5)
      --> Reward (xp, amount=200)
      --> End (completed)
```

### Example: Branching Quest

```
Start --> Objective ("Talk to the merchant")
      --> Branch (conditionType=hasItem, key="gold", operator=">=", value="100")
          True  --> Reward (item, itemId="sword")
                --> End (completed)
          False --> Objective ("Earn more gold", targetCount=100)
                --> Reward (item, itemId="sword")
                --> End (completed)
```

### Serialization

Quest flow graphs are saved and loaded automatically with the scene. The serialization key is `"questFlow"` in the scene JSON. All graph layout, node metadata, and quest info fields are preserved. Runtime state (active nodes, counters, timers) is not saved.

---

## 22. Visual Scripting

Enjin includes a full Blueprint-style visual scripting system for creating game logic without writing code. The visual script editor is accessible from **View > Tools > Visual Script** or by clicking **Open Editor** on a Visual Script component.

### Getting Started

1. Select an entity and add the **Visual Script** component from **Add Component > Scripting > Visual Script**.
2. Click **Open Editor** in the inspector to open the visual scripting panel.
3. Right-click on the canvas to add nodes from the categorized context menu.
4. Connect nodes by dragging from output pins to input pins.

### Node Categories

The visual script system provides 80+ built-in nodes organized into these categories:

| Category | Nodes | Description |
|----------|-------|-------------|
| **Events** | On Start, On Update | Entry points that fire once or every frame. |
| **Flow Control** | Branch, Sequence, Delay, For Loop, While Loop, Do Once, Gate, Flip Flop | Control execution flow with conditions and loops. |
| **Variables** | Get Variable, Set Variable, Get Self | Read/write named variables on the script's blackboard. |
| **Math** | Add, Subtract, Multiply, Divide, Modulo, Power, Sqrt, Abs, Min, Max, Clamp, Lerp, Floor, Ceil, Round, Sin, Cos, Tan, Atan2, Random Float, Random Int, Negate | Arithmetic and trigonometric operations. |
| **Logic** | Greater Than, Less Than, Equal, Not Equal, Greater or Equal, Less or Equal, Not, And, Or, Nand, Xor | Boolean comparison and logic gates. |
| **Transform** | Get/Set Position, Get/Set Rotation, Get/Set Scale, Translate, Rotate, Look At | Manipulate entity transforms. |
| **Vector** | Make Vector3, Break Vector3, Vector Length, Normalize, Dot Product, Cross Product, Distance, Lerp Vector | 3D vector operations. |
| **Entity** | Find Entity, Destroy Entity, Spawn Entity, Is Valid, Get Name, Has Component | Entity lifecycle and queries. |
| **Physics** | Add Force, Add Impulse, Set/Get Velocity, Set Gravity Scale, Raycast, Sphere Check, Box Check | Physics simulation and spatial queries. |
| **Health** | Get Health, Set Health, Damage | Health/damage system integration. |
| **Collision** | On Collision Enter/Exit, On Trigger Enter/Exit | Collision event handlers. |
| **Audio** | Play Audio, Stop Audio, Is Audio Playing, Set Audio Volume, Wait For Audio | Sound playback control. |
| **Animation** | Play Animation, Set/Get Animation Speed, Wait For Animation | Skeletal animation control. |
| **Debug** | Print String, Print Warning, Print Error | Console output for debugging. |
| **Functions** | Function Entry, Function Return, Call Function | Reusable subgraph functions. |
| **Script** | Call Script | Call AngelScript functions from visual scripts. |
| **Gameplay** | Save To Slot, Load From Slot, Delete Slot, Checkpoint, Meta Set Float, Meta Get Float, Weather Set/Get, Quest Start/Complete/Query, Cinematic Play/Stop, Particle Play/Stop/Burst, Destructible Damage, Prefab Instantiate, UI Set Focus/Clear Focus, Localization Get | Save system, weather, quests, cinematics, particles, destructibles, prefabs, UI focus, and localization. |
| **Physics 2D** | Raycast 2D, Overlap Circle 2D, Add Force 2D, Set Velocity 2D, Set Gravity 2D | 2D physics queries, forces, and gravity control. |
| **Networking** | Host Game, Join Game, Disconnect, Is Connected, Get Player Count, Call RPC | LAN multiplayer session management and RPC calls. |

### Debugger

The visual script debugger helps you step through execution in Play mode:

- **Breakpoints**: Click the left margin of any node to toggle a breakpoint (red dot). Execution pauses when a breakpoint is hit.
- **Conditional Breakpoints**: Shift+F9 on a node to set a condition expression and/or hit count threshold.
- **Step Through**: When paused, use Step Over / Step Into / Step Out to advance execution.
- **Watch Window**: View variable values in real-time while paused.
- **Call Stack**: See the current execution path through function calls (max depth: 32).
- **Execution Timeline**: Profiler showing which nodes executed each frame and their duration.

### Subgraph Functions

Create reusable logic as functions:

1. Use the **Functions** panel to create a new function with a name.
2. The function gets a **Function Entry** node (with configurable input pins) and one or more **Function Return** nodes.
3. Call the function from any graph using a **Call Function** node.
4. Functions support up to 32 levels of nested calls.

### AngelScript Interop

Visual scripts can call AngelScript functions via the **Call Script** node, and AngelScript can trigger visual script execution via bound functions:

- `VisualScript_SendEvent(entity, eventName)` — triggers a named event on the entity's visual script.
- `VisualScript_SetVariable(entity, name, value)` — sets a variable on the script's blackboard.
- `VisualScript_GetVariable(entity, name)` — reads a variable from the script's blackboard.

---

## 23. Behavior Tree Editor

The Behavior Tree (BT) editor provides a visual tool for designing AI logic as hierarchical trees of tasks, conditions, and decorators.

### Opening the Editor

1. Select an entity and add the **Behavior Tree** component from **Add Component > AI > Behavior Tree**.
2. Click **Open Editor** in the inspector, or open from **View > Tools > Behavior Tree**.

### Node Types

The BT system provides 20 node types in 4 categories, each color-coded in the editor:

#### Composite Nodes (Blue)

| Node | Description |
|------|-------------|
| **Sequence** | Runs children left-to-right. Fails on first failure. Succeeds if all succeed. |
| **Selector** | Runs children left-to-right. Succeeds on first success. Fails if all fail. |
| **Parallel** | Runs all children simultaneously. Configurable success/failure policy. |
| **RandomSelector** | Picks a random child to run. |
| **RandomSequence** | Runs children in random order. |

#### Decorator Nodes (Purple)

| Node | Description |
|------|-------------|
| **Inverter** | Flips child's success/failure result. |
| **Repeater** | Repeats child N times (or indefinitely). |
| **RepeatUntilFail** | Repeats child until it returns failure. |
| **Succeeder** | Always returns success regardless of child result. |
| **Cooldown** | Prevents child from running again for a time duration. |
| **TimeLimit** | Fails child if it runs longer than a time limit. |

#### Action Nodes (Green)

| Node | Description |
|------|-------------|
| **MoveTo** | Moves the entity toward a target position. |
| **Wait** | Waits for a specified duration. |
| **PlayAnimation** | Triggers an animation clip. |
| **SetBlackboard** | Writes a value to the blackboard. |
| **Log** | Prints a message to the console. |

#### Condition Nodes (Orange)

| Node | Description |
|------|-------------|
| **CheckBlackboard** | Checks a blackboard value against a condition. |
| **IsInRange** | Checks if a target is within a specified distance. |
| **HasLineOfSight** | Checks for an unobstructed path to target. |
| **Custom** | User-defined condition with a custom key. |

### Blackboard

The blackboard is a key-value store shared across all nodes in a tree. Edit default values in the **Blackboard** section of the BT editor panel. Keys can store strings that are interpreted as floats, booleans, or entity references at runtime.

### Play Mode Visualization

During Play mode, the BT editor shows live status for each node:

- **Green dot** — node succeeded this tick.
- **Yellow dot** — node is running (in progress).
- **Red dot** — node failed this tick.
- **Grey dot** — node was not reached.

### Auto Layout

Click **Auto Layout** to arrange the tree in a top-down hierarchy from the root node. The layout algorithm spaces nodes evenly and handles subtree widths.

---

## 24. Pixel Editor

The Pixel Editor is a built-in sprite creation tool for making pixel art directly in the engine.

### Opening the Editor

Open from **View > Tools > Pixel Editor**.

### Drawing Tools

| Tool | Description |
|------|-------------|
| **Pencil** | Draw individual pixels. |
| **Eraser** | Erase pixels to transparent. |
| **Line** | Draw straight lines between two points. |
| **Rectangle** | Draw filled or outlined rectangles. |
| **Circle** | Draw filled or outlined circles. |
| **Fill** | Flood-fill a contiguous area with the current color. |
| **Color Picker** | Sample a color from the canvas. |
| **Select** | Rectangular selection for copy/paste/move. |

### Layers

- Create, delete, reorder, and rename layers.
- Toggle layer visibility and opacity.
- Layers composite top-to-bottom.

### Retro Presets

Quick-apply classic console color palettes and resolution constraints (e.g., NES, Game Boy, SNES).

### Onion Skinning

When working with animation frames, onion skinning shows ghost images of adjacent frames to help with smooth motion. Configure the number of previous/next frames and their opacity.

### Animation Timeline

A timeline bar at the bottom lets you create frame-by-frame animations:

- Add, remove, and reorder frames.
- Set per-frame duration.
- Preview animation playback directly in the editor.

### Undo/Redo

Full undo/redo support for all drawing operations (Ctrl+Z / Ctrl+Y).

### Export

- **Export as PNG** — saves the current canvas to disk via stb_image_write.
- **Export as Prefab** — generates a sprite sheet image plus a `.enjprefab` file ready for use in the engine.

---

## 25. Sprite Sheet Importer

The Sprite Sheet Importer slices existing sprite sheet images into individual frames for animation.

### Opening the Importer

Open from **View > Tools > Sprite Sheet Importer**.

### Import Modes

| Mode | Description |
|------|-------------|
| **Grid** | Specify cell width and height. The importer divides the sheet into a uniform grid. |
| **Auto Detect** | Automatically detects sprite boundaries by analyzing alpha transparency. |

### Workflow

1. Load a sprite sheet image.
2. Choose Grid or Auto Detect mode.
3. For Grid mode, enter the cell dimensions and optional padding.
4. Preview the detected frames overlaid on the source image.
5. Confirm to create individual sprite frames usable by `SpriteAnimationComponent`.

---

## 26. Asset Browser

The Asset Browser provides a visual interface for browsing, searching, and managing project files.

### Layout

- **Toolbar** — search bar, grid/list toggle button, thumbnail size slider.
- **Content Area** — file cards displayed in grid or list view.

### Features

| Feature | Description |
|---------|-------------|
| **Search** | Case-insensitive filter by file name. |
| **Grid View** | Thumbnail cards with file type labels and color coding. |
| **List View** | Compact rows with name, type, and file size. |
| **Thumbnails** | Image files (.png, .jpg, .bmp, .tga, .svg) show a thumbnail preview. |
| **Hover Preview** | Hovering an image file shows a larger 256px tooltip preview. |
| **Drag & Drop** | Drag files from the browser to other panels (payload type: `ASSET_PATH`). |
| **Thumbnail Size** | Adjustable slider from 48px to 200px. |

### File Type Labels

Files are automatically categorized and color-coded:

| Label | Extensions | Color |
|-------|-----------|-------|
| **IMG** | .png, .jpg, .jpeg, .bmp, .tga, .svg | Teal |
| **3D** | .gltf, .glb, .fbx, .obj, .dae | Blue |
| **SCN** | .enjscene | Green |
| **SHD** | .vert, .frag, .comp, .glsl | Yellow |
| **AS** | .as | Orange |
| **SFX** | .wav, .mp3, .ogg, .flac | Purple |
| **PFB** | .enjprefab | Cyan |

### SVG Support

Enjin supports loading SVG vector images at runtime via the integrated nanosvg library. SVG files are rasterized to textures at load time and can be used anywhere a regular image texture is accepted — including **UIElement Image widgets** in UI canvases. The Asset Browser shows SVG files with thumbnail previews alongside raster images.

---

## Appendix A: Shader Workflow

Shaders are written in GLSL and stored in `Engine/shaders/`. They must be compiled to SPIR-V and then embedded in `ShaderData.h`.

### Steps

1. Edit the shader source file (e.g., `Engine/shaders/triangle.vert` or `triangle.frag`).
2. Compile to SPIR-V:
   ```bash
   glslangValidator -V Engine/shaders/triangle.vert -o Engine/shaders/triangle.vert.spv
   glslangValidator -V Engine/shaders/triangle.frag -o Engine/shaders/triangle.frag.spv
   ```
3. Convert the `.spv` binary to a C++ byte array and update `ShaderData.h`.
4. Rebuild the engine.

### Descriptor Bindings

| Binding | Stage | Content |
|---------|-------|---------|
| 0 | Vertex | View/Projection UBO |
| 1 | Vertex + Fragment | Lighting UBO (multi-light arrays) |
| 2 | Fragment | Material UBO |
| 3 | Fragment | Base color texture sampler |
| 4 | Fragment | Shadow map sampler |
| 5 | Fragment | Height map (parallax mapping) |
| 6 | Fragment | Normal map |
| 7 | Vertex | Bone matrix SSBO (skeletal animation) |

### Push Constants (128 bytes, per-object)

| Field | Size | Description |
|-------|------|-------------|
| `model` | 64 bytes | Model matrix. |
| `baseColor` + `metallic` | 16 bytes | Base color (RGB) and metallic factor. |
| `emissiveColor` + `roughness` | 16 bytes | Emissive color (RGB) and roughness. |
| `emissiveStrength`, `opacity`, `alphaCutoff`, `flags` | 16 bytes | Material parameters and bit-packed flags. |
| `parallaxScale` + padding | 16 bytes | Parallax depth. |

**Flags bit layout:**

- Bits 0-2: Render flags (double-sided, cast shadows, receive shadows).
- Bit 3: Skinned mesh.
- Bits 8-9: Alpha mode.
- Bit 10: Has height texture.
- Bits 12-13: UV quantize, Gouraud only.
- Bits 16-19: Texture flags (base color, normal, metallic-roughness, emissive).
- Bits 20-23: Retro flags (flat shading, affine texturing, vertex snapping, stipple transparency).
- Bits 24-31: Vertex snap resolution.

---

## Appendix B: Project File Structure

```
my_game/
  my_game.enjinproject      # Project manifest (JSON)
  scenes/
    main.enjin               # Scene files
    level1.enjin
    level2.enjin
  assets/
    textures/                # Texture files (PNG, JPG)
    models/                  # 3D models (GLTF, GLB)
    audio/                   # Audio files
    fonts/                   # TTF fonts
  scripts/                   # AngelScript files (.as)
  templates/                 # Custom scene templates
```

### .enjinproject Format

```json
{
  "name": "My Game",
  "scenes": [
    { "name": "Main Menu", "path": "scenes/main.enjin", "buildIndex": 0, "isStart": true },
    { "name": "Level 1", "path": "scenes/level1.enjin", "buildIndex": 1 },
    { "name": "Level 2", "path": "scenes/level2.enjin", "buildIndex": 2 }
  ]
}
```

---

## Appendix C: Type Conventions

Enjin uses fixed-width type aliases throughout the codebase and exposed APIs:

| Alias | C++ Type |
|-------|----------|
| `u8` | `uint8_t` |
| `u16` | `uint16_t` |
| `u32` | `uint32_t` |
| `u64` | `uint64_t` |
| `i8` | `int8_t` |
| `i16` | `int16_t` |
| `i32` | `int32_t` |
| `i64` | `int64_t` |
| `f32` | `float` |
| `f64` | `double` |
| `usize` | `size_t` |

---

## Appendix D: Engine Namespaces

| Namespace | Purpose |
|-----------|---------|
| `Enjin::Core` | Application, window, input fundamentals. |
| `Enjin::Math` | Vector, Matrix, Quaternion, Spline math. |
| `Enjin::Renderer` | Vulkan renderer, pipeline, buffers, skybox. |
| `Enjin::ECS` | Entity-Component-System, World, Entity, all components. |
| `Enjin::Editor` | Editor layer, play mode, settings, performance stats. |
| `Enjin::Effects` | Weather, water, retro effects, world time, seasonal weather, wind. |
| `Enjin::Accessibility` | Colorblind filter, subtitle system, content warnings, runtime settings. |
| `Enjin::InputSystem` | Remappable input action map (note: uses `InputSystem::` to avoid collision with `Enjin::Input`). |
| `Enjin::Build` | Build pipeline, asset packer, asset reader, build report. |
| `Enjin::Physics` | SimplePhysics, PhysicsWorld, ConstraintSolver, joints. |
| `Enjin::Scripting` | AngelScript engine, script system, coroutines, script event bus. |
| `Enjin::Gameplay` | HUD, quest, quest flow, footstep, cinematic, and object pool systems. |
| `Enjin::Debug` | Profiler, scope timers, frame data. |
| `Enjin::Plugin` | Plugin system, hot-reload. |
| `Enjin::Animation` | Timeline/sequencer system. |
| `Enjin::AI` | AI behaviors, navmesh, A* pathfinding, behavior trees. |
| `Enjin::GUI` | UI canvas, UI elements, UI system, dialogue tree rendering. |
| `Enjin::VisualScript` | Visual scripting node definitions, registry, executor, debugger. |

---

## Appendix E: Logging

Use the engine's categorized logging macros:

```cpp
ENJIN_LOG_INFO(Category, "Message with %s formatting", "printf-style");
ENJIN_LOG_WARN(Category, "Warning: value is %d", value);
ENJIN_LOG_ERROR(Category, "Error: %s", errorMsg.c_str());
ENJIN_LOG_FATAL(Category, "Fatal: cannot continue");
```

Common categories: `Renderer`, `Editor`, `Physics`, `Audio`, `Build`, `Player`, `Script`.

Log output appears in the editor's Console panel and in the terminal.

---

## 27. Ray Tracing

Enjin includes a full Vulkan ray tracing pipeline for hybrid raster+RT rendering. The system detects RT hardware support at startup and gracefully falls back to raster-only rendering on unsupported GPUs.

> **Note:** The RT pipeline code is complete but currently uses placeholder SPIR-V shader stubs. Once the RT shaders are compiled and embedded, the system will activate automatically.

### Requirements

- **GPU:** NVIDIA RTX 20xx+, AMD RX 6000+, or Intel Arc (Vulkan RT extensions required)
- **Extensions:** `VK_KHR_ray_tracing_pipeline`, `VK_KHR_acceleration_structure`, `VK_KHR_deferred_host_operations`, `VK_KHR_buffer_device_address`

### Editor Panel

The Ray Tracing settings are located in the **Rendering** panel (View > Rendering > Rendering):

- **Supported indicator** — Green "Supported" or red "Not Supported" text based on GPU capabilities
- **Enable toggle** — Master on/off for the RT pipeline
- **Mode dropdown** — Choose between Hybrid (raster + RT effects) or Path Trace (progressive full path tracing)

### RT Effects (Hybrid Mode)

Each effect can be independently enabled/disabled with its own configuration:

| Effect | Output Format | Settings |
|--------|---------------|----------|
| **RT Shadows** | R16F | Max distance, shadow radius |
| **RT Reflections** | RGBA16F | Max distance, roughness threshold |
| **RT Ambient Occlusion** | R16F | Radius, power |
| **RT Global Illumination** | RGBA16F | Bounce count, intensity |

**Composite strength sliders** control the blend factor for each effect when composited into the final image.

### Path Tracing Mode

Progressive path tracer for reference-quality rendering:

- **Max bounces** — Maximum ray bounce depth (default 8)
- **Target SPP** — Target samples per pixel for convergence
- **Progress bar** — Shows current SPP / target SPP
- **Converged indicator** — Displays when target SPP is reached
- **Reset button** — Clears accumulation buffer (automatically resets on camera/scene changes)

### SVGF Denoiser

The SVGF (Spatiotemporal Variance-Guided Filtering) denoiser smooths noisy RT output:

1. **Temporal accumulation** — Blends current frame with history using motion vectors (configurable alpha)
2. **Variance estimation** — Computes per-pixel variance from luminance moments
3. **A-trous wavelet** — Edge-preserving spatial filter (configurable iteration count, default 5)

Settings: temporal alpha, a-trous iterations, reset history button.

### Scene Render Settings

All RT settings are saved/loaded with scene render settings (JSON). 24 configuration fields are persisted, including:
- Master enable, RT mode (Hybrid/PathTrace)
- Per-effect enable flags and config values
- Path tracer max bounces and target SPP
- Denoiser type and SVGF parameters
- Composite strength per effect

### How It Works

1. **BLAS** (Bottom-Level Acceleration Structure) — One per unique mesh, cached by hash. Built lazily when new meshes appear
2. **TLAS** (Top-Level Acceleration Structure) — Rebuilt each frame from entity transforms. Uses UPDATE mode when only transforms changed
3. **RT dispatch** — After shadow pass, before main render pass. Each effect dispatches ray generation shaders
4. **Denoise** — SVGF 3-pass compute shader smooths noisy RT output
5. **Composite** — Compute shader multiplies shadows, adds reflections, multiplies AO, adds GI into scene HDR

The RT pipeline only runs for 3D scenes (`SceneRenderMode::Scene3D`). 2D and 2.5D scenes skip RT entirely with no performance impact.

---

## 28. Bug Reporting & Feedback

The editor includes a built-in bug reporting and feedback system accessible from the Help menu. Reports are saved locally as JSON and can optionally be submitted to a remote endpoint.

### Accessing the System

- **Help > Report Bug...** (Ctrl+Shift+B) — Opens the panel on the New Bug Report tab
- **Help > Send Feedback...** — Opens the panel on the New Feedback tab
- **Help > Bug Reports & Feedback** — Toggle the panel visibility
- **Command Palette** (Ctrl+P) — Search for "Report Bug", "Send Feedback", or "Browse Bug Reports"

### Bug Reports

Create detailed bug reports with auto-captured diagnostics:

| Field | Description |
|-------|-------------|
| **Title** | Brief summary of the bug (required) |
| **Type** | Bug, Crash, Performance, Visual, Audio, Other |
| **Severity** | Low, Medium, High, Critical |
| **Description** | Detailed description of the issue |
| **Steps to Reproduce** | Step-by-step instructions |
| **Expected Behavior** | What should happen |
| **Actual Behavior** | What actually happens |
| **Include Logs** | Attach last 50 console log lines |
| **Include Scene** | Attach current scene JSON snapshot |

Each bug report automatically captures a diagnostic snapshot including:
- Engine version, platform, GPU name
- RAM/VRAM usage (total, available, process)
- FPS, frame time, draw calls, entity count, triangle count
- Current scene path and timestamp

### Feedback Entries

Submit feature requests, usability feedback, or general comments:

| Field | Description |
|-------|-------------|
| **Title** | Brief summary |
| **Type** | General, Feature Request, Usability, Documentation, Praise |
| **Priority** | Low, Medium, High |
| **Category** | Freeform category label |
| **Satisfaction** | 1-5 star rating |
| **Description** | Detailed feedback text |
| **Include Diagnostics** | Optionally attach system diagnostics |

### Browsing Reports

The Bug Reports tab provides:
- **Search bar** — Filter by title or description (case-insensitive)
- **Status filter** — Draft, Submitted, Acknowledged, Resolved, Closed
- **Severity filter** — Low, Medium, High, Critical
- **Stats row** — Shows open count / total count
- Click any report to view full details with Edit, Delete, Export, and Submit buttons

### Persistence

Reports are automatically saved to `%APPDATA%/enjin/feedback/feedback_data.json` (Windows) or `~/.config/enjin/feedback/` (Linux). Auto-save triggers on editor shutdown. Reports can be exported individually as JSON files.

---

## 29. Vector Drawing Editor

A built-in vector drawing editor for creating 2D art assets directly in the engine. Access via **Tools > Vector Drawing Editor**.

### Shape Tools

| Tool | Description |
|------|-------------|
| **Line** | Draw straight lines |
| **Rectangle** | Draw rectangles |
| **Ellipse** | Draw ellipses/circles |
| **Pen** | Freehand drawing |
| **Bezier** | Cubic bezier curves |
| **Star** | N-pointed stars |
| **Polygon** | Regular polygons |

### Editor Features

- **8 tools**: Select, Line, Rectangle, Ellipse, Pen, Bezier, Star, Polygon
- **Layer system** with add/remove/reorder
- **Undo/Redo** with 50 levels of history
- **Snap to grid** with configurable grid size
- **Zoom and pan** with mouse wheel and middle mouse drag
- **Property panel** for editing shape fill color, stroke color, stroke width
- **SVG export** for use in web or other applications
- **Flash symbol library** integration for Flash game revival workflow

---

## 30. HTML5 Export

Export your project as a web-ready HTML5 application. Access via **Build > Export HTML5**.

### Generated Files

| File | Purpose |
|------|---------|
| **index.html** | Main page with canvas element, Module config, fullscreen support |
| **preloader.js** | Loading progress bar with click-to-play audio interstitial |
| **style.css** | Responsive scaling, preloader styling, fullscreen layout |

### Export Dialog

Configure the export via a modal dialog:
- Output directory selection
- Window title and resolution
- Embed code generation (iframe, Newgrounds-compatible)

---

## 31. Newgrounds.io Integration

Built-in support for the Newgrounds.io API for publishing Flash-style web games. Provides session management, medals, scoreboards, and cloud saves.

### AngelScript Functions

| Function | Description |
|----------|-------------|
| `NG_Connect(appId, encKey)` | Initialize connection |
| `NG_IsConnected()` | Check connection status |
| `NG_CheckSession()` | Validate current session |
| `NG_UnlockMedal(medalId)` | Unlock an achievement |
| `NG_PostScore(boardId, value)` | Submit a high score |
| `NG_GetPassportUrl()` | Get login URL |
| `NG_GetUserName()` | Get logged-in username |
| `NG_SaveSlot(slotId, data)` | Save to cloud slot |
| `NG_LoadSlot(slotId)` | Load from cloud slot |

Configuration is done via the Newgrounds tab in the Flash Timeline panel.


---

## 32. Networking & Security Settings

Enjin loads its runtime networking configuration from a JSON file so multiplayer tuning does not require a rebuild.

**Config file path**

`config/network_settings.json` (relative to the working directory of the editor or game).

**Default config**

```json
{
  "port": 7777,
  "maxPlayers": 16,
  "serverIP": "127.0.0.1",
  "syncRate": 0.05,
  "rateLimit": {
    "maxPacketsPerSecond": 200.0,
    "maxBytesPerSecond": 131072.0,
    "burstPackets": 50.0,
    "burstBytes": 65536.0
  },
  "security": {
    "maxViolations": 10,
    "violationWindowSeconds": 10.0,
    "banSeconds": 30.0,
    "kickOnViolation": true
  }
}
```

**Notes**

The `rateLimit` block controls per-sender packet and bandwidth throttling with a token-bucket burst allowance. The `security` block determines how many violations within a rolling window will trigger a temporary ban and optional kick. For competitive or high-traffic games, raise `maxBytesPerSecond`, `burstBytes`, and/or `maxPacketsPerSecond` to match your netcode needs.

