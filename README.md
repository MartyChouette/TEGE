# Enjin Engine

A proprietary, licensable game engine built from scratch using C++20 and the Vulkan graphics API. Features a complete editor with ImGui, an Entity-Component-System architecture, and modern rendering capabilities.

## Features

### Rendering
- **Vulkan Renderer** - Modern graphics with Blinn-Phong lighting, PBR materials, and deferred rendering framework
- **Shadow Mapping** - 4-cascade CSM for directional lights, cubemap array shadows for point lights (up to 4), 2D array shadows for spot lights (up to 4), 16-sample Poisson disk PCF soft shadows, configurable softness radius, texel stabilization, distance fade, pipeline depth bias, per-entity shadow dither (by darkness/distance/angle) with 6 built-in patterns (Bayer 4x4/8x8, Blue Noise, Halftone, Crosshatch, Overlook)
- **PBR Material System** - Base color, metallic, roughness, emissive, normal mapping, parallax occlusion mapping (4 modes: Basic/Steep/Occlusion/Relief), transmission/IOR/thickness for refractive materials, subsurface scattering (intensity/radius/color), receiveShadows toggle, dithered gradient banding (2-8 bands, 6 dither patterns), material presets (Glass, Water, Skin, Leaf)
- **Post-Processing** - Bloom, vignette, color grading, FXAA, film grain, tone mapping, full-screen stipple/dither (8 combinable patterns, 3 color modes), post-process volumes with spatial blending (Box/Sphere shapes, priority-based, smoothstep falloff, selective override mask)
- **Retro Effects** - PSX-style flat shading, affine texturing, vertex snapping, stipple transparency, CRT scanlines, dithering, color quantization
- **Weather System** - Rain, snow, fog, storms with toggleable lightning
- **Water Rendering** - 3D water plane with Gerstner waves, shore foam, freeze system, ocean mode
- **Skybox** - Procedural gradient sky, solid color, or six-face cubemap with rotation and sun direction
- **Vegetation** - Instanced grass, shrub, and tree rendering with wind sway
- **Terrain** - 3D heightmap terrain with sculpting brushes (raise, lower, flatten, smooth, paint) and 2D polyline terrain with drag-to-edit control points
- **Multiple Light Sources** - Directional, point, and spot lights with shadow support for all three types
- **GPU Skinning** - Skeletal animation via bone matrix SSBO
- **Wireframe Rendering** - Toggle wireframe mode with wide line support
- **World Curvature** - Vertex-shader horizon bending effect
- **Render-to-Texture** - Offscreen rendering for Game View with separate uniform buffers
- **Per-Scene Render Settings** - Full rendering config per scene with project-level defaults and editor UI
- **Particle System** - CPU particle simulation (5 emitter shapes, size/speed curves, gravity/drag) with GPU instanced billboard rendering
- **Shadow Quality Settings** - Configurable resolution (512-4096), shadow distance, shadow strength, per-entity dither modes, point/spot shadow light selection by intensity/distance scoring
- **GPU Frustum Culling** - Automatic culling of off-screen entities before draw submission
- **Sprite Texture Atlas** - Auto-packing small sprites into a single GPU texture for batched draws
- **Descriptor Set Caching** - Per-entity texture caching with material sort for minimal GPU descriptor writes
- **Ray Tracing Pipeline** - RT shadows, reflections, AO, GI, translucency (refraction/SSS), caustics (photon-traced), path tracing with 3 denoiser options (SVGF compute, OIDN Intel neural, OptiX NVIDIA CUDA), real depth buffer, RT composition pass
- **SH Light Probes** - L2 spherical harmonics irradiance probes with grid generation, baking, and renderer integration (LightingUBO + ambient blending)
- **SDF Scene** - CPU-side signed distance field evaluation with 6 primitives, 6 boolean ops (incl. smooth), GPU buffer packing
- **Order-Independent Transparency** - Weighted Blended OIT (McGuire & Bavoil 2013) with accumulation + revealage textures, fullscreen composite pipeline
- **Depth of Field** - Bokeh DoF with aperture shapes (circle/hexagon/octagon), focal distance/range, CoC debug visualization
- **Tilt-Shift** - Miniature/toy-model post-process blur with configurable focus band and falloff
- **Camera Presets** - 9 built-in presets (Isometric 45/30, TopDown, SideScroller, FPS, TPS, Cinematic, SecurityCam, BirdsEye)

