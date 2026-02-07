# Enjin Engine

A proprietary, licensable game engine built from scratch using C++20 and the Vulkan graphics API. Features a complete editor with ImGui, an Entity-Component-System architecture, and modern rendering capabilities.

## Features

### Rendering
- **Vulkan Renderer** - Modern graphics with Blinn-Phong lighting, PBR materials, and deferred rendering framework
- **Cascaded Shadow Maps** - 4-cascade CSM with PCF filtering, texel stabilization, distance fade, per-cascade bias
- **PBR Material System** - Base color, metallic, roughness, emissive, normal mapping, parallax occlusion mapping
- **Post-Processing** - Bloom, vignette, color grading, FXAA, film grain, tone mapping
- **Retro Effects** - PSX-style flat shading, affine texturing, vertex snapping, stipple transparency, CRT scanlines, dithering, color quantization
- **Weather System** - Rain, snow, fog, storms with toggleable lightning
- **Water Rendering** - 3D water plane with Gerstner waves, shore foam, freeze system, ocean mode
- **Skybox** - Procedural gradient sky, solid color, or six-face cubemap with rotation and sun direction
- **Vegetation** - Instanced grass, shrub, and tree rendering with wind sway
- **Terrain** - 3D heightmap terrain with sculpting brushes (raise, lower, flatten, smooth, paint) and 2D polyline terrain with drag-to-edit control points
- **Multiple Light Sources** - Directional, point, and spot lights
- **GPU Skinning** - Skeletal animation via bone matrix SSBO
- **Wireframe Rendering** - Toggle wireframe mode with wide line support
- **World Curvature** - Vertex-shader horizon bending effect
- **Render-to-Texture** - Offscreen rendering for Game View with separate uniform buffers
- **Per-Scene Render Settings** - Full rendering config per scene with project-level defaults and editor UI
- **Particle System** - CPU particle simulation (5 emitter shapes, size/speed curves, gravity/drag) with GPU instanced billboard rendering
- **Shadow Quality Settings** - Configurable resolution (512-4096), shadow distance, and shadow strength

### Editor
- **Full ImGui Editor** - Hierarchy, inspector, viewport, effects, and settings panels
- **Transform Gizmos** - Translate, rotate, scale via ImGuizmo
- **Entity Selection** - Click-to-select with ray casting, double-click to focus
- **Play Mode** - Play/pause/stop game preview with input isolation
- **Game View** - Renders from in-scene camera components independently from the editor camera
- **Scene Serialization** - JSON save/load with full component support
- **Undo/Redo** - Command-pattern undo/redo system
- **Entity Clipboard** - Cut/copy/paste entities via JSON serialization
- **Native File Dialogs** - Cross-platform (Win32, macOS osascript, Linux zenity/kdialog)
- **Startup Templates** - 15 templates (Blank, 2D Platformer, 2D Top-Down, 3D Isometric, 3D Third/First Person, Visual Novel, RPG Village, Survival, Game Manager, 3D Narrative, 4P Racing, Arena Fighter, PS1 RPG, City Builder)
- **Custom Templates** - Save/load from templates/ directory
- **Terrain Brushes** - Viewport sculpting with 5 brush modes (raise, lower, flatten, smooth, paint), adjustable radius/strength/falloff, real-time cursor feedback
- **Stats Overlay** - FPS, frame time, draw calls, triangle count
- **Skybox Panel** - Dedicated panel with procedural presets (Midday, Sunset, Dawn, Night, Overcast)
- **Asset Hot-Reload** - File watcher polls texture files for changes
- **Build Dialog** - Configure and export standalone game builds from the editor
- **Particle Editor** - 7 presets, color gradient bar, size/speed curves, shape preview, playback controls
- **UI Editor** - Viewport WYSIWYG editing with click-select, drag-move, resize handles, and element tree
- **Project Settings** - Dedicated panel for rendering/physics defaults separated from editor preferences
- **Profiler Panel** - Per-frame breakdown, FPS graph, scope-based profiling with ENJIN_PROFILE_SCOPE macro
- **Multi-Select** - Ctrl+click toggle, Shift+click range, viewport marquee/rubber-band selection with batch transform
- **Animation Graph** - Visual state machine editor for AnimatorComponent with Entry pseudo-node, transitions, parameters
- **Dialogue Editor** - Visual dialogue tree editor with 7 node types, EntityEventBus integration, SubtitleSystem support
- **Visual Script Editor** - Blueprint-style visual scripting with 50+ nodes, breakpoint debugging, execution profiler

