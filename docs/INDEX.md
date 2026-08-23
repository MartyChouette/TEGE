# Documentation Index

**TEGE — The Enjin Game Engine**
*An aesthetics-first game engine — preserving and expanding the digital aesthetics of yesterday for the storytellers of tomorrow.*

See [About](../README.md#about) for the developer disclaimer.

---

## Getting Started

1. **[README.md](../README.md)** - Project overview and feature list
2. **[BUILD.md](BUILD.md)** - Consolidated build guide (all platforms, dependencies, troubleshooting)
3. **[USER_MANUAL.md](USER_MANUAL.md)** - Editor usage, scripting guide, and workflow reference
4. **[TUTORIALS.md](TUTORIALS.md)** - 55 hands-on tutorials covering all engine features (foundations, 2D/3D, scripting, game systems, AI, VFX, audio, procedural gen, networking, building, accessibility, advanced topics)

## Architecture & Design

- **[ARCHITECTURE.md](ARCHITECTURE.md)** - System architecture overview (rendering, ECS, physics, scripting, plugins, streaming)
- **[CODING_STANDARDS.md](CODING_STANDARDS.md)** - Coding style and documentation standards
- **[ROADMAP.md](ROADMAP.md)** - Technical roadmap, performance findings, node graph expansion, GUI modernization plans
- **[DEVJOURNAL.md](DEVJOURNAL.md)** - Development journal with daily progress, decisions, and implementation notes
- **[SECURITY_AUDIT.md](SECURITY_AUDIT.md)** - Security & robustness audit report (38 findings across serialization, networking, renderer, future-proofing; 35 resolved)
- **[AUDIT_2026_04_12.md](AUDIT_2026_04_12.md)** - Full engine audit: memory safety, thread safety, Vulkan, security, logic, performance (55 findings, 6 fixed, 42 remaining)
- **[ENGINE_ANALYSIS.md](ENGINE_ANALYSIS.md)** - Comprehensive technical analysis with Mermaid architecture diagrams, feature completeness matrix, market positioning, revenue models, performance diagnostics, and technical debt assessment

## API Reference

- **[API_REFERENCE.md](API_REFERENCE.md)** - C++ API documentation
- **[SCRIPTING_API.md](SCRIPTING_API.md)** - AngelScript scripting API reference (1,100+ functions)

## Key Systems

| System | Header | Description |
|--------|--------|-------------|
| Scripting | `Enjin/Scripting/` | AngelScript VM, TegeBehavior, bindings, coroutines, events |
| Physics | `Enjin/Physics/` | IPhysicsBackend / IPhysicsBackend2D abstraction — Jolt v5.2 (3D) and Box2D v3 (2D), strictly separated per scene |
| Profiler | `Enjin/Debug/Profiler.h` | Frame profiling, scope timers, ImGui overlay |
| Plugins | `Enjin/Plugin/PluginSystem.h` | Dynamic library loading, manifest system |
| Timeline | `Enjin/Animation/Timeline.h` | Property/event/animation tracks, keyframe sequencing |
| Hot-Reload | `Enjin/Plugin/HotReload.h` | C++ gameplay DLL hot-reload with state save/restore |
| Streaming | `Enjin/Scene/LevelStreaming.h` | Chunk-based level streaming with priority queue |
| Terrain | `Enjin/ECS/Components/Terrain.h` | 3D heightmap sculpting (5 brush modes) and 2D polyline terrain |
| AI | `Enjin/AI/AIBehaviors.h` | State-based AI, navmesh generation, A* pathfinding |
| Render Backend | `Enjin/Renderer/RenderBackend.h` | Platform abstraction interface |
| Ray Tracing | `Enjin/Renderer/RayTracing/` | RT pipeline, acceleration structures, SVGF/OIDN/OptiX denoisers, path tracer, material SSBO (binding 9), motion vectors (binding 4) |
| Feedback | `Enjin/Editor/FeedbackSystem.h` | Bug reporting, feedback collection, diagnostics capture, JSON persistence |
| Vector Drawing | `Enjin/Editor/VectorDrawingEditor.h` | 7 shape types, layers, SVG export, Flash symbol library |
| HTML5 Export | `Enjin/Build/HTML5Exporter.h` | Canvas export, preloader, responsive scaling, web embed |
| 2D Physics | `Enjin/Physics/Physics2D.h` | Circle/Box/Polygon shapes, 5 joint types, CCD, SAT collision |
| Localization | `Enjin/GUI/Localization.h` | String tables, CSV/JSON I/O, LOC() macro, parameterized strings |
| Behavior Trees | `Enjin/AI/BehaviorTree.h` | 20 node types, visual editor, blackboard, play-mode debugging |
| Procedural Gen | `Enjin/Procedural/ProceduralAlgorithms.h` | 9 algorithms, visual node graph editor, preview canvas |
| Destructible | `Enjin/Effects/Destructible.h` | 4 fracture patterns, debris physics, chain destruction |
| Networking | `Enjin/Networking/LANMultiplayer.h` | Host-authoritative UDP, prediction, interpolation, RPC, lobby |
| Save System | `Enjin/Gameplay/TieredSaveSystem.h` | 20-slot tiered saves (SceneState/RunState/MetaProgression), auto-save, checkpoints, cloud backends |
| Save Backends | `Enjin/Gameplay/SaveBackend.h` | ISaveBackend interface + Local/Steam implementations |
| Play Mode Diff | `Enjin/Editor/PlayModeDiff.h` | JSON diff of pre/post play scene states, cherry-pick apply dialog |
| Reaction-Diffusion | `Enjin/Effects/ReactionDiffusion.h` | Gray-Scott model, 9 presets, bake-to-texture/heightmap |
| Cellular Automata | `Enjin/Effects/CellularAutomataGeometry.h` | 7 CA rules, 3 mesh modes (Voxels/MarchingCubes/PointCloud) |
| Physarum Sim | `Enjin/Effects/PhysarumSimulation.h` | Agent-based slime mold, 5 presets, trail diffusion/decay |
| Timeline Editor | `Enjin/Animation/TimelineEditor.h` | Flash-style keyframe editor, layers, Bezier/CatmullRom curves, auto-key |
| Thumbnails | `Enjin/Assets/ThumbnailGenerator.h` | Asset preview generation with CPU rasterizer and caching |
| Texture Compression | `Enjin/Assets/TextureCompressor.h` | BCn/ASTC compression with mipmap generation |
| SWF Converter | `Enjin/Assets/SWFConverter.h` | SWF→ECS entity conversion (shapes, MovieClips, timelines) |
| AS3 Transpiler | `Enjin/Scripting/AS3Transpiler.h` | Pattern-based ActionScript→AngelScript source transpilation |
| Flash API Shim | `Enjin/Scripting/FlashAPIShim.h` | ~40 AngelScript bindings emulating Flash APIs |
| Fourier Mesh | `Enjin/Effects/FourierMesh.h` | DFT contour decomposition, progressive reconstruction, 3D extrusion |
| 4D Projection | `Enjin/Effects/Projection4D.h` | 5 polytopes, 6 rotation planes, stereographic 4D→3D projection |
| Inverse Rendering | `Enjin/Renderer/InverseRendering.h` | CPU gradient descent scene parameter optimization |
| Adaptive Quality | `Enjin/Renderer/AdaptiveQuality.h` | FPS-based auto-adjustment (5 quality levels, render scale) |
| Linux Platform | `Enjin/Platform/LinuxPlatform.h` | XDG paths, zenity/kdialog dialogs, fork/exec |
| Steam Deck | `Enjin/Platform/SteamDeck.h` | Deck detection, adaptive quality, gyro, suspend/resume |
| Steam Input | `Enjin/Platform/SteamInput.h` | Steam Input API stubs (ENJIN_STEAM guarded) |
| NVN Backend | `Enjin/Renderer/NVN/NVNBackend.h` | Nintendo Switch 1 render backend stub |
| Colorblind Palette | `Enjin/Accessibility/ColorblindPalette.h` | 9 palettes with pattern+icon alongside color |
| Font Library (Accessibility) | `Enjin/Accessibility/FontLibrary.h` | FontFamily enum (Default/Mono/OpenDyslexic), spacing config |
| AppImage Builder | `Enjin/Build/AppImageBuilder.h` | Linux AppImage packaging with .desktop generation |
| Non-Euclidean | `Enjin/Effects/NonEuclidean.h` | Portal rendering (stencil recursion), hyperbolic/spherical/toroidal space warping |
| Metaballs | `Enjin/Effects/Metaballs.h` | Implicit surface field, marching cubes mesh extraction, per-group color blending |
| Voxel Cone Tracing | `Enjin/Renderer/VoxelConeTracing.h` | VXGI: voxel grid + mip chain, cone-traced diffuse/specular GI, AO, god rays |
| SDF Renderer | `Enjin/Renderer/SDFRenderer.h` | Mesh-to-SDF, sphere tracing, isosurface extraction, 8SSEDT text rendering |
| Framebuffer Feedback | `Enjin/Effects/FramebufferFeedback.h` | Ping-pong compositing, 8 presets (Echo/Melt/VHS/Kaleidoscope/etc.), 5 blend modes |
| Screen Distortion | `Enjin/Effects/ScreenDistortion.h` | 7 distortion types (HeatHaze/Shockwave/Underwater/etc.), composited UV offset field |
| Spline IK Deformer | `Enjin/Effects/SplineIKDeformer.h` | FABRIK solver, Verlet physics, tube/ribbon mesh generation for tentacles/ropes |
| Interactive Water | `Enjin/Effects/InteractiveWater.h` | Spring-damper wave propagation, splashes/wakes, buoyancy, boundary modes |
| Audio Reactive | `Enjin/Effects/AudioReactive.h` | Cooley-Tukey FFT, bass/mid/treble bands, per-vertex mesh displacement |
| Collaborative Editing UI | `Enjin/Editor/CollaborativeEditingUI.h` | OT protocol UI, peer cursors, conflict resolution, session management |
| Symbol Library | `Enjin/Editor/SymbolLibrary.h` | Reusable graphic symbols as prefabs, nested editing, category browser, update propagation |
| Input Action Map | `Enjin/Input/InputAction.h` | Remappable input bindings, sensitivity, deadzone, presets, 18 game actions |
| Screen-Space Effects | `Enjin/Renderer/PostProcessing.h` | SSAO, god rays, contact shadows, caustics, fog shafts (all in postprocess.frag) |

---

**Start Here**: [README.md](../README.md) > [BUILD.md](BUILD.md) > [USER_MANUAL.md](USER_MANUAL.md)