### Editor
- **Full ImGui Editor** - Hierarchy, inspector, viewport, effects, and settings panels
- **Transform Gizmos** - Translate, rotate, scale via ImGuizmo
- **Entity Selection** - Click-to-select with ray casting, double-click to focus
- **Play Mode** - Play/pause/stop game preview with input isolation, scene changes persist on stop
- **Game View** - Renders from in-scene camera components independently from the editor camera
- **Scene Serialization** - JSON save/load with full component support
- **Undo/Redo** - Command-pattern undo/redo system
- **Entity Clipboard** - Cut/copy/paste entities via JSON serialization
- **Native File Dialogs** - Cross-platform (Win32, macOS osascript, Linux zenity/kdialog)
- **Startup Templates** - 44 templates across 7 categories (Foundations, Genre Showcases, Systems Deep-Dives, Retro & Flash, Advanced, Multiplayer, Debug/Test)
- **Template Creator** - Save current scene as reusable template with metadata (View > Tools > Template Creator), custom templates stored in templates/ directory
- **Template Marketplace** - Bundled catalog of 15 curated templates across 5 categories with search, filter by category, sort by name/rating/downloads, install/uninstall (View > Tools > Template Marketplace)
- **Terrain Brushes** - Viewport sculpting with 5 brush modes (raise, lower, flatten, smooth, paint), adjustable radius/strength/falloff, real-time cursor feedback
- **Stats Overlay** - FPS, frame time, draw calls, triangle count
- **Skybox Panel** - Dedicated panel with procedural presets (Midday, Sunset, Dawn, Night, Overcast)
- **Asset Hot-Reload** - File watcher polls texture files for changes
- **Build Dialog** - Configure and export standalone game builds from the editor
- **Particle Editor** - 12 presets (inspector dropdown + dedicated panel), color gradient bar, size/speed curves, shape preview, playback controls
- **UI Editor** - Viewport WYSIWYG editing with click-select, drag-move, resize handles, and element tree
- **Project Settings** - Dedicated panel for rendering/physics/networking defaults separated from editor preferences
- **Profiler Panel** - Per-frame breakdown, FPS graph, scope-based profiling with ENJIN_PROFILE_SCOPE macro
- **Multi-Select** - Ctrl+click toggle, Shift+click range, viewport marquee/rubber-band selection with batch transform
- **Animation Graph** - Dual-mode visual state machine editor: AnimatorComponent mode (clip dropdown from loaded animations, speed, blend/exit time, ASM parameters) and StateMachineComponent mode (game logic SM with script callbacks), Entry pseudo-node, transitions, play-mode state highlighting
- **Shader Graph** - Visual shader authoring with 54 node types, topological sort GLSL code generation, .enjshader save/load
- **Audio Event Graph** - Dynamic audio mixing with runtime execution (trigger events, parameter thresholds, delay scheduling), .enjaudiopkg save/load
- **Particle Graph** - Visual particle system authoring with compiler to ParticleEmitterComponent, .enjparticle save/load
- **Dialogue Editor** - Visual dialogue tree editor with 7 node types, EntityEventBus integration, SubtitleSystem support
- **Visual Script Editor** - Blueprint-style visual scripting with 146+ nodes, breakpoint debugging, execution profiler
- **Bug Reporting & Feedback** - Built-in bug reports with auto-captured diagnostics, feedback with satisfaction ratings, JSON persistence, remote submission (Help > Report Bug)
- **Vector Drawing Editor** - 7 shape types, layers, undo/redo, SVG export, snap-to-grid, zoom/pan
- **HTML5 Export** - Generate web-ready HTML5 builds with preloader and responsive scaling
- **Newgrounds Game Page** - Themed HTML5 export template with medal sidebar, scoreboard, embed codes, and NG.io API integration
- **Command Palette** - Ctrl+P fuzzy-search popup with 25+ commands for quick access
- **Project Hub** - Startup wizard with template browser, recent projects, and project creation flow
- **Entity Icons** - Bracket-tag icons in hierarchy by component type ([C] Camera, [L] Light, [M] Mesh, etc.)
- **Empty State Patterns** - Helpful empty-state messages with call-to-action buttons in all panels
- **Collaborative Editing** - Real-time multi-user scene editing with OT protocol, peer cursors, conflict resolution, lock enforcement
- **Symbol Library** - Reusable graphic symbols as prefabs with nested editing, category browser, update propagation, Flash timeline sync
- **Notification Toasts** - Stacked bottom-right toast notifications (Info/Success/Warning/Error) with slide-in animation and fade-out, wired to save/build/template events
- **Accent Color Presets** - 6 harmony presets (Default Blue, Warm Orange, Forest Green, Royal Purple, Crimson Red, Teal) with auto-derived accent colors and fine-tune controls
- **Theme Preview** - 250x160 live preview pane in Editor Settings showing current theme appearance in real-time
- **Keyboard Shortcuts Help** - Ctrl+Shift+/ searchable modal listing all editor shortcuts grouped by category (General, Viewport, Selection, Play Mode, Editor)