### Entity-Component System
- **60+ Component Types** - Full inspector UI for all components
- **Character Controllers** - Platformer 2D, Top-Down 2D/3D, Third Person, First Person
- **Camera Component** - In-game cameras with projection settings and frustum visualization
- **Physics** - Collision detection (sphere-sphere, AABB-AABB, sphere-AABB), ground detection
- **Gravity Zones** - Per-entity gravity override with directional, point, and zero-G modes
- **Temperature Zones** - Heat/cold environmental effects
- **Camera Trigger Zones** - Camera override volumes
- **Text Rendering** - TextComponent with stb_truetype rasterization to texture
- **Vegetation Components** - Grass, shrub, tree volume definitions
- **Physics Joints** - 6 joint types (Distance, Hinge, BallSocket, Spring, Fixed, Slider) with breakable mode
- **Ragdoll System** - Bone-to-joint mapping, animation-to-ragdoll blend, auto-settle
- **LOD System** - Distance-based mesh swapping
- **Level Streaming** - Chunk-based distance loading with priority queue and async support
- **Runtime UI** - Anchor-based layout, 8 widget types, event bus, 4 theme presets

### Animation
- **Skeletal Animation** - glTF skin/joint/animation import, GPU skinning, auto-play
- **Animation State Machines** - FSM with blending and transitions
- **2D Sprite Animation** - Frame-based flipbook animation
- **Inverse Kinematics** - LookAt IK, FABRIK chain solving, interaction IK

### Audio
- **Cross-Platform Audio** - miniaudio backend (WAV, MP3, FLAC)
- **3D Spatialization** - Positional audio with distance attenuation models
- **Multi-Channel Mixing** - Multiple simultaneous sounds
- **Category Volumes** - Separate master, SFX, music, ambient, voice volumes
- **Scene Serialization** - AudioSource and AudioListener components saved/loaded with scenes

### Accessibility
- **Editor Themes** - Dark, Light, High Contrast Dark, High Contrast Light
- **Colorblind Correction** - 8 GPU modes (protanopia, deuteranopia, tritanopia, anomalous variants, achromatopsia)
- **Remappable Input** - Semantic game actions with hold/toggle modes and one-handed presets
- **Reduced Motion** - Weather particle reduction, head-bob disable
- **Subtitles** - Configurable font size, background, speaker names, direction indicators
- **Content Warnings** - Per-scene warning flags with dismissable overlay
- **Quick Presets** - Low Vision, Motor Impaired, Photosensitive, Reset All

### Scene Management
- **Project File Format** - .enjinproject JSON manifest
- **Scene Manager** - Project manifests, scene lists, build indices
- **Scene Transitions** - Instant, Fade Black, Fade White, Cross Fade with configurable duration
- **Prefab System** - Save/load entity templates

### Gameplay Systems
- **Save/Load System** - 10-slot save system with quick save/load support
- **HUD Overlay** - Health bars, resource bars, labels, and crosshair rendering
- **Quest/Objective Tracking** - Start, complete, and fail quests with objective tracking
- **Damage Resistance/Weakness** - Per-type damage multipliers for resistance and weakness
- **Stamina/Resource System** - Generic resource with regeneration and controller integration
- **Footstep Audio** - Surface-based footstep sounds with walk/run interval support
- **Object Pooling** - Entity recycling with configurable pool sizes and auto-release
- **Cinematic Camera** - Waypoint sequences with easing curves for cutscenes
- **Entity Event Bus** - Decoupled C++ entity communication system
- **Raw Mouse Input** - Bypass OS mouse acceleration with smoothing options

