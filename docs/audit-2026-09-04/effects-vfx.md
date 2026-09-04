I'll parallelize this audit across five forks, each covering a coherent subset of the Effects layer, then synthesize the findings myself.

---

All five audit forks are running in the background (weather/water/time, vegetation renderers, particles/sprites, fluid/grid simulations, and destruction/screen-fx/misc). I'll wait for their completion notifications and then synthesize a single ranked report. I'll let you know when the full findings are ready.

---

First fork (weather/water/time) is done. Waiting for the remaining four before I synthesize.

---

Second fork (vegetation renderers) is done. Waiting for particles/sprites, fluid/grid simulations, and destruction/screen-fx forks.

---

Third fork (particles/sprites) is done. Waiting for fluid/grid simulations and destruction/screen-fx forks.

---

Fourth fork (fluid/grid simulations) is done — this one found the biggest issue so far (dead systems). Waiting on the last fork (destruction/screen-fx/misc).

---

All five forks are done. Let me spot-check the two biggest claims (10 allegedly-dead effect systems) myself before presenting them with confidence.

---

Confirmed — all `src/**/*.cpp` are globbed in, so all ten files compile into the engine on every build. Findings verified. Here is the synthesized, ranked report.

## VFX/Effects Layer Audit — Ranked Findings

Scope: `Engine/src/Effects/*.cpp` + headers. Verified by direct reading and repo-wide grep (including RenderSystem.cpp, Editor/, Player/, Scripting/, VisualScript/, Tests/). Ranked by (cost saved) / (risk of change).

### 1. Ten entire effect systems are compiled every build and never instantiated anywhere — ~6,600 dead lines (highest value, near-zero risk)
Zero references outside their own file/header — no ECS registration, no editor UI, no script binding, no `RenderSystem.cpp` hit, no test (except one). Confirmed by grep across the whole repo, and confirmed these all pass through the `file(GLOB_RECURSE ENGINE_SOURCES ...)` in `Engine/CMakeLists.txt:57`, so they compile into every build forever:

- `Engine/src/Effects/Metaballs.cpp` (668 lines) — `MetaballSystem`/`MetaballComponent`, no ECS/serializer/inspector entry.
- `Engine/src/Effects/PhysarumSimulation.cpp` (571 lines) — fully dead, not even a test.
- `Engine/src/Effects/CellularAutomataGeometry.cpp` (1073 lines) — unrelated to (and easily confused with) the live `Procedural::CellularAutomata` dungeon generator; this Effects-namespace class is separate and unreachable.
- `Engine/src/Effects/ReactionDiffusion.cpp` (361 lines) — only referenced by `Tests/Unit/Effects/TestReactionDiffusion.cpp`, no ECS/component/system wiring.
- `Engine/src/Effects/NonEuclidean.cpp` (599 lines) — `PortalComponent`/`PortalRenderer`/`NonEuclideanSystem` (name-collides with the unrelated, live `Scene::StreamingPortalComponent`).
- `Engine/src/Effects/Projection4D.cpp` (726 lines) — 4D-polytope wireframe generation, unreferenced.
- `Engine/src/Effects/FourierMesh.cpp` (552 lines) — DFT contour reconstruction + mesh extrusion, unreferenced.
- `Engine/src/Effects/SplineIKDeformer.cpp` (762 lines) — spline-driven IK chain deformer, unreferenced.
- `Engine/src/Effects/ScreenDistortion.cpp` (611 lines) — full CPU per-pixel distortion field + bilinear resample, never wired to any render pass.
- `Engine/src/Effects/FramebufferFeedback.cpp` (651 lines) — Echo/Melt/VHS/Kaleidoscope/etc. presets, unreferenced.

**Fix:** either wire each into the render/editor path it was clearly built for, or delete. Nothing in the tree depends on them, so deletion is safe; each is a self-contained decision (do file-by-file, not as one giant PR).