### Entity-Component System
- **70+ Component Types** - Full inspector UI for all components
- **Character Controllers** - Platformer 2D, Top-Down 2D/3D, Third Person, First Person
- **Camera Component** - In-game cameras with projection settings and frustum visualization
- **Physics** - `IPhysicsBackend` abstraction with Jolt Physics v5.2.0 (3D) and Box2D v3.0.0 (2D) backends enabled by default, plus legacy SimplePhysics fallback (compile-optional via `ENJIN_PHYSICS_SIMPLE`). Collision detection, 2D/3D ground detection for character controllers, debug wireframes for colliders and joints
- **Gravity Zones** - Per-entity gravity override with directional, point, and zero-G modes
- **Temperature Zones** - Heat/cold environmental effects
- **Camera Trigger Zones** - Camera override volumes
- **Text Rendering** - TextComponent with stb_truetype rasterization to texture
- **Vegetation Components** - Grass, shrub, tree volume definitions
- **Physics Joints** - 6 joint types (Distance, Hinge, BallSocket, Spring, Fixed, Slider) with breakable mode
- **Ragdoll System** - Bone-to-joint mapping, animation-to-ragdoll blend, auto-settle
- **LOD System** - Distance-based mesh swapping
- **Level Streaming** - Chunk-based distance loading with priority queue and async support
- **Runtime UI** - Anchor-based layout, 8 widget types, event bus, 6 theme presets (incl. high contrast), font scaling, accessible labels
- **Behavior Tree AI** - 20 node types with visual editor, blackboard system, play-mode debugging
- **Dialogue Box** - Auto-built UICanvas dialogue display with speaker, portrait, choices
- **Surface Aligned Controller** - Planet gravity walking on spherical surfaces

### Animation
- **Skeletal Animation** - glTF and Assimp (FBX/DAE/3DS/20+ formats) skin/joint/animation import, bone weight extraction, skeleton building, GPU skinning, auto-play first clip
- **Animation State Machines** - FSM with blending and transitions
- **2D Sprite Animation** - Frame-based flipbook animation
- **Inverse Kinematics** - LookAt IK, FABRIK chain solving, interaction IK

