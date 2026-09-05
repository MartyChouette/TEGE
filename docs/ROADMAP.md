# Enjin Engine Technical Roadmap

This document captures detailed technical plans, performance findings, and strategic initiatives identified through codebase audits. It complements CLAUDE.md's feature roadmap with implementation-specific details.

## Status

Windows and Linux both build and ship. The Linux CI job builds the engine, runs
the full ctest suite, runs a lavapipe render smoke under xvfb, and packs and
boots an exported Linux game on every push. Test suite: 149/149.

Web is a real target. PBR, shadows, particles, vegetation, weather, inverted-hull
outlines, weighted-blended order-independent transparency and a depth-driven
post-process pass all run in the browser. Ray tracing displays end to end on
desktop: shadows, AO, reflections, GI and path tracing.

Shipped flagship initiatives: reverse-step debugging and shareable replays, the
editor MCP server, per-style splash screens, Linux as a first-class build.
Held by choice: one-click web share (no commercial-platform branding).
Not started: Gaussian splat import, the tutorial.

Steam, Steam Deck, VR/XR/AR and mobile all require the renderer to stop assuming
a single view and projection. One multiview refactor serves all four.

1.0 ships October 2026 at $30 one-time. Paid binaries, source stays BSL 1.1.

## The list

Everything before 1.0, everything known broken, everything after. One place.

### Ship gate

- [ ] 1. Cut the template roster below 16
- [ ] 2. Every surviving template boots and plays (play-probe + golden harness)
- [x] 3. Test suite green — 149/149
- [ ] 4. Installer: ISCC compile + double-click test of both file associations
- [ ] 5. Linux tarball or AppImage attached to the release
- [ ] 6. Disable-with-reason across the settings surface
- [ ] 7. Fix or disable: TAA on garbage velocity in the editor game view; MSAA modes against 1-sample offscreen targets; clustered lighting never dispatching in the editor; HDR only on the real surface; LUT needs an asset
- [ ] 8. Web demos verified in a browser (OIT, depth SSAO, cel outline all unwatched)
- [ ] 9. Pricing page and licence wording — $30 one-time, BSL 1.1 source
- [ ] 10. Version bump, changelog, release

### Tutorial

- [ ] 11. Guided first game — in-editor overlay, highlights the next control

### Particle system

- [ ] 12. Audit the CPU path for correctness and perf; good defaults on a fresh emitter
- [ ] 13. Inspector with live preview
- [ ] 14. Fire/Start/Stop/Loop/Burst from nodes and script, 2D and 3D
- [ ] 15. Primitives: mesh particles, ribbons/trails, point sprites, soft particles
- [ ] 16. Emitter shapes: sphere, box, circle, edge, mesh-surface (cone exists)
- [ ] 17. Preset library: mist, spray, blood, sparks, smoke, fire, dust, rain-splash, magic
- [ ] 18. GPU-compute scale path for huge counts

### Traversal

- [x] 19. Ropes and swinging — `Rope.h`, verlet ropes and chains
- [x] 20. Ladders — `Ladder.h`
- [x] 21. Doors — `Door.h`
- [ ] 22. Cover — volume, controller state, snap to edge, peek, corner detection
- [ ] 23. Ledge climb / mantle — sweep detect, translate up and over
- [ ] 24. Ledge hang and shimmy — shares ledge detection with mantle
- [ ] 25. `Interactable` + interaction prompt — the substrate the others register with
- [ ] 26. Click-to-move — `ControllerSystem.cpp:979` is empty; needs raycast-to-ground + FindPath

### Node coverage

`NodeRegistry.cpp` carries ~705 node definitions and `ScriptApiNodes` reflects the
AngelScript registry (1,268 registered functions) into nodes at boot. The gap is
what the generator skips.

- [ ] 27. Count the generator's `skippedTypes` and `skippedDup` lists
- [ ] 28. Generic accessors for the bound component types
- [ ] 29. AI, behaviour-tree and navmesh nodes
- [ ] 30. Input and UI nodes
- [ ] 31. Rewind, MIDI, Accessibility, AudioReactive
- [ ] 32. Graph readability at scale — functions, subgraphs, groups

### Single-instance vegetation

A volume scatters plants procedurally. Placing ONE tree means importing a mesh
and adding a VegetationComponent, which is a different code path with different
art, so a lone tree does not match the grove behind it.

- [x] 82. Single tree placement that matches the tree volume - same template
      mesh, same wind, same seasonal response, one instance
- [x] 83. Single shrub placement, same rule

### Model and texture authoring

- [ ] 84. Direct UV texture painting - paint onto a model's texture in the
      viewport, through the existing PixelEditor canvas
- [ ] 85. In-editor vertex weight painting. This is not only a rigging tool:
      wind sway reads its amplitude from vertex colour red (trunk 0, leaves 1),
      and an imported mesh carries no vertex colours, so there is currently no
      way to author that weight without going back to Blender
- [ ] 86. MeshComponent should carry its own height, so sway can normalise
      against the mesh instead of the fixed 2-unit ramp it uses now

### Controls hint

- [x] 87. The on-screen controls hint should list only what the scene can
      actually do. FoliageDemo has no character controller and still advertises
      sprint and move; the touch preset drives the hint and defaults to a
      character set regardless of what the scene contains

### Editor authoring tools

- [ ] 33. Terrain sculpting
- [ ] 34. Tilemap painter
- [ ] 35. Visual UI layout editor
- [ ] 36. Navmesh generation and visualisation UI
- [ ] 37. Template preview images

### Pipeline honesty

- [ ] 38. Real `.enjpak` compression — currently passthrough
- [ ] 39. Pak-only exports — loose files ship and take precedence
- [ ] 40. Quest custom conditions — always return true
- [ ] 41. Navmesh `agentRadius` — documented, ignored
- [ ] 42. Server authority — allow-all
- [ ] 43. Collab component-removal sync — 3 of 140 types

### Navmesh

- [ ] 44. Off-mesh links (jumps, portals)
- [ ] 45. Spline-path nav — `Math/Spline.h` unwired to nav
- [ ] 46. Painted-region authoring UI

### Web

- [ ] 47. Texture volume vegetation — `TREE_WGSL` and the shrub shader have no texture bindings
- [ ] 48. Cross-backend vegetation texture paths — `ResolveBindlessTextureIndex` is Vulkan-only
- [ ] 49. Scatter a custom mesh across a volume — one custom tree works, a field does not
- [ ] 50. Textured sprites, UI/text render path, accessibility overlays
- [ ] 51. Depth of field
- [ ] 52. God rays — also drop the hardcoded 0.5 luminance threshold
- [ ] 53. Tilt-shift

### Simulation and scale

- [ ] 54. Spline-path water current (lazy river) — last open item in the buoyancy spec
- [ ] 55. Swarm fork-join across cores
- [ ] 56. GPU-instanced Swarm draw — position buffer to one draw
- [ ] 57. Tier-2 GPU-compute agents

### Known broken

- [ ] 58. Hybrid rchit uses fake normals and fake N·L
- [ ] 59. RT goldens never blessed — nothing regression-guards RT
- [ ] 60. Async RT forced off — compute raced the G-buffer and overlay
- [ ] 61. Skinned meshes ray-trace in bind pose
- [ ] 62. LightBVH never constructed
- [ ] 63. `rtEnabled` under `useProjectDefaults` has no warning
- [ ] 64. Editor and player render through two code paths
- [ ] 65. Physics has no automated coverage
- [ ] 66. Component serializers ~7% round-trip tested
- [ ] 67. World.h Stage B — hoist `GetComponentStorage` out of hot loops
- [ ] 68. ~35 components unbound to script
- [ ] 69. ARCHITECTURE.md drift — MaterialGPU is 144 bytes
- [ ] 70. `_pgweb` held open by a process

### After 1.0

- [ ] 71. Multiview / stereo refactor — unblocks Steam Deck, VR/XR/AR, mobile split-screen
- [ ] 72. Steam + Steamworks behind an `ISteam` service
- [ ] 73. Steam Deck Verified
- [ ] 74. VR/XR via OpenXR, then AR
- [ ] 75. Android (Vulkan), iOS (Metal — backend empty)
- [ ] 76. Gaussian splat import
- [ ] 77. One-click web share — held, no commercial-platform branding
- [ ] 78. Sprite editor through UICanvas + imported-art round-trip
- [ ] 79. Refractive indices — IOR is in MaterialGPU, no render consumer
- [ ] 80. Glacier phases 1 and 3
- [ ] 81. Nanite-style cluster streaming, virtual texturing revival


## Big Platform Initiatives (added 2026-08-21, P1)

Two large multi-part initiatives promoted to the top of the platform work. Both are
greenfield relative to the current Windows-first shipping story, but the Vulkan
foundation, the multi-backend `IRenderBackend` seam, and the already-abstracted
input layer make them tractable.

### 1. Linux + Steam + Steam Deck

The goal: TEGE builds, ships, and *feels native* on Linux and is Steam Deck Verified.

- **Linux desktop as a first-class build.** DONE (2026-08-25): builds under GCC 13 on
  Ubuntu 24.04, and the `linux-desktop` CI job builds the engine, runs the full `ctest`
  suite (105/105), runs the lavapipe render smoke under xvfb, AND packs + boots an
  exported Linux game (build pipeline -> `Game` binary + enjpak, then `--frames 120`
  headless) on every push. The exported-game player target works: `CopyPlayer` already
  finds the Linux `EnjinPlayer` and copies it executable. CI uploads a
  `tege-linux-x86_64` tarball (stripped editor + player + runtime README) as a
  downloadable build. The port surfaced and fixed several MSVC-only-passing bugs
  (namespaced includes, block-scope externs, AngelScript SysV ABI class flags,
  cross-platform path validation, and a shutdown dangling-renderer segfault in the
  exported player). STILL TO DO: attach the Linux tarball to formal GitHub releases (not
  just per-run artifacts); a nicer package (AppImage) if desired.
- **Steamworks integration.** Wrap the Steamworks SDK behind an optional `ISteam`
  service: app-id init, achievements/stats, Steam Cloud saves (route the tiered save
  system through it), rich presence, the overlay, and Input API for controller glyphs.
  Keep it OFF by default / stubbed so non-Steam builds don't require the SDK.
- **Steam Deck Verified checklist.** Runs under Proton first (validate the Windows
  build on Deck), then a native Linux build. Gamepad-first: every menu fully navigable
  with a controller (the accessibility motor layer already helps), default resolution
  1280x800, readable text at that size (ties into the a11y text-scaling), suspend/resume
  survival, and a controller glyph set. The exported game — not just the editor — is the
  target here.
- **Risk / unknowns:** audio backend parity on Linux (miniaudio is portable), file-path
  case sensitivity (the `.enjin`/pak loaders must not assume Windows casing), and Proton
  quirks for the Vulkan swapchain.

### 2. AR / XR / VR + Mobile

The goal: TEGE can output to a headset and to a phone, from the same project.