### 2. The same xorshift32/LCG PRNG core is hand-copied independently in at least 10 Effects files, with no shared `Math::Random` utility
Confirmed identical bit-twiddle core in: `Weather.cpp:8-16`, `SeasonalWeather.cpp:54-58`, `ParticleSystem.cpp:9-26` (this one also *bypasses* the engine's actual seeded RNG — see #3), `ReactionDiffusion.cpp:10-15`, `PhysarumSimulation.cpp:20-25`, `CellularAutomataGeometry.cpp:327-332`, `FramebufferFeedback.cpp:136-138`, plus an LCG variant duplicated between `VoronoiMeshFracture.h:38-47` and `Destructible.cpp:716-727` (same constants, different bit-mask precision — 16-bit vs 24-bit — and NOT sharing a seed even though `DestructibleSystem::CreatePersistentFragments` calls into `VoronoiMeshFracture::Fracture`, so one destruction event runs on two disconnected RNG streams). No `Enjin/Math/Random.h`/`Xorshift.h` exists to consolidate into.
**Fix:** one small `Math::Xorshift32`/`Math::LCGRandom` header; each site keeps its own instance/seed, just shares the algorithm.

### 3. `ParticleSystem.cpp:9-26` — CPU particle emitters use an unseeded local RNG instead of the engine's replay-seeded one
This duplicates `Enjin::Math::Random01()`/`Random()` (`Core/include/Enjin/Math/Math.h:106-133`) byte-for-byte, but `Math::Random` is the one `PlayMode.cpp:341,344` seeds via `Math::SetRandomSeed()` for ADR-0005 replay determinism. Because `ParticleSystem` keeps its own independent stream, CPU particle emitters won't reproduce identically across a replay even though the seeding plumbing exists specifically for that.
**Fix:** delete the local RNG, call `Math::Random01()`/`Math::Random(min,max)` — direct drop-in.

### 4. Unit-quad vertex/index buffer construction duplicated verbatim across four renderers
`WeatherRenderer.cpp:56-83`, `ParticleRenderer.cpp:57-81`, `SpriteBatchRenderer.cpp:67-91`, `FluidRenderer.cpp:56-81` — identical 4-vertex/6-index quad + identical `VulkanBuffer` create/upload sequence, ~25 lines × 4. (`SplatRenderer`/`GPUParticleSystem` already avoid this by generating the quad in the vertex shader, proving one source of truth is viable.)
**Fix:** one shared `Renderer::CreateUnitQuadBuffers(context)` helper.

### 5. Vegetation pipeline creation copy-pasted 6 times, and `ReloadShaders` rebuilds against the wrong render pass after an offscreen re-target — a real, reachable Vulkan compatibility bug
`GrassRenderer.cpp:93-153/155-216`, `TreeRenderer.cpp:103-160/162-222`, `ShrubRenderer.cpp:94-151/153-213` each carry two ~60-line near-identical `CreatePipeline`/`CreatePipelineWithPass` methods. More importantly: `RenderSystem::RecreateEffectPipelinesForRenderPass` (`RenderSystem.cpp:15134-15159`) re-targets these three renderers at the offscreen game-view pass (1 color attachment, per CLAUDE.md's documented offscreen format), but none of the three track which pass/attachment-count they were last built with — so the shader hot-reload watcher (`RenderSystem.cpp:11519-11550`, wired to `grass/shrub/tree.vert/frag`) calls the parameterless `ReloadShaders`, which hardcodes `CreatePipeline()` → swapchain pass + `colorAttachmentCount=2`. Editing a vegetation shader while the editor's offscreen game-view is active silently rebuilds against the wrong render pass (same VUID class as the MRT bug CLAUDE.md already documents).
**Fix (bug):** add `m_LastRenderPass`/`m_LastColorAttachmentCount` members set by both `CreatePipeline*` variants; have `ReloadShaders` call `CreatePipelineWithPass` with the remembered values.
**Fix (duplication):** extract the shared ~60-line pipeline-config builder into one helper taking shader spans + render pass + attachment count.

### 6. `TreeColliders.cpp:29-33` hand-copies the vegetation placement hash a third time
A local unnamed lambda `cpuHash` reimplements, bit-for-bit, `RenderSystem::VegPlacementHash` (`RenderSystem.cpp:16663-16667`), which is itself already a documented intentional replication of the GLSL hash in `grass.vert`/`shrub.vert`/`tree.vert` for RT instancing. Nothing ties `TreeColliders.cpp`'s copy to the others — if the placement hash ever changes for a scatter/visual tweak, trunk colliders can silently drift off their visual trunks.
**Fix:** move `VegPlacementHash` into `VegetationTemplates.h` (already the documented single source of truth for placement/shape) and have both `RenderSystem.cpp` and `TreeColliders.cpp` call it.

### 7. Per-frame waste, smaller scale
- `Water.cpp:103-108` (`Water3D::GenerateMesh`) — three vectors grown via `push_back` with no `reserve()`, called every frame per animated water surface (`PlayMode.cpp:1157-1166`) even though final vertex/index counts are known up front. Trivial 3-line fix.
- `InteractiveWater.cpp:322-472` (`Update`) + `PlayMode.cpp:1147-1156` — full CPU wave propagation and full mesh rebuild (up to 65k vertices, triggering GPU re-upload) every frame forever, even when a pond is at rest and `damping` has decayed to near-zero. Fix: track max-disturbance and skip propagation/rebuild once it's below an epsilon for a few frames; wake on splash/wake events.
- `GPUParticleSystem.cpp:229` (`SpawnWithParams`) — heap-allocates a fresh `std::vector<GPUParticle>` every call, including every frame for any continuous emitter (`RenderSystem.cpp` `TickGPUEmitters`, ~line 6802). Fix: reuse a member scratch vector, following the pattern the class already uses for `m_ImpactEvents`.

### 8. Misleading no-op UI / stale comment (low cost, but actively misleading)
- `Water.h:64-66` (`enableFoam`/`foamThreshold`/`foamScale`) — fully exposed in the editor inspector and round-tripped through scene JSON, but never read by `RenderSystem.cpp` (a same-named-but-different field, `WaterVolumeComponent::foamScale`, is what's actually live). A user can toggle/tune foam settings that do nothing.
- `GPUParticleSystem.cpp:84-97` — comment/log claims "no spawn source wired (Spawn has no callers)"; this is false, `RenderSystem.cpp` calls `SpawnWithParams`/`SpawnStain` from `TickGPUEmitters` and the impact-event handler. Risks someone "cleaning up" a live feature. Doc-only fix.

### Minor duplication, not separately ranked
- Marching-cubes lookup tables (`s_EdgeTable`/`s_TriTable`, ~330 lines) copy-pasted identically between `Metaballs.cpp:15-308` and `CellularAutomataGeometry.cpp:15-308` — moot if both are deleted per finding #1, but a plain single-source-of-truth violation regardless.
- `GPUParticleTypes.cpp:11-15` (`ShapeHash`) and `GPUParticleSystem.cpp:198-202` (`HashUnit`) are the same hash function under two names, one file apart.

### Checked and ruled out (not flagged)
Wind-sway formulas are shader-side only, not duplicated in `.cpp` files. `WindSystem`, `VegetationTemplates.cpp`, `CurlNoiseSystem.cpp` (correctly delegates to `Math::CurlNoise3D`), `FluidSimulation.cpp` (proper hysteresis-gated ping-pong buffers), `ElementalSystem`, `RetroEffects`, `AudioReactive`, `Destructible`/`VoronoiMeshFracture` fracture logic itself, and `SpriteTextureAtlas` are all genuinely wired with no waste. `Water3D::WaterInteraction` class (`Water.cpp:348-392`) is dead (~45 lines, superseded by `InteractiveWater`) but small enough it's folded in rather than ranked separately.

One item outside scope worth flagging to whoever owns `Engine/src/Time/`: `Enjin::TimeOfDay` (`Engine/src/Time/TimeOfDay.cpp`) appears to duplicate `Effects::WorldTimeSystem`'s sun/ambient/sky computation with zero callers — not verified in depth since it's outside `Effects/`.