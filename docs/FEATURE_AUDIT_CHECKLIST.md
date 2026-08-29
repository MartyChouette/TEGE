# TEGE Feature Audit Checklist

Last updated: 2026-06-15

Purpose: verify every feature the engine and the website currently claim. Built from the engine `README.md`, the user manual, and the source where already confirmed. Open the editor and confirm each line. Check the box when a claim is verified, and add a note when something fails, is partial, or needs the wording changed.

How to use: mark `[x]` when verified. For anything that fails or is only partly true, write the real state in a note so the public copy can be corrected. The numeric claims (component counts, node counts, controller counts) should be confirmed against the running build, not just the docs.

---

## Editor
- [ ] Hierarchy, inspector, viewport, console, asset browser panels all present
- [ ] Play / pause / stop mode works
- [ ] Undo / redo (including inspector property edits)
- [ ] Cut / copy / paste, multi-select (Ctrl+click, Shift+range, marquee)
- [ ] Transform gizmos (translate / rotate / scale)
- [ ] Terrain sculpting brushes (claim: 5 brush modes)
- [ ] Command palette (Ctrl+P)
- [ ] Drop-down console with commands (claim: 60+ commands)
- [ ] 48 starter templates load and produce a populated scene
- [ ] Template Creator and Template Marketplace open and function
- [ ] Real-time multi-user editing (claim: OT protocol, peer cursors) - verify this actually connects

## Rendering (core)
- [ ] PBR materials: metallic/roughness, normal, emissive, transmission/IOR, subsurface, matcap
- [ ] Parallax occlusion mapping (claim: 4 modes)
- [ ] Shadows: 4-cascade CSM, point cubemap, spot, Poisson PCF
- [ ] Anti-aliasing: TAA, FXAA, SMAA
- [ ] Post: bloom, vignette, color grading, film grain, tone mapping, DoF, tilt-shift, post-process volumes
- [ ] Retro: PSX vertex snap, affine textures, CRT scanlines (claim: 11 models), VHS, palettes (PICO-8, Game Boy, NES, CGA, C64)
- [ ] 8 art-style presets (Realistic PBR, Blinn-Phong, Hand-Painted, Cel/Toon, Low-Poly, Pixel, NPR Sketch, Analog)
- [ ] Optimizations: Hi-Z occlusion culling, clustered forward lighting, multi-draw indirect, async compute

## Lighting and adaptive quality (flagship)
- [ ] Adaptive quality holds a target framerate (verified in source: AdaptiveQualitySystem)
- [ ] It adjusts shadows, particles, render scale, and LOD bias on the fly
- [ ] Shadow budget gives top lights real shadow tiles, rest fall back to probe lighting (source: ShadowBudget)
- [ ] Confirm it behaves on actually-constrained hardware (Steam Deck / laptop), not just a fast PC
- [ ] FSR 2 upscaling works (caveat: DLSS/XeSS only when vendor SDKs are linked)

## Ray tracing (caveated)
- [ ] Path tracer renders (NEE, MIS, Cook-Torrance)
- [ ] Denoisers: SVGF, Intel OIDN, NVIDIA OptiX
- [ ] Caveat to test honestly: hybrid RT effects (shadows, reflections, AO, GI) per the README require manual SPIR-V compilation. Confirm whether they work out of the box, and word the site to match
- [ ] RT features gated to RT-capable hardware behave gracefully when absent

## Elemental and weather (flagship)
- [ ] Fire, water, earth, air in one pool, interacting (verified in source: ElementalSystem)
- [ ] Elements respond to wind, weather, seasons (WindSystem, WeatherSystem, SeasonalWeatherSystem)
- [ ] Fire emits light into the scene (source: FireLight feeds the renderer)
- [ ] Weather: rain, snow, fog, storms
- [ ] Water with Gerstner waves
- [ ] Instanced vegetation reacts to wind

## ECS and components
- [ ] Component count claim - README says both 80+ and 70+; pick the true number and make it consistent everywhere
- [ ] Every component has inspector UI

## Character controllers
- [ ] 7 controllers (verified in source): Platformer 2D, Top-Down 2D, Top-Down 3D, Third Person, First Person, Vehicle, Surface-Aligned. README undercounts at 5 and omits Vehicle, fix it
- [ ] Gravity zones, temperature zones, camera trigger zones

## Physics
- [ ] Jolt 5.2.0 (3D) and Box2D 3.0.0 (2D)
- [ ] Collision filtering, sensors, ground detection, debug wireframes
- [ ] Joints (claim: 6 types), ragdolls, destructibles

## Animation and audio
- [ ] Skeletal animation, glTF + Assimp import, GPU skinning, state machines with blending
- [ ] 2D flipbook animation
- [ ] IK: LookAt, FABRIK, interaction
- [ ] Timeline editor: keyframes, layers, curve editor, onion skinning
- [ ] Audio: miniaudio (WAV/MP3/FLAC/Vorbis), 3D spatialization, SFX/Music/UI/Voice channels

## Scripting
- [ ] AngelScript with hot reload, coroutines, event system (claim: ~960 bindings)
- [ ] Visual scripting 146+ nodes, breakpoint debugging (F9/F5/F10), execution profiler
- [ ] Plugin system: DLL/SO hot-reload with state save/restore

## Gameplay systems
- [ ] Save/load (claim: 10-slot, plus tiered RunState/SceneState/MetaProgression in the manual, reconcile these two descriptions)
- [ ] Quest system with objective tracking
- [ ] Dialogue trees, Yarn/Twine import
- [ ] Behavior trees (claim: 20 node types, blackboard, visual editor), navmesh A*
- [ ] Combat: damage resist/weakness, stamina/resource
- [ ] Localization: string tables, CSV/JSON, LOC() macro
- [ ] Dynamic difficulty adjusts AI/damage/resources

## UI runtime
- [ ] Anchor layout, widget types (claim: 8), theme presets (claim: 6), font scaling, accessible labels

## Accessibility
- [ ] Colorblind correction (claim: 8 modes)
- [ ] Screen reader
- [ ] Switch access, dwell-click, sticky drag, one-handed presets, remappable input
- [ ] Dyslexia mode (OpenDyslexic), reduced motion, content warnings
- [ ] Subtitles: size, background, speaker labels
- [ ] High-contrast WCAG AAA themes
- [ ] Quick presets: Low Vision, Motor Impaired, Photosensitive, Reset All

## Build, distribution, platforms
- [ ] Windows EXE + Inno Setup installer
- [ ] Linux AppImage
- [ ] HTML5 / WebAssembly via Emscripten with WebGPU renderer (test an actual web export)
- [ ] `.enjpak` asset packs with compression + CRC32
- [ ] Standalone player builds run without the editor
- [ ] Steam Deck auto-detect + adaptive quality
- [ ] Caveat: Switch backend is a stub requiring a licensed devkit; console is not ready. Keep these off the public claims
- [ ] Caveat: networking is LAN only, not online multiplayer

## Asset libraries
- [ ] 42 bundled fonts
- [ ] 16 CC0 3D model packs, 15 CC0 2D sprite/tileset packs
- [ ] Import presets for 10 DCC tools, BCn/ASTC compression, auto-thumbnails

---

## Claims to reconcile before publishing
- [ ] Test count: README badge says 1800+ passing, the architecture section says 1100+, the roadmap says ~1100 across 82 targets. Settle on one.
- [ ] Component count: 80+ vs 70+.
- [ ] Controllers: README says 5, source has 7.
- [ ] Ray tracing: turnkey vs requires manual SPIR-V compilation.
