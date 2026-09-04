I'll start by reading CLAUDE.md and getting a lay of the land.

---

Now let me fan out on the other areas while I take RenderSystem.cpp and the shaders myself.

---

Now my own deep dive into RenderSystem.cpp and WebShaderData.h.

---

Audit complete. I ran four parallel sweeps and verified every load-bearing claim myself; I corrected two subagent false positives along the way (noted at the end).

## Tier A — broken paths and stubs that lie (highest value / lowest risk)

**A1. Shader hot-reload is inert in all seven effect sub-renderers, and reports success.**
`Engine/src/Effects/GrassRenderer.cpp:314-341` — `ReloadShaders` compiles the edited GLSL, assigns `m_VertexShader`/`m_FragmentShader`, then calls `CreatePipeline(sharedLayout)` (line 93), whose first act is `m_VertexShader = make_unique<VulkanShader>(); LoadFromSPIRV(ShaderData::GrassVertexShaderData...)` — overwriting the just-compiled module with the baked SPIR-V. It returns `m_Pipeline != nullptr` (true), and `RenderSystem.cpp:11523` logs "Shader hot-reload: grass shaders reloaded". Identical in `TreeRenderer.cpp`, `ShrubRenderer.cpp`, `ParticleRenderer.cpp`, `WeatherRenderer.cpp`, `FluidRenderer.cpp`, `SpriteBatchRenderer.cpp` (the latter four route through `CreatePipelineWithPass`, same overwrite).
*Cost:* every grass/tree/shrub/particle/weather/fluid/sprite shader iteration since the feature landed has silently done nothing but report success. Syntax errors are caught (so it feels alive); semantic edits vanish.
*Fix:* split shader loading from pipeline building — `CreatePipeline*` should reuse existing `m_VertexShader`/`m_FragmentShader` when non-null, loading the baked SPIR-V only on first build. One helper, seven call sites.

**A1b (same functions).** `ReloadShaders` unconditionally calls `CreatePipeline` → swapchain render pass, `colorAttachmentCount = 2`. But `RenderSystem::RecreateEffectPipelinesForRenderPass` (`RenderSystem.cpp:15134`, called from `EditorLayer.cpp:2371,2396` and `Player/src/main.cpp:2302`) retargets these same renderers at the offscreen pass with `colorAttachmentCount = 1`. Nothing remembers which pass the pipeline was last built for, so a reload while the offscreen path is live rebuilds against an incompatible pass (the VUID-07609 class CLAUDE.md documents). *Fix:* store `m_LastRenderPass`/`m_LastColorAttachmentCount` in both `CreatePipeline*` variants; `ReloadShaders` calls `CreatePipelineWithPass` with them.

