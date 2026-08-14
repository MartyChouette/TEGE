# TEGE Master Plan

Living document. Master list of all known unfinished work, ordered to carry 0.9.7 to release and then to 1.0.
Compiled 2026-08-06 from the full audit backlog (2026-03-14), the whole-engine review (2026-07-17), all session earmarks, and the RT/settings campaigns of 2026-08-03 through 08-06.
Last updated 2026-08-06 (evening): RT batch committed and pushed through `d53f464`; the mixed-RT set is functionally complete.

## Timeline

| Date | Milestone |
|---|---|
| Wed Aug 6 (done) | RT batch committed + pushed (`a52fb5e9..d53f464`). Mixed RT complete: all four hybrid effects display in editor + player, PT displays end-to-end, veg/water/sky in the TLAS, per-frame descriptor rewrites killed (player probe 0 validation errors). |
| Wed Aug 6 (remaining) | Regenerate 0.9.7 artifacts from current build. Marty eyeball pass. Installer double-click test. |
| Thu Aug 7, before 5PM ET | Upload zip to site, publish GitHub release v0.9.7, post publicly. Submit GDFL application. |
| Aug 8 - Sep 2 | 0.9.8 sprint: template QA (all 48), settings verification matrix + honest-feedback UI, editor redesign wave 4, hybrid rchit real shading, web runtime smoke. |
| Wed Sep 3 | GDFL Pitch Day (in person, 20-minute slot, live demo). |
| September | 0.9.9 stabilization: test backlog, VWS runtime verify, UI unification leftovers, FBX polish, doc refresh. |
| October 2026 | 1.0 commercial launch. $20 one-time, paid official binaries, source stays BSL 1.1. Scope = QA + packaging + pricing page, not new features. |
| Nov 2, 2026 | GDFL program start (if accepted): present real launch numbers. |
| Feb / May / Oct 2027 | GDFL milestones M1-M3 (see application answers). |

## Recently landed (pushed, part of 0.9.7)

The whole ray-tracing campaign plus the run-up to it, in order:
- Ray tracing made real: it runs, shows, and shades right (`d68c7d2`). NES palette rebuilt from real 2C02 values (`0979389`).
- The whole world into ray tracing, then art-directed: vegetation / water / sky in the TLAS, procedural placeholder effects given a real look (`dc35a0f`).
- Mixed RT built end to end: RT-native G-buffer (`de3230c`), shadows on the raster scene (`66955f0`), reflections + GI (`57c17e5`), live strength sliders (`8e5a37e`), and hybrid in exported games via the player overlay (`4d5389c`).
- Per-frame RT descriptor rewrites killed, player probe 0 validation errors (`d53f464`).

Before the RT work, also shipped and pushed:
- Editor redesign waves 1-3: fonts, themes, transport bar, the 185-header type sweep, viewport toolbar, hub cards, console (`950864c` and the wave commits).
- Test audit: suite to 101/101, five zero-coverage criticals closed (undo, anchor API, HUD migration, Rigidbody, script round-trips) (`a52fb5e9`).
- Fly-cam whip fixed at the input layer. The February scene-save crash fixed. HUDSystem retired, UICanvas is the only UI.
- Compute skinning activated (ADR-0002 Phase 1), Vulkan default on.

## RT status (where the ray-tracing campaign actually stands)

Done and on screen now (committed + pushed through `d53f464`):
- Path tracing, full: raw, SVGF-denoised, OIDN. Displays end to end at swapchain res, converges ~300spp.
- Mixed (hybrid) RT: shadows, AO, reflections, GI all DISPLAY. Editor uses an RT-native G-buffer + a post-process overlay; player uses fixed-function blend passes. All four differ from the raster baseline.
- Vegetation (grass/shrub/tree), water, and sky are in the TLAS, so PT metal reflects the greenery and the sky matches raster.
- Stable and clean: RT matrix 14/14 validation-clean, player probe 0 validation errors (was 8), suite 101/101, no crash. TLAS in-place refit, per-frame descriptor rewrites killed.

