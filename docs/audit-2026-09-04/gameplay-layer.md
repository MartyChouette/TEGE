I'll start by reading the project context and then the Gameplay sources.

---

I have my own findings complete. Waiting on the ClothSystem/RecordRewind sub-audit.

---

Waiting on the sub-audit of ClothSystem/RecordRewindSystem before finalizing.

---

I'll hold until the sub-audit reports back.

---

Still holding for the sub-audit result.

---

Now spot-verifying the sub-audit's biggest claims before I relay them.

---

I read all 18 `Engine/src/Gameplay/*.cpp` and their headers, and verified every claim with repo-wide greps. Findings ranked by value/risk. (`ClothSystem.cpp` and `RecordRewindSystem.cpp` were audited by a sub-agent; I re-verified each of its claims against the source before including them.)

---

## 1. The cinematic camera never moves the view — in any runtime

`Engine/src/Gameplay/CinematicSystem.cpp:60-67, 93-100` writes the interpolated waypoint pose into the `Renderer::Camera*` argument, and line 105 writes `transform->position` of the **cinematic entity itself**. But every runtime rebuilds the render camera from the active `CameraComponent` **entity** transform after the update:

- Editor game view: `Engine/src/Editor/EditorLayer.cpp:3067-3074` constructs a fresh `Renderer::Camera` from `cameraComp` + `cameraTransform`.
- Player: `Player/src/main.cpp:1386-1397` — `m_Camera->SetPosition(camTransform->position)` / `SetLookAt(...)`, in the render block, after `m_CinematicSystem.Update(..., m_Camera.get(), ...)` at line 1079.
- Web: `Player/src/web_main.cpp:1040-1058` `SyncCameraToWorld()`, same thing.

`CameraDirector.cpp:189-196` solves this correctly and its comment says exactly why ("the GAME VIEW renders from the CameraComponent entity's transform, not from the Renderer::Camera above"). `CinematicSystem` never got that fix. The component is fully authored in the editor (`EditorLayerComponents.cpp:8453` waypoint list, easing, fov, holdTime), the system is enabled in all three runtimes, and `DialogueSystem.cpp:104` triggers `Play()` from dialogue nodes. **Cost:** a user authors a cutscene, presses play, and the camera sits still — with no error anywhere.
**Fix:** move the "write the camera entity transform + CameraComponent.fieldOfView" block out of `CameraDirector::Update` into one shared `ApplyCameraPose(world, gameCamera, pos, lookPoint, fov)` and call it from both systems.

Same file pair also duplicates the projection triple: `CinematicSystem.cpp:66` and `:99` and `CameraDirector.cpp:182` each hardcode `SetPerspective(fov, 16.0f/9.0f, 0.1f, 1000.0f)` while `CameraComponent` carries authored `nearPlane`/`farPlane`/`GetAspectRatio(w,h)`.

---

## 2. The contact-damage rule exists five times; the script copy silently disables death

The sequence *shield absorb → i-frame check → subtract health → reset `timeSinceLastDamage` → start i-frames → knockback → set `isDead`* is written out independently at:

- `GameplayLoop.cpp:66-111` — `ProcessContactDamage` (the canonical one)
- `GameplayLoop.cpp:225-261` — `CheckHazardOverlaps` (2D)
- `GameplayLoop.cpp:325-357` — `CheckEnemyOverlaps2D`
- `Engine/src/Scripting/ScriptBindings_Components.cpp:81-103` — `Health_Damage`, the script API (`RegisterGlobalFunction("void Health_Damage(uint64, float)")` at `:2095`)

`CheckHazardOverlaps3D` (`:497`) is the one that does it right — it calls `ProcessContactDamage`, with a comment "Shared damage path: respects damageOnce, i-frames, shields, death."