**A2. Onion-skin ghosts are computed every frame and can never be drawn.**
`Engine/src/Editor/EditorLayer.cpp:4213` builds ghosts (including per-ghost skinning-matrix copies) and pushes them via `SetOnionSkinGhosts`. The only draw call is `RenderSystem.cpp:6489`. In editor mode `EditorLayer.cpp:2993` sets `SetSkipMainPassRendering(true)` every frame, and `RenderSystem::Update` returns at `RenderSystem.cpp:5885` long before 6489. Outside editor mode, `RenderOnionSkinGhosts` (`:12357`) early-returns on `!m_IsEditorMode`. The two conditions are mutually exclusive — the pass is unreachable in every configuration.
*Fix:* call `RenderOnionSkinGhosts()` from `RenderToTarget()` (the editor's actual scene path), same as `RenderPlanarReflections` at `:7891`.

**A3. The per-entity wireframe checkbox does nothing in the editor.**
`MeshRendererComponent::wireframe` is exposed at `Engine/src/Editor/EditorLayerInspector.cpp:1727` and serialized (`SceneSerializer.cpp:3196`). Its only consumer, `RenderWireframeOverlayPass` (`RenderSystem.cpp:9211`), is called only from `:6132` and `:6486` — both on the swapchain path the editor never reaches. `m_OffscreenWireframeOverlayPipeline` (`RenderSystem.h:1475`) exists for the offscreen case, is `reset()` at `RenderSystem.cpp:11424`, and is never created or bound.
*Fix:* same as A2 — call it from `RenderToTarget`/`RenderSplitscreen` using the already-existing `(*m_ActiveDescriptorSets)[GetActiveBufferIndex(f)]` indirection.

**A4. Multi-material meshes render with the wrong materials outside the offscreen path.**
`RenderToTarget` handles sub-meshes (`RenderSystem.cpp:7653`, per-submesh draws at `:7807-7818`). `RenderEntity()` (`:11850-12245`) — the swapchain draw — has no `HasSubMeshes()` branch at all; it issues one `vkCmdDrawIndexed(indexCount, 1, 0, 0, matIdx)` at `:12241`. `RenderSplitscreen`'s inline copy (`:8304-8320`) likewise. So a `MaterialSlotsComponent` mesh looks right in the editor and in a player build with post-processing, and wrong when the active camera has `enablePostProcessing = false` (`Player/src/main.cpp:1467`) or splitscreen is active (`main.cpp:1468`).

**A5. The OIT toggle is inert and the manager allocates unconditionally.**
`RenderSystem.cpp:4272-4276` constructs and `Initialize()`s `OITManager` on every scene load (full-res accumulation + revealage images, render pass, framebuffer, pipeline — `OITManager.cpp:673-760`). `BeginTransparentPass`/`EndTransparentPass`/`CompositePass` (`OITManager.cpp:132,213,273`) have zero call sites repo-wide. `m_OITEnabled` (`RenderSystem.h:2411`) has exactly one reader — its own getter. The editor ships a live checkbox for it (`EditorLayerRendering.cpp:2654`) and a status readout (`EditorLayerPanels.cpp:7928`).
*Fix:* gate `Initialize()` on `m_OITEnabled` and wire the three passes, or remove the checkbox and the allocation.

**A6. Water foam settings are UI-only.** `Water3DSettings::enableFoam` / `foamThreshold` (`Engine/include/Enjin/Effects/Water.h:64-65`) have inspector UI (`EditorLayerComponents.cpp:2070-2072`) and round-trip through scene JSON (`SceneSerializer.cpp:897,924`), and are read by nothing. (`InteractiveWater`'s same-named `foamThreshold` is a different field and *is* live.)

## Tier B — one problem, N unenforced copies

**B1. The WGSL↔C++ layout contract (your calibration case, confirmed and larger than stated).** `ViewProjection` is hand-declared **seven** times in `WebShaderData.h` (lines 11, 598, 1108, 1162, 1239, 1376, 1442), plus an eighth aliased variant `ShadowViewProjection` at `:87` (same 144 bytes, fields renamed `viewPos`→`lightPos`), against C++ `WebViewProjectionUBO` (`RenderSystem.cpp:102`). `LightingUBO` is declared twice (`:18`, `:1448`) against `WebLightingUBO` (`RenderSystem.cpp:111`). `ObjectData` twice (`:45`, `:606`) against `WebObjectDataUBO` (`RenderSystem.cpp:137`) — whose own comment admits "keep in lockstep with BOTH WGSL ObjectData structs". Nothing enforces any of it: `tools/check_wgsl.mjs` compiles the WGSL through Dawn but has no view of the C++ side, and there is no test in `Tests/` referencing `WebShaderData`/`WebLightingUBO` (the Vulkan side has a `static_assert` in `TestMaterial`).
*Fix:* generate the shared struct prologue (`ViewProjection`/`LightingUBO`/`ObjectData` + the group-0/1 bindings) into `WebShaderData.h` from the C++ structs, the way `_gen_all.py` generates `ShaderData.h`; failing that, add a test that reflects each WGSL struct's field list and byte size and compares to `sizeof()`.

**B2. `Engine/shaders/wgsl/*.wgsl` are a second, already-drifted copy of the shipped shaders.** `WebShaderData.h:9` says "Embedded PBR shader (matches Engine/shaders/wgsl/pbr.wgsl)" and `WebGPUShaderCompiler.h:15` says "Shaders are maintained as .wgsl files in Engine/shaders/wgsl/". Both are false. Normalized diff of `Engine/shaders/wgsl/pbr.wgsl` against the embedded `PBR_WGSL` shows 326 differing lines including real layout drift: `LightingUBO` is missing `snowParams`; `ObjectData` is missing `uvScrollU/V`, `scrollReflSpeedU/V`, `scrollReflStrength`, `matcapBlend` (96 bytes short); and `@group(2) @binding(6)/(7)` mean `ddgiIrradiance`/`ddgiSampler` in the file vs `matcapTex`/`matcapSmp` in the shipped copy. `shadow.wgsl` has the same stale `ObjectData`. `Engine/shaders/web_pbr.wgsl` (93 lines) is a third, older copy. `WebGPUShaderCompiler::CompileFile` (`WebGPUShaderCompiler.cpp:39`) — the only thing that could read these — has zero callers.
*Fix:* delete `Engine/shaders/wgsl/`, `web_pbr.wgsl` and `CompileFile`, and correct the two header comments. Or make the files the source and generate the strings. Either way, not both.

**B3. Four copies of "render one view of the scene", and they have already diverged.**
`Update()` splitscreen branch (`RenderSystem.cpp:5987-6155`), `Update()` single-camera path (`:6160-6528`), `RenderToTarget()` (`:6948-7918`), `RenderSplitscreen()` (`:7919-8345`). Which passes each runs:

| pass | Update main | Update split | RenderToTarget | RenderSplitscreen |
|---|---|---|---|---|
| wireframe overlay | yes | yes | **no** | **no** |
| onion-skin ghosts | yes | **no** | **no** | **no** |
| planar reflections | yes | **no** | yes | **no** |
| selection highlight | yes | **no** | yes | **no** |
| sub-mesh materials | **no** | **no** | yes | **no** |
| elemental/weather particles | yes | **no** | (via caller) | **no** |

The code documents its own cost — `RenderSystem.cpp:7186`: *"This loop is the editor game view - it never went through BindGeometryPipelineForMaterial, so applied graph shaders were invisible here."* That was one round of this bug being paid for.
*Fix:* one `RenderSceneView(target, camera, viewportIndex, flags)`. The unifying indirection already exists and is used in 20+ places (`m_ActiveDescriptorSets` / `GetActiveBufferIndex`, `RenderSystem.h:2033,2055`); the three inline draw loops should call `RenderEntity()`. This is the largest single win and also the highest-risk change — do it incrementally, one pass at a time, golden-capture between.

**B4. Two pairs of copy-paste twins inside that duplication, cheap to fix now.** `RenderOutlinePass` (`:12476-12577`) vs `RenderOutlinePassForTarget` (`:12657-12756`) — ~100 lines each, differing only in pipeline selection, descriptor-set source, and a render-list fallback. `RenderGridLines` 5-arg (`:9292-9343`) vs 7-arg (`:9345-9399`) — ~50 lines each, differing only in pipeline, descriptor set, and viewport extent. In both cases the offscreen variant already uses `(*m_ActiveDescriptorSets)[GetActiveBufferIndex(f)]` and the main one hardcodes `m_DescriptorSets[currentFrame]`. *Fix:* delete the main-path variant, make the offscreen one the only one, add a pipeline selector. Low risk, independently verifiable.

**B5. One push-constant slot, three names.** `Engine/include/Enjin/Renderer/RenderStructs.h:33` calls it `surfaceParam1`; `Engine/src/Effects/GrassRenderer.cpp:304` writes the bindless texture index into it every draw; `Engine/shaders/grass.frag:107` reads it as `texIndex`; `Engine/shaders/grass.vert:23` declares it `float _pad0`. Same in `shrub.vert:20` vs `shrub.frag:105`. (`tree.vert`/`tree.frag` are already consistent.) *Cost:* the next person needing a per-vertex grass parameter sees free padding and silently clobbers live data. *Fix:* rename `_pad0`→`texIndex` in the two `.vert` files. Two lines.

**B6. The same xorshift32 copied ~10 times in `Engine/src/Effects/`, and one copy breaks replay determinism.** `ParticleSystem.cpp:9-16` is byte-identical to `Math::Random01` (`Core/include/Enjin/Math/Math.h:106-125`) — same algorithm, same seed constant `2463534242u` — but on its own `s_RandState`, so `Math::SetRandomSeed()` (called by `PlayMode.cpp:341` specifically for ADR-0005 replay determinism) never reaches CPU particle emitters. Same copy also in `Weather.cpp:8`, `SeasonalWeather.cpp:54`, `ReactionDiffusion.cpp:10`, `PhysarumSimulation.cpp:20`, `CellularAutomataGeometry.cpp:327`, `FramebufferFeedback.cpp:136`, plus an LCG variant duplicated between `VoronoiMeshFracture.h:38` and `Destructible.cpp:716` at *different bit precision* (16 vs 24) despite one calling into the other. *Fix:* `ParticleSystem` calls `Math::Random01()`; the rest share one `Math::Xorshift32` type with per-instance state.

**B7–B10 (smaller, same shape).** `VkImageMemoryBarrier` boilerplate hand-rolled 13 times across 7 files with no `VulkanImage::TransitionLayout` helper (`VulkanImage.cpp:421` *and* `:528` in the same file; `GPUDriven/HiZPyramid.cpp:304` is a live per-frame path). Unit-quad VB/IB construction verbatim in `WeatherRenderer.cpp:56`, `ParticleRenderer.cpp:57`, `SpriteBatchRenderer.cpp:67`, `FluidRenderer.cpp:56`. The height/distance fog block copied into `grass.frag:167`, `shrub.frag:167`, `tree.frag:178`, `sprite_lit.frag:276` (scalar UBO fields only — the unsized-array exemption in CLAUDE.md does not apply). `TreeColliders.cpp:29` hand-copies `RenderSystem::VegPlacementHash` (`:16663`) a third time, so a placement tweak can drift trunk colliders off their visual trunks.

## Tier C — wasted per-frame work

**C1. WebGPU main pass creates and destroys two GPU objects per draw command per frame.** `RenderSystem.cpp:2969-2993` (batch buffer + bind group) and `:2997-3009` (per-entity buffer + bind group), both destroyed at `:2991/3055` immediately after recording. Plus `:3025-3049` creates a fresh texture bind group *and* re-runs `WebGetOrLoadTexture` for every sub-mesh of every multi-material mesh, every frame. Root cause is one line: `OBJ_ALIGN = 256` at `:2702`, commented "WebGPU minUniformBufferOffsetAlignment" — but group 1 binding 0 is `var<storage, read>` (`WebShaderData.h:66`), the buffers are created with `GPUBufferUsage::Storage`, and every bind uses offset 0. The 256 stride is a fossil from a dynamic-offset uniform design that no longer exists, and it is the sole reason `:2964` heap-allocates a `std::vector<u8> batchData` and memcpys each instance into it (the shader's `array<ObjectData>` stride is 144).
*Fix:* stride `objDataBuf` by `sizeof(WebObjectDataUBO)`, upload it once per frame into one persistent growable storage buffer with one bind group, and index draws by `firstInstance` — exactly the scheme the Vulkan half already uses for the material SSBO (adr-0003). Removes all per-draw allocation and the repack.

**C2.** `GPUParticleSystem.cpp:229` heap-allocates a fresh `std::vector<GPUParticle>` on every `SpawnWithParams` call, including every frame for continuous emitters driven from `RenderSystem::TickGPUEmitters`. The class already uses a member scratch pattern for `m_ImpactEvents`.

**C3.** `InteractiveWater::Update` (`InteractiveWater.cpp:322-472`, driven from `PlayMode.cpp:1147`) runs full CPU wave propagation and a full mesh rebuild (up to 65k verts + GPU re-upload) every frame forever, including when the surface is at rest. Gate on a max-disturbance epsilon, wake on splash.

**C4.** `Water3D::GenerateMesh` (`Water.cpp:103-108`) grows three vectors by `push_back` with no `reserve()`, every frame per animated surface, with the final counts known up front.

**C5.** `UpdateFrameUniforms` (`RenderSystem.cpp:10495`) runs 2–3× per frame in the editor (viewport `RenderToTarget`, game-view `RenderToTarget`/`RenderSplitscreen`). Each call does `m_TAAFrameCounter++` (`:10537`) and overwrites `m_PrevViewProj` (`:10545`). So the Halton jitter sequence advances once per *view* rather than per frame, and each view's previous-frame matrix is whichever view rendered last. Bounded impact today (offscreen targets have no velocity attachment), but it is per-frame state written more than once per frame and will bite when TAA reaches the offscreen path.

## Tier D — dead code and dead members

Roughly **14,000 lines** compile into every build and are unreachable. Verified by whole-repo grep for the type name outside its own file:

- Top-level `Engine/src/Renderer/`: `InverseRendering.cpp` (`InverseRenderer`), `VoxelConeTracing.cpp` (`VoxelGrid`/`ConeTracer` — superseded by the wired `DDGIProbeSystem`), `SDFRenderer.cpp` (`MeshToSDF`/`SDFTextRenderer`/`SDFMeshRenderer` — distinct from the live `SDFScene`/`SDFGenerator`), `ShaderManager.cpp` (superseded by `VulkanShaderManager`/`WebGPUShaderManager`), `PipelineVariantCache.cpp` (`GetOrCreate` never called; only `Destroy` on an always-empty cache at `VulkanPipeline.cpp:68`). ~2,900 lines.
- Subdirectories: `FrameGraph/` and `RenderGraph/` — two independent, complete, mutually unaware implementations of the same pass/barrier graph, neither ever constructed. `Techniques/` + `RenderPipeline/` + `Scripting/RenderScript.cpp` — an entire alternate rendering-technique stack with a hand-rolled DSL, unrelated to the real AngelScript runtime. `MeshShader/Meshlet.cpp` (`GenerateMeshlets` zero callers), `VirtualTexture/`, `Materials/MaterialSystem.cpp` (its only consumer `GUI::ShaderGUI` is itself never instantiated). ~4,150 lines.
- `Engine/src/Effects/`: ten systems never instantiated — `Metaballs.cpp`, `PhysarumSimulation.cpp`, `CellularAutomataGeometry.cpp`, `ReactionDiffusion.cpp` (test-only), `NonEuclidean.cpp`, `Projection4D.cpp`, `FourierMesh.cpp`, `SplineIKDeformer.cpp`, `ScreenDistortion.cpp`, `FramebufferFeedback.cpp`. ~6,600 lines. (`Metaballs.cpp:15-308` and `CellularAutomataGeometry.cpp:15-308` also carry byte-identical marching-cubes tables.)
- `PostProcessing.cpp:1329-1900`: `ApplyDepthOfField`, `ApplyTiltShift`, `SeparableWeightedBlur`, `CreateDofStagingBuffers` — a CPU readback+blur fallback with zero callers, superseded by the live GPU `applyDoF`/`applyTiltShift` in `postprocess.frag:1780,1837`. ~650 lines.
- Shaders: `Engine/shaders/evaluate_lighting.glsl` — zero references anywhere, under a header that emphatically declares itself "ONE LIGHTING PATH FOR EVERYTHING ... called by ALL fragment shaders" while five shaders each hand-roll their own. `vt_feedback.frag` / `vt_resolve.comp` — well-formed, no C++ or build reference.
- 1.4 MB of orphan tracked files: five `*ShaderData.hex`, `postprocess_spirv_embed.txt`, `Engine/shaders/wgsl/`, `web_pbr.wgsl`.
- Dead `RenderSystem` members: `m_ClusterLightsCache`, `m_PrevCameraPos`, `m_PrevEntityCount` (the last two superseded by the working `m_LastSortCamPos`/`m_RenderListStaticHash` gate at `:6237`), `m_CascadeUpdateCooldown`, `m_MainBindGroupLayout`, `m_OffscreenWireframeOverlayPipeline`, and `m_EditorWireframe` (saved and restored by `EditorLayer.cpp:2894/2986`, read by no render path — redundant with `m_WireframeMode`).
- `ENJIN_VISIBILITY_BUFFER` is `OFF` by default (`Engine/CMakeLists.txt:328`), so `RenderSystem.cpp:4375`'s `Initialize()` costs nothing today — but turning the option on allocates two images, a render pass, a framebuffer and two pipelines, and `BeginVisibilityPass`/`ResolvePass` are never called, so it renders nothing.
- Stale size comments on the `MaterialGPU` triple: `Engine/shaders/triangle.frag:127` says 128 bytes, `Engine/include/Enjin/ECS/Components/Material.h:274` says 112; the actual, correct, `static_assert`-guarded size is 144. The struct bodies themselves are genuinely in lockstep — only the comments lag, which is precisely the copy-paste vector CLAUDE.md's own note describes.
- `VulkanContext::GetGPUMemoryUsed()` (`VulkanContext.cpp:829`) queries memory properties and unconditionally returns 0. Currently harmless (no callers), but it will lie to the first debug HUD that adopts it.

## Two subagent claims I rejected

- "VisibilityBuffer allocates GPU resources on every scene load" — false in a default build; the call is inside `#ifdef ENJIN_VISIBILITY_BUFFER`, which is `OFF`. Reframed above.
- "`m_UpscalerSharpness` / `m_WebSharpness` are dead" — false; both are read externally (`EditorLayer.cpp:3402`, `RenderSystem.h:820`).

## Suggested order

1. B5 (2 lines), D's comment/file deletions, A6 — trivial, no runtime risk.
2. A1 + A1b — the single highest cost-per-line-changed item in the report.
3. A2, A3, A4, A5 — four broken user-facing features, each a handful of lines once you decide where the call belongs.
4. C1 — the only sizeable per-frame win, and self-contained to the web main pass.
5. B4, then B3 — B4 is a safe rehearsal for B3, which is the real structural fix and wants golden captures at every step.