### Build & Distribution
- **Build Pipeline** - Scan project → validate assets → compress/obfuscate → pack into `.enjpak` with CRC32 integrity verification
- **Asset Packer** - `.enjpak` archive format with compression, XOR obfuscation, and per-file CRC32 checksums
- **Build Dialog** - Editor UI for configuring and running builds with progress tracking
- **Build Manifest** - Window title, resolution, fullscreen, and start scene baked into the pack
- **Standalone Player** - Editor-free runtime that loads `game.enjpak`, reads the build manifest, and runs the game loop

### Scripting & Extensibility
- **AngelScript Integration** - TegeBehavior base class, ~170 API bindings, hot-reload
- **Script Coroutines** - YieldSeconds, YieldFrames, StartCoroutine for async game logic
- **Script Event System** - String-named events with typed EventData payloads
- **Plugin System** - IPlugin interface, DLL/SO loading, manifest JSON, editor panel
- **C++ Hot-Reload** - File watching, DLL reload with state save/restore
- **Animation Timeline** - Property/event/animation tracks with easing, loop, and ping-pong modes

### Visual Scripting
- **Blueprint-Style Editor** - Node graph visual programming without code
- **50+ Built-in Nodes** - Events, flow control, math, logic, transform, components, audio, debug
- **Latent Nodes** - Delay, WaitForAudioComplete, WaitForAnimationComplete for multi-frame operations
- **Variable System** - Bool, Int, Float, String, Vector3, Entity variables with exposed option
- **Breakpoint Debugging** - F9 toggle breakpoint, F5 continue, F10 step-through
- **Execution Profiler** - Color-coded timeline of node execution with duration tooltips
- **Multi-Select Editing** - Box selection, Ctrl+click, multi-node drag, copy/paste with preserved links
- **Undo/Redo** - Full undo support for nodes, links, properties, and variables
- **Collision Callbacks** - OnCollisionEnter/Exit, OnTriggerEnter/Exit events

## Project Structure

```
enjin/
├── Core/           # Foundation layer (Memory, Math, Logging, Platform)
├── Engine/         # Engine layer (Renderer, ECS, Audio, Effects, Editor, Build, Assets)
├── Editor/         # Editor application entry point
├── Player/         # Standalone game player entry point
├── third_party/    # External dependencies (GLFW, ImGui, ImGuizmo)
└── build/          # Build output (bin/, lib/)
```

## Roadmap

### Phase 1: Foundation ✅
- [x] Memory Management (Stack, Pool, Linear allocators)
- [x] Math Library (Vectors, Matrices, Quaternions, Splines)
- [x] Logging System (Thread-safe, categorized)
- [x] Platform Abstraction Layer
- [x] Entry Point Abstraction

### Phase 2: Vulkan Renderer ✅
- [x] Vulkan Context Initialization
- [x] Swapchain Management
- [x] Command Buffer System
- [x] SPIR-V Shader Pipeline
- [x] Depth Buffer / Z-testing
- [x] Blinn-Phong Lighting
- [x] Uniform Buffer Objects (MVP, Lighting, Material)

### Phase 3: Engine Core ✅
- [x] ECS (Entity Component System)
- [x] glTF Asset Loading (.gltf/.glb)
- [x] Scene Importer (glTF to ECS conversion)
- [x] Input System (Keyboard/Mouse)
- [x] Camera System (Fly camera with WASD + mouse)

### Phase 4: Editor Tooling ✅
- [x] Editor GUI (Dear ImGui integration)
- [x] Scene Hierarchy Panel
- [x] Entity Inspector Panel (50+ component types)
- [x] Transform Gizmos (ImGuizmo - translate/rotate/scale)
- [x] Entity Selection via Ray Casting
- [x] Viewport Panel with camera controls
- [x] Settings Panel (gizmo options, render settings)
- [x] Stats Overlay (FPS, frame time, draw calls, triangles)
- [x] Play Mode (play/pause/stop)
- [x] Undo/Redo System
- [x] Entity Clipboard (Cut/Copy/Paste)
- [x] Startup Template Selector (15 templates)