### Audio
- **Cross-Platform Audio** - miniaudio backend (WAV, MP3, FLAC, Vorbis)
- **3D Spatialization** - Positional audio with distance attenuation models
- **Audio Channels** - SFX/Music/UI/Voice with independent volume (Music/UI force non-diegetic 2D)
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
- **Keyboard Navigation** - Panel focus shortcuts (Ctrl+1-5), gizmo nudge (Arrow/PageUp/PageDown), focus rings
- **Motor Accessibility** - Adjustable click/drag thresholds, dwell-click, sticky drag, motor impaired preset
- **Command Palette** - Ctrl+P for keyboard-driven command access
- **Alternative Input** - Switch access, eye tracking, sip-and-puff, head tracking support
- **Scene & Entity Locking** - Advisory .enjinlock files for collaborative editing, stale lock detection, collaborative editing UI (peer cursors, conflict dialog, session management)
- **Screen Reader Support** - Priority-queued text announcer with visual status bar
- **Audio Visual Indicators** - Colored dot overlays for audio events
- **High Contrast UI Themes** - HighContrastDark and HighContrastLight presets with WCAG AAA 7:1+ contrast ratios
- **Font Scaling** - Runtime font size multiplier for UISystem (0.5-3.0x)
- **Accessible Labels** - Per-element screen reader labels on UICanvas elements
- **Dyslexia Mode** - Configurable letter/word/line spacing for improved readability
- **Colorblind-Safe Theme** - Blue/orange palette universally distinguishable across all color vision types
- **Colorblind-Safe UI Palettes** - 9 palettes with patterns (Stripes/Dots/Crosshatch/Chevron) + icons alongside color
- **Switch Access** - One-button auto-scan mode for UICanvas focus navigation
- **OpenDyslexic Font** - FontLibrary with Default/Monospace/OpenDyslexic families, letter/word/line spacing controls
- **Motor Accessibility Runtime** - Dwell-click and sticky drag on UICanvas elements, configurable timings
- **Content Warnings in Player** - Pre-scene dismissable warning overlay driven by per-scene content flags

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
- **Destructible Environments** - 4 fracture patterns (Voronoi, Grid, Radial, Shatter) with debris physics
- **2D Physics** - Circle, box, polygon shapes with 5 joint types, CCD, SAT collision
- **Fluid-Terrain Coupling** - Bidirectional FluidSimulation-to-TerrainComponent coupling with erosion and accumulation modes
- **Localization** - String tables, CSV/JSON I/O, parameterized strings, LOC() macro
- **Dialogue Assets** - .enjdlg dialogue files with visual tree editor
- **Reaction-Diffusion** - Gray-Scott model Turing pattern simulation with 9 presets, bake-to-texture and heightmap export
- **Cellular Automata Geometry** - 7 CA rules (GameOfLife, BriansBrain, Rule110, etc.) with 3 mesh modes (Voxels, Marching Cubes, Point Cloud)
- **Physarum Simulation** - Agent-based slime mold network generation with 5 presets, food sources, trail diffusion/decay
- **Timeline Editor** - Flash-style keyframe animation editor with layers, 4 interpolation modes (Constant/Linear/Bezier/CatmullRom), curve editor, onion skinning, auto-key
- **Fourier Transform Meshes** - DFT decomposition of 2D contours, progressive reconstruction animation, 3D extrusion from contour
- **4D Stereographic Projection** - 5 polytopes (Tesseract, 5-Cell, 16-Cell, 24-Cell, 120-Cell), 6 rotation planes, wireframe mesh generation
- **Inverse/Differentiable Rendering** - CPU-based scene parameter optimization via gradient descent with finite differences
- **Non-Euclidean Geometry** - Portal rendering with stencil-buffer recursion, hyperbolic/spherical/toroidal space warping, oblique near-plane clipping
- **Metaball / Blob Rendering** - Implicit surface field evaluation with marching cubes mesh extraction, gradient-based normals, per-group color blending
- **Voxel Cone Tracing (VXGI)** - Voxel grid with mip chain, conservative triangle rasterization, cone-traced diffuse/specular GI, AO, and volumetric god rays
- **SDF Rendering** - Mesh-to-SDF conversion, sphere tracing, marching cubes isosurface extraction, 8SSEDT text rendering with outline/shadow, SDF volume blending
- **Framebuffer Feedback Effects** - Ping-pong compositing with 8 presets (Echo, Melt, InfiniteMirror, VHS, Kaleidoscope, Phosphor, DreamSequence), 5 blend modes
- **Screen-Space Distortion** - 7 distortion types (HeatHaze, Shockwave, Underwater, PortalEdge, Ripple, BarrelFisheye, Custom), composited UV offset field
- **IK-Driven Mesh Deformation** - FABRIK solver with Verlet physics, Catmull-Rom spline subdivision, tube/ribbon mesh generation for tentacles/ropes/tails
- **Interactive Water** - Grid-based spring-damper wave propagation, object splashes and V-wakes, buoyancy, shallow/deep/foam color blending, boundary modes
- **Mesh Audio Reactivity** - Cooley-Tukey radix-2 FFT, bass/mid/treble band analysis, per-vertex displacement with 4 mapping modes and 4 displacement axes