- **VR/XR via OpenXR** (see the VR scoping note in session memory: ~2-3mo MVP, greenfield,
  riskiest piece = swapchain-ownership flip). Add an `IXRBackend`: OpenXR session +
  reference spaces, per-eye stereo/**multiview** rendering (the renderer currently assumes
  a single view/proj — this is the core change), motion-controller input mapped through
  the existing input abstraction, and a comfort/vignette layer (the a11y reduced-motion
  toggle is a natural fit). WebXR as the browser counterpart once desktop OpenXR works.
- **AR** rides on the same XR session model (passthrough + anchors) once the stereo/pose
  plumbing exists; scope it as a follow-on to VR rather than a parallel track.
- **Mobile (Android/iOS).** Vulkan on Android, MoltenVK/Metal on iOS. Needs: render tiers
  + TBDR/tiled-friendly passes (avoid full-screen resolves the desktop path leans on),
  touch input + on-screen virtual controls in the UI system, aggressive asset/memory
  budgets, and a build/packaging path (Gradle/APK, Xcode). Touch is the input-layer piece;
  the render-tier work overlaps with the Steam Deck "small GPU" tuning.
- **Shared prerequisite:** both tracks need the renderer to stop assuming one camera/view.
  Doing the multiview/stereo refactor once unblocks VR, AR, and cleaner mobile split-screen.

## Flagship Feature Initiatives (added 2026-08-23, P1)

Continuing the initiative list (1–4 are the platform tracks above). These are the
identity features: what makes TEGE's pitch bleeding-edge rather than catch-up.

### 5. Reverse-step debugging + shareable replays — the flagship bleeding-edge claim

Step BACKWARD through a running game frame by frame, and share a replay file that
reproduces a session anywhere.
- **Already in place:** `RecordRewindSystem` + `RecordRewindComponent` (per-entity
  record/rewind), `PlayModeDiff` (what-changed tracking), and the procgen suite is
  seeded-deterministic end to end (every generator reproduces from a u32 seed).
- **To build:** an input-stream recorder (timestamped device events → the replay is
  initial state + seed + inputs, tiny to share); a deterministic fixed-step play mode
  (replays diverge without it — float math is fine same-machine, document cross-machine
  caveats); a global snapshot ring buffer (every N frames) so reverse-step = restore
  nearest snapshot + re-simulate forward; editor UI: a timeline scrubber in play mode
  with step-back/step-forward buttons next to Play/Pause.
- **Risk:** determinism discipline — any system reading wall-clock or unseeded RNG
  breaks replays (the player's dt fallback, particle random seeds). Needs an audit
  pass + a replay-divergence test harness. This is the hard 20%.
- **Why flagship:** reverse-step + shareable repro turns every playtester into a bug
  reporter with a perfect repro attached. No indie engine ships this.

### 6. One-click shareable web link — the growth loop

From editor to a URL someone can click, in one action.
- **Already in place:** headless `--build-web` (project → game.enjpak), the web player
  (WASM/WebGPU), `HTML5Exporter` packages a zip for web hosting, and the export dialog
  serves the build on localhost ("Run in Browser") so a maker can play it without a
  terminal.
- **To build:** a hosted quick-share option (upload the web build to a TEGE share
  endpoint → short link, the way the existing web demo is hosted). Keep publishing
  storefront-neutral: no built-in branded upload buttons — a generic export the maker
  can host or upload anywhere, plus a configurable publish-command hook if wanted.
- **Risk:** low technical, medium operational (hosting costs/abuse for quick-share).

### 7. MCP server for the editor

Expose the editor over the Model Context Protocol so AI assistants can drive it:
open/create projects, entity + component CRUD, scene queries, play control, golden
screenshots, build/export.
- **Already in place:** the string-keyed component serdes registry (SerializeOne/
  DeserializeOne/RemoveOneComponent — a ready-made generic CRUD surface for every one
  of ~160 components), the golden capture path (screenshots on demand), headless CLI
  modes, and the HTTPClient. The engine is itself AI-assisted — this closes the loop.
- **To build:** an MCP transport (stdio subprocess or local HTTP/SSE) hosting tools
  like `list_entities`, `get_component`, `set_component`, `add_component`,
  `spawn_prefab`, `enter_play`, `capture_view`, `build_web`; a main-thread command
  queue so MCP calls marshal onto the editor loop (adr-0004: no off-thread mutation).
- **Risk:** small. The registry did the hard part already. Security: bind localhost
  only, opt-in setting.

### 8. Per-style splash screens carrying the accessibility statement

The "made with TEGE" card becomes a beautiful, art-style-matched moment that also
states the accessibility commitment ("this game ships with colorblind modes, screen
reader, remappable input...").
- **Already in place:** `EngineSplash.cpp` (`DrawEngineSplash`, editor + player, font
  injection), 7 art-style presets to key the visuals from, the a11y feature set to
  describe, reduced-motion setting to respect.
- **To build:** per-style splash variants (palette + typography + motion per preset:
  PBR = cinematic fade, Pixel Art = dithered reveal, Toon = ink wipe...); the
  accessibility statement line with an optional "press A for accessibility options"
  hook straight into the GameMenus a11y tab; a Build setting to pick style/opt out.
- **Risk:** none technical — design time. High identity value per hour spent.

### 9. Gaussian splat import (.ply/.spz) with art styles applied

Import 3D-scanned Gaussian splats and put them through the TEGE art-style grading —
scanned reality rendered as toon, pixel art, or PS1.
- **Already in place:** `.ply` parses today (as triangle meshes via Assimp — splat
  .ply is a different schema: per-point position/scale/rotation/SH-color/opacity);
  bindless textures, compute infrastructure (GPU particles prove the sim+draw path),
  and the art-style/post pipeline to feed the result through.
- **To build:** a splat loader (.ply splat schema + .spz compressed); a splat renderer
  (GPU radix/bitonic depth sort per frame + instanced quad rasterization with
  anisotropic 2D Gaussian evaluation — the established real-time approach; the GPU
  particle system is the closest in-engine cousin); LOD/culling for multi-million-splat
  scenes; then route the composite through the art-style + post stack (that last part
  is nearly free — splats land in the same HDR buffer everything else grades from).
- **Risk:** renderer-heavy (the sort at scale is the perf cliff), and splats bypass
  the lighting model (they are baked radiance) — document that they take post-process
  styling, not per-light styling, at first.
- **Why flagship:** "scan your room, make it a PS1 level" is a one-sentence pitch
  nobody else has.

### 10. A tutorial — the guided first game (added 2026-08-25)

A first-run tutorial that walks a new user from empty editor to a playable game,
inside the editor itself. The three-clicks pitch needs a path that proves it.
- **Already in place:** ComponentHelp registry (every panel already explains itself),
  project templates in the hub, `docs/BEGINNERS_GUIDE.md` as the written version,
  the wiring-board flip view for showing how components connect.
- **To build:** a step-by-step in-editor tutorial overlay (highlight the next control,
  advance on the user's action); a small authored tutorial project it builds toward
  (move a character, add a light, drop a script, press Play, export); a "Start the
  tutorial" entry on the hub for first launches.
- **Risk:** low technical, high authoring care — the tone has to teach without
  patronizing, and every step must survive UI changes (drive it from stable command
  IDs, not pixel positions).
- **Why flagship:** accessibility from the ground up is the identity; a guided first
  game is that identity made concrete for someone who has never touched an engine.

---

## Art Style Rendering System (P0 — COMPLETE 2026-03-22)

**Goal:** Support 9 distinct visual aesthetics beyond the default PBR pipeline, enabling developers to create games with any art direction without fighting the renderer. Now complete with per-entity `ArtStyleComponent` overrides.

### Current Coverage

| Style | Status | Key Features |
|-------|--------|--------------|
| Low-poly retro 3D | 100% | Vertex snap, affine textures, flat/Gouraud, UV quantize, stipple, resolution downscale, texture page warping, polygon sort jitter |
| Analog/degraded | 100% | VHS (tracking, wobble, bleed, noise, tape dropout), CRT (10 real presets, phosphor subpixels), film grain, chromatic aberration, film gate weave, light leaks |
| Pixel/3D hybrid | 100% | Resolution downscale, color quantization, Bayer dither, palette lock (PICO-8, Game Boy, NES, CGA, C64 presets), normal quantization |
| Cel/toon | 100% | Diffuse bands (2-8), specular cutoff, Sobel depth+normal outlines, colored shadows, rim lighting, per-material ramp override, inverted-hull geometry outlines (per-material width/color) |
| NPR illustrative | 100% | 8 stipple patterns, configurable density/color modes, curvature-driven outline thickness variation (pen/ink) |
| Pre-PBR realism | 100% | Blinn-Phong shading, specular map slot, half-Lambert wrap, scene specular strength |
| Hand-painted | 100% | Light ramp textures (4 modes), wrap lighting, per-material ramp override, saturation boost |
| Material-expression | 100% | SSS wired in forward shader (wrap lighting + offset samples), matcap texture slot (binding 18, per-material), procedural surface noise (3-octave world-space hash) |
| Inherit (scene default) | 100% | Per-entity override via ArtStyleComponent, propagation to children |

### Phase 0: Shared Infrastructure

Reusable building blocks that unlock multiple styles:

- **Light ramp texture** (1D gradient LUT) — new descriptor binding, replace `NdotL` with artist-authored gradient sample in `calcBlinnPhong`/`calcBlinnPhongCel`. Fallback to current behavior when no ramp bound. Enables hand-painted + enhanced cel.
- **Normal-based edge detection** — extend `applyCelOutline()` in `postprocess.frag` with Sobel on reconstructed normals (using existing `reconstructNormal()`). Add `celOutlineNormalWeight`/`celOutlineDepthWeight`. Improves outlines for cel, NPR, material worlds.
- **Specular map texture slot** — new `specularTexturePath` on `MaterialComponent`. When `shadingModel == Blinn-Phong`, interpret as direct specular intensity instead of deriving from metallic. Enables pre-PBR realism.
- **Paper/overlay texture** — bindable overlay in `PostProcessSettings`, applied as multiply/overlay blend after stipple. Enables watercolor and material-expression looks.

### Phase 1: Pre-PBR Realism + Hand-Painted (Styles 1 & 2)

- Classic Blinn-Phong with specular maps (not derived from metallic)
- `halfLambert` toggle (`NdotL * 0.5 + 0.5`), scene-level `specularStrength` multiplier
- Artist light ramp textures with `lightRampTexturePath` file picker
- `lightWrapAmount` (0.0-1.0), per-material `lightRampOverride`
- Files: `triangle.frag`, `SceneRenderSettings.h`, `EditorLayerRendering.cpp`

### Phase 2: Enhanced Cel/Toon (Style 3) ✅ COMPLETE

- ~~`celShadowColor` (Vector3) in LightingUBO — tinted shadows instead of just dark~~
- ~~Rim lighting: wire existing `rimLightStrength` from MaterialComponent into shader~~
- ~~Per-material `celBandsOverride` (u8, 0 = use global) through push constants~~
- ~~Inverted-hull geometry outlines: backface extrusion pass, per-material `outlineWidth`/`outlineColor`~~
- Files: `outline.vert`, `outline.frag`, `Material.h`, `RenderSystem.cpp` (outline pipeline + pass)

### Phase 3: NPR Illustrative (Style 4) — Partially Complete

- Tonal Art Map (TAM) hatching: 6-tone luminance-layered atlas, interpolate layers by fragment brightness
- Watercolor post-process: edge darkening, wet-edge diffusion (edge-aware Gaussian), paper texture overlay, color granulation (noise-modulated saturation)
- ~~Pen/ink line variation: curvature-driven outline thickness~~ ✅ — `celOutlineCurvatureWeight` in PostProcessSettings, Laplacian of normals modulates Sobel outline thickness
- Files: `postprocess.frag`, `PostProcessSettings`, `SceneRenderSettings.h`

### Phase 4: Pixel Art Polish (Style 6)

- Custom palette texture binding (1D Nx1 pixels), nearest-color matching in RGB/LAB space
- Built-in palettes: PICO-8, CGA, EGA, Game Boy, NES, C64
- Resolution-coherent outlines: render outlines at internal resolution before upscale
- Normal quantization mode: snap normals to N cardinal directions for 2D-in-3D look

### Phase 5: Material-Expression Worlds (Style 7) DONE

- ~~Wire existing `sssIntensity`/`sssRadius`/`sssColor` into forward shader (wrap-lighting + offset samples)~~ DONE
- ~~Matcap texture slot replacing procedural sphere env map~~ DONE — binding 18 (`matcapMap`), per-material `matcapTexturePath`, samples real texture when bound, falls back to procedural Dreamcast-style sheen
- ~~Procedural surface noise~~ DONE — `surfaceNoiseScale`/`surfaceNoiseStrength` on MaterialComponent, 3-octave world-space hash noise, encoded in surfaceParam range 400+

### Phase 6: Analog/Degraded Polish (Style 8)

- Film gate weave: per-frame UV jitter with low-frequency sine + randomization
- Light leak / film burn: bindable overlay texture, additive blend, animated position
- Analog tape dropout: horizontal signal loss bands in VHS filter

### Art Style Presets (Editor UI) ✅ COMPLETE

7 one-click presets in Settings > Scene > Art Style Preset dropdown:
- ~~Realistic PBR~~ — GGX, Fresnel, energy conservation, geometry term
- ~~Classic Blinn-Phong~~ — clean default, no stylized effects
- ~~Hand-Painted~~ — half-Lambert + warm shadow ramp (TF2/Genshin-style)
- ~~Toon/Anime~~ — 4-band cel, purple shadows, geometry outlines, anime ramp
- ~~Low-Poly Retro~~ — flat shading, affine texturing, vertex snapping, 16-level posterization
- ~~Pixel Art~~ — 320x240 downscale, point filtering, 16-color palette, Bayer dither
- ~~NPR Sketch~~ — 2-band cel, Sobel outlines (2.0 thickness + curvature weight), crosshatch stipple

`ApplyArtStylePreset()` in SceneRenderSettings.cpp, `artStylePreset` serialized per scene.

### Key Files

- `Engine/shaders/triangle.frag` — lighting model changes (ramp, SSS, specular, wrap, rim, cel color, normal quantize)
- `Engine/shaders/postprocess.frag` — outlines, watercolor, TAM hatching, palette, paper overlay, film effects
- `Engine/include/Enjin/Renderer/SceneRenderSettings.h` — all new scene-level parameters
- `Engine/include/Enjin/ECS/Components/Material.h` — specular map, matcap, per-material cel overrides
- `Engine/src/Editor/EditorLayerRendering.cpp` — art style presets, parameter controls

---


## Networking Config & Settings UX (2026-02-16)

**Change Summary**
- Added external JSON config for networking and security defaults: `config/network_settings.json`.
- Added load/save helpers on `NetworkConfig` and `NetworkSystem` so defaults can be overridden without recompiling.
- Added rate-limit + abuse protection knobs (burst limits, violation window, ban/kick behavior).

**User Editability**
- User-editable: Yes.
- How: Settings > Project > Networking (visual editor), or edit `config/network_settings.json` directly.
- Safe defaults ship in repo; values are runtime-tunable for different games/traffic profiles.

**Settings UX Restructure ✅ COMPLETE (2026-02-20)**
- Consolidated 5 separate panels (Editor Settings, Project Settings, Rendering, Post Processing, Retro Effects) into a single unified Settings window with 3 tabs:
  - **System** (machine-local): Camera, Editor Performance, External IDE, Accessibility, Fonts
  - **Project** (shared in repo via `.enjinproject`): Project Mode, Window Icon, Physics, Frame Rate, Audio (HRTF/Occlusion/Transmission), Collision Groups, Build Config
  - **Scene** (per-scene overrides): "Use Project Defaults" toggle, Skybox, Shadows, Ambient Lighting, Cel Shading, Display Options, Ray Tracing, Light Probes, Post Processing, Retro Effects, Environment
- Migrated HRTF/audio, window icon, and build config from machine-local `editor_settings.json` to project-level `SceneManager` (`.enjinproject`) with backward-compat migration on first load
- ~22 section drawer methods extracted from monolithic panel functions for reusability
- Menu bar restructured: View > Settings > System Settings / Project Settings / Scene Settings
- Command palette updated with 3 new settings commands
- ~~Networking config should live under Project Settings with optional Scene overrides (future).~~ ✅ Networking config editor added to Project Settings tab (2026-02-22). Connection, Sync/Interpolation, Rate Limiting, and Security sections. Also added missing `interpDelay`/`predictionCorrectionSpeed` to JSON serialization.

## Performance Optimization Findings

### Critical Rendering Pipeline Issues

These issues cause frame hitches and should be addressed first.

#### 1. ~~GPU Synchronization Blocking~~ ✅ RESOLVED

Replaced `vkDeviceWaitIdle()` with `VulkanContext::WaitForGPU()` — fence-based wait registered by VulkanRenderer (waits on in-flight fences only, fast graphics-queue-only). 18 subsystems migrated. Falls back to `vkDeviceWaitIdle` only during device teardown. ECS World thread safety added: recursive mutex on structural ops, deferred entity destruction queue flushed at Update() start.

#### 2. ~~Entity Iteration Inefficiency~~ ✅ RESOLVED

Replaced all `GetAllEntities()` + filter patterns with `GetEntitiesWithComponent<T>()` across render, shadow, and flower systems.

#### 3. ~~Per-Entity Texture Lookups~~ ✅ RESOLVED

Added `cachedBaseColorTexture`, `cachedHeightTexture`, `cachedNormalTexture`, `cachedMetallicRoughnessTexture`, `cachedEmissiveTexture` on `MaterialComponent`. Cache invalidated via `InvalidateTextureCache()` on path changes.

#### 4. ~~Play Mode Double-Draw~~ ✅ RESOLVED

During play mode, `m_SkipMainPassRendering` flag bypasses all main swapchain geometry/effects. The sorted render list is now built before the skip check so the offscreen path reuses it (previously fell back to unsorted entity list with zero descriptor cache hits). Pipeline bind, descriptor set bind, viewport/scissor commands moved outside the per-entity loop in `RenderToTarget()`.

#### 5. ~~Frame Jitter on Windows~~ ✅ RESOLVED

Added `timeBeginPeriod(1)` on Windows startup for 1ms sleep resolution (default 15.6ms was causing 5-14ms variance in `sleep_for()`). Increased frame limiter spin margin from 1ms to 2ms. Linked `winmm.lib` via CMake.

### Medium Priority Optimizations

#### 5. ~~Redundant Component Lookups~~ ✅ RESOLVED

Added `GetEntitiesWithComponents<T1, T2>()` multi-component query to World. Replaced `HasComponent() + GetComponent()` double lookups with single `GetComponent()` + null check in ControllerSystem (all 6 controller loops + FollowTarget + LookAtTarget) and SimplePhysics. Extracted `GetColliderInfo()` helper in SimplePhysics to replace repeated 3-way Box/Sphere/Capsule discrimination across 6+ locations.

#### 6. ~~String-Based Entity Lookups in Scripts~~ ✅ RESOLVED

Added `m_NameCache` (`unordered_map<string, Entity>`) to World with lazy rebuild on `FindEntityByName()`. Cache invalidated on entity destroy, world clear, and name changes. `Scene_FindEntity()` now O(1) via `FindEntityByName()`.

#### 7. ~~Vector Allocations Without Reserve~~ ✅ RESOLVED

Added `reserve()` calls before all particle spawn loops in FlowerSystem: `SpawnBreakParticles()`, `SpawnGroundSplash()`, `SpawnTensionDrip()`.

### Physics & Scripting Optimization Pass ✅ RESOLVED (2026-02-11)

#### 8. ~~Physics O(N²) Collision Detection~~ ✅ RESOLVED

Added `SpatialHashGrid` broad-phase to `DetectCollisionEvents()`. Entities inserted into hash cells (FNV-1a, 4m cell size), only entities sharing adjacent cells are tested. 100 colliders: 4,950 brute-force pairs → typically <200 candidate pairs.

#### 9. ~~Physics GetAllEntities() in Hot Paths~~ ✅ RESOLVED

Added `RebuildColliderCache()` — builds deduplicated list of all Box/Sphere/Capsule collider entities once per `Update()`. Replaced 6 `GetAllEntities()` scans (ground check, Raycast, RaycastAll, MoveAndSlide, GetCollidersInRadius, OverlapBox) with `m_CachedColliderEntities`. Scenes with 500 entities but 50 colliders now check 50 instead of 500.

#### 10. ~~Physics Gravity Zone Query Per Rigidbody~~ ✅ RESOLVED

`GetEntitiesWithComponent<GravityZoneComponent>()` hoisted outside the rigidbody loop — 1 query per frame instead of N (one per rigidbody).

#### 11. ~~ScriptSystem Redundant Entity Queries~~ ✅ RESOLVED

Merged 6 `GetEntitiesWithComponent<ScriptComponent>()` calls (4 in Update + 1 FixedUpdate + 1 LateUpdate) into a single per-frame cached query (`m_CachedScriptEntities`). Init + OnStart merged into one pass.

#### 12. ~~EditorLayer GetAllEntities() for Player Lookup~~ ✅ RESOLVED

Cached `m_CachedPlayerEntity` for CharacterController zone detection. Full entity scan only runs on cache miss (play mode start, entity destroyed). Re-scan avoided during steady-state play.

#### 13. ~~LevelStreaming O(N) Duplicate Checks~~ ✅ RESOLVED

Replaced linear scan in load/unload queues with `unordered_set` for O(1) duplicate detection. Pre-built `unordered_map` priority lookup before sorting (sort comparator O(1) instead of O(M) per comparison).

#### 14. ~~World::IsValid() O(N) Pending Destruction Scan~~ ✅ RESOLVED

`IsValid()` and `IsPendingDestruction()` scanned the `m_PendingDestructions` vector linearly for every call. Added companion `m_PendingDestructionSet` (unordered_set) for O(1) lookup. `DestroyEntity()` duplicate check also upgraded from O(N) to O(1).

### Data Structure Improvements

| Current | Recommended | Location | Status |
|---------|-------------|----------|--------|
| ~~`std::map<string, string>`~~ | ~~`std::unordered_map`~~ | ~~DialogueTree.h:106, Gameplay.h:883~~ | ✅ RESOLVED |
| ~~`unordered_map<Entity, RenderData>`~~ | ~~Dense vector indexed by entity~~ | ~~RenderSystem.h~~ | ✅ RESOLVED — `std::vector<EntityRenderData>` indexed by entity ID with `valid` flag, O(1) direct lookup |
| ~~String-keyed texture cache~~ | ~~Integer-keyed or pointer cache~~ | ~~RenderSystem.h~~ | ✅ RESOLVED — `m_TexturePathToId` (path→u32 ID, only on first load) + `m_TextureById` (dense vector), eliminates per-frame string hashing |

### CPU Performance & Scalability Sprint (P0 — 2026-03-17)

Goal: eliminate CPU-side bottlenecks, maximize GPU utilization, and push entity throughput from ~500 to ~2000+ animated actors at 60fps.

#### Tier 1: Quick Wins (1-2 hours each)

- [x] **Binary search keyframes** — `SampleKeyframes()` switched from O(N) linear scan to O(log N) `std::upper_bound`. ✅
- [x] **Integer sprite sort keys** — SpriteBatchRenderer sort comparator uses pre-hashed `usize` keys instead of `std::string` comparison. ✅
- [x] **Cache ECS storage pointers** — `World::GetComponentStorage<T>()` added; RenderSystem caches 5 hot storage pointers per frame, 38 `GetComponent` calls replaced with direct `storage->Get()`. ✅
- [x] **Maintain light entity list** — `m_LightListDirty` flag gates rebuild; set on entity add/remove, with O(1) size-mismatch fallback for dynamic component changes. ✅
- [x] **Pre-allocate sprite shadow vectors** — 3 shadow-pass vectors moved to member variables; `clear()` preserves capacity across frames. ✅

#### Tier 2: Competitive Parity (1-2 days each)

- [x] **Cache world matrices** — `ComputeWorldMatrix()` caches via `mutable` fields on TransformComponent (`cachedWorldMatrix` + `worldMatrixDirty`). Recursive caching shares parent results across siblings. All transforms marked dirty once per frame in a tight linear sweep. ✅
- [x] **Batched material SSBO** — Binding 2 converted from UBO to `STORAGE_BUFFER_DYNAMIC`. All MaterialGPU data collected at frame start into single SSBO, uploaded once. Per-entity dynamic offset at draw time. Fixes latent SSS data race. ✅
- [ ] **Wire bindless into main render path** — `BindlessResourceManager` exists but isn't used in hot path. Migrate material textures to descriptor indexing, eliminate per-entity descriptor set binds.
- [x] **Multi-draw indirect batching** — GPU culling compute shader emits compacted `VkDrawIndexedIndirectCommand` buffer with `indirectEligible` filter. Non-textured pool-eligible entities drawn via single `vkCmdDrawIndexedIndirectCount` call. Shaders read per-object data from ObjectData SSBO (binding 13) via `gl_InstanceIndex`. Textured entities remain on per-entity path. Previous-frame model matrix now sourced from SSBO for correct motion vectors. ✅
- [x] **View-based ECS iteration** — `ECS::View<Components...>` variadic template added (`View.h`). Iterates smallest component set, batch-checks others, supports `Exclude()` filter. Sorted render list build loop converted as proof-of-concept. ✅

#### Tier 3: Next-Gen Scalability (1-2 weeks each)

- [ ] **SIMD math library** — Vector.h / Matrix.h use scalar `f32` ops. Add SSE/AVX paths for matrix multiply, vector normalize, batch bone matrix interpolation. 2-4x math throughput.
- [ ] **Slim MaterialComponent** — Currently ~400 B with 5 `std::string` paths + cached pointers + sort key. Split into hot data (~80 B: colors, floats, flags) and cold data (paths, cache) in separate component. 20-30% cache efficiency.
- [ ] **Delta sprite sorting** — Maintain sorted order across frames; insert/remove on entity add/remove instead of full `std::sort` every frame. O(K) for K changes vs O(N log N).

#### Tier 4: Architectural (1+ month, future)

- [ ] Archetype-based ECS storage (group entities by component signature for cache-optimal multi-component iteration)
- [ ] Task/mesh shader pipeline (VK_EXT_mesh_shader for geometry amplification)
- [ ] Async scene serialization (offload to worker thread)

#### Performance Targets

| Metric | Current | Target | Notes |
|--------|---------|--------|-------|
| 2D animated sprites at 60fps | ~500-1K | 2K-5K | Sprite batching + integer sort |
| 3D skeletal actors at 60fps | ~100-500 | 500-2K | Binary search keyframes + SIMD bones |
| Draw calls per frame (3D) | 1 per entity (textured), 1 total (non-textured pool) | 1 per material bucket | Multi-draw indirect (partial ✅, full with bindless) |
| Descriptor set updates/frame | 1 per entity | 0 (bindless) | Descriptor indexing |
| Animation keyframe lookup | O(N) linear | O(log N) binary | Animation.cpp |
| Sprite sort comparator | String compare | u32 compare | SpriteBatchRenderer |
| World matrix computation | Per-entity walk | Cached + dirty flag | TransformComponent |

### Physics: Jolt + Box2D Integration

**Context:** Stress test (2026-02-11) showed SimplePhysics hits 18ms at 1000 colliders (single-threaded, no sleep, no CCD). Rather than incrementally optimizing SimplePhysics, replace it entirely with battle-tested libraries: **Jolt Physics** (3D, MIT) and **Box2D v3** (2D, MIT). SimplePhysics is retired — Jolt handles 3D+mixed, Box2D handles pure 2D. Both are permissive-licensed, zero royalty, full Enjin ownership retained.

#### Phase 1: IPhysicsBackend Interface + CMake Setup ✅ COMPLETE

- `IPhysicsBackend` abstract 3D interface: `SetWorld`, `Update`, `SetGravity`/`GetGravity`, `Raycast`, `RaycastAll`, `CheckGround`, `GetCollidersInRadius`, `OverlapBox`, `MoveAndSlide`, `CheckAABBCollision`, `CheckSphereCollision`, `GetPendingCollisionEvents`, `ClearPendingCollisionEvents`, `GetConstraintSolver`, `GetName`
- `IPhysicsBackend2D` abstract 2D interface: `Initialize`, `Update`, `Shutdown`, `SetGravity`/`GetGravity` (Vector2), `Raycast`, `RaycastAll` (2D), `OverlapCircle`, `OverlapBox` (2D), collision callbacks (Enter/Exit/SensorEnter/SensorExit), `SetCCDEnabled`, `SetVelocityIterations`, `SetPositionIterations`, `GetName`
- `SimplePhysicsBackend` / `SimplePhysicsBackend2D` — adapter wrappers delegating 1:1 to existing SimplePhysics / PhysicsWorld2D
- `PhysicsBackendFactory` — `CreatePhysicsBackend(type, mode)` / `CreatePhysicsBackend2D(type, mode)` (currently always returns SimplePhysics adapters)
- `PhysicsBackendType` enum: `Auto`, `Jolt`, `Box2D`
- All consumers rewired: PlayMode (`unique_ptr<IPhysicsBackend>` + `unique_ptr<IPhysicsBackend2D>`), ControllerSystem, ScriptBindings, VisualScriptExecutor, NodeDefinition (ExecutionContext), NodeRegistry, EditorLayer, Player app
- Fixed orphaned `SetBindingsPhysics()` — now properly wired in PlayMode::Play()/Stop()
- CMake: `ENJIN_PHYSICS_JOLT` (FetchContent Jolt v5.2.0) and `ENJIN_PHYSICS_BOX2D` (FetchContent Box2D v3.0.0), both OFF by default
- **Files:** `IPhysicsBackend.h`, `IPhysicsBackend2D.h`, `PhysicsBackendType.h`, `SimplePhysicsBackend.h/.cpp`, `SimplePhysicsBackend2D.h/.cpp`, `PhysicsBackendFactory.h/.cpp`, plus 9 modified consumer files

#### Phase 2: Jolt Backend (3D) ✅ COMPLETE

- `JoltBackend : IPhysicsBackend` — full Jolt v5.2.0 wrapper (1273 lines)
- `JoltContactListener` — thread-safe contact buffer + bilateral collision filtering in `OnContactValidate`
- Body creation: `BoxColliderComponent` → `BoxShape`, `SphereColliderComponent` → `SphereShape`, `CapsuleColliderComponent` → `CapsuleShape` (with X/Z rotation via `RotatedTranslatedShape`)
- Collider center offset via `RotatedTranslatedShape`, scale applied at creation, min size clamped to 0.01
- `RigidbodyComponent` mapping: `BodyType` → `EMotionType`, mass override, linear/angular damping, gravity factor, DOF freezing via `mAllowedDOFs`, CCD via `mMotionQuality::LinearCast`, initial velocity
- Entity ID stored in `mUserData` for O(1) reverse lookup from Jolt body to ECS entity
- Jolt `JobSystemThreadPool` (hardware_concurrency - 1 threads), 16 MB `TempAllocatorImpl`
- ECS↔Jolt sync: full per-frame reconciliation (create new bodies, destroy removed, update kinematic via `MoveKinematic`, static position sync)
- Jolt → ECS writeback: position, rotation, velocity, angular velocity, sleep state, ground check (short downward raycast)
- Collision filtering: bilateral rule in `JoltContactListener::OnContactValidate` using per-body `categoryBits`/`collisionMask` (Jolt's `ObjectLayerPairFilter` only sees ObjectLayers 0/1, not BodyIDs — filtering must happen in contact listener)
- Broad phase: 2 layers (NonMoving/Moving), all-pass `ObjectVsBroadPhaseLayerFilter` and `ObjectLayerPairFilter`
- Collision events: Enter/Exit detection via previous/current pair tracking (same pattern as SimplePhysics)
- Raycasting: `NarrowPhaseQuery::CastRay` with `EnjinBodyFilter` for layer mask, surface normal via `BodyLockRead` + `GetWorldSpaceSurfaceNormal`. `RaycastAll` via `AllHitCollisionCollector`
- MoveAndSlide: iterative AABB slide resolution (3 iterations, same API as SimplePhysics)
- Joint mapping (body locking via `BodyLockWrite` for constraint creation):
  - `DistanceJointComponent` → `DistanceConstraint` (spring settings for stiffness < 1)
  - `HingeJointComponent` → `HingeConstraint` (limits, velocity motor)
  - `BallSocketJointComponent` → `PointConstraint` + optional `ConeConstraint`
  - `SpringJointComponent` → `DistanceConstraint` with spring stiffness/damping
  - `FixedJointComponent` → `FixedConstraint` (auto-detect point)
  - `SliderJointComponent` → `SliderConstraint` (axis, limits, velocity motor)
- Breakable joints: stress checked per frame, constraint destroyed if > breakForce
- Gravity zones: per-body `SetGravityFactor(0)` + `AddForce(customGravity * gravityScale * mass)`, highest-priority zone wins
- Sleep: automatic via Jolt (reported as `rb->isSleeping = !bodyInterface.IsActive()`)
- Factory: `PhysicsBackendFactory` returns `JoltBackend` when `ENJIN_PHYSICS_JOLT=ON` and (type==Jolt or Auto with 3D/Mixed mode)
- **Files:** `JoltBackend.h`, `JoltContactListener.h`, `JoltBackend.cpp`, modified `PhysicsBackendFactory.cpp`

#### Phase 3: Box2D Backend (2D) ✅ DONE

- `Box2DBackend : IPhysicsBackend2D` wrapping Box2D v3.0.0 C API (~755 lines)
- Handle-based IDs (`b2WorldId`, `b2BodyId`, `b2ShapeId`, `b2JointId`), not C++ classes
- Full ECS↔Box2D synchronization (create/destroy/update bodies per frame)
- Map ECS 2D components to Box2D bodies:
  - `Body2DComponent` (static/dynamic) → `b2BodyDef` + `b2ShapeDef`
  - Circle/Box/Polygon shapes → `b2CreateCircleShape` / `b2CreatePolygonShape`
  - Physics materials (friction, restitution, density) via direct `b2ShapeDef` fields
  - Collision filtering: `b2Filter` with `categoryBits`/`maskBits` (`uint32_t`)
- Contact + sensor events via polling: `b2World_GetContactEvents()` / `b2World_GetSensorEvents()`
- Raycasting: `b2World_CastRayClosest()` for single, `b2World_CastRay()` with C callback for all
- Overlap queries: `b2World_OverlapAABB()` with C callback for circle/box
- 5 joint types: Revolute, Prismatic, Distance, Rope (Distance+limit), Weld — with limits, motors, spring settings
- CCD via `isBullet` flag on `b2BodyDef`
- Sub-step count (default 4) replaces old velocity/position iterations
- Factory: `PhysicsBackendFactory` returns `Box2DBackend` when `ENJIN_PHYSICS_BOX2D=ON` and (type==Box2D or Auto with 2D/Mixed mode)
- **Files:** `Box2DBackend.h`, `Box2DBackend.cpp`, modified `PhysicsBackendFactory.cpp`

#### Phase 4: Enable Production Backends ✅ COMPLETE

- ~~PlayMode: swap `m_Physics` from `SimplePhysics` to `IPhysicsBackend*`~~ ✅ done (Phase 1)
- ~~ControllerSystem: `SetPhysics(IPhysicsBackend*)` + `SetPhysics2D(IPhysicsBackend2D*)`~~ ✅ done (Phase 1 + Phase 6) — Platformer2D uses 2D raycasts for ground detection, 2D templates switched to Body2DComponent
- ~~ScriptBindings: `SetBindingsPhysics(IPhysicsBackend*)`~~ ✅ done (Phase 1)
- ~~VisualScriptExecutor: `SetPhysics(IPhysicsBackend*)`~~ ✅ done (Phase 1)
- ~~Player app: `unique_ptr<IPhysicsBackend>` via factory~~ ✅ done (Phase 1)
- ~~EditorLayer: Physics debug draw~~ ✅ done (collider wireframes + joint lines via View > Show Colliders toggle)
- ~~Project Settings UI: Physics Backend dropdown (Auto / Jolt / Box2D / Simple)~~ ✅ done — 4 options with availability indicators and resolved name display
- ~~Jolt/Box2D ON by default in CMake~~ ✅ done
- ~~PhysicsTypes.h / PhysicsTypes2D.h extracted~~ ✅ done — interfaces no longer depend on SimplePhysics
- ~~`Simple = 3` added to `PhysicsBackendType`~~ ✅ done
- ~~Factory handles Simple type, dimension mismatch warnings, fallback logging~~ ✅ done
- ~~`IsJoltAvailable()` / `IsBox2DAvailable()` / `IsSimpleAvailable()` helpers~~ ✅ done
- ~~`ResolveBackendName()` for UI display~~ ✅ done

#### Phase 5: Retire SimplePhysics Behind Compile Guard ✅ COMPLETE

- ~~`ENJIN_PHYSICS_SIMPLE` CMake option (ON by default)~~ ✅ done
- ~~All SimplePhysics files guarded: `SimplePhysics.h/.cpp`, `SimplePhysicsBackend.h/.cpp`, `SimplePhysicsBackend2D.h/.cpp`, `ConstraintSolver.h/.cpp`, `Physics2D.h/.cpp`, `PhysicsWorld.h/.cpp`~~ ✅ done
- ~~Factory fallback paths guarded~~ ✅ done — returns `nullptr` + error log when SIMPLE=OFF
- ~~Null-safety audit~~ ✅ done — all physics pointer dereferences already guarded
- ~~StressTest guarded~~ ✅ done — physics benchmarks skipped when SIMPLE=OFF
- ~~Build verified: Jolt=ON Box2D=ON Simple=ON (all targets) and Simple=OFF (all targets)~~ ✅ done

---

## Node Graph Expansion Plan

The existing `NodeGraphEditor` framework is production-ready and currently powers only the Animation Graph. It should be expanded to power multiple systems.

### Framework Capabilities (Already Implemented)

- 12 typed pins (Flow, Bool, Float, Int, String, Vector2, Vector3, Vector4, Quaternion, Color, Entity, Any)
- Wong colorblind-safe pin colors
- Drag-to-connect with Bezier curve visualization
- Pan/zoom canvas with minimap
- Node/link selection, box select, keyboard nav (Tab, Delete, Escape)
- Type-safe link validation callbacks
- Custom node body rendering
- Right-click context menus with categories
- Full JSON serialization
- Node flags (NoDelete, NoMove, Highlighted, Error)

### Expansion Priority

#### Phase 1: Dialogue Tree Editor ✅ COMPLETE

**Status:** Fully implemented with 7 node types, DialoguePlayer state machine, Dialogue Editor panel (View > Tools), EntityEventBus integration, SubtitleSystem accessibility, 10 AngelScript bindings, typewriter effect, choice navigation, and full serialization.

#### Phase 2: Visual Scripting / Blueprints - Phase 1 ✅ COMPLETE

**Status:** Foundation implemented with:
- VisualScriptComponent (graph data, variables, event mappings, node metadata)
- NodeDefinition system with ExecutionContext, PinDefinition, NodeCategory
- NodeRegistry singleton with 15 built-in nodes (OnStart, OnUpdate, Branch, Sequence, Get/Set Variable, Add, Subtract, Multiply, GetPosition, SetPosition, Print, Greater, Less, Not, GetSelf)
- VisualScriptExecutor with flow execution, pure node caching, input resolution
- VisualScriptSystem ECS integration with Initialize/Shutdown/Update lifecycle
- VisualScriptEditor panel (View > Tools) with entity sidebar, node graph canvas, variable editor, node inspector
- Full scene serialization

**Phase 2 (Complete):**
- Extended flow control nodes (ForLoop, While, Delay, Sequence, Gate)
- Math/logic nodes (Add, Subtract, Multiply, Divide, Greater, Less, Equal, And, Or, Not, Negate, Clamp, Lerp, Normalize, DotProduct, CrossProduct, Distance, RandomFloat, RandomInt)
- Transform nodes (GetPosition, SetPosition, GetRotation, SetRotation, GetScale, SetScale, Translate, Rotate, LookAt)
- Component access nodes (GetHealth, SetHealth, Damage, PlayAudio, StopAudio, PlayAnimation)
- Node palette with fuzzy search and recently-used tracking
- Pin value display on nodes
- Undo/redo for node/link creation and deletion

**Phase 3 (Complete):**
- Collision callbacks (OnCollisionEnter, OnCollisionExit, OnTriggerEnter, OnTriggerExit)
- Latent Delay node with execution resume
- Execution visualization (highlighted currently-executing node)

**Phase 4 (Complete):**
- Breakpoints and step-through debugging (F5 continue, F10 step, F9 toggle)
- Execution timeline profiler with color-coded node bars
- WaitForAudioComplete latent node
- WaitForAnimationComplete latent node
- Multi-select copy/paste with preserved internal links
- Box/marquee selection and Ctrl+click multi-select
- Multi-node drag
- Undo/redo for node property edits and variable value edits

**Phase 5+ (Complete):**
- Physics nodes: Raycast, SphereCheck, BoxCheck, AddForce, AddImpulse, SetVelocity, GetVelocity, SetGravityScale + Math_Negate, Component_Has, Debug_PrintWarning/Error
- Subgraph/function nodes: Function_Entry, Function_Return, Function_Call (call depth 32, Functions panel in VS editor)
- AngelScript interop: Script_Call node (call AS functions from VS), reverse bindings (VisualScript_SendEvent/SetVariable/GetVariable from AS)
- Conditional breakpoints (BreakpointInfo with condition string, hit count, Shift+F9 to edit)
- Watch window for variable inspection (panel when paused)
- Call stack view (CallStackEntry push/pop in ExecuteFlow, panel when paused)

#### Phase 3: AI Behavior Tree Editor ✅ COMPLETE

**Status:** Fully implemented with 20 node types (BTNodeType/BTStatus/BTNodeMeta/Blackboard), BehaviorTreeComponent, BehaviorTreeExecutor (tick dispatch by category), BehaviorTreeSystem (ISystem with tickInterval), and color-coded node editor with context menu, auto-layout, blackboard editor, and play-mode status visualization. EditorPanel `1<<19`.

#### Phase 4: Quest Flow Editor ✅ COMPLETE

**Status:** Fully implemented with QuestFlowComponent, visual quest graph editor, node types for quest start, objectives (sequential/parallel), conditions, rewards, branches, and end states. EditorPanel `1<<20`.

### Future Graph Systems ✅ COMPLETE

| System | Effort | Use Case | Status |
|--------|--------|----------|--------|
| Shader Graph | 6-8 weeks | Visual shader authoring, GLSL generation | ✅ Complete (58 node types incl. SceneColor/SceneNormal/SceneDepth/StaticSwitch, type mismatch validation, topological sort GLSL codegen, .enjshader save/load, editor wired) |
| Audio Event Graph | 2-3 weeks | Dynamic audio mixing based on game state | ✅ Complete (runtime execution, trigger events, parameter thresholds, delay scheduling, .enjaudiopkg, 4 AS + 3 VS bindings) |
| Particle System Graph | 2-3 weeks | Sub-emitter chains, complex particle systems | ✅ Complete (compiler to ParticleEmitterComponent, emitter/modifier/control/renderer mapping, .enjparticle, apply-to-entity) |
| Procedural Generation Graph | 4-6 weeks | Visual WFC/L-system/BSP rule composition | ✅ Complete (node-based pipeline editor with 38 node types, graph execution, preview) |

---

## GUI Modernization Plan ✅ COMPLETE

### Brand Color Palette

Replace current blue accent with TEGE brand sage green for distinct identity.

| Role | Hex | RGB | Use |
|------|-----|-----|-----|
| Primary Accent | #C7DAC4 | 199, 218, 196 | Selection, active tabs, buttons, gizmos |
| Secondary Accent | #5B7FA1 | 91, 127, 161 | Links, info icons, secondary buttons |
| Success | #6DB876 | 109, 184, 118 | Validation, play button |
| Warning | #D4A855 | 212, 168, 85 | Cautions |
| Error | #D6726B | 214, 114, 107 | Deletions, errors |
| Background | #0E0E11 | 14, 14, 17 | Keep current |
| Secondary BG | #0B0B0E | 11, 11, 14 | Slightly darker for depth |

### Typography System

| Level | Size | Weight | Color | Use |
|-------|------|--------|-------|-----|
| H1 | 23px | Bold (700) | White | Panel headers |
| H2 | 18px | SemiBold (600) | Light gray | Section titles |
| Body | 17px | Regular (400) | Medium gray | Content |
| Small | 14px | Regular (400) | Dim gray | Labels, hints |
| Mono | 16px | Regular (400) | Soft cyan | Code, IDs |

### Spacing Scale (8px grid)

| Token | Pixels | Use |
|-------|--------|-----|
| XS | 4 | Icon-label gap |
| S | 8 | Tight grouping |
| M | 12 | Default item spacing |
| L | 16 | Section boundaries |
| XL | 24 | Major panel dividers |
| XXL | 32 | Empty state spacing |

### Micro-Interactions ✅ COMPLETE

**Spring Easing:** `cubic-bezier(0.175, 0.885, 0.32, 1.275)` for button press bounce — implemented as `SpringEase()` utility

**Transitions:**
- Hover: 100ms smooth fade — `SmoothFade()` utility with speed parameter
- State change: 200ms spring — `AdvanceSpring()` with configurable stiffness/damping/mass
- Focus: 150ms with glow shadow

**Implementation in ImGui (ImGuiLayer.h):**
- `SpringState` + `SpringParams` + `AdvanceSpring()` — per-frame spring physics simulation
- `ColorTransition` — RGBA color interpolation over frames for hover/focus transitions
- `HoverAnimation` — per-widget tracker with hover alpha fade + press bounce spring
- `UIAnimationState` singleton — global widget animation state manager with ID-keyed hover anims
- `SmoothFade()` / `SpringEase()` inline utilities

### Empty States

Design pattern for panels with no content:
- Centered icon (64x64px, outlined, 40% opacity)
- Heading: "No [Items]"
- Body: "Create one to get started"
- Optional CTA button

### Implementation Phases

1. **Foundation (1-2 weeks):** ✅ Color tokens, font hierarchy, spacing constants
2. **Core Panels (2-3 weeks):** ✅ Hierarchy, Inspector, Viewport styling
3. **Micro-interactions (2 weeks):** ✅ Spring easing, hover effects, color transitions
4. **Polish (1-2 weeks):** ~~Empty states~~ ✅ (DrawEmptyState helper with centered icon, heading, body, CTA button; applied to Hierarchy, Inspector, Asset Browser, Scene List, Dialogue, Plugin Browser, Console, Network, and all "No world loaded" panels), ~~notification toasts~~ ✅ (4 types: Info/Success/Warning/Error, slide-in animation, fade-out on expiry, wired to save/build/template/scene-load/model-import/component-remove events), ~~accent color presets~~ ✅ (6 harmony presets, auto-derive 11 colors), ~~theme preview~~ ✅ (250x160 live preview pane), ~~keyboard shortcuts help~~ ✅ (Ctrl+Shift+/, searchable modal, 5 categories), loading indicators, ~~tooltips~~ ✅ (50+ inspector tooltips across Transform/Material/Light/Camera/Rigidbody/Collider), ~~hierarchy search~~ ✅ (case-insensitive text filter), ~~delete confirmation~~ ✅ (modal dialog for entity deletion), ~~LOD warning~~ ✅ (disabled state warning when LOD system inactive)

---

## Creative Intelligence Features

Ideas for "simple creation of complex games":

### Smart Defaults ✅ COMPLETE

- ~~When adding "Chase Player" AI node, auto-suggest connecting to "Player Entity" pin~~ ✅ Make Patroller auto-targets player
- ~~When creating dialogue, auto-populate speaker name from entity name~~ ✅ Setup NPC reads NameComponent
- ~~When adding physics joint, auto-suggest connected entity from selection~~ ✅ Smart Suggestions detect missing colliders/skeletons/transforms

### Context-Aware Collider Selection ✅ COMPLETE (2026-02-23)

`ChooseColliderForEntity()` selects optimal collider shape based on entity context:
1. **Sprite2D** → Box (1,1,0.1) for 2D sprites
2. **Character/AI controllers** → Capsule sized from mesh AABB (or 0.3r/1.8h default)
3. **DamageComponent** → Sphere trigger sized from mesh max dimension (or r=1.0 default)
4. **Mesh with valid AABB** → Aspect-ratio best-fit: tall (h>1.5x width) → Capsule, uniform (min/max>0.7) → Sphere, else → Box sized to AABB
5. **Fallback** → Box (1,1,1)

`ApplyColliderRecommendation()` applies the recommendation struct to any entity. Used by Smart Suggestions (rules 3, 8, 9), Make Patroller, Setup Destructible, and Add Physics Object patterns.

### Smart Suggestions — 12 Rules ✅ COMPLETE (2026-02-23)

| Rule | Condition | Action |
|------|-----------|--------|
| 1 | Dialogue + no DialogueBox | Add DialogueBox |
| 2 | AIController + no Health | Add Health (100) |
| 3 | Health + no Collider | Add context-aware collider |
| 4 | Controller + no Camera | Add camera for controller type |
| 5 | Mesh + no Material | Add Material |
| 6 | BehaviorTree + no AIController | Add AIController |
| 7 | Sprite + Controller + no Camera2D | Add Camera2DBounds |
| 8 | Damage + no Collider | Add Sphere trigger |
| 9 | Rigidbody + no Collider | Add best-fit collider |
| 10 | Animator + no Skeleton | Add Skeleton |
| 11 | NetworkIdentity + no NetworkTransform | Add NetworkTransform |
| 12 | Health + no controller/AI + no Destructible | Add Destructible |

### One-Click Patterns — 7 Quick Setup ✅ COMPLETE (2026-02-23)

| Pattern | Components Added |
|---------|-----------------|
| Add Basic Movement | Controller + collider + camera (2D/3D aware) |
| Setup 2D Platformer | Platformer2D + Box + Sprite2D + Camera2D |
| Make Patroller | AI (Patrol) + Health (50) + BT + capsule/box collider |
| Setup NPC | Dialogue + DialogueBox + Interactable + sphere/box trigger |
| Make Collectible | Pickup (Coin) + TriggerZone + Tween (bob) |
| Setup Destructible | Health (25) + Destructible + best-fit collider |
| Add Physics Object | Rigidbody (dynamic, gravity) + best-fit collider |

### Cross-System Integration

- Dialogue tree nodes can auto-create quest objectives
- Behavior tree states can trigger animation graph states
- Quest completion can trigger cinematic camera sequences

### Template Marketplace ✅ COMPLETE

`TemplateMarketplace` class (`TemplateMarketplace.h/cpp`) with bundled catalog of 15 curated templates across 5 categories (Starter, Genre, Systems, Retro, Advanced). `MarketplaceEntry` struct with id, name, description, category, author, version, license, projectMode, tags, accentColor, downloadCount, rating, ratingCount, fileSizeBytes, quality. Features: multi-field fuzzy search, category filter chips, sort by name/rating/downloads, install/uninstall to `templates/` directory. Full marketplace UI panel with accent-colored cards and metadata display. Uses `IsOpen()/SetOpen()` pattern. Menu: View > Tools > Template Marketplace.

---

## Feature Accessibility Audit — Walled-Off Systems

Comprehensive audit (2026-02-10) of all engine features to identify systems that are built but unreachable by game developers. **31 gaps found** across 12 categories — **all 31 resolved** ✅

### CRITICAL — Features Built But Broken/Unreachable

| # | System | Issue | Fix |
|---|--------|-------|-----|
| 1 | **Player App — Physics** | ~~Player doesn't create/update PhysicsWorld or PhysicsWorld2D~~ ✅ Fixed | Player now uses `IPhysicsBackend` via `PhysicsBackendFactory` |
| 2 | ~~**Player App — Weather/Water**~~ | ~~No WeatherSystem or Water3D init in Player~~ ✅ Fixed | WeatherSystem initialized, updated, wired to script bindings. Water3D renders through the PBR path on Gerstner-displaced meshes. EnsureWaterMeshes generates the surface for both backends; the player animates it each frame. 2D water is separate (water2d.frag) |
| 3 | ~~**Player App — Particles**~~ | ~~ParticleSystem not created in Player~~ ✅ Fixed | Player now creates and updates ParticleSystem |
| 4 | ~~**Player App — Post-Processing**~~ | ~~No bloom/vignette/FXAA/film grain/color grading/retro effects~~ ✅ Wired | PostProcessing initialized from swapchain, wired to script bindings |
| 5 | ~~**Player App — Save System**~~ | ~~TieredSaveSystem not initialized in Player~~ ✅ Fixed | Default LocalSaveBackend, LoadMeta/SaveMeta, Update, script bindings, VS extern all wired |
| 6 | ~~**Build Pipeline — Asset Packing**~~ | ~~BuildPipeline does NOT pack .as scripts, .enjdlg dialogue, .enjprefab, DataAssets, or audio files~~ ✅ Fixed | All asset types packed (.as, .wav/mp3/ogg/flac, .enjdlg, .enjprefab, .enjdata/.enjschema, .csv, .gltf/.glb/.fbx/.obj, .svg, .png/.jpg/.jpeg/.bmp/.tga/.hdr). Scene scanning fixed: removed broken "components" wrapper check, fixed sprite2D/scriptComponent key names, added tilemap/tree/shrub texture scanning |
| 7 | ~~**Script Engine in Player**~~ | ~~ScriptEngine subsystem pointers (physics, audio, scene manager, etc.) never wired in Player~~ ✅ Fixed | All SetXxx() calls wired after system creation |
| 8 | ~~**UI System — No Script Bindings**~~ | ~~UICanvas/UISystem have zero AngelScript or Visual Script bindings~~ ✅ Fixed | UI canvas bindings + focus management + VS nodes added |
| 9 | ~~**Level Streaming**~~ | ~~Fully implemented but no trigger volumes, no editor UI, no script bindings~~ ✅ Fixed | StreamingVolume/Portal serialization, inspector UI, 6 AS + 6 VS bindings |
| 10 | ~~**Localization System**~~ | ~~LocalizationManager complete but no editor panel, no runtime script bindings~~ ✅ Fixed | 5 AS bindings + 1 VS node added |
| 12 | ~~**Player App — Game Menus**~~ | ~~GameMenuSystem rendered MainMenu/PauseMenu but no callback wired — all buttons dead, no title screen shown~~ ✅ Fixed | Callback wired (New Game/Continue/Resume/Options/How to Play/Quit to Menu/Quit), MainMenu shown after splash, ESC state-aware, Options/HowToPlay Back returns to correct parent screen |

### HIGH — Systems Partially Walled Off (No Script Bindings)

| # | System | Missing |
|---|--------|---------|
| ~~12~~ | ~~Weather~~ | ~~No AS/VS bindings~~ ✅ 6 AS + 2 VS bindings added |
| ~~13~~ | ~~Water~~ | ~~No AS/VS bindings~~ ✅ 6 VS nodes added (SetStyle/WaveHeight/WaveSpeed/GetWaveHeight/Opacity/Color) |
| ~~14~~ | ~~Particles~~ | ~~No AS/VS bindings~~ ✅ 10 AS + 3 VS bindings added |
| ~~15~~ | ~~Networking~~ | ~~No AS/VS bindings~~ ✅ 20 AS + 6 VS bindings added |
| ~~16~~ | ~~Quest System~~ | ~~No AS/VS bindings~~ ✅ 4 AS + 3 VS bindings added |
| ~~17~~ | ~~HUD System~~ | ~~No AS/VS bindings~~ ✅ 2 VS nodes added (SetEnabled/IsEnabled) |
| ~~18~~ | ~~Cinematic System~~ | ~~No AS/VS bindings~~ ✅ 2 AS + 2 VS bindings added |
| ~~19~~ | ~~Destructible System~~ | ~~No AS/VS bindings~~ ✅ 1 AS + 1 VS binding added |
| ~~20~~ | ~~Physics 2D~~ | ~~No AS/VS bindings~~ ✅ 15 AS + 5 VS bindings added |
| ~~21~~ | ~~Prefab System~~ | ~~No AS/VS bindings~~ ✅ 4 AS + 1 VS binding added |
| ~~22~~ | ~~Procedural Generation~~ | ~~No AS/VS bindings~~ ✅ ~15 AS + 9 VS bindings added |

### MEDIUM — Missing Editor Exposure

| # | System | Issue |
|---|--------|-------|
| ~~23~~ | ~~IKComponent~~ | ~~Not in Add Component menu~~ ✅ Added |
| ~~24~~ | ~~TerrainComponent~~ | ~~Not in Add Component menu~~ ✅ Added (Terrain + Terrain2D) |
| ~~25~~ | ~~SkeletonComponent~~ | ~~Not in Add Component menu~~ ✅ Added |
| ~~26~~ | ~~FlowerComponent~~ | ~~Not in Add Component menu~~ ✅ Added (FlowerStem + FlowerPetal) |
| ~~27~~ | ~~Shader Graph~~ | ~~Editor shell exists but generates no actual shader code~~ ✅ Full GLSL code generation with topological sort, 54 node types |
| ~~28~~ | ~~Audio Event Graph~~ | ~~Editor shell exists but doesn't connect to AudioSystem~~ ✅ Full runtime execution via SimpleAudio, 4 AS + 3 VS bindings |
| ~~29~~ | ~~Particle Graph~~ | ~~Editor shell exists but doesn't connect to ParticleSystem~~ ✅ Full compiler to ParticleEmitterComponent |
| ~~30~~ | ~~Animation Graph~~ | ~~Editor shell exists but doesn't drive AnimatorComponent~~ ✅ Dual-mode editor: AnimatorComponent (clip dropdown, speed, blend/exit time, ASM parameters) + StateMachineComponent (game logic SM) |

### LOW

| # | System | Issue |
|---|--------|-------|
| ~~31~~ | ~~Visual Script Nodes~~ | ~~No nodes for weather/water/networking/quest/HUD/cinematic/destructible/physics2D/prefabs/procedural gen~~ ✅ Complete — 126+ nodes across all categories including Gameplay, Physics 2D, Networking, AI/BT, Accessibility, Noise, Streaming, Water, HUD, Procedural Gen |

---

## Implementation Priority Matrix

| Task | Impact | Effort | Priority | Status |
|------|--------|--------|----------|--------|
| Replace GetAllEntities() loops | High | Low | P0 | ✅ Complete |
| Cache texture pointers on materials | High | Medium | P0 | ✅ Complete |
| Replace vkDeviceWaitIdle() with fences | Critical | Medium | P0 | ✅ Complete |
| GPU frustum culling | High | Medium | P0 | ✅ Complete |
| Shadow pipeline overhaul (CSM) | High | Medium | P0 | ✅ Complete |
| Per-entity shadow dither mode | Medium | Low | P1 | ✅ Complete |
| receiveShadows flag wiring | Low | Low | P1 | ✅ Complete |
| Dialogue Tree Editor | Very High | Medium | P1 | ✅ Complete |
| Visual Scripting (Phases 1-4) | Very High | High | P1 | ✅ Complete |
| Project Hub enhancements | Medium | Medium | P1 | ✅ Complete |
| Skybox rendering fixes | Medium | Low | P1 | ✅ Complete |
| Skeleton/Animator serialization | High | High | P1 | ✅ Complete |
| Visual Scripting (Phase 5+) | High | Medium | P1 | ✅ Complete |
| Soft shadows (Poisson disk PCF) | Medium | Medium | P1 | ✅ Complete |
| Sprite batching by texture atlas | High | Medium | P1 | ✅ Complete |
| Point/spot light shadows | Medium | High | P1 | ✅ Complete |
| 2D sprite art pipeline | High | High | P1 | ✅ Complete |
| AI Behavior Tree Editor | High | Medium | P2 | ✅ Complete |
| Quest Flow Editor | High | Low | P2 | ✅ Complete |
| GUI color palette update | Medium | Low | P2 | ✅ Complete |
| Typography system | Medium | Low | P2 | ✅ Complete |
| Multi-threaded command buffer recording | High | High | P2 | ✅ Complete |
| Indirect rendering + GPU culling fix | High | High | P1 | ✅ Complete |
| Async compute queue | High | Medium | P2 | ✅ Complete |
| Hi-Z occlusion culling | High | High | P2 | ✅ Complete |
| Frame graph resource scheduling | Medium | High | P2 | ✅ Complete |
| Merged geometry buffer | High | Medium | P1 | ✅ Complete |
| Undo/redo for inspector property edits | Medium | Medium | P2 | ✅ Complete |
| Customizable accent colors | Medium | Low | P2 | ✅ Complete |
| Asset browser with thumbnails | Medium | Medium | P2 | ✅ Complete |
| Micro-interactions | Medium | Medium | P3 | ✅ Complete |
| Basic networking (LAN multiplayer) | High | High | P2 | ✅ Complete |
| RT Shadows | High | High | P2 | ✅ Complete |
| RT Reflections + AO | High | High | P2 | ✅ Complete |
| RT Global Illumination | High | High | P2 | ✅ Complete |
| Path tracing mode | Medium | High | P3 | ✅ Complete |
| SVGF denoiser | Medium | Medium | P2 | ✅ Complete |
| RT Compositor | Medium | Medium | P2 | ✅ Complete |
| Scene & Entity Locking | Medium | Medium | P2 | ✅ Complete |
| Motor Accessibility | Medium | Low | P2 | ✅ Complete |
| Keyboard Navigation | Medium | Low | P2 | ✅ Complete |
| Procedural Generation Graph | High | Medium | P2 | ✅ Complete |
| Alternative Input Devices | Medium | Low | P2 | ✅ Complete |
| Command Palette | Medium | Low | P2 | ✅ Complete |
| Audio Visual Indicators | Medium | Low | P2 | ✅ Complete |
| Screen Reader Announcer | Medium | Low | P2 | ✅ Complete |
| Scene Lock UI Enhancements | Medium | Low | P2 | ✅ Complete |
| Panel Reorganization | Medium | Medium | P2 | ✅ Complete |
| **— Runtime Accessibility —** | | | | |
| UICanvas Keyboard/Gamepad Navigation | High | Medium | P1 | ✅ Complete |
| In-Game Accessibility Menu Template | High | Medium | P1 | ✅ Complete |
| Focus Indicators for UICanvas | Medium | Low | P1 | ✅ Complete |
| Wire AlternativeInput to Player/UISystem | Medium | Medium | P2 | ✅ Complete |
| Wire Announcer to UISystem (screen reader) | Medium | Medium | P2 | ✅ Complete |
| Accessible Labels on UIElement | Medium | Low | P2 | ✅ Complete |
| High Contrast UI Theme | Medium | Low | P2 | ✅ Complete |
| Player Font Scaling | Medium | Low | P2 | ✅ Complete |
| Reduced Motion for UI | Medium | Low | P2 | ✅ Complete |
| Dyslexia-Friendly Font/Spacing | Low | Low | P3 | ✅ Complete |
| Colorblind-Safe UI Palettes | Low | Low | P3 | ✅ Complete |
| One-Button / Switch Access for UICanvas | Low | Medium | P3 | ✅ Complete |
| Security & Robustness Audit | High | Low | P1 | ✅ Complete |
| **— Feature Accessibility (Walled-Off Systems) —** | | | | |
| Player App: Wire physics/particles/weather/post-process/save | Critical | High | P0 | ✅ Complete (physics via IPhysicsBackend) |
| Build Pipeline: Pack scripts/audio/dialogue/prefabs/data assets | Critical | Medium | P0 | ✅ Complete |
| Wire ScriptEngine subsystem pointers in Player | Critical | Medium | P0 | ✅ Complete |
| Add UI System script bindings (AS + VS) | Critical | High | P1 | ✅ Complete |
| Add Weather/Water/Particles script bindings | High | Medium | P1 | ✅ Complete |
| Add Physics 2D script bindings | High | Medium | P1 | ✅ Complete |
| Add Quest/HUD/Cinematic/Destructible script bindings | High | Medium | P1 | ✅ Complete |
| Add Networking script bindings | High | High | P1 | ✅ Complete |
| Add Prefab script bindings | Medium | Medium | P2 | ✅ Complete |
| Add Procedural Gen script bindings | Medium | Medium | P2 | ✅ Complete |
| Wire Level Streaming (trigger volumes + editor + bindings) | High | High | P1 | ✅ Complete |
| Wire Localization (editor panel + script bindings) | High | Medium | P1 | ✅ Complete |
| Add IK/Terrain/Skeleton/Flower to Add Component menu | Medium | Low | P2 | ✅ Complete |
| Add missing Visual Script nodes (mirrors binding gaps) | Medium | Medium | P2 | ✅ Complete (22 Gameplay + 6 Noise + 6 Streaming nodes) |
| Connect graph editor shells (shader/audio/particle/anim) | Low | Very High | P3 | ✅ Complete (shader codegen, audio runtime, particle compiler) |
| Default 60 FPS Frame Rate Limit | Low | Low | P1 | ✅ Complete |
| Tiered Save System (20 slots, 3 tiers) | High | High | P1 | ✅ Complete |
| Play Mode Diff Dialog | High | Medium | P1 | ✅ Complete |
| Save/Load UI (Editor + In-Game) | Medium | Medium | P2 | ✅ Complete |
| Cloud Save Backends (NG/Steam) | Medium | Medium | P2 | ✅ Complete |
| Save System Script Bindings (AS + VS) | Medium | Medium | P2 | ✅ Complete |
| Wire AISystem into PlayMode and Player | Critical | Low | P0 | ✅ Complete (patrol/chase/flee/wander/navmesh — was 615+ lines dead code) |
| Player App: Automatic title screen & pause menu | High | Low | P1 | ✅ Complete (MainMenu after splash, callback wiring, state-aware ESC, Options/HowToPlay return-screen tracking) |
| Add AS bindings for 22 gameplay component types | High | High | P1 | ✅ Complete (~180 new bindings, total ~570) |
| Wire plugin/audio graph/input action bindings in PlayMode | Medium | Low | P1 | ✅ Complete |
| Pack graph assets in BuildPipeline | Medium | Low | P2 | ✅ Complete (.enjshader/.enjaudiopkg/.enjparticle) |
| Quaternion GetRotationZ/GetForward/GetRight/GetUp helpers | High | Low | P1 | ✅ Complete (eliminates ToEuler/ToMatrix in 19+ hot paths) |
| OIDN integration | Medium | Medium | P3 | ✅ Complete (OIDNDenoiser as SVGF alternative, CMake ENJIN_RAYTRACING_OIDN, editor denoiser type selector) |
| **— Rendering & Camera —** | | | | |
| Camera presets (iso, side-scroller, etc.) | Medium | Low | P2 | ✅ Complete |
| Tilt-shift / miniature effect | Low | Medium | P3 | ✅ Complete |
| Bokeh depth of field | Medium | High | P2 | ✅ Complete |
| Cel shading / toon rendering | High | Medium | P2 | ✅ Complete |
| Full-screen stippling & dither | Medium | Low | P3 | ✅ Complete |
| **— Artistic Rendering —** | | | | |
| Parallax occlusion mapping (advanced) | Medium | Medium | P2 | ✅ Complete |
| Flat-shaded low-poly with dithered gradients | Medium | Medium | P3 | ✅ Complete (ditherGradient on MaterialComponent, 2-8 bands, 6 dither patterns, surfaceParam1 push constant) |
| Metaball / blob rendering | Medium | High | P3 | ✅ Done |
| Spherical harmonics lighting | High | High | P2 | ✅ Complete |
| Beam tracing and cone tracing (VXGI) | High | Very High | P3 | ✅ Done |
| SDF ray marching | High | High | P2 | ✅ Complete |
| SDF rendering (3D vector art) | Medium | High | P3 | ✅ Done |
| Order-independent transparency (depth peeling) | Medium | Medium | P2 | ✅ Complete |
| Framebuffer feedback effects | Medium | Low | P3 | ✅ Done |
| Screen-space distortion as primary aesthetic | Medium | Medium | P3 | ✅ Done |
| IK-driven mesh deformation | Medium | High | P3 | ✅ Done |
| Fractal terrain & L-system vegetation (advanced) | High | High | P2 | ✅ Complete (fBm terrain with ridged multifractal, hydraulic/thermal erosion, 3D L-system turtle with stochastic rules) |
| **— Simulation-Driven Geometry —** | | | | |
| ~~Reaction-diffusion on meshes~~ | Medium | High | P3 | ✅ Done |
| ~~Cellular automata as geometry~~ | Medium | Medium | P3 | ✅ Done |
| ~~Slime mold simulation (Physarum)~~ | Medium | Medium | P3 | ✅ Done |
| Fluid simulation as terrain | Medium | Medium | P2 | ✅ Complete (FluidTerrainCoupling: erosion + accumulate modes, bidirectional terrain↔fluid) |
| Voronoi fracture with persistent physics | High | Medium | P2 | ✅ Complete |
| **— Simulation & Flow —** | | | | |
| Curl noise flow fields | High | Medium | P2 | ✅ Complete |
| Wave Racer 64 water | Medium | High | P3 | ✅ Done |
| Mesh audio reactivity via FFT | Medium | Medium | P3 | ✅ Done |
| **— Mathematical & Exotic Geometry —** | | | | |
| Fourier transform meshes | Low | Medium | P4 | ✅ Done |
| Non-Euclidean geometry rendering | High | Very High | P3 | ✅ Done |
| Stereographic projection of 4D objects | Low | Medium | P4 | ✅ Done |
| **— Inverse & Advanced Rendering —** | | | | |
| Inverse / differentiable rendering | Medium | Very High | P4 | ✅ Done |
| **— Asset Libraries —** | | | | |
| Font library (30-50 OFL fonts) | High | Low | P2 | ✅ Complete (42 OFL/Apache fonts, 8 categories, FontLibrary.h/cpp, editor browser with search/category/install) |
| 3D asset library (CC0) | High | High | P2 | ✅ Complete (16 CC0 3D model packs — Kenney/Quaternius, AssetLibrary.h/cpp, editor browser) |
| 2D asset library (CC0) | High | Medium | P2 | ✅ Complete (15 CC0 2D sprite/tileset/UI packs, 14 categories, editor browser) |
| **— Editor & Project —** | | | | |
| Template rebuild & demo scenes | Medium | Medium | P2 | ✅ Complete (38→22→44→51 templates, all polished +50%) |
| Project Hub redesign (landing + wizard) | Medium | High | P2 | ✅ Complete |
| Template creator tool | Medium | Medium | P3 | ✅ Complete (TemplateCreator.h/cpp, save/load/scan/delete, View > Tools > Template Creator, templates/ directory) |
| Template marketplace | Medium | Medium | P3 | ✅ Complete (TemplateMarketplace.h/cpp, 15 curated templates, search/filter/sort, install/uninstall, View > Tools) |
| Notification toast system | Medium | Low | P3 | ✅ Complete (4 types, slide-in/fade-out, wired to save/build/template/scene-load/model-import/component-remove events) |
| Accent color harmony presets | Medium | Low | P3 | ✅ Complete (6 presets, auto-derive 11 accent colors, fine-tune in sub-tree) |
| Theme preview pane | Low | Low | P3 | ✅ Complete (250x160 live preview in Settings > System) |
| Settings UX restructure | High | Medium | P2 | ✅ Complete (unified 3-tab window: System/Project/Scene, data migration to .enjinproject, ~22 section drawers) |
| Keyboard shortcuts help | Medium | Low | P3 | ✅ Complete (Ctrl+Shift+/, searchable modal, 5 categories) |
| Hierarchy search bar | Medium | Low | P3 | ✅ Complete (case-insensitive text filter, hides non-matching entities) |
| Entity delete confirmation | Low | Low | P3 | ✅ Complete (modal dialog with Cancel/Delete) |
| Inspector tooltips | Medium | Low | P3 | ✅ Complete (50+ tooltips across Transform/Material/Light/Camera/Rigidbody/Collider) |
| Source-app import presets | Medium | High | P3 | ✅ Complete (10 DCC presets with auto-detection, per-axis flip toggles, texture search paths, editor import dialog) |
| Pre-built binary distribution | High | Medium | P2 | ✅ Complete (CMake install rules + CPack, Windows ZIP, scripts/package.bat + package.sh) |
| Installer distribution | Medium | High | P3 | ✅ Complete (Inno Setup installer with icon, license page, component selection, file associations, Start Menu/Desktop shortcuts; NSIS via CPack as alternative) |
| Hub application (launcher) | Medium | Very High | P4 | ✅ Done |
| **— Legacy Timeline Tools —** | | | | |
| Legacy animation import & conversion | Medium | Very High | P4 | ✅ Done |
| ~~Timeline-based authoring~~ | Medium | High | P3 | ✅ Done |
| AS2/AS3 → AngelScript transpiler | Medium | Very High | P4 | ✅ Done |
| **— Collaboration —** | | | | |
| Collaborative editing (OT/CRDT) | Medium | Very High | P4 | ✅ Complete (EditorLayer wiring: remote edit callbacks, scene sync, edit recording at 5 points, status indicator) |

---

## Metrics to Track

### Performance

- Frame time P99 (target: <16ms for 60fps)
- Draw calls per frame
- Entity iteration count per system per frame
- Texture cache hit rate

### Editor UX

- Time to create first playable entity (target: <30s)
- Clicks to add a component (target: 2)
- Time to find a component in search (target: <2s)

### System Complexity

- Node graph node count (warning at 500+)
- Script instruction count per frame
- Scene entity count (warning at 10,000+)

---

## Editor Tools & UX ✅ COMPLETE

### Completed

- ~~**Extended Model Format Support**~~ ✅ — PLY (ASCII/binary point cloud/mesh) and VOX (MagicaVoxel voxel with greedy face merging) import via custom loaders, routed through SceneImporter
- ~~**Template Rebuild & Demo Scenes**~~ ✅ — Redesigned from 38 to 22 focused templates, then expanded to 51 total across 7 categories (Foundations, Genre Showcases, Systems Deep-Dives, Retro & Timeline, Advanced, Multiplayer, Debug/Test), each showcasing real engine features. All templates polished with additional entities, components, atmosphere, HUD elements, and gameplay setups
- ~~**Planet Gravity Template**~~ ✅ — Super Mario Galaxy-style spherical gravity third-person platformer (GravityZoneComponent Point mode, SurfaceAlignedController, orbit camera, 4 surface platforms, 6 coins)
- ~~**Editor Accent Color & Theming**~~ ✅ — ~~Replace blue accent with TEGE brand sage green~~ (done), ~~customizable accent colors in editor settings~~ (done), ~~accent color harmony presets~~ (done — 6 presets: Default Blue, Warm Orange, Forest Green, Royal Purple, Crimson Red, Teal, auto-derive 11 colors), ~~theme preview pane~~ (done — 250x160 live preview), ~~notification toasts~~ (done — 4 types, slide-in/fade-out), ~~keyboard shortcuts help~~ (done — Ctrl+Shift+/, searchable, categorized), rounded corners, softer panel borders, distinct visual identity
- ~~**Curved Grid Snapping**~~ ✅ — Snap entity placement to curved/spherical grid surfaces with orientation alignment. Surface Snap mode projects entities onto terrain heightmaps and sphere gravity zones, with normal alignment (yaw-preserving) and settings persistence. `Quaternion::FromToRotation()` utility added
- ~~**Improved Icon/Window Inspector**~~ ✅ — Entity icons in hierarchy (bracket-tagged by primary component type), component icons on inspector headers, window icon picker in Settings > Project with browse/apply/persist
- ~~**Settings UX Restructure**~~ ✅ — Unified 5 separate panels into single Settings window with 3 tabs (System/Project/Scene). Extracted ~22 section drawer methods. Migrated HRTF/audio, window icon, and build config from EditorSettings to SceneManager (.enjinproject) with backward-compat migration. Menu bar restructured (View > Settings > System/Project/Scene). Command palette updated
- ~~**Asset Browser Panel**~~ ✅ — Grid/list view toggle with cached directory listing, image thumbnails via texture cache, search/filter bar, hover tooltip with 256px preview, drag source for future drag-to-viewport, type-colored labels (3D/SCN/SHD/IMG/AS/SFX/PFB), adjustable thumbnail size

### Partially Complete

- ~~**Project Hub & Creation Wizard**~~ ✅ (v1) — All 4 tabs (Recent/New/Open/Demos), 38 templates with category filtering and search, git init option, custom templates, folder structure auto-creation, template hover preview all done. ~~Auto-thumbnail capture~~ ✅ done. **v2 redesign planned** — see "Project Hub Redesign & Template Creator" section: 3-action landing (New/Open/Sandbox), template creator with panel checkboxes, project name in separate popup, `TEGE_Projects` default directory, software distribution tiers
- ~~**Undo/Redo**~~ ✅ — Entity operations, visual script node edits, inspector property edits, tilemap paint (per-stroke with cell deduplication), terrain sculpt (heightmap+splatmap snapshot), UI editor edits (move/resize/nudge/delete) all done
- ~~**Drag and Drop**~~ ✅ — OS file drop, hierarchy reparenting, asset browser to Game View (model/prefab/image/scene/audio/script dispatch), material inspector texture fields, sprite inspector texture fields all done
- ~~**Asset Import Pipeline**~~ ✅ — Import settings dialog, .enjinasset metadata, asset browser drag-import, source-app import presets (10 DCC tools), thumbnails (CPU rasterizer with caching), texture compression (BC1/BC3/BC4/BC5/BC7/ASTC with mipmap gen) — fully complete

### Source-App Import Presets ✅ COMPLETE

Smart import presets for common DCC tools with automatic axis/scale/material fixups. Editor dialog with auto-detection from file metadata, per-axis flip toggles, and texture search paths. **Skeletal animation import** added to Assimp path (2026-02-22): bone weight extraction, skeleton building, animation clip conversion with axis conversion — FBX/DAE/3DS and all Assimp-supported formats now import rigged/animated models with SkeletonComponent + AnimatorComponent + auto-play, matching glTF parity.

- **Blender** — Z-up → Y-up axis swap, -X forward convention, scale factor (Blender default 1.0 = 1m), auto-detect .blend material names for PBR slot mapping
- **Maya** — Y-up (native match), cm-to-m scale conversion (0.01), FBX ASCII vs binary handling, Lambert/Phong → PBR material approximation
- **3ds Max** — Z-up → Y-up axis swap, system-unit scale auto-detect, multi/sub-object material splitting, .max bitmap path resolution
- **Houdini** — Y-up (native match), scale normalization, procedural attribute mapping (Cd → vertex color, N → normals), packed primitive instancing
- **Cinema 4D** — Y-up (native match), cm-to-m scale, C4D material channels → PBR mapping, MoGraph instance support
- **ZBrush** — Z-up → Y-up, massive poly count warning + auto-decimate option, polypaint → vertex color, subdivision level selection
- **Substance Painter** — Texture set auto-detection, PBR channel mapping (baseColor/normal/roughness/metallic/height/emissive), texture resolution options
- **Unreal/Unity** — Left-hand → right-hand coordinate flip, FBX scale factor differences, material parameter name remapping
- **SketchUp** — Z-up → Y-up, inches/feet-to-meters, face-front/back material separation
- **Custom** — User-defined axis order, handedness, scale, forward/up vectors, material slot overrides

Import dialog enhancements:
- Source app dropdown with auto-detection heuristic (parse FBX metadata `Creator` field, glTF `generator` field)
- Per-axis sign flip toggles (negate X/Y/Z individually) for edge cases
- Material companion file support: OBJ+MTL (already supported via Assimp), FBX embedded textures, glTF separate .bin + texture folder resolution
- Material path remapping: search paths for missing textures, relative ↔ absolute path conversion, texture folder override
- Preview panel: show model with current import settings before committing (axis orientation, scale, material preview)

---

## Runtime Systems ✅ COMPLETE

- ~~**Improved Physics**~~ ✅ — 2D physics (Box2D-style): PhysicsWorld2D with circle/box/polygon shapes, 5 joint types (revolute, prismatic, distance, rope, weld), CCD, physics materials (friction, restitution, density), 2D raycasts/overlap queries, impulse-based collision resolution with SAT, collision enter/exit callbacks, bitmask filtering
- ~~**Basic Networking**~~ ✅ — Host-authoritative UDP networking with client-side prediction: `NetworkSystem` (connections, heartbeats, timeouts), `NetworkTransport` (cross-platform non-blocking UDP sockets — Winsock2/BSD), `NetworkSerializer` (binary read/write), `NetworkIdentityComponent` + `NetworkTransformComponent`, entity ownership + transfer, 20Hz state sync with delta compression (field bitmask), interpolation buffer (4-state ring with configurable delay), RPC system (FNV-1a hashed names, reliable/unreliable), lobby (player list, ready state, host migration stubs), reliable delivery (sequence numbers, ack bitfield, retransmission), HMAC-SHA256 packet authentication (`NetworkSecurity.h`: SHA-256 + HMAC + session key exchange + 64-bit replay window with constant-time verify), editor Network Panel (host/join/disconnect, player list table, ping/loss/bandwidth stats), full scene serialization
- ~~**Destructible Environments**~~ ✅ — DestructibleSystem with 4 fracture patterns (Voronoi, Grid, Radial, Shatter), debris spawning with physics (velocity, gravity, angular velocity, lifetime), chain destruction propagation with radius/delay/falloff, per-entity FractureConfig, health-based damage triggers. Extended with persistent Voronoi fracture (VoronoiMeshFracture + FractureConfigComponent): real ECS fragment entities with rigidbodies, pre-fracture with breakable joints, recursive re-fracture, fragment entity limit, auto-cleanup
- **Simple Fluid Simulation** — ~~Grid-based Eulerian fluid (water, lava, gas). FluidVolumeComponent with preset configs. Target: 64x64 2D / 32x32x32 3D at 60fps~~ ✅ Stable Fluids solver (Jos Stam), 5 presets (Water/Lava/Gas/Smoke/Steam), GPU instanced cell renderer, full editor integration
- **SVG Support** — ~~nanosvg parsing, rasterize-to-texture via SVGLoader, GetOrLoadTexture routing for .svg files~~ ✅. ~~SDF vector rendering~~ ✅. ~~UIElement Image widget integration~~ ✅ (RenderImage uses TextureResolver, SVG routes automatically)
- ~~**Dialogue System Future Work**~~ ✅ (partial) — .enjdlg asset files (DialogueAsset save/load with versioned JSON), LocalizationManager singleton (string key → locale tables, CSV/JSON import/export, parameterized strings with {key} substitution, runtime locale switching, LOC() macro). ~~UICanvas dialogue box~~ ✅ (DialogueBoxComponent auto-builds UICanvas elements with speaker name, typewriter text, portrait, choice buttons, continue indicator). Remaining: Yarn Spinner/Twine import/export
- ~~**Tiered Save System**~~ ✅ — 20-slot save system (17 manual + 3 rotating auto-save) with 3-tier persistence (`PersistenceTier`: SceneState/RunState/MetaProgression). `TieredSaveSystem` class with slot operations, meta-progression key-value store (float/int/bool/string), auto-save timer (configurable interval + on scene transition + on checkpoint), `ISaveBackend` interface with pluggable backends (`LocalSaveBackend`, `SteamSaveBackend` via `ENJIN_STEAM` CMake flag). 15 AngelScript bindings (SaveGame_ToSlot/FromSlot/DeleteSlot/Checkpoint, Meta_Set/Get Float/Int/Bool/String, Meta_Save, AutoSave_Enable/SetInterval). 6 visual script nodes (Gameplay category: SaveToSlot, LoadFromSlot, DeleteSlot, Checkpoint, MetaSetFloat, MetaGetFloat). `SaveLoadMenuComponent` for in-game save/load grid UI. Save Debug editor panel (bit 31). `PlayModeDiff` for cherry-pick entity changes on play mode Stop

---

## Rendering Pipeline & Performance

### Recently Completed

- **GPU Frustum Culling** — Integrated into render pipeline, skips off-screen entities before draw calls
- **Shadow Pipeline Overhaul** — Back-face culling in shadow pass with pipeline depth bias (CSM 0.75/0.75, point 0.5/0.5, spot 0.5/0.5), removed shader-side bias. Fixes ring-of-light under curved objects. Correct cascade frustum computation with world-space ray interpolation
- **Per-Entity Shadow Dither** — 3 modes (by darkness, distance, angle) with 6 built-in dither patterns (Bayer 4x4, Bayer 8x8, Blue Noise, Halftone, Crosshatch, Overlook). Mode in flag bits 14-15, pattern in bits 29-31
- **receiveShadows Flag** — Now checked in shader; entities can opt out of receiving shadows
- **Shadow Caster Caching** — Pre-filtered shadow caster list avoids redundant iteration per cascade

### Recently Completed (cont.)

- **Player Title Screen & Pause Menu** — `GameMenuSystem` buttons now fully functional in game builds. Splash screen transitions to MainMenu (New Game / Continue / Options / How to Play / Quit). New Game starts gameplay, ESC opens PauseMenu with Resume / Options / How to Play / Quit to Menu. Options and HowToPlay Back buttons track which parent screen (MainMenu or PauseMenu) they came from via `m_ReturnScreen`. Gameplay updates gated on `m_GameStarted` flag. Files: `GameMenus.h/cpp`, `Player/src/main.cpp`
- **Sprite Texture Atlas** — Runtime shelf-packing of sprite textures (<=512px) into a single 4096x4096 GPU texture. Sprites sharing the atlas batch into one instanced draw call via `"__atlas__"` sentinel key. Per-instance UVs linearly remapped into atlas regions. Lazy rebuild on new textures, invalidation on texture hot-reload. Oversized/failed textures excluded and fall back to individual draw calls
- **Soft Shadows (Poisson Disk PCF)** — 16-sample Poisson disk PCF with configurable shadow softness radius. Applied to directional (CSM), point, and spot light shadows
- **Point/Spot Light Shadow Maps** — Cubemap array depth maps for up to 4 point lights (1024² per face, 6 faces each), 2D array depth maps for up to 4 spot lights (1024²). Shadow data SSBO (binding 12), new descriptor bindings 10-12. Shadow-casting light selection by intensity/distance² scoring. Soft shadows via 3D tangent-frame Poisson disk for point lights, standard 2D Poisson for spot lights
- **3D/2D Pipeline Audit** — Auto-disable shadow pass for 2D-only scenes (`ClassifySceneComposition()` gates all shadow passes by `Scene3D`), sprite batching by texture atlas, ortho/perspective camera mixing diagnostic (every 300 frames), Scene2D fast paths (skip normal map descriptor for unlit sprites, early-out of `UpdateFrameUniforms()` light iteration)
- **Editor Frustum Culling Fix** — GPU frustum culling now disabled in editor mode (`SetEditorMode(true)`) so all entities are visible in the scene view for editing. Player builds retain frustum culling against the game camera. Guards on `BuildCullableObjectList()`, culling dispatch, and all `IsVisible()` skip checks
- **Inspector Undo/Redo** — `InspectorUndo.h` with `PropertyEditCommand<T>` template and drop-in ImGui widget wrappers (DragFloat, DragFloat3, SliderFloat, SliderInt, DragInt, ColorEdit3, Checkbox, Combo, InputText, InputTextMultiline). Continuous widgets snapshot on `IsItemActivated()` and push one undo entry on `IsItemDeactivatedAfterEdit()`. All 60+ `Draw*Component` functions in EditorLayer converted (~500+ widget calls)

### Post-Process Volumes ✅ COMPLETE

- **PostProcessVolumeComponent** — Box/Sphere spatial volumes with priority-based blending, smoothstep falloff at edges via `blendRadius`, global volumes (apply everywhere), selective override mask (19 bits for all effect groups). `BlendPostProcessSettings()` lerps all ~80 PP fields grouped by override mask. Full inspector UI with shape/extents/blend radius/weight/priority, override group checkboxes, embedded PP settings tree. Purple wireframe visualization for volume bounds. Full serialization (all 3 serialize + 4 deserialize paths). 10 AS bindings + 4 VS nodes. Player wired.
- **PP Pipeline Optimization** — `HasAnyActiveEffects()` skips entire PP pass when no effects active (no intermediate RT, no barriers, no fullscreen draw). `NeedsDepthBuffer()` skips 2 depth barriers when DoF/TiltShift/CelOutline are all disabled.

### Screen-Space Effects ✅ COMPLETE

- **God Rays** ✅ — Radial blur from screen-space light position. Configurable density, weight, decay, exposure, samples. Runs in postprocess.frag using depth + invViewProj + light direction
- **SSAO** ✅ — Hemisphere sampling with depth-reconstructed view-space positions. Configurable radius, bias, intensity, sample count
- **Contact Shadows** ✅ — Screen-space ray march toward light for sub-cascade shadowing. Configurable ray length, step count, thickness, fade distance
- **Fake Caustics** ✅ — Animated Voronoi pattern projected below configurable water height. Configurable scale, speed, intensity, depth-aware fade
- **Fog Shafts** ✅ — Volumetric-style light shafts through fog via depth-aware radial sampling. Configurable density, intensity, decay, samples
- All 5 effects: ~256 bytes added to PostProcessSettings UBO, editor UI with 5 collapsing headers, 30 AS bindings + 5 VS nodes, full SceneRenderSettings serialization, Player wiring (invViewProj + light direction/screen-pos), PostProcessVolume override bits 19-23

### Pipeline Optimization ✅ COMPLETE

All pipeline optimization items resolved: multi-threaded command buffer recording, GPU payload batching (sort by pipeline/material), indirect rendering (VkCmdDrawIndexedIndirect), async compute for culling/particles/post-process, frame graph resource scheduling, Hi-Z culling. Extended with clustered forward lighting (16x9x24 grid, compute shaders for bounds + light assignment), GPU two-phase HiZ occlusion culling (frustum + depth pyramid cull, partial regen, re-test pass), Variable Rate Shading (`VK_KHR_fragment_shading_rate`), Virtual Texturing (page-based streaming with feedback buffer), Visibility Buffer (deferred material resolve), per-frame linear allocator (`FrameAllocator`), 64-bit material sort keys, and LOD hysteresis with screen-space sizing.

### Camera Presets & Cinematic Effects ✅ COMPLETE

- **Camera Presets** ✅ — 9 built-in presets (Isometric45/30, TopDown, SideScroller, FirstPerson, ThirdPerson, CinematicWide, SecurityCam, BirdsEye). `CameraPreset` enum with `ApplyCameraPreset()` returning configured camera + recommended rotation. Inspector dropdown in Camera component header. Script bindings: `Camera_ApplyPreset()`, `Camera_GetPresetName()`
- **Tilt-Shift / Miniature Effect** ✅ — Post-process blur with configurable focus Y position, band width, and blur amount. Full PostProcessSettings UBO fields, SceneRenderSettings config, JSON serialization, editor UI. GPU shader: 25-tap blur weighted by screen-Y distance from focus band
- **Bokeh Depth of Field** ✅ — Focal distance, focal range, near/far blur strength, bokeh size, aperture shape (Circle/Hexagon/Octagon), CoC debug visualization mode. Full pipeline infrastructure with serialization. GPU shader: 16-tap Poisson disc blur weighted by Circle of Confusion, depth linearization with camera near/far planes
- ~~**Cel Shading / Toon Rendering**~~ ✅ — Diffuse band quantization (2-8), specular cutoff, colored shadow tints (5 modes), rim lighting, per-material light ramp override, post-process Sobel outlines (depth+normal weighted), **inverted-hull geometry outlines** (separate backface-extrusion pipeline, per-material `outlineWidth`/`outlineColor` override, skinned mesh support), full editor UI + scene serialization
- ~~**Full-Screen Stippling & Dither**~~ ✅ — Post-process stipple/dither effect with 8 combinable patterns via bitmask (Bayer 4x4/8x8, Blue Noise, Halftone, Crosshatch, Overlook, Ordered 2x2, Floyd-Steinberg — any combination, thresholds averaged), 3 color modes (Monochrome, Duo-Tone, Full Color), configurable scale/density/strength, foreground/background color pickers, full editor UI in Settings > Scene > Post Processing with checkbox grid, scene render settings serialization

### Artistic Surface Materials ✅ COMPLETE

Lightweight artist controls for metal, glass, and rim-light effects reusing existing push constant slots. Reflectivity (fake environment reflection via `skyReflectColor`), Fresnel Power (edge vs center falloff), Rim Light (additive edge glow). 4 presets: Metal, Glass, Rim Glow, Clear. Zero new descriptor bindings.

---

## 2D Sprite Art Pipeline ✅ COMPLETE

A complete draw-to-game workflow for 2D and 2.5D projects, from pixel art creation through sprite sheets to playable prefabs.

### Built-In Pixel Editor ✅

Minimal but functional sprite editor inside the engine — no external tools needed for prototyping.

- **Canvas sizes:** Freeform + retro resolution presets (9 presets: Game Boy, NES, SNES, GBA, Genesis, PC Engine, EarthBound, DS, Custom)
- **Tools (8):** Pencil, eraser, fill (flood fill BFS), line (Bresenham), rectangle, ellipse (midpoint), eyedropper, selection
- **Layers:** Layer stack with add/remove, visibility toggle, opacity
- **Palette:** 16-color default palette with color picker, custom palette support
- **Animation:** Onion skinning (before/after tint, configurable frames/opacity), frame timeline with thumbnails, FPS playback preview
- **Export:** Save as `.png` sheet or individual frames, auto-register as engine asset
- **Undo/Redo:** 50-entry stack for all canvas operations
- **Files:** `PixelEditor.h/cpp`, EditorPanel `1<<18`

### Retro Resolution Presets

| Preset | Tile | Sprite | Portrait | System |
|--------|------|--------|----------|--------|
| Game Boy | 8×8 | 8×8, 8×16 | — | 160×144 |
| NES | 8×8 | 8×16 | — | 256×240 |
| SNES | 8×8, 16×16 | 16×16, 32×32 | — | 256×224 |
| Genesis | 8×8 | 8×16, 16×16 | — | 320×224 |
| GBA | 8×8 | 16×16, 32×32, 64×64 | — | 240×160 |
| PC Engine | 8×8 | 16×16, 32×64 | 256×512 | 256×224 |
| EarthBound-style | 8×8 | 16×24, 32×48 | 128×128 | 256×224 |
| DS | 8×8 | 16×16, 32×32 | 128×128 | 256×192 |
| Custom | User-defined | User-defined | User-defined | User-defined |

### Sprite Sheet / Atlas Workflow ✅

- **Import:** Load existing sprite sheets via `SpriteSheetImporter` (stb_image)
- **Grid slicing:** Uniform cell width/height with configurable padding
- **Auto-detect slicing:** BFS flood fill for connected non-transparent regions, noise filtering (4x4 min), position-sorted output
- **Atlas packing:** Runtime `SpriteTextureAtlas` shelf-packs sprites <=512px into 4096x4096 GPU texture
- **UI:** EditorPanel `1<<17` with grid/auto-detect toggle, 48px slice thumbnails, apply-to-entity button
- **Files:** `SpriteSheetImporter.h/cpp`

### Auto Collider Generation ✅

- **Alpha-based bounds:** `FindOpaqueBounds()` scans alpha channel against configurable threshold
- **Bounding shapes (3):** Auto-fit box, capsule, sphere colliders from sprite bounds via `SpriteColliderGenerator`
- **World-space conversion:** Pixel bounds converted relative to sprite pivot and size
- **Files:** `SpriteColliderGenerator.h/cpp`

### Vector Art / SVG Import ✅

- **SVG import:** Parse SVG via nanosvg (`third_party/nanosvg/`), rasterize to RGBA at configurable scale
- **Size capping:** Max 4096x4096, auto-scales down if exceeded
- **Engine integration:** `GetOrLoadTexture()` routes `.svg` files through `SVGLoader::LoadAsTexture()`
- **Files:** `SVGLoader.h/cpp`

### Prefab Output Pipeline ✅

Complete flow from art to playable entity:

1. **Draw** sprite in pixel editor (or import PNG/SVG)
2. **Slice** into frames if sprite sheet
3. **Define** animation sequences with frame timeline
4. **Generate** colliders (box/capsule/sphere from alpha bounds)
5. **Export as prefab** — `ExportAsPrefab()` writes horizontal sprite sheet PNG + `.enjprefab` with Transform, Sprite2D, AnimatedSprite2D (if multi-frame), BoxCollider components
6. **Drag into scene** — instantiate as ready-to-play entity

### Per-Frame Colliders ✅

- **PerFrameColliderComponent:** Vector of `FrameCollider` (offset, size, enabled) indexed by animation frame
- **Auto-apply:** On frame change in RenderSystem, updates BoxCollider center/size from current frame data
- **Inspector:** Match-to-animation button, per-frame offset/size editing

### Polygon Collider 2D ✅

- **PolygonCollider2DComponent:** Arbitrary polygon vertices (CCW winding, local space) with physics material + collision filtering
- **Silhouette tracing:** `SpriteContourTracer` (marching squares + Douglas-Peucker simplification) generates polygon from sprite alpha
- **SpriteColliderGenerator::FitPolygonCollider():** Traces contour, simplifies, converts to world-space relative to pivot
- **Inspector:** Vertex list editing, "Trace Silhouette" auto-generate button, Fit Polygon button in Sprite2D
- **Files:** `SpriteContourTracer.h/cpp`, `SpriteColliderGenerator.h/cpp` (extended)

### Polygon Vertex Editor ✅

- **PixelEditor PolygonEdit tool:** Click to add vertices, click first vertex to close, drag to move, right-click to delete
- **Visual overlay:** Colored dots, edge lines, translucent convex fill over sprite canvas
- **Integration:** 9th tool in toolbar, generates collider from pixel-space vertices

### SDF Vector Rendering ✅

- **SDFGenerator:** 8SSEDT (two-pass sweep) signed distance field from alpha images
- **SVGLoader::LoadAsSDF():** Rasterize SVG then generate SDF for resolution-independent rendering
- **UIWidgetData fields:** `sdfMode`, `sdfSoftness`, `sdfOutlineWidth`, `sdfOutlineColor`
- **Files:** `SDFGenerator.h/cpp`

### Normal Map Generation ✅

- **NormalMapGenerator:** Sobel operator on height/grayscale images → RGB normal map (flat = 128,128,255)
- **GenerateAndSave():** Load height PNG via stb_image, generate, save normal PNG via stb_image_write
- **Inspector integration:** "Generate Normal Map" button in Sprite2D inspector, auto-assigns to normalMapPath
- **Options:** Strength multiplier, Y-flip for DirectX/OpenGL convention
- **Files:** `NormalMapGenerator.h/cpp`

---

## Procedural Generation ✅ COMPLETE

- ~~**Procedural Generation Algorithms**~~ ✅ — 9 standalone algorithm classes in ProceduralAlgorithms.h/cpp:
  - CellularAutomata (cave generation, Moore neighborhood birth/death rules)
  - RandomWalker (dungeon carving, directional bias, turn chance)
  - BSPGenerator (Binary Space Partition room-corridor dungeons, recursive splitting)
  - DiamondSquare (heightmap terrain, midpoint displacement, roughness control) + FractalTerrain (fBm octave stacking, ridged multifractal, hydraulic erosion via droplet simulation, thermal erosion via talus angle)
  - LSystemGenerator (string rewriting with turtle graphics interpretation, F/+/-/[/] commands; advanced 3D turtle with yaw/pitch/roll, stochastic rules, branch radius decay)
  - WaveFunctionCollapse (tile-based with adjacency constraints, entropy-based collapse, backtracking)
  - VoronoiGenerator (Euclidean/Manhattan/Chebyshev distance, region ID grid)
  - GrammarGenerator (shape grammars for buildings, weighted rule selection)
  - PrefabAssembler (snap-together rooms with directional connection points)
  - Editor panel (EditorPanel `1<<24`): algorithm dropdown, parameter sliders, seed input, 256x256 ImGui canvas preview, "Apply to Tilemap" button
- ~~**Custom Flora Assets**~~ ✅ — `customAssetPath` field on GrassVolumeComponent and ShrubVolumeComponent (TreeVolumeComponent already had texture paths). Browse/clear buttons in inspector. Serialization support.

---

## Scripting & Extensibility ✅ COMPLETE

- ~~**Component/Plugin DLL Repositories**~~ ✅ RESOLVED — Plugin repository system with catalog browsing, search/filter by category, install/uninstall, version comparison, `repository.json` format, persistent source management. Extended `PluginManifest` with author/category/tags fields. Editor Plugin Browser panel. Enhanced with `PluginContext` (World, RenderSystem, ScriptEngine, SimpleAudio, SceneManager), `PluginSDK.h` single-header with `ENJIN_IMPLEMENT_PLUGIN()` macro, `OnSaveState/OnRestoreState` for hot-reload state preservation, 4 AS bindings (`Plugin_IsLoaded/GetVersion/Load/Unload`), 3 VS nodes, example plugin in `examples/ExamplePlugin/`.
- ~~**Documentation Generator**~~ ✅ RESOLVED — `DocGenerator` auto-generates markdown from component headers (field parser), AngelScript bindings (via `asIScriptEngine` enumeration), visual script nodes (from `NodeRegistry`), and data asset schemas. Outputs COMPONENTS.md, SCRIPTING_API.md, VISUAL_SCRIPT_NODES.md, DATA_ASSETS.md, INDEX.md to `docs/generated/`. Accessible via Tools menu.
- ~~**ScriptableObject / DataAsset System**~~ ✅ RESOLVED — `DataAssetRegistry` singleton with schemas (`.enjschema`) and assets (`.enjdata`) JSON I/O. 8 field types (String, Float, Int, Bool, Vector3, Vector4, StringArray, FloatArray). AngelScript bindings (`DataAsset_Load/GetFloat/GetInt/GetBool/GetString/GetVector3`). 3 visual script nodes (`DataAsset_Load`, `DataAsset_GetFloat`, `DataAsset_GetString`). Editor Data Asset panel with schema editor, asset browser, inline field editing.
- ~~**MIDI Input**~~ ✅ RESOLVED — Platform-specific MIDI input (WinMM on Windows, stubs on other platforms). `MIDIInput` class with device enumeration, open/close, per-frame double-buffered event polling, persistent CC state. 12 AngelScript bindings (`MIDI_GetDeviceCount/GetDeviceName/OpenDevice/CloseDevice/IsDeviceOpen/IsNoteOn/IsNoteOff/GetNoteVelocity/GetCC/GetCCValue/GetEventCount`). Wired in PlayMode and Player. Files: `MIDIInput.h/cpp`, `ScriptBindings_MIDI.cpp`.

---

## Platform & Export

### Desktop Platforms

#### Linux ✅ Done
- **Render backend:** Vulkan (native, no translation layer)
- **Window/input:** GLFW already cross-platform — validate Wayland + X11 paths
- **Audio:** miniaudio already supports ALSA/PulseAudio/PipeWire — verify and test
- **Build system:** CMake + GCC/Clang, produce `.AppImage` or Flatpak via BuildPipeline
- **File I/O:** POSIX path normalization (forward slashes), XDG directory conventions for saves/config
- **Packaging:** AppImage bundle (self-contained), Flatpak manifest, optional .deb/.rpm
- **Dev API stubs:** `PlatformLinux.h` — `LinuxWindow`, `LinuxFileBrowser`, `LinuxNotification`, XDG desktop integration

#### macOS ✦ Planned
- **Render backend:** MoltenVK (Vulkan-over-Metal translation)
- **Window/input:** GLFW already supports macOS — validate Retina scaling, trackpad gestures
- **Audio:** miniaudio CoreAudio backend — verify and test
- **Build system:** CMake + AppleClang, produce `.app` bundle via BuildPipeline
- **Code signing:** Notarization pipeline (codesign + staple) for Gatekeeper
- **Platform abstractions:** `PlatformMacOS.h` — `MacOSWindow`, `MacOSFileBrowser`, Retina DPI scaling, Dark Mode detection, menu bar integration
- **Considerations:** Apple Silicon (ARM64) + Intel (x86_64) universal binary, Metal shader cross-compilation validation

#### Steam Deck ✅ Done
- **Render backend:** Vulkan (native — AMD RDNA2 GPU, same as desktop Linux)
- **Input:** Steam Input API integration for trackpad, gyroscope, back paddles, haptic feedback. Fallback to standard gamepad via SDL/GLFW
- **Display:** 1280x800 (Deck LCD) / 1280x800 HDR (Deck OLED) target resolution, 40-60fps adaptive framerate
- **Performance profiles:** Configurable TDP-aware quality presets (Low/Medium for 15W, High for 25W)
- **Steamworks integration:** Steam overlay, cloud saves, achievements, Rich Presence, workshop
- **Dev API stubs:** `PlatformSteamDeck.h` — `SteamInput`, `SteamCloud`, `SteamOverlay`, gyro aim assist, on-screen keyboard, suspend/resume lifecycle
- **Testing:** Proton compatibility layer validation (for Windows builds running on Deck), native Linux build preferred
- **Build target:** Linux x86_64 binary + Steam app manifest, verified Deck compatibility badge requirements

### Console Platforms

#### Nintendo Switch (Original) ✅ Stub Done (requires licensed devkit for full impl)
- **Render backend:** NVN (Nintendo's proprietary Vulkan-like API) or Vulkan 1.1 subset via nnsdk
- **Platform SDK:** Nintendo SDK (NintendoSDK / nnsdk), requires approved developer agreement
- **Input:** Joy-Con (accelerometer, gyro, IR, HD rumble), Pro Controller, touch screen, motion controls
- **Display:** 1280x720 docked / 720x480 handheld, 30-60fps adaptive
- **Memory:** 4GB RAM — aggressive memory budgeting, texture streaming, LOD bias
- **Audio:** nn::audio (AAC hardware decode, 6-channel mixing)
- **Dev API stubs:** `PlatformSwitch.h` — `SwitchInput` (Joy-Con/Pro), `SwitchAccount` (user profiles), `SwitchSave` (save data mount), `SwitchPerformance` (CPU/GPU clock modes), docked/handheld detection, sleep/wake lifecycle
- **Certification:** Nintendo Lotcheck requirements checklist (controller disconnect handling, error screens, HOME button, sleep mode, user account)
- **Build target:** ARM64 (Cortex-A57) binary via Nintendo toolchain, .nsp/.nca packaging

#### Nintendo Switch 2 ✦ Planned (requires licensed devkit)
- **Render backend:** Vulkan 1.3 (expected NVIDIA Ampere-class GPU), potential DLSS/FSR upscaling
- **Platform SDK:** NintendoSDK next-gen (TBD — API stubs prepared based on Switch 1 patterns)
- **Input:** Joy-Con 2 (magnetic attachment, mouse-like optical sensor, C-button expected), Pro Controller 2, backward compatibility with Switch 1 controllers
- **Display:** 1920x1080 docked / 720p+ handheld (expected), HDR support (expected)
- **Memory:** 8-12GB RAM (expected) — more generous budgets than Switch 1
- **Dev API stubs:** `PlatformSwitch2.h` — extends `PlatformSwitch.h` with `Switch2Input` (optical sensor, C-button), `Switch2Display` (HDR, higher res), `Switch2Performance` (DLSS hooks, ray tracing stubs), backward-compat mode flag
- **Certification:** Nintendo Lotcheck 2 requirements (TBD — prepare based on Switch 1 + expected additions)
- **Build target:** ARM64 binary via Nintendo toolchain, next-gen packaging format

#### PlayStation 5 ✦ Planned (requires licensed devkit)
- **Render backend:** AGC (PlayStation proprietary low-level graphics API) or Vulkan subset via PSDK
- **Platform SDK:** PlayStation Partners SDK (PSDK), requires SIE partnership approval
- **Input:** DualSense — adaptive triggers (resistance curves), haptic feedback (HD rumble per-actuator), touchpad, motion sensor, microphone, speaker. PS VR2 passthrough
- **Display:** 4K (3840x2160) primary, 1080p performance mode, 120Hz support, HDR10/Dolby Vision
- **Audio:** Tempest 3D AudioTech (object-based spatial audio, HRTF), custom audio pipeline integration
- **Storage:** SSD streaming — custom I/O decompression (Kraken/Oodle), no loading screens design pattern
- **Dev API stubs:** `PlatformPS5.h` — `PS5Input` (DualSense adaptive triggers, haptics), `PS5Trophy` (achievements), `PS5Activity` (Activity Cards, Game Help), `PS5Save` (save data), `PS5Network` (PSN, multiplayer), `PS5SSD` (direct storage streaming), `PS5HDR` (HDR tone mapping)
- **Certification:** Sony TRC (Technical Requirements Checklist) — error handling, graceful degradation, PS button behavior, suspend/resume, network disconnection
- **Build target:** x86_64 (AMD Zen 2) binary via PSDK toolchain, .pkg packaging

#### Xbox Series X|S ✦ Planned (requires ID@Xbox or licensed devkit)
- **Render backend:** Direct3D 12 (or Vulkan via D3D12 interop layer — preferred: native D3D12 backend)
- **Platform SDK:** GDK (Game Development Kit), available via ID@Xbox program or Microsoft partnership
- **Input:** Xbox Wireless Controller (impulse triggers, share button), adaptive controller support
- **Display:** 4K Series X / 1440p Series S, 120Hz, HDR10, Dolby Vision, Auto HDR
- **Audio:** Dolby Atmos, Windows Sonic spatial audio
- **Storage:** Xbox Velocity Architecture — DirectStorage API, Sampler Feedback Streaming, BCPack texture compression
- **Smart Delivery:** Single package with Series X (high quality) and Series S (reduced resolution/LOD) asset tiers
- **Dev API stubs:** `PlatformXbox.h` — `XboxInput` (impulse triggers, share), `XboxLive` (achievements, leaderboards, cloud saves, multiplayer), `XboxGDK` (activity feed, Game Pass integration), `XboxDirectStorage` (SSD streaming), `XboxHDR` (HDR metadata)
- **Certification:** Microsoft XR (Xbox Requirements) — controller disconnection, network loss, Quick Resume save/restore, accessibility requirements (narrator, high contrast)
- **Build target:** x86_64 (AMD Zen 2) binary via GDK toolchain, MSIXVC packaging

### Platform Abstraction Layer

All platform targets share a common abstraction:

```cpp
// Engine/include/Enjin/Platform/PlatformTarget.h
class PlatformTarget {
public:
    virtual ~PlatformTarget() = default;

    // Lifecycle
    virtual bool Initialize() = 0;
    virtual void Shutdown() = 0;
    virtual void PollEvents() = 0;

    // Input
    virtual InputDeviceCapabilities GetInputCapabilities() = 0;
    virtual bool HasGyroscope() = 0;
    virtual bool HasTouchscreen() = 0;
    virtual bool HasHaptics() = 0;

    // Display
    virtual DisplayInfo GetDisplayInfo() = 0;  // resolution, refresh, HDR
    virtual bool SupportsHDR() = 0;

    // Storage
    virtual std::string GetSaveDirectory() = 0;
    virtual std::string GetCacheDirectory() = 0;
    virtual bool HasFastStorage() = 0;  // SSD direct streaming

    // Platform services
    virtual bool HasAchievements() = 0;
    virtual bool HasCloudSaves() = 0;
    virtual bool HasOverlay() = 0;

    // Power/thermal
    virtual PowerProfile GetPowerProfile() = 0;  // Desktop/Plugged/Battery/TDP-limited
    virtual float GetThermalHeadroom() = 0;       // 0.0-1.0
};
```

### Implementation Priority

| Platform | Priority | Rationale |
|----------|----------|-----------|
| Linux | P1 | ✅ Done — LinuxPlatform.h, XDG paths, AppImageBuilder, CMake Linux targets |
| Steam Deck | P1 | ✅ Done — SteamDeck.h, SteamInput.h, AdaptiveQuality.h, gyro/suspend stubs |
| macOS | P2 | MoltenVK translation layer, Apple Silicon support needed, code signing complexity |
| Xbox Series X\|S | P2 | GDK freely available via ID@Xbox, D3D12 backend required |
| PlayStation 5 | P3 | Requires SIE partnership, proprietary AGC graphics API, high cert bar |
| Nintendo Switch 2 | P3 | NDA SDK, Vulkan 1.3 expected — closer to desktop pipeline |
| Nintendo Switch 1 | P4 | ✅ Stub — NVNBackend.h, SwitchPlatform.h (requires licensed devkit for full impl) |

### Shared Systems of Support

- **Cross-platform build matrix** — CMake presets per platform, CI/CD build agents (GitHub Actions for desktop, self-hosted for console devkits)
- **Platform capability query** — Runtime `PlatformTarget::Has*()` checks drive adaptive quality, input schemes, and feature availability
- **Adaptive quality system** — Auto-select render resolution, shadow quality, particle count, LOD bias based on platform power profile and thermal headroom
- **Input abstraction** — Unified `InputAction` system already exists; extend with platform-specific device capabilities (gyro, adaptive triggers, touchpad)
- **Save data abstraction** — Platform-agnostic save API wrapping platform-specific storage (PSN save data, Xbox cloud saves, Steam cloud, local filesystem)
- **Achievement/trophy abstraction** — Common API mapping to PSN trophies, Xbox achievements, Steam achievements
- **Certification test suite** — Automated test runner covering common cert requirements: controller disconnect/reconnect, network loss recovery, suspend/resume, error dialogs, accessibility checks

### Mobile Platforms (Lower Priority)

- **Android** — Vulkan (native), touch input, gyroscope, screen density, mobile render paths, Google Play services
- **iOS** — MoltenVK, touch input, Game Center, App Store guidelines, notch/safe area handling

### Emerging Platforms (Future)

- **VR/XR Support** — OpenXR integration, stereo rendering, hand tracking, spatial input, roomscale
- ~~**WebAssembly Export**~~ ✅ — WebGPU renderer backend, WASM via Emscripten, canvas window, browser input/audio, one-click web export

---

## Ray Tracing & Path Tracing ✅ COMPLETE

The full Vulkan ray tracing pipeline is implemented and wired end-to-end. `CompositeRTResults()` is called after denoising (SVGF, OIDN, or OptiX), the real depth buffer is bound to RT descriptor binding 2, and camera change detection resets path tracer accumulation. Effects include shadows, reflections, AO, GI, translucency (refraction/SSS), and caustics (photon-traced). Three denoiser options: SVGF (compute), OIDN (Intel neural), OptiX (NVIDIA CUDA). All 25 RT shaders are compiled and embedded in `RTShaderData.h`. The RT pipeline activates automatically on supported hardware.

### Architecture

Hybrid rendering pipeline: rasterization for primary visibility (existing Vulkan pipeline unchanged) with optional ray/path tracing for lighting effects. Vulkan Ray Tracing extensions (`VK_KHR_ray_tracing_pipeline`, `VK_KHR_acceleration_structure`, `VK_KHR_deferred_host_operations`, `VK_KHR_buffer_device_address`).

### Implementation Status

| Feature | Technique | Status |
|---------|-----------|--------|
| **RT Capabilities** | Extension detection, properties query, graceful fallback | ✅ Complete |
| **Acceleration Structures** | BLAS per unique mesh (hash dedup), TLAS rebuilt per frame | ✅ Complete |
| **RT Pipeline** | vkCreateRayTracingPipelinesKHR, SBT construction | ✅ Complete |
| **RT Shadows** | 1 SPP ray traced shadows, configurable distance/radius | ✅ Complete |
| **RT Reflections** | Single-bounce specular reflections with roughness threshold | ✅ Complete |
| **RT Ambient Occlusion** | Short-range AO hemisphere sampling | ✅ Complete |
| **RT Global Illumination** | Multi-bounce diffuse GI with configurable bounces/intensity | ✅ Complete |
| **Path Tracing Mode** | Progressive accumulation, camera-reset, SPP tracking | ✅ Complete |
| **SVGF Denoiser** | 3-pass compute (temporal, variance, a-trous wavelet) | ✅ Complete |
| **RT Compositor** | Fullscreen compute composite of RT layers into scene HDR | ✅ Complete |
| **Editor Panel** | Per-effect toggles, config sliders, path tracer progress, stats | ✅ Complete |
| **Scene Settings** | 24 RT config fields with full JSON serialization | ✅ Complete |
| **Caustics** | Photon-traced caustic patterns from refractive surfaces | ✅ Complete (RTCaustics class, binding 15, 3 shaders) |
| **RT Translucency** | Refraction rays (Snell's law, Fresnel, SSS approximation) | ✅ Complete (RTTranslucency class, binding 14, 3 shaders) |
| **OIDN integration** | Intel Open Image Denoise for cross-platform neural denoising | ✅ Complete (OIDNDenoiser.h/cpp, CMake ENJIN_RAYTRACING_OIDN, editor denoiser type selector) |

### Files

**Headers** (`Engine/include/Enjin/Renderer/RayTracing/`):
- `RTCapabilities.h` — Feature detection struct + `Query()` + `GetRequiredExtensions()`
- `AccelerationStructure.h` — BLAS/TLAS low-level wrappers with vkGetDeviceProcAddr function pointers
- `AccelerationStructureManager.h` — BLAS cache by mesh hash, per-frame TLAS rebuild, instance management
- `RTPipeline.h` — RT pipeline wrapper (SBT regions, shader groups)
- `RTShadows.h` — Shadow ray dispatch + config (SPP, distance, radius)
- `RTReflections.h` — Specular reflection dispatch + config (SPP, distance, roughness threshold)
- `RTAmbientOcclusion.h` — AO hemisphere sampling dispatch + config (radius, power)
- `RTGlobalIllumination.h` — Multi-bounce diffuse GI dispatch + config (bounces, intensity)
- `PathTracer.h` — Progressive path tracer with accumulation buffer
- `IDenoiser.h` — Abstract denoiser interface
- `SVGFDenoiser.h` — 3-pass SVGF compute denoiser
- `RTCompositor.h` — Compute shader to composite RT layers into scene HDR (enable bits 0-5: shadows/reflections/AO/GI/translucency/caustics)
- `RTTranslucency.h` — Refraction ray dispatch + config (Snell's law, Fresnel, SSS approximation)
- `RTCaustics.h` — Photon-traced caustic dispatch + config
- `OptiXDenoiser.h` — NVIDIA OptiX AI Denoiser (3 modes: LDR/HDR/Temporal), compile-guarded ENJIN_RAYTRACING_OPTIX
- `RTShaderData.h` — Embedded compiled SPIR-V for all 25 RT + compute shaders

**Sources** (`Engine/src/Renderer/RayTracing/`):
- Matching `.cpp` files for all headers above (11 source files)

**Shaders** (`Engine/shaders/`):
- `rt_common.glsl` — Shared structs, RNG, hemisphere sampling
- `rt_shadow.rgen/.rmiss/.rchit` — Shadow rays toward lights
- `rt_reflect.rgen/.rmiss/.rchit` — Specular reflection rays
- `rt_ao.rgen/.rmiss/.rchit` — Cosine-weighted AO hemisphere
- `rt_gi.rgen/.rmiss/.rchit` — Multi-bounce diffuse GI
- `rt_pathtrace.rgen/.rmiss/.rchit` — Full progressive path tracer
- `svgf_temporal.comp`, `svgf_variance.comp`, `svgf_atrous.comp` — SVGF denoiser passes
- `rt_translucency.rgen/.rmiss/.rchit` — Refraction rays with Snell's law and SSS
- `rt_caustics.rgen/.rmiss/.rchit` — Photon-traced caustic patterns
- `rt_composite.comp` — Composite RT layers into scene HDR

### Render Frame Flow (with RT)

```
BeginFrame()
  │
  FlushPendingBLASBuilds()        ← batch BLAS builds for new meshes
  │
  RenderShadowPass()              ← raster shadows (skipped if RT shadows on)
  │
  RebuildTLAS()                   ← update TLAS from entity transforms
  DispatchRTEffects()             ← trace shadow/reflect/AO/GI/translucency/caustics rays
  DenoiseRTOutputs()              ← SVGF / OIDN / OptiX denoise
  │
  BeginMainRenderPass()
    RenderEntities()              ← existing rasterization
    RenderSprites/Effects()
  EndMainRenderPass()
  │
  CompositeRTResults()            ← compute: multiply shadows, add reflections,
  │                                  multiply AO, add GI, blend translucency,
  │                                  add caustics into scene HDR
  PostProcessing::Apply()         ← existing tonemapping/bloom/FXAA
  │
  EndFrame()
```

Only runs for `SceneRenderMode::Scene3D`. 2D/2.5D scenes skip the RT pipeline entirely.

### RT Descriptor Set Layout (Set 1)

| Binding | Type | Purpose |
|---------|------|---------|
| 0 | ACCELERATION_STRUCTURE | TLAS |
| 1 | STORAGE_IMAGE | Scene HDR (read-write for composite) |
| 2 | COMBINED_IMAGE_SAMPLER | Depth buffer |
| 3 | COMBINED_IMAGE_SAMPLER | World normals |
| 4 | COMBINED_IMAGE_SAMPLER | Motion vectors |
| 5 | STORAGE_IMAGE | RT Shadow output (R16F) |
| 6 | STORAGE_IMAGE | RT Reflection output (RGBA16F) |
| 7 | STORAGE_IMAGE | RT AO output (R16F) |
| 8 | STORAGE_IMAGE | RT GI output (RGBA16F) |
| 9 | STORAGE_BUFFER | Material data |
| 10 | STORAGE_BUFFER | Vertex data |
| 11 | STORAGE_BUFFER | Index data |
| 12 | STORAGE_BUFFER | Per-instance transforms |
| 13 | UNIFORM_BUFFER | Light data |
| 14 | STORAGE_IMAGE | RT Translucency output (RGBA16F) |
| 15 | STORAGE_IMAGE | RT Caustics output (RGBA16F) |

### Graceful Fallback

- `RTCapabilities::Query()` checks extension support at physical device selection
- If unsupported: all RT unique_ptrs remain null, editor shows "Not Supported" badge
- All 25 RT shaders are compiled SPIR-V; pipeline creates automatically on supported hardware
- All RT code paths guarded by `if (m_RTEnabled && m_ASManager)` checks
- Raster shadows/SSAO remain the fallback path

### Hardware Requirements

- **RT hardware path:** Vulkan RT extensions (NVIDIA RTX 20xx+, AMD RX 6000+, Intel Arc)
- **Denoising:** SVGF on any Vulkan GPU with compute shaders; OIDN (Intel, cross-platform); OptiX (NVIDIA CUDA)
- **Minimum for real-time RT:** Target 1080p @ 30fps with 1 SPP + SVGF on RTX 3060-class hardware

### Remaining Work

1. ~~**Compile RT shaders**~~ ✅ — All 25 GLSL shaders compiled to SPIR-V and embedded in `RTShaderData.h`
2. ~~**Embed SPIR-V**~~ ✅ — Real compiled bytecode (384–10,964 bytes each) replaces placeholder stubs
3. ~~**Wire composition + denoising**~~ ✅ — `CompositeRTResults()` wired after denoising, real depth buffer on binding 2, `DenoiseRTOutputs()` replaced with real SVGF calls, camera change detection for path tracer reset
4. ~~**OIDN integration**~~ ✅ — Intel Open Image Denoise as alternative cross-platform neural denoiser. `OIDNDenoiser.h/cpp`, `ENJIN_RAYTRACING_OIDN` CMake option, editor denoiser type selector. `RegisterImageMapping()` wired for all RT effect outputs + dummy image in `InitializeRayTracing()`
5. ~~**OptiX integration**~~ ✅ — NVIDIA OptiX AI Denoiser (CUDA-based GPU denoising, 3 modes: LDR/HDR/Temporal). `OptiXDenoiser.h/cpp`, `ENJIN_RAYTRACING_OPTIX` CMake option

---

## Rendering Pipeline Roadmap — Sparse Reconstruction-First

The rendering pipeline is moving toward **sparse sampling + smart reconstruction**: render fewer true samples, choose them intelligently, reconstruct with temporal methods, denoisers, and neural models. This roadmap prioritizes importance-driven ray selection, temporal reuse, and radiance caching over brute-force path tracing.

### Phase 1: Motion Vectors + TAA ✅ COMPLETE (2026-03-11)

Foundation for all temporal techniques — upscalers, RT temporal reuse, ReSTIR temporal.

- ~~Per-pixel velocity buffer (RG16F, swapchain MRT)~~ ✅
- ~~TAA jitter injection (Halton 2,3)~~ ✅
- ~~TAA resolve compute shader (neighborhood clamping, velocity reprojection)~~ ✅
- ~~History ping-pong buffers with reset on camera cut/scene change~~ ✅
- ~~Editor UI: AA dropdown (None/FXAA/TAA/SMAA), TAA sharpness/jitter/feedback sliders~~ ✅
- ~~SceneRenderSettings: aaMode, taaSharpness, taaJitterScale, taaFeedbackMin/Max~~ ✅

### Phase 2: Finish Ray Tracing ✅ COMPLETE (2026-03-11)

- ~~Material SSBO for RT hit shaders (binding 9)~~ ✅
- ~~Wire OptiX denoiser CUDA interop~~ ✅
- ~~Recompile RT shaders → update RTShaderData.h~~ ✅

Remaining RT work (vertex/index SSBOs, full material-correct .rchit shaders) deferred — functional but not material-accurate.

### Phase 3: Path Tracer Polish ✅ DONE

- ✅ **Next Event Estimation (NEE)** — direct light sampling at each bounce for faster convergence. Uniform random light selection across directional, point, and spot lights via packed NEE light SSBO (binding 16). Shadow rays for visibility testing.
- ✅ **Multiple Importance Sampling (MIS)** — combine BRDF sampling + light sampling PDFs, power heuristic. Combined PDF uses metallic/roughness-adaptive specular probability. MIS weight applied to NEE contributions.
- ✅ **Russian Roulette** — probabilistic bounce termination based on throughput luminance (configurable min bounce depth, default 3, and min survival probability, default 0.05). Throughput divided by survival probability to remain unbiased.
- ✅ **Firefly clamping** — clamp outlier per-bounce throughput and final accumulated radiance (configurable clamp value, default 10.0)
- ✅ **Accumulation reset** — detect camera/scene changes, reset SPP counter
- ✅ **Progressive UI** — show current SPP count, convergence indicator, target SPP setting
- ✅ **Cook-Torrance BRDF** — Full GGX microfacet model with Fresnel-Schlick, Smith-Schlick geometry, GGX importance sampling, cosine-weighted hemisphere diffuse sampling, combined mixture PDF
- ✅ **Simplified materials** — Pre-baked CPU-side simplified materials (F0, kDiffuse, effectiveRoughness) for deep bounces (after bounce 1), reducing hit shader divergence. Skips SSS, transmission, caustics on deep paths.
- **Reference mode** — high-SPP offline render-to-file for ground truth comparison (planned)

### Phase 4: ReSTIR — Importance-Driven Light Selection

The single biggest efficiency win for the RT pipeline. Focus limited ray budget on the most important lights and paths per pixel.

- ~~**Reservoir-based light sampling** — implement ReSTIR DI (direct illumination)~~ FOUNDATION COMPLETE
  - ~~Per-pixel light reservoir (candidate, weight, M count)~~ ✅
  - ~~Initial candidates: random light sampling weighted by estimated contribution~~ ✅
  - Temporal reuse: carry reservoirs across frames using motion vectors (Phase 1)
  - Spatial reuse: share reservoirs with neighbors for faster convergence
- ~~**Light PDF estimation** — approximate light contribution without tracing~~ ✅
  - ~~Distance-based falloff, solid angle, emission intensity~~ ✅
  - Used for initial candidate generation and MIS weighting
- **Integration with existing RT effects**
  - RT Shadows: ReSTIR selects which lights to cast shadow rays toward
  - RT GI: ReSTIR DI for direct bounce, importance-weighted indirect
  - Path tracer: NEE (Phase 3) uses ReSTIR-selected lights
- **Adaptive ray budget** — per-pixel ray count based on variance/importance
  - High-variance regions (edges, specular, first-frame) get more rays
  - Stable temporal regions get fewer rays
  - Feed variance estimate to VRS (already have `VK_KHR_fragment_shading_rate`)
- ~~**Editor UI** — ReSTIR toggle~~ ✅, reservoir visualization debug mode, ray budget display

### Phase 5: Temporal RT Reuse (Foundation Complete)

Carry ray-traced results across frames, not just denoise per-frame. The denoisers (SVGF/OIDN/OptiX) already do temporal filtering, but this goes deeper — reusing actual ray results.

- **Temporal reprojection of RT outputs** ✅
  - Reproject previous frame's shadow/reflection/AO/GI using motion vectors
  - Confidence-weighted blend: high confidence = reuse, low = retrace (configurable 0-0.99)
  - Disocclusion detection via depth ratio + normal dot-product thresholds
  - Per-buffer enable toggles (shadows, AO, reflections, GI)
  - Compute shader: `rt_temporal_reuse.comp` (8x8 workgroups)
  - History ping-pong images (RGBA16F) + R8 confidence map output
  - Editor UI: TreeNode with blend/depth/normal sliders, per-buffer checkboxes, reset button
- **Temporal accumulation for GI**
  - Multi-frame GI accumulation (like path tracer but for hybrid mode) — covered by temporal reuse GI channel
  - Exponential moving average with configurable history length ✅
  - Reset on significant lighting/geometry changes
- **Denoiser-aware temporal pipeline** (pending)
  - Feed temporal reuse confidence to SVGF/OIDN as additional signal
  - Reduce denoiser aggressiveness where temporal data is reliable
  - Tighter feedback loop: reuse → denoise → display

### Phase 6: Radiance Caching

Cache and reuse indirect lighting instead of recomputing every frame. The most expensive part of GI is multi-bounce indirect — caching makes it tractable at real-time rates.

- **World-space irradiance cache**
  - Sparse 3D grid of cached irradiance probes (extends existing SH probe system)
  - Probe placement: adaptive based on geometry density and lighting complexity
  - Update rate: stagger probe updates across frames (not all probes every frame)
- **Screen-space radiance cache**
  - Cache indirect lighting results in screen space
  - Temporal reprojection for cache reuse
  - Invalidation on disocclusion or lighting change
- **Cache-guided ray allocation**
  - Regions with valid cache data → fewer rays needed
  - Regions with stale/missing cache → prioritize ray budget there
  - Feeds into adaptive ray budget from Phase 4
- **Integration with RT GI**
  - First bounce: ray traced (Phase 2)
  - Second+ bounces: read from radiance cache where available, trace where not
  - Progressive cache refinement over multiple frames

### Phase 7: DLSS / FSR / XeSS Upscaling — IN PROGRESS

Shared prerequisite: Phase 1 (TAA jitter + velocity buffer + depth access) ✅

- ~~**IUpscaler interface** — abstract base for swappable upscaler backends~~ ✅
  - `IUpscaler` base class with Initialize/Resize/Dispatch/Shutdown/ResetHistory
  - `UpscalerInput` struct: color, depth, velocity, jitter, delta time, sharpness, camera cut
  - `UpscalerQuality` enum: Performance (50%), Balanced (58%), Quality (67%), UltraQuality (77%)
  - `GetRenderResolution()` helper computes optimal render size for each quality preset
- ~~**FSR 2 (Built-in)** — Lanczos + CAS upscaler, always available (no SDK dependency)~~ ✅
  - Pipeline: render at lower resolution → TAA resolve at low-res → Lanczos-2 upsample → CAS sharpen
  - `upscale_lanczos.comp`: 4x4 tap Lanczos-2 separable filter compute shader
  - `upscale_cas.comp`: Contrast Adaptive Sharpening compute shader (edge-aware, adaptive intensity)
  - Jitter injection adjusted for render resolution when upscaler is active
  - Editor UI: upscaler type/quality/sharpness controls with real-time sync to RenderSystem
  - Scene serialization: upscalerType, upscalerQuality, upscalerSharpness saved/loaded
- **DLSS 3.5** (NVIDIA Streamline SDK) — NVIDIA GPUs only, **NEXT**
  - **DLSS Ray Reconstruction** — replaces RT denoisers with trained model (huge quality win at 1 SPP)
  - CMake flag: `ENJIN_UPSCALING_DLSS` (OFF by default)
- **XeSS** (Intel SDK) — all GPUs via DP4a, best on Intel Arc, **THIRD**
  - CMake flag: `ENJIN_UPSCALING_XESS` (OFF by default)
- **AdaptiveQuality integration** — hook upscaler render scale into existing quality level system

### Phase 8: Additional Antialiasing

- ~~**SMAA** — morphological edge-aware AA with edge walking (SMAA-lite in post-process)~~ ✅
- **MSAA runtime toggle** — pipeline recreation on sample count change (requires render pass modification)
- **AA comparison mode** — split-screen to compare methods

### Phase 9: GPU-Driven Work Scheduling

Move beyond CPU-organized draw submission. The GPU should manage more of the rendering workload itself.

- **Indirect draw from GPU culling output** ✅
  - GPU culling (HiZ + frustum) emits compacted indirect draw commands
  - `indirectEligible` flag on CullableObject controls which entities emit draw commands
  - `vkCmdDrawIndexedIndirectCount` for variable-length draw lists
  - Non-textured pool entities batched; textured entities remain per-entity
- **Multi-draw indirect batching** ✅
  - Non-textured: single `vkCmdDrawIndexedIndirectCount` replaces per-entity `vkCmdDrawIndexed`
  - Textured: `IndirectDrawBatcher` groups pool entities by texture set hash (CPU-side sort)
  - One `vkCmdDrawIndexedIndirect` per texture group with shared descriptor binding
  - Shaders select data source: ObjectData SSBO (indirect) vs push constants (per-entity)
  - Future: bindless textures to collapse all texture groups into a single draw
- **Device-generated commands** (when Vulkan spec stabilizes)
  - GPU-side pipeline/descriptor binding changes
  - Full GPU-driven rendering loop for dynamic scenes
- **Async compute overlap** ✅
  - `AsyncComputeScheduler`: dedicated compute queue with timeline semaphore support
  - RT effects + temporal reuse + denoising dispatched on compute queue
  - Overlaps with main geometry rasterization on graphics queue
  - Graphics queue waits on compute-finished semaphore via `AddExternalWaitSemaphore`
  - Graceful fallback to single-queue when dedicated compute unavailable
  - Configurable per work type: `AsyncComputeConfig` (RT, denoise, culling)

### Phase 10: Mobile Support (separate major effort)

#### 10a: Rendering Tier System
- Capability tiers: High (Vulkan 1.3, desktop), Medium (Vulkan 1.1, mobile), Low (GLES)
- Simplified shaders without RT, reduced post-processing
- ASTC/ETC2 texture compression

#### 10b: Android Port (FIRST)
- ANativeWindow, touch input, Gradle+CMake, TBDR-aware render path
- GPU quirk handling (Adreno, Mali, PowerVR)
- Thermal throttling

#### 10c: iOS Port (SECOND)
- MoltenVK integration
- Touch input + Safe Area, Xcode project

#### 10d: Mobile Asset Pipeline
- ASTC/ETC2, lower-res LODs, smaller `.enjpak`

### Phase 11: Advanced ReSTIR (Research-Informed, 2026)

Upgrade the existing ReSTIR DI foundation with latest techniques from NVIDIA RTXDI 2.0+ and academic research.

- **Pairwise MIS bias correction** — replace 1/M normalization in spatial reuse with pairwise MIS weights. Eliminates shadow-edge darkening without extra rays. (Eurographics 2024)
- **Permutation sampling** — decorrelate temporal reuse pattern to fix "boiling" artifacts. NVIDIA reports significant temporal stability improvement.
- **Visibility reuse** — 1-bit visibility flag in reservoir prevents occluded samples from propagating through spatial reuse
- **Confidence-weighted aging** — replace hard M_max cap with per-reservoir age/confidence tracking for graceful staleness decay
- **ReSTIR GI** — extend reservoirs to store GI samples (secondary hit position + radiance). Jacobian correction for solid angle changes. Packed format (~32 bytes) to keep memory manageable. (Ouyang et al., 2021)
- **ReGIR (world-space reservoirs)** — hash grid of pre-sampled light reservoirs for secondary bounces. 32K cells × 512 reservoirs. Resolution-independent, persistent across camera motion. (Boksansky et al., 2021)
- **ReSTIR PT (path reuse via GRIS)** — hybrid shift mapping (random replay through specular chain + reconnection at first diffuse vertex). Full multi-bounce specular + diffuse GI at 1 path/pixel. (Lin et al., SIGGRAPH 2022)

### Phase 12: Hash Grid Radiance Cache (Research-Informed, 2026)

Replace or supplement surfel spatial hash with multi-resolution hash grid (instant-NGP style). Most impactful single GI technique for the engine.

- **Multi-resolution hash grid** — 16 levels, 2^17 entries per level, 4 features per entry. ~32 MB total. Excellent spatial resolution (~1cm at finest level)
- **Trilinear interpolation** across 8 corners per level, concatenated across levels
- **Compute shader update** — trace rays from screen-space probes, scatter irradiance into hash grid via atomic EMA
- **Integration** — use as alternative backend for bindings 24-26 (surfel cache), or new bindings alongside
- **Radiance Cascades for 2D** — Alexander Sannikov's technique (Path of Exile 2). Multi-resolution angular + spatial cascade for beautiful 2D GI with emissive bounce. Target Scene2D/Scene2_5D modes.

### Phase 13: Real-Time Denoising & Upscaling SDK Integration (2026)

Production-grade denoising and upscaling beyond the built-in Lanczos+CAS path.

- **NRD integration** — NVIDIA Real-time Denoisers (open source, Vulkan-native, vendor-agnostic). ReBLUR + SIGMA shadow denoiser. Complement existing OIDN/OptiX with a real-time option
- **FSR 3.1 SDK** — FidelityFX open-source temporal upscaler (MIT license). Replace Lanczos+CAS with proper temporal accumulation. Frame generation via optical flow (decoupled from upscaling)
- **DLSS Streamline SDK** — DLSS Super Resolution + Ray Reconstruction for NVIDIA GPUs. CMake: `ENJIN_UPSCALING_DLSS`
- **XeSS SDK** — Intel upscaler with DP4a cross-vendor fallback. CMake: `ENJIN_UPSCALING_XESS`
- **Neural Radiance Cache (NRC)** — NVIDIA NRC library (Vulkan-native). 1-2ms overhead, dramatically accelerates path tracer GI convergence. Requires Turing+ Tensor Cores

### Phase 14: GPU-Driven Rendering & Modern Vulkan (2026)

Leverage Vulkan 1.4 and new extensions to eliminate CPU bottlenecks.

- **VK_EXT_shader_object** — eliminate pipeline permutations and compilation stutter. No more `RecreateEffectPipelinesForRenderPass()`. All 3 major vendors supported (2025)
- **VK_EXT_device_generated_commands** — GPU-side draw call emission from culling output. Eliminate CPU draw call bottleneck entirely. NVIDIA + AMD production drivers (2025)
- **VK_EXT_descriptor_buffer** — direct buffer-backed descriptors, massive CPU overhead reduction. Replace descriptor pool/set management. NVIDIA + AMD production (2023+)
- **VK_KHR_dynamic_rendering** (core 1.3+) — migrate from VkRenderPass/VkFramebuffer to dynamic rendering. Simplify render graph
- **GPU particle compute** — move ParticleSystem to compute shaders. Alive/dead list via atomics, indirect draw. 1M+ particles at 60fps. SDF collision via existing RT SDF binding 17

### Phase 15: Nanite-Style Virtual Geometry (2026-2027)

Meshlet-based continuous LOD with GPU-driven culling. Build on existing HiZ occlusion culling foundation.

- **Meshlet preprocessing** — integrate meshoptimizer `clusterlod.h` (MIT) for DAG construction. 64 vertices / 126 triangles per meshlet
- **Task + mesh shader pipeline** — `VK_EXT_mesh_shader` for per-meshlet frustum + cone + HiZ culling in task shader. Mesh shader outputs vertices directly
- **Screen-space error LOD selection** — DAG traversal with `screenError = worldError * screenHeight / (2 * distance * tan(fov/2))`. Group-based decisions prevent boundary cracks
- **Visibility buffer improvements** — add index buffer SSBO, PBR material resolve, material binning by type. Async compute for surface resolve
- **Cluster streaming** — root pages always resident, streaming pages loaded/evicted by priority. Fixed GPU memory pool with page-based management

### Phase 16: Advanced Shadows (2026)

Modern shadow techniques beyond cascaded shadow maps.

- **Progressive shadow caching** — split CSM into static (cached) + dynamic (per-frame) partitions. Static cascades only re-render on light/geometry change. (Pearl Abyss uses 6 cascades: 2 static + 1 terrain + 2 dynamic + 1 near-field)
- **Hybrid raster + RT shadows** — AMD FidelityFX Classifier tiles screen, only RT penumbra regions. 0.7-2ms total vs 2-4ms full-screen RT
- **Screen-space contact shadows** — depth buffer ray march (8-16 steps) for pixel-level contact detail. ~0.2ms
- **PCSS improvements** — Vogel disk sampling, decoupled penumbra estimation at quarter-res, bilateral upsample
- **Many-light shadow strategies** — ReSTIR-sampled shadow maps (top-K full shadow maps + imperfect shadow maps for rest). Bounded cost regardless of light count

### Phase 17: Procedural Generation & WFC (2027)

Tiny Glade-inspired procedural building system.

- **Module authoring pipeline** — mesh module format with typed connection sockets (label + orientation)
- **WFC/constraint solver** — graph-based constraint propagation with backtracking. Incremental local re-solve on edit
- **Mesh stitching** — weld boundary vertices, recompute normals/tangents. Triplanar UV generation
- **Procedural terrain** — extend existing ProceduralGen with SDF-based terrain sculpting

### Phase 18: Neural Rendering (2027+)

ML-assisted rendering techniques that run on consumer GPUs.

- **Neural texture compression (NTC)** — NVIDIA RTXNTC SDK. 96% VRAM reduction. Requires `VK_NV_cooperative_vector` for fast path. Fallback to standard BC textures on non-NVIDIA
- **Gaussian splatting renderer** — import format for photogrammetry assets. Reference: NVIDIA `vk_gaussian_splatting`. Specialized tool, not primary geometry path
- **NRD learned denoisers** — if NRD adds neural denoiser modes beyond algorithmic ReBLUR/ReLAX

### Phase 19: Tege Rendering Language (2027+)

Composable material expression library for aesthetics-first rendering. Each effect is a shader graph node artists can mix and match. See `docs/RENDERING_LANGUAGE.md` for full taxonomy.

- **Screen-Derived** — Heat Veil, Broken Reflection, Ghosted Transmission, Fiber Breakup, Contact Foam
- **Normal/Depth-Derived** — Edge Bloom, Thickness Tint, Staged Atmosphere, Depth Tint, Angle-Shift Color, Band Sheen, Fuzz Rim
- **Projected** — Projected Caustics, Surface-Wake Caustics, Crack-Light Ice, Shape-Driven Flame, Surface Drift
- **Event-Driven** — Pulsed Subsurface, Authored Light Script, Event Sparkle, Settling Haze
- **Quantized/Layered** — Dithered Volume, Plane-Fog Stack, Layered Water Body, Blue-Body Ice, Layered Ember Body
- **Material Taxonomy** — Surface-Bound, Transmissive, Participating Volume, View-Reactive, Event-Driven, Layer-Composed
- **Naming Convention** — `[Domain]_[Behavior]_[Mode]` (e.g., `Glass_Ghosted_Screen`, `Snow_Sparkle_Event`)

### Key Integration Notes

- **Velocity buffer** ✅ — shared dependency for TAA, upscalers, RT temporal, ReSTIR temporal reuse
- **RT descriptor binding 4** — motion vectors wired
- **AdaptiveQuality** — render scale (0.5x-1.0x), upscalers plug in naturally
- **VRS** — adaptive ray budget (Phase 4) feeds into existing `VK_KHR_fragment_shading_rate`
- **Scene classification** (2D/2.5D/3D) — skip RT/TAA/ReSTIR in 2D-only scenes
- **SH Light Probes** — existing probe system is foundation for radiance caching (Phase 6)
- **Hash grid** — natural upgrade path from surfel cache (Phase 12), shares bindings 24-26
- **Mesh shaders** — natural evolution of existing GPU culling (Phase 15), uses HiZ pyramid already built
- **Rendering philosophy**: Sparse reconstruction-first — spend fewer rays more deliberately, reuse across frames, reconstruct intelligently. Not brute-force path tracing.

---

## Accessibility (Engine-Level)

### Editor Accessibility ✅ Complete

The editor itself is fully accessible:

- ~~**Screen Reader Support**~~ ✅ (partial) — AccessibilityAnnouncer with priority-queued text status bar (Low/Normal/High/Critical), visual bottom-bar overlay with color-coded priority, console logging option, configurable display duration. Groundwork for future OS accessibility API integration (UI Automation, AT-SPI, NSAccessibility)
- ~~**Keyboard-Only Navigation**~~ ✅ — Panel focus shortcuts (Ctrl+1-5 for Hierarchy/Inspector/Viewport/Console/Assets), focus ring indicators (blue border on active panel), keyboard gizmo nudge (Arrow keys: translate/rotate/scale, Ctrl+Arrow: fine nudge, PageUp/PageDown: Y axis), configurable nudge amounts
- ~~**Alternative Input Devices**~~ ✅ — AlternativeInputManager with switch access (scanning mode, configurable scan speed, 1-4 switches, auto-reverse), eye tracking (dwell-click, smoothing, dead zone, gaze indicator), sip-and-puff (configurable pressure thresholds), head tracking (sensitivity, smoothing, dead zone, axis inversion). Editor settings panel with per-device configuration. Requires external driver software for hardware integration
- ~~**Motor Accessibility**~~ ✅ — Adjustable click threshold (1-20px), drag threshold (1-30px), dwell-click (auto-click after configurable hover delay 0.3-3.0s), sticky drag (click-to-start, click-to-release), hold repeat delay/rate. "Motor Impaired" quick preset enables all motor aids + keyboard nav. All settings persistent in editor_settings.json
- ~~**Visual Accessibility**~~ ✅ — Configurable font sizes, UI scale (0.75-2.0x), custom accent colors (11 elements), 8 colorblind modes, brightness/contrast, 11 themes including 4 high-contrast
- ~~**Audio Accessibility**~~ ✅ — AudioVisualIndicatorSystem: colored dot overlays for audio events (one-shot fade-out + continuous pulse), configurable size/position/labels. Test buttons in editor settings
- ~~**Blind-Accessible Workflow**~~ ✅ — Command palette (Ctrl+P) with fuzzy search over 25+ registered commands for entity CRUD, scene operations, view panel toggles, gizmo modes, play mode, and accessibility settings. Keyboard-only navigation (Up/Down/Enter/Esc), category tags, shortcut hints. Announcer integration announces executed commands for screen reader workflows

### Runtime Game Accessibility — Gaps & Planned Work

The following accessibility features exist as infrastructure/settings structs but are **not yet wired to the runtime Player app or UICanvas system**. Games built with the engine currently lack user-facing accessibility unless developers manually implement it.

#### Working at Runtime
- ~~**Subtitles**~~ ✅ — SubtitleSystem integrated with DialogueSystem, configurable font size (16-48px), background opacity, direction indicators, speaker names, position control
- ~~**Colorblind Filter**~~ ✅ — 8 colorblind modes applied globally to GPU rendering pipeline via PostProcessing, configurable strength
- **RuntimeAccessibilitySettings struct** ✅ — Defined with reducedMotion, disableScreenShake, disableFlashingLights, disableFOVEffects, brightness/contrast, subtitle config, input toggle modes. `ApplyToPostProcessing()` method exists

#### ~~Not Yet Runtime-Ready~~ ✅ All Wired

| Feature | Status |
|---------|--------|
| ~~**Screen Reader for Game UI**~~ | ✅ Done — Announcer wired into UISystem via SetAnnouncerCallback(), announces on focus change/activation in PlayMode + Player |
| ~~**Alternative Input for Game UI**~~ | ✅ Done — AlternativeInputManager wired into PlayMode + Player, Update/RenderOverlay called, scan targets from UISystem |
| ~~**Audio Visual Indicators at Runtime**~~ | ✅ Done — AudioVisualIndicatorSystem wired into PlayMode + Player, auto-indicators from SimpleAudio callbacks |
| ~~**Content Warnings**~~ | ✅ Done — ContentWarningSystem in Player, reads per-scene flags, shows overlay before game render |
| ~~**Motor Accessibility at Runtime**~~ | ✅ Done — Dwell-click + sticky drag on UICanvas, configurable via accessibility settings |

#### Missing Entirely (Needs Implementation)

| Feature | Priority | Effort | Description |
|---------|----------|--------|-------------|
| ~~**UICanvas Keyboard Navigation**~~ | P1 | Medium | ~~Tab/Shift+Tab between focusable UIElements, Enter/Space activation, arrow key menu navigation. `tabOrder` field on UIElement, focus state machine in UISystem~~ ✅ Complete |
| ~~**UICanvas Gamepad Navigation**~~ | P1 | Medium | ~~D-pad/analog stick navigation between UIElements, A/B activation. Arrow/DPad/Left Stick navigation with key repeat~~ ✅ Complete |
| ~~**Focus Indicators**~~ | P1 | Low | ~~Visible focus ring rendering on active UIElement (configurable color/width in UITheme, per-element focusColor override). Rendered in UISystem after widget render~~ ✅ Complete |
| ~~**Accessible Labels**~~ | P2 | Low | ~~Add `accessibleLabel` field to UIElement for screen reader text. Falls back to element name if not set. Serialized in SceneSerializer, editable in inspector~~ ✅ Complete |
| ~~**In-Game Accessibility Menu**~~ | P1 | Medium | ~~Accessibility Menu startup template with subtitle toggle, subtitle size slider, colorblind toggle, reduced motion toggle, input sensitivity slider. Uses focus navigation for keyboard/gamepad control~~ ✅ Complete |
| ~~**High Contrast UI Theme**~~ | P2 | Low | ~~`HighContrastDark` and `HighContrastLight` presets with WCAG AAA 7:1+ contrast ratios, 3px borders, bright focus indicators~~ ✅ Complete |
| ~~**Font Scaling for Players**~~ | P2 | Low | ~~`fontScale` field on RuntimeAccessibilitySettings, `SetFontScale()` on UISystem — all 5 text render paths multiply by scale factor. Wired in Player from accessibility settings~~ ✅ Complete |
| ~~**Dyslexia-Friendly Options**~~ | P3 | Low | ✅ Done — FontLibrary.h with FontFamily enum (Default/Monospace/OpenDyslexic), letter/word/line spacing config |
| ~~**Reduced Motion for UI**~~ | P2 | Low | ~~✅ Done — UISystem honors `m_ReducedMotion` flag: cursor blink disabled (always visible), instant tooltips, switch access pulse/scan indicator made static. Wired via `PlayMode::GetUISystem()` setter from EditorLayer~~ |
| ~~**Colorblind-Safe UI Palettes**~~ | P3 | Low | ✅ Done — ColorblindPalette.h with 9 palettes, pattern+icon alongside color (PatternType enum) |
| ~~**One-Button Mode**~~ | P3 | Medium | ✅ Done — Switch access scanning in UISystem with configurable scan speed, visual highlight pulse |

---

## Version Control & Collaboration

- ~~**Git Integration**~~ ✅ — Built-in git panel (stage, commit, push, pull, branch, merge), visual scene diff (structured JSON), optional `git init` on project creation
- ~~**Scene & Entity Locking**~~ ✅ — SceneLockManager with advisory `.enjinlock` JSON sidecar files: entity-level locking (Lock/Unlock in hierarchy context menu), user/machine identification (auto-detected from environment), visual indicators ([X] locked by other = red overlay, [=] locked by self), inspector lock warning banner, automatic lock file cleanup (deleted when no locks remain), 5-second auto-refresh. Scene-level locking API (LockScene/UnlockScene). All locks released on scene close/shutdown
- ~~**Collaborative Editing**~~ ✅ — CollaborativeEditingUI wires OT-based system into editor: host/join panel, remote op handlers (8 op types applied to local scene), peer cursor visualization, conflict resolution dialog, lock enforcement, scene sync on join
- ~~**Clean Git Serialization**~~ ✅ — Deterministic scene files (sorted keys, stable ordering, no floating-point drift)

---

## Legacy Timeline & Retro Web Game Support (unannounced - do not market)

Status: internal capability, deliberately unannounced. Marty will decide its positioning separately; keep it out of README/site/marketing copy until then.

### ~~SWF Import & Conversion~~ ✅ Done
- ~~**SWF parser**~~ ✅ — `SWFLoader` + `SWFConverter` — parse SWF, rasterize shapes to PNG, extract bitmaps/sounds, convert to ECS entities with Sprite2DComponent
- ~~**Timeline-to-Animation**~~ ✅ — SWF frames → TimelineComponent keyframes, frame labels → markers, MovieClip depths → child entities
- ~~**MovieClip mapping**~~ ✅ — SWF sprite hierarchy → entity hierarchy, SWFMatrix → TransformComponent (twip→pixel, Y-flip), SWFColorTransform → material tint

### Flash-Style Authoring Workflow
- ~~**Frame-based timeline editor**~~ ✅ — `TimelineEditor` class extends Timeline with Flash-style authoring: layers, property tracks, 4 interpolation modes (Constant/Linear/Bezier/CatmullRom), auto-key, curve editor with tangent handles, dopesheet view, onion skinning, copy/paste/delete, frame snapping
- ~~**Vector drawing tools**~~ ✅ — VectorDrawingEditor with 7 shape types (Line/Rect/Ellipse/Pen/Bezier/Star/Polygon), 8 tools, ImDrawList canvas rendering, layers, undo/redo (50 levels), SVG export, snap-to-grid, zoom/pan, property panel. EditorPanel `1<<29`
- ~~**Symbol library**~~ ✅ — SymbolLibrary.h/cpp: catalog-based system with grid browser, category tabs, search, nested editing (isolated World), prefab bridge, FlashTimeline integration, thumbnail previews

### ~~ActionScript Compatibility Layer~~ ✅ Done
- ~~**AS2/AS3 → AngelScript transpiler**~~ ✅ — `AS3Transpiler` class: pattern-based line-by-line transpilation with regex, type mapping (Number→float, etc.), class/function/var conversion, Flash API → shim calls, scope tracking
- ~~**Flash API shim library**~~ ✅ — `FlashAPIShim` namespace: `RegisterFlashBindings()` with DisplayObject (position/scale/rotation/alpha/visible), MovieClip (gotoAndPlay/Stop/Play/Stop), Stage (width/height/fps), Mouse/Keyboard, TextField, Math, Sound (via SimpleAudio), Timer (setTimeout/setInterval)

### Flash Game Templates
- ~~**Starter templates**~~ ✅ — Pre-built project templates for common Flash game genres: point-and-click adventure, dress-up game, tower defense, bullet hell, rhythm game, escape room, idle/clicker. All included in the 43 built-in templates and polished with HUD elements, enemies, inventory, and gameplay setups

### WebAssembly Export (Prerequisite) ✅ COMPLETE
- ~~**WebGPU/WebAssembly target**~~ ✅ — WebGPU renderer backend with WGSL shaders (PBR, shadow, post-process), compile-time switching via `ENJIN_RENDERER_WEBGPU`
- ~~**Emscripten integration**~~ ✅ — Canvas window, browser keyboard/mouse/wheel input, miniaudio Web Audio, `emscripten_set_main_loop_arg` adaptation, web player entry point
- **Size optimization** — Tree-shaking unused systems, texture compression (Basis Universal), audio compression (Opus), code splitting for progressive loading

### Renderer Backend Abstraction (IN PROGRESS — started 2026-04-18)

**Goal**: RenderSystem drives Vulkan, WebGPU, and Metal through `IRenderBackend` so games are playable on all platforms through one codebase.

| Phase | Description | Status |
|-------|-------------|--------|
| 0 | Foundation types (`GPUTypes.h`, `GPUCapabilities.h`) | ✅ Done |
| 1 | `IRenderBackend` expansion (capabilities, frame state, manager accessors) | ✅ Done |
| 2 | Buffer + Texture managers (`IGPUBufferManager`, `IGPUTextureManager`) | Pending |
| 3 | Pipeline + Shader managers | Pending |
| 4 | Bind group manager (unified descriptors) | Pending |
| 5 | Render command encoder (`IRenderEncoder`) | Pending |
| 6 | Migrate `RenderSystem` core path to abstract interface | Pending |
| 7 | Shadow mapping for WebGPU | Pending |
| 8 | Skeletal animation + post-processing for WebGPU | Pending |
| 9 | Retire `WebRenderPipeline`, web player uses `RenderSystem` | Pending |

**Platform matrix**: Windows/Linux (Vulkan), Web (WebGPU), macOS (Metal future), Android (Vulkan future), iOS (Metal future)

---

## Project Hub Redesign & Template Creator (Partially Complete)

### Terminology (Clarified Hierarchy)

The engine uses four distinct save concepts — users must clearly understand each:

| Concept | What It Stores | File(s) | When Saved |
|---------|---------------|---------|------------|
| **Scene** | Entity hierarchy, component data, world state | `.enjin` | File > Save Scene |
| **Window Layout** | Panel visibility flags, panel sizes/positions, splitter ratios | `editor_layout.json` (per-project) | Auto-saved on close, or Layout > Save Layout |
| **Template** | A starter scene + a window layout + metadata (name, description, category, thumbnail) | `.enjintemplate` (directory: `scene.enjin` + `layout.json` + `meta.json` + `thumbnail.png`) | Template Creator > Save |
| **Project** | All scenes, assets, scripts, settings, templates, build config | `.enjinproject` (directory with `project.enjinproject` manifest) | File > Save All / auto-save |

### Project Hub Flow Redesign

Replace the current 4-tab flat layout (Recent / New / Open / Demos) with a clearer entry flow:

**Splash Screen** (unchanged: logo + version fade-in, 4s)

**Landing Page** (replaces current Project Hub — 3 primary actions):

```
┌──────────────────────────────────────────────────┐
│                  TEGE Engine                     │
│                                                  │
│   ┌──────────┐  ┌──────────┐  ┌──────────┐      │
│   │  New     │  │  Open    │  │  Sandbox │      │
│   │ Project  │  │ Project  │  │          │      │
│   └──────────┘  └──────────┘  └──────────┘      │
│                                                  │
│              Recent Projects (below)             │
└──────────────────────────────────────────────────┘
```

- **New Project** → Template browser (current "New" tab grid), then a **popup dialog** for project name, location, scene name, git init. Template selection and naming are separate steps — pick template first, then configure project in a modal
- **Open Project** → Project browser with recent project list + "Browse..." button. Shows any valid `.enjinproject` directory. Can be linked/aliased from anywhere on disk (like Unity Hub). Quick-filter search bar, sort by name/date/size
- **Sandbox** → Starts an unnamed, unsaved session using the Template Creator. User picks which panels are visible, optionally loads a template scene, and starts working. Session is disposable by default — prompt to save on close. If saved, becomes a named project. Good for quick prototyping, testing, learning

### Template Creator ✅ COMPLETE (Core)

Core template creator implemented: `TemplateCreator.h/cpp` with `SaveTemplate()`, `LoadTemplate()`, `ScanTemplates()`, `DeleteTemplate()`. Editor UI at View > Tools > Template Creator with metadata editing (name, description, category), save/load/delete. Custom templates stored in `templates/` directory. Original detailed plan below for future enhancements:

A dedicated tool (accessible from Sandbox flow + Tools menu) for creating and editing templates:

**Template Metadata:**
- Template name, description, category (2D/3D/Multiplayer/Custom)
- Author name (auto-filled from system user)
- Thumbnail image — **auto-captured from the viewport** on save (screenshot of the initial scene state at the current camera angle). Can also be manually set via file picker

**Window Layout Configuration:**
- Checklist of all `EditorPanel` flags — user checks which panels should be visible when the template is applied:
  - [ ] Hierarchy
  - [ ] Inspector
  - [ ] Viewport
  - [ ] Console
  - [ ] Asset Browser
  - [ ] Game View
  - [ ] Settings
  - [ ] Effects
  - [ ] Profiler
  - [ ] etc.
- Panel size presets (Compact / Standard / Wide) or custom ratio sliders
- Game View default size and position

**Scene Content:**
- The current scene state becomes the template's starter scene
- Entity list preview (read-only summary of what the template will include)
- Option to strip editor-only data (selection state, camera position) or preserve it

**Save/Export:**
- Save to `templates/` directory as `.enjintemplate` folder (scene.enjin + layout.json + meta.json + thumbnail.png)
- Templates appear in the New Project template browser automatically
- Export as shareable `.enjintemplate.zip` for community sharing

### ~~Template Thumbnail Capture~~ ✅ DONE

~~Each built-in template should have a representative screenshot.~~ Implemented: `RenderTarget::CaptureToPixels()` reads Vulkan framebuffer via staging buffer with BGRA-to-RGBA swizzle. `TemplateCreator::SaveThumbnail()` downscales to 280x180 and writes PNG via `stbi_write_png`. Auto-captures game view on template save. Files: `RenderTarget.h/cpp`, `TemplateCreator.h/cpp`.

### Per-Template Window Layouts

Every built-in template should ship with a thoughtfully designed default layout:

| Template | Visible Panels | Layout Notes |
|----------|---------------|-------------|
| Blank | All defaults | Standard editor layout |
| 2D Platformer | Hierarchy, Inspector, Viewport, Console, Game View | Wide game view (ortho), compact hierarchy |
| 3D Third Person | Hierarchy, Inspector, Viewport, Console, Game View, Settings | Standard 3D layout |
| Visual Novel | Hierarchy, Inspector, Game View, Dialogue | Large game view, dialogue editor prominent |
| RPG Village | All + Effects + Skybox | Full-featured RPG development layout |
| Racing | Viewport, Game View, Console | Maximized game view for splitscreen testing |
| Pixel Art (2D) | Hierarchy, Inspector, Viewport, Pixel Editor, Console | Pixel editor front-and-center |
| Horror | Hierarchy, Inspector, Game View, Effects, Settings | Dark theme auto-applied, effects panel visible |

### Default Project Directory (`TEGE_Projects`)

- **First-run setup**: On first launch (no `editor_settings.json` found), prompt the user to select a default project directory
- Default suggestion: `Documents/TEGE_Projects/` (Windows: `%USERPROFILE%\Documents\TEGE_Projects`, Linux: `~/TEGE_Projects`, macOS: `~/Documents/TEGE_Projects`)
- Stored in `editor_settings.json` as `defaultProjectPath`
- New Project dialog pre-fills this path
- Open Project browser shows this directory by default
- Can be changed anytime in Settings > Project

### Software Distribution & Installation

The engine is currently source-built (clone + cmake + build). Future distribution tiers:

**Tier 1: Source Distribution (Current)**
- Clone repo, install dependencies (Vulkan SDK, CMake, C++20 compiler)
- `cmake --build` produces `EnjinEditor.exe` + `EnjinPlayer.exe`
- No installer, no registry entries, fully portable
- Target audience: engine developers, contributors

**Tier 2: Pre-Built Binary Distribution ✅ COMPLETE**
- CMake install rules + CPack config in root CMakeLists.txt for Windows ZIP packaging
- `scripts/package.bat` (Windows) and `scripts/package.sh` (Linux/macOS) for one-command builds
- Extract-and-run — no installer needed (portable)
- Includes `EnjinEditor.exe`, `EnjinPlayer.exe`, `glslangValidator`, sample templates
- Target audience: game developers who don't need to modify the engine

**Tier 3: Installer Distribution ✅ COMPLETE**
- Windows (primary): **Inno Setup** installer (`installer/EnjinSetup.iss`) — app icon (`enjin.ico`), license page (BSL 1.1), component selection (Editor/Player/Shaders/Scripts/Docs), `.enjin` file associations, Start Menu + Desktop shortcuts, modern wizard style, LZMA2 compression. Build: `ISCC.exe installer\EnjinSetup.iss`
- Windows (alternative): NSIS installer via CPack (`cpack -G NSIS`) with Start Menu + Desktop shortcuts, `.enjin`/`.enjpak` file associations, standard uninstaller. Generates both ZIP and NSIS from a single `cpack` invocation
- Linux: AppImage or Flatpak (self-contained, no system dependencies) — future
- macOS: `.dmg` with drag-to-Applications — future
- Auto-updater (check GitHub releases API on startup, download + replace binaries) — future
- Target audience: end users, non-technical game developers

**Tier 4: Hub Application (Future)**
- Standalone launcher (like Unity Hub / Epic Games Launcher)
- Manages multiple engine versions side-by-side
- Project browser with create/open/recent
- Template marketplace integration
- Account system for license management (free/indie/pro tiers)

---

## Artistic Rendering & Visual Techniques ✅ COMPLETE

All planned artistic rendering techniques have been implemented.

### Surface & Material Rendering

- ~~**Parallax Occlusion Mapping**~~ ✅ — 4 POM modes (Basic/Steep/Occlusion/Relief) with self-shadowing, silhouette correction, configurable depth. Per-material toggle in inspector
- ~~**Flat-Shaded Low-Poly with Dithered Gradients**~~ ✅ — Per-material `ditherGradient` with 2-8 bands and 6 dither patterns (Bayer 4x4/8x8, Blue Noise, Halftone, Crosshatch, Overlook). `surfaceParam1` push constant
- ~~**Metaball / Blob Rendering**~~ ✅ — Marching cubes isosurface extraction, gradient normals, per-group color blending. Inspector component with live preview

### Lighting & Global Illumination

- ~~**Spherical Harmonics Lighting**~~ ✅ — SH probe baking (L2, 9 coefficients) for diffuse indirect lighting. Probe grid placement tool in editor (per-probe list with bake status colors, individual Bake/Delete buttons, L0 irradiance preview, manual probe placement via DragFloat3 + Add), viewport visualization (green=baked, red=empty spheres, yellow grid bounds AABB), blend weights. Wired to renderer via LightingUBO shProbeIrradiance. Bake button in Settings > Scene > Light Probes
- ~~**Beam Tracing and Cone Tracing (VXGI)**~~ ✅ — Voxel cone tracing for real-time diffuse/specular GI, AO, and god rays via mip pyramid. Voxelized scene representation

### SDF & Distance Field Rendering

- ~~**SDF Ray Marching**~~ ✅ — 6 SDF primitives, 6 boolean ops, CPU evaluation, GPU buffer packing, sphere tracing
- ~~**SDF Rendering (3D Vector Art)**~~ ✅ — Mesh-to-SDF conversion, sphere tracing, 8SSEDT text rendering, volume blending. Resolution-independent 3D vector graphics

### Transparency & Compositing

- ~~**Order-Independent Transparency**~~ ✅ — Weighted blended OIT with composite pipeline and embedded SPIR-V (`oit_composite.frag` + `fullscreen.vert`)
- ~~**Framebuffer Feedback Effects**~~ ✅ — 8 presets (Echo, Melt, Infinite Mirror, VHS, etc.), ping-pong compositing, 5 blend modes, configurable decay/UV offset

### Distortion & Post-Processing

- ~~**Screen-Space Distortion**~~ ✅ — 7 distortion types including shockwave, heat haze, underwater caustics, dream warping. Per-object distortion volumes

### Mesh & Animation

- ~~**IK-Driven Mesh Deformation**~~ ✅ — FABRIK + Verlet physics IK chains with tube/ribbon mesh skinning for tentacles, ropes, tails

### Procedural & Terrain

- ~~**Fractal Terrain and L-System Vegetation**~~ ✅ — Fractal terrain generation (fBm octave stacking, ridged multifractal) with erosion simulation (hydraulic via droplet simulation, thermal via talus angle). 3D L-system with full turtle interpreter (yaw/pitch/roll), stochastic production rules, branch radius decay. Added to `ProceduralAlgorithms.h/cpp`. One-click "Generate Forest" UI in Procedural panel: 4 forest types (Mixed/Dense Conifer/Deciduous Park/Sparse Savanna), area radius, tree/shrub/grass density controls, creates Tree+Shrub+Grass volume entities

### Simulation-Driven Geometry

- ~~**Reaction-Diffusion on Meshes**~~ ✅ — `ReactionDiffusion` class (Gray-Scott model) with 9 presets (MitosisSpots, CoralGrowth, Fingerprints, Leopard, Labyrinth, WormHoles, BubblePacking, Spirals, Custom). Configurable feed/kill rates, diffusion coefficients, sub-stepping, circular/random seeding, bake-to-RGBA8 texture and heightmap export.
- ~~**Cellular Automata as Geometry**~~ ✅ — `CellularAutomataGeometry` class with 7 CA rules (GameOfLife, HighLife, DayAndNight, Seeds, BriansBrain, Rule110, Diamoeba, Custom). 3 mesh modes: Voxels (greedy face culling), Marching Cubes (256-entry tables), Point Cloud. Classic pattern stamps (Glider, Pulsar, Gosper Glider Gun).
- ~~**Slime Mold Simulation (Physarum)**~~ ✅ — `PhysarumSimulation` class with 50K+ agents, trail map diffusion/decay, 5 presets (ClassicSlime, BranchingNetwork, DenseWeb, Tendrils, Pulsating). Food source placement, bilinear trail sampling, bake-to-RGBA8 and heightmap export.
- ~~**Fluid Simulation as Terrain**~~ ✅ — `FluidTerrainCoupling` system wires FluidSimulation density/velocity grids to TerrainComponent heightmap. Erosion mode (velocity erodes terrain height) and accumulate mode (density builds terrain, e.g. lava). Bidirectional: terrain slope drives fluid flow. Configurable erosion rate, accumulation rate, coupling strength. Files: `FluidTerrainCoupling.h/cpp`
- ~~**Voronoi Fracture with Persistent Physics**~~ ✅ — VoronoiMeshFracture (Sutherland-Hodgman mesh clipping) produces real mesh fragments. FractureConfigComponent for per-entity config (fragment count, explosion force, persistence, re-fracture depth, pre-fracture with breakable joints, auto-cleanup). DestructibleSystem extended with `CreatePersistentFragments()` — fragments become full ECS entities (Transform+Mesh+Material+Rigidbody+BoxCollider). FIFO fragment entity limit, recursive re-fracture with reduced count per depth level. Editor inspector UI and full serialization

### Simulation & Flow

- ~~**Curl Noise Flow Fields for Particle/Mesh Advection**~~ ✅ — Header-only CurlNoise3D math (finite differences of FBM3D, 12 FBM calls per sample). CurlNoiseFieldComponent with AABB volume, falloff (None/Linear/Smooth), configurable octaves/frequency/amplitude/seed/timeScale. CurlNoiseSystem applies divergence-free forces to particles within volume. Editor inspector with debug arrow visualization and full serialization
- ~~**Wave Racer 64 Water**~~ ✅ — Interactive water with spring-damper wave propagation, splashes/wakes/buoyancy, boat/object interaction. `InteractiveWaterComponent` wired in PlayMode, Player, editor inspector, and full serialization
- ~~**Mesh Audio Reactivity via FFT Vertex Displacement**~~ ✅ — Cooley-Tukey FFT analysis, bass/mid/treble frequency bands mapped to per-vertex mesh displacement. 4 mapping modes. `AudioReactiveMesh` component

### Mathematical & Exotic Geometry

- ~~**Fourier Transform Meshes**~~ ✅ — `FourierMesh` class: DFT decomposition of 2D contours, progressive reconstruction animation, 3D extrusion. Epicycle visualization
- ~~**Non-Euclidean Geometry Rendering**~~ ✅ — Portal rendering with stencil recursion, hyperbolic/spherical/toroidal space warping. Impossible geometry for puzzle games and horror
- ~~**Stereographic Projection of 4D Objects**~~ ✅ — `Projection4D` class: 5 polytopes (tesseract, 120-cell, 600-cell, 24-cell, 16-cell), 6 rotation planes (XY/XZ/XW/YZ/YW/ZW), stereographic 4D→3D projection

### Inverse & Advanced Rendering

- ~~**Inverse Rendering / Differentiable Rendering**~~ ✅ — Gradient descent parameter optimization. Optimize scene parameters (materials, lights, camera) to match target images. Compute shader forward pass with parameter gradients, iterative optimizer

---

## Asset Libraries ✅ COMPLETE

Goal: Ship a curated, commercially licensable library so users have beautiful assets out of the box — important for a licensable engine.

### Font Library ✅ COMPLETE

42 curated OFL/Apache-licensed fonts across 8 categories (Sans-Serif, Serif, Monospace, Display, Handwriting, Pixel, Fantasy, Sci-Fi). `FontLibrary.h/cpp` + `AssetLibrary.h/cpp`. Editor browser in Settings > System > Fonts with search, category filter, and install status.

- Categories: Sans-serif (UI/HUD), Serif (narrative/books), Monospace (code/terminal), Display/decorative (titles/logos), Handwriting/script, Pixel/retro, Fantasy/medieval, Sci-fi/futuristic
- Sources: Google Fonts (OFL), Font Squirrel (verified commercial licenses), The League of Moveable Type

### 3D Asset Library ✅ COMPLETE

16 CC0 3D model packs (Kenney, Quaternius) with editor browser. `AssetLibrary.h/cpp`.

- Categories: Architecture, Nature, Props, Characters, Vehicles, Weapons, Dungeon, Sci-Fi
- Sources: Kenney.nl (CC0), Quaternius (CC0)

### 2D Asset Library ✅ COMPLETE

15 CC0 2D sprite/tileset/UI packs with editor browser. Uses shared `AssetLibrary.h/cpp`.

- Categories: UI Kits, Tilesets, Sprites, VFX, Backgrounds, Textures, Characters, Props, Architecture, Nature, Vehicles, Weapons, Dungeon, Sci-Fi
- Sources: Kenney.nl (CC0), OpenGameArt.org (curated CC0)

### Licensing Strategy

- All bundled assets must be **CC0 or OFL/Apache 2.0** — no CC-BY-NC, no CC-BY-SA (share-alike creates friction for commercial games)
- Engine license grants full sublicense rights — users can ship assets in their games without attribution requirements
- Clear `LICENSE.md` per asset category documenting provenance and license terms
- Consider commissioning original CC0 assets for a distinctive "Enjin style" that differentiates from Unity/Unreal default look

---

## UI/UX Design Philosophy

- Aesthetically accessible, clean, forward-thinking, timeless
- Own identity (not Unity grey, not Unreal dark, not Apple-style)
- Information-dense but not cluttered
- Consistent patterns (context menus, drag behavior, property editing)

---

---

## Known Stubs & Incomplete Features

All 24 previously-tracked stubs have been resolved (Audit #4, 2026-02-14). Notable completions: OIT composite shader, cross-platform HTTP (WinHTTP + libcurl), SWF zlib decompression, network HMAC-SHA256 auth, DoF/tilt-shift, SH light probes, OIDN GPU→CPU copy, UI Phase 2 widgets (9 types), Water3D rendering, Player/PlayMode refactor into shared GameplayLoop module, VSync toggle, shader graph nodes, MIDI input.

No outstanding stubs remain.

*Last updated: 2026-03-18 — ReSTIR DI foundation: per-pixel reservoir struct, importance-weighted light selection compute shader (binding 19), editor toggle. Temporal/spatial reuse planned. Phases 1-3 complete.*

---

# Reference: release detail and backlogs

Merged from MASTER_PLAN.md.

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
- ~~Cross-project scene open keeps the wrong script root silently~~ **FIXED 2026-08-14 (warn)**: AutoDetectProjectForScene early-returned whenever any project was loaded, so opening a scene from a different project kept the old script/asset roots silently. Now, if the opened scene isn't under the current project root and has its own .enjinproject, it warns + logs (does not auto-switch — that could discard unsaved work).

### Template QA
- Roster cut to 16 templates on 08-07 (the old 48-entry catalog is gone for good). Most of the 16 still untested — biggest user-facing risk for anyone downloading 0.9.7. The play-probe and golden harness can automate the boots-and-plays tier.

### Earmarked latent bugs (diagnosed, unfixed)
- ~~Progressive cascade update: forceFullUpdate checks position only, never rotation~~ **FIXED 2026-08-14**: also forces a full cascade update when the camera's view direction changes appreciably (fwd dot < 0.9998, ~1 deg/frame), so far cascades no longer lag/show stale shadows while turning in place. Wants an eyeball (turn the camera with progressive cascades on).
- God rays: hardcoded 0.5 luminance threshold.
- Cel outline: distance-dependent Sobel threshold on raw depth.

### Tests
- Component round-trip batch: ~~ParticleEmitter, AudioSource~~ DONE 2026-08-14 (also fixed AudioSource pitchMin/pitchMax not serializing); Animator + Skeleton remain (complex nested data; partly covered by TestGoldenVerification).
- ~~Negative-path serializer suite~~ DONE 2026-08-14 (empty/garbage/non-object/truncated JSON fail gracefully).
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
- **Sprite / pixel-art editor upgrade (Marty backlog, added 2026-08-15)**: a `PixelEditor` already exists (`Engine/*/Editor/PixelEditor.{h,cpp}` — NewCanvas, LoadImage, ExportPNG, layers, named palettes, drawing tools, Render). TODO: (1) **pass it through the unified UICanvas UI** (per the UI unification charter — it's currently ImGui) so it's consistent + web-capable; (2) **upgrade the workflow so people can easily edit IMPORTED pixel art** — open an imported sprite/texture straight into the editor, edit + save back to the asset (round-trip), palette extraction from the imported image, per-layer editing, zoom/grid/onion-skin niceties. Make "right-click a sprite asset → Edit" a first-class flow.

### Particle system overhaul (Marty backlog, added 2026-08-15)
Goal: a particle system that is robust, user-friendly, cool, and dead-easy to fire/start/stop/loop from nodes OR script, in 2D or 3D, with a good variety of particle PRIMITIVES and a preset library so you can make mist/spray/blood/sparks/smoke/etc. without fiddling.
- **What exists**: CPU `ParticleSystem` + `ParticleRenderer` + `GPUParticleSystem` (compute, dormant — see the DOTS/GPU-compute note); `ParticleEmitterComponent` has loop, `EmitterShape` (Cone + others), shapeRadius, `RenderMode` (Billboard/VelocityStretch), texture sheet animation; nodes exist: Play Particles / Stop Particles / Particle Burst / Apply Particle Preset. So the bones are there.
- **Robustness + UX**: audit the CPU path for correctness/perf; make emitters reliable and cheap; clear inspector with live preview; sensible defaults so a fresh emitter looks good immediately.
- **Control parity (node + script, 2D + 3D)**: easy Fire/Start/Stop/Loop/Burst from BOTH VisualScript nodes and AngelScript (some particle nodes exist; fill gaps + ensure script bindings match; works the same in 2D and 3D scenes). This ties into the node-coverage + scripting-parity work.
- **Primitive variety**: billboard (have), stretched/velocity (have), plus mesh particles (spawn a mesh per particle), ribbons/trails, point sprites, soft particles; multiple emitter shapes (cone/sphere/box/circle/edge/mesh-surface).
- **Preset library**: ship ready-made presets — mist, spray, blood, sparks, smoke, fire, dust, rain-splash, magic — selectable in inspector and via `Apply Particle Preset`. This is the "easily make anything" payoff.
- **GPU scale path**: for huge counts, route through the GPU-compute particle system once its draw path is revived (shared work with the DOTS-competitor).
- ~~Entity-reference script property drag-assign (Marty)~~ **DONE 2026-08-14**: `[Property] uint64`/`[Property] Entity` script properties existed (ScriptPropertyType::Entity) but the inspector only drew a raw DragInt (type the numeric ID). Now a proper object slot — shows the assigned entity's name (or None/missing), drop an entity from the Hierarchy to assign it (accepts the existing ENTITY_REPARENT payload), with a clear button. This is the static entity-reference assignment mechanism.

### Scripting and serialization surface
- Remaining unbound/partially-bound components (joints, colliders, tilemap, and hierarchy were bound in the May cleanup; ~35 remain).
- Component serializer coverage from ~7% round-trip-tested toward full; mid-play save state (AI, audio, tweens).
- SCRIPTING_API doc gaps (HasComponent coverage, AI bindings, Camera_ApplyPreset).

### Docs
- ARCHITECTURE.md drift (MaterialGPU 80 to 112, backend phases). Dual-system explanations. RT shader workflow. ROADMAP refresh against this document.

### Simulation and materials (Marty backlog, added 2026-08-13)
- Buoyancy volumes: water zones that float/sink bodies via depth-based buoyancy + drag, with a 2D-heightfield wave sim. **Full spec (Gobliny-driven, canonical): `D:\TEGE_Projects\goblin\design\tech\tege-buoyancy-spec.md`.**
  - **~70% ALREADY BUILT as `InteractiveWaterSystem` (Engine/*/Effects/InteractiveWater.{h,cpp}), wired into PlayMode:743 + Player:976 + tested (TestInteractiveWater).** This answers the spec's open Q1 (reuse this heightfield, don't build a new one). Done: grid heightfield + spring-damper `PropagateWaves`, `GetWaterHeight` bilinear sample (= spec `get_surface_height`), `CreateSplash` (= `apply_displacement`), `CreateWake`, depth-based upward buoyancy + water drag on `RigidbodyComponent.velocity` (InteractiveWater.cpp:343-362), `WaterInteractorComponent` marker (= `BuoyancyBody`), absorbing/reflecting boundaries, mesh gen. NOTE a separate simpler `WaterVolumeComponent` (Components/WaterVolume.h) also exists (visual/freeze water, ContainsPoint) — the heightfield sim is the InteractiveWater one.
  - **GAPS vs spec (the real remaining work):** (1) ~~per-object DENSITY~~ **DONE 2026-08-13**: `WaterInteractorComponent` now has density (<1 floats, >1 sinks), volume, waterlogRate/waterlogMaxDensity; buoyancy force = `buoyancyForce * submergedDepth * volume * (1 - effectiveDensity)`; serialized + inspector controls + tests (light rises / dense sinks / waterlog accumulates). (2) ~~game-subscribable `WaterEnterEvent`~~ **DONE 2026-08-13**: an interactor crossing the surface downward faster than the water's `entryVelocityThreshold` fires a deferred `water_enter` event on the `EntityEventBus` (target=interactor, sender=water, floats impactForce/velocityY/x/y/z), edge-detected so it fires once on entry; wired PlayMode+Player+web; serialized threshold; tests. (3) ~~VERIFY the buoyancy `rb->velocity` writes reach Jolt~~ **VERIFIED 2026-08-13**: they reach Jolt via `SyncECSToJolt`'s external-velocity override (JoltBackend.cpp:413-422), one-frame latency. So the MVP (heightfield + density buoyancy + WaterEnterEvent + player drag) is COMPLETE. REMAINING: (4) ~~`WaterCurrent` (lazy-river flow)~~ **DONE 2026-08-13**: `InteractiveWaterComponent` carries `currentDirection`/`currentSpeed`/`currentPull`; floating interactors have their horizontal velocity lerped toward the flow (carried at flow speed, a swimmer can fight it); serialized + inspector + tests (carried up to flow speed / no-current-no-drift). Spline-path current still Alpha-tier. (5) ~~`apply_sustained_pressure`~~ **DONE 2026-08-13**: `InteractiveWaterSystem::ApplySustainedPressure(water, transform, x, z, radius, force)` eases the surface toward a held depression each call (vs CreateSplash's one-shot impulse), so a continuously-applied source holds a standing wave and relaxes when it stops; tests. Water interaction API is now SCRIPT-BOUND (2026-08-13): `Water_Splash`/`Water_Wake`/`Water_SustainedPressure`/`Water_GetHeight` in `ScriptBindings_Water.cpp` (RegisterWaterBindings, available in editor/Player/web), documented in SCRIPTING_API.md, tested. **Buoyancy spec is now feature-complete except one Alpha-tier item: spline-path current (curved lazy river).** Four components: `WaterVolume` (bounds/rest_height/heightfield resolution/damping/wave_speed + `get_surface_height`/`apply_displacement`/`apply_sustained_pressure`), `BuoyancyBody` (density, waterlog_rate, submerged/surface drag, displaced volume; polls the volume per physics tick), `WaterEnterEvent` (velocity-threshold splash → auto-displacement + game-subscribable), `WaterCurrent` (directional/spline flow for lazy river). MVP = WaterVolume heightfield + BuoyancyBody upward force + WaterEnterEvent + player submersion drag. Explicitly OUT: full 3D fluid, underwater refraction (separate render concern), boat pitch/roll, inter-object wave interaction. Physics side is Jolt 3D / Box2D 2D per the strict separation; net-sync the displacement EVENTS not the heightfield (deterministic local sim). 5 open impl questions in the spec (reuse a heightfield struct? tick rate? non-rect bounds? rigidbody drag controls? shader sampling of the heightfield).
- Refractive indices: real refraction through transparent/glass/water materials. Material already carries IOR (extended MaterialGPU: SSS/transmission/IOR/thickness), so this is the render consumer — screen-space refraction / IOR-based bending, and the RT path can read true IOR for path-traced glass.

### Tooling, scripting parity, adaptive graphics (Marty backlog, added 2026-08-13)
- Scripting parity (charter, Marty 2026-08-13): a creator should be able to build a WHOLE game in ANY one surface — all nodes, all C++, or all AngelScript — and each surface's runtime strengths/weaknesses + readability should be documented so the choice is informed. **AUDITED + DOC WRITTEN: `docs/SCRIPTING_PARITY.md`** (three-surface coverage table, node-vs-script + script-vs-C++ gap lists, runtime/readability tradeoff matrix, same-behavior-in-3-surfaces example, path to parity). FINDING: AngelScript (~1023 bound fns) is near-parity with C++; **VisualScript nodes (~261) are THE bottleneck (~25% of the bound surface)** — thin on AI/behavior-tree/generic-component-access/UI/input, ABSENT on Rewind/MIDI/Accessibility/AudioReactive. So a nodes-only game hits walls today.
  - Node editor: work on the VisualScript graph editor + node COVERAGE. The dominant parity work = generate/author nodes to match the AngelScript surface area-by-area (biggest hole first: generic accessors for the ~40 bound component types, then AI/BT/navmesh, then input/UI, then the absent areas). Much can be code-genned from the same registration data the bindings use. Plus graph-readability-at-scale (functions/subgraphs, groups) so big graphs don't become spaghetti.
  - Script-vs-C++ gap: bind the ~35 still-unbound components/systems; new engine features should ship a binding + a node in the same change (keep the three in lockstep).
- Review adaptive graphics (the 120-FPS-No-Matter-What pillar). **REVIEWED 2026-08-13: `AdaptiveQualitySystem` (Renderer/AdaptiveQuality.{h,cpp}) is FULLY IMPLEMENTED but 100% DORMANT — never instantiated, never Update()d, no QualityChangeCallback ever registered anywhere in the tree. The pillar's adaptive mechanism is dead code.** The algorithm is correct (FPS avg/variance, hysteresis, up/down cooldowns, emergency downgrade < minFPS, per-level recommendations for shadow res/render scale/LOD/particles/SSAO/bloom/volumetrics/cascades). SteamDeckSupport has its OWN separate STATIC recommendations, not this dynamic loop. Levers: `RenderSystem::SetShadowResolution` is frame-safe (defers to FlushPendingChanges) = ready; render-scale/dynamic-resolution = the crash-historied offscreen-resize path (risky). FPS input = 1/deltaTime at the call site. **FIX — SAFE SUBSET WIRED 2026-08-13**: RenderSystem now owns an AdaptiveQualitySystem, ticks it in Update() with 1/deltaTime, and a change-callback applies the frame-safe shadow levers (SetShadowResolution [deferred/GPU-idle], shadows-off at the floor, progressive far cascades below High). API: `SetAdaptiveQualityEnabled`/`SetAdaptiveQualityTargetFPS`/`GetAdaptiveQualityLevel`. Default OFF (editor never stomps authoring); the desktop **Player enables it** (target 60). Does NOT force levers on enable — only changes them when it decides to (no boot-time settings stomp). Vulkan-only for now (WebGPU shadow levers are a follow-up). Unit tests: TestAdaptiveQuality 6/6 (downgrade under sustained low FPS, emergency downgrade < minFPS, callback fires, recommendations track level, disabled = no-op). **PHASE 2 = render-scale (dynamic resolution — the big GPU lever, but the crash-historied offscreen-resize path) + cross-system levers (post-process toggles, particle cap) + WebGPU.**

### Traversal & interaction mechanics (Marty backlog, added 2026-08-15)
Standard character-action building blocks. Most need a component + controller-state + a bit of physics/animation glue; none exist yet.
- **Ropes / swinging**: verlet or physics-joint rope (Jolt distance/point constraints for 3D, Box2D for 2D); attach/detach, climb up/down, swing momentum. Ties to grapple.
- **Ladders**: climb volume + controller ladder-state (gravity off, vertical input = climb, snap to ladder axis), mount/dismount at top/bottom.
- **Doors**: openable component (hinged swing / sliding), locked/unlocked + key check (hook to Inventory keys), trigger or interact-to-open, one/two-way, auto-close timer.
- **Cover**: cover-volume + controller cover-state (snap to edge, peek, blind-fire hook, move along cover), corner detection. Pairs with AI (AI uses cover points).
- **Ledge climb (mantle)**: detect a climbable ledge ahead+above (raycast/capsule sweep), play mantle, translate up-and-over. Root-motion or scripted curve.
- **Ledge hang / shimmy**: grab a ledge edge (hang state, gravity off), shimmy left/right along the edge, climb up (→ mantle) or drop. Ledge-detection shared with mantle.
- Shared substrate: a small "interaction/traversal" state machine on the character controller + an `Interactable`/interaction-prompt system (raycast for the nearest interactable, show prompt, fire on button). Doors/ladders/ropes/cover all register as interactables. **Also unblocks: click-to-move is currently a STUB (ControllerSystem.cpp:979 empty) — wire raycast-to-ground + straight-line or navmesh FindPath first.**

### Performance & world scale (Marty backlog, added 2026-08-15)
- **DOTS-competitive batch type**: Tier-1 SoA `Swarm` PROTOTYPED (`Core/include/Enjin/Sim/Swarm.h`, TestSwarm ~300M agent-updates/sec single-thread; editor `SwarmComponent`+`SwarmSystem` visualizes via instanced cube proxies). NEXT: (a) fork-join split across cores, (b) a real GPU-instanced draw (position buffer → one draw, replaces proxy entities), (c) **Tier-2 GPU-compute agents = the actual DOTS-beater, esp. on web where DOTS can't run.** The WebGPU backend has compute pipelines: WebGPUPipelineManager::CreateComputePipeline is used by WebGPUComputeSmokeTest and by WebGPUParticleSystem, which runs a live @compute simulation shader on web. Honest verdict: TEGE ECS does NOT match DOTS on raw million-entity throughput and doesn't need to (200-dog test was GPU/draw-bound); the Swarm type is the escape hatch for the cases that do need scale.
- **Bleeding-edge world streaming**: existing = classic chunk streamer (LevelStreaming.h). Target = continuous virtualized streaming: Nanite-style geometry-cluster streaming on the existing GPUCulling/HiZ/DeviceGeneratedCommands substrate; revive the gated-OFF virtual texturing; streaming surfel GI tied to the sparse-reconstruction thesis. TEGE-specific hooks: frame-budget-bounded time-sliced streaming (never hitch = 120fps pillar), accessibility-aware cross-fade LOD (no pop-in / motion-sickness), origin-rebasing for large worlds. Phase 1 (contained, do first) = make StreamingManager time-sliced + hitch-free + predictive prefetch along camera velocity.
- **Navmesh variety**: area costs + areaType already exist (painted-regions data model half-built, needs authoring UI); MISSING off-mesh links (jumps/portals) and spline-path nav (Math/Spline.h exists but unwired to nav).

### Parked (1.0+ or cut, Marty's call)
- DLSS/XeSS vendor SDKs. FMOD/Wwise/NVN stubs (cut candidates). SteamAudio default-on decision.
- VR/OpenXR. Nanite/Lumen-style tech. Frame generation. Vulkan video decode.

---

# Reference: Glacier phases (post-1.0)

Merged from ROADMAP_GLACIER.md.

> Post-1.0. Unowned, and no phase here is tracked against the engine.
> Volumetric fog (phase 5) and GPU particles (phase 6) have shipped work.

## Context

After analyzing IO Interactive's Glacier engine advances (Digital Foundry interview, 007 First Light), this plan addresses 8 major capability gaps in Enjin. The goal: bring Enjin's rendering, streaming, ECS, animation, and editor architecture to the same caliber as a AAA in-house engine — while maintaining the cross-platform story (Vulkan + WebGPU + future Metal) and the "120 FPS No Matter What" pillar.

**Key user requirement:** All particles, transparents, and volumetrics must integrate with EVERY lighting system (clustered lights, shadows, GI probes, volumetric fog). Artists must be able to build effects however they want without worrying about which lighting features apply.

---

## Phase 1: Shader Specialization + Shadow Budget (6-8 weeks)

**Why:** Every subsequent phase depends on efficient shader variants and a dynamic shadow system. The current monolithic push-constant flags cause unnecessary GPU ALU waste; fixed shadow allocations can't scale.

**Deliverables:**
1. **Vulkan specialization constants** — Replace 32-bit push-constant feature flags with `VkSpecializationInfo`. Cache SPIR-V permutations keyed by constant values in `ShaderManager`.
2. **WGSL override constants** — WebGPU equivalent using `override` declarations in all 4 WGSL shaders.
3. **Shadow atlas pool** — Unify `ShadowMap`, `PointLightShadowMap`, `SpotLightShadowMap` into a single tiled depth atlas. Dynamic tile allocation replaces fixed per-light resources.
4. **Dynamic shadow budget** — Cluster-pixel-count priority: the `ClusteredLighting` compute pass outputs per-light pixel-weight via atomics. Top N lights get shadow tiles per frame; rest fall back to SH probe irradiance.

**Key files:** `ShaderManager.h`, `ShadowMap.h`, `PointLightShadowMap.h`, `SpotLightShadowMap.h`, `ClusteredLighting.h`, all `.wgsl` shaders
**New:** `ShadowAtlas.h`, `ShadowBudget.cpp`

**Scalability:** One shadow atlas, one budget knob. Low-end = 4 tiles at 512x512. High-end = 16+ tiles at 2048x2048. `AdaptiveQualitySystem` feeds tile size/count.

---

## Phase 2: Software-Traced DDGI via SDF (8-10 weeks)

**Why:** Glacier's crown jewel — GI that works on ALL platforms including Switch 2 class hardware, no hardware RT required. The engine already has SH probes, analytic SDFs, VCT, and RT GI — but nothing connects them into a unified software-traced probe system.

**Deliverables:**
1. **GPU mesh voxelization** — Move `VoxelGrid::Voxelize()` from CPU to compute shader. Read from `MergedGeometryBuffer`, output 3D texture (R16F) for SDF storage.
2. **GPU SDF volumes from mesh data** — Extend `MeshToSDF` to output GPU-resident 3D textures for probe tracing.
3. **DDGI probe system** — Grid of irradiance probes updated via SDF ray marching in compute. Octahedral encoding in probe texture atlas. Stores both irradiance and radiance (for volumetric sampling).
4. **Temporal amortization** — Update 1/8th of probes per frame, cycling over 8 frames. Keeps GI within 2ms budget.
5. **Fallback cascade** — VCT or existing SH probes fill in at distance where DDGI grid is sparse.

**Key files:** `VoxelConeTracing.h`, `SDFRenderer.h`, `SDFScene.h`, `SHLightProbe.h`
**New:** `DDGIProbeSystem.h`, `ddgi_probe_update.comp`, `ddgi_sample.comp`, `gpu_voxelize.comp` + WGSL equivalents

**Scalability:** Low-end: 8x4x8 grid, 32 rays/probe, update 1/16 per frame. High-end: 16x8x16, 128 rays/probe, update 1/4. WebGPU: CPU voxelization at 32^3, update once/second.

---

## Phase 3: Async Streaming with 2ms Frame Budget (6-8 weeks)

**Why:** Glacier enforces that no single streaming function exceeds 2ms on main thread at 60fps. Current `StreamingManager` is main-thread-blocking. This is critical for open-world and driving sequences.

**Deliverables:**
1. **Time-sliced entity integration** — Deserialize on worker thread into staging buffer. Integrate entities in batches on main thread, yield after 2ms wall-clock.
2. **Priority queue scheduling** — Replace flat load queue with distance + `StreamPriority` heap. Critical chunks always first.
3. **Predictive loading** — Camera velocity analysis to pre-fetch chunks 1-2 seconds ahead.
4. **Brick layer composition** — Glacier-style stacking: base layer + additive/override layers. Higher layers can override or remove entities from lower layers. Enables mission-specific scene variants without duplicating content.
5. **Memory budget** — Track resident memory; evict lowest-priority chunks approaching limits.

**Key files:** `LevelStreaming.h/.cpp`, `SceneSerializer.cpp`, `World.h`
**New:** `BrickComposition.h`, `StreamingBudget.h`

**Scalability:** Fixed 2ms budget is absolute. Low-end CPUs integrate fewer entities/frame (more pop-in, never frame drops). Web: staged integration within requestAnimationFrame, no worker threads.

---

## Phase 4: ECS Optimization for 2M Entities (5-7 weeks)

**Why:** Glacier achieves 32 bytes per entity with 2M loaded (70% logic, 30% visual). Enjin's current overhead is ~104 bytes per entity with 3 components, mostly from `unordered_set` hash buckets.

**Deliverables:**
1. **Eliminate `m_ActiveEntitySet`** — Replace with `vector<u8>` validity array indexed by entity ID. Saves ~48 bytes/entity.
2. **Generation-packed entity IDs** — Upper 32 bits = generation, lower 32 = index. `IsValid()` becomes single array lookup. Eliminates separate active entity vector.
3. **Archetype fast path** — For logic-only entities (no mesh/transform), contiguous archetype storage. Bulk insertion/iteration. Coexists with existing sparse-dense storage for visual entities.
4. **Component memory pools** — Chunked allocator for `ComponentStorage<T>::Add()` instead of per-entity `push_back` reallocation.
5. **Parallel system update** — System dependency graph allows independent systems to run concurrently.

**Key files:** `Entity.h`, `Component.h`, `World.h`, `System.h`

**Scalability:** Editing cost is O(visible), not O(total). Scene hierarchy uses virtual scrolling. Generation-packed IDs make `IsValid()` constant-time regardless of entity count.

---

## Phase 5: Volumetric Fog, Clouds, and Froxel Lighting (8-10 weeks)

**Why:** The engine currently only has linear distance fog. Glacier uses volumetric fog as both atmosphere AND a lighting medium. This phase adds true 3D volumetric rendering integrated with clustered lighting and DDGI.

**Deliverables:**
1. **Froxel volume** — 3D texture (160x90x64) in view space. Reuses cluster grid concept from `ClusteredLightingSystem`.
2. **Volumetric fog compute** — Per-froxel: sample noise-driven density, accumulate in-scattered light from ALL clustered lights (using Phase 1's shadow atlas for shadow testing), sample DDGI probes (Phase 2) for indirect light contribution.
3. **Temporal reprojection** — Reproject previous frame's volume to reduce noise. Uses existing `HaltonSequence.h` for jitter.
4. **Volumetric clouds** — Separate ray-march pass driven by `WeatherSystem` coverage parameters.
5. **Full lighting integration for fog** — Every direct light, every shadow, DDGI irradiance, all affect the fog volume. No exceptions.
6. **God ray upgrade** — Replace screen-space `fogShafts` with volumetric-aware light shafts reading from froxel volume.

**Key files:** `ClusteredLighting.h`, `PostProcessing.h`, `OITManager.h`, `Weather.h`
**New:** `VolumetricFog.h`, `volumetric_fog.comp`, `volumetric_clouds.frag` + WGSL equivalents

**Scalability:** Low-end: 80x45x32 froxels, 4 temporal samples, no clouds. High-end: 160x90x128, 16 samples, full clouds.

---

## Phase 6: GPU Particles + Volumetric Particles + Full Lighting Integration (8-10 weeks)

**Why:** This is the "Smolder" equivalent. Glacier renders 400+ volumetric particle instances with full lighting integration. **Every particle and transparent object must receive light from every system — no exceptions.** Artists build effects however they want.

**Deliverables:**
1. **GPU particle simulation** — Compute shader replaces `ParticleSystem::Update()`. Position, velocity, lifetime, forces (gravity, wind via `WindSystem`) all GPU-side. Emitter config as uniform buffer, particle state in SSBOs.
2. **Indirect draw from compute** — Eliminates CPU readback. WebGPU fallback: read count, issue direct draw.
3. **Volumetric particle shapes** — Pre-baked 3D density textures (OpenVDB import or SDF-generated). Rendered as view-aligned volume slice stacks with ray marching.
4. **FULL lighting integration for ALL particles and transparents:**
   - Every clustered light affects every particle (point, spot, directional)
   - Shadow atlas lookup per particle (Phase 1's budget system)
   - DDGI probe sampling for indirect light (Phase 2)
   - Volumetric fog in-scattering applied to particles (Phase 5's froxel volume)
   - Self-shadowing within volumetric particles via density accumulation
   - Emissive particles contribute back to the froxel volume (two-way coupling)
5. **OIT compositing for everything** — Upgrade `OITManager` so volumetric particles, billboard particles, transparent meshes, and volumetric fog ALL composite through the same order-independent pipeline. No sorting artifacts, no special cases. Artists drop any effect anywhere and it just works.
6. **Particle ↔ volumetric fog interaction** — Particles inside fog receive fog scattering. Volumetric particles contribute density to the froxel grid. Explosions thicken local fog.
7. **100K+ particle cap** — GPU simulation enables massive counts. Display cap configurable per platform.

**Key files:** `ParticleSystem.h`, `ParticleRenderer.h`, `OITManager.h`, `VolumetricFog.h` (Phase 5), `ClusteredLighting.h`
**New:** `GPUParticleSystem.h`, `particle_simulate.comp`, `volumetric_particle.frag`, `VolumetricParticleRenderer.h`

**Scalability:** CPU particle fallback remains for min-spec. GPU compute with 32K cap on GTX 1660 class. Volumetric particles are high-end only; billboards remain default on low-end. But the LIGHTING integration applies to ALL particles regardless of rendering mode — even billboard particles on low-end receive clustered light + shadows + GI.

**The principle:** "One lighting path for everything." The fragment shader for particles (billboard or volumetric) calls the same `EvaluateLighting()` function as opaque geometry. It reads from the same clustered light grid, the same shadow atlas, the same DDGI probes, the same froxel volume. No separate "particle lighting" system. No "transparent lighting" system. One system. Every light. Every shadow. Every particle.

---

## Phase 7: Motion Matching + Animation Pipeline (8-10 weeks)

**Why:** Glacier ships 10x the animation count of Hitman via motion matching. Enjin has solid animation foundations but no motion matching. This is the difference between "functional" and "AAA feel."

**Deliverables:**
1. **Motion matching database** — Offline tool processes clips into searchable pose database. Features: foot positions, hip velocity, trajectory prediction (0.2s intervals). Flat float arrays for SIMD search.
2. **Runtime pose search** — Per-frame: compute current features + desired trajectory, search database for best match. `SkeletalAnimator` gains motion matching mode alongside state machine mode.
3. **Inertialization blending** — Spring-damper decay of pose difference at clip transitions. Eliminates foot-sliding that crossfades cause.
4. **Motion warping** — Warp root motion to align with gameplay targets (vault hand → ledge, punch → enemy position). Builds on existing bone manipulation API.
5. **Data-driven ragdoll** — Animation-driven joint limits. Sample current pose for per-joint limit ranges → more natural ragdoll falls.
6. **Blend tree integration** — Motion matching handles locomotion base layer; blend trees/state machines handle upper body, combat, interaction overlays.

**Key files:** `Animation.h/.cpp`, `RagdollSystem.cpp`
**New:** `MotionMatching.h`, `Inertialization.h`, `MotionWarping.h`

**Scalability:** Search budget, not search space. Low-end: 10K poses, 6 features. High-end: 100K poses, 12 features. KD-tree with early termination. Memory-limited platforms: fewer clips, fall back to existing blend trees.

---

## Phase 8: Editor Process Isolation (6-8 weeks)

**Why:** Glacier separates editor and engine into different processes. Engine crash → restart child process, no work lost. This comes last because all systems must be stable first.

**Deliverables:**
1. **Editor-runtime protocol** — Cross-process IPC (shared memory on desktop, WebSocket for Web/remote). Messages: entity CRUD, component modification, play/pause/stop, viewport camera sync, asset reload.
2. **Runtime child process** — Rendering loop in standalone child. Editor owns ImGui + scene editing state. Crash → restart child, resend scene.
3. **Viewport sharing** — Vulkan: `VK_KHR_external_memory` for zero-copy. Fallback: compress via shared memory.
4. **Multi-platform runtime connection** — Editor connects to runtime on different machine/platform (browser WebGPU, Android device) via WebSocket transport.
5. **Scene state persistence** — Editor maintains shadow JSON copy. On crash, shadow copy → new runtime. Zero data loss.
6. **Play mode hot-switch** — Edit/play mode without restarting runtime. Edit = paused physics + continued rendering.

**Key files:** `Editor/src/main.cpp`, `Player/`, `SceneSerializer.cpp`, `RenderTarget.h`
**New:** `EditorProtocol.h`, `RuntimeProcess.cpp`, `EditorBridge.h`

**Scalability:** Communication cost is O(changes), not O(scene). Delta protocol. 2M entities ≠ 2M messages. Low-bandwidth: downscaled viewport frames.

---

## Phase Dependency Graph

```
Phase 1 (Shaders + Shadows)
    │
    ▼