Unfinished, and what it means when you look:
- Hybrid rchit real shading: reflections / GI / caustics / translucency still use fake normals and fake NdotL. Only the path tracer reads true geometry normals. So in the hybrid variants the reflection and GI color is approximate, not physically correct. That is expected, not a bug.
- Goldens not blessed yet: the captures in `goldens/rt/_candidates` are candidates, not references. Nothing regression-guards RT until you `-Record` after the eyeball.
- Async RT is forced off (the compute path raced the G-buffer + overlay). Single-queue only for now.
- Skinned meshes ray-trace in bind pose (the TLAS uses unskinned vertex buffers).

How to check it: open the per-variant PNGs in `goldens/rt/_candidates` (one per variant, fastest), or open the RT probe project (`D:\TEGE_Projects\_RTProbe`) in the editor and toggle modes in Scene Settings. Compare each `rt_*` against `raster_base`: shadows/AO should darken contact areas, reflections/GI should add bounce, path tracer should look fully lit and converged. Judge whether the approximate hybrid color is good enough to ship; the rest is correctness polish scheduled for 0.9.8.

## A. Release gate: 0.9.7 (this week)

1. ~~Commit the RT batch.~~ DONE + pushed (`d53f464`): suite 101/101, RT matrix 14/14 validation-clean, player probe 0 errors, mixed RT displaying in editor + player.
2. Marty eyeball pass, one editor session: Shells PT floor, NES preset, redesign waves 1-3, shadow rotation-flicker fix, HUD scaling, undo History panel, compute-skinning look, the now-visible hybrid RT effects.
3. Regenerate artifacts: current TEGESetup-0.9.7.exe and TEGE-0.9.7.zip predate the RT work, redesign, and this week's fixes. Rebuild Release, ISCC, re-zip.
4. Installer test: double-click install, .enjinproject and .enjin associations.
5. Ship: upload zip (replace the stale one on the site), publish the GitHub release with the drafted notes, post publicly.
6. Submit GDFL before Thu 5PM ET.
7. Push and deploy marty64-net (3 commits ahead plus uncommitted).

Not release-blocking: goldens blessing, settings A/B matrix.

## B. 0.9.8 - before Pitch Day (Sep 3)

### Render options truth
- Finish the settings A/B pixel matrix (loader keys extracted, harness design settled): stamp each setting, capture, diff vs baseline. Proves every option changes pixels or flags it.
- Editor feedback UI: disable-with-reason for anything inert, incompatible, or content-dependent (the rtSimplifiedMaterials "(planned)" pattern, applied everywhere).
- Known gaps to encode: TAA uses garbage velocity in the editor game view; MSAA modes dead in editor (offscreen targets 1-sample); clustered lighting never dispatches in editor; HDR only visible on the real window surface; LUT needs an asset; shading-model toggle invisible on matte materials.

### RT track
- ~~Hybrid effects consumption: real G-buffer, apply shadows/AO/GI/reflections to the lit image.~~ DONE (RT-native G-buffer + PP overlay in editor, fixed-function blend in player; all four effects display).
- ~~Per-frame RT descriptor rewrites (VUID-03047).~~ DONE (`d53f464`): handle-gated rewrites + vkCmdUpdateBuffer for the light/NEE buffers; player probe 0 errors.
- Hybrid rchit real shading (reflect/gi/caustics/translucency still use fake normals and fake NdotL; the rchit-side geometry infra exists, only rt_pathtrace uses it so far).
- Bless RT goldens (-Record after eyeball; probe ambient lowered so effects read).
- Re-enable async RT (forced off; the rework must account for the G-buffer + overlay ordering).
- Exercise player pak-script loading (loose files still take precedence).
- LightBVH build wiring (class, shader traversal, and UI exist; never constructed).
- rtEnabled-under-useProjectDefaults UX warning.
- RT sees bind pose on skinned meshes (TLAS uses unskinned vertex buffers).
- Custom-asset FBX inheritance for vegetation (drop-in tree/shrub meshes that inherit wind + seasonal systems; currently stubbed).

### Editor
- Redesign wave 4: default dockspace declutter, remaining toolbar and panel polish, tooltip sweep on icon-only buttons (draw-list vector icons only; the font atlas has no emoji).
- Cross-project scene open keeps the wrong script root silently (warn or switch context).

