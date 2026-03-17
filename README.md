<div align="center">

<img src="installer/social_preview.png" alt="TEGE — The Enjin Game Engine" width="720">

<br><br>

**An aesthetics-first game engine — preserving and expanding the digital aesthetics of yesterday for the storytellers of tomorrow.**

<br>

[![License: BSL 1.1](https://img.shields.io/badge/License-BSL_1.1-blue.svg)](LICENSE)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C.svg?logo=cplusplus&logoColor=white)](https://en.cppreference.com/w/cpp/20)
[![Vulkan 1.3](https://img.shields.io/badge/Vulkan-1.3-AC162C.svg?logo=vulkan&logoColor=white)](https://www.vulkan.org/)
[![Platform](https://img.shields.io/badge/Platform-Windows%20%7C%20Linux-lightgrey.svg)]()
[![CMake](https://img.shields.io/badge/Build-CMake-064F8C.svg?logo=cmake&logoColor=white)](https://cmake.org/)
[![Tests](https://img.shields.io/badge/Tests-1100%2B_passing-brightgreen.svg)]()

<br>

[Website](https://www.marty64.net/enjin/) · [Download](https://www.marty64.net/enjin/TEGE-0.9.0.zip) · [Documentation](docs/) · [Build Guide](docs/BUILD.md) · [Scripting API](docs/SCRIPTING_API.md)

</div>

<br>

## Screenshots

<table>
<tr>
<td width="33%" align="center">
<img src="Engine/previews/thirdperson/frame_0.png" alt="3D Third Person" width="100%">
<br><sub><b>3D Third Person</b> — PBR renderer, cascaded shadows, terrain</sub>
</td>
<td width="33%" align="center">
<img src="Engine/previews/platformer/frame_0.png" alt="2D Platformer" width="100%">
<br><sub><b>2D Platformer</b> — Sprite rendering, Box2D physics, particles</sub>
</td>
<td width="33%" align="center">
<img src="Engine/previews/topdown2d/frame_0.png" alt="Top-Down Dungeon" width="100%">
<br><sub><b>Top-Down Dungeon</b> — Tile-based levels, AI, lighting</sub>
</td>
</tr>
</table>

> Full editor with hierarchy, inspector, viewport, console, and asset browser. 44 starter templates across 7 categories.

---

## Why TEGE?

| | |
|:---|:---|
| **Complete Editor** | Hierarchy, inspector, viewport, play mode, undo/redo, command palette, project hub, 44 starter templates |
| **70+ ECS Components** | Transform, mesh, material, lights, cameras, physics, AI, UI, audio, scripting — all with inspector UI |
| **Vulkan PBR Renderer** | Cascaded shadows, post-processing, GPU culling, clustered lighting, full ray tracing pipeline |
| **Dual Scripting** | 721 AngelScript bindings with hot-reload + visual scripting with 146+ nodes and breakpoint debugging |
| **Dual Physics** | Jolt 5.2.0 (3D) + Box2D 3.0.0 (2D), 5 character controller types, joints, ragdolls, sensors |
| **Ship Everywhere** | Standalone builds, HTML5/WebAssembly, Newgrounds, Windows installer, Linux AppImage |
| **Gameplay Out of the Box** | Save/load, quests, HUD, dialogue trees, destructibles, LAN multiplayer, localization |
| **Accessibility Suite** | Colorblind correction (8 modes), screen reader, switch access, dyslexia mode, WCAG AAA themes |

---

## Quick Start

```bash
# Clone
git clone https://github.com/MartyChouette/TEGE.git && cd TEGE

# Build
mkdir build && cd build
cmake ..
cmake --build . --config Release

# Run
./bin/Release/EnjinEditor.exe   # Windows
./bin/EnjinEditor               # Linux
```

> **Prerequisites:** CMake 3.20+, C++20 compiler, Vulkan SDK, GLFW3. See [BUILD.md](docs/BUILD.md) for full details.

### Windows Installer

A pre-built Windows installer is available — no build tools required:

1. Download **[TEGESetup-0.9.0.exe](https://www.marty64.net/enjin/TEGE-0.9.0.zip)** from the website
2. Run the installer — it sets up the editor, player, and file associations (`.enjin`, `.enjscene`)
3. Launch TEGE from the Start Menu or desktop shortcut

---

## Feature Breakdown

<details>
<summary><b>Rendering</b> — Vulkan 1.3, PBR, ray tracing, retro effects</summary>
<br>

- **PBR Material System** — Base color, metallic, roughness, emissive, normal mapping, parallax occlusion (4 modes), transmission/IOR/thickness, subsurface scattering, material presets (Glass, Water, Skin, Leaf)
- **Shadow Mapping** — 4-cascade CSM, cubemap point shadows, spot shadows, 16-sample Poisson PCF, 6 dither patterns (Bayer, Blue Noise, Halftone, Crosshatch, Overlook)
- **Anti-Aliasing** — TAA (Halton jitter, neighborhood clamping, velocity reprojection), FXAA, per-scene selectable
- **Motion Vectors** — Per-pixel velocity buffer (RG16F) for TAA and temporal denoising
- **Post-Processing** — Bloom, vignette, color grading, film grain, tone mapping, full-screen stipple/dither, depth of field (bokeh), tilt-shift, post-process volumes with spatial blending
- **Ray Tracing Pipeline** — RT shadows, reflections, AO, GI, translucency, caustics, path tracing with 3 denoisers (SVGF, Intel OIDN, NVIDIA OptiX)
- **Retro Effects** — PSX flat shading, affine texturing, vertex snapping, stipple transparency, CRT scanlines, color quantization
- **Environment** — Procedural gradient sky / cubemap skybox, weather system (rain, snow, fog, storms), water plane with Gerstner waves, instanced vegetation with wind
- **GPU Optimizations** — Two-phase HiZ occlusion culling, clustered forward lighting (16x9x24), 64-bit material sort keys, per-frame linear allocator, descriptor set caching
- **Advanced** — Variable Rate Shading, Virtual Texturing, Visibility Buffer, OIT, SH Light Probes, SDF scene, voxel cone tracing, non-Euclidean portals, world curvature
- **Camera** — 9 presets (Isometric, TopDown, SideScroller, FPS, TPS, Cinematic, SecurityCam, BirdsEye)

</details>

<details>
<summary><b>Editor</b> — Full-featured ImGui editor with visual authoring tools</summary>
<br>

- **Core Editor** — Hierarchy, inspector, viewport, play/pause/stop, undo/redo, cut/copy/paste, multi-select (Ctrl+click, Shift+range, marquee), entity icons by type
- **Gizmos & Viewport** — Translate/rotate/scale via ImGuizmo, click-to-select ray casting, terrain sculpting (5 brush modes), stats overlay, wireframe toggle
- **Visual Authoring** — Shader Graph (54 node types), Audio Event Graph, Particle Graph, Dialogue Tree Editor (7 node types, Yarn/Twine import), Animation Graph (state machines, blend trees), Vector Drawing Editor (SVG export)
- **Visual Scripting** — Blueprint-style editor with 146+ nodes, breakpoint debugging (F9/F5/F10), execution profiler with color-coded timeline
- **Project Tools** — Project Hub with template browser, 44 starter templates (7 categories), Template Creator, Template Marketplace (15 curated templates)
- **Smart Features** — Smart Suggestions (12 context-aware rules), Quick Setup Patterns (7 one-click setups), Command Palette (Ctrl+P, 25+ commands), keyboard shortcuts help (Ctrl+Shift+/)
- **Settings** — Unified 3-tab window (System/Project/Scene), accent color presets, theme preview, networking config editor
- **Collaboration** — Real-time multi-user editing with OT protocol, peer cursors, conflict resolution, entity locking
- **Build & Export** — Build dialog, HTML5/WebAssembly export, Newgrounds export with medal/scoreboard integration
- **Feedback** — Built-in bug reports with auto-captured diagnostics, notification toasts, empty-state patterns with CTAs

</details>

<details>
<summary><b>ECS & Gameplay</b> — 70+ components, physics, AI, saves, quests</summary>
<br>

#### Entity-Component System
- **70+ Component Types** — Full inspector UI for all components
- **Character Controllers** — Platformer 2D, Top-Down 2D/3D, Third Person, First Person, Surface Aligned (spherical gravity)
- **Physics** — Jolt 5.2.0 (3D) + Box2D 3.0.0 (2D) via `IPhysicsBackend` abstraction. Collision detection, ground detection, sensor bodies, debug wireframes, 6 joint types, ragdolls
- **Environment** — Gravity zones, temperature zones, camera trigger volumes, destructible environments (4 fracture patterns)
- **AI** — Behavior Trees (20 node types, blackboard, visual editor), navmesh pathfinding (A*)
- **UI Runtime** — Anchor-based layout, 8 widget types, 6 theme presets (incl. high contrast), font scaling, accessible labels
- **LOD & Streaming** — Distance-based mesh swapping with hysteresis, chunk-based level streaming with async loading

#### Gameplay Systems
- **Save/Load** — 10-slot system with quick save/load
- **Quest System** — Start/complete/fail quests with objective tracking
- **HUD** — Health bars, resource bars, labels, crosshair
- **Dialogue** — Auto-built dialogue display with speaker, portrait, choices
- **Combat** — Damage resistance/weakness multipliers, stamina/resource system with regen
- **Cinematic Camera** — Waypoint sequences with easing curves for cutscenes
- **Object Pooling** — Entity recycling with configurable pool sizes
- **Localization** — String tables, CSV/JSON I/O, parameterized strings, LOC() macro

</details>

<details>
<summary><b>Animation & Audio</b> — Skeletal, sprite, timeline, spatial audio</summary>
<br>

#### Animation
- **Skeletal Animation** — glTF + Assimp (FBX/DAE/3DS/20+ formats), GPU skinning, animation state machines with blending
- **2D Sprite Animation** — Frame-based flipbook animation
- **Inverse Kinematics** — LookAt IK, FABRIK chain solving, interaction IK
- **Timeline Editor** — Flash-style keyframe animation with layers, 4 interpolation modes (Constant/Linear/Bezier/CatmullRom), curve editor, onion skinning, auto-key

#### Audio
- **Cross-Platform** — miniaudio backend (WAV, MP3, FLAC, Vorbis)
- **3D Spatialization** — Positional audio with distance attenuation
- **Channels** — SFX/Music/UI/Voice with independent volume control
- **Scene Integration** — AudioSource and AudioListener components saved/loaded with scenes

</details>

<details>
<summary><b>Scripting</b> — AngelScript, visual scripting, plugins, Flash shim</summary>
<br>

- **AngelScript** — TegeBehavior base class, ~721 API bindings, hot-reload, coroutines (YieldSeconds, YieldFrames), string-named event system
- **Visual Scripting** — 146+ node types, breakpoint debugging, execution profiler, latent nodes (Delay, WaitForAnimation, WaitForAudio)
- **Plugin System** — IPlugin interface, DLL/SO hot-reload with state save/restore, manifest JSON, editor panel
- **Flash Compatibility** — ~40 Flash API shim bindings, AS2/AS3 transpiler, SWF binary import to ECS entities
- **DataAssets** — Schema definitions with typed instances, JSON I/O, script bindings

</details>

<details>
<summary><b>Procedural Generation</b> — Reaction-diffusion, physarum, CA, SDF, 4D</summary>
<br>

- **Reaction-Diffusion** — Gray-Scott Turing patterns, 9 presets, bake-to-texture and heightmap export
- **Cellular Automata** — 7 CA rules, 3 mesh modes (Voxels, Marching Cubes, Point Cloud)
- **Physarum Simulation** — Agent-based slime mold networks, 5 presets, food sources, trail diffusion
- **Fourier Meshes** — DFT contour decomposition, progressive reconstruction, 3D extrusion
- **4D Polytopes** — 5 polytopes (Tesseract through 120-Cell), 6 rotation planes, stereographic projection
- **Advanced** — Inverse/differentiable rendering, non-Euclidean portals, metaballs, voxel cone tracing, SDF rendering, framebuffer feedback (8 presets), screen-space distortion (7 types), IK-driven mesh deformation, interactive water, mesh audio reactivity

</details>

<details>
<summary><b>Accessibility</b> — 20+ features for inclusive game development</summary>
<br>

- **Vision** — Colorblind correction (8 GPU modes), high contrast themes (WCAG AAA 7:1+), font scaling (0.5-3.0x), colorblind-safe palettes with patterns
- **Motor** — Remappable input with hold/toggle, one-handed presets, dwell-click, sticky drag, adjustable thresholds, switch access (auto-scan)
- **Cognitive** — Dyslexia mode (OpenDyslexic font, letter/word/line spacing), reduced motion, content warnings per scene
- **Communication** — Screen reader (priority-queued announcer), subtitles (configurable size/background/speaker/direction), audio-visual indicators
- **Alternative Input** — Switch access, eye tracking, sip-and-puff, head tracking support
- **Quick Presets** — Low Vision, Motor Impaired, Photosensitive, Reset All

</details>

<details>
<summary><b>Build & Distribution</b> — Pack, export, ship to desktop and web</summary>
<br>

- **Asset Pipeline** — `.enjpak` archive with compression, CRC32 integrity, build dialog in editor
- **Desktop** — Windows EXE + Inno Setup installer, Linux AppImage, standalone player
- **Web** — HTML5/WebAssembly via Emscripten, WebGPU renderer, Newgrounds integration (medals, scoreboards, cloud saves)
- **Asset Libraries** — 42 bundled fonts (8 categories), 16 CC0 3D model packs, 15 CC0 2D sprite/tileset packs
- **Import** — Presets for 10 DCC tools (Blender, Maya, 3ds Max, Houdini, etc.), texture compression (BCn/ASTC), auto-thumbnails
- **Adaptive Quality** — FPS-based auto-adjustment of render scale, shadow quality, and particle count

</details>

---

## Architecture

```
TEGE/
├── Core/           Foundation layer — memory, math, logging, platform abstraction
├── Engine/         Engine layer — renderer, ECS, audio, effects, editor, build, scripting
│   └── shaders/    GLSL shaders (compiled to SPIR-V) + WGSL for WebGPU
├── Editor/         Editor application (ImGui-based)
├── Player/         Standalone game player (no editor UI)
├── Tests/          1100+ unit and integration tests
├── third_party/    GLFW, ImGui, ImGuizmo
└── build/          Build output
```

## Technology

| | |
|:---|:---|
| **Language** | C++20 |
| **Graphics** | Vulkan 1.3 + WebGPU (web) |
| **Audio** | miniaudio (public domain) |
| **Windowing** | GLFW3 (zlib/libpng) |
| **3D Import** | Assimp (BSD) |
| **UI** | Dear ImGui + ImGuizmo (MIT) |
| **JSON** | nlohmann/json (MIT) |
| **Physics** | Jolt 5.2.0 (MIT) + Box2D 3.0.0 (MIT) |
| **Build** | CMake 3.20+ |

All dependencies use permissive open-source licenses.

## Build Options

| Option | Default | Description |
|:-------|:--------|:------------|
| `ENJIN_BUILD_EDITOR` | ON | Editor application |
| `ENJIN_BUILD_PLAYER` | ON | Standalone game player |
| `ENJIN_BUILD_TESTS` | OFF | 1100+ unit tests |
| `ENJIN_PHYSICS_JOLT` | ON | Jolt 5.2.0 (3D physics) |
| `ENJIN_PHYSICS_BOX2D` | ON | Box2D 3.0.0 (2D physics) |
| `ENJIN_CLUSTERED_LIGHTING` | ON | Clustered forward lighting |
| `ENJIN_VRS` | OFF | Variable Rate Shading |
| `ENJIN_VIRTUAL_TEXTURING` | OFF | Virtual texturing |
| `ENJIN_VISIBILITY_BUFFER` | OFF | Visibility buffer render path |

---

## Documentation

| | |
|:---|:---|
| [Architecture](docs/ARCHITECTURE.md) | System design and diagrams |
| [Build Guide](docs/BUILD.md) | Prerequisites and platform instructions |
| [User Manual](docs/USER_MANUAL.md) | Components and editor guide |
| [Scripting API](docs/SCRIPTING_API.md) | Complete AngelScript reference (721 bindings) |
| [Roadmap](docs/ROADMAP.md) | Planned work and progress |

---

## About

I use AI heavily in implementation. I design, direct, and debug everything as rigorously as I know how. I am a single human trying to build accessible software far beyond my station.

I cannot guarantee the safety or security of this software at this stage — I am one person. This software is provided as-is, without warranty of any kind. I am not liable for any damages arising from its use. By downloading and using TEGE, you accept that I am doing my best and you are placing your trust in me. I will do everything in my power to make this safe, secure software, and I hope that commitment becomes a guarantee when we reach a stable release.

Thank you.

## License

Licensed under the **Business Source License 1.1** (BSL 1.1). See [LICENSE](LICENSE) for details.

**You are free to use TEGE to create and sell games.** The only restriction is that you cannot fork the engine source and sell it as a competing engine product. The source code becomes **Apache 2.0** four years after each release.

## Contributing

TEGE is not accepting outside contributions at this time. If you find a bug or have a feature request, feel free to [open an issue](https://github.com/MartyChouette/TEGE/issues).
