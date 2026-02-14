# Enjin Engine — Dev Journal

---

## 2026-02-14 (Session 15)

### Comprehensive Audit #4 — 51 Findings, 16 Fixed
Four parallel audit agents examined the entire codebase:

**Physics Backend Audit: PASS** — Jolt handles all 3D physics, Box2D handles all 2D physics, all SimplePhysics code properly guarded by `ENJIN_PHYSICS_SIMPLE`. Factory defaults correct, wiring complete in both PlayMode and Player.

**Fixes Applied (16):**
- PlayMode::Stop() — Added 7 missing SetBindings*(nullptr) calls (World, RenderSystem, DialogueSystem, CoroutineScheduler, EventBus, ScriptEngine, PostProcessing)
- Player::Shutdown() — Added 12 missing SetBindings*(nullptr) calls + s_VisualScriptWeather cleanup
- SceneSerializer — Added .contains() guards on 3 deserialization sites (NameComponent, TransformComponent, MeshComponent vertices)
- Dead Weather::WeatherSystem — Wrapped in #if 0 (superseded by Effects::WeatherSystem)
- VS Accessibility nodes — ShowSubtitle and AnnouncerAnnounce now call actual SubtitleSystem/Announcer via global pointers
- PlayMode — Wired s_VisualScriptSubtitleSystem and s_VisualScriptAnnouncer in Play()/Stop()
- Player — Added Water3D member, wired s_VisualScriptWater/SubtitleSystem/Announcer
- Audio Event Graph — Save/Load menu items now call Save()/Load() methods

**Documented (not fixed — require larger feature work):**
- OIT render passes are empty stubs (needs Vulkan pipeline)
- HTTP Client no-op on non-Windows (needs libcurl/WinHTTP abstraction)
- SWF Zlib decompression missing (needs zlib integration)
- DoF/Tilt-Shift no shader implementation (needs SPIR-V)
- SH Light Probe baking is fake (needs cubemap rendering)
- OIDN GPU-to-CPU copy not implemented (needs staging buffer)
- ASTC texture compression falls back to BC7 (needs ASTC library)
- UI Phase 2+ widget types render as grey boxes (large feature)
- Network auth/replay protection missing (security feature)

---

## 2026-02-14 (Session 14)

### Collaborative Editing UI
New `CollaborativeEditingUI.h/cpp` (122+936 lines). Wires the existing CollaborativeEditingSystem into the editor with full UI and scene integration. Host/Join session panel with IP/port inputs, peer list with colored dots and latency, connection status display. Remote operation handlers that apply 8 op types (Create/Delete/Rename/SetComponent/RemoveComponent/ModifyTransform/SetParent/Lock) to the local scene via SceneSerializer. Peer cursor visualization with colored selection rings and name labels in viewport. Conflict resolution modal dialog showing both versions with Accept Local/Remote/Merge buttons. Lock enforcement (CanEditEntity checks SceneLockManager). Scene sync callbacks for full-scene JSON serialization on join.

### Symbol Library Manager
New `SymbolLibrary.h/cpp` (223+1160 lines). Catalog-based symbol management system built on PrefabManager + VectorDrawingEditor. SymbolEntry with id, name, category, type (VectorDrawing/EntityPrefab/SpriteSheet/Custom), tags, thumbnail, use count. Symbol CRUD: create from entity hierarchy or vector drawing, delete, rename. Instantiation via PrefabManager. Nested symbol editing: isolated ECS::World for editing symbol contents, breadcrumb navigation back to main scene. Symbol browser panel with grid/list view toggle, category tabs, search bar, thumbnail preview (128x128). Update propagation: modifying a symbol updates all PrefabInstanceComponent instances. FlashTimeline integration via SyncToTimeline(). Catalog persisted as symbols/catalog.json.

