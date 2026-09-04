I'll start by reading the project's CLAUDE.md and reference docs, then systematically explore each layer.

---

Let me read the biggest gameplay-logic file myself while delegating breadth searches.

---

Now I'll take ECS Systems, Physics, and Input myself. Starting with ControllerSystem.

---

I have a solid set of verified findings for my portion. Let me do a final cross-check while the delegated sweeps finish.

---

Two sweeps are back. Let me independently verify their highest-value claims before I report them.

---

All three sweeps are in. Let me independently verify the Gameplay sweep's load-bearing claims.

---

All three delegated sweeps are back, and I verified roughly twenty of their load-bearing claims against the source myself. Every one held.

## Audit: ECS / Gameplay / Physics / Scripting / Input

Scope covered: `Engine/src/ECS/` (minus RenderSystem), `Gameplay/`, `Physics/`, `Scripting/`, `Input/`, `Animation/`, `AI/`, `Scene/`. Every finding below was verified by reading the code and grepping call sites across `Engine/`, `Editor/`, `Player/`, `Tests/`, `enjin_api/`, `Examples/`. Claims of "no caller" show the grep result.

---

## Tier A: silently broken features a user will hit

**A1. Script hot reload has never worked for any script in the repo.**
`Engine/src/Scripting/ScriptEngine.cpp:807-833`. `CompileScript` (`:283-308`) reads the source, checks for `TegeBehavior.as`, and injects the base class if absent. `ProcessHotReload` re-implements the compile from scratch with a bare `AddSectionFromFile` and does none of that. All eight example scripts extend `TegeBehavior` and none mention `TegeBehavior.as`, so all eight depend on injection. Saving a script produces `Identifier 'TegeBehavior' is not a data type`, the module is discarded while live instances keep running old bytecode, and because `info.lastModified` is only updated on the success path the poller re-detects and re-fails every 30 frames forever. Fix: delete the second compile path, call `CompileScript(info.filePath)`.

**A2. On web, every behavior tree, visual script, quest flow and FSM graph is wiped at scene load.**
`Engine/src/AI/NodeGraphStub.cpp:83-84`. Two of the file's methods are stubs while the rest is a faithful port: `ToJson` returns empty arrays, `FromJson` calls `Clear()`. The real bodies live in `Engine/src/Editor/NodeGraph.cpp:182,232`, and `build-web/.../link.txt` contains `NodeGraphStub.cpp.o` and no `NodeGraph.cpp.o`. Scene load calls it unconditionally at `SceneSerializer.cpp:4900, 4978, 5241, 5311`. All three consumers tick on web (`web_main.cpp:894-896`) and `BehaviorTreeSystem::Initialize` logs a nonzero tree count because it counts components, not nodes. Fix: move the data layer out of the editor-only TU into a platform-neutral file and delete the stub.

**A3. The cinematic camera never moves the view, in any runtime.**
`Engine/src/Gameplay/CinematicSystem.cpp:60-67, 93-105` writes the pose into the `Renderer::Camera*` argument and into the cinematic entity's own transform. Every runtime then rebuilds the render camera from the active `CameraComponent` entity afterwards (`Player/src/main.cpp:1079` update, `:1388` overwrite; `web_main.cpp:1040`; `EditorLayer.cpp:3067`). `CameraDirector.cpp:186-200` solves this correctly and its comment says exactly why. `DialogueSystem.cpp:104` triggers `Play()`, so authored cutscenes run and the camera sits still with no error. Fix: one shared `ApplyCameraPose(world, ...)` used by both.

**A4. `Health_Damage` from AngelScript can never kill anything.**
`Engine/src/Scripting/ScriptBindings_Components.cpp:81-103`. It sets `onDeathNotify` (a field typed `ECS::Entity`, assigned `true`) but never `isDead`. The six real `isDead = true` writers are all in `GameplayLoop.cpp` and `AISystem.cpp`. `UpdateHealthSystems` and `UpdateGameOverState` key on `isDead`, never on `currentHealth <= 0`. A game dealing damage from script drops the player to 0 HP with no death, no respawn, no game over. It also absorbs shield before the i-frame check, unlike the other three copies.

