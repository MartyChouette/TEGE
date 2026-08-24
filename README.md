<div align="center">

<img src="installer/social_preview.png" alt="TEGE — The Enjin Game Engine" width="720">

<br><br>

# TEGE

**An opinionated, aesthetic-forward game engine that gets you building in three clicks and gives greater accessibility to obsolete rendering styles.**

*The GarageBand of open source game engines.*

<br>

[![License: BSL 1.1](https://img.shields.io/badge/License-BSL_1.1-blue.svg)](LICENSE)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C.svg?logo=cplusplus&logoColor=white)](https://en.cppreference.com/w/cpp/20)
[![Vulkan 1.3](https://img.shields.io/badge/Vulkan-1.3-AC162C.svg?logo=vulkan&logoColor=white)](https://www.vulkan.org/)
[![Platform](https://img.shields.io/badge/Platform-Windows%20%7C%20Linux-lightgrey.svg)]()
[![CI](https://github.com/MartyChouette/TEGE/actions/workflows/ci.yml/badge.svg)](https://github.com/MartyChouette/TEGE/actions/workflows/ci.yml)
[![Tests](https://img.shields.io/badge/Tests-2%2C000%2B_passing-brightgreen.svg)](https://github.com/MartyChouette/TEGE/actions/workflows/ci.yml)

<br>

[Website](https://www.marty64.net/enjin/) · [Download](https://github.com/MartyChouette/TEGE/releases/latest) · [Beginner's Guide](docs/BEGINNERS_GUIDE.md) · [Documentation](docs/) · [Scripting API](docs/SCRIPTING_API.md)

</div>

---

<table>
<tr>
<td width="33%" align="center">
<img src="Engine/previews/thirdperson/frame_0.png" alt="3D Third Person" width="100%">
<br><sub><b>3D Third Person</b> -- PBR renderer, cascaded shadows, terrain</sub>
</td>
<td width="33%" align="center">
<img src="Engine/previews/platformer/frame_0.png" alt="2D Platformer" width="100%">
<br><sub><b>2D Platformer</b> -- Sprite rendering, Box2D physics, particles</sub>
</td>
<td width="33%" align="center">
<img src="Engine/previews/topdown2d/frame_0.png" alt="Top-Down Dungeon" width="100%">
<br><sub><b>Top-Down Dungeon</b> -- Tile-based levels, AI, lighting</sub>
</td>
</tr>
</table>

---

## Why TEGE?

TEGE is a complete game engine -- editor, renderer, physics, scripting, audio, build pipeline -- written from scratch. It's built for artists, educators, and new videogame makers of all backgrounds: people who want to make something that looks like them and plays for everyone, without first becoming a graphics programmer or an accessibility consultant. How your game looks and who can play it are choices you make in a dropdown, not specialist work you bolt on.

Your game is a file you own: readable JSON scenes, plain-text scripts, documented formats, no account, no license server, works fully offline. See [OPENNESS.md](docs/OPENNESS.md) for the policy in writing.

The engine is free and open source now. A paid official 1.0 release (installer, pre-built binaries, updates -- the Aseprite model) launches in spring 2027. You are always free to build from source, and games you make with TEGE are yours to sell.

---

## Everything in the box

<table>
<tr valign="top">
<td width="50%">

### 🎨 Look and feel
One-click art style presets, PS1 wobble to painterly · retro effects: dithering, CRT, palette crush, the gatekept 4th to 6th gen console looks · ray tracing when hardware allows: RT shadows, reflections, AO, GI, path tracing with denoisers · **Gaussian splat import**: photoreal phone captures as scene objects · custom shader graph with live apply · post-process volumes, reflection probes, lens effects · procedural skies for 2D and 3D with custom cloud textures · non-Euclidean spaces, 4D projection, framebuffer feedback, the weird stuff

</td>
<td width="50%">

### 🌦️ World and simulation
Dynamic fabric: tearable cloth that catches the wind · weather in zones: rain, snow, sleet by temperature, seasons that change the trees · water three ways: 2D volumes, 3D surfaces with buoyancy, and fluid simulation that carves terrain · GPU particles with world collision · living vegetation: grass, shrubs, trees swaying with the wind · destructibles, Voronoi fracture, metaballs, reaction-diffusion, cellular automata geometry · an elemental system where fire spreads and water douses · swarms, gravity zones, wind as a first-class system

</td>
</tr>
<tr valign="top">
<td width="50%">

### 🎲 Procedural generation
Dungeon generator · wave function collapse in 2D and 3D · scatter with Poisson and Voronoi modes that conforms to terrain · terrain generator with erosion and auto-splatting · random bags with Markov chains for loot and sequencing · everything seeded and replay-stable

</td>
<td width="50%">

### 🕹️ Play and feel
Character controllers for every genre: first person, third person, 2D platformer, top-down, dungeon crawler grid, vehicles · **time rewind** as a game mechanic and a debug tool · **shareable replays**: record a session, export one file, someone else watches it, with bookmarks that mark bugs by themselves · surface response: footsteps and impacts that know what they hit · quests, dialogue trees, cinematics, tiered saves, dynamic difficulty · IK, skeletal animation with blend trees, morph targets, motion matching

</td>
</tr>
<tr valign="top">
<td width="50%">

### 🎚️ Sound
DAW-style audio: buses, event graphs, audio-reactive visuals that pulse to the music · spatial audio with HRTF, occlusion, and per-zone reverb through Steam Audio · MIDI input as a controller

</td>
<td width="50%">

### 🧰 The editor
Build within three clicks: templates, primitives, drag and drop everything · **every component explains itself**: what it does, how to use it, what it connects to · the wiring board: flip any entity into a node map of its connections · **the layers concept**: non-destructive scene variant layers · a DAW-style play transport: record, scrub backward through time, step frame by frame · visual scripting with breakpoints, behavior trees, quest flow graphs · real-time collaborative editing · built-in pixel editor, vector drawing, sprite sheet importer with auto collider tracing · terrain brushes, command palette, undo everywhere, git integration

</td>
</tr>
<tr valign="top">
<td width="50%">

### 🔓 Openness
**Your game is a file you own**: readable JSON scenes, plain-text scripts, documented formats · AngelScript with a beginner-friendly TegeBehavior skeleton, deep C++ when you want it · **an MCP server**: any AI agent becomes a copilot, with scene, component, script, and graph access · one-click desktop builds, web export that runs in a browser · LAN multiplayer, HTTP client, webhooks

</td>
<td width="50%">

### ♿ Accessibility from the ground up
Colorblind modes, screen reader announcer, subtitles, content warnings · remappable input, alternative input devices, reduced motion honored everywhere · OpenDyslexic built in, audio-visual indicators · **every exported game ships with the accessibility menu**

</td>
</tr>
</table>

---

## Quick Start

### Build from Source

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

**Prerequisites:** CMake 3.20+, a C++20 compiler, Vulkan SDK, GLFW3. See [BUILD.md](docs/BUILD.md) for detailed platform instructions.

**First time?** The [Beginner's Guide](docs/BEGINNERS_GUIDE.md) is the end-to-end cheat sheet; the [User Manual](docs/USER_MANUAL.md) walks through the editor.

### Windows Installer

A pre-built Windows installer is available -- no build tools required:

1. Download the latest release from **[GitHub Releases](https://github.com/MartyChouette/TEGE/releases/latest)** (or [the website](https://www.marty64.net/enjin/))
2. Run the installer -- it sets up the editor, player, and file associations (`.enjinproject`, `.enjin`)
3. Launch TEGE from the Start Menu or desktop shortcut

### Enjin::App -- Minimal Rendering

For users who want Vulkan rendering without the full editor:

```cpp
#include "Enjin/App.h"

class MyApp : public Enjin::App {
    void OnStart() override {
        AddPlane({0, 0, 0}, 20.0f);
        AddCube({0, 0.5f, 0});
        AddDirectionalLight({0.5f, -1, -0.3f});
        SetCameraPosition({0, 4, 8});
    }
};
ENJIN_SIMPLE_MAIN(MyApp)
```

Build with `-DENJIN_BUILD_EXAMPLES=ON`. See `Examples/Simple3D/`, `Examples/Simple2D/`, and `Examples/OrbCollector/` (a small complete game).

---

## The deep dive

<details>
<summary><b>Rendering</b></summary>
<br>

- **PBR Materials** -- Base color, metallic, roughness, emissive, normal mapping, parallax occlusion (4 modes), transmission/IOR/thickness, subsurface scattering, matcap, procedural surface noise, material presets
- **Shadow Mapping** -- 4-cascade CSM, cubemap point shadows, spot shadows, 16-sample Poisson PCF, 6 dither patterns, shaped shadows for masked materials
- **Anti-Aliasing** -- TAA (Halton jitter, velocity reprojection), FXAA, SMAA
- **Post-Processing** -- Bloom, vignette, color grading, film grain, tone mapping, depth of field, tilt-shift, post-process volumes with spatial blending, in-game options menus with live split-screen effect preview
- **Gaussian Splats** -- Import .ply (INRIA 3DGS) and .spz (Niantic) captures as scene entities; sorted, blended gaussians that take every art style and post effect
- **Ray Tracing (experimental, in progress)** -- Path tracer with NEE, MIS, Russian Roulette, Cook-Torrance BRDF. 3 denoisers (SVGF, Intel OIDN, NVIDIA OptiX). ReSTIR light sampling with temporal and spatial reuse. Hybrid RT effects (shadows, reflections, AO, GI)
- **Upscaling** -- FSR 2 (built-in Lanczos + CAS). 4 quality modes. DLSS/XeSS available when vendor SDKs are linked
- **Retro Effects** -- PSX vertex snapping, affine textures, flat/Gouraud shading, CRT scanlines (11 models), VHS, film gate weave, light leaks, 6 named palettes (PICO-8, Game Boy, NES, CGA, C64)
- **Environment** -- Procedural sky for 2D and 3D scenes with custom cloud textures, cubemap skybox, weather zones (rain, snow, fog, storms, temperature-driven sleet), water with Gerstner waves, instanced vegetation with wind and seasons
- **Optimizations** -- GPU two-phase HiZ occlusion culling, clustered forward lighting, batched material SSBO, multi-draw indirect, async compute, SIMD math, per-frame linear allocator, descriptor set caching

</details>

<details>
<summary><b>Editor</b></summary>
<br>

- **Core** -- Hierarchy, inspector, viewport, play/pause/stop, undo/redo, cut/copy/paste, multi-select (Ctrl+click, Shift+range, marquee), entity icons, inspector lock, selection sync
- **Self-Documenting** -- Every component panel explains what it does, how to use it, and what it connects to, with one-click adds for missing partners; Tab flips the inspector into a wiring-board node map of the entity
- **Play Transport** -- DAW-style strip: record dot with session time, step back/forward, timeline scrubber over the whole recorded session, replay export and playback, F8 bookmarks (script errors bookmark themselves)
- **Gizmos** -- Translate/rotate/scale via ImGuizmo, click-to-select, terrain sculpting (5 brush modes), stats overlay, wireframe toggle
- **Visual Authoring** -- Shader Graph (54 nodes), Audio Event Graph, Particle Graph, Dialogue Tree Editor, Animation Graph, Vector Drawing Editor, Pixel Editor
- **Visual Scripting** -- Blueprint-style editor, 347 nodes, breakpoint debugging (F9/F5/F10), execution profiler
- **Layers** -- Non-destructive scene variant layers: capture, toggle, and resolve variations of a scene without branching the file
- **Project Tools** -- Project Hub with 16 built-in starter templates, Template Creator, Template Marketplace
- **Smart Features** -- Context-aware suggestions, Quick Setup Patterns, Command Palette (Ctrl+P), keyboard shortcuts help
- **Debug** -- Game Debug Panel (F1), Debug Workstation (F2), Quake-style drop-down console (backtick, 60+ commands), play mode diff
- **Collaboration** -- Real-time multi-user editing with OT protocol, peer cursors, conflict resolution
- **MCP Server** -- Localhost, off by default: 18 tools for scene manipulation, component CRUD, script editing with compile diagnostics, graph authoring, async builds, screenshots. Bring your own agent; see [MCP.md](docs/MCP.md)
- **Build & Export** -- Async builds that keep the editor responsive, HTML5/WebAssembly export, standalone player builds
- **Drag & Drop** -- Drag textures onto viewport entities or inspector fields, drag models to import, drag prefabs to instantiate, drag assets between folders

</details>

<details>
<summary><b>ECS & Gameplay</b></summary>
<br>

- **150+ Component Types** with full inspector UI, all serialized through one registry
- **Character Controllers** -- Platformer 2D, Top-Down 2D/3D, Third Person, First Person, Surface Aligned, vehicles, dungeon-crawler grid movement
- **Physics** -- Jolt 5.2.0 (3D) + Box2D 3.0.0 (2D), collision detection, ground detection, sensor bodies, debug wireframes, 6 joint types, ragdolls, exact triangle-mesh colliders for terrain and level geometry
- **Procedural Generation** -- Dungeon generator, WFC (2D tile and 3D module modes), Scatter (uniform, Poisson, jittered grid, Voronoi; terrain-conforming), terrain generation with thermal and hydraulic erosion plus slope/height auto-splatting, RandomBag with Markov mode
- **Rewind & Replays** -- Record-and-rewind snapshot ring usable as a gameplay mechanic (keys, charges, cooldowns) and as the editor debug timeline; shareable plain-JSON .tegereplay files with deterministic input playback, recorded end states, and bookmarks
- **AI** -- Behavior Trees (20 node types, blackboard, visual editor), navmesh pathfinding (A*)
- **UI Runtime** -- Anchor-based layout, 8 widget types, 6 theme presets, font scaling, accessible labels
- **Save/Load** -- 10-slot system with quick save/load, tiered saves, pluggable backends
- **Quest System** -- Start/complete/fail quests with objective tracking, quest flow graphs
- **Dialogue** -- Auto-built dialogue display with speaker, portrait, choices; Yarn/Twine import
- **Surface Response** -- Materials carry footstep and impact sounds plus particles; characters and collisions play them automatically in 2D and 3D
- **Combat** -- Damage resistance/weakness multipliers, stamina/resource system
- **Localization** -- String tables, CSV/JSON I/O, parameterized strings, LOC() macro
- **Dynamic Difficulty** -- Auto-adjusts AI aggression, damage, resources based on player performance

</details>

<details>
<summary><b>Animation & Audio</b></summary>
<br>

- **Skeletal Animation** -- glTF + Assimp import (FBX/DAE/20+ formats), GPU skinning, animation state machines with blending, retargeting
- **2D Sprite Animation** -- Frame-based flipbook animation
- **Inverse Kinematics** -- LookAt IK, FABRIK chain solving, interaction IK
- **Timeline Editor** -- Keyframe animation with layers, 4 interpolation modes, curve editor, onion skinning
- **Audio** -- miniaudio backend (WAV, MP3, FLAC, Vorbis), 3D spatialization, SFX/Music/UI/Voice buses, audio event graphs, audio-reactive system (beat sync, VU-driven visuals), MIDI input, Steam Audio HRTF/occlusion/reverb

</details>

<details>
<summary><b>Scripting & Plugins</b></summary>
<br>

- **AngelScript** -- TegeBehavior base class, 1,100+ API bindings, hot-reload, coroutines, event system, sandboxed with instruction limits
- **Visual Scripting** -- 347 node types, breakpoint debugging, execution profiler, latent nodes
- **Plugin System** -- IPlugin interface, DLL/SO hot-reload with state save/restore
- **DataAssets** -- Schema definitions with typed instances, JSON I/O, script bindings

</details>

<details>
<summary><b>Accessibility</b></summary>
<br>

- **Vision** -- Colorblind correction (8 GPU modes), high contrast themes (7:1+ contrast), font scaling, colorblind-safe palettes
- **Motor** -- Remappable input, one-handed presets, dwell-click, sticky drag, switch access
- **Cognitive** -- Dyslexia mode (OpenDyslexic font, spacing adjustments), reduced motion, content warnings
- **Communication** -- Screen reader, subtitles (configurable size/background/speaker), audio-visual indicators
- **Quick Presets** -- Low Vision, Motor Impaired, Photosensitive, Reset All

</details>

<details>
<summary><b>Build & Distribution</b></summary>
<br>

- **Asset Pipeline** -- `.enjpak` archive with compression and CRC32 integrity
- **Desktop** -- Windows EXE + Inno Setup installer, Linux AppImage, standalone player
- **Web** -- HTML5/WebAssembly via Emscripten, WebGPU renderer
- **Asset Libraries** -- 42 bundled fonts, 16 CC0 3D model packs, 15 CC0 2D sprite/tileset packs
- **Import** -- Presets for 10 DCC tools, texture compression (BCn/ASTC), auto-thumbnails

</details>

---

## Architecture

```
TEGE/
├── Core/           Foundation -- memory allocators, math, logging, platform abstraction
├── Engine/         Engine -- renderer, ECS, physics, audio, editor, scripting, build
│   └── shaders/    GLSL shaders (compiled to SPIR-V)
├── Editor/         Editor application (ImGui)
├── Player/         Standalone game player (no editor UI)
├── Tests/          2,000+ unit and integration tests (18 categories)
├── third_party/    GLFW, ImGui, ImGuizmo
└── build/          Build output
```

The engine is split into two layers: **Core** (zero engine dependencies -- math, memory, logging, platform) and **Engine** (everything else). The Editor and Player are thin applications that compose Engine systems. All rendering goes through Vulkan 1.3 with a WebGPU path for web exports.

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
| `ENJIN_BUILD_TESTS` | OFF | Unit tests (output: `bin/Tests/`) |
| `ENJIN_BUILD_EXAMPLES` | OFF | Enjin::App examples (output: `bin/Examples/`) |
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
| [Beginner's Guide](docs/BEGINNERS_GUIDE.md) | The end-to-end cheat sheet |
| [User Manual](docs/USER_MANUAL.md) | Editor walkthrough and component reference |
| [Architecture](docs/ARCHITECTURE.md) | System design and diagrams |
| [Build Guide](docs/BUILD.md) | Prerequisites and platform instructions |
| [Scripting API](docs/SCRIPTING_API.md) | Complete AngelScript reference (1,100+ bindings) |
| [MCP Server](docs/MCP.md) | Editor tools for AI agents |
| [Openness](docs/OPENNESS.md) | Your work outlives this engine: the policy |
| [Roadmap](docs/ROADMAP.md) | Planned work and progress |

---

## Contributing

TEGE is not accepting outside contributions at this time. If you find a bug or have a feature request, please [open an issue](https://github.com/MartyChouette/TEGE/issues).

## About

I use AI heavily in implementation. I design, direct, and debug everything as rigorously as I know how. I am a single human trying to build accessible software far beyond my station.

I cannot guarantee the safety or security of this software at this stage -- I am one person. This software is provided as-is, without warranty of any kind. I am not liable for any damages arising from its use. By downloading and using TEGE, you accept that I am doing my best and you are placing your trust in me. I will do everything in my power to make this safe, secure software, and I hope that commitment becomes a guarantee when we reach a stable release.

Thank you.

## License

Licensed under the **Business Source License 1.1** (BSL 1.1). See [LICENSE](LICENSE) for full terms.

**You are free to use TEGE to create and sell games.** The only restriction is that you cannot fork the engine source and sell it as a competing engine product. The source code becomes **Apache 2.0** four years after each release.