Phase 2 (Software DDGI) ────────────┐
    │                                │
    ▼                                ▼
Phase 3 (Async Streaming)    Phase 5 (Volumetric Fog)
    │                                │
    ▼                                ▼
Phase 4 (ECS 2M Entities)   Phase 6 (GPU Particles + Full Lighting)
    │                                │
    └────────────────┬───────────────┘
                     ▼
             Phase 7 (Motion Matching)
                     │
                     ▼
             Phase 8 (Editor Isolation)
```

Phases 3-4 (streaming/ECS track) and 5-6 (rendering/effects track) run in parallel after Phase 2.

## Timeline

| Phase | Effort | Cumulative |
|-------|--------|------------|
| 1. Shaders + Shadows | 6-8 weeks | 6-8 weeks |
| 2. Software DDGI | 8-10 weeks | 14-18 weeks |
| 3. Async Streaming | 6-8 weeks | 20-26 weeks (parallel with 5) |
| 4. ECS 2M Entities | 5-7 weeks | 25-33 weeks (parallel with 6) |
| 5. Volumetric Fog | 8-10 weeks | 22-28 weeks (parallel with 3) |
| 6. GPU Particles + Lighting | 8-10 weeks | 30-38 weeks (parallel with 4) |
| 7. Motion Matching | 8-10 weeks | 38-48 weeks |
| 8. Editor Isolation | 6-8 weeks | 44-56 weeks |

**Critical path:** ~44-56 weeks (~10-13 months) with parallel tracks.

## Verification

Each phase ships with:
- New test suites added to CTest (existing 82 targets + ~1100 cases must continue passing)
- Visual regression screenshots for rendering changes
- Performance benchmark capturing frame time before/after
- WebGPU build verified (no Vulkan-only regressions)
- `docs/ROADMAP.md` updated with completed items

---

# Reference: recreating desktop effects on web

Merged from WEB_EFFECT_RECREATION.md.

> Measured 2026-09-05. `build-web` had **no CMAKE_BUILD_TYPE**, so every web
> build shipped unoptimised. Setting Release took the wasm from 12.0 MB to
> **11.25 MB** - only 6%, far less than the 3-5 MB that was predicted, because
> the binary is dominated by what is LINKED, not by codegen. `assimp` (FBX,
> Collada, glTF parsers) and `imgui` are both live in the player wasm; strings
> for `Deformer.Fbx`, `Collada: Found unsupported <bind>` and `imgui_log.txt`
> are all in the shipped binary. A player that loads pre-packed assets needs
> neither. That, and `-Os` over `-O3`, are the real size levers. The Release
> switch is still worth keeping for the runtime speed.

`python tools/shader_parity.py` reports the split:

```
GLSL (desktop / Vulkan) : 60 effects, 74 stage files
WGSL (web / WebGPU)     : 13 effects, 25 stages
compute shaders         : desktop 36, web 0
```

Fifty-one effects have no web implementation (fifty-two before `outline`
landed). The instinct is to port them. That is the wrong instinct, and this
document is the argument for why plus what to do instead.

## The reframe

Most of the 52 exist to make **physically based rendering fast at scale**: ray
traced GI, visibility buffers, GPU culling of a hundred thousand draws, virtual
texturing, variable rate shading. They are answers to problems a stylised game
does not have.

Whistland packs to **130 KB**. Its scene is 79 entities, 3,162 triangles, flat
shaded vertex colour, one directional light. Porting ReSTIR to that is not an
optimisation, it is a category error.

Thirty-six of the desktop shaders are compute. WebGPU compute exists but the web
renderer has none, and the effects that need it are exactly the ones a cel-shaded
game does not want. So the useful question is not "how do we port these" but
**"which of these is the art direction actually asking for, and what is the
cheapest thing that reads the same?"**

The answer is about nine of them, and every one is fragment or vertex only.

---

## Bucket 1 — art direction critical

These change how the game LOOKS. Missing them is why web would not match.

### `outline` -> inverted hull. SHIPPED
The single most important one. A Wind Waker read needs outlines and there was no
web implementation at all.

Worse than missing: `SceneRenderSettings::ApplyToRuntime` already wrote
`geometryOutlinesEnabled` / `Width` / `Color` into the web RenderSystem, and the
scene file already round-tripped them. Nothing on web read them. A cel-shaded
project exported to the browser lost its outlines and no runtime said a word.

What shipped is the inverted hull: `OUTLINE_WGSL` draws the mesh again with
front faces culled, vertices pushed along their object-space normal by the
outline width, in flat colour. Vertex shader only, no postprocess pass, no depth
prepass, no new render target, and it follows the skeleton so an animated
character's outline does not stay behind in the bind pose. It reuses the main
pass's frame and object bind group layouts by repurposing `ObjectData.baseColor`
as the outline colour and `.metallic` as the width, which is the same trick
outline.vert plays with push constants on Vulkan, so the two paths cannot drift
apart in what they read. Consecutive entities sharing a mesh instance into one
draw, so an outlined crowd costs one call, not one per body.

Priority order matches RenderOutlinePass exactly: per-material, then a cel
ArtStyle override, then the global setting, with a hover highlight beating all
three. That last one means `HoverHighlightComponent` now works on web.

The other route, **edge detect** in the postprocess pass, is still open and
still worth it later: it catches the interior edges a hull misses. It needs a
normal target, which the web MRT pass does not have yet.

### `skinning` -> vertex shader skinning. ALREADY TRUE
Listed here first time round as missing. It is not. `PBR_WGSL` already does the
four-weight matrix palette in the vertex shader, gated on FLAG_SKINNED, and the
web vertex layout carries `boneWeights` / `boneIndices`.

What the parity report counts is `skinning.comp`, and that is a compute shader
with no web path (`--compute-skinning` is a desktop flag). The report is
counting files, so a desktop effect implemented inline on web reads as a gap.
Worth remembering when reading the 52: some of them are already answered under a
different name, which is the same trap the 2026-08-28 audit hit.

Vulkan skins with eight weights (two sets of four), web with four. That is a
real difference and it will show on a mesh authored past four influences.

### `oit_composite` -> weighted blended OIT. WIRED
Sails, water, foam and spray all need transparency that does not pop when it
sorts wrong. Two blended surfaces that intersect have no correct per-object
order, so the order flips as the camera moves and the image jumps. Weighted
blended OIT (McGuire/Bavoil) accumulates instead of sorting, and cannot pop
because there is no order to get wrong.

PBR_WGSL's fragment entry point was five hundred lines with the shading inline.
It is now `shadeSurface()`, with `fs_main` a one-line delegate and `fs_oit`
the second entry point, so a transparent surface is lit by the same code as an
opaque one and the two cannot drift.

The frame gained two passes after the scene pass. Accumulation writes
premultiplied colour times the paper's depth weight into an RGBA16Float target
(additive) and alpha into an R8Unorm revealage target (dst *= 1 - src, cleared
to 1). The composite resolves the pair back over the opaque scene with
src = OneMinusSrcAlpha, dst = SrcAlpha, which is the paper's formula evaluated
by the blender. Depth is loaded and never written, so transparency is hidden by
opaque geometry in front of it and never hides other transparency.

Three things worth knowing:

- **It is lazy.** The targets and pipelines are built the first frame a scene
  actually contains a blended entity. A fully opaque scene allocates nothing
  and renders exactly as before. If the targets fail to build, the draw loop
  keeps drawing blended entities the old sorted way rather than dropping them.
- **Pass order is load-bearing.** It runs after the scene pass, which is after
  the sky. The sky is a z = 1 fullscreen triangle, so transparency drawn before
  it is painted over.
- **`GPURenderPipelineDesc` had to grow.** It hardcoded `fs_main` and carried
  one `colorFormat` and one `blendState`, and OIT's two targets blend
  differently by definition. It now takes a `fragmentEntryPoint` and a list of
  extra colour targets. Note for whoever touches the pipeline manager next: a
  `WGPUColorTargetState` holds a POINTER to its blend state, so the vector of
  blend states is reserved up front - a reallocation there would leave every
  target pointing at freed memory.

**Unverified.** It compiles, it boots, and no demo scene has been checked with
real transparency in front of a human yet.

---

## Bucket 2 — the perf levers

### `upscale_*` -> render scale + CAS. ALREADY SHIPPED
The biggest web performance win available, and it was already in the tree before
this document was written. `RenderSystem::ApplyRenderScale` clamps to 0.5 - 1.0,
sizes the web scene target through `WebScaledDim`, and turns the sharpener on
below 1.0; `sharpenCAS` in POSTPROCESS_WGSL is a four tap contrast adaptive
sharpen with a clamp to the local min/max, so it does not halo.

EASU is a complex gather and is still not worth porting. At 0.7x you draw half
the pixels, which is why nothing else on this list comes close.

### `cull`, `cull_hiz`, `hiz_generate`, `light_cluster_*` -> CPU cull
GPU culling exists so a hundred thousand draws can be culled in a compute pass.
Web scenes will not have a hundred thousand draws. A CPU frustum cull over a
coarse spatial grid costs microseconds at these counts and needs no shader at
all. Light clustering matters past a few dozen lights; until then, a plain loop.

### `taa_resolve` -> FXAA, or supersample
TAA needs motion vectors, a history buffer and a reprojection pass, and it ghosts
badly on high contrast stylised content, which is exactly what a cel look is.
Use FXAA (one fragment shader) or, where there is headroom, render scale above
1.0 and downsample. The second is better looking and simpler.

---

## Bucket 3 — mood, with analytic stand ins

### `volumetric_fog` -> analytic height fog + radial blur
A raymarched froxel volume is compute. Analytic exponential height fog is a few
lines in the fragment shader and **is already there**: PBR_WGSL reads
`fogParams` and applies height-based distance fog, matching Vulkan.

What is missing is the god rays. A radial blur from the projected sun position
over a bright pass, fragment only, and for open water it reads better than a
froxel grid anyway.

### The GI stack -> hemisphere ambient
`ddgi_probe_update`, `ddgi_sample`, `restir_initial/spatial/temporal`,
`surfel_lookup/placement/update`, `radiance_cache_read/update`,
`svgf_atrous/temporal/variance`, `rt_composite`, `rt_hybrid_apply`,
`rt_temporal_reuse`, `adaptive_ray_budget`, `adaptive_ray_variance`.

**Seventeen of the fifty-two are one feature**: ray traced global illumination
with denoising. A cel shaded game does not want it. Physically correct bounce
light fights flat colour and hard shading bands; it is the opposite of the look.

Replaced with **hemisphere ambient**, already shipped: PBR_WGSL builds a sky
dome irradiance from the scene's configured sky and mixes it into the flat
ambient term by the surface normal's Y. It mixes rather than replaces, anchored
to the scene's tuned ambient level, because replacing it made sky-configured
builds read dimmer than flat-ambient ones. No probes, no rays, no denoiser. If
more is wanted later, bake spherical harmonic probes on the CPU at load time.

### `weather_particle` -> instanced billboards
Rain and snow through the existing particle system, which already runs on web
(`WebGPUParticleSystem`, max 65,536 particles). No new shader.

---

## Bucket 4 — do not port

`gpu_voxelize`, `vt_feedback`, `vt_resolve`, `vrs_generate`, `dgc_generate`,
`material_resolve`, `visibility`, `splat`, and everything already folded into the
GI item above.

Virtual texturing, visibility buffers, variable rate shading and device generated
commands are answers to scale problems. They cost more to maintain on a second
backend than they can possibly return on a game that fits in 130 KB.

---

## Order of work

1. ~~**Render scale + CAS.**~~ DONE, and it was done before this document was
   written. `RenderSystem::ApplyRenderScale` sizes the web scene target and
   turns the sharpener on below 1.0; `sharpenCAS` is in POSTPROCESS_WGSL.
2. ~~**Outline (inverted hull).**~~ DONE. See the bucket 1 entry.
3. ~~**Hemisphere ambient.**~~ DONE, also before this document. PBR_WGSL mixes a
   sky-dome irradiance into the flat ambient term when the scene has a
   configured sky.
4. ~~**Weighted blended OIT.**~~ WIRED. See the bucket 1 entry.
5. ~~**Vertex skinning.**~~ ALREADY TRUE, see above. Eight-weight parity is the
   only piece left and no content needs it yet.
6. ~~**Analytic fog.**~~ DONE, height-based distance fog is in PBR_WGSL.
   **Radial god rays** are not, and are still worth one fragment pass.
7. **CPU cull.** Only when draw counts justify it.

So the list was three items long, not seven, and all three are closed.
Check what the web renderer already does before adding to it: the audit habit of
grepping for the FEATURE rather than the SHADER NAME would have caught three of
these before they were written down as work.

Every one is fragment or vertex only. None needs compute. Together they are
perhaps a tenth of the work of porting the 52, and they get closer to the
intended look than the 52 would, because the 52 are built for a different look.

## The web renderer never freed GPU memory

Found 2026-09-05, chasing a live demo that froze in Firefox with:

```
Uncaptured WebGPU error: Not enough memory left.
Uncaptured WebGPU error: In a set_bind_group command, caused by: BindGroup with '' label is invalid
Uncaptured WebGPU error: Buffer with '' label is invalid
```

`WebGPURenderer::DestroyBuffer` called `wgpuBufferRelease` and
`DestroyTexture` called `wgpuTextureRelease`. Neither `wgpuBufferDestroy` nor
`wgpuTextureDestroy` appeared anywhere in the engine. Release only drops the
JavaScript reference; the GPU allocation behind it survives until the garbage
collector runs, and the collector is looking at a tiny wrapper object rather
than the megabytes it owns, so it is in no hurry.

A web frame creates on the order of a hundred transient buffers, one per shadow
caster and one per draw batch, each with a bind group. The GPU budget is gone
long before anything triggers a collection, allocations start failing, and
every bind group built on a failed buffer is invalid. That is the cascade
above, in order.

The fix cannot destroy in place. Measured against Dawn:

```
ok     destroy AFTER submit
ERROR  destroy BEFORE submit: [Buffer (unlabeled)] used in submit while destroyed.
```

and the transient buffers are destroyed mid-frame by design. So destroys are
queued and drained at the top of the next frame, once the submit that used them
has happened. `RenderSystem`'s web `Update` now also logs live and created
buffer counts once a second, because the report arrived with no way to tell a
leak from ordinary churn: a flat live count next to a large total is recycling
working as intended, a climbing live count is a leak.

Not yet confirmed against the original failure. The mechanism is proven and the
fix is deployed; whether it is the whole of what froze that session is open
until someone runs it again.

### The other half: not allocating in the first place

Reclaiming properly still left the frame creating and destroying roughly a
hundred buffers and a hundred bind groups. `m_WebObjectArrayBuf` /
`m_WebObjectArrayBG` / `m_WebObjectArrayGen` were sitting in the header with a
comment describing exactly the design that removes it, alongside
`EntityRenderData::objBoneBindGroup` and `objBoneBindGroupGen` for the skinned
case, and not one of the five was referenced anywhere but shutdown. The
optimisation had been written, documented, and then lost.

It is back. The frame's ObjectData is packed into one persistent storage
buffer, grown on demand, uploaded once, bound once. Records are reordered into
draw order first so an instanced batch is contiguous, and each draw addresses
its slice with `firstInstance`. Skinned entities are the one case that cannot
share the bind group, because binding 1 is their own bone buffer; theirs is
cached on the entity and rebuilt only when the shared buffer moves, which is
what the generation counter is for.

Two things this rests on, both measured against Dawn rather than assumed:

```
ok    firstInstance=0 -> instance_index=0
ok    firstInstance=3 -> instance_index=3
ok    firstInstance=7 -> instance_index=7
```

and the destroy-ordering result above. If `instance_index` had been 0-based per
draw, every entity would have read row zero and the whole scene would have
collapsed onto one transform.

The padding had to go with it. ObjectData was stride-256 because each entity
had its own uniform binding and 256 is `minUniformBufferOffsetAlignment`. A
storage array's stride is the struct size, so 256 would have made every index
read the wrong row.

The shadow passes went the same way. They were the worse offender: a buffer
and a bind group per caster for the directional pass, again per caster for
every spot light, and six more times per caster for a point light. Fifty-six
casters and one point light is over four hundred of each, per frame.
SHADOW_WGSL now reads a storage array of model matrices - the depth pass only
ever wanted the model matrix, so it carries matrices rather than a copy of the
full ObjectData - and the caster list is identical and identically ordered in
all three passes, so one upload serves every one of them and the row is just
the caster's index.

**Unverified.** This one is behaviour-visible: get `firstInstance` wrong and
every object renders with another object's transform. Dawn confirms the
semantics but nothing here has rendered a frame of it.

## Depth-driven post-processing

Scene depth is single-sample and readable by the post-process pass. The depth texture
carries TextureBinding usage alongside RenderAttachment, plus a second view with
`aspect = DepthOnly` - a depth-stencil format is viewed one aspect at a time to be read
in a shader. The pass binds it as `texture_depth_2d` and reads it with `textureLoad`, so
it needs no sampler.

Depth is linearised against the camera near and far planes, fed per frame.

- **SSAO** samples an eight-point ring in depth: a nearer neighbour is an occluder.
- **Cel outline** is a Sobel on linear depth, driven by the `celOutline*` fields in
  `SceneRenderSettings`. It catches interior edges; the inverted hull draws silhouettes.

**DoF, god rays and tilt-shift are unbuilt.**

## Keep the parity report honest

`python tools/shader_parity.py --strict` exits non-zero when a shared effect has
a stage on one backend and not the other. Run it before hand writing another
WGSL shader. The twelve shared effects are maintained twice by hand and nothing
else in the build compares them; two real divergences were found on 2026-09-04
and neither was deliberate.