### Asset Libraries
- **Font Library** - 42 curated OFL/Apache fonts across 8 categories (Sans-Serif, Serif, Monospace, Display, Handwriting, Pixel, Fantasy, Sci-Fi) with editor browser, search, and install
- **3D Asset Library** - 16 CC0 3D model packs (Kenney, Quaternius) across Architecture, Nature, Props, Characters, Vehicles, Weapons, Dungeon, Sci-Fi
- **2D Asset Library** - 15 CC0 2D sprite/tileset/UI packs across UI Kits, Tilesets, Sprites, VFX, Backgrounds, Textures

### Build & Distribution
- **Build Pipeline** - Scan project → validate assets → compress/obfuscate → pack into `.enjpak` with CRC32 integrity verification
- **Asset Packer** - `.enjpak` archive format with compression, XOR obfuscation, and per-file CRC32 checksums
- **Build Dialog** - Editor UI for configuring and running builds with progress tracking
- **Build Manifest** - Window title, resolution, fullscreen, and start scene baked into the pack
- **Standalone Player** - Editor-free runtime that loads `game.enjpak`, reads the build manifest, and runs the game loop with full particle, subtitle, announcer, alternative input, and post-processing support
- **Import Presets** - Source-app import presets for 10 DCC tools (Blender, Maya, 3ds Max, Houdini, Cinema 4D, ZBrush, Substance Painter, Unreal, Unity, SketchUp) with auto-detection and per-axis flip toggles
- **Texture Compression** - CPU-side BCn/ASTC compression (BC1/BC3/BC4/BC5/BC7, ASTC 4x4/6x6/8x8) with mipmap generation and quality presets
- **Asset Thumbnails** - Auto-generated preview thumbnails for images, 3D models (software rasterizer), and scenes with caching
- **Binary Distribution** - CMake install rules + CPack config for Windows ZIP + NSIS installer (Start Menu shortcuts, file associations, uninstaller), one-command build scripts (package.bat/package.sh)
- **Linux AppImage** - AppImageBuilder for Linux packaging with .desktop file generation
- **Adaptive Quality** - FPS-based auto-adjustment of render scale, shadow quality, and particle count (5 quality levels)

