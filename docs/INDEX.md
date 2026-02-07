# Documentation Index

## Getting Started

1. **[README.md](../README.md)** - Project overview and feature list
2. **[BUILD.md](BUILD.md)** - Consolidated build guide (all platforms, dependencies, troubleshooting)
3. **[USER_MANUAL.md](USER_MANUAL.md)** - Editor usage, scripting guide, and workflow reference

## Architecture & Design

- **[ARCHITECTURE.md](ARCHITECTURE.md)** - System architecture overview (rendering, ECS, physics, scripting, plugins, streaming)
- **[CODING_STANDARDS.md](CODING_STANDARDS.md)** - Coding style and documentation standards
- **[ROADMAP.md](ROADMAP.md)** - Technical roadmap, performance findings, node graph expansion, GUI modernization plans

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

---

**Start Here**: [README.md](../README.md) > [BUILD.md](BUILD.md) > [USER_MANUAL.md](USER_MANUAL.md)
