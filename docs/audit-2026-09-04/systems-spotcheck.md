I'll start by exploring the directory structure to understand what's there.

---

Let me search for hardcoded constant patterns across these files in parallel.

---

Findings — hardcoded designer-facing tuning values, ranked by likelihood a designer wants to change them.

## `D:\GitHub\enjin\Engine\src\ECS\Systems\AISystem.cpp`
1. **:290** — `3.0f` — seconds an AI keeps hunting past `loseTargetRange` before giving up chase. → `AIControllerComponent::giveUpTime` (component already has `loseTargetRange` right beside it at Gameplay.h:943).
2. **:359** — `ai.attackRange * 1.5f` — re-chase hysteresis: how far past attack range the target must drift before dropping back to Chase. → `AIControllerComponent::attackRangeHysteresis`.
3. **:452** — `ai.stateTimer > 5.0f` — max flee duration before returning to Idle. → `AIControllerComponent::maxFleeTime`.

## `D:\GitHub\enjin\Engine\src\Gameplay\GameplayLoop.cpp`
4. **:50, :317** — `jumpForce * 0.6f` — stomp bounce height. Classic platformer feel knob. → `Platformer2DController::stompBounceScale` (or `DamageComponent::stompBounce`).
5. **:39, :311** — `velocity.y < -1.0f` — minimum downward speed to register a stomp instead of taking damage. → `DamageComponent::stompMinFallSpeed`.
6. **:42, :312** — `position.y + 0.3f` — height advantage required for a stomp. → `DamageComponent::stompHeightMargin`.
7. **:413** — `constexpr f32 kMargin = 0.25f;` — hazard-overlap forgiveness margin; directly controls how punishing spikes/lava feel. → `DamageComponent::contactMargin`.
8. **:301, :421, :470** — `0.4f/0.4f`, `0.3f/0.9f`, `0.4f/0.8f` — fallback player/enemy collision half-extents when no collider exists. → default-collider settings, not per-callsite literals.

## `D:\GitHub\enjin\Engine\src\ECS\Systems\ControllerSystem.cpp`
9. **:2212** — `f32 planetRadius = 5.0f;` — default planet radius for spherical-gravity walking when no `SphereCollider` on the zone. → `GravityZoneComponent::pointRadius`.
10. **:2230, :2296** — `gz->halfExtents.x * 0.1f` — same fallback derived by an arbitrary 10 % factor. → `GravityZoneComponent::pointRadius`.
11. **:1238** — `swimStrokeImpulse * 0.28f` — weakened stroke at the water surface (bob vs. launch). → `CharacterControllerBase::swimSurfaceStrokeScale` (sibling of your new `swimDrag`/`swimSpeedScale`).
12. **:1237** — `surfaceY - 0.25f` — depth band that counts as "at surface". → `CharacterControllerBase::swimSurfaceBand`.
13. **:1271** — `jumpForce * 0.7f` — ladder push-off strength. → `LadderComponent::pushOffScale` (component already owns `topBoost`, `climbSpeed`, `allowJumpOff`).
14. **:1278** — `topY - 0.2f` — mantle window at the ladder top. → `LadderComponent::mantleWindow`.
15. **:1599** — `5.0f * dt` — crouch/stand height transition rate. → `ThirdPersonController::crouchTransitionSpeed`.
16. **:1866** — `ctrl.isSprinting ? 1.5f : 1.0f` — head-bob frequency multiplier while sprinting. → `FirstPersonController::headBobSprintMultiplier` (sits next to existing `headBobFrequency`/`headBobAmplitude`).
17. **:1327** — `ctrl.cameraDistance -= scroll.y * 0.5f` — camera zoom units per scroll notch. → `ThirdPersonController::zoomSpeed`.
18. **:2063-2064** — `6000.0f` max RPM / `800.0f` idle RPM — drives engine audio pitch and any RPM gauge. → `VehicleController::maxRPM` / `idleRPM`.
19. **:1941** — `brakeForce * 1.5f` — handbrake deceleration multiplier. → `VehicleController::handbrakeMultiplier`.
20. **:2015** — `driftFactor * 0.3f` — grip loss under handbrake; the whole drift feel. → `VehicleController::handbrakeGripScale`.
21. **:1978** — `1.0f - speedFactor * 0.5f` — high-speed steering reduction. → `VehicleController::highSpeedSteerReduction`.
22. **:1960** — `acceleration * 0.5f` — reverse acceleration scale; **:1964** `currentSpeed > 0.5f` reverse-engage threshold. → `VehicleController::reverseAccelScale` / `reverseEngageSpeed`.
23. **:2018** — `Math::Abs(lateralVel) > 1.0f` — drift-detection threshold gating skid VFX/SFX. → `VehicleController::driftThreshold`.
24. **:1895/1897** — `sprintFOVIncrease * 4.0f` — sprint FOV blend rate. → `FirstPersonController::sprintFOVBlendSpeed`.
25. **:1286-1287** — `deceleration * 2.0f` — horizontal damping while on a ladder. → `LadderComponent::horizontalDamping`.
26. **:712, :749, :1609, :1628** — `0.3f` input magnitude to trigger a grid step. → `CharacterControllerBase::gridInputThreshold`.
27. **:647** — `checkDist = radius + 0.15f` — 2D wall-detection reach (affects wall-jump/wall-slide grab feel). → `Platformer2DController::wallCheckMargin`.
28. **:1478** — `distToGround < 0.15f` — grounded tolerance; **:588** `position.y <= 0.1f` implicit Y=0 ground plane. → controller `groundCheckDistance` / world settings.
29. **:1393, :1539** — `hBias * 0.3f` — over-shoulder camera look-target offset scale. → `ThirdPersonController::shoulderLookBias`.
30. **:1316, :1577, :2174** — `gamepadLookSensitivity * 100.0f` — hidden 100× on the designer-visible sensitivity field, so the exposed number is meaningless without reading code. → fold into the field's documented unit (deg/s).