**A5. The `World` name cache is maintained by hand at 3 of 210 sites.**
`Engine/src/ECS/World.cpp:145-163`. `m_NameCacheDirty` is set only by `DestroyEntityInternal` and `Clear`. `InvalidateNameCache()` is called from exactly three places (`ScriptBindings_Scene.cpp:324`, two in `CollaborativeEditingUI.cpp`), while `AddComponent<NameComponent>` appears at 210 sites across 32 files. `PrefabManager::Instantiate` (`Assets/Prefab.cpp:449`) is one of them. Once the cache has been built, anything spawned afterwards is invisible to `FindEntityByName`, and a rename leaves a stale entry. Consumers: `Scene_FindEntity`, the VisualScript find-by-name node, `ActionTriggerSystem::ResolveTarget` (the components-only path), `ClothSystem.cpp:763`, `BrickComposition`, the MCP server. It is intermittent because any destroy that frame masks it. Fix: bump an epoch from `ComponentStorage<NameComponent>` add/remove, or drop the cache.

**A6. Script-driven entity destruction skips all four teardown paths.**
`ScriptBindings_Scene.cpp:46-53` calls `World::DestroyEntity` directly. `ScriptAttachment::instance` is a raw `void*` with no destructor and `World` has no destroy hook. `ScriptEventBus::RemoveAllForEntity` and `CoroutineScheduler::StopAllForEntity` have **zero callers repo-wide**; `ReleaseInstance` and `OnDestroy` are reached only from `ShutdownAllScripts`. Every despawn in a spawn-heavy game leaks an `asIScriptObject`, leaves coroutines ticking, and leaves listeners firing on a zombie until the 1024-per-event cap starts rejecting new ones. `OnDestroy()` never fires during play.

**A7. There is no interaction system, but the whole surface is authored.**
`GameAction::Interact` is a full row in `kActionInfo` (`InputActionMap.cpp:38`) and appears in four of six touch presets (`TouchActionBridge.cpp:31-37`), so the on-screen "USE" button ships on mobile. Nothing in the engine reads it. The one built-in interaction, doors, reads `Input::IsKeyPressed(KeyCode::E)` raw at `ControllerSystem.cpp:1133`, so rebinding does not move it, gamepad X does not work, the USE button does nothing, and input focus is not enforced (E in a menu toggles doors behind it). Meanwhile `InteractableComponent` (`Gameplay.h:850`) has `promptText`, `interactionRange`, `requiresLookAt`, `lookAtAngle`, `singleUse`, `hasBeenUsed`, `highlightOnHover`, `onInteractNotify`, and **no system reads any of them**. Twelve editor templates author it ("Open Chest", "Talk to Villager", "Read Book"). `Interactable_HasBeenUsed` is a registered getter for a field nothing ever writes. Same for `SwitchComponent::promptText` and `PossessableComponent::promptText` ("Press E to enter"). CLAUDE.md's own rule forbids exactly this pattern for dialogue and menus.

**A8. Hold-to-rewind accelerates instead of playing at `rewindSpeed`.**
`Gameplay/RecordRewindSystem.cpp:236-271`. `rewindPlayhead` accumulates, `targetTime = currentRecordedTime - rewindPlayhead`, and then the pop loop does `currentRecordedTime -= recordInterval`, moving the anchor the offset was measured against. The offset compounds, so a 5s buffer empties in a fraction of a second. Scene rewind on the same scene is correct (it pops by timestamp), so it reads as a mystery. `EntitySnapshot` has no timestamp field, which is why frame times are reconstructed by index arithmetic in four places; line 240 reads `history.Back().position.x` under a comment calling it a timestamp.

---

## Tier B: one rule, N implementations

**B1. Contact damage exists five times; the copies have already diverged.** The shield/i-frame/health/knockback/death sequence is written out at `GameplayLoop.cpp:66-111` (`ProcessContactDamage`, canonical), `:225-261`, `:325-357`, and `ScriptBindings_Components.cpp:81-103`. Only `CheckHazardOverlaps3D` (`:497`) calls the shared one. The vertical knockback `* 0.5f` is a literal in three of them while `knockbackForce` is authored. The stomp rule still exists twice (`:39-55`, `:311-322`) even though its magic numbers were correctly promoted to component fields.