### Newgrounds-Style Game Page
New `NewgroundsGamePage.h/cpp` (93+1752 lines). Enhanced HTML5 export producing a full Newgrounds-aesthetic game page. GamePageConfig with title, author, description, tags, thumbnail, NG app ID + encryption key, medal/scoreboard toggles, theme colors, canvas size, controls text. Generated page: dark theme (#1a1a2e bg, #e94560 accent), header with title + author + version, centered canvas with glow border, right sidebar with medal progress (locked/unlocked icons with grayscale filter) and scoreboard (top 10), controls section, description panel, responsive layout. CSS: custom properties, flexbox, card components, toast notifications, @media collapse at 900px. JavaScript: NG.io API init, medal fetch/display/unlock toasts, scoreboard fetch/post, fullscreen toggle, preloader with "Click to Play" audio resume. Embed codes: standard iframe + Newgrounds container div. XSS protection on all user text. ImGui config panel for build dialog.

---

## 2026-02-14 (Session 13)

### Non-Euclidean Geometry Rendering
New `NonEuclidean.h/cpp` (176+423 lines). `PortalComponent` with linked portal pairs, recursion depth, seamless transitions, momentum preservation. `PortalRenderer` performs stencil-buffer recursive portal rendering — for each visible portal, computes virtual camera through the portal pair (180-degree rotation + relative offset), applies oblique near-plane clipping to prevent seeing behind the destination. `SpaceWarpComponent` with 4 space types: Euclidean, Hyperbolic, Spherical, Toroidal. `NonEuclideanSystem` processes warp zones — hyperbolic translation via Poincare disk model, spherical via great circle arcs, toroidal seamless wrapping.

### Metaball / Blob Rendering
New `Metaballs.h/cpp` (153+668 lines). `MetaballComponent` per entity with radius, strength, threshold, groupId, color. `MetaballSystem` evaluates scalar field f(p) = sum(strength_i * r_i^2 / |p - c_i|^2), runs marching cubes on a 3D grid (16-64 resolution) to extract isosurface, computes gradient-based normals via central differences, blends colors by field contribution weights. Full marching cubes lookup tables (256 edge + triangle entries). Auto-centers grid on group centroid. Configurable update rate.

### Voxel Cone Tracing (VXGI)
New `VoxelConeTracing.h/cpp` (214+716 lines). `VoxelGrid` with mip chain generation — voxelizes scene by conservative triangle-AABB rasterization, injects direct lighting, averages 2x2x2 blocks for coarser mips. `ConeTracer` traces cones through the mip pyramid: samples at increasing mip levels as cone diameter grows. Computes diffuse GI (6 hemisphere cones), specular GI (1 reflection cone, aperture from roughness), ambient occlusion, and volumetric god rays. `VXGIConfig` with resolution (64/128/256), cone spread, trace distance, indirect multiplier.

### SDF Rendering (3D Vector Art)
New `SDFRenderer.h/cpp` (176+1037 lines). `MeshToSDF` converts polygon meshes to voxelized signed distance fields via closest-point-on-triangle computation with pseudo-normal sign determination. Trilinear-interpolated SDF evaluation. `SDFTextRenderer` generates resolution-independent text via 8SSEDT (8-point Sequential Signed Distance Transform) from binary glyph bitmaps — supports outline, drop shadow, smoothing. `SDFMeshRenderer` with sphere tracing (ray marching through SDF) and marching cubes isosurface extraction. SDF volume blending for smooth CSG.

### Framebuffer Feedback Effects
New `FramebufferFeedback.h/cpp` (149+502 lines). CPU-side RGBA8 ping-pong compositing. 8 presets: Echo (fading trails), Melt (downward UV shift), InfiniteMirror (recursive scale-down), VHSTracking (scanline offset + color bleed), Kaleidoscope (rotational symmetry), Phosphor (CRT persistence), DreamSequence (wavy distortion + glow). Full UV transform pipeline: offset, rotation, scale around pivot. 5 blend modes (Alpha, Additive, Multiply, Screen, Max). Post effects: blur, saturation, brightness, color tint per iteration.

### Screen-Space Distortion System
New `ScreenDistortion.h/cpp` (181+430 lines). 7 distortion types: HeatHaze, Shockwave, Underwater, PortalEdge, Ripple, BarrelFisheye, Custom (texture-based). Per-entity `DistortionSourceComponent` with strength, radius, frequency, speed, falloff. One-shot shockwave trigger with expanding radius, auto-destroy. `ScreenDistortionSystem` composites all sources into a `DistortionField` (2D UV offset grid), then remaps source pixels via bilinear sampling. Downscaled compute option.

### IK-Driven Mesh Deformation
New `SplineIKDeformer.h/cpp` (131+631 lines). `SplineIKComponent` with joint chain, FABRIK IK solver, Verlet physics simulation (gravity, damping, stiffness). Distance constraint solver with configurable iterations. Catmull-Rom spline subdivision for smooth joint interpolation. Two mesh modes: Tube (radial extrusion with taper) and Ribbon (flat strip). Joint rotation computation via LookRotation. Use cases: tentacles, ropes, tails, vines.

### Interactive Water (Wave Racer 64 Style)
New `InteractiveWater.h/cpp` (200+483 lines). Grid-based height-field water simulation with spring-damper wave propagation. `InteractiveWaterComponent` with configurable grid resolution (16-256), wave speed, damping, tension, shallow/deep/foam color blend. Object interaction: `WaterInteractorComponent` entities generate splashes and V-shaped wakes. Bilinear water height sampling for buoyancy. Absorbing/reflecting boundary modes. Animated UV scrolling, shoreline foam detection. Mesh generation with computed normals and vertex colors.

### Mesh Audio Reactivity via FFT
New `AudioReactive.h/cpp` (316+624 lines). `FFTAnalyzer` implements Cooley-Tukey radix-2 in-place FFT with Hanning window and bit-reversal permutation. Exponentially smoothed spectrum. Bass/mid/treble frequency band energy extraction. `AudioReactiveComponent` with 4 displacement axes (Normal, Radial, YUp, Custom) and 4 mapping modes (Uniform, HeightBased, RadialBased, UVBased). `MeshDisplacer` applies per-vertex displacement with normal recomputation. Test audio signal generator for preview. Smooth animation with configurable response/return speeds.

---

## 2026-02-14 (Session 12)

### Linux Platform Support
New `LinuxPlatform.h/cpp` with XDG path helpers (GetXDGConfigDir/DataDir/CacheDir), zenity/kdialog file dialogs, fork/exec process creation. Updated `Paths.h/cpp` with Linux-specific implementations using /proc/self/exe and XDG directories. `AppImageBuilder.h/cpp` generates AppDir structure and .desktop files for AppImage packaging. CMake Linux targets with pthread, dl, X11/wayland conditionals.

### Steam Deck Support
`SteamDeck.h/cpp` with auto-detection (SteamDeck env var / /sys/devices), adaptive quality recommendations based on thermal state, suspend/resume lifecycle hooks, gyro input stubs. `SteamInput.h/cpp` with Steam Input API stubs (ENJIN_STEAM guarded) for controller configuration, action sets, haptic feedback. `AdaptiveQuality.h/cpp` with 5 quality levels, FPS-based auto-adjustment, render scale/shadow/particle recommendations.

### Colorblind-Safe UI Palettes
`ColorblindPalette.h/cpp` with 9 palettes matching all colorblind modes. Each PaletteEntry has color + PatternType (None/Stripes/Dots/Crosshatch/Chevron) + icon string. GetPalette(ColorblindMode) returns full 6-color palette (primary, secondary, success, warning, error, info) with universally distinguishable patterns.

### One-Button Mode for UICanvas
Enhanced UISystem with switch access scanning — auto-cycles focus through focusable elements at configurable speed when m_SwitchAccessEnabled. Visual highlight pulse on scanned element. Single key press (Enter/Space) activates. m_SwitchScanIndex and m_SwitchScanTimer state tracked in UISystem.

### OpenDyslexic Font Option
`FontLibrary.h/cpp` with FontFamily enum (Default/Monospace/OpenDyslexic), FontLibraryConfig for letter/word/line spacing multipliers. Integrated with RuntimeAccessibilitySettings (fontFamily + spacing fields). Placeholder for embedded OpenDyslexic font data.

### Screen Reader Game UI Wiring
Announcer wired into UISystem via SetAnnouncerCallback() in both PlayMode::Play() and Player::Initialize(). Announces element type + label on focus change, activation events for buttons/checkboxes/sliders.

### Alternative Input Runtime Wiring
AlternativeInputManager added as members in PlayMode and Player. Update/RenderOverlay called per frame. Config loaded from accessibility.json. Scan targets connected from UISystem's focusable elements.

### Audio Visual Indicators Runtime
AudioVisualIndicatorSystem wired into PlayMode and Player with Update/RenderOverlay. Config loaded from accessibility.json. Auto-indicators triggered from SimpleAudio play callbacks.

### Content Warnings in Player
ContentWarningSystem in Player reads per-scene content flags from scene JSON. Shows dismissable overlay before game rendering. Any key press dismisses.

### Motor Accessibility UICanvas
Dwell-click and sticky drag support added to UISystem. m_DwellClickEnabled and m_DwellClickTime members track hover time on focusable elements. Exceeding dwell time triggers activation. Sticky drag locks sliders until explicit release. Config loaded from accessibility settings in PlayMode and Player.

### SWF Import & Conversion
`SWFConverter.h/cpp` converts parsed SWFDocument to ECS entities. Rasterizes shapes to PNG via SWFLoader::RasterizeShape(), creates Sprite2DComponent entities. MovieClip hierarchy → entity hierarchy with TimelineComponent keyframes. SWFMatrix → TransformComponent (twip→pixel, Y-down→Y-up). SWFColorTransform → material tint. Import options: rasterScale, importSounds, importTimelines.

### AS2/AS3 → AngelScript Transpiler
`AS3Transpiler.h/cpp` with pattern-based line-by-line transpilation. 15+ transformations: class extends→colon, typed vars/params, Number→float type mapping, MovieClip API→Flash shim calls, addEventListener→Events_Listen, trace→Print, package stripping, for-each conversion. Brace-tracking for scope. TranspileResult with stats.

### Flash API Shim Library
`FlashAPIShim.h/cpp` with RegisterFlashBindings() providing ~40 bound functions: DisplayObject (position/scale/rotation/alpha/visible), MovieClip (gotoAndPlay/Stop/currentFrame/totalFrames), Stage (stageWidth/Height/frameRate), Mouse/Keyboard input, TextField, Math (random/floor/ceil/etc.), Sound (play/stop/volume via SimpleAudio), Timer (setTimeout/setInterval/clearInterval).

### Nintendo Switch 1 NVN Backend Stub
`NVNBackend.h/cpp` stub implementing RenderBackend interface. All methods return false/nullptr with ENJIN_LOG_WARN. `SwitchPlatform.h/cpp` with Joy-Con detection, handheld/docked mode, touch screen, performance mode stubs. `ENJIN_PLATFORM_SWITCH` detection added to Platform.h. NVNCapabilities with Tegra X1 specs.

### Hub Application
New `Hub/` directory with standalone launcher executable. HubApplication class with project manager (scan/create/open), engine version manager, template browser, settings. `Hub/CMakeLists.txt` builds with GLFW + ImGui (OpenGL backend). ProjectEntry/EngineVersion/TemplateEntry data structures.

### Exotic Rendering — Fourier Transform Meshes
`FourierMesh.h/cpp` with DFT decomposition of 2D contour points. Fourier coefficients (amplitude, frequency, phase). Reconstruct(t, numTerms) evaluates series at parameter. GenerateApproximation for full contour. Animate() smoothly adds terms. GenerateMeshFromContour() triangulates 2D and extrudes to 3D.

### Exotic Rendering — 4D Stereographic Projection
`Projection4D.h/cpp` for visualizing 4D polytopes. Vector4D, Matrix4D with 6 rotation planes (XY/XZ/XW/YZ/YW/ZW). StereographicProject 4D→3D. 5 built-in polytopes: Tesseract (16v/32e), 5-Cell (5v/10e), 16-Cell (8v/24e), 24-Cell (24v/96e), 120-Cell (600v/1200e). AnimateRotation, GenerateWireframeMesh.

### Exotic Rendering — Inverse/Differentiable Rendering
`InverseRendering.h/cpp` for scene parameter optimization via gradient descent. OptimizableParam enum (light/material/camera params). ComputeLoss (MSE), ComputeGradient (finite differences), OptimizationStep. CPU-based: render, perturb, render, compute gradient. Per-pixel error map visualization.

---

## 2026-02-14 (Session 11)

### Reaction-Diffusion Simulation

New `ReactionDiffusion` system (`ReactionDiffusion.h/cpp`) implements the Gray-Scott model for Turing pattern generation on 2D grids. 9 presets (MitosisSpots, CoralGrowth, Fingerprints, Leopard, Labyrinth, WormHoles, BubblePacking, Spirals, Custom). Features: configurable feed/kill rates, diffusion coefficients, sub-stepping, circular/random seeding, bake-to-RGBA8 texture and heightmap export. Uses 5-point Laplacian stencil with toroidal boundary conditions.

### Cellular Automata as Geometry

New `CellularAutomataGeometry` system (`CellularAutomataGeometry.h/cpp`) converts CA grids into real-time 3D geometry. 7 rules (GameOfLife, HighLife, DayAndNight, Seeds, BriansBrain, Rule110, Diamoeba, Custom) with birth/survival bitmask configuration. 3 mesh generation modes: Voxels (greedy face culling), Marching Cubes (smooth isosurface with 256-entry edge/triangle tables), Point Cloud (icosphere per cell). Classic pattern stamps: Glider, Pulsar, Gosper Glider Gun.

### Physarum Slime Mold Simulation

New `PhysarumSimulation` system (`PhysarumSimulation.h/cpp`) implements agent-based Physarum polycephalum behavior. 50K+ agents sense, turn, move, and deposit trails on a 2D grid. Trail map diffuses and decays each step, creating organic self-organizing networks. 5 presets (ClassicSlime, BranchingNetwork, DenseWeb, Tendrils, Pulsating). Features: food source placement, circle/ring/point seeding, bilinear trail sampling, bake-to-RGBA8 and heightmap export.

### Flash-Style Timeline Editor

New `TimelineEditor` class (`TimelineEditor.h/cpp`) extends the existing `Timeline.h` system with keyframe authoring workflow. Features: layers (like Flash layers), property tracks with 4 interpolation modes (Constant, Linear, Bezier, Catmull-Rom), auto-key recording, curve editor with tangent handles, dopesheet view, onion skinning, copy/paste/delete keyframes, frame snapping. Animates transform.position, transform.rotation (Quaternion-safe), transform.scale, material.opacity, material.emissiveStrength, light.intensity, light.color, sprite.color.

### Asset Import Pipeline Wiring

Wired `ThumbnailGenerator` and `TextureCompressor` into the editor's asset browser. Thumbnail generation on hover with caching, right-click "Compress Texture..." context menu for image files, compression settings dialog (format, quality, mipmaps, sRGB), compression ratio preview. Texture formats: BC1/BC3/BC4/BC5/BC7 (desktop), ASTC 4x4/6x6/8x8 (mobile).

### Accessibility Runtime Wiring

Verified and completed accessibility settings wiring in Player app. All RuntimeAccessibilitySettings fields now properly applied: colorblind mode to PostProcessing, reduced motion to ControllerSystem/WeatherSystem/ParticleSystem, subtitle settings to SubtitleSystem, font scale to UISystem, dyslexia-friendly spacing. Settings loaded from JSON file on startup.

### Comprehensive Tutorial Book

Created `docs/TUTORIALS.md` — 55 tutorials + 3 appendices covering the entire engine feature set. Organized in 13 parts: Foundations, 2D Development, 3D Development, Scripting, Game Systems, AI/Pathfinding, Visual Effects, Audio, Procedural Generation, Animation/Timeline, Networking, Building/Distribution, Advanced Topics. Includes cross-feature interaction patterns (procedural dungeon+AI, living world, retro flash game, narrative RPG, physics puzzle).

---

## 2026-02-14 (Session 10)

### OIDN Denoiser for RT Pipeline

Added Intel Open Image Denoise as an alternative to the existing SVGF compute denoiser for the ray tracing pipeline. New `OIDNDenoiser` class (`OIDNDenoiser.h/cpp`) wraps the OIDN library with the same interface as `SVGFDenoiser`. CMake option `ENJIN_RAYTRACING_OIDN` (OFF by default) controls compilation. Editor UI in the Rendering panel adds a denoiser type selector (SVGF / OIDN) when RT is enabled.

### Dithered Gradient Rendering

Per-material flat-shaded banded lighting with dither transitions between bands. New fields on `MaterialComponent`: `ditherGradient` (bool), `ditherGradientBands` (2-8), `ditherGradientPattern` (6 patterns). Encoded in `surfaceParam1` push constant. The fragment shader quantizes luminance into discrete bands and applies the selected dither pattern at band boundaries for a stylized low-poly aesthetic.

### Font Library

Curated catalog of 42 OFL/Apache-licensed fonts across 8 categories (Sans-Serif, Serif, Monospace, Display, Handwriting, Pixel, Fantasy, Sci-Fi). New files: `FontLibrary.h/cpp` and `AssetLibrary.h/cpp`. Editor UI: font browser in Editor Settings > Fonts with search bar, category filter dropdown, and install status indicators.

### 2D/3D Asset Library

Curated CC0 asset catalog: 16 3D model packs (Kenney, Quaternius) and 15 2D sprite/tileset/UI packs. 14 categories: Architecture, Nature, Props, Characters, Vehicles, Weapons, Dungeon, Sci-Fi, UI Kits, Tilesets, Sprites, VFX, Backgrounds, Textures. Uses the same `AssetLibrary.h/cpp` framework as the font library. Editor UI: asset browser with search, category filter, and download/install workflow.

### Fractal Terrain & Advanced L-System

Extended `ProceduralAlgorithms.h/cpp` with two new algorithms:
- **Fractal Terrain:** fBm terrain generation with octave stacking and ridged multifractal variant. Hydraulic erosion via droplet simulation (configurable iterations, sediment capacity, evaporation). Thermal erosion via talus angle (material slumps when slope exceeds threshold).
- **Advanced L-System:** Full 3D turtle interpreter with yaw/pitch/roll commands, stochastic production rules (weighted random rule selection), and branch radius decay for realistic tree/plant generation.

### Fluid Simulation as Terrain

New `FluidTerrainCoupling` system (`FluidTerrainCoupling.h/cpp`) wires `FluidSimulation` density/velocity grids to `TerrainComponent` heightmaps. Two modes: erosion mode (fluid velocity erodes terrain height) and accumulate mode (fluid density builds terrain, e.g., lava cooling). Bidirectional coupling: terrain slope drives fluid flow direction. Configurable erosion rate, accumulation rate, and coupling strength.

### Source-App Import Presets UI

Editor dialog for model import presets targeting 10 DCC tools: Blender, Maya, 3ds Max, Houdini, Cinema 4D, ZBrush, Substance Painter, Unreal, Unity, SketchUp. Auto-detection from file metadata (FBX `Creator` field, glTF `generator` field). Per-axis flip toggles, texture search paths, and material slot remapping. Backend code already existed; this wires the UI.

### Template Creator Tool

Save current scene as a reusable startup template. New `TemplateCreator` class (`TemplateCreator.h/cpp`) with `SaveTemplate()`, `LoadTemplate()`, `ScanTemplates()`, `DeleteTemplate()`. Editor UI at View > Tools > Template Creator with metadata editing (name, description, category), save/load/delete buttons, and template list. Custom templates stored in `templates/` directory alongside the editor.

### Binary Distribution

CMake install rules and CPack configuration added to root `CMakeLists.txt` for Windows ZIP packaging. New scripts: `scripts/package.bat` (Windows) and `scripts/package.sh` (Linux/macOS) for one-command Release builds with packaging.

---

## 2026-02-14 (Session 9)

### Bug Fixes: Parent-Child Transforms, Weather Transitions, Delete Key

**Parent-Child World Transforms:**
`ComputeWorldMatrix()` added to `Hierarchy.h` — walks parent chain bottom-up (depth-capped at 64) to compute world matrices. 7 sites in `RenderSystem.cpp` (RenderToTarget, main pass, RenderEntity, RenderEntityShadow, GPU culling x2, RT acceleration structure) updated from local `ToMatrix()` to world matrix. `SpriteBatchRenderer.cpp` extracts world position/rotation/scale from the matrix for child sprites (root sprites keep the original fast path).

**Weather Particle Transition:**
When weather type changes (e.g., snow→rain from temperature), existing particles now have their lifetime capped to 0.5s for a smooth fade-out instead of lingering with stale size/speed properties.

**Delete Key Shortcut:**
Changed keyboard guard from `WantCaptureKeyboard` (true when ANY ImGui panel has focus) to `WantTextInput` (true only during text field editing). Delete, Ctrl+D, gizmo keys, and undo/redo now work after clicking in hierarchy/inspector.

### Water & HUD Visual Script Nodes

6 water nodes (SetStyle, SetWaveHeight, SetWaveSpeed, GetWaveHeight, SetOpacity, SetColor) and 2 HUD nodes (SetEnabled, IsEnabled) added. Fixed 2 existing weather VS node stubs (Set Weather, Set Fog) that were no-ops — they now call the weather system. All globals wired in PlayMode.cpp, EditorLayer.cpp, and Player/main.cpp.

### Roadmap Cleanup

All 31 feature accessibility gaps from the 2026-02-10 audit are now resolved and struck through in ROADMAP.md.

---

## 2026-02-14 (Session 8)

### UISystem Texture Resolver + Animation Graph Dual-Mode Editor

**UISystem Texture Resolver in Player:**
Wired `GetImGuiTexture()` cache in Player app so UICanvas Image widgets render textures in standalone builds. Pattern mirrors the Editor: `RenderSystem::LoadTexture()` → `ImGui_ImplVulkan_AddTexture()` → cache by path. Cleanup in `Shutdown()` via `ImGui_ImplVulkan_RemoveTexture()`.

**Animation Graph → AnimatorComponent:**
Extended `AnimationGraphEditor` from SM-only to dual-mode: when entity has `AnimatorComponent`, the editor targets its `AnimationStateMachine` (animation clips); otherwise falls back to `StateMachineComponent` (game logic SM).

Animator mode features:
- **Animation clip dropdown** from `SkeletalAnimator::GetAnimations()` (or text input if no clips loaded)
- **Speed slider**, play mode combo (Once/Loop/PingPong)
- **Transition inspector** with blend time, exit time, `TransitionCondition` types (Bool/Float/Int/Trigger)
- **ASM parameter system** — Bool/Float/Int parameters with add/edit UI
- **Play mode** — current state display, set-trigger button, state node highlighting
- All CRUD: add/delete states, create/delete transitions, rename states (re-keys map + updates references)
- Toolbar shows `[Animator]` / `[State Machine]` mode indicator

SM mode preserved unchanged (script callbacks, SMTransitionCondition types, SendTrigger).

---

## 2026-02-14 (Session 7)

### Player App Wiring Gaps Fixed

Comprehensive comparison of PlayMode vs Player revealed 7 wiring gaps. All fixed:

- **`s_VisualScriptSaveSystem` extern** — VS save/load/checkpoint/meta nodes now work in Player (was silently null)
- **HUDSystem::Update()** — HUD widgets (health bars, etc.) now render in Player Render() loop
- **QuestFlow ResetRuntimeState()** — Quest flow graphs properly initialized on scene load
- **CinematicSystem.SetEnabled(true)** — Cinematics now enabled in Player
- **3D collision → damage/pickup** — 3D physics collision enter events now trigger ProcessContactDamage and ProcessPickup in both PlayMode and Player (was only wired for 2D callbacks)
- **ObjectPool.DestroyAll()** — Pooled objects properly cleaned up on Player shutdown
- **CinematicSystem disabled on shutdown** — Clean disable on exit

Also updated ROADMAP.md: marked Player Weather/Save items as resolved.

---

## 2026-02-13 (Session 6)

### Wire IPhysicsBackend2D into ControllerSystem

Platformer2D controllers now use 2D physics raycasts for ground detection instead of requiring 3D BoxColliderComponents as a workaround.

- **ControllerSystem.h/cpp:** Added `SetPhysics2D()`, `m_Physics2D` member, and `CheckGround2D()` method that casts a downward 2D raycast via `IPhysicsBackend2D::Raycast()`
- **UpdatePlatformer2D:** Ground check now tries `CheckGround2D()` first (hits Body2DComponent), then `CheckGround()` (3D), then Y=0 fallback
- **PlayMode + Player:** Both wire `SetPhysics2D(m_Physics2D.get())` into the controller system
- **Templates:** Platformer ground/platforms/wall/moving-platform and TopDown2D walls/obstacles switched from `addBoxCollider3D` to `addBoxCollider2D` (Body2DComponent with proper half-extents)
- Player entities still have NO Body2DComponent — controllers handle movement kinematically

---

## 2026-02-13 (Session 5)

### Comprehensive Audit Fix Round — 132 of 152 Findings Resolved

Applied fixes for 132 of 152 findings from `docs/AUDIT_2026_02_13.md` across 6 parallel agents. All 6 build targets verified clean. 20 remaining findings are mostly LOW/API consistency items.

**Security (29 fixes):**
- Replaced all remaining `std::system()`/`popen()` on Linux/macOS with `posix_spawn()` (S-C1, S-H8)
- Collaborative editing: enum validation on EditOpType, NaN/Inf guard on remote transforms (S-C2, S-H4)
- Network payload >64KB now rejected instead of silently truncated via `static_cast<u16>` (S-C3, S-C4)
- NetworkId wraparound guard, audio handle overflow guard (S-H3, S-H9)
- All `std::stoi`/`std::stoul` in deserialization wrapped in try-catch (S-H5, S-H6, S-H7)
- Collaborative editing: capped pending ops, string length, log entries, scene sync allocation (S-H10, S-M1, S-M2, S-M6)
- WAV loader division-by-zero guards for bitsPerSample/channels/sampleRate (S-M3, S-M4)
- VOX loader integer overflow check (S-M5), save file JSON type checks (S-M7)
- Newgrounds ExtractString infinite loop fix (S-M8), HTTP truncation error flag (S-M9)
- HTML5 width/height validation (S-M13), SceneLock getenv null check (S-M14)
- Audio clip map cleanup, collab peer list bounds, network interpolation buffer cap, Newgrounds unescape (S-L1-L4)

**Stability (20 fixes):**
- EntityEventBus copy-before-dispatch prevents use-after-free (T-C4)
- ShaderGraph `nodeMap.at()` replaced with `find()` (T-C2), prefab recursion depth limit (T-C3)
- BehaviorTree near-zero distance guard (T-C5), AISystem flee zero-length normalization guard (T-H2)
- Spline single-point underflow guard (T-H1), FlashTimeline empty check (T-H4)
- Dialogue legacy bounds check (T-H5), Quaternion ToMatrix Shepperd div-by-zero guard (T-H6)
- AudioEventGraph thread_local RNG (T-H7), ObjectPool stale entity check (T-H8)
- Prefab JSON array validation (T-H9), SceneSerializer stoul overflow (T-H10)
- AIBehaviors empty waypoint guard (T-M1), Timeline reverse playback fix (T-M4)
- VisualScriptSystem deferred entity destruction (T-M11), Animation zero quaternion guard (T-M12)
- SubtitleSystem/Announcer fade div-by-zero guard (T-M6), CoroutineScheduler optimization (T-M13)
- FluidSimulation grid reallocation hysteresis (T-L4), SubtitleSystem zero viewport guard (T-L8)
- EditorLayer log format fix (T-L7)

**Performance (19 fixes):**
- `Quaternion::GetRotationZ()` — single `atan2` replacing full Euler decomposition in 12+ hot paths (P-1, P-5, P-21)
- `Quaternion::GetForward()/GetRight()/GetUp()` helpers eliminating `ToMatrix()` overhead (P-6, P-7, P-16)
- SpriteBatchRenderer: cached atlas region pointer + pre-hashed texture paths (P-2, P-3)
- RenderSystem: pre-classified entity component flags reducing optional `GetComponent()` calls (P-4)
- Per-frame vector allocations promoted to members: IK chain (P-13), TweenSystem callbacks (P-9), FlowerSystem events (P-19)
- Editor `GetAllEntities()` calls replaced with component queries (P-14)
- SpriteBatchRenderer texture string copy eliminated (P-15)
- DialogueSystem string construction reduced (P-11, P-12)
- Weather2D double iteration eliminated (P-17), AISystem debug lines bounded (P-18)

**Feature Wiring (12 fixes):**
- AISystem fully wired into PlayMode and Player — was 615+ lines of completely dead code (F-H1, F-H2)
- `SetBindingsPluginSystem` and `SetBindingsAudioGraphRuntime` now called in PlayMode (F-M1, F-M2)
- `InputActionMap.Update()` called in PlayMode (F-M3)
- Graph assets (.enjshader, .enjaudiopkg, .enjparticle) included in BuildPipeline (F-M6)
- FlashAPIShim bindings registered (F-L1), TweenRotation VS node registered (F-L2)
- FlowerSystem.SetGameCameraEntity called in PlayMode (F-L3)

**AngelScript Bindings (~180 new functions across 22 component types):**
- Rigidbody, BoxCollider, SphereCollider, CapsuleCollider, TriggerZone, Interactable, Pickup, Inventory, Timer, Lock, Switch, GoalZone, Conveyor, Teleporter, MovingPlatform, Checkpoint, DamageZone, SpawnPoint, WaypointPath, LOD, Layer, Tag — all now scriptable from AngelScript

---

## 2026-02-13 (Session 4)

### Startup Template Polish Pass — All 43 Templates Improved ~50%

Comprehensive improvement pass across all 43 startup templates in `ApplyTemplate()`. Each template received 3-10 new entities, components, atmosphere, and gameplay polish to better showcase engine features out of the box.

**Tier 1 (SPARSE templates — major additions, 5-10 entities each):**
- **Isometric:** Added buildings, NPCs, chest, lantern entities
- **Visual Script:** Moving platform, door+switch puzzle, score counter, particle trigger
- **UI Canvas:** 5 UICanvas elements (health panel, progress bar, score label, ammo counter, pause button)
- **Bullet Hell:** 3 enemy types, power-up, score display, parallax background layer, bloom post-processing
- **Idle/Clicker:** UICanvas HUD (currency, click power, upgrade, auto-click buttons), trophy markers, click burst particles, decorations
- **Point & Click:** Inventory key item, locked door, dialogue box, cursor indicator, UICanvas inventory panel

**Tier 2 (GOOD templates — 3-5 additions each):**
- **Platformer:** Wall-jump wall, moving platform with tween, 2nd enemy type
- **Top-Down 2D:** Obstacles, speed boost pickup, particle effects on enemies
- **Third Person:** Ramp geometry, collectible coin with bob tween, fog atmosphere
- **First Person:** Interactable door, flashlight spotlight, ambient sound source
- **RPG Village:** Fountain with particle effects, 2nd NPC, quest item
- **Narrative:** Interactable objects, firefly particles, point light atmosphere
- **Save System:** Danger zone, checkpoint particles, score text display
- **PS1 RPG:** Treasure chest, dungeon entrance, battle arena trigger zone
- **Visual Novel:** Name plate + speaker name labels, 3 choice buttons
- **Game Manager:** Enemy spawner entity, sample enemy, wave counter display

**Tier 2B (already-good templates — small targeted additions):**
- **City Builder:** Population/income/funds HUD labels
- **FPS Arena:** Crosshair + kill feed HUD elements
- **Team Sports:** Center circle + center line field markings
- **Tower Defense:** Sample turret + creep enemy entities
- **Runner:** High obstacles, speed boost, shield power-up
- **Flower:** Ambient bee particles, second smaller flower
- **Fixed Camera:** 2nd camera zone, corridor gem collectible

**Tier 2C (restored templates — filling gaps):**
- **Metroidvania:** Save station, spike trap hazard
- **Vampire Survivor:** Level-up zone, wave counter display
- **Roguelike:** Spike trap, health potion pickup
- **Soulslike:** Soul fragment pickup, bloodstain corpse run marker
- **Couch Co-op:** Shared power-up, treasure chest cooperative objective
- **Shadow Test:** Archway (pillars+lintel), rotated cube, point+spot lights

**Flash templates:**
- **Flash TD:** Creep enemy, gold/wave/lives HUD labels
- **Flash Dress-Up:** Background entity, save/clear buttons
- **Flash Escape Room:** Inventory bar + 5 slots, hint text label
- **Flash Rhythm:** Background, judgment line, score/combo/judgment text

---

## 2026-02-13 (Session 3)

### Physics Phase 4-5: Production Backends Enabled, SimplePhysics Retired

Completed the final two phases of the 5-phase physics backend migration. Jolt and Box2D are now ON by default in CMake; SimplePhysics is behind a compile guard and can be disabled entirely.

**Phase 4 — Enable Production Backends:**
- Extracted shared physics data types into `PhysicsTypes.h` (6 structs: AABB, CollisionResult, Ray, RaycastHit, CollisionEvent, ColliderInfo) and `PhysicsTypes2D.h` (10 types: shapes, Body2DComponent, Joint2DComponent, Contact2D, RayHit2D). Interfaces (`IPhysicsBackend.h`, `IPhysicsBackend2D.h`) no longer depend on SimplePhysics headers.
- Added `Simple = 3` to `PhysicsBackendType` enum, updated validation bounds in SceneManager and Player.
- Changed CMake defaults: `ENJIN_PHYSICS_JOLT=ON`, `ENJIN_PHYSICS_BOX2D=ON`.
- Updated `PhysicsBackendFactory` with `Simple` type handling, dimension mismatch warnings (Box2D for 3D, Jolt for 2D), fallback logging, and helper functions (`IsJoltAvailable()`, `IsBox2DAvailable()`, `IsSimpleAvailable()`, `ResolveBackendName()`).
- Editor Project Settings physics UI expanded to 4-option combo (Auto/Jolt/Box2D/Simple) with resolved backend name display and compile-time availability indicators.
- Added null guard on gravity UI when no physics backend is active.

**Phase 5 — Retire SimplePhysics Behind Compile Guard:**
- Added `ENJIN_PHYSICS_SIMPLE` CMake option (ON by default for backward compat).
- Wrapped 10 source files with `#ifdef ENJIN_PHYSICS_SIMPLE`: `SimplePhysics.h/.cpp`, `SimplePhysicsBackend.h/.cpp`, `SimplePhysicsBackend2D.h/.cpp`, `ConstraintSolver.h/.cpp`, `Physics2D.h/.cpp`, `PhysicsWorld.h/.cpp`.
- Factory returns `nullptr` + error log when SIMPLE=OFF and no production backend matches.
- StressTest physics benchmarks guarded — skipped with info message when SIMPLE=OFF.
- Null-safety audit: all `m_Physics->` / `m_Physics2D->` dereferences already properly guarded across PlayMode, Player, EditorLayer, ControllerSystem, ScriptBindings, VisualScriptExecutor, NodeRegistry.

**New files:** `PhysicsTypes.h`, `PhysicsTypes2D.h`
**Modified:** 24 files (headers, sources, CMake, editor UI, factory, tests, docs)
**Build verified:** Config A (Jolt=ON Box2D=ON Simple=ON) — all 6 targets. Config B (Simple=OFF) — all 6 targets.

---

## 2026-02-13 (Session 2)

### Graph System Full Implementations + RT Pipeline Wiring + Plugin SDK + Collaborative Editing

Six skeletal systems promoted to full implementations, two bug fixes applied. Build clean, all 6 targets verified.

**Bug Fix: Template Hover Popup** — Consolidated duplicate template arrays into shared `s_BuiltinTemplates[]` at file scope. Hover preview now indexes the correct array, fixing mismatched tooltip content.

**Bug Fix: Shadow Offset in Editor Game View** — Added `SelectShadowLights()` call with the game camera in `RenderShadowPassForCamera()`. Clamped `m_ShadowDistance` to camera far plane in the offscreen rendering path, fixing shadow cascade misalignment.

**Collaborative Editing Wiring** — Wired collab callbacks in `EditorLayer::Initialize()` (remote edit, scene sync request/received). Added edit recording at 5 key edit points (entity creation x2, deletion, rename, gizmo transform). Added collab status indicator in menu bar and `m_CollabSystem.Update(deltaTime)` in `EditorLayer::Update()`.

**Shader Graph: Full GLSL Code Generation** — Replaced `GenerateGLSL()` stub with full topological sort and per-node GLSL emission for all 54 node types. Save/load in `.enjshader` JSON format. GLSL code display window with error/success status. EditorLayer wiring: View > Tools > Shader Graph menu entry with graph editor rendering.

**Audio Event Graph: Full Runtime Execution** — `AudioEventGraphRuntime` class with `TriggerEvent()`, `SetParameter()`, `GetParameter()`, `StopAll()`, `Update()`. Graph execution walks from trigger nodes through processing chain (Volume/Pitch/Pan/Delay) to source nodes (SoundClip/RandomClip/SequenceClip), plays via SimpleAudio. Parameter triggers with threshold crossing. Delayed sound scheduling. Save/load in `.enjaudiopkg` JSON format. 4 AngelScript bindings (`AudioGraph_TriggerEvent`, `AudioGraph_SetParameter`, `AudioGraph_GetParameter`, `AudioGraph_StopAll`). 3 visual script nodes (AudioGraph_TriggerEvent, AudioGraph_SetParameter, AudioGraph_StopAll).

**Particle Graph: Full Compiler to Component** — `ParticleGraphCompiler::Compile()` converts graph nodes to `ParticleEmitterComponent` fields. Maps emitter types to EmitterShape (Point/Sphere/Box/Cone), modifiers (Gravity, Drag, SizeOverLife, SpeedOverLife, RotationOverLife), controls (Burst, Loop, Delay), and renderers (Billboard, VelocityStretch). "Apply to Selected Entity" button with compile feedback. Save/load in `.enjparticle` JSON format.

**Ray Tracing Pipeline Completion** — Added `CompositeRTResults(commandBuffer)` call after denoising. Wired real depth buffer to RT descriptor binding 2. Camera change detection for path tracer accumulation reset. Replaced `DenoiseRTOutputs()` stub with real SVGF calls (temporal, variance, a-trous wavelet passes).

**Plugin System Enhancement** — `PluginContext` struct providing World, RenderSystem, ScriptEngine, SimpleAudio, SceneManager to plugins. `IPlugin::OnLoad(PluginContext&)` context-aware overload. `IPlugin::OnSaveState/OnRestoreState` for hot-reload state preservation. `PluginSDK.h` single-header include with `ENJIN_IMPLEMENT_PLUGIN()` macro. 4 AngelScript bindings (`Plugin_IsLoaded`, `Plugin_GetVersion`, `Plugin_Load`, `Plugin_Unload`). 3 visual script nodes (Plugin_IsLoaded, Plugin_Load, Plugin_Unload). Example plugin in `examples/ExamplePlugin/`.

---

## 2026-02-13

### OIT, SH Light Probes, SDF Scene, and Parallax Occlusion Mapping

Added three new rendering subsystems and advanced material mapping:

**Order-Independent Transparency (OIT):** Weighted Blended OIT (McGuire & Bavoil 2013) with RGBA16F accumulation texture and R8 revealage texture. Full Vulkan resource management (create/destroy/resize). Render pass stubs (BeginTransparentPass/EndTransparentPass/CompositePass) ready for composite shader SPIR-V. Configurable weight function (depth-based, alpha-based, combined). Editor toggle in Rendering panel.

**SH Light Probes:** L2 spherical harmonics lighting system with 9 coefficients per RGB channel (27 floats per probe). `SHProbeGrid` for axis-aligned bounding box coverage with configurable resolution. `GenerateGridProbes()` fills the grid with evenly spaced probes. `BakeProbe()` initializes L0 band with ambient light (stub for full cubemap sampling). `GetIrradiance()` queries nearest-probe irradiance at any world position. Full JSON serialization for scene persistence. Editor UI: grid bounds/resolution sliders, Generate/Bake/Clear buttons, probe count display.

**SDF Scene:** CPU-side signed distance field evaluation with 6 primitive types (Sphere, Box, Cylinder, Torus, Plane, RoundedBox) and 6 boolean operations (Union, Subtract, Intersect + smooth variants with configurable smoothness). `SDFObjectGPU` struct (48 bytes, `alignas(16)`) for shader upload. Transform support (position, rotation via conjugate quaternion, scale). Material properties (color, metallic, roughness) per object. Editor UI: Add Sphere/Box buttons, object count, Clear.

**Parallax Occlusion Mapping:** Extended `MaterialComponent` with `parallaxMode` (Basic/Steep/Occlusion Mapping/Relief Mapping), `pomMaxSteps` (8-128), and `pomHeightScale` (0-0.3). Inspector UI: mode dropdown, conditional step/scale controls for POM modes. Full serialization with validated deserialization.

All three systems instantiated in `RenderSystem::Initialize()`, cleaned up in `Shutdown()`.

### Depth of Field and Tilt-Shift Post-Processing

Added DOF and tilt-shift post-processing infrastructure to the rendering pipeline:

**Depth of Field:** Focal distance, focal range, near/far blur strength, bokeh size, aperture shape (Circle/Hexagon/Octagon), CoC debug visualization mode. Full `PostProcessSettings` UBO fields and `SceneRenderSettings` config with JSON serialization. Editor UI in PostProcessing panel. Shader implementation pending SPIR-V compilation.

**Tilt-Shift:** Focus Y position, band width, blur amount controls for miniature/toy-model effect. Same serialization and editor UI treatment.

### Camera Presets

Added `CameraPreset` enum with 9 built-in presets: Isometric45, Isometric30, TopDown, SideScroller, FirstPerson, ThirdPerson, CinematicWide, SecurityCam, BirdsEye. `ApplyCameraPreset()` returns configured camera settings + recommended Euler rotation angles.

Inspector: Preset dropdown in Camera component header applies values immediately on selection. Script bindings: `Camera_ApplyPreset(entity, presetIndex)`, `Camera_GetPresetName(index)`.

### Accessibility: Dyslexia Mode, Reduced Motion, Colorblind-Safe Theme, Switch Access

**Dyslexia-friendly font infrastructure:** Letter spacing, word spacing, and line spacing fields on `UITheme` and `RuntimeAccessibilitySettings`. Editor toggle in Cognitive section.

**Reduced motion:** `SetReducedMotion()` on `UISystem` skips animations when enabled.

**ColorblindSafe theme:** New `UIThemePreset` using blue/orange palette universally distinguishable across all color vision types, avoids red/green for state indication.

**Switch access / one-button mode:** Auto-cycles focus through UICanvas elements at configurable scan speed, single input (Enter/Space/Gamepad-A) to select. Enables one-button gameplay for motor-impaired users.

### Procedural Generation Script Bindings and Visual Script Nodes

Exposed all 9 procedural generation algorithms to AngelScript (~15 bindings) and visual scripting (9 nodes under `NodeCategory::Procedural`): CellularAutomata, RandomWalker, BSP, DiamondSquare, LSystem, Voronoi, WFC, Grammar, PrefabAssemble. Plus result query functions (GetCell, GetHeight, GetWidth, GetGridHeight) and SpawnGrid entity creation. Wired `SetBindingsProcedural` into PlayMode and Player.

---

## 2026-02-12

### Comprehensive Audit #3 — Round 2: 38 More Fixes (All 83 Addressed)

Second pass addressing all remaining findings from audit #3. 38 more fixes across 31 files.

**Security (13 fixes):** FileDialog shell escaping on macOS/Linux (S1), network payload size validation (S11), save slot range validation (S13), xorshift32 PRNG replacing rand() globally (S14), Newgrounds save metadata slot mapping eliminates hash collisions (S15), PixelEditor sheet export size_t (S16), snapshot count cap 1024 (S17), shader compiler fork/exec on Unix (S18), IDE/folder/git shell escaping on Unix (S19/S20/S23), SDFGenerator dimension validation (S22). TODO comments for network auth (S12) and replay protection (S21).

**Stability (5 fixes):** Network connections reserve(MAX_PLAYERS+1) prevents pointer invalidation (T10), spline length table size guard (T13), navmesh triangulate + terrain safer loop idioms (T14/T15).

**Performance (11 fixes):** SimplePhysics AABB cache in RebuildColliderCache (P5/P15), VS executor guarded timestamps + callstack + reserved input vectors (P11-P13), NetworkSystem member send buffer (P14), scene composition mesh3D by subtraction (P20), SpriteBatch precomputed isAtlased + callback by ref (P21/P22), MeshComponent cached AABB (P23), RenderSystem cached light entity list (P24).

**Features (9 fixes):** SceneManager AssetReader integration for Player .enjpak scene loading (F5), Player data asset loading from pack (F10), PlayMode weather update + particle system wiring (F12/F13), PlayMode pause toggles streaming (F14), dead code/include cleanup (F17/F18), TODO comments for remaining items (F11/F15/F16/F19).

### Comprehensive Audit #3 — 40 Fixes Applied

Full codebase audit (83 findings documented in `docs/AUDIT_2026_02_12_R2.md`), 40 fixes applied across 31 files covering security, stability, performance, and feature gaps.

**Security (9 fixes):** Plugin path traversal validation (PluginSystem + PluginRepository), HTML5 CSS XSS sanitization, Newgrounds JSON string escaping, HTTP form URL encoding, VulkanImage/PixelEditor integer overflow guards, HotReload/VulkanShader 60s compile timeout (was INFINITE), NetworkSystem MessageType range validation.

**Stability (10 fixes):** Dialogue recursion depth limit (128), Player Update() physics null guard (was early-returning and skipping all gameplay), animation keyframe div-by-zero + size mismatch guards, SWFLoader shape record iteration limit, navmesh polyPath underflow guard + cellSize div-by-zero, cone mesh height=0 guard, custom template stoi try-catch, script range attribute stof try-catch.

**Performance (13 fixes):** Splitscreen pipeline/descriptor/viewport bind hoisted out of entity loop, SpriteBatchRenderer pipeline bind hoisted out of flush lambda, ControllerSystem ToMatrix() cached, ConstraintSolver ToEuler() cached, Box2DBackend 3 per-frame hash sets → member caches, Physics2D 2 per-frame hash sets → member caches, ClassifySceneComposition() .size() instead of counting loops, SpriteBatchRenderer per-sprite string copy → static const ref, Box2DBackend/Physics2D ToEuler+FromEuler → direct quaternion construction, EntityEventBus index-based iteration (no vector copy).

**Feature gaps (8 fixes):** PlayMode SimpleAudio init/update/shutdown, PlayMode DestructibleSystem init/update/shutdown, Accessibility script bindings declared in header + wired in PlayMode+Player (20 bindings now functional), 2D physics collision events dispatched to VS+scripts in PlayMode+Player, Player scene render settings applied after loading, Player FluidSimulation/WindSystem/WorldTime+SeasonalWeather systems wired.

### P2 Roadmap: Player Systems, Physics Debug, Accessibility

**Player App — 5 New Runtime Systems:**
Wired ParticleSystem (CPU emitter simulation), SubtitleSystem (with DialogueSystem integration and config from RuntimeAccessibilitySettings), AlternativeInputManager (update + overlay), AccessibilityAnnouncer (UISystem focus callback + status bar), and PostProcessing (initialized from swapchain render pass, wired to script bindings). Added RuntimeAccessibilitySettings member with reduced motion wiring to ControllerSystem and font scale to UISystem.

**Physics Debug Visualization:**
Added `m_ShowColliderWireframes` toggle in View menu. When enabled, draws wireframe overlays for all collider and joint entities: BoxCollider (yellow wireframe box), SphereCollider (green-yellow, 3 orthogonal wire circles), CapsuleCollider (orange, box + end circles per direction axis). Joint lines between connected entities: Distance (white), Hinge (cyan), BallSocket (magenta), Spring (green), Fixed (red), Slider (blue). Uses the existing `drawWireBox`/`drawLine3D` lambda pattern from weather/vegetation volumes.

**Accessibility Data Model + UI Themes:**
Added `accessibleLabel` field to UIElement (serialized, inspector UI, used by announcer with fallback to element name). Two new UIThemePreset values: `HighContrastDark` (pure white on black, yellow focus, 3px borders) and `HighContrastLight` (pure black on white, orange focus, 3px borders) — both WCAG AAA 7:1+ contrast. Added `fontScale` (f32) to RuntimeAccessibilitySettings and `SetFontScale()` on UISystem — all 5 text render paths in UISystem.cpp multiply resolved font sizes by the scale factor. Wired in Player from accessibility settings.

### Noise & Streaming Visual Script Nodes + Weather/SceneManager Binding Wiring
Added 12 new visual script nodes across two new categories:
- **Noise (6 nodes):** Perlin2D, Simplex2D, Worley2D, FBM2D, Perlin3D, Simplex3D — pure evaluate nodes for procedural generation in visual scripts.
- **Streaming (6 nodes):** ForceLoad, ForceUnload, GetState, IsLoaded, GetLoadedCount, SetEnabled — level streaming control from visual scripts.

Added `streamingManager` to `ExecutionContext` and wired it through `VisualScriptExecutor` -> `VisualScriptSystem` -> `PlayMode` + `Player`. Wired `SetBindingsWeather` and `SetBindingsSceneManager` in `PlayMode::Play()/Stop()`. Added `Noise` and `Streaming` to the `NodeCategory` enum. Total VS node count now 108+.

### Rendering Optimization, Frame Pacing, Play Mode Fix, Feature Wiring
Major optimization and feature wiring pass across 15 files + 1 new file.

**Rendering:** Sorted render list now built before main pass skip, so the offscreen game view path reuses the material-sorted list instead of falling back to unsorted entity queries. This eliminates redundant descriptor writes during play mode. Point and spot shadow passes added to the offscreen path (previously only directional CSM rendered).

**Frame Pacing:** Added `timeBeginPeriod(1)` on Windows for 1ms sleep resolution (default is 15.6ms, causing severe frame jitter). Increased frame limiter spin margin from 1ms to 2ms. Linked `winmm.lib` in Core CMake.

**Play Mode:** Stop button no longer restores pre-play scene state — objects persist after stopping, only camera position is restored. This avoids the previous behavior where all scene changes were lost on stop.

**Feature Wiring:** New `ScriptBindings_Sprite.cpp` with 13 bindings (8 Sprite2D + 5 AnimatedSprite2D). 4 Input VS nodes (IsKeyPressed, IsKeyDown, GetMousePosition, GetAxis). 2 Scene VS nodes (LoadScene, GetCurrentScene). StreamingVolume/Portal serialization in SceneSerializer (all 6 serialize/deserialize paths). StreamingVolume/Portal inspector UI with undo support. LayerComponent added to Add Component menu. NetworkSystem fully wired in Player app (was previously null).

### Comprehensive Audit — Round 3: 30+ More Fixes
Third round of audit fixes across 19 files + 2 new files.

**Feature (F1, F5, F6, F10, F12, F16, F17, F19, F20, F23, F24):** PlayMode now owns SimpleAudio, DestructibleSystem, and InputActionMap — wired in Play()/Stop() so audio, destructible, and remappable input work in editor play mode. New `ScriptBindings_AI.cpp` with 34 bindings (15 AI controller, 13 behavior tree blackboard, 2 navmesh). New `ScriptBindings_Accessibility.cpp` with 20 bindings (8 subtitle, 5 announcer, 4 colorblind, 3 general). 13 new VS nodes (4 AI, 4 BT, 1 Navmesh, 2 StateMachine, 3 Accessibility). Animator inspector UI with playback controls, speed, animation list. Build pipeline now packs 3D models (.gltf/.glb/.fbx/.obj/.dae/.3ds/.ply/.vox), SVG files, and window icons.

**Performance (P14-P16, P21, P28, P30-P32):** JoltBackend per-frame sync vectors → member caches (4 vectors: currentEntities, toRemove, jointEntities, jointToRemove). Box2DBackend per-frame toRemove → member caches (2 vectors). SpriteBatchRenderer instance data reserve(). Physics2D BroadPhase reserve(). Water mesh vertices/indices reserve(). Hierarchy.h SetParent() HasComponent+GetComponent → single GetComponent.

**Stability (S10, S14):** Flower/Flash bindings use shared s_BindingsWorld instead of separate pointers. ObjectPool.DestroyAll moved before ShutdownAllScripts in PlayMode::Stop().

### Comprehensive Audit — Round 2: 28 More Fixes
Second round of audit fixes across 18 files.

**Security (1 fix):** Replaced popen() with CreateProcessA+pipe in Git integration (N2 — command injection via project path).

**Performance (16 fixes):** FindEntityByName VS node O(N)→O(1) via World::FindEntityByName (P1). HasComponent+GetComponent merged to single GetComponent in PhysicsWorld SyncFromECS (6 collider lookups, P6-P10), EditorLayer hierarchy (3 NameComponent lookups), SpriteBatchRenderer (1 TransformComponent), ControllerSystem FPS grid (1 BoxCollider). GetAllEntities→component query in VisualScriptEditor (P22), RenderSystem OnEntityRemoved (P23), EditorLayer FocusOnSelection uses GetChildren instead of full scan (P24). Per-frame allocation elimination: Physics2D pairs+manifolds→member vectors (P11), SimplePhysics candidatePairs→member vector (P12), PhysicsWorld aliveEntities map→unordered_set (P13). SpriteBatchRenderer reserve() (P20). Physics2D BroadPhase avoids entity copy (P11).

**Feature (9 fixes):** EditorLayer wires SetBindingsWeather+SetBindingsSceneManager before PlayMode::Play() (F1). Player wires UISystem TextureResolver stub (F9). 6 new Animator script bindings: Stop, Pause, Resume, IsPlaying, GetCurrentAnimation, GetSpeed (F13). 10 new VS nodes: Tween Position, Tween Scale, Dialogue Start/Advance/IsActive, Animator Stop/IsPlaying (F14/F15), plus extended Animator node type IDs.

**Stability (1 fix):** RenderSystem OnEntityRemoved no longer scans all entities to find replacement player (deferred to lazy lookup).

### Comprehensive Audit — 46 Fixes Applied
Full codebase audit (96 findings documented in `docs/AUDIT_2026_02_12.md`), 46 fixes applied across 24 files covering security, stability, performance, and feature gaps.

**Security (15 fixes):** Replaced std::system with CreateProcessA in shader compiler (command injection), validated NaN/Inf in network entity snapshots, HTML-escaped config fields in HTML5 export (XSS), fixed PlayerId u8 overflow, capped lobby player count, dropped malformed RPC packets, validated Newgrounds API parameters, added contains() checks for save file JSON, capped HTTP response at 16MB, replaced std::stoi with strtol, validated streaming paths against traversal, thread-safe CRC32 init via std::call_once, capped reliable outbox, fixed sequence wraparound arithmetic, replaced inet_addr with inet_pton.

**Stability (10 fixes):** Initialized Physics2D with World in PlayMode+Player, deferred entity destruction during script iteration, clear coroutines before hot-reload, per-entity baseOrthoSize (was static), null-guard physics after factory, index-based coroutine loop, vector division epsilon guard, Sequence node recursion limit, zero-length raycast guard, pure node evaluation depth limit, atomic reference counts.

**Feature gaps (9 fixes):** Wired coroutine scheduler/event bus/script engine bindings in PlayMode, Physics2D updated every frame in PlayMode+Player, localization .csv/.gltf/.glb/.fbx/.obj/.svg files packed in build pipeline, wired VS SetPhysics2D in Player, FlowerSystem SetRenderSystem in PlayMode.

**Performance (6 fixes):** Component queries replace GetAllEntities in FindEntityByTag and ScenePicker, LevelStreaming O(N*M)→O(N+M), GetChildren/GetParent return by reference.

### Play Mode Entity Fix
Fixed child entities disappearing after stopping play mode. Root cause: ChildrenComponent was never serialized or rebuilt during scene deserialization. After LoadFromString restored a scene, ParentComponent was restored but ChildrenComponent on parents was never created, making child entities invisible in the hierarchy panel.

### Full-Screen Stipple / Dither Post-Process
Added a full-screen stipple/dither post-process effect as a primary aesthetic tool. 8 patterns selectable via bitmask — any combination can be enabled simultaneously with thresholds averaged: Bayer 4x4, Bayer 8x8, Blue Noise (interleaved gradient noise), Halftone (circular dot grid), Crosshatch (diagonal lines), Overlook (hex geometric), Ordered 2x2 (coarse retro), and Floyd-Steinberg approximation (pseudo error diffusion). 3 color modes: Monochrome (fg/bg ink/paper), Duo-Tone (two configurable colors), Full Color (pattern applied to luminance, preserving hue). Controls: scale (0.5-8x), density (threshold bias), strength (blend with original), and foreground/background color pickers. Editor shows a 2-column checkbox grid for pattern multi-select. Applied in the post-process chain after palette lock and before gamma correction. Full scene render settings persistence with JSON serialization and auto-migration from old single-pattern format.

### Shadow Dither Patterns
Added 6 built-in shadow dither patterns selectable per-material: Bayer 4x4 (default), Bayer 8x8, Blue Noise (interleaved gradient noise), Halftone (circular dots), Crosshatch (diagonal lines), and Overlook (hexagonal geometric pattern inspired by The Shining's Overlook Hotel carpet). Pattern stored in flag bits 29-31 (freed by compressing vertex snap resolution from 8 bits to 5 bits, packed as value/8). Inspector shows pattern dropdown only when a dither mode is active.

### Shadow Rendering Fix
Fixed shadows not rendering in the editor game view. The root cause was that the offscreen rendering path (`RenderToTarget()` / `RenderOffscreen()`) was missing the shadow pass entirely — shadow depth maps weren't being generated for the game view camera, and the offscreen LightingUBO wasn't populated with shadow cascade matrices or `shadowEnabled`. Added `RenderShadowPassForCamera()` to RenderSystem and wired it into `EditorLayer::RenderOffscreen()`. Also fixed missing `SetupEntityBuffers()` in the shadow pass for entities that hadn't been rendered to the main swapchain yet.

### Template System Redesign
Rebuilt the startup template system from 38 templates down to 22 focused templates organized into 5 categories:
- **Foundations (5):** Blank, 2D Platformer, 2D Top-Down Action, 3D Third Person, 3D First Person
- **Genre Showcases (7):** Sokoban Puzzle, Survival, RPG Village, Horror, Vehicle Racing, PS1 RPG, Arena Fighter
- **Systems Deep-Dives (5):** Physics Playground, Dialogue & Narrative, Save System Demo, Visual Scripting, UI Canvas Demo
- **Retro & Flash (3):** Point & Click, Bullet Hell, Idle/Clicker
- **Advanced (2):** Planet Gravity, Dungeon Crawler

Each template now showcases specific engine features using real components. Layout configs simplified to a 3-tier system (2D / 3D standard / 3D wide for splitscreen). Net code reduction: ~4400 lines removed.

### SceneSerializer Robustness
Applied tolerant `JB()` bool deserialization across all scene components. This prevents crashes when loading scenes saved with the old `RF()` rounding bug that wrote booleans as `0.0`/`1.0` instead of `true`/`false`.

### Physics 2D Script Bindings
Added 15 AngelScript bindings for 2D physics via `IPhysicsBackend2D`: 4 raycast variants (basic, with mask, full hit info, full hit info + mask), 4 overlap queries (circle/box, each with mask variant), 3 body manipulation (AddForce, AddImpulse, SetVelocity, GetVelocity), and 3 gravity functions (Set/Get global gravity, per-body gravity scale). New file: `ScriptBindings_Physics2D.cpp`. Also added 5 visual script nodes: Raycast 2D, Overlap Circle 2D, Add Force 2D, Set Velocity 2D, Set Gravity 2D.

### Networking Script Bindings
Added 21 AngelScript bindings for LAN multiplayer via `NetworkSystem`: connection management (Host/Join/Disconnect), 7 state queries (IsConnected, IsHost, GetRole, GetLocalPlayerId, GetPlayerCount, GetPing, GetPacketLoss), 4 lobby functions (SetReady, GetLobbyPlayerCount/Name/Ready), 3 entity ownership (Register/Unregister/RequestOwnership), and 3 RPC functions (CallRPC, CallRPCAll, RegisterRPCHandler). RPC handlers fire `__rpc_<name>` events via ScriptEventBus. Registered `NetworkRole` enum (None/Host/Client). New file: `ScriptBindings_Networking.cpp`. Also added 6 visual script nodes: Host Game, Join Game, Disconnect, Is Connected, Get Player Count, Call RPC.

### UICanvas Keyboard & Gamepad Focus Navigation
Implemented full keyboard/gamepad navigation for UICanvas: Tab/Shift+Tab cycles through focusable elements (ordered by `tabOrder`), Arrow keys and DPad navigate with key repeat (150ms initial, 100ms repeat), Enter/Space/Gamepad-A activates focused element, Left/Right adjusts sliders. Added `tabOrder`, `focusable`, and `focusColor` fields to `UIElement`. Focus state machine in `UISystem` with `m_FocusedElementId` tracking. Focus indicator rendered as outset rounded-rect border using theme `inputFocused` color or per-element override. 6 new AngelScript bindings: `UI_SetFocus`, `UI_ClearFocus`, `UI_GetFocusedElement`, `UI_IsFocused`, `UI_SetTabOrder`, `UI_SetFocusable`. Serialization of new fields in SceneSerializer.

### Accessibility Menu Template
Added template #23: "Accessibility Menu" — an in-game accessibility settings screen built with UICanvas. Features subtitle toggle + subtitle size slider, colorblind toggle, reduced motion toggle, and input sensitivity slider. All controls are focusable with proper tab order, demonstrating the new keyboard/gamepad focus navigation system.

---

## 2026-02-11

### Physics Backend Abstraction (Phases 1-3)
Completed the full physics backend abstraction layer:
- **Phase 1:** Created `IPhysicsBackend` (3D) and `IPhysicsBackend2D` (2D) abstract interfaces. Wrapped existing SimplePhysics engines in adapter classes. Rewired all consumers (ControllerSystem, ScriptBindings, VisualScriptExecutor, NodeRegistry) to use interfaces. Added `PhysicsBackendFactory` with `PhysicsBackendType` enum (Auto/Jolt/Box2D).
- **Phase 2:** `JoltBackend` wrapping Jolt Physics v5.2.0 (1273 lines). Full ECS-to-Jolt synchronization per frame, thread-safe contact events via `JoltContactListener`, bilateral collision filtering, 6 joint types, gravity zones, raycasting, CCD support.
- **Phase 3:** `Box2DBackend` wrapping Box2D v3.0.0 (~755 lines). C API with handle-based IDs, contact/sensor event polling, 5 joint types, raycasting, overlap queries, CCD, bilateral collision filtering.

### Comprehensive Audit — 70+ Fixes
Ran a full security, stability, performance, and feature accessibility audit. Applied 70+ fixes:
- **Security:** Replaced 4 `std::system()` calls with `CreateProcessA`/`ShellExecuteA`, added Vulkan error checks (9 sites), JSON bounds checks (13), enum validation (38)
- **Stability:** PlayMode null guards, PlayModeDiff try-catch, animation PingPong underflow fix, weather NaN guard, particle resize safety
- **Performance:** Replaced 12 `GetAllEntities()` with component queries, merged 12 HasComponent+GetComponent pairs, added xorshift32 PRNG, reserve() calls
- **Feature wiring:** Connected 6 missing `SetBindings*()` in Player, initialized 5 new systems

### Spatial Hash Performance Fix
Fixed `SpatialHashGrid::Insert()` inserting large colliders into ~170 cells per frame. Capped to 8 cells per dimension; oversized entities go into a brute-force list instead.

---

## 2026-02-10

### Tiered Save System
Implemented a complete tiered save system with 20 slots (17 manual + 3 auto-save), three persistence tiers (SceneState/RunState/MetaProgression), pluggable backends (Local/Newgrounds/Steam), and an in-game save/load menu component. Added play mode diff dialog that shows entity changes on Stop with cherry-pick apply.

### Feature Accessibility Fixes
Wired up 14 missing runtime systems in the Player app. Extended the build pipeline to pack scripts, audio, dialogue, prefabs, and data assets. Added 6 new script binding files (Weather, Gameplay, UI, Particles, Prefab, Streaming) and 16 new visual script nodes. Connected Newgrounds bindings and level streaming into PlayMode.

### Panel Reorganization
Reorganized editor panels for better discoverability: Settings split into EditorSettings/ProjectSettings, Effects renamed to RetroEffects, Skybox expanded to Rendering panel. Moved collision groups, environment settings, and rendering settings to their logical homes.

### Security Audit
Documented 35 vulnerabilities (6 CRITICAL, 12 HIGH, 12 MEDIUM, 5 LOW) in `docs/SECURITY_AUDIT.md`.
