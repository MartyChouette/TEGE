<div align="center">

# TEGE

### The Enjin Game Engine

**A production-grade game engine built from scratch in C++20 and Vulkan 1.3.**\
**2D, 2.5D, and 3D. Editor, player, visual scripting, ray tracing, and more.**\
**Built in 9 weeks. 150+ features. 400+ audit findings resolved.**

---

![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C?style=flat-square&logo=cplusplus&logoColor=white)
![Vulkan](https://img.shields.io/badge/Vulkan-1.3-AC162C?style=flat-square&logo=vulkan&logoColor=white)
![CMake](https://img.shields.io/badge/CMake-3.20+-064F8C?style=flat-square&logo=cmake&logoColor=white)
![Platform](https://img.shields.io/badge/Platform-Windows%20%7C%20Linux-grey?style=flat-square)
![License](https://img.shields.io/badge/License-Proprietary-lightgrey?style=flat-square)

</div>

---

## At a Glance

| | |
|---|---|
| **49** production features | **44** starter templates |
| **70+** component types | **222+** visual scripting nodes |
| **~844** script bindings | **32** editor panels |
| **19** ray tracing shaders | **9+** procedural generation algorithms |
| **40** unit tests across 18 build targets | **400+** audit findings, 300+ resolved |

---

## What Is TEGE?

TEGE is a complete game engine &mdash; not a framework, not a library. It ships with a full editor, a standalone player runtime, visual scripting, production physics (Jolt + Box2D), AngelScript integration, a build pipeline, and everything you need to make and ship a game.

It was built from a blank `main.cpp` to a 150+ feature engine in roughly 9 weeks (Dec 23, 2025 &ndash; Feb 19, 2026), then hardened through 7 rounds of systematic security and stability auditing.

**Target audience:** Indie developers, Flash game revivalists, solo devs who want a lightweight alternative to Godot/Unity with built-in gameplay systems, accessibility, and retro aesthetics.

---

## Architecture

```
enjin/
├── Core/           Zero-dependency foundation layer
│   ├── Memory      Stack, Pool, Linear allocators
│   ├── Math        Vectors, Matrices, Quaternions, Noise
│   ├── Logging     Thread-safe, categorized
│   └── Platform    Window management (GLFW), entry point abstraction
│
├── Engine/         Full engine layer
│   ├── Renderer    Vulkan 1.3 (forward + deferred, RT pipeline)
│   ├── ECS         Entity Component System (70+ component types)
│   ├── Physics     Jolt v5.2.0 (3D) + Box2D v3.0.0 (2D)
│   ├── Audio       miniaudio backend, 3D spatial, mixer
│   ├── Scripting   AngelScript (~844 bindings) + Visual Scripting (222+ nodes)
│   ├── Assets      glTF, FBX, SWF import, texture atlas, asset packs
│   ├── Shaders     SPIR-V pipeline, hot-reload, shader graph (58+ nodes)
│   └── Systems     AI, navigation, dialogue, quests, save, weather, terrain...
│
├── Editor/         ImGui-based editor (32 panels)
│   ├── Hierarchy, Inspector, Console, Asset Browser
│   ├── Scene Picker, Gizmos, Profiler, Command Palette
│   ├── Terrain Brushes, Tilemap Editor, Vector Editor
│   └── PlayMode with PlayModeDiff (cherry-pick runtime changes)
│
├── Player/         Standalone game player runtime
├── Tests/          Unit + integration tests (CTest)
├── Examples/       Example projects
├── third_party/    GLFW, ImGui, ImGuizmo
└── docs/           40+ documentation files
```

---

## Features

<details>
<summary><b>Rendering</b></summary>

### 3D Pipeline
- Vulkan 1.3 with GPU-driven frustum culling
- PBR + Blinn-Phong materials with normal mapping, parallax occlusion (4 modes)
- 4-Cascade Directional Shadow Maps with PCF, texel stabilization, distance fade, shadow dither (6 patterns)
- Point light cubemap shadows + spot light 2D array shadows (up to 4 each)
- Bindless resource management
- Descriptor caching (75-85% hit rate)
- Material sorting by texture key
- Deferred rendering framework

### 2D Pipeline
- GPU-instanced sprite batching
- 4096x4096 sprite texture atlas (shelf-pack algorithm)
- 2.5D lit sprite support with normal maps and drop shadows
- Sprite animation system

### Ray Tracing
- RT Shadows, Reflections, Ambient Occlusion, Global Illumination
- Path Tracer
- SVGF + Intel OIDN denoisers
- RT Compositor
- Graceful fallback on non-RT hardware

### Post-Processing
Bloom, Depth of Field (bokeh, 3 aperture shapes), FXAA, Vignette, Film Grain, Color Grading, SSAO, God Rays, Caustics, Stipple/Dither (8 patterns, 3 color modes), Tilt-Shift, Post-Process Volumes (spatial blending, priority-based, selective override)

### Advanced
- Spherical Harmonics Light Probes (L2)
- Signed Distance Field scenes (6 primitives, 6 boolean ops)
- Order-Independent Transparency (weighted blended OIT)
- Voxel Cone Tracing (VXGI)
- Cel shading with outlines
- Non-Euclidean geometry (portal rendering, hyperbolic/spherical/toroidal space)
- Metaball / implicit surface rendering
- Framebuffer feedback effects (8 presets: Echo, Melt, InfiniteMirror, VHS, Kaleidoscope, Phosphor, DreamSequence)
- Screen-space distortion (7 types: HeatHaze, Shockwave, Underwater, PortalEdge, Ripple, BarrelFisheye, Custom)

### Retro / Stylization
CRT shader, PSX-style rendering (flat shading, affine texturing, vertex snapping), pixelation, dithering, color quantization, stipple transparency

### Environment
- Skybox (cubemap + procedural with 5 presets)
- Weather system (rain, snow, fog, storms with lightning)
- Water (Gerstner waves, shore foam, reflections, refraction, underwater, freeze, ocean mode)
- Interactive water (spring-damper propagation, splashes, V-wakes, buoyancy)
- Vegetation with wind sway (grass, shrubs, trees)
- Terrain with sculpting (5 brush modes)
- GPU-instanced particle system (5 emitter shapes, size/speed curves)
- Destructible environments (4 fracture patterns: Voronoi, Grid, Radial, Shatter)
- World curvature (vertex-shader horizon bending)

</details>

<details>
<summary><b>Physics</b></summary>

### 3D &mdash; Jolt Physics v5.2.0
- Production-grade rigid body simulation
- 6 joint types (hinge, slider, spring, cone, distance, fixed) with breakable mode
- Ragdoll support (bone-to-joint mapping, animation blend, auto-settle)
- Destructible physics
- 1000 colliders in ~2-3ms (multi-threaded)

### 2D &mdash; Box2D v3.0.0
- Full 2D rigid body simulation
- Circle, box, polygon shapes with 5 joint types
- Continuous collision detection (CCD)
- SAT collision

### Shared
- Runtime backend swapping via `IPhysicsBackend` abstraction
- Spatial hash grid broad-phase
- Gravity zones (directional, point, zero-G)
- Debug wireframes for colliders and joints
- Ground detection for character controllers (2D + 3D)

</details>

<details>
<summary><b>Scripting</b></summary>

### AngelScript
- ~844 script bindings covering the full engine API
- `TegeBehavior` base class with lifecycle callbacks
- Hot-reload support
- Script coroutines (YieldSeconds, YieldFrames, StartCoroutine)
- Script event system (string-named events with typed payloads)
- Flash API shim (~40 bindings: DisplayObject, MovieClip, Stage, Mouse, TextField, Sound, Timer)
- AS2/AS3 transpiler (pattern-based ActionScript to AngelScript conversion)
- SWF binary import to ECS entities

### Visual Scripting
- 222+ node types (events, flow control, math, physics, AI, dialogue, audio, networking, procgen, debug)
- Blueprint-style node graph editor
- Breakpoint debugging (F9 toggle, F5 continue, F10 step)
- Execution profiler with color-coded timeline
- State machines and behavior trees
- Latent nodes (Delay, WaitForAudio, WaitForAnimation)
- Variable system (Bool, Int, Float, String, Vector3, Entity)
- Multi-select editing, undo/redo, copy/paste

### Shader Graph
- 58+ nodes
- Topological sort GLSL code generation
- Hot-reload
- .enjshader save/load

### Audio Event Graph
- Dynamic audio mixing with runtime execution
- Trigger events, parameter thresholds, delay scheduling
- .enjaudiopkg save/load

### Particle Graph
- Visual particle system authoring
- Compiler to ParticleEmitterComponent
- .enjparticle save/load

</details>

<details>
<summary><b>Gameplay Systems</b></summary>

| System | Details |
|--------|---------|
| **Quest System** | Full quest tracking, start/complete/fail with objective tracking |
| **Dialogue Trees** | 7 node types, visual editor, EntityEventBus + SubtitleSystem integration |
| **Save System** | 10 slots + quick save/load, SharedObject persistence |
| **AI / Behavior Trees** | 20 node types, blackboard system, visual editor, play-mode debugging |
| **Navmesh + Pathfinding** | A* pathfinding with navigation mesh generation |
| **Animation** | GPU skinning (skeletal), sprite animation, IK (LookAt, FABRIK), tweening (25 easing functions) |
| **Character Controllers** | FPS, TPS, platformer, 2D platformer, flying, swimming, surface-aligned |
| **HUD Overlay** | Health bars, resource bars, labels, crosshair |
| **Damage System** | Per-type resistance/weakness multipliers |
| **Stamina / Resources** | Generic resource with regeneration and controller integration |
| **Object Pooling** | Entity recycling with configurable pool sizes and auto-release |
| **Cinematic Camera** | Waypoint sequences with easing curves for cutscenes |
| **Localization** | String tables, CSV/JSON I/O, parameterized strings, LOC() macro |
| **Networking** | LAN multiplayer (host-authoritative UDP, HMAC-SHA256, 20Hz sync, RPC, lobby) |
| **Footstep Audio** | Surface-based footstep sounds with walk/run intervals |
| **Entity Event Bus** | Decoupled C++ entity communication |

### Procedural Generation (9+ algorithms)
Cellular automata, BSP, Diamond-Square, L-Systems (3D stochastic), Wave Function Collapse, Voronoi, Random walker, Grammar rules, Fractal terrain with hydraulic/thermal erosion, Reaction-Diffusion (Gray-Scott), Physarum simulation

### Advanced Systems
- Timeline editor (Flash-style keyframe animation, layers, curve editor, onion skinning)
- Fourier transform meshes (DFT decomposition, progressive reconstruction)
- 4D stereographic projection (5 polytopes, 6 rotation planes)
- Inverse/differentiable rendering (CPU gradient descent optimization)
- IK-driven mesh deformation (FABRIK + Verlet, tube/ribbon generation)
- Mesh audio reactivity (FFT analysis, per-vertex displacement)
- Fluid-terrain coupling (bidirectional erosion and accumulation)

</details>

<details>
<summary><b>Editor</b></summary>

32 ImGui panels including:

- **Hierarchy** &mdash; Scene tree with drag-and-drop, entity icons by component type
- **Inspector** &mdash; 70+ component types with undo/redo (PropertyEditCommand)
- **Console** &mdash; Logging and command input
- **Asset Browser** &mdash; File management with auto-generated thumbnails
- **Scene Picker** &mdash; Ray-cast entity selection, double-click to focus
- **Gizmos** &mdash; Translate, rotate, scale (ImGuizmo)
- **Profiler** &mdash; Per-frame breakdown, FPS graph, scope-based profiling
- **Terrain Brushes** &mdash; 5 brush modes with adjustable radius/strength/falloff
- **Tilemap Editor** &mdash; 2D level design
- **Vector Editor** &mdash; 7 shape types, layers, SVG export
- **Command Palette** &mdash; Ctrl+P fuzzy search (25+ commands)
- **Shader Graph** &mdash; Visual shader authoring (58+ nodes)
- **Visual Script Editor** &mdash; Blueprint-style with debugging and profiler
- **Dialogue Editor** &mdash; Visual dialogue tree (7 node types)
- **Animation Graph** &mdash; Dual-mode state machine editor
- **Audio Event Graph** &mdash; Dynamic audio mixing
- **Particle Editor** &mdash; 7 presets, color gradients, size/speed curves
- **UI Editor** &mdash; WYSIWYG with click-select, drag-move, resize handles
- **PlayMode** &mdash; Play/Pause/Stop with PlayModeDiff to cherry-pick runtime changes
- **Game View** &mdash; Renders from in-scene camera independently from editor camera
- **Collaborative Editing** &mdash; Real-time multi-user with OT protocol, peer cursors, conflict resolution
- **Bug Reporter** &mdash; Built-in reports with auto-captured diagnostics

### Project Management
- 44 starter templates across 7 categories
- Template creator + marketplace (15 curated templates)
- Project hub with startup wizard
- Scene serialization (JSON)
- Scene transitions (Instant, Fade Black, Fade White, Cross Fade)
- Prefab system
- Build dialog with progress tracking
- Import presets for 10 DCC tools (Blender, Maya, 3ds Max, Houdini, Cinema 4D, ZBrush, Substance Painter, Unreal, Unity, SketchUp)

### Editor Controls

| Action | Control |
|--------|---------|
| Move Camera | `W/A/S/D` |
| Look Around | Hold Right-click + Mouse |
| Camera Up/Down | `Space/E` and `Q/Ctrl` |
| Sprint | `Shift` |
| Select Entity | Left-click in viewport |
| Focus Entity | Double-click |
| Multi-Select | `Ctrl+click` or marquee drag |
| Translate / Rotate / Scale | `1` / `2` / `3` |
| Toggle Local/World | `4` |
| Undo / Redo | `Ctrl+Z` / `Ctrl+Y` |
| Command Palette | `Ctrl+P` |

</details>

<details>
<summary><b>Audio</b></summary>

- **miniaudio backend** &mdash; Cross-platform (WAV, MP3, FLAC, Vorbis)
- **3D spatial audio** &mdash; Positional audio with distance attenuation models
- **Steam Audio integration** &mdash; HRTF, occlusion, transmission (geometry-aware)
- **Audio channels** &mdash; SFX/Music/UI/Voice with independent volume
- **Multi-channel mixing** &mdash; Multiple simultaneous sounds
- **Audio event graph** &mdash; Visual dynamic mixing
- **MIDI input** support
- **Scene serialization** &mdash; AudioSource and AudioListener components persist

</details>

<details>
<summary><b>Accessibility</b></summary>

| Category | Features |
|----------|----------|
| **Vision** | 8 colorblind modes (GPU), WCAG AAA high-contrast themes, dyslexia-friendly fonts (OpenDyslexic), reduced motion, runtime font scaling (0.5-3.0x), colorblind-safe UI palettes (9 palettes with patterns + icons) |
| **Motor** | Switch access scanning, dwell-click, sticky drag, adjustable thresholds, one-handed presets, input remapping |
| **Audio** | Screen reader announcer (priority queue), audio visual indicators, MIDI input |
| **Input** | Eye tracking, sip-and-puff, head tracking support, full rebinding |
| **Content** | Per-scene content warnings with dismissable overlay |
| **Presets** | Low Vision, Motor Impaired, Photosensitive, Reset All |

</details>

<details>
<summary><b>Asset Libraries</b></summary>

| Library | Contents |
|---------|----------|
| **Font Library** | 42 OFL/Apache fonts across 8 categories (Sans-Serif, Serif, Monospace, Display, Handwriting, Pixel, Fantasy, Sci-Fi) |
| **3D Asset Library** | 16 CC0 model packs (Kenney, Quaternius) &mdash; Architecture, Nature, Props, Characters, Vehicles, Weapons, Dungeon, Sci-Fi |
| **2D Asset Library** | 15 CC0 sprite/tileset/UI packs &mdash; UI Kits, Tilesets, Sprites, VFX, Backgrounds, Textures |

</details>

<details>
<summary><b>Platforms & Export</b></summary>

### Supported Now
- **Windows** &mdash; Standalone .exe, NSIS installer (Start Menu, file associations, uninstaller)
- **Linux** &mdash; AppImage packaging
- **HTML5** &mdash; Web export with preloader and responsive scaling
- **Steam Deck** &mdash; Auto-detection, adaptive quality, gyro input stubs

### Planned
- macOS (MoltenVK) &mdash; Q2 2026+
- WebAssembly / WebGPU
- Console platforms (Switch stub exists, requires licensed devkit)
- Mobile (Android/iOS)
- VR/XR (OpenXR)

### Build & Distribution
- Asset pack pipeline (`.enjpak` with compression, CRC32 integrity)
- Texture compression (BCn/ASTC with mipmap generation)
- Asset thumbnails (auto-generated previews for images, 3D models, scenes)
- Adaptive quality (FPS-based auto-adjustment of render scale, shadow quality, particle count)

### Integrations
- Newgrounds.io API (medals, scoreboards, cloud saves, themed game page template)
- Steam Audio (HRTF, occlusion, transmission)
- Plugin system (IPlugin interface, DLL/SO hot-reload, manifest JSON, editor panel)

</details>

---

## Performance

| Metric | Target | Achieved |
|--------|--------|----------|
| Empty scene | < 2ms | ~1ms |
| 1000 3D entities | < 16ms | < 16ms with Jolt |
| 1000 2D sprites | < 8ms | < 5ms |
| Descriptor cache hit rate | > 70% | 75-85% |

**Frame budget breakdown (60fps, 3D scene):**

```
Shadow Passes (CSM + Point + Spot)    3.0ms  ████████████
Main Render Pass (Opaque)             4.0ms  ████████████████
Sprite Batch Rendering                1.5ms  ██████
Particle Rendering                    1.0ms  ████
Post-Processing                       2.5ms  ██████████
ECS Systems (Physics, AI, Scripts)    3.0ms  ████████████
Editor / ImGui                        1.0ms  ████
Scene Classification + UBO            0.5ms  ██
Headroom                              0.2ms  █
                                     ──────
                                     16.7ms  (60fps)
```

---

## Quality Assurance

TEGE has been through **7 rounds of systematic auditing** covering security, stability, performance, and API contracts.

| | |
|---|---|
| **400+** total findings | **300+** resolved (75%+) |
| **All CRITICAL/HIGH** fixed in Tier 1 systems | **40** unit tests (up from 13) |
| **18** build targets, all clean | **3x** test coverage increase |

### Tier 1 (Stable) Systems
ECS, Renderer (Vulkan), Physics (Jolt + Box2D), Audio &mdash; all CRITICAL and HIGH findings resolved.

### Tier 2 (Beta) Systems
Serialization (14/17 fixed), Networking (16/18 fixed) &mdash; deferred items are LOW risk or design-level.

See the [full audit documentation](docs/) for per-subsystem breakdowns.

---

## Building

### Prerequisites

- CMake 3.20+
- C++20 compiler (MSVC 2019+, GCC 10+, Clang 12+)
- Vulkan SDK
- GLFW3

### Windows

```cmd
mkdir build && cd build
cmake .. -G "Visual Studio 16 2019" -A x64 -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake
cmake --build . --config Release
```

### Linux

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
```

See [WINDOWS_BUILD.md](docs/WINDOWS_BUILD.md) for detailed Windows setup instructions.

### Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `ENJIN_BUILD_EDITOR` | `ON` | Build the editor application |
| `ENJIN_BUILD_PLAYER` | `ON` | Build the standalone game player |
| `ENJIN_BUILD_TESTS` | `OFF` | Build unit and integration tests |
| `ENJIN_BUILD_EXAMPLES` | `OFF` | Build example projects |

### Running

```bash
# Editor
./build/bin/Release/EnjinEditor.exe     # Windows
./build/bin/EnjinEditor                  # Linux

# Standalone Player (requires game.enjpak in same directory)
./build/bin/Release/EnjinPlayer.exe     # Windows
./build/bin/EnjinPlayer                  # Linux
```

---

## Dependencies

All dependencies use permissive licenses compatible with proprietary licensing.

| Dependency | License | Purpose |
|-----------|---------|---------|
| GLFW3 | zlib/libpng | Windowing and input |
| Vulkan SDK | Apache 2.0 | Graphics API |
| Dear ImGui | MIT | Editor UI |
| ImGuizmo | MIT | Transform gizmos |
| Assimp | BSD | 3D model import (glTF, FBX) |
| nlohmann/json | MIT | JSON serialization |
| stb libraries | Public Domain | Image loading, font rasterization |
| Jolt Physics | MIT | 3D physics |
| Box2D | MIT | 2D physics |
| miniaudio | Public Domain | Audio backend |
| AngelScript | zlib | Scripting language |

---

## Timeline

```
Dec 23-25  Foundation          Core layer, Vulkan context, 3D pipeline
Jan 22-28  Renderer & Editor   Shaders, ImGui, PBR, CSM shadows, FBX
Jan 29-31  Core Systems        Weather, water, animation, physics, audio
Feb  1-5   2D & Build          Terrain, UI, sprite batch, dialogue, tweening
Feb  6-8   Visual Scripting    222+ nodes, GPU sync, descriptor caching
Feb  9-10  Massive Expansion   AI, proc gen, RT pipeline, Newgrounds API
Feb 11-14  Physics & Advanced  Jolt, Box2D, OIT, SH probes, SDF, shaders
Feb 15-19  Audit & Hardening   7 audit rounds, 400+ findings, test expansion
```

---

## License

Proprietary &mdash; All rights reserved.

---

<div align="center">

**TEGE** &mdash; The Enjin Game Engine

C++20 &middot; Vulkan 1.3 &middot; CMake &middot; Proprietary Software

</div>