### Phase 5: Advanced Rendering ✅
- [x] PBR Material System (baseColor, metallic, roughness, emissive)
- [x] Alpha cutoff / transparency support
- [x] Multiple Light Sources (point, spot, directional)
- [x] Cascaded Shadow Maps (4-cascade CSM with PCF, texel stabilization, distance fade)
- [x] Texture Support (albedo, normal, height, metallic-roughness, emissive)
- [x] Normal Mapping (tangent-space)
- [x] Parallax Occlusion Mapping
- [x] Post-Processing Effects (bloom, tone mapping, vignette, color grading, FXAA, film grain)
- [x] Retro Effects (PSX, CRT, dithering, vertex jitter)
- [x] Weather System (rain, snow, fog, storms)
- [x] Water Rendering (Gerstner waves, shore foam, freeze, ocean)
- [x] Environment Mapping / Skybox
- [x] Render-to-Texture (Game View offscreen rendering)
- [x] Wireframe Rendering
- [x] GPU-Driven Frustum Culling
- [x] Deferred Rendering Framework

### Phase 6: Production Features ✅
- [x] Scene Serialization (JSON save/load)
- [x] Undo/Redo System (command pattern)
- [x] Prefab System (save/load entity templates)
- [x] Asset Hot-Reloading (file watcher)
- [x] Scene Management (project manifests, scene lists)
- [x] Scene Transitions (fade, cross-fade)
- [x] Native File Dialogs (cross-platform)

### Phase 7: Animation & Audio ✅
- [x] 2D Sprite Animation (frame-based, flipbook)
- [x] 3D Skeletal Animation (bone hierarchy, GPU skinning)
- [x] Animation Blending & State Machines
- [x] Inverse Kinematics (LookAt, FABRIK)
- [x] Audio System (miniaudio - cross-platform, multi-channel)
- [x] 3D Spatialized Audio

### Phase 8: AI & Procedural Generation ✅
- [x] Spline System (Linear, Bezier, Catmull-Rom, B-Spline)
- [x] Enemy AI Behaviors (patrol, chase, flee, attack patterns)
- [x] AI State Machines (FSM with transitions)
- [x] 2D Procedural Level Generation (prefab-based)
- [x] 3D Procedural Level Generation (room/corridor system)
- [x] Navmesh Generation & Pathfinding (A*)

### Phase 9: Gameplay Systems ✅
- [x] Character Controllers (5 types)
- [x] Gravity Zones
- [x] Temperature Zones
- [x] Camera Trigger Zones
- [x] Wind System with Vegetation Sway
- [x] Terrain Editing with Brushes
- [x] World Time & Seasonal Weather
- [x] In-Game Text Rendering

### Phase 10: Accessibility ✅
- [x] Editor Themes (4 themes)
- [x] GPU Colorblind Correction (8 modes)
- [x] Remappable Input System
- [x] Reduced Motion Support
- [x] Subtitle/Caption System
- [x] Content Warning System
- [x] Accessibility Quick Presets

### Phase 11: Distribution
- [x] Standalone Game Player
- [x] Asset Pack Build Pipeline (.enjpak)
- [x] Splitscreen Rendering
- [x] Scripting Language (AngelScript with hot-reload, coroutines, event bus)
- [x] Visual Scripting System (Blueprint-style nodes, debugging, profiler)
- [x] Dialogue Tree Editor (7 node types, EntityEventBus, SubtitleSystem)
- [ ] Networking (client-server, peer-to-peer)

### Phase 12: Advanced Gameplay
- [x] Save/Load System (10-slot persistence)
- [x] HUD Overlay System
- [x] Quest/Objective System
- [x] Damage Resistance System
- [x] Stamina/Resource System
- [x] Footstep Audio System
- [x] Object Pooling
- [x] Cinematic Camera System
- [x] Entity Event Bus
- [x] Raw Mouse Input + Smoothing
- [x] Window Icon Support

## Editor Controls

