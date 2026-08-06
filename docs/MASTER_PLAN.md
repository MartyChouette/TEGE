# TEGE Master Plan

Living document. Master list of all known unfinished work, ordered to carry 0.9.7 to release and then to 1.0.
Compiled 2026-08-06 from the full audit backlog (2026-03-14), the whole-engine review (2026-07-17), all session earmarks, and the RT/settings campaigns of 2026-08-03 through 08-06.

## Timeline

| Date | Milestone |
|---|---|
| Wed Aug 6 | Commit the RT batch. Regenerate 0.9.7 artifacts from current build. Marty eyeball pass. Installer double-click test. |
| Thu Aug 7, before 5PM ET | Upload zip to site, publish GitHub release v0.9.7, post publicly. Submit GDFL application. |
| Aug 8 - Sep 2 | 0.9.8 sprint: template QA (all 48), settings verification matrix + honest-feedback UI, editor redesign wave 4, hybrid RT visible, web runtime smoke. |
| Wed Sep 3 | GDFL Pitch Day (in person, 20-minute slot, live demo). |
| September | 0.9.9 stabilization: test backlog, VWS runtime verify, UI unification leftovers, FBX polish, doc refresh. |
| October 2026 | 1.0 commercial launch. $20 one-time, paid official binaries, source stays BSL 1.1. Scope = QA + packaging + pricing page, not new features. |
| Nov 2, 2026 | GDFL program start (if accepted): present real launch numbers. |
| Feb / May / Oct 2027 | GDFL milestones M1-M3 (see application answers). |

## A. Release gate: 0.9.7 (this week)

1. Commit the uncommitted RT batch (verified state: suite 101/101, RT matrix 14/14 validation-clean, player probe 0 errors).
2. Marty eyeball pass, one editor session: Shells PT floor, NES preset, redesign waves 1-3, shadow rotation-flicker fix, HUD scaling, undo History panel, compute-skinning look.
3. Regenerate artifacts: current TEGESetup-0.9.7.exe and TEGE-0.9.7.zip predate the RT work, redesign, and this week's fixes. Rebuild Release, ISCC, re-zip.
4. Installer test: double-click install, .enjinproject and .enjin associations.
5. Ship: upload zip (replace the stale one on the site), publish the GitHub release with the drafted notes, post publicly.
6. Submit GDFL before Thu 5PM ET.
7. Push and deploy marty64-net (3 commits ahead plus uncommitted).

Not release-blocking: goldens blessing, hybrid RT visibility, settings A/B matrix.

## B. 0.9.8 - before Pitch Day (Sep 3)

### Render options truth
- Finish the settings A/B pixel matrix (loader keys extracted, harness design settled): stamp each setting, capture, diff vs baseline. Proves every option changes pixels or flags it.
- Editor feedback UI: disable-with-reason for anything inert, incompatible, or content-dependent (the rtSimplifiedMaterials "(planned)" pattern, applied everywhere).
- Known gaps to encode: TAA uses garbage velocity in the editor game view; MSAA modes dead in editor (offscreen targets 1-sample); clustered lighting never dispatches in editor; HDR only visible on the real window surface; LUT needs an asset; shading-model toggle invisible on matte materials.

### RT track
- Hybrid effects consumption: real G-buffer (depth + normals at game-view resolution), apply shadows/AO/GI/reflections to the lit image. The rchit-side geometry infra exists now.
- Hybrid rchit real shading (reflect/gi/caustics/translucency still use fake normals and fake NdotL).
- Bless RT goldens (-Record after eyeball).
- Exercise player pak-script loading (loose files still take precedence).
- LightBVH build wiring (class, shader traversal, and UI exist; never constructed).
- rtEnabled-under-useProjectDefaults UX warning.
- RT sees bind pose on skinned meshes (TLAS uses unskinned vertex buffers).

### Editor
- Redesign wave 4: default dockspace declutter, remaining toolbar and panel polish, tooltip sweep on icon-only buttons (draw-list vector icons only; the font atlas has no emoji).
- Cross-project scene open keeps the wrong script root silently (warn or switch context).

### Template QA
- 47 of 48 templates untested. Biggest user-facing risk for anyone downloading 0.9.7. The play-probe and golden harness can automate the boots-and-plays tier.

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
- World.h per-GetComponent recursive_mutex; per-frame DrawCmd alloc and sort; ECS archetype storage evaluation.

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

### Parked (1.0+ or cut, Marty's call)
- DLSS/XeSS vendor SDKs. FMOD/Wwise/NVN stubs (cut candidates). SteamAudio default-on decision.
- VR/OpenXR. Nanite/Lumen-style tech. Frame generation. Vulkan video decode.