### Template QA
- Roster cut to 16 templates on 08-07 (the old 48-entry catalog is gone for good). Most of the 16 still untested — biggest user-facing risk for anyone downloading 0.9.7. The play-probe and golden harness can automate the boots-and-plays tier.

### Earmarked latent bugs (diagnosed, unfixed)
- Progressive cascade update: matrices update every frame while far-cascade textures lag; forceFullUpdate checks position only, never rotation.
- God rays: hardcoded 0.5 luminance threshold.
- Cel outline: distance-dependent Sobel threshold on raw depth.

### Tests
- Component round-trip batch: Animator, ParticleEmitter, AudioSource, Skeleton.
- Negative-path serializer suite (corrupt JSON, invalid enums, missing refs).
- ShadowMap::UpdateCascades unit test (rotation invariance).
- Input mock seam, then CameraController behavioral tests.

### VWS layers
- Runtime-verify the whole flow: capture, toggle, save, reopen-resume, merge-down.
- Marty decisions: base-vs-resolved save semantics; PlayModeDiff stableId migration.

### UI unification phase 2 leftovers
- Options screen as UICanvas (GameMenus options still bespoke).
- Wire-or-cut dead utilities: InventoryUI, MinimapRenderer, ScreenTransition, SaveLoadMenu.
- WYSIWYG anchor drag handles; canvas scaler match-width/height modes.
- Web runtime smoke of the rebuilt WASM.

### FBX import polish
- Material auto-apply on Mixamo models, scale calibration, auto mesh collider generation.

## C. To 1.0 (October 2026)

### Rendering
- Hybrid RT + DDGI + surfel GI visually complete and demo-able.
- Editor/player render parity: one code path. Every latent bug this week (shadow binds, TLAS lifetime, clustered, fog usage flags) hid in whichever loop does not run. This is the structural fix.
- SMAA shaders; VRS content-adaptive and motion modes; player post-process chain parity.

### Platforms
- Web parity completion: textured sprites, UI/text render path, accessibility overlays.
- Then Android (Vulkan), then iOS/Metal (backend currently empty).

### Architecture consolidation
- Retire AudioManager (SimpleAudio is the system). Retire legacy AIAgent. ConstraintSolver decision.
- Visual-script globals into ExecutionContext.
- World.h per-GetComponent recursive_mutex: Stage A DONE (2026-08-13, adr-0004) — GetComponent/HasComponent are lock-free reads under the single-writer / fork-join model, writes keep the recursive_mutex + debug owner-thread assert, fork-join concurrency test added. Stage B (hoist GetComponentStorage into the hottest per-entity loops = remove calls not just locks) still open.
- Per-frame DrawCmd alloc and sort; ECS archetype storage evaluation.

### Pipeline honesty
- Real .enjpak compression (currently passthrough). Pak-only exports (loose files still ship).
- Quest custom conditions (always return true). Navmesh agentRadius (documented, ignored). Click-to-move.
- Server authority (currently allow-all). Collab component-removal sync (3 of 140 types).

### Editor authoring tools
- Navmesh generation + visualization UI. Terrain sculpting. Tilemap painter. Visual UI layout editor. Template preview images.

### Scripting and serialization surface
- Remaining unbound/partially-bound components (joints, colliders, tilemap, and hierarchy were bound in the May cleanup; ~35 remain).
- Component serializer coverage from ~7% round-trip-tested toward full; mid-play save state (AI, audio, tweens).
- SCRIPTING_API doc gaps (HasComponent coverage, AI bindings, Camera_ApplyPreset).

### Docs
- ARCHITECTURE.md drift (MaterialGPU 80 to 112, backend phases). Dual-system explanations. RT shader workflow. ROADMAP refresh against this document.

