<div align="center">

<img src="installer/social_preview.png" alt="TEGE, The Enjin Game Engine" width="720">

<br><br>

# TEGE

**A game engine for 2D and 3D games, with an editor, renderer, physics, scripting, audio, and build pipeline. Written from scratch in C++20.**

<br>

[![License: BSL 1.1](https://img.shields.io/badge/License-BSL_1.1-blue.svg)](LICENSE)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C.svg?logo=cplusplus&logoColor=white)](https://en.cppreference.com/w/cpp/20)
[![Vulkan 1.3](https://img.shields.io/badge/Vulkan-1.3-AC162C.svg?logo=vulkan&logoColor=white)](https://www.vulkan.org/)
[![Platform](https://img.shields.io/badge/Platform-Windows%20%7C%20Linux%20%7C%20Web-lightgrey.svg)]()
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
<br><sub><b>3D Third Person</b> · PBR renderer, cascaded shadows, terrain</sub>
</td>
<td width="33%" align="center">
<img src="Engine/previews/platformer/frame_0.png" alt="2D Platformer" width="100%">
<br><sub><b>2D Platformer</b> · Sprite rendering, Box2D physics, particles</sub>
</td>
<td width="33%" align="center">
<img src="Engine/previews/topdown2d/frame_0.png" alt="Top-Down Dungeon" width="100%">
<br><sub><b>Top-Down Dungeon</b> · Tile-based levels, AI, lighting</sub>
</td>
</tr>
</table>

---

## What it is

TEGE covers the whole path from an empty project to a shipped build: scene editing, rendering,
physics, scripting, audio, and one-click export to Windows, Linux, and the browser.

Art direction is a first-class control rather than specialist work. Art style presets, retro
rendering modes, and post-process stacks are dropdowns in the editor. So is accessibility:
colorblind modes, remappable input, subtitles, text scaling, and a screen reader announcer are
built in, and every exported game ships with the accessibility menu.

Scenes, projects, prefabs, and shader graphs are plain JSON. Scripts are plain text. The editor
runs offline with no account and no license server.

---

## Features

**Rendering.** PBR materials, 4-cascade shadow maps, point and spot shadows, TAA/FXAA/SMAA,
bloom, depth of field, tone mapping, post-process volumes, reflection probes, clustered forward
lighting, HiZ occlusion culling, FSR 2 upscaling.

**Art styles.** One-click presets from PS1 vertex wobble to painterly. Retro modes: dithering,
CRT scanlines, affine texture mapping, palette quantization, flat and Gouraud shading. Custom
shader graph with live apply.

**Ray tracing.** Experimental. Path tracer with NEE and MIS, hybrid RT shadows, reflections, AO
and GI, three denoisers, ReSTIR light sampling. Activates on capable hardware.

**Simulation.** Cloth with tearing and wind, weather zones with temperature-driven rain and snow,
seasons, water as 2D volumes and 3D surfaces with buoyancy, fluid simulation, GPU particles with
world collision, instanced grass, shrubs and trees, destructibles and Voronoi fracture.

**Procedural generation.** Dungeon generator, wave function collapse in 2D and 3D, Poisson and
Voronoi scatter that conforms to terrain, terrain generation with erosion and auto-splatting.
Seeded and replay-stable.

**Gameplay.** Character controllers for first person, third person, 2D platformer, top-down,
grid dungeon crawler, and vehicles. Quests, dialogue trees, cinematics, saves, inverse kinematics,
skeletal animation with blend trees, behavior trees, navmesh pathfinding.

**Time rewind and replays.** A snapshot ring usable as a gameplay mechanic and as the editor's
debug timeline. Sessions export as plain-JSON replay files with deterministic input playback and
bookmarks.

**Editor.** Hierarchy, inspector, viewport, undo everywhere, command palette, multi-select,
gizmos, terrain brushes, git integration. Every component panel documents what it does and what
it connects to. Non-destructive scene variant layers. Visual scripting with breakpoints. Built-in
pixel editor, vector drawing, and sprite sheet importer. Real-time collaborative editing.

**Audio.** miniaudio backend, 3D spatialization, buses, audio event graphs, audio-reactive
visuals, MIDI input, Steam Audio HRTF with occlusion and reverb.

**Integration.** AngelScript with a beginner-friendly `TegeBehavior` skeleton, and C++ underneath.
An optional localhost MCP server exposes scene, component, script, and graph access to an AI
agent of your choice. LAN multiplayer, HTTP client, webhooks.

Full detail in the [User Manual](docs/USER_MANUAL.md) and [Architecture](docs/ARCHITECTURE.md).

---

## Quick start

### Build from source

```bash
git clone https://github.com/MartyChouette/TEGE.git && cd TEGE
mkdir build && cd build
cmake ..
cmake --build . --config Release

./bin/Release/EnjinEditor.exe   # Windows
./bin/EnjinEditor               # Linux
```

**Prerequisites:** CMake 3.20+, a C++20 compiler, Vulkan SDK, GLFW3.
See [BUILD.md](docs/BUILD.md) for platform detail.

### Windows installer

Pre-built, no build tools required. Download the latest
[release](https://github.com/MartyChouette/TEGE/releases/latest), run the installer, and launch
from the Start Menu. It registers the `.enjinproject` and `.enjin` file associations.

### Enjin::App

Vulkan rendering without the editor:

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

Build with `-DENJIN_BUILD_EXAMPLES=ON`. See `Examples/Simple3D/`, `Examples/Simple2D/`, and
`Examples/OrbCollector/`.

---

## Documentation

| | |
|---|---|
| [Beginner's Guide](docs/BEGINNERS_GUIDE.md) | Start here |
| [User Manual](docs/USER_MANUAL.md) | The editor, component by component |
| [Scripting API](docs/SCRIPTING_API.md) | AngelScript reference |
| [Architecture](docs/ARCHITECTURE.md) | How the engine is put together |
| [Build Guide](docs/BUILD.md) | Platform setup |
| [FAQ](docs/FAQ.md) | Common questions |
| [Progress](docs/PROGRESS.md) | Where the engine is on the way to 1.0 |

---

## License

Business Source License 1.1. Free for personal, educational, and commercial game development.
Converts to Apache 2.0 after four years. See [LICENSE](LICENSE) and
[THIRD_PARTY_LICENSES.md](THIRD_PARTY_LICENSES.md).