**B2. Dash is duplicated twelve ways with a magic constant in three of them.** The dash field block (`enableDash`, `dashSpeed`, `dashDuration`, `dashCooldown`, `dashTimer`, `dashCooldownTimer`, `isDashing`) is copy-pasted into three controller structs (`CharacterController.h:137, 172, 275`) instead of `CharacterControllerBase`, which already hosts the swim tuning block with a comment explaining why tuning belongs there. The cooldown/trigger/tick logic is copy-pasted at `ControllerSystem.cpp:935, 1012, 1746`, the serializer has three copies (`SceneSerializer.cpp:2346, 2379, 2485`), and the inspector three (`EditorLayerComponents.cpp:3479, 3564, 3802`) whose clamps already disagree (5-50 vs 1-50). Dash acceleration is the literal `1000.0f` at `ControllerSystem.cpp:968, 1056, 1795`, so a ramped dash cannot be authored. Platformer2D, ThirdPerson, Vehicle and SurfaceAligned have no dash at all; moving the block to the base gives all seven controllers dash for free.

**B3. Three easing tables; the Flash bake throws away 21 of 25 curves.** `ECS::ApplyEasing` (`TweenSystem.cpp:34`, exported in `Tween.h:73`) has 30 curves. `TimelineSystem::ApplyEasing` (`Timeline.cpp:352`) has 5. `CinematicSystem::ApplyEasing` (`CinematicSystem.cpp:9`) has 4. `FlashTimeline.cpp:122` uses the full one for preview, then `:813-843` collapses everything to four when baking to a runtime `TimelineComponent`. The editor offers "Ease Out Elastic", previews it correctly, and plays back a quadratic.

**B4. Three path-following implementations that disagree about the same component.** `AISystem::MoveAlongPath` (`:465-570`, honours navmesh + `arrivalRadius` + `stoppingDistance` + `turnSpeed`), `BehaviorTreeExecutor::TickAction` MoveTo/Patrol (`:282-343`, honours `moveSpeed` + `stoppingDistance` only), and `PathFollower::Update` (`Navmesh.cpp:691-751`, the richest, **entirely unused**: `PathFollower` appears outside `Navmesh.*` only in a test). BT and AI both write `transform.position` from the same `AIControllerComponent` and both tick every frame in all three runtimes, BT immediately before AI. BT ignores `useNavmesh` entirely, so a `Move To` node walks through walls on a navmeshed scene while `AIState::Chase` paths around them. BT's `Patrol` ignores `patrolWaitTime`, `patrolLoop` and `arrivalRadius`. And `BTNodeType::CanSeeTarget` (`BehaviorTreeExecutor.cpp:406-416`) is a pure range check whose own comment claims "range + FOV" while never reading `ai.fieldOfView`, which `AISystem::CanSeeTarget` (`:604-631`) does properly.

**B5. The water-volume rule exists twice, and swimming does not work in `Water3D`.** `ControllerSystem.cpp:1181` (`FindWaterAt`, comment: "Mirrors JoltBackend's buoyancy-zone rule") handles only `WaterVolumeComponent`; `JoltBackend::ApplyBuoyancy` (`:972-1044`) handles `WaterVolumeComponent` **and** `Water3DComponent`. `grep -c Water3DComponent` gives 0 in ControllerSystem, 2 in JoltBackend. Author a Water3D lake: barrels float, the player walks along the bottom.

**B6. The capsule convention CLAUDE.md flags as a trap is hand-written five times.** `height * 0.5f + radius` at `ControllerSystem.cpp:154, 172, 2223` and `GameplayLoop.cpp:379, 425`, plus the `(0.3f, 0.8f)` default pair twice. Belongs on `CapsuleColliderComponent`.

**B7. "Is this entity a player?" is written out twelve times.** The five-controller check appears longhand at `GameplayLoop.cpp:130, 265, 359, 398, 507, 544, 571, 697, 800, 817, 828, 869`. A sixth controller type silently cannot collect pickups, take hazard damage, trigger game over, or enter a trigger zone, with no compiler help.