### Scripting & Extensibility
- **AngelScript Integration** - TegeBehavior base class, ~721 API bindings (incl. AI/BT, accessibility, physics 2D, networking, procedural gen, audio graph, plugins, MIDI, input actions, screen-space effects, HUD widgets, text components, particle presets, material transmission/SSS, Flash API shim), hot-reload
- **Script Coroutines** - YieldSeconds, YieldFrames, StartCoroutine for async game logic
- **Script Event System** - String-named events with typed EventData payloads
- **Plugin System** - IPlugin interface with PluginContext (World, RenderSystem, ScriptEngine, Audio, SceneManager), PluginSDK.h single-header, state save/restore for hot-reload, DLL/SO loading, manifest JSON, editor panel
- **C++ Hot-Reload** - File watching, DLL reload with state save/restore
- **Animation Timeline** - Property/event/animation tracks with easing, loop, and ping-pong modes; Flash-style timeline editor with layers, curve editor, and onion skinning
- **Newgrounds.io API** - Session management, medals, scoreboards, cloud saves for web games
- **DataAsset System** - Schema definitions with typed instances, JSON I/O, script bindings
- **Flash API Shim** - ~40 AngelScript bindings emulating Flash APIs (DisplayObject, MovieClip, Stage, Mouse, TextField, Sound, Timer)
- **AS2/AS3 Transpiler** - Pattern-based ActionScript to AngelScript conversion (type mapping, class syntax, Flash API calls)
- **SWF Converter** - SWF binary import to ECS entities (shapes→sprites, MovieClips→entity hierarchy with timeline)

### Platform Support
- **Linux** - LinuxPlatform helpers (XDG paths, zenity dialogs, fork/exec), AppImage packaging, CMake Linux targets
- **Steam Deck** - Auto-detection, adaptive quality, gyro input stubs, suspend/resume lifecycle, Steam Input API stubs
- **Nintendo Switch 1** - NVN render backend stub, Joy-Con/touch/docked mode stubs (requires licensed devkit)
- **Hub Application** - Standalone project launcher with project manager, engine version manager, template browser

### Visual Scripting
- **Blueprint-Style Editor** - Node graph visual programming without code
- **146+ Built-in Nodes** - Events, flow control, math, logic, transform, physics, AI/BT, accessibility, tweens, dialogue, audio, audio graph, plugins, noise, streaming, networking, procedural generation, HUD widgets, text, particle presets, material transmission/SSS, debug
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
- [x] Startup Template Selector (44 templates)

### Phase 5: Advanced Rendering ✅
- [x] PBR Material System (baseColor, metallic, roughness, emissive)
- [x] Alpha cutoff / transparency support
- [x] Multiple Light Sources (point, spot, directional)
- [x] Cascaded Shadow Maps (4-cascade CSM with PCF, texel stabilization, distance fade, shadow dither)
- [x] Point/Spot Light Shadow Maps (cubemap array for point, 2D array for spot, Poisson disk soft shadows)
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
- [x] 3D Procedural Level Generation (room/corridor system, fractal terrain, advanced L-system)
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
- [x] Keyboard Navigation (panel focus, gizmo nudge)
- [x] Motor Accessibility (dwell-click, sticky drag, thresholds)
- [x] Command Palette (Ctrl+P, 25+ commands)
- [x] Alternative Input Devices (switch, eye tracking, sip-and-puff)
- [x] Scene & Entity Locking
- [x] Screen Reader Announcer
- [x] Audio Visual Indicators

### Phase 11: Distribution
- [x] Standalone Game Player
- [x] Asset Pack Build Pipeline (.enjpak)
- [x] Splitscreen Rendering
- [x] Scripting Language (AngelScript with hot-reload, coroutines, event bus)
- [x] Visual Scripting System (Blueprint-style nodes, debugging, profiler)
- [x] Dialogue Tree Editor (7 node types, EntityEventBus, SubtitleSystem)
- [x] Behavior Tree Editor (20 node types, visual editor)
- [x] Bug Reporting & Feedback System
- [x] Vector Drawing Editor (SVG)
- [x] HTML5 Export
- [x] Newgrounds.io API Integration
- [x] Procedural Generation (9+ algorithms incl. fractal terrain + erosion + 3D L-system, graph editor)
- [x] Destructible Environments
- [x] 2D Physics System
- [x] Localization System
- [x] LAN Multiplayer (host-authoritative UDP, HMAC-SHA256 auth, 20Hz entity sync, RPC, lobby, reliable delivery)

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
- [x] Dialogue Assets (.enjdlg)
- [x] DataAsset System
- [x] Tweening (25 easing functions)

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
