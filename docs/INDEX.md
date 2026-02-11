# Documentation Index

## Getting Started

1. **[README.md](../README.md)** - Project overview and feature list
2. **[BUILD.md](BUILD.md)** - Consolidated build guide (all platforms, dependencies, troubleshooting)
3. **[USER_MANUAL.md](USER_MANUAL.md)** - Editor usage, scripting guide, and workflow reference

## Architecture & Design

- **[ARCHITECTURE.md](ARCHITECTURE.md)** - System architecture overview (rendering, ECS, physics, scripting, plugins, streaming)
- **[CODING_STANDARDS.md](CODING_STANDARDS.md)** - Coding style and documentation standards
- **[ROADMAP.md](ROADMAP.md)** - Technical roadmap, performance findings, node graph expansion, GUI modernization plans
- **[SECURITY_AUDIT.md](SECURITY_AUDIT.md)** - Security & robustness audit report (35 findings across serialization, networking, renderer, future-proofing)

## API Reference

- **[API_REFERENCE.md](API_REFERENCE.md)** - C++ API documentation
- **[SCRIPTING_API.md](SCRIPTING_API.md)** - AngelScript scripting API reference (~170 functions)

## Key Systems

| System | Header | Description |
|--------|--------|-------------|
| Scripting | `Enjin/Scripting/` | AngelScript VM, TegeBehavior, bindings, coroutines, events |
| Physics | `Enjin/Physics/` | SimplePhysics (queries), PhysicsWorld (dynamics), ConstraintSolver (joints) |
| Profiler | `Enjin/Debug/Profiler.h` | Frame profiling, scope timers, ImGui overlay |
| Plugins | `Enjin/Plugin/PluginSystem.h` | Dynamic library loading, manifest system |
| Timeline | `Enjin/Animation/Timeline.h` | Property/event/animation tracks, keyframe sequencing |
| Hot-Reload | `Enjin/Plugin/HotReload.h` | C++ gameplay DLL hot-reload with state save/restore |
| Streaming | `Enjin/Scene/LevelStreaming.h` | Chunk-based level streaming with priority queue |
| Terrain | `Enjin/ECS/Components/Terrain.h` | 3D heightmap sculpting (5 brush modes) and 2D polyline terrain |
| AI | `Enjin/AI/AIBehaviors.h` | State-based AI, navmesh generation, A* pathfinding |
| Render Backend | `Enjin/Renderer/RenderBackend.h` | Platform abstraction interface |
| Ray Tracing | `Enjin/Renderer/RayTracing/` | RT pipeline, acceleration structures, SVGF denoiser, path tracer |
| Feedback | `Enjin/Editor/FeedbackSystem.h` | Bug reporting, feedback collection, diagnostics capture, JSON persistence |
| Vector Drawing | `Enjin/Editor/VectorDrawingEditor.h` | 7 shape types, layers, SVG export, Flash symbol library |
| HTML5 Export | `Enjin/Build/HTML5Exporter.h` | Canvas export, preloader, responsive scaling, Newgrounds embed |
| Newgrounds | `Enjin/Networking/NewgroundsAPI.h` | NG.io REST gateway: medals, scoreboards, cloud saves |
| 2D Physics | `Enjin/Physics/Physics2D.h` | Circle/Box/Polygon shapes, 5 joint types, CCD, SAT collision |
| Localization | `Enjin/GUI/Localization.h` | String tables, CSV/JSON I/O, LOC() macro, parameterized strings |
| Behavior Trees | `Enjin/AI/BehaviorTree.h` | 20 node types, visual editor, blackboard, play-mode debugging |
| Procedural Gen | `Enjin/Procedural/ProceduralAlgorithms.h` | 9 algorithms, visual node graph editor, preview canvas |
| Destructible | `Enjin/Effects/Destructible.h` | 4 fracture patterns, debris physics, chain destruction |
| Networking | `Enjin/Networking/LANMultiplayer.h` | Host-authoritative UDP, prediction, interpolation, RPC, lobby |
| Save System | `Enjin/Gameplay/TieredSaveSystem.h` | 20-slot tiered saves (SceneState/RunState/MetaProgression), auto-save, checkpoints, cloud backends |
| Save Backends | `Enjin/Gameplay/SaveBackend.h` | ISaveBackend interface + Local/Newgrounds/Steam implementations |
| Play Mode Diff | `Enjin/Editor/PlayModeDiff.h` | JSON diff of pre/post play scene states, cherry-pick apply dialog |

---

**Start Here**: [README.md](../README.md) > [BUILD.md](BUILD.md) > [USER_MANUAL.md](USER_MANUAL.md)