**B8. Gravity-zone selection three ways; the Box2D copy never got the Jolt fix.** `JoltBackend::ApplyGravityZones` (`:903`) was explicitly optimized to hoist storage pointers out of the O(bodies x zones) inner loop, with a comment describing it. `Box2DBackend::ApplyGravityZones` (`:816-864`) is the same algorithm doing two `GetComponent` hash lookups per body-zone pair every frame. `ControllerSystem::UpdateSurfaceAligned` implements a third rule that ignores `priority` entirely, so an authored zone priority applies to rigidbodies and is silently dropped for the SurfaceAligned character.

**B9. Registration boilerplate duplicated 62 times.** `AS_CHECK` is defined in 36 translation units (35 byte-identical). `extern ECS::World* s_BindingsWorld;` is hand-written in 26 `.cpp` files and declared in no header, then aliased again in two more (`FlashAPIShim.cpp:34`, `ScriptBindings_Flower.cpp:19`). `ASCallConv.h` already exists for exactly this.

**B10. Two more.** Module-name derivation (`parentDir_stem`, a documented trap) is duplicated at `ScriptEngine.cpp:254` and `ScriptSystem.cpp:219`, both carrying comments saying they must match. Entity transform access from script is implemented three times (`ScriptBindings.cpp:300-351` `EntityHandle`, `:377-437` `TransformProxy`, `ScriptBindings_Scene.cpp:64-133` `Entity_*`), two of them body-for-body identical on the euler degree/radian conversion that CLAUDE.md records as having already caused a multi-day bug. `TransformProxy` is unreachable: registered with only a default constructor, nothing produces one bound to a real entity, and `enjin_api/TegeBehavior.as` has no `transform` member. A script writing `TransformProxy t; t.position = v;` compiles and does nothing.

**B11. Footsteps solved twice; the surface-aware half is unreachable.** `FootstepSystem` (authored intervals, volume, pitch, per-surface table) and `SurfaceResponseSystem`'s auto-footstep (`kStepLength = 1.7f`, `kWalkMinSpeed = 0.4f` etc. as `static constexpr`) both tick in all three runtimes. Their entity sets barely overlap: FootstepSystem needs an FP/TP controller, SurfaceResponse needs a `RigidbodyComponent` that `ControllerSystem` never adds. The half that raycasts the ground never runs for player characters. And `FootstepComponent::currentSurface`, the key into the per-surface table, has **zero writers**, so only `defaultWalkSound`/`defaultRunSound` ever play.

**B12. Four save systems.** `Gameplay::SaveSystem` is dead except one static (`SaveBackend.cpp:2` is its only include). `Scene::GameSaveSystem` (384 lines) has zero references anywhere. `ECS::SaveSystemComponent` (30 authored fields) duplicates `AutoSaveConfig` field-for-field and has no reader outside the serializer. The slot count is hardcoded four times (20, 17, 17, 20). `SaveDataComponent`'s per-entity `savePosition`/`saveRotation`/`saveScale` flags are inspector-editable and ignored by `CollectEntitiesByTier`. `ApplySaveJson` never restores `runState`, so the three-tier save system restores no tiers while writing each entity twice.

---

## Tier C: dead systems and inert authored controls

**C1. The list of systems a runtime must tick is written by hand three times, and has drifted.** `PlayMode.cpp`, `Player/src/main.cpp` and `web_main.cpp` each enumerate it independently. `Player/src/main.cpp:3250` even carries a comment about a previous instance of this bug ("Editor PlayMode ticks this; the shipped player must too, or vcams do nothing in exported games"). Current gaps, all verified with hard counts:

| Missing from | What |
|---|---|
| both players | `SwarmSystem` (0 occurrences in either; component is in the Add menu and serialized, and its proxies are created inside `Update`, so a swarm is entirely absent in a shipped game) |
| both players | `ParallaxSystem::Render` + `ParallaxMachineComponent` (an ImGui-drawlist overlay; the sibling `ParallaxLayerComponent` path works everywhere, so there are two parallax implementations and one is editor-only) |
| all three | `Animation::TimelineSystem` (only tests reference it, while SWF import, the Flash editor, the scene serializer and `FlashAPIShim`'s `gotoAndPlay`/`play`/`stop` all write to `TimelineComponent`) |
| all three | `AnimationRecorderSystem::Update` (Record/Stop are wired in the editor UI; `Update`, which samples the poses, has no caller, so Stop always logs "no keyframes recorded") |
| all three | `DynamicDifficultySystem` (full 130-line inspector, Add-Component entry, serialization; its multipliers are read only by the editor's own readout) and `FaceCardSystem` |
| all three | `RagdollSystem::ActivateRagdoll` / `DeactivateRagdoll` / `UpdateRagdolls` (zero callers; `GameplayLoop.cpp:528` inlines a partial copy of Activate that skips the skeleton guard, and nothing advances `blendProgress`, so a corpse freezes in its last animated pose forever) |
| web only | `FlowerSystem` and the four generators (`Dungeon`, `Scatter`, `Terrain`, `WFC`). None has a platform guard or a Vulkan dependency, so these are plain omissions, not deferrals |

**C2. Authored controller fields no runtime reads.** Each has an inspector widget and a serializer entry, and zero readers: `enableWallJump` + `wallJumpForce`, `cameraAngle`, `enableCameraCollision` + `cameraCollisionRadius`, `enableLockOn` + `lockOnRange`, `arrivalThreshold`, `groundCheckDistance`, `disableOnUnpossess`, and `VehicleController::grip` / `downforceMultiplier` / `mass`. Two of these are advertised by the editor's own templates: `EditorLayerProjectHub.cpp:2788` tells the user "Enable wall jumping: Platformer2DController.enableWallJump", and `:4743/:4870` sets `cameraAngle = 45.0f` with help text saying it "gives isometric view". CLAUDE.md carries a whole trap paragraph about `cameraAngle`'s semantics for a field nothing reads.

**C3. Notify and config fields with no reader.** `TriggerZoneComponent::onEnterNotify/onExitNotify/onStayNotify` (and `GameplayLoop.h:93` documents that `UpdateTriggerZones` fires them; `:743` only sets `hasTriggered`). `HealthComponent::onDamageNotify/onDeathNotify/onHealNotify`. `SpawnPointComponent` has a full inspector and a viewport gizmo and **nothing spawns**, while `GameplayLoop.cpp:560` respawns the player at a hardcoded `Vector3(0, 2, 0)`. `SavePointComponent` has a five-function script API and nothing saves; nothing sets `used`, so `SavePoint_IsUsed` always returns false. Rewind's Animation/Physics/Material channel checkboxes are drawn twice with hand-copied magic bits; `linearVelocity`/`angularVelocity` are read at `RecordRewindSystem.cpp:120` and never captured, so every restore zeroes velocity.

**C4. Quest graphs cannot advance.** `QuestFlow.cpp:132-161` `EvaluateCondition` returns `true` on every path, including a dead `nodeCounters.find(0) // placeholder`, while `QuestFlowEditor.cpp:128-141` invites the user to fill in `conditionType`/`key`/`operator`/`value`. Objective nodes gate on `nodeCounters[nodeId] >= targetCount` and the only writer of `QuestFlowComponent::nodeCounters` in the repo is the editor's read-only display, so any graph containing one stalls forever. `QuestSystem::DrawQuestLog` has no caller and `SetWorld` is never called.

**C5. Stubs that report success.** `AS3Transpiler` emits 26 function names no binding registers (`Debug_Print` vs the real `Debug_Log`, `Input_IsKeyDown` vs `Input_GetKey`, `Math_Min` vs bare `Min`, plus `KEY_LEFT` which does not exist and undeclared `this_entity`/`root_entity` identifiers), covering `trace()`, `_x`/`_y`, `gotoAndPlay`, `Key.isDown`, `Stage.width` and `getTimer`. `Transpile` sets `success = true` unconditionally and the editor panel prints "Transpiled: N lines, M matches". `FlashAPIShim`'s `Flash_PlaySound` logs "Flash_PlaySound: %s" and does nothing: `g_FlashAudio` is written by `SetFlashShimAudio`, which has zero callers, and read nowhere. `Flash_SetTimeout`/`SetInterval` insert into `s_Timers`, which is never iterated by anything, and `TimerEntry` has no callback field. `AS3Transpiler.cpp:311-327` maps AS `setTimeout` straight onto them. Eight `Flash*` C++ classes (~300 lines) are never registered with AngelScript (`grep -c RegisterObjectType` gives 0).

**C6. Dead code encoding a rule that contradicts the documented one.** `JoltBackend::MoveAndSlide` (`:1676-1760`) is on the public `IPhysicsBackend` interface and has zero callers. It re-derives every collider AABB by hand from four component types and multiplies by transform scale, while `CreateBodyForEntity` (`:481`) explicitly does not ("Collider size is in world space"). The first gameplay programmer to use it gets silently wrong bounds on any scaled entity. `CheckSphereCollision` is also uncalled. Other fully unreferenced files: `Scene/BrickComposition` (186 lines), `Animation/TimelineEditor` (1096 lines, while the editor uses the unrelated `Editor::FlashTimelineEditor`), `Animation/MotionMatching` (359 lines, roadmap-tracked), `Gameplay/SaveLoadMenu::DrawSaveLoadMenu` (against a `SaveLoadMenuComponent` struct declared twice in two namespaces), and `ObjectPool::Acquire(poolId, world)`, the validating overload, while both real call sites use the unvalidated one.

---

## Tier D: wasted work

- `BehaviorTreeExecutor` re-parses node properties with `atof`/`atoi` on **every tick** (`:213, 233, 349, 385, 423, 458, 468`) and rediscovers topology in `GetChildren` (`:99-127`), which heap-allocates, linear-scans all links per pin, linear-scans all nodes per link, then sorts calling `FindNode` twice per comparison. O(N·L) per agent per frame for a graph that changes only in the editor.
- `RecordRewindSystem.cpp:414-439` snapshots every entity **twice** per scene-rewind tick, discarding the first result. With `deltaCompression` off, the cache it builds is never read and holds a second full copy of the scene that `GetMemoryUsageBytes` does not count.
- `EditorLayerPanels.cpp:5884` calls `GetAllSlots()` every frame the panel is open: up to 20 full-scene JSON parses per frame.
- `ControllerSystem::UpdateSurfaceAligned` scans all gravity zones three separate times per entity per frame (`:2110, 2227, 2294`), with the fallback radius `halfExtents.x * 0.1f` written twice.
- `AISystem.cpp:123-129` takes the world recursive mutex once per live agent per frame in a cleanup sweep, immediately after the main loop was deliberately optimized (`:67-70`) to avoid exactly that. Wrapping it in the existing `checkValid` flag is a one-line fix.
- `ScriptSystem::FixedUpdate` iterates `m_CachedScriptEntities`, which is assigned only at the top of `Update()` (`:379`), and `FixedUpdate` runs before `Update` in all three runtimes (`PlayMode.cpp:1012` vs `:1059`, `main.cpp:1026` vs `:1060`, `web_main.cpp:836` vs `:883`). Under a fixed timestep, a newly spawned entity misses `OnFixedUpdate` for a frame and the first frame after init does nothing at all.
- `UpdateGameOverState` iterates every `DamageComponent` entity twice with identical skip logic (`GameplayLoop.cpp:817-841`).
- `ClothSystem.cpp:696` recomputes `j % nx, j / nx` inside the O(n²) self-collision loop (~330k integer divisions per cloth per frame at the 24x24 sheet its own comment describes).
- `LevelStreaming.cpp:173-187` rebuilds a `unordered_map<string, StreamPriority>` over all chunks every frame to feed a sort comparator, for a priority fixed at `AddChunk`.
- `Script_StartCoroutine` (`ScriptBindings.cpp:571-573`) calls `obj->AddRef()` after `SetObject`, which already AddRefs script objects. Nothing releases it, so a behavior instance is never collectable after its first coroutine.

---

## Suggested order

A1, A2, A4, A5 first: each is a small, contained fix for a feature that is currently silently dead. Then A6 and A3 (both are "route through the one function that already does it right"). Then B1 and B2, which are the largest duplication surfaces and the ones most likely to drift again. C1 deserves a structural answer rather than seven one-line additions: a single `RegisterRuntimeSystems(...)` the three runtimes share would make the whole class of bug impossible.

Two things I checked and did **not** find: there is no second behavior-tree evaluator (`BehaviorTree.cpp` is enums plus a blackboard, `BehaviorTreeSystem.cpp` is a dispatcher), and `AnimationStateMachine` is genuinely live in all three runtimes via `AnimatorComponent::Update`, so its overlap with `ECS::StateMachineSystem` is not duplication. `WebStubs.cpp` and the non-Windows `MIDIInput` stubs are honest documented no-ops.