The copies have already diverged:
- **`Health_Damage` never sets `isDead`.** The only five `isDead = true` writers are `GameplayLoop.cpp:46,105,260,316,353` plus `AISystem.cpp:392`. `UpdateHealthSystems` (`:523`) and `UpdateGameOverState` (`:790`) both key on `isDead`, never on `currentHealth <= 0`. So a game that deals damage from AngelScript drops the player to 0 HP and **no death, no respawn, no game-over screen ever fires**.
- `Health_Damage` also absorbs shield *before* the i-frame check (`:86-94`), so invulnerable characters still lose shield — the other three check i-frames first.
- It sets `hc->onDamageNotify = true` / `onDeathNotify = true` on fields typed `ECS::Entity` (u64), writing entity handle `1`.

The vertical knockback factor `* 0.5f` is a bare literal in three of them (`:96`, `:254`, `:347`) while `knockbackForce` itself is an authored `DamageComponent` field. The Mario stomp rule likewise still exists twice (`:39-55` and `:311-322`) — the magic numbers were correctly moved to `stompMinFallSpeed`/`stompMinHeight`/`stompBounceScale`, but the two implementations remain and must agree.

**Fix:** one `ApplyDamage(world, damager, target, damage, knockbackForce, deferredDestroys)` used by all five, including the script binding.

---

## 3. Save configuration and slot logic exist three to four times; the authored one is read by nobody

- **`Engine/src/Gameplay/SaveSystem.cpp` is dead except one static.** `MAX_SLOTS = 10`, `SaveToSlot`/`LoadFromSlot`/`DeleteSlot`/`GetSlotInfo`/`GetAllSlots`/`QuickSave`/`QuickLoad` and the `SaveSlot` struct have **zero callers repo-wide**. Grep for `Gameplay::SaveSystem` / includes of `Gameplay/SaveSystem.h` returns only `SaveBackend.cpp:27` calling the static `GetSaveDirectory()`. `Engine/src/Scene/GameSaveSystem.cpp` is a *third* slot-save implementation with its own `GetSaveDirectory`/`GetSlotPath`/`SaveSlotInfo` — grep for `GameSaveSystem` outside its own `.cpp` returns nothing at all.
- **`ECS::SaveSystemComponent`** (`Gameplay.h:2899`, 30 authored fields: auto-save enable/interval/slot count, cloud sync, save points, meta progression) has **no reader outside `SceneSerializer.cpp`**. It duplicates field-for-field the `AutoSaveConfig` struct (`TieredSaveSystem.h:27-34`) the runtime actually uses, down to `autoSaveSlotCount = 3`, and carries its own duplicate `autoSaveTimer`/`autoSaveRotation` runtime state. Its serializer comment (`SceneSerializer.cpp:5456`) claims "its header says it is exposed to the editor inspector, serialization and scripting" — there is no `DrawSaveSystemComponent` anywhere.
- **The slot count is hardcoded four times:** `TieredSaveSystem::MAX_SLOTS = 20` (h:39), `MANUAL_SLOTS = 17` (h:40, no code users), `SaveSystemComponent::maxManualSlots = 17`, and `kMaxSaveSlots = 20` in `ScriptBindings_Save.cpp:35` re-validating what the callee already validates.
- **`SaveDataComponent`'s per-entity flags do nothing.** `savePosition`/`saveRotation`/`saveScale`/`saveEnabled`/`tags`/`customData` are inspector-editable (`EditorLayerComponents.cpp:6390-6392`) and serialized, but `CollectEntitiesByTier` (`TieredSaveSystem.cpp:59-78`) serializes the whole entity via `SceneSerializer` and ignores all of them. `PersistenceTier::MetaProgression` is never collected (only `SceneState` at `:87` and `RunState` at `:110`) — yet `EditorLayerProjectHub.cpp:4571` sets `sd.tier = MetaProgression` in a template with the tooltip "SaveDataComponent with MetaProgression persists across runs."
- **The tier data is written but never restored.** `ApplySaveJson` (`:141-185`) restores only `sceneData` (the full scene snapshot); it never reads `runState`, and reads `sceneStates` only to refill a cache that is then written straight back out. So every save file carries each tier entity twice for nothing, and the "3-tier save system" in the class comment restores no tiers.