| Action | Control |
|--------|---------|
| Move Camera | `W/A/S/D` |
| Look Around | Hold Right-click + Mouse |
| Camera Up | `Space` / `E` |
| Camera Down | `Q` / `Ctrl` |
| Sprint | `Shift` |
| Select Entity | Left-click in viewport |
| Focus Entity | Double-click entity |
| Adjust Move Speed | Scroll wheel |
| Translate Gizmo | `1` |
| Rotate Gizmo | `2` |
| Scale Gizmo | `3` |
| Toggle Local/World | `4` |
| Toggle Multi-Select | `Ctrl+click` |
| Range Select (Hierarchy) | `Shift+click` |
| Marquee Select | Viewport drag |
| Undo | `Ctrl+Z` |
| Redo | `Ctrl+Y` |

**Visual Script Editor:**
| Action | Control |
|--------|---------|
| Toggle Breakpoint | `F9` |
| Continue Execution | `F5` |
| Step Over | `F10` |
| Copy Nodes | `Ctrl+C` |
| Cut Nodes | `Ctrl+X` |
| Paste Nodes | `Ctrl+V` |
| Duplicate Nodes | `Ctrl+D` |
| Delete Nodes | `Delete` |

## Skybox

The engine includes a dedicated Skybox panel (View > Skybox) for configuring the scene background.

**Supported types:**
- **None** - No skybox rendered
- **Procedural** - Gradient sky with configurable top, horizon, and bottom colors plus sun direction
- **Solid Color** - Single flat color fill
- **Cubemap** - Six-face cubemap with individual texture paths (Right, Left, Top, Bottom, Front, Back)

**Procedural presets:**
Quick-apply presets that configure colors and sun direction in one click:
- **Midday** - Bright blue sky with overhead sun
- **Sunset** - Warm orange horizon with low sun
- **Dawn** - Soft pinks and purples with rising sun
- **Night** - Deep dark sky with sun below horizon
- **Overcast** - Flat grey tones with diffused light

All non-None types support a rotation slider (0-360 degrees) around the Y axis. Skybox configuration is persisted with scene save/load, including sun direction.

## Building

### Prerequisites
- CMake 3.20+
- C++20 compatible compiler (GCC 10+, Clang 12+, MSVC 2019+)
- Vulkan SDK
- GLFW3

### Build Instructions

**Linux / macOS:**
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
```

**Windows:**
```cmd
mkdir build && cd build
cmake .. -G "Visual Studio 16 2019" -A x64
cmake --build . --config Release
```

**See [BUILD.md](docs/BUILD.md) for detailed build instructions on all platforms.**

### Build Options
- `ENJIN_BUILD_EDITOR=ON` - Build the editor (default: ON)
- `ENJIN_BUILD_PLAYER=ON` - Build the standalone game player (default: ON)
- `ENJIN_BUILD_TESTS=OFF` - Build unit tests (default: OFF)
- `ENJIN_BUILD_EXAMPLES=OFF` - Build example projects (default: OFF)

### Running
```bash
# Editor
./build/bin/Release/EnjinEditor.exe   # Windows
./build/bin/EnjinEditor               # Linux/macOS

# Standalone Player (requires game.enjpak in same directory)
./build/bin/Release/EnjinPlayer.exe   # Windows
./build/bin/EnjinPlayer               # Linux/macOS
```

## Technology Stack

- **Language**: C++20
- **Graphics API**: Vulkan 1.3
- **Audio**: miniaudio (public domain)
- **Windowing**: GLFW3 (zlib/libpng)
- **3D Import**: Assimp (BSD)
- **UI**: Dear ImGui (MIT) + ImGuizmo (MIT)
- **JSON**: nlohmann/json (MIT)
- **Build System**: CMake

## License Compatibility

All dependencies use permissive licenses compatible with proprietary licensing:
- GLFW3: zlib/libpng (permissive)
- Vulkan SDK: Apache 2.0 (permissive)
- Dear ImGui: MIT (permissive)
- ImGuizmo: MIT (permissive)
- Assimp: BSD (permissive)
- miniaudio: Public domain (permissive)
- nlohmann/json: MIT (permissive)
- stb libraries: Public domain (permissive)

## License

Proprietary - All rights reserved.