### Simulation and materials (Marty backlog, added 2026-08-13)
- Buoyancy volumes: water zones that float/sink bodies via depth-based buoyancy + drag, with a 2D-heightfield wave sim. **Full spec (Gobliny-driven, canonical): `D:\TEGE_Projects\goblin\design\tech\tege-buoyancy-spec.md`.**
  - **~70% ALREADY BUILT as `InteractiveWaterSystem` (Engine/*/Effects/InteractiveWater.{h,cpp}), wired into PlayMode:743 + Player:976 + tested (TestInteractiveWater).** This answers the spec's open Q1 (reuse this heightfield, don't build a new one). Done: grid heightfield + spring-damper `PropagateWaves`, `GetWaterHeight` bilinear sample (= spec `get_surface_height`), `CreateSplash` (= `apply_displacement`), `CreateWake`, depth-based upward buoyancy + water drag on `RigidbodyComponent.velocity` (InteractiveWater.cpp:343-362), `WaterInteractorComponent` marker (= `BuoyancyBody`), absorbing/reflecting boundaries, mesh gen. NOTE a separate simpler `WaterVolumeComponent` (Components/WaterVolume.h) also exists (visual/freeze water, ContainsPoint) — the heightfield sim is the InteractiveWater one.
  - **GAPS vs spec (the real remaining work):** (1) ~~per-object DENSITY~~ **DONE 2026-08-13**: `WaterInteractorComponent` now has density (<1 floats, >1 sinks), volume, waterlogRate/waterlogMaxDensity; buoyancy force = `buoyancyForce * submergedDepth * volume * (1 - effectiveDensity)`; serialized + inspector controls + tests (light rises / dense sinks / waterlog accumulates). (2) ~~game-subscribable `WaterEnterEvent`~~ **DONE 2026-08-13**: an interactor crossing the surface downward faster than the water's `entryVelocityThreshold` fires a deferred `water_enter` event on the `EntityEventBus` (target=interactor, sender=water, floats impactForce/velocityY/x/y/z), edge-detected so it fires once on entry; wired PlayMode+Player+web; serialized threshold; tests. (3) ~~VERIFY the buoyancy `rb->velocity` writes reach Jolt~~ **VERIFIED 2026-08-13**: they reach Jolt via `SyncECSToJolt`'s external-velocity override (JoltBackend.cpp:413-422), one-frame latency. So the MVP (heightfield + density buoyancy + WaterEnterEvent + player drag) is COMPLETE. REMAINING: (4) ~~`WaterCurrent` (lazy-river flow)~~ **DONE 2026-08-13**: `InteractiveWaterComponent` carries `currentDirection`/`currentSpeed`/`currentPull`; floating interactors have their horizontal velocity lerped toward the flow (carried at flow speed, a swimmer can fight it); serialized + inspector + tests (carried up to flow speed / no-current-no-drift). Spline-path current still Alpha-tier. (5) `apply_sustained_pressure` (Surfs Up leafblower). Four components: `WaterVolume` (bounds/rest_height/heightfield resolution/damping/wave_speed + `get_surface_height`/`apply_displacement`/`apply_sustained_pressure`), `BuoyancyBody` (density, waterlog_rate, submerged/surface drag, displaced volume; polls the volume per physics tick), `WaterEnterEvent` (velocity-threshold splash → auto-displacement + game-subscribable), `WaterCurrent` (directional/spline flow for lazy river). MVP = WaterVolume heightfield + BuoyancyBody upward force + WaterEnterEvent + player submersion drag. Explicitly OUT: full 3D fluid, underwater refraction (separate render concern), boat pitch/roll, inter-object wave interaction. Physics side is Jolt 3D / Box2D 2D per the strict separation; net-sync the displacement EVENTS not the heightfield (deterministic local sim). 5 open impl questions in the spec (reuse a heightfield struct? tick rate? non-rect bounds? rigidbody drag controls? shader sampling of the heightfield).
- Refractive indices: real refraction through transparent/glass/water materials. Material already carries IOR (extended MaterialGPU: SSS/transmission/IOR/thickness), so this is the render consumer — screen-space refraction / IOR-based bending, and the RT path can read true IOR for path-traced glass.

### Parked (1.0+ or cut, Marty's call)
- DLSS/XeSS vendor SDKs. FMOD/Wwise/NVN stubs (cut candidates). SteamAudio default-on decision.
- VR/OpenXR. Nanite/Lumen-style tech. Frame generation. Vulkan video decode.