## `D:\GitHub\enjin\Engine\src\Gameplay\SurfaceResponseSystem.cpp`
31. **:20** — `kStepLength = 1.7f` — world units travelled per automatic footstep (stride length). → `FootstepComponent::strideLength`.
32. **:21** — `kWalkMinSpeed = 0.4f` — speed below which the walker counts as standing. → `FootstepComponent::minMoveSpeed`.
33. **:23** — `kEventSuppress = 0.25f` — window muting auto-steps after an animation footstep event. → `FootstepComponent::eventSuppressTime`.
34. **:22** — `kGroundProbe = 1.5f` — how far below the walker to look for ground. → `FootstepComponent::groundProbeDistance`.
35. **:53** — `impact ? 20u : 12u` — surface particle burst counts. → `MaterialComponent::impactParticleCount` / `footstepParticleCount`.

## `D:\GitHub\enjin\Engine\src\Gameplay\DynamicDifficultySystem.cpp`
36. **:141** — `dd->hintCooldown = 30.0f - easeFactor * 20.0f;` plus **:142** `std::max(5.0f, ...)` — hint pacing base/range/floor, all hardcoded while every neighbouring weight and range is a serialized field. → `DynamicDifficultyComponent::hintCooldownBase` / `hintCooldownRange` / `hintCooldownMin`.
37. **:47** — `dd->recentDeaths / 5.0f` — deaths that equal "maximum struggle". → `DynamicDifficultyComponent::maxStruggleDeaths`.
38. **:77** — `std::min(elapsed / expected, 2.0f) / 2.0f` — the 2× overrun cap that saturates the time-pressure score. → `DynamicDifficultyComponent::timeOverrunCap`.

## `D:\GitHub\enjin\Engine\src\ECS\Systems\DialogueSystem.cpp`
39. **:660** — `std::max(3.0f, len * 0.05f)` — auto-advance reading pace: 3 s minimum, 0.05 s per character. Pure designer/accessibility tuning. → `DialogueBoxComponent::minDisplayTime` / `secondsPerCharacter`.

## `D:\GitHub\enjin\Engine\src\Gameplay\FootstepSystem.cpp`
40. **:33, :41** — `velocity.Length() > 0.5f` — movement threshold for footstep playback, duplicated per controller type. → `FootstepComponent::minMoveSpeed` (component already has `walkStepInterval`/`runStepInterval` at Gameplay.h:2292).

## `D:\GitHub\enjin\Engine\src\AI\BehaviorTreeExecutor.cpp`
41. **:231** — `f32 duration = 5.0f;` — default Timeout-node duration when the graph omits the `duration` property; silently changes BT behaviour. → BT node default in the node schema/settings.

## `D:\GitHub\enjin\Engine\src\Gameplay\CameraDirector.cpp`
42. **:122** — `f32 liveDamping = 0.3f;` — damping used when no vcam wins; every other path reads `vc->damping`. → `CameraDirectorSettings::defaultDamping`.

## `D:\GitHub\enjin\Engine\src\ECS\Systems\ParallaxSystem.cpp`
43. **:142, :144** — `viewportHeight / 12.0f` — hardcoded 12-world-unit ortho view height, duplicated twice; must match the 2D camera or layers desync. → read from the camera's `orthoSize`, or `ParallaxSettings::worldUnitsPerScreen`.

## `D:\GitHub\enjin\Engine\src\Gameplay\ClothSystem.cpp`
44. **:370** — `0.7f + 0.45f * sin(time * 1.9f + pos.x * 0.7f)` — wind gust base/amplitude/frequency for cloth. → `ClothComponent` wind fields (`windGustBase`, `windGustAmplitude`, `windGustFrequency`).
45. **:220** — `radius = 0.3f, totalHalfH = 0.8f` — duplicated ControllerSystem capsule defaults, with a comment admitting the coupling; will silently desync if the controller default changes.

Notes: the strongest single cluster is ControllerSystem's vehicle block (:1941-2064), where nearly every field is serialized except the multipliers that actually define handling feel. The GameplayLoop stomp triple (:39/:42/:50, duplicated at :311/:312/:317) is the highest-value fix per line — it is duplicated 2D/3D logic, so a designer tuning stomp feel currently has to edit two places in C++.