**Fix:** delete `Gameplay::SaveSystem` and `Scene::GameSaveSystem`; make `SaveSystemComponent` the single authored source that `TieredSaveSystem::ConfigureAutoSave` reads at play start (deleting `AutoSaveConfig`'s duplicate fields), with `MAX_SLOTS` the one slot-count constant.

---

## 4. Entity rewind's timeline is reconstructed by index arithmetic and double-counts the playhead

`RecordRewindSystem.cpp:243-275`. `EntitySnapshot` has no timestamp (unlike `DeltaFrame::timestamp`, `RewindChannel.h:69`), so frame times are derived from `currentRecordedTime - (Count()-1-i) * recordInterval` — the same expression written four times (`:248`, `:259`, `:260`, `:561`). Then the pop loop **decrements the anchor it just measured against**:

```
236:  rr->rewindPlayhead += deltaTime * rr->rewindSpeed;   // accumulates
243:  f32 targetTime = rr->currentRecordedTime - rr->rewindPlayhead;
271:      rr->currentRecordedTime -= rr->recordInterval;   // moves the anchor back too
```

The offset is applied twice, then three times, and so on. **Cost:** hold-to-rewind on a `RecordRewindComponent` accelerates instead of playing at `rewindSpeed` — a 5 s buffer empties in a fraction of a second. Scene rewind on the same scene is correct (it pops by timestamp, `:385`, and leaves the anchor alone), so it reads as a mystery rather than a bug. Line 240 also computes a never-used `latestTime` from `history.Back().position.x` under the comment "timestamp stored in EntitySnapshot" — it is a position.
**Fix:** add `f32 timestamp` to `EntitySnapshot`, stamp at capture, and share one timestamp-keyed `FindSurroundingFrames` + `PopFramesNewerThan` between both paths.

---

## 5. Delta→full-state reconstruction written twice, and both copies drop entities

`RecordRewindSystem.cpp:358-382` (`UpdateSceneRewind`) and `:528-545` (`SeekSceneToTime`) are the same ~25-line algorithm with renamed locals (`alreadyRestored`/`covered`, `bestA`/`best`). Both restore the target frame, then walk to the nearest keyframe and skip any entity appearing in *any* intermediate frame `k+1..target`. An entity that changed at `k+3` and then stopped is **not in the target frame**, so it is skipped from the keyframe and never restored at all. With the defaults (`deltaCompression = true`, `keyframeInterval = 30`) only entities that changed in the exact target frame, or never changed since the keyframe, rewind correctly. **Cost:** anything that came to rest during the window — a dropped crate, a stopped patrol — stays in the present while the scene scrubs back. `TestRewindSimulation.cpp:83-86` moves its entity every frame, so it never hits the hole.
**Fix:** one `RestoreSceneFrame(sr, index)` that walks keyframe→index applying each frame in order (later wins), called from both sites; also drops O(frames² × entities) to O(frames × entities).

---

## 6. Two systems never instantiated by any runtime — one with a full editor UI

- **`DynamicDifficultySystem`** — grep across `Editor/`, `Player/`, `Engine/src/` finds it only in its own `.h`/`.cpp` and the web build's `link.txt`. Yet `DynamicDifficultyComponent` has an Add-Component entry (`EditorLayerInspector.cpp:672-674`), a 130-line inspector (`EditorLayerComponents.cpp:10013`), and serialization (`SceneSerializer.cpp:8560`). Its outputs are equally dead: `enemyDamageMultiplier`/`enemyHealthMultiplier`/`aiAggressionMultiplier`/`resourceDropMultiplier` are read *only* by the editor's own display (`EditorLayerComponents.cpp:10114-10141`) — no gameplay code consumes them. **Cost:** a user configures difficulty tracking, sees live multiplier readouts that never move, and nothing in the game changes.
- **`FaceCardSystem`** — same grep result, never instantiated. `FaceCardComponent` is serialized (`SceneSerializer.cpp:8631`) and its header (`Gameplay.h:2860-2869`) documents "When the expression changes, the system swaps the texture/sprite to match."

Also in `DynamicDifficultySystem.cpp:141`, `hintCooldown = 30.0f - easeFactor * 20.0f` overwrites the user's authored `hintCooldown` field with a magic formula, and `:48` hardcodes "5+ deaths = max struggle" beside the authored `deathWindow`.
**Fix:** instantiate them in the three runtimes alongside `FootstepSystem`/`QuestSystem`, or delete system + component + inspector together.

---

## 7. Footsteps are solved twice, and the surface-aware half is unreachable

`FootstepSystem` (timer cadence: `movementThreshold`, `walkStepInterval`, `runStepInterval`, a `surfaceSounds` table keyed by `currentSurface`, `volume`, `pitchVariance` — all authored) and `SurfaceResponseSystem`'s auto-footstep (distance cadence, `SurfaceResponseSystem.cpp:20-23`: `kStepLength = 1.7f`, `kWalkMinSpeed = 0.4f`, `kGroundProbe = 1.5f`, `kEventSuppress = 0.25f` — all `static constexpr`, none authored) both run every frame in all three runtimes (`PlayMode.cpp:1032/1142`, `main.cpp:1041-1042/1101`, `web_main.cpp:848/979`).

They pick their sound differently and their entity sets barely overlap: `FootstepSystem` requires a `FirstPersonController`/`ThirdPersonController`, `SurfaceResponseSystem` requires a `RigidbodyComponent` with physics-maintained `isGrounded`/`velocity` (`ControllerSystem` never adds one). The half that knows about surfaces — raycasting the ground and reading `MaterialComponent.footstepSound` — never runs for player characters. And the half that does run keys its surface table on **`FootstepComponent.currentSurface`, which has zero writers repo-wide** (grep: only the default initializer at `Gameplay.h:2305`, the read at `FootstepSystem.cpp:64`, and serialization). **Cost:** a user fills in per-surface walk/run sounds and only `defaultWalkSound`/`defaultRunSound` ever play.
**Fix:** one footstep system — keep `SurfaceResponseSystem`'s ground raycast (it is the thing that knows the surface), have it write `currentSurface` and honour `FootstepComponent`'s authored intervals/volume/pitch, and move `kStepLength`/`kWalkMinSpeed` onto the component.

---

## 8. "Is this entity a player?" is written out about twelve times

The five-controller check (`Platformer2D` / `TopDown2D` / `FirstPerson` / `ThirdPerson` / `TopDown3D`) appears longhand in `GameplayLoop.cpp` at `:130-134` (ProcessPickup), `:265-266`, `:359-360`, `:398-400`, `:507-509`, `:544-546`, `:571-575`, `:697-701`, `:800-804`, `:817-822`, `:828-833`, `:869-873`. **Cost:** a sixth controller type silently can't collect pickups, can't take hazard damage, can't trigger a game over, and can't enter a trigger zone — twelve edits with no compiler help.

Bundled waste in the same function: `UpdateGameOverState` iterates every `DamageComponent` entity **twice** with byte-identical skip logic — once for `anyEnemyAlive` (`:817-828`) and again for `hasEnemies` (`:829-841`) — every frame, when both could be computed in one pass.
**Fix:** one `IsPlayerControlled(world, e)` predicate plus one `GatherPlayers(world)` helper.

---

## 9. Every entity is snapshotted twice per scene-rewind tick

`RecordRewindSystem.cpp:414-431` captures a snapshot per entity to build the frame, then `:435-439` iterates the **same** entity set and calls `CaptureEntitySnapshot` a second time on each to refresh `prevFrameCache` — throwing away the snapshot the first loop already computed. `CaptureEntitySnapshot` does up to five component lookups; at the default 15 Hz over a 2000-entity scene that is ~150k redundant lookups/sec. Worse, when `deltaCompression` is **off**, `isKeyframe` is unconditionally true (`:405`) so `prevFrameCache` is never read (`:425-427`) — yet it is still fully rebuilt each tick and holds a second complete copy of the scene (each `EntitySnapshot` carries a `std::string`), memory that `GetMemoryUsageBytes` (`:600-613`) does not count, so the editor's rewind HUD under-reports.
**Fix:** capture once, decide keyframe/delta, move that snapshot into the cache — and guard the cache behind `if (sr->deltaCompression)`.

---

## 10. A family of authored "event" and config fields with no reader anywhere

Each verified by repo-wide grep — inspector and `SceneSerializer` hits only:

| Field / component | Where authored | Reader |
|---|---|---|
| `TriggerZoneComponent::onEnterNotify` / `onExitNotify` / `onStayNotify` (`Gameplay.h:226-228`) | serialized `SceneSerializer.cpp:4000-4015` | none — and `GameplayLoop.h:93` documents that `UpdateTriggerZones` "fire[s] onEnter/onExit notify entities"; the implementation (`:743-748`) only sets `hasTriggered` |
| `HealthComponent::onDamageNotify` / `onDeathNotify` / `onHealNotify` (`Gameplay.h:51-53`) | serialized `:3247-3273` | none (only the bogus `= true` writes in finding 2) |
| `SpawnPointComponent` — `spawnOnStart`, `prefabToSpawn`, `respawnTime`, `maxSpawns`, `spawnRadius` | full inspector `EditorLayerComponents.cpp:6153`, viewport gizmo `EditorLayer.cpp:4867` | **nothing spawns** — no runtime reader of `spawnOnStart`/`prefabToSpawn`/`spawnTimer` |
| `SavePointComponent` | full script API — `SavePoint_SetSlot`/`SetSaveOnEnter`/`SetRadius`/`SetMessage`/`IsUsed` (`ScriptBindings_GameplayComponents.cpp:199-219`) | nothing reads `saveOnEnter`/`radius`/`slotTarget` to save; nothing sets `used`, so `SavePoint_IsUsed` always returns false |
| Rewind channels Animation (0x08) / Physics (0x10) / Material (0x20) | six checkboxes drawn twice with hand-copied magic bit values, `EditorLayerComponents.cpp:3912-3917` and `:3979-3984` | Animation capture is restored nowhere (`RecordRewindSystem.cpp:112-114`, `:163-164`); `snap.linearVelocity`/`angularVelocity` appear on exactly one line, `:120`, where they are **read** — never captured, so every restore zeroes velocity; `opacity`/`baseColor` are never referenced |
| `rewindVignetteStrength` (`Gameplay.h:2797`, `:2839`) | inspector `:3922`, `:3989`, serialized `:3290`, `:3323` | none |

Directly related: `GameplayLoop.cpp:560-572` respawns a dead player at the hardcoded `Vector3(0, 2, 0)` with `invulnerabilityTimer = 1.0f`, in an engine that has an authored, gizmo-drawn `SpawnPointComponent` sitting unused.
**Fix:** either fire these (one `NotifyEntity(world, target, event)` helper for the three notify families) or delete field + inspector row + serializer entry together.

---

## 11. Quest graphs cannot advance, and the quest HUD never draws

- **`QuestFlow.cpp:132-161` `EvaluateCondition` returns `true` on every path.** `"questComplete"` returns `flow.status == Completed`, which is never true while the graph is being evaluated (status only flips at an `End` node, after which the function returns). `"variable"`/`"custom"` does a dead `nodeCounters.find(0) // placeholder` into an unused local, then returns true. So `Condition` and `Branch` nodes always take output 0 — while `QuestFlowEditor.cpp:128-141` seeds every one of them with editable `conditionType`/`key`/`operator`/`value` properties the user is invited to fill in.
- **`Objective` nodes never complete.** `AdvanceQuestFlow:210` gates on `flow->nodeCounters[nodeId] >= targetCount`, and the only writer of `QuestFlowComponent::nodeCounters` in the repo is the editor's read-only progress display (`QuestFlowEditor.cpp:502`). No script binding, no visual-script node, no gameplay path increments it. Any authored graph containing an Objective node stalls forever in all three runtimes.
- **`QuestSystem::DrawQuestLog`** (`QuestSystem.cpp:93`, header: "HUD overlay (draws active quests in corner during play)") has no caller, and `QuestSystem::SetWorld` is never called either, so `m_World` is permanently null and it would early-return anyway.

**Fix:** a `QuestFlow_AddProgress(entity, nodeId, n)` binding (mirroring `BehaviorTreeExecutor.cpp:216`, which does increment its counters), a real `EvaluateCondition` backed by `TieredSaveSystem`'s meta store, and either call `DrawQuestLog` from the three runtimes or delete it.

---

## 12. `DrawSaveLoadMenu` is dead, against a duplicated component struct

`SaveLoadMenuComponent` is declared **twice** with identical fields: `Enjin/Gameplay/SaveLoadMenu.h:14` (namespace `Gameplay`) and `Gameplay.h:1975` (namespace `ECS`). The editor adds, inspects and serializes the `ECS::` one (`EditorLayerInspector.cpp:667-669`, `EditorLayerComponents.cpp:6462`, `SceneSerializer.cpp:8615`); `DrawSaveLoadMenu` (`SaveLoadMenu.cpp:9`) takes the `Gameplay::` one and **has no caller anywhere** — grep returns only its own declaration and definition. Meanwhile `EditorLayerPanels.cpp:5849-5960` implements a third, near-identical save-slot grid. **Cost:** a user adds the Save/Load Menu component, sets `showOnPause`, and no menu ever appears in play mode, the player, or web.

Adjacent waste: that editor panel calls `saveSystem->GetAllSlots()` (`EditorLayerPanels.cpp:5884`) **every frame it is open**. `GetAllSlots` (`TieredSaveSystem.cpp:251-258`) reads and fully JSON-parses all 20 save files, each of which embeds a complete scene snapshot — up to 20 full-scene parses per frame.

---

## 13. Cloth/rope tuning constants and hot-loop waste

- **`ClothSystem.cpp:98`** — `strength = 0.85f;` for links beside the stitching, the value that decides *where a tear runs*, is a literal sitting between its two authored siblings `c.seamStrength` (`:96`) and `c.pinStrength` (`:100`), both inspector-editable (`EditorLayerInspector.cpp:2087`, `:2098`) and serialized. It already has a second copy that must agree: `Cloth.h:62` documents "neighbors get ~0.85 automatically". **Fix:** `nearSeamStrength` on `ClothComponent`.
- **`ClothSystem.cpp:617` and `:775`** each write `-9.81f * gravityScale` and `1.0f - clamp(damping, 0, 0.5f)` out longhand for cloth and rope. The `9.81f` is a literal while scene gravity is authored (`EditorLayerSettings.cpp:1537`), so cloth in a low-gravity scene still falls at 9.81.
- **`ClothSystem.cpp:696-698`** — `jx = j % nx, jy = j / nx` is recomputed inside the O(n²) self-collision pair loop; two integer divisions per pair, ~330k per cloth per frame at the 24×24 sheet the file's own comment (`:687`) describes, for a value that costs O(n) to precompute.
- **`RopeComponent::collidable` / `collisionRadius` / `friction` / `collisionSkin` / `wind`** are read by `GatherColliders` (`ClothSystem.cpp:244-249`), serialized (`SceneSerializer.cpp:1334-1362`) and covered by a 230-line test, but `DrawRopeComponent` (`EditorLayerComponents.cpp:3110-3164`) exposes none of them and there is no script path. The clothesline-drape behaviour is reachable only by hand-editing `.enjin` JSON — including the trap at `Rope.h:48-54` that `collisionRadius` must exceed the cloth point spacing or nothing catches. Cloth exposes all of its equivalents.

---

## 14. Minor

`ObjectPool::Acquire(poolId, world)` (`ObjectPool.cpp:98`) — the overload that discards stale entities — has **zero callers**. Both real call sites use the unvalidated one-arg version: `ScriptBindings_Gameplay.cpp:103` (`Pool_Acquire`) and `NodeRegistry.cpp:7680`. Two implementations of the same operation, the safer one dead.

---

That notification is just the leftover wait-loop I started earlier being cleaned up; it has no bearing on the audit. All background tasks are now stopped and the audit is complete — my findings are in the message above.