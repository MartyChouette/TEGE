# System Verification Backlog

**Date:** 2026-06-16
**Why this exists:** A depth-audit of the whole test suite, triggered by finding
that `TestAssetLoaders` had ~50 passing tests that only checked struct default
values and nonexistent-file failure paths. Zero real-data coverage. Green, but
proving nothing. We added `TestGoldenVerification` to actually prove glTF/rig/
animation/SVG/tween/timeline import with real data, then audited every other
suite the same way.

The question for each suite: **does a test run real behavior and assert the
result, or does it only construct structs and check defaults / failure paths?**

## How to read the verdict

- **PROVEN** — at least one test runs real behavior end-to-end and asserts correctness.
- **THIN** — some real behavior tested, major paths unverified.
- **SHALLOW** — only struct defaults and/or only failure-path checks. False confidence.
- **SELF-REFERENTIAL** — the test re-implements the logic in its own body and asserts its own arithmetic. Green, tests nothing in the engine. Actively misleading.
- **NONE** — code exists, no behavioral test.

---

## Confidence map

| Subsystem | Verdict | Note |
|---|---|---|
| ECS core (deferred destroy, generational IDs) | **PROVEN** | Both documented traps genuinely asserted. Solid. |
| Scene serializer / component round-trips | **PROVEN** | `SerializerRoundTrip` (34 components, ~200 checks), `TestSceneSerializerComponents`, `TestSaveSystem`, `TestDataAsset`, `TestSceneAssetValidator`. Strong. |
| Asset packs | **PROVEN** | Real pack→reopen→read round-trips, CRC32. (Tamper test is conditional — see backlog.) |
| Import: glTF / rig / animation / SVG / tween / timeline-interp | **PROVEN** | New `TestGoldenVerification`. |
| Elemental system | **PROVEN** | Runs the sim, asserts emergent behavior. Strongest gameplay suite. |
| Behavior trees | **PROVEN** | Ticks real executor against a world. |
| Navmesh pathfinding | **PROVEN** (path) | A* runs; path correctness/detour not asserted. |
| UI anchor math (the documented trap) | **PROVEN** | `TestUISystem` computes the Unity-anchor formula. (Bad-ordering fallback not tested.) |
| Dialogue / dialogue tree | **PROVEN** | Real player traversal + branching. |
| Networking adversarial + security | **PROVEN** | Genuinely deep: malformed packets hit real bounds checks, SHA256 vs NIST vectors. |
| Procedural generation | **PROVEN** | Every generator runs; connectivity/traversability not asserted. |
| IK solver (FABRIK, LookAt) | **PROVEN** | Real convergence checks. (TwoBoneIK untested.) |
| Screen transitions | **PROVEN** | Full phase machine driven. |
| Visual script node eval | **PROVEN** | Node evaluate/branch asserted; no full graph traversal side-effect. |
| **Physics (Jolt 3D + Box2D 2D)** | **GOOD** | `TestPhysicsSimulation` steps both backends and asserts behavior: box falls and rests on a floor (Jolt + Box2D), 3D raycast hits, gravity round-trips, bilateral collision filtering suppresses contacts (incompatible masks tunnel through), collider size is world-space (ignores entity scale), `CharacterVirtual` grounds with normal up, and 2D sensor enter events fire. Remaining nice-to-haves: joint constraint behavior, 2D raycast-skips-sensors, kinematic-kinematic hazard sensors. |
| **Rewind (RecordRewindSystem)** | **NONE** | Capture/restore/seek/delta-compression never instantiated in any test. |
| Camera view/projection matrix math | **THIN** | Deterministic and CPU-testable, but matrices only asserted "not all zeros". |
| Transform `ToMatrix` rotation | **SHALLOW** | Only identity quaternion tested; rotation never applied. |
| Build pipeline `Execute()` success | **THIN** | Only the failure path runs; no project ever built into a `.enjpak`. |
| Prefab save/load + instantiate | **SHALLOW** | `SavePrefab`/`LoadPrefab` never called. |
| Tiered save slots (disk) | **THIN** | In-memory KV tested; `SaveToSlot`/`LoadFromSlot` never round-tripped. |
| Level streaming load/unload | **SHALLOW** | No `Update(cameraPos)`; state machine unproven. |
| SkeletalAnimator pose sampling | **THIN** | `BoneTrack::SamplePosition` tested; no Play→Update→sample→bone-matrix; rotation/scale sampling untested. |
| TimelineSystem `Update` | **SELF-REFERENTIAL** | `TestTimeline`'s loop/pingpong/easing tests re-implement the logic inline; the engine `Update` is never run. |
| BlendTree pose blend | **THIN** | Clip selection proven; actual blended pose not checked. |
| LevelGenerator `Generate()` | **SHALLOW** | Only prefab bookkeeping; the placement/connection algorithm never runs. |
| Interactive water / weather | **SHALLOW** | No `Update()`; wave propagation, particle spawning, buoyancy unproven. |
| Quest / cinematic systems | **THIN** | Only enums/defaults/reset; systems never run, no state transitions. |
| Health / damage / gameplay components | **SHALLOW** | Pure struct defaults; no take-damage/death/regen behavior. |
| Script bindings (~960) | **THIN** | Read bindings proven (`IsValid`); no binding proven to MUTATE the world. |
| Plugin loader | **THIN** | No DLL fixture; load/symbol/init lifecycle unverified (by design). |
| Audio playback | **SHALLOW** | dB/attenuation math real; no headless playback/pan (env-gated). |
| LOD selection | **SELF-REFERENTIAL** | Test asserts its own arithmetic; the LOD-pick function is never called. |
| Collision filtering | **SELF-REFERENTIAL** | Bitmask rule proven against a test-local copy, not the engine's filter path. |

---

## A. Automated tests to add (engine-side)

Prioritized. P0 = core system with no behavioral coverage; P1 = high-value and
cheap or ship-critical; P2 = fills out coverage.

### P0 — core systems currently unproven
1. **Physics: step a Jolt world** — DONE (`TestPhysicsSimulation.PhysicsSim3D.DynamicBoxFallsAndRestsOnFloor`).
2. **Physics: step a Box2D world** — DONE (`TestPhysicsSimulation.PhysicsSim2D.DynamicBodyFallsAndRestsOnFloor`).
3. **Physics: sensor event fires** — DONE (`PhysicsSim2D.SensorFiresOnOverlap`): a dynamic visitor falling through a static sensor zone fires `OnSensorEnter`.
4. **Physics: filtering applied by the engine** — DONE (`PhysicsSim3D.CollisionFilteringSuppressesContact`): mutually-exclusive masks tunnel through, compatible masks rest. Proves the `JoltContactListener::OnContactValidate` path, not the test-local copy.
5. **Physics: raycast + character grounding** — DONE. `PhysicsSim3D.RaycastHitsFloor` and `PhysicsSim3D.CharacterControllerGroundsOnFloor` (`CharacterVirtual` grounds, `groundNormal.y ≈ 1`).
6. **Physics: world-space collider size** — DONE (`PhysicsSim3D.ColliderSizeIsWorldSpaceIgnoringScale`): a 2×-scaled entity rests at y=1.0, not 1.5.
7. **Rewind: record then restore** — move an entity, record, `SeekEntityToTime`, assert transform matches the recorded frame; plus scene-rewind round-trip and delta-compression reconstruction.

### P1 — high value, cheap, or ship-critical
8. **Camera matrices with computed expected values** — DONE. `GetViewProjectionMatrix == proj·view` (composition order), `SetLookAt` places the world origin at view-space z=-5, rotation drives an orthonormal forward basis. Note: `GetForward` extracts the rotation-matrix row (camera world-space basis), which differs in handedness from `Quaternion::Rotate` (column); both are internally consistent, not a bug.
9. **Transform `ToMatrix` rotation + full TRS** — DONE. Non-identity quaternion produces the rotated basis; full translate·rotate·scale composition verified.
10. **`static_assert(sizeof(MaterialGPU) == 112)`** — DONE, and it caught a real doc drift: the struct is **112 bytes**, not the 80 documented in CLAUDE.md and the header comment (SSS block + bindless texture indices were added later). Renderer works at 112, so 112 is the true SSBO stride; CLAUDE.md and the header comment corrected.
11. **BuildPipeline end-to-end** — temp project + scene → `Execute()` → assert `.enjpak` contains the scene + a build manifest with correct window settings.
12. **Prefab save/load round-trip** — `SavePrefab` a hierarchy to disk, `LoadPrefab`, assert entities/parents/component maps survive.
13. **Tiered save slot persistence** — `SaveToSlot` to disk, fresh system, `LoadFromSlot`, assert KV + entity data match.
14. **SkeletalAnimator pose-over-time** — known rotation keyframes, Play + Update to t=0.5, assert sampled local rotation and skinning matrix match hand-computed values; cover `SampleRotation`/`SampleScale`.
15. **TimelineSystem `Update` (replace the self-referential tests)** — build a world+entity, `sys.Update(world, dt)`, assert time advances, loop wraps, ping-pong flips, events fire once, `isComplete` sets.
16. **LevelGenerator `Generate()` + connectivity** — run it, assert rooms within bounds, start+end exist, BFS-reachable (no orphaned pockets). Same BFS check for BSP/RandomWalker output.
17. **Script binding mutates world** — script calls a position setter, assert C++ `TransformComponent.position` changed (closes the only real scripting gap).
18. **FBX/OBJ import golden (Assimp path)** — OBJ mesh + material DONE (`GoldenOBJ` suite: cube geometry, normals, `Kd` material color, full `ImportAssimp`). FBX rig test SCAFFOLDED (`GoldenFBX.RiggedMeshImports`): asserts mesh+skeleton+skinning+animation, skips until a valid fixture lands at `Tests/Fixtures/humanrig.fbx` (recipe in `Tests/Fixtures/README.md`). The rig/animation import *logic* is already proven via the glTF path. *(The original item that started this list.)*

> **Discovered while scaffolding the FBX test (2026-06-17):** `AssimpLoader::Load`
> (`Engine/src/Assets/AssimpLoader.cpp:76`) treats Assimp's `AI_SCENE_FLAGS_INCOMPLETE`
> as a hard failure, so **any mesh-less FBX is rejected** — a valid rig-only or
> animation-only export (the standard "import an animation clip to retarget onto an
> existing character" workflow) cannot be loaded. Decide: accept INCOMPLETE scenes
> that still have a root node + bones/animations, or document this as intended.
19. **Extend the golden round-trip** — save + reload a `SkeletonComponent` + `AnimatorComponent` and assert bones/clips survive (we proved tween; skeleton/animator serialization exists but is unproven on real data).

### P2 — coverage fill
20. Health system: damage → shield-then-health depletion → death → regen delay → invulnerability.
21. Quest system: Start→Objective→End graph runs, status transitions, branch routing.
22. Cinematic system: `Update` over a 2-waypoint segment, assert interpolation + completion.
23. Interactive water: splash → N `Update()` steps → wave propagates outward, damping decays.
24. Weather: high rain intensity → `Update()` → particle count grows; storm → lightning fires.
25. Level streaming: `Update()` inside loadDistance → Loaded; past unloadDistance → Unloading; hysteresis prevents thrash.
26. LOD selection (replace self-referential test): call the real pick function at several distances, assert active LOD + hysteresis.
27. Navmesh detour/blocked: wall off a region, assert path routes around or a true "no path" negative.
28. UICanvas anchor-trap regression: feed the documented bad `+100/-100` ordering, assert the element is not off-screen.
29. Asset-pack tamper (unconditional): flip a payload byte, assert `VerifyIntegrity()` returns false.
30. Replace `EXPECT_TRUE(true)` "no crash" asserts in `TestStressFuzz` / `TestHardeningRegression` with concrete post-conditions so silent corruption is caught.

### Meta-fix
- **Audit the SELF-REFERENTIAL suites** (`TestTimeline`, `TestLOD`, `TestCollisionFiltering` filter math, `TestHardeningRegression` oversized-payload): these pass while testing nothing in the engine. Rewrite each to call the real engine function. Until then they are worse than no test, because they read as covered.

---

## B. Manual checks for you (editor / runtime)

These verify the things only a human at the screen can confirm. Roughly ordered
by risk. Treat this as a QA pass checklist.

### Physics (no automated coverage at all — check carefully)
1. 3D scene: drop a dynamic cube above a static floor in Play, confirm it falls and rests without tunneling or jitter.
2. 2D scene: walk a character into a kinematic hazard sensor, confirm the trigger/damage event fires.
3. Set two colliders incompatible (category/mask) in the inspector, Play, confirm they pass through; make compatible, confirm they collide.
4. TopDown3D/isometric template: confirm the capsule character stands on ground (height convention) without sinking or floating.
5. Author a `BoxColliderComponent.size` on a scaled entity, confirm the collider gizmo matches world-space dimensions, not scaled ones.
6. Fire a 2D raycast through a sensor zone, confirm it ignores the sensor and reports the solid wall behind it.

### Rendering (inherently visual — the suite only covers CPU config)
7. **RT effects on an RT-capable GPU** (memory flags these embedded-but-unverified): toggle shadows/reflections/AO/GI individually and confirm each visibly appears; switch to path tracer and confirm progressive accumulation.
8. Shadows: directional light + occluder, confirm shadow maps render and respect resolution/distance/softness, no cascade popping.
9. PBR: metal/rough sphere grid under one light, confirm energy-conserving falloff and no NaN black spots (WebGPU tangent-fallback trap), ACES tonemap default.
10. Clustered lighting: 50+ point lights in one scene, confirm all illuminate with no per-cluster dropout or banding; check the 32 fire-light/frame cap.
11. Post-FX + accessibility: cycle all 8 colorblind modes + brightness/contrast + `disableFlashingLights`, confirm each visibly transforms the frame (accessibility pillar).
12. Retro stack: enable CRT/VHS/dither/quantize/vertex-snap/affine, confirm each produces its artifact.
13. WebGPU web build parity: run the same scene in the browser build, side-by-side vs Vulkan for shadows/PBR/fog/FXAA.

### Animation
14. Import a rigged glTF/FBX, scrub an animation in the timeline, confirm the mesh actually deforms (the sample→skinning→render path no test touches).
15. Drive a 1D BlendTree (Idle/Walk/Run) parameter 0→2 at runtime, confirm the pose interpolates rather than snapping.
16. Add foot/LookAt IK and move the target, confirm the chain follows and the knee/elbow bends the right way.
17. Place a Timeline with property + event tracks, Play, confirm it advances/loops/ping-pongs and fires events at the marks.

### Gameplay & save
18. Press the rewind keys on a moving entity, confirm position/velocity rewind and resume cleanly — and that physics bodies rewind in sync, not just ECS transforms.
19. Save to a slot, quit fully, relaunch, load, confirm player position + inventory + meta-progression restore.
20. Build a project, run the `.enjpak` in the standalone Player, confirm it launches into the start scene with correct window title/size.
21. Drag a hierarchy to a prefab, save, delete the source, re-instantiate, confirm components and child transforms come back.
22. Run a multi-node quest to completion in a template, confirm objective→completion and branch routing.
23. Drive a cinematic with 3+ waypoints, confirm smooth interpolation, hold times, easing, HUD/input disable.

### Content systems
24. Author a level with streaming volumes, fly toward/away from a chunk boundary, confirm chunks pop in before you reach them and don't thrash at the boundary.
25. Add an InteractiveWater plane, throw an object in, confirm ripples propagate and a buoyant object floats.
26. Run a procedural level in-editor and walk it, confirm every room is reachable and special rooms spawned.
27. Toggle Weather to Rain/Storm, confirm particles spawn, fog ramps, lightning flashes.
28. Open a scene referencing a missing texture/audio file, confirm the editor shows the missing-asset warning naming the component.

### Scripting / AI / audio
29. Attach a script that moves/rotates an entity each frame, confirm it animates in the viewport (the untested mutation path).
30. Build a VisualScript graph in the editor, confirm it executes and produces the side effect at runtime.
31. Tick a behavior tree with real MoveTo/Attack nodes on an NPC, confirm the agent moves/attacks.
32. Play a scene with an AudioSource, confirm sound plays and a 3D source pans left/right and attenuates with distance.
33. Load an actual plugin DLL, confirm it initializes, lists in `GetPlugins()`, and unloads cleanly.

---

## Bottom line

The serialization, ECS, AI, dialogue, networking-security, procedural, and
import layers are genuinely well-tested. The dangerous gap is **physics**: a
core system with thousands of lines and seven documented traps, and not one test
steps a simulation. **Rewind** is second (never instantiated). After those, the
pattern is consistent — systems whose *config* is tested but whose *behavior*
runs only in the editor. The manual checklist above is how you cover that until
the automated backlog catches up.
