# AngelScript API Reference

Complete reference for all functions callable from AngelScript via `TegeBehavior` scripts. ~1,010 functions across all categories.

The `TegeBehavior` base class and the `enjin_api` helper scripts (Timer, Tween, Math, StateMachine) are embedded in the engine. Scripts need no `#include` for TegeBehavior (it is auto-injected unless the source references `TegeBehavior.as` itself), and `#include "Timer.as"` etc. resolve from the embedded copies when no `enjin_api` folder exists. A project-local `scripts/enjin_api/` overrides the embedded copies.

---

## Math Types

- **Vector2**: `x`, `y`, `Length()`, `Normalized()`, `Dot()`, operators `+`, `-`, `*`, `/`, unary `-`
- **Vector3**: `x`, `y`, `z`, `Length()`, `Normalized()`, `Dot()`, `Cross()`, operators
- **Vector4**: `x`, `y`, `z`, `w`
- **Quaternion**: `x`, `y`, `z`, `w`, `Rotate(Vector3)`, `Normalized()`, `Inverse()`, `ToEuler()`, operators. Statics: `Quaternion_Identity()`, `Quaternion_FromEuler(Vector3)`, `Quaternion_Slerp(q1, q2, t)`

## Global Math Functions

`Abs`, `Sin`, `Cos`, `Tan`, `Asin`, `Acos`, `Atan2`, `Sqrt`, `Pow`, `Floor`, `Ceil`, `Round`, `Min`, `Max`, `Clamp`, `Lerp`, `MoveTowards`, `Sign`, `Random()`, `RandomRange(min, max)`, `RandomInt(min, max)`, `Radians`, `Degrees`, `PI()`

## Entity & Transform

- `Entity_GetPosition/SetPosition(uint64, Vector3)`
- `Entity_GetRotation/SetRotation(uint64, Vector3)` — degrees
- `Entity_GetScale/SetScale(uint64, Vector3)`
- `Entity_GetName(uint64)`
- `Entity_SetVisible/Entity_IsVisible(uint64, bool)` — visibility toggle
- **Hierarchy**: `Entity_SetParent(uint64 child, uint64 parent)`, `Entity_RemoveParent(uint64)`, `Entity_GetParent(uint64)` — returns parent entity ID, `Entity_GetChildCount(uint64)`, `Entity_GetChild(uint64, int index)` — returns child entity ID
- **EntityHandle** class: `IsValid()`, `GetID()`, `GetPosition/SetPosition()`, `GetRotation/SetRotation()`, `GetScale/SetScale()`, `GetName()`, `HasTag(string)`
- **TransformProxy**: `position`, `rotation`, `scale`, `forward`, `right`, `up` (read-only)

## Scene Management

- `Scene_FindEntity(string name)`, `Scene_FindEntityByTag(string tag)`
- `Scene_DestroyEntity(uint64)`, `Scene_Instantiate()`, `Scene_InstantiateNamed(string)`, `Scene_InstantiateAt(Vector3)`
- `Scene_IsValid(uint64)`, `Scene_GetEntityCount()`
- `Scene_GetEntityName/SetEntityName(uint64, string)`
- `Scene_AddTag/RemoveTag/HasTag(uint64, string)`
- `Scene_LoadScene(string)`, `Scene_GetCurrentScene()`, `Scene_Restart()` — scene changes are DEFERRED to a safe point at the top of the next frame (never mid-script), and work in editor play AND exported games: the editor restarts/switches the play session, the player transitions scenes. Unknown scene names warn immediately.

## Time

`Time_GetDeltaTime()`, `Time_GetFixedDeltaTime()`, `Time_GetTime()`, `Time_GetTimeScale()`, `Time_SetTimeScale(float)`, `Time_GetFrameCount()`

- **OnFixedUpdate(float fixedDt)**: TegeBehavior lifecycle hook that runs once per physics tick. On fixed-timestep projects (Settings > Project > Fixed Physics Timestep) it lands exactly in step with the physics simulation - use it for forces and tick-locked gameplay. On classic projects it runs from a 60Hz accumulator. OnUpdate(dt) stays frame-paced either way.
- **Time scale**: `Time_SetScale(float)` / `float Time_GetScale()` — global slow-mo/hitstop (0..10, 1 = normal). Scales the dt gameplay systems receive (physics, scripts, tweens, animation); UI and the frame limiter stay real-time. Resets to 1 on play start.

## Debug

`Debug_Log(string)`, `Debug_LogWarning(string)`, `Debug_LogError(string)`

## Input — Keyboard

`Input_GetKey(int)`, `Input_GetKeyDown(int)`, `Input_GetKeyUp(int)` — Key enum: A-Z, Num0-9, F1-F12, Space, Escape, Enter, Tab, Backspace, arrows, Shift, Control, Alt, etc.

## Input — Text

`Input_GetTextInput()` → string — Characters typed this frame as a UTF-8 string. OS-processed (shift-aware, keyboard-layout-aware, dead-key/compose-aware). Supports full Latin range including accented characters. Returns empty string if nothing was typed.

`Input_GetTextInputCount()` → int — Number of characters typed this frame. Useful for checking whether any text input occurred without allocating a string.

## Input — Mouse

`Input_GetMouseButton/Down/Up(int)` — MouseBtn: Left, Right, Middle
`Input_GetMousePosition()`, `Input_GetMouseDelta()`, `Input_GetScrollDelta()`
`Input_IsMouseCaptured()`, `Input_SetMouseCaptured(bool)`

## Input — Gamepad

`Input_IsGamepadConnected(int)`, `Input_GetGamepadButton/ButtonDown(int, int)` — GamepadBtn: A, B, X, Y, bumpers, back, start, D-pad
`Input_GetGamepadAxis(int, int)` — GamepadAx: LeftX/Y, RightX/Y, triggers
`Input_GetGamepadLeftStick/RightStick(int)`, `Input_GetGamepadLeftTrigger/RightTrigger(int)`

## Physics

- `Physics_Raycast(origin, dir, maxDist)`, `Physics_RaycastHit(origin, dir, maxDist, &hit)` — RaycastHit: `point`, `normal`, `distance`, `entity`
- `Physics_CheckSphere(center, radius)`, `Physics_CheckBox(center, halfExtents)`
- Masked overloads (filter by collision group): `Physics_Raycast(origin, dir, maxDist, layerMask)`, `Physics_RaycastHit(origin, dir, maxDist, layerMask, &hit)`, `Physics_CheckSphere(center, radius, layerMask)`, `Physics_CheckBox(center, halfExtents, layerMask)`
- `Physics_AddForce/AddImpulse(uint64, Vector3)`, `Physics_SetVelocity/GetVelocity(uint64)`, `Physics_SetGravityScale(uint64, float)`
- `Physics_Teleport(uint64, Vector3)` — instantly move a dynamic body (and its entity) to a position, zeroing its velocities. Use for respawns/resets; setting the transform alone is overwritten by the physics step.
- **Overlap (entity list)**: `Physics_OverlapSphereEntities(Vector3, float)`, `Physics_OverlapBoxEntities(Vector3, Vector3)` — return count of overlapping entities. Masked variants: append `Mask` suffix + `uint layerMask`. Retrieve results: `Physics_GetOverlapResult(int index)` — returns entity ID.
- **Joints**: `Physics_CreateDistanceJoint(uint64 entityA, uint64 entityB, float restDistance)` — returns joint entity, `Physics_CreateHingeJoint(uint64 entityA, uint64 entityB, float axisX, axisY, axisZ)` — returns joint entity, `Physics_DestroyJoint(uint64)`. Accessors: `DistanceJoint_SetRestDistance(uint64, float)`, `DistanceJoint_GetCurrentStress(uint64)`, `HingeJoint_SetLimits(uint64, float lower, float upper)`, `HingeJoint_SetMotor(uint64, float speed, float maxForce)`, `HingeJoint_GetCurrentAngle(uint64)`
- **Collider physics**: `BoxCollider_GetFriction/SetFriction(uint64, float)`, `BoxCollider_GetBounciness/SetBounciness(uint64, float)` — same for `SphereCollider_` and `CapsuleCollider_` prefixes

## Audio

`Audio_Play(uint64)`, `Audio_PlayAtPosition(string, Vector3)`, `Audio_Stop(uint64)`, `Audio_StopAll()`
`Audio_SetVolume/SetPitch(uint64, float)`, `Audio_IsPlaying(uint64)`
`Audio_SetMasterVolume/GetMasterVolume(float)`
`Audio_SetChannelVolume(uint8, float)`, `Audio_GetChannelVolume(uint8)`, `Audio_StopChannel(uint8)`
Channel constants: `AUDIO_CHANNEL_SFX=0`, `AUDIO_CHANNEL_MUSIC=1`, `AUDIO_CHANNEL_UI=2`, `AUDIO_CHANNEL_VOICE=3`

## Component Access

- **Health**: `Health_Get/GetMax/SetCurrent(uint64)`, `Health_Damage(uint64, float)`
- **Material**: `Material_SetBaseColor/GetBaseColor(uint64, Vector3)`, `Material_SetMetallic/SetRoughness(uint64, float)`, `Material_SetTransmission/GetTransmission(uint64, float)`, `Material_SetIOR/GetIOR(uint64, float)`, `Material_SetThickness/GetThickness(uint64, float)`, `Material_SetSSSIntensity/GetSSSIntensity(uint64, float)`, `Material_SetSSSRadius/GetSSSRadius(uint64, float)`, `Material_SetSSSColor/GetSSSColor(uint64, Vector3)`, `Material_SetOutlineWidth/GetOutlineWidth(uint64, float)`, `Material_SetOutlineColor/GetOutlineColor(uint64, Vector3)`, `Material_SetSurfaceNoiseScale/GetSurfaceNoiseScale(uint64, float)`, `Material_SetSurfaceNoiseStrength/GetSurfaceNoiseStrength(uint64, float)`
- **Light**: `Light_SetColor/GetColor(uint64, Vector3)`, `Light_SetIntensity/GetIntensity(uint64, float)`, `Light_SetRange/GetRange(uint64, float)`, `Light_SetType/GetType(uint64, int)` — 0=Directional, 1=Point, 2=Spot, `Light_SetCastShadows/GetCastShadows(uint64, bool)`, `Light_SetSpotAngles(uint64, float inner, float outer)` — cone angles in degrees
- **Camera**: `Camera_SetFOV/GetFOV(uint64, float)`, `Camera_SetOrthoSize/GetOrthoSize(uint64, float)` — orthographic half-height (2D zoom), `Camera_SetProjectionType/GetProjectionType(uint64, int)` — 0=Perspective, 1=Orthographic, `Camera_SetNearFar(uint64, float near, float far)`
- **AudioSource**: `AudioSource_Play/Stop/SetClip/SetVolume(uint64, ...)`
- **Animator**: `Animator_Play(uint64, string)`, `Animator_CrossFade(uint64, string, float fadeTime)` — smooth blend to new animation, `Animator_SetSpeed(uint64, float)`, `Animator_Stop(uint64)`, `Animator_Pause(uint64)`, `Animator_Resume(uint64)`, `Animator_IsPlaying(uint64)`, `Animator_GetCurrentAnimation(uint64)`, `Animator_GetSpeed(uint64)`
- **Controller**: `Controller_SetMoveSpeed/GetVelocity(uint64, ...)`, `Controller_SetEnabled(uint64, bool)` — suspend/resume player control (menus, cutscenes); works with all 5 controller types
- **Viewmodel**: `Viewmodel_Set(uint64, bool)`, `Viewmodel_Get(uint64)` — first person viewmodel rendering (in front of world, no wall clipping, no shadows); typically on a weapon mesh parented to the camera
- **Camera2D**: `Camera2D_Shake(uint64, float intensity, float duration)`, `Camera2D_GetZoom/SetZoom(uint64, float)`, `Camera2D_AddTarget/RemoveTarget(uint64 camera, uint64 target)`, `Camera2D_ClearTargets(uint64)`, `Camera2D_SetDeadZone(uint64, float w, float h)`, `Camera2D_SetLookAhead(uint64, float distance, float smoothing)`, `Camera2D_SetFollowTarget/GetFollowTarget(uint64, uint64)`

## Virtual Cameras (the Camera Director)

The 3D camera system. Author `VirtualCamera` components (vcams) in the editor — a
follow target, an offset, a priority. The **Camera Director** activates the
highest-priority vcam and blends the real camera to it.

**Single-owner invariant:** while it is active, the Director is the ONLY thing
that writes the game camera's transform. This is what makes the bindings below
safe to expose — you drive *virtual cameras*, never the real camera, so you can
never fight the Director. Three tiers of access:

**Tier 1 — editor (no code).** Add vcams, set priority/targets. Can't go wrong.

**Tier 2 — directed (safe).** Drive vcams from a script; the Director still owns
the transform and does the blend. A camera "cut" is just raising a vcam's priority.
- `Camera_HasVCam(uint64)` — does this entity have a vcam?
- `Camera_SetVCamPriority(uint64, int)` / `Camera_GetVCamPriority(uint64)` — the highest-priority enabled vcam goes live; the Director blends over its `blendTime`.
- `Camera_SetVCamEnabled(uint64, bool)` — take a vcam out of / into the running.
- `Camera_IsVCamLive(uint64)` — is this the shot currently on screen?
- `Camera_SetVCamOffset(uint64, float x, y, z)` / `Camera_SetVCamFOV(uint64, float)` — retune a shot live.
- `Camera_ApplyVCamShot(uint64, int shot)` — apply a named preset (seeds framing, keeps priority/targets): `1`=Isometric, `2`=Over-the-Shoulder, `3`=Follow, `4`=Top-Down, `5`=Close-Up, `6`=Wide, `7`=Side-Scroller, `8`=Bird's-Eye, `0`=Custom.

**Tier 3 — manual (contract).** Take the wheel from the Director. **Contract:**
after `TakeManualControl`, the Director stops writing the transform and YOU own
it (via `Entity_SetPosition` on the camera, etc.) until you `ReleaseManualControl`,
which blends back to the live vcam. Control is a token — hold it or don't; there
is no in-between where you and the Director both write.
- `Camera_TakeManualControl(uint64 owner)` — Director yields.
- `Camera_ReleaseManualControl()` — Director resumes, blending back.
- `Camera_IsManualControl()` — is a script currently holding the token?

When a scene has **no** vcams, the Director is dormant and the controller/cinematic
camera path is untouched — adopting the system is opt-in per scene.
- **Tilemap**: `HasComponent_Tilemap(uint64)`, `Tilemap_GetTile(uint64, int x, int y)`, `Tilemap_SetTile(uint64, int x, int y, int tileIndex)`, `Tilemap_GetWidth/GetHeight(uint64)`
- **Existence checks**: `HasComponent_Health/Light/Camera/Material/AudioSource/Rigidbody/BoxCollider/Animator/Tilemap(uint64)`
- **ReflectionProbe**: `ReflectionProbe_SetIntensity/GetIntensity(uint64, float)`, `ReflectionProbe_SetBoxMin/SetBoxMax(uint64, Vector3)`, `ReflectionProbe_Bake(uint64)`
- **DynamicDifficulty**: `Difficulty_GetScore(uint64)`, `Difficulty_GetMultiplier(uint64, string)` — "enemyDamage"/"enemyHealth"/"aiAggression"/"resourceDrops", `Difficulty_SetBaseDifficulty/GetBaseDifficulty(uint64, uint)`, `Difficulty_RecordDeath(uint64)`, `Difficulty_RecordShot(uint64)`, `Difficulty_RecordHit(uint64)`, `Difficulty_RecordCheckpointHealth(uint64, float)`
- **State Machine**: `SM_AddState(uint64, string)`, `SM_AddTransition(uint64, from, to)`, `SM_SetState/GetCurrentState/GetPreviousState(uint64)`, `SM_GetStateTime(uint64)`, `SM_SendTrigger(uint64, string)`, `SM_SetBool/GetBool(uint64, string, bool)`, `SM_SetFloat/GetFloat(uint64, string, float)`, `SM_SetInt/GetInt(uint64, string, int)`, `SM_HasState(uint64, string)`, `SM_SetOnEnter/SetOnUpdate/SetOnExit(uint64, stateName, funcName)`, `SM_GetOnEnter/GetOnUpdate/GetOnExit(uint64, stateName)`

## Coroutines

`StartCoroutine(string)`, `YieldSeconds(float)`, `YieldFrames(uint)`, `YieldEndOfFrame()`

## Event System

- **EventData** class: `SetFloat/GetFloat`, `SetInt/GetInt`, `SetString/GetString`, `SetEntity/GetEntity`
- `Events_Listen(string, EventCallback@)` — returns listener ID
- `Events_Send(string, EventData@)`, `Events_Broadcast(EventData@)`
- `Events_CurrentFloat(string key)`, `Events_CurrentInt(string key)`, `Events_CurrentString(string key)` — read the payload of the event currently being dispatched (valid only inside an EventCallback). UI events bridge with keys `"value"` (slider), `"checked"` (toggle, 0/1), `"text"` (button label).

## Tweening

- `uint Tween_Position(uint64, const Vector3&in, float duration, int easing)` — returns tween index
- `uint Tween_Rotation(uint64, const Vector3&in, float, int)`, `uint Tween_Scale(uint64, const Vector3&in, float, int)`
- `uint Tween_Color(uint64, const Vector3&in, float, int)`, `uint Tween_Opacity(uint64, float, float, int)`
- `uint Tween_Float(uint64, float start, float end, float duration, int easing)` — generic float interpolation, no component write
- `void Tween_SetOnComplete(uint64, uint tweenIndex, const string&in funcName)` — set callback on tween completion
- `void Tween_SetDelay(uint64, uint tweenIndex, float delay)` — set delay on specific tween
- `float Tween_GetValue(uint64, uint tweenIndex)` — read current interpolated value (Float property)
- `void Tween_StopAll(uint64)` — stop and clear all tweens on entity

## Noise

- **Base 2D**: `Noise_Value2D(float, float, uint)`, `Noise_Perlin2D(float, float, uint)`, `Noise_Simplex2D(float, float, uint)`, `Noise_Worley2D(float, float, uint)`
- **Base 3D**: `Noise_Value3D(float, float, float, uint)`, `Noise_Perlin3D(float, float, float, uint)`, `Noise_Simplex3D(float, float, float, uint)`, `Noise_Worley3D(float, float, float, uint)`
- **Fractal 2D**: `Noise_FBM2D(x, y, octaves, lacunarity, persistence, frequency, seed)`, `Noise_Ridged2D(...)`, `Noise_Billow2D(...)`
- **Fractal 3D**: `Noise_FBM3D(x, y, z, octaves, lacunarity, persistence, frequency, seed)`, `Noise_Ridged3D(...)`, `Noise_Billow3D(...)`
- **Domain Warp**: `Noise_DomainWarp2D(x, y, warpStrength, frequency, seed)`, `Noise_DomainWarp3D(x, y, z, warpStrength, frequency, seed)`
- Value/Perlin/Simplex return [-1, 1], Worley returns [0, 1], FBM/Billow return [-1, 1], Ridged returns [0, ~2]

## Rendering

- **Shadows**: `Render_SetShadowsEnabled(bool)` / `Render_IsShadowsEnabled()`, `Render_SetShadowDistance/GetShadowDistance(float)`, `Render_SetShadowStrength/GetShadowStrength(float)`
- **Ambient**: `Render_SetAmbientIntensity/GetAmbientIntensity(float)`, `Render_SetAmbientColor/GetAmbientColor(Vector3)`
- **Fog**: `Render_SetFogDensity/GetFogDensity(float)`, `Render_SetFogColor/GetFogColor(Vector3)`, `Render_SetFogStart/GetFogStart(float)`, `Render_SetFogEnd/GetFogEnd(float)`, `Render_SetFogHeightFalloff/GetFogHeightFalloff(float)`
- **Weather**: `Render_SetSnowIntensity/GetSnowIntensity(float)`, `Render_SetRainActive/IsRainActive(bool)`
- **Effects**: `Render_SetWorldCurvature/GetWorldCurvature(float)`, `Render_SetWireframeEnabled/IsWireframeEnabled(bool)`
- **Render targets (live camera→texture)**: `uint64 RenderTarget_Create(int width, int height)`, `RenderTarget_SetCamera(uint64 handle, uint64 cameraEntity)`, `bool RenderTarget_BindToEntity(uint64 handle, uint64 entity)` (points the entity material's base color at the live target), `RenderTarget_Destroy(uint64 handle)`. The camera entity needs Camera + Transform components (leave it `isActive = false` so it doesn't take over the screen). One target renders per frame round-robin. Desktop only for now — on web `Create` returns 0 so scripts can degrade gracefully. Example: a hand mirror — point a second camera at the scene, bind the target to the mirror quad.

## Post-Processing

- **Tone Mapping**: `PostProcess_SetToneMapping/GetToneMapping(int)`, `PostProcess_SetExposure/GetExposure(float)`, `PostProcess_SetGamma/GetGamma(float)`
- **Bloom**: `PostProcess_SetBloomEnabled/IsBloomEnabled(bool)`, `PostProcess_SetBloomThreshold/GetBloomThreshold(float)`, `PostProcess_SetBloomIntensity/GetBloomIntensity(float)`
- **Vignette**: `PostProcess_SetVignetteEnabled/IsVignetteEnabled(bool)`, `PostProcess_SetVignetteIntensity/GetVignetteIntensity(float)`, `PostProcess_SetVignetteSmoothness/GetVignetteSmoothness(float)`
- **Chromatic Aberration**: `PostProcess_SetChromaticAberrationEnabled/IsChromaticAberrationEnabled(bool)`, `PostProcess_SetChromaticAberrationIntensity/GetChromaticAberrationIntensity(float)`
- **Color Grading**: `PostProcess_SetColorFilter/GetColorFilter(Vector3)`, `PostProcess_SetSaturation/GetSaturation(float)`, `PostProcess_SetContrast/GetContrast(float)`, `PostProcess_SetBrightness/GetBrightness(float)`
- **Film Grain**: `PostProcess_SetFilmGrainEnabled/IsFilmGrainEnabled(bool)`, `PostProcess_SetFilmGrainIntensity/GetFilmGrainIntensity(float)`
- **FXAA**: `PostProcess_SetFXAAEnabled/IsFXAAEnabled(bool)`
- Note: PostProcess_ functions return sensible defaults if PostProcessing is unavailable (e.g. in Player app)

## Dialogue

- `Dialogue_Start(uint64)`, `Dialogue_Advance(uint64)`, `Dialogue_Choose(uint64, int)`
- `Dialogue_SetVariable(uint64, string, string)`, `Dialogue_GetVariable(uint64, string)`
- `Dialogue_IsActive(uint64)`, `Dialogue_GetCurrentText(uint64)`, `Dialogue_GetCurrentSpeaker(uint64)`
- `Dialogue_GetChoiceCount(uint64)`, `Dialogue_GetChoiceText(uint64, int)`

## Save System

- `SaveGame_ToSlot(int slot)` — Save current game state to the specified slot (0-19). Returns `bool` success.
- `SaveGame_FromSlot(int slot)` — Load game state from the specified slot. Returns `bool` success.
- `SaveGame_DeleteSlot(int slot)` — Delete the save data in the specified slot. Returns `bool` success.
- `SaveGame_Checkpoint()` — Create a checkpoint save (writes to the next rotating auto-save slot).

### Meta-Progression

Permanent key-value storage that survives across runs and save slot deletion.

- `Meta_SetFloat(const string& key, float value)`, `Meta_GetFloat(const string& key, float fallback)` — float values
- `Meta_SetInt(const string& key, int value)`, `Meta_GetInt(const string& key, int fallback)` — integer values
- `Meta_SetBool(const string& key, bool value)`, `Meta_GetBool(const string& key, bool fallback)` — boolean values
- `Meta_SetString(const string& key, const string& value)`, `Meta_GetString(const string& key, const string& fallback)` — string values
- `Meta_Save()` — Force-write meta-progression to disk immediately.

### Auto-Save Configuration

- `AutoSave_Enable(bool enabled)` — Enable or disable timed auto-save.
- `AutoSave_SetInterval(float seconds)` — Set auto-save interval in seconds (default: 300).

## Weather System

- `Weather_Set(int type, float transitionTime = 2.0)` — Change weather. Types: 0=Clear, 1=Cloudy, 2=Rain, 3=HeavyRain, 4=Snow, 5=Fog, 6=Storm.
- `Weather_Get()` — Returns current `WeatherType` as int.
- `Weather_SetRainIntensity(float)`, `Weather_GetRainIntensity()` — Rain intensity (0-1).
- `Weather_SetSnowIntensity(float)`, `Weather_GetSnowIntensity()` — Snow intensity (0-1).
- `Weather_SetFogDensity(float)`, `Weather_GetFogDensity()` — Fog density (0-1).
- `Weather_SetFogColor(float r, float g, float b)` — Fog color RGB.
- `Weather_SetFogRange(float start, float end)` — Fog start/end distances.
- `Weather_SetWind(float dirX, float dirY, float dirZ, float strength)` — Wind direction and strength.
- `Weather_IsLightning()` — True if lightning is currently active.
- `Weather_LightningJustFired()` — True for one frame when a lightning bolt triggers (use for SFX).
- `Weather_SetLightningInterval(float minSec, float maxSec)` — Set random lightning interval range.

## Particle System

- `Particle_Play(uint64)`, `Particle_Stop(uint64)` — Start/stop a particle emitter entity.
- `Particle_IsPlaying(uint64)` — Check if an emitter is playing.
- `Particle_SetEmissionRate(uint64, float)`, `Particle_GetEmissionRate(uint64)` — Particles per second.
- `Particle_Burst(uint64, int count)` — Emit a burst of particles instantly.
- `Particle_SetLifetime(uint64, float)` — Particle lifetime in seconds.
- `Particle_SetSpeed(uint64, float)` — Particle start speed.
- `Particle_SetSize(uint64, float startSize, float endSize)` — Particle size over lifetime.
- `Particle_SetColor(uint64, float sr, sg, sb, float er, eg, eb)` — Start/end color RGB.
- `Particle_SetAlpha(uint64, float startAlpha, float endAlpha)` — Fade over lifetime.
- `Particle_SetLoop(uint64, bool)` — Enable/disable looping.
- `Particle_SetGravity(uint64, float gx, float gy, float gz)` — Particle gravity vector.
- `Particle_ApplyPreset(uint64, const string &in)` — Apply a named preset ("Fire", "Smoke", "Sparks", "Snow", "Rain", "Magic", "Explosion", "Water Splash", "Blood/Sap", "Lava", "Fountain", "Drip").

## Interactive Water

Drive the height-field water on an entity that has an `InteractiveWaterComponent` (the entity's `TransformComponent` positions the surface). Coordinates are world-space X/Z.

- `Water_Splash(uint64, float x, float z, float strength)` — One-shot impulse at a point (a cannonball hit). Bigger strength = bigger wave.
- `Water_Wake(uint64, float x, float z, float velX, float velZ, float wakeWidth)` — Continuous V-wake behind a moving object.
- `Water_SustainedPressure(uint64, float x, float z, float radius, float force)` — Hold a standing depression while called each frame (a leafblower); stop calling and it relaxes back.
- `Water_GetHeight(uint64, float x, float z)` — Sample the current surface height (world Y) at a point, for gameplay/NPC logic.

## Gameplay & Visual Components

Accessors for components that previously had no script access (closing the script-vs-C++ parity gap, see docs/SCRIPTING_PARITY.md).

- **LookAtTarget** — make an entity rotate to face a target: `LookAt_SetTarget(uint64, uint64 target)`, `LookAt_SetTargetPosition(uint64, float x, y, z)`, `LookAt_ClearTarget(uint64)`, `LookAt_SetSpeed(uint64, float degPerSec)`, `LookAt_GetSpeed(uint64)`, `LookAt_SetInstant(uint64, bool)`, `LookAt_SetConstraints(uint64, bool x, bool y, bool z)`.
- **DamageResistance** — per-type damage multipliers (type = "physical"/"fire"/"ice"/"electric"/"poison"/"magic"): `DamageResist_Set(uint64, const string &in type, float mult)`, `DamageResist_Get(uint64, const string &in type)`.
- **Ragdoll** — physics-driven bodies: `Ragdoll_SetActive(uint64, bool)`, `Ragdoll_IsActive(uint64)`, `Ragdoll_SetBlendWeight(uint64, float)`, `Ragdoll_SetGravityScale(uint64, float)`.
- **Pushable** — block-pushing objects: `Pushable_SetAxes(uint64, bool x, bool y, bool z)`, `Pushable_SetPushSpeed(uint64, float)`, `Pushable_IsBeingPushed(uint64)`.
- **TemperatureZone** — hot/cold regions: `TempZone_SetTemperature(uint64, float)`, `TempZone_GetTemperature(uint64)`, `TempZone_SetPriority(uint64, int)`.
- **ReflectionProbe** — environment reflections: `ReflectionProbe_SetIntensity(uint64, float)`, `ReflectionProbe_GetIntensity(uint64)`, `ReflectionProbe_SetActive(uint64, bool)`, `ReflectionProbe_IsActive(uint64)`.
- **Billboard** — camera-facing quads: `Billboard_SetFaceCamera(uint64, bool)`, `Billboard_SetLockY(uint64, bool)`, `Billboard_SetRotationOffset(uint64, float degrees)`.
- **Possessable** — entities the player can take control of: `Possessable_IsPossessed(uint64)`, `Possessable_SetPrompt(uint64, const string &in)`, `Possessable_SetRange(uint64, float)`, `Possessable_SetPlayerIndex(uint64, int)`.
- **SavePoint** — `SavePoint_SetSlot(uint64, int)`, `SavePoint_SetSaveOnEnter(uint64, bool)`, `SavePoint_IsUsed(uint64)`, `SavePoint_SetRadius(uint64, float)`, `SavePoint_SetMessage(uint64, const string &in)`.
- **Footstep** — `Footstep_SetVolume(uint64, float)`, `Footstep_SetWalkInterval(uint64, float)`, `Footstep_SetRunInterval(uint64, float)`, `Footstep_SetPitchVariance(uint64, float)`.
- **ReverbZone** — audio ambiance: `Reverb_SetActive(uint64, bool)`, `Reverb_SetRoomSize(uint64, float)`, `Reverb_SetDamping(uint64, float)`, `Reverb_SetWetDryMix(uint64, float)`, `Reverb_SetDecayTime(uint64, float)`.
- **Lens** — per-camera lens: `Lens_SetEnabled(uint64, bool)`, `Lens_SetDistortion(uint64, float)`, `Lens_SetChromaticAberration(uint64, float)`, `Lens_SetVignette(uint64, float intensity, float softness)`, `Lens_SetAnamorphicSqueeze(uint64, float)`.
- **Physics joints** (runtime tuning): `SpringJoint_SetRestLength/SetStiffness/SetDamping(uint64, float)`, `SpringJoint_GetStress(uint64)`; `SliderJoint_SetMotor(uint64, bool, float speed, float maxForce)`, `SliderJoint_SetLimits(uint64, bool, float lower, float upper)`, `SliderJoint_GetDisplacement(uint64)`; `FixedJoint_SetBreakable(uint64, bool, float force)`; `BallSocket_SetConeLimit(uint64, bool, float angle)`, `BallSocket_SetTwistLimit(uint64, bool, float lower, float upper)`.

## HUD Widget

- `HUD_SetVisible(uint64, bool)` — Show/hide a HUD widget.
- `HUD_IsVisible(uint64)` — Check widget visibility.
- `HUD_SetText(uint64, const string &in)` — Set display text.
- `HUD_GetText(uint64)` — Get current display text.
- `HUD_SetValue(uint64, float current, float max)` — Set bar current/max values.
- `HUD_GetValue(uint64)`, `HUD_GetMaxValue(uint64)` — Read bar values.
- `HUD_SetFillColor(uint64, float r, float g, float b)` — Bar fill color.
- `HUD_SetTextColor(uint64, float r, float g, float b)` — Label text color.
- `HUD_SetPosition(uint64, float anchorX, float anchorY)` — Screen position (0-1 normalized).
- `HUD_SetSize(uint64, float width, float height)` — Widget dimensions (normalized).
- `HUD_SetFontSize(uint64, float)` — Font size in pixels.
- `HUD_SetBindField(uint64, const string &in)` — Data binding field ("health", "stamina", "custom").

## Text Component

- `Text_SetContent(uint64, const string &in)` — Set text content (triggers re-rasterization).
- `Text_GetContent(uint64)` — Get current text content.
- `Text_SetFontSize(uint64, float)` — Font size in pixels.
- `Text_SetColor(uint64, float r, float g, float b)` — Text color RGB.
- `Text_SetBgColor(uint64, float r, float g, float b)` — Background color RGB.
- `Text_SetBgOpacity(uint64, float)` — Background opacity (0-1).
- `Text_SetAlignment(uint64, int)` — Horizontal alignment (0=Left, 1=Center, 2=Right).
- `Text_SetWrapWidth(uint64, float)` — Word wrap width in pixels.

## Quest System

- `Quest_Start(const string& questId)` — Start a quest by ID.
- `Quest_CompleteObjective(const string& questId, int objectiveIndex)` — Complete a specific quest objective.
- `Quest_Fail(const string& questId)` — Fail a quest.
- `Quest_IsActive(const string& questId)` — Check if a quest is currently active.
- `Quest_IsComplete(const string& questId)` — Check if a quest is fully complete.

## Cinematic System

- `Cinematic_Play(uint64 entity)` — Start a cinematic camera on an entity with `CinematicCameraComponent`.
- `Cinematic_Stop(uint64 entity)` — Stop a playing cinematic.
- `Cinematic_IsPlaying()` — Check if any cinematic is currently playing.

## Object Pool

- `Pool_Acquire(const string& poolId)` — Get an entity from a pool (returns entity ID or 0).
- `Pool_Release(const string& poolId, uint64 entity)` — Return an entity to a pool.

## Destructible System

- `Destructible_Destroy(uint64, float dirX, dirY, dirZ, float force)` — Trigger destruction of an entity.
- `Destructible_ApplyDamage(uint64, float damage)` — Apply damage (destroys if health depleted).
- `Destructible_ApplyDamageAt(uint64, float damage, float px, py, pz)` — Apply damage at a specific point.

## UI Canvas

- `UI_SetCanvasVisible(uint64, bool)`, `UI_IsCanvasVisible(uint64)` — Show/hide a canvas.
- `UI_SetCanvasSortOrder(uint64, int)` — Set canvas render order.
- `UI_SetText(uint64, int elementId, const string&)`, `UI_GetText(uint64, int)` — Set/get text on a label or button.
- `UI_SetElementVisible(uint64, int, bool)`, `UI_IsElementVisible(uint64, int)` — Show/hide an element.
- `UI_SetElementEnabled(uint64, int, bool)` — Enable/disable an element.
- `UI_SetProgress(uint64, int, float)`, `UI_GetProgress(uint64, int)` — Progress bar value (0-1).
- `UI_SetSliderValue(uint64, int, float)`, `UI_GetSliderValue(uint64, int)` — Slider value.
- `UI_SetChecked(uint64, int, bool)`, `UI_IsChecked(uint64, int)` — Checkbox/toggle state.
- `UI_SetImagePath(uint64, int, const string&)` — Change an image element's texture.
- `UI_SetImageAlpha(uint64, int, float)` — Image transparency (0-1).
- `UI_SetBgColor(uint64, int, float r, g, b, float a)` — Element background color.
- `UI_SetTextColor(uint64, int, float r, g, b)` — Text color (uniform, all characters).
- `UI_SetCharColor(uint64, int elementId, int charIndex, float r, g, b)` — Set color for a single character by index. Enables per-character coloring on Label/Button elements. Characters beyond the charColors array use the element's textColor.
- `UI_SetCharColorRange(uint64, int elementId, int startIdx, int endIdx, float r, g, b)` — Set color for a range of characters (inclusive).
- `UI_ClearCharColors(uint64, int elementId)` — Remove all per-character colors, revert to uniform textColor.
- `UI_IsHovered(uint64, int)`, `UI_IsPressed(uint64, int)` — Interaction state queries.
- `UI_SetFocus(uint64, int elementId)` — Set keyboard/gamepad focus to a specific element.
- `UI_ClearFocus(uint64)` — Remove focus from all elements on a canvas.
- `UI_GetFocusedElement(uint64)` — Get the currently focused element ID (0 = none).
- `UI_IsFocused(uint64, int)` — Check if a specific element has focus.
- `UI_SetTabOrder(uint64, int elementId, int order)` — Set explicit tab order (0 = auto from element order).
- `UI_SetFocusable(uint64, int elementId, bool)` — Set whether an element can receive focus.

## Localization

- `Loc_Get(const string& key)` — Look up a localized string by key for the current locale.
- `Loc_GetWithFallback(const string& key, const string& fallback)` — Look up with a fallback if key is missing.
- `Loc_SetLocale(const string& localeCode)` — Switch the active locale (e.g. "en", "fr", "ja").
- `Loc_GetLocale()` — Get the current locale code.
- `Loc_HasString(const string& key)` — Check if a key exists in the current locale.

## Prefab System

- `Prefab_Instantiate(const string& path, float x, y, z)` — Load and instantiate a `.enjprefab` at a position. Returns the root entity ID.
- `Prefab_InstantiateEx(const string& path, float px, py, pz, float rx, ry, rz, float sx, sy, sz)` — Instantiate with position, rotation, and scale.
- `Prefab_IsPrefabInstance(uint64)` — Check if an entity is a prefab instance root.
- `Prefab_Unpack(uint64)` — Disconnect a prefab instance from its source prefab.

## Level Streaming

- `Streaming_ForceLoad(const string& chunkId)` — Synchronously load a streaming chunk by ID.
- `Streaming_ForceUnload(const string& chunkId)` — Unload a streaming chunk.
- `Streaming_GetState(const string& chunkId)` — Get chunk state as int (0=Unloaded, 1=Loading, 2=Loaded, 3=Unloading).
- `Streaming_IsLoaded(const string& chunkId)` — Check if a chunk is currently loaded.
- `Streaming_GetLoadedCount()` — Get the number of currently loaded chunks.
- `Streaming_SetEnabled(bool)` — Enable or disable the streaming system.

## Physics 2D

- `Physics2D_Raycast(float originX, originY, float dirX, dirY, float maxDist)` — Cast a 2D ray, returns true on hit.
- `Physics2D_RaycastMask(float originX, originY, float dirX, dirY, float maxDist, uint mask)` — 2D raycast with collision mask filter.
- `Physics2D_RaycastHit(float originX, originY, float dirX, dirY, float maxDist, Vector2 &out hitPoint, Vector2 &out hitNormal, float &out hitDist, uint64 &out hitEntity)` — 2D raycast returning full hit info.
- `Physics2D_RaycastHitMask(float originX, originY, float dirX, dirY, float maxDist, uint mask, Vector2 &out hitPoint, Vector2 &out hitNormal, float &out hitDist, uint64 &out hitEntity)` — Full 2D raycast with mask.
- `Physics2D_OverlapCircle(float cx, cy, float radius)` — Check for any body overlapping a circle. Returns entity ID (0 = none).
- `Physics2D_OverlapCircleMask(float cx, cy, float radius, uint mask)` — Circle overlap with collision mask.
- `Physics2D_OverlapBox(float cx, cy, float hw, hh)` — Check for any body overlapping an AABB. Returns entity ID (0 = none).
- `Physics2D_OverlapBoxMask(float cx, cy, float hw, hh, uint mask)` — Box overlap with collision mask.
- `Physics2D_AddForce(uint64, float fx, fy)` — Apply a continuous force to a 2D rigidbody.
- `Physics2D_AddImpulse(uint64, float ix, iy)` — Apply an instant impulse to a 2D rigidbody.
- `Physics2D_SetVelocity(uint64, float vx, vy)` — Directly set 2D velocity.
- `Physics2D_GetVelocity(uint64)` — Get current 2D velocity as Vector2.
- `Physics2D_SetGravity(float gx, gy)` — Set global 2D gravity.
- `Physics2D_GetGravity()` — Get current 2D gravity as Vector2.
- `Physics2D_SetGravityScale(uint64, float scale)` — Per-body gravity multiplier.
- **Overlap (entity list)**: `Physics2D_OverlapCircleEntities(Vector2, float)`, `Physics2D_OverlapBoxEntities(Vector2, Vector2)` — return count of overlapping entities. Masked variants: append `Mask` suffix + `uint layerMask`. Retrieve results: `Physics2D_GetOverlapResult(int index)` — returns entity ID.

## Screen-Space Effects

Raster-tier screen-space effects running in the post-process fragment shader. All use existing depth buffer + inverse view-projection matrix. No additional render passes required.

### God Rays

- `PostProcess_SetGodRaysEnabled(bool)` — Enable/disable screen-space god rays.
- `PostProcess_IsGodRaysEnabled()` — Check if god rays are enabled. Returns `bool`.
- `PostProcess_SetGodRaysIntensity(float)` — Set god rays brightness multiplier.
- `PostProcess_GetGodRaysIntensity()` — Get current god rays intensity.
- `PostProcess_SetGodRaysSamples(int)` — Set number of radial blur samples (default 64).
- `PostProcess_GetGodRaysSamples()` — Get current sample count.

### SSAO (Screen-Space Ambient Occlusion)

- `PostProcess_SetSSAOEnabled(bool)` — Enable/disable screen-space ambient occlusion.
- `PostProcess_IsSSAOEnabled()` — Check if SSAO is enabled. Returns `bool`.
- `PostProcess_SetSSAORadius(float)` — Set hemisphere sampling radius (world units).
- `PostProcess_GetSSAORadius()` — Get current SSAO radius.
- `PostProcess_SetSSAOIntensity(float)` — Set occlusion intensity multiplier.
- `PostProcess_GetSSAOIntensity()` — Get current SSAO intensity.

### Contact Shadows

- `PostProcess_SetContactShadowsEnabled(bool)` — Enable/disable contact shadows.
- `PostProcess_IsContactShadowsEnabled()` — Check if contact shadows are enabled. Returns `bool`.
- `PostProcess_SetContactShadowsIntensity(float)` — Set contact shadow darkness.
- `PostProcess_GetContactShadowsIntensity()` — Get current contact shadow intensity.

### Fake Caustics

- `PostProcess_SetCausticsEnabled(bool)` — Enable/disable fake caustics.
- `PostProcess_IsCausticsEnabled()` — Check if caustics are enabled. Returns `bool`.
- `PostProcess_SetCausticsIntensity(float)` — Set caustics brightness.
- `PostProcess_GetCausticsIntensity()` — Get current caustics intensity.
- `PostProcess_SetCausticsWaterY(float)` — Set water surface Y position (caustics render below this).
- `PostProcess_GetCausticsWaterY()` — Get current water height threshold.

### Fog Shafts

- `PostProcess_SetFogShaftsEnabled(bool)` — Enable/disable volumetric fog shafts.
- `PostProcess_IsFogShaftsEnabled()` — Check if fog shafts are enabled. Returns `bool`.
- `PostProcess_SetFogShaftsIntensity(float)` — Set fog shaft brightness.
- `PostProcess_GetFogShaftsIntensity()` — Get current fog shaft intensity.
- `PostProcess_SetFogShaftsMaxDistance(float)` — Set max ray march distance (world units).
- `PostProcess_GetFogShaftsMaxDistance()` — Get current max distance.

## Input Actions

**Enum `GameAction`:** `MoveForward = 0`, `MoveBack = 1`, `MoveLeft = 2`, `MoveRight = 3`, `Jump = 4`, `Sprint = 5`, `Crouch = 6`, `Dash = 7`, `Interact = 8`, `Attack = 9`, `Block = 10`, `Pause = 11`, `LookUp = 12`, `LookDown = 13`, `LookLeft = 14`, `LookRight = 15`, `CameraZoomIn = 16`, `CameraZoomOut = 17`

### Query

- `InputAction_IsDown(int action)` — Check if action is held down. Returns `bool`.
- `InputAction_IsPressed(int action)` — Check if action was just pressed this frame. Returns `bool`.
- `InputAction_IsReleased(int action)` — Check if action was just released this frame. Returns `bool`.
- `InputAction_GetValue(int action)` — Get analog value for action (0.0-1.0). Returns `float`.
- `InputAction_GetMovement()` — Get combined WASD/stick movement vector. Returns `Vec2`.

### Sensitivity

- `InputAction_SetSensitivity(int action, float sensitivity)` — Set sensitivity multiplier for an action.
- `InputAction_GetMouseSensitivity()` — Get global mouse sensitivity. Returns `float`.
- `InputAction_SetMouseSensitivity(float sens)` — Set global mouse sensitivity.

### Toggle Settings

- `InputAction_IsSprintToggle()` — Check if sprint uses toggle mode. Returns `bool`.
- `InputAction_SetSprintToggle(bool toggle)` — Set sprint to toggle or hold mode.
- `InputAction_IsCrouchToggle()` — Check if crouch uses toggle mode. Returns `bool`.
- `InputAction_SetCrouchToggle(bool toggle)` — Set crouch to toggle or hold mode.

### Rebinding

- `InputAction_Rebind(int actionIndex, int keyCode)` — Rebind an action to a new key.
- `InputAction_PollNextKey()` — Poll for the next key press (for rebind UI). Returns `int` (-1 if none).

### Display Helpers

- `InputAction_GetCount()` — Get total number of actions. Returns `int`.
- `InputAction_GetName(int index)` — Get display name of an action. Returns `string`.
- `InputAction_GetBindingName(int index)` — Get display name of current key binding. Returns `string`.

### Presets

- `InputAction_ApplyLeftHandOnly()` — Apply left-hand-only key layout.
- `InputAction_ApplyRightHandOnly()` — Apply right-hand-only key layout.
- `InputAction_ApplyGamepadOnly()` — Apply gamepad-only layout.
- `InputAction_ResetDefaults()` — Reset all bindings to defaults.

## Networking

**Enum `NetworkRole`:** `None = 0`, `Host = 1`, `Client = 2`

- `Net_HostGame(int port)` — Start hosting a game on the given port.
- `Net_JoinGame(const string& address, int port)` — Connect to a host.
- `Net_Disconnect()` — Disconnect from the current session.
- `Net_IsConnected()` — Check if connected to a session.
- `Net_IsHost()` — Check if this peer is the host.
- `Net_GetRole()` — Get current NetworkRole enum value.
- `Net_GetLocalPlayerId()` — Get this peer's player ID.
- `Net_GetPlayerCount()` — Get total connected player count.
- `Net_GetPing()` — Get round-trip latency in milliseconds.
- `Net_GetPacketLoss()` — Get packet loss percentage (0-1).
- `Net_SetReady(bool)` — Set lobby ready state.
- `Net_GetLobbyPlayerCount()` — Get number of players in the lobby.
- `Net_GetLobbyPlayerName(int index)` — Get lobby player name by index.
- `Net_GetLobbyPlayerReady(int index)` — Check if lobby player is ready.
- `Net_RegisterEntity(uint64)` — Register an entity for network replication.
- `Net_UnregisterEntity(uint64)` — Stop replicating an entity.
- `Net_RequestOwnership(uint64)` — Request ownership of a networked entity.
- `Net_CallRPC(const string& name, const string& data, int targetId)` — Send an RPC to a specific player.
- `Net_CallRPCAll(const string& name, const string& data)` — Broadcast an RPC to all players.
- `Net_RegisterRPCHandler(const string& name)` — Register an RPC handler. When received, fires `"__rpc_" + name` event via ScriptEventBus with data as payload.

### Record & Rewind

Per-entity (Braid-style) and scene-wide (Sands of Time-style) time rewind.

- `Rewind_StartEntity(uint64)` — Start rewinding a specific entity (requires RecordRewindComponent).
- `Rewind_StopEntity(uint64)` — Stop rewinding and enter cooldown.
- `Rewind_IsEntityRewinding(uint64)` — Check if an entity is currently rewinding.
- `Rewind_SetEntityChannels(uint64, uint)` — Set which data channels to record (bitmask: 1=Transform, 2=Velocity, 4=Health, 8=Animation, 16=Physics, 32=Material).
- `Rewind_StartScene()` — Start scene-wide rewind (requires SceneRewindComponent on a manager entity).
- `Rewind_StopScene()` — Stop scene-wide rewind.
- `Rewind_IsSceneRewinding()` — Check if scene rewind is active.
- `Rewind_SeekScene(float)` — Seek to a specific time offset from the latest recorded frame.
- `Rewind_GetRecordedDuration()` — Get total seconds of recorded scene history.
- `Rewind_GetCurrentTime()` — Get current playback position during rewind.
- `Rewind_IsAnyRewinding()` — Check if any entity or scene rewind is active.

## AI & Pathfinding

- **AI Controller**: `AI_SetState/GetState(uint64, int)` — 0=Idle, 1=Patrol, 2=Chase, 3=Attack, 4=Flee, 5=Dead
- `AI_SetTarget/GetTarget(uint64, uint64)`, `AI_SetTargetPosition(uint64, Vector3)`
- `AI_SetDetectionRange/GetDetectionRange(uint64, float)`, `AI_SetAttackRange(uint64, float)`
- `AI_SetMoveSpeed/GetMoveSpeed(uint64, float)`, `AI_SetChaseSpeed/FleeSpeed(uint64, float)`
- `AI_SetFieldOfView(uint64, float)`, `AI_SetUseNavmesh(uint64, bool)`
- **Navmesh**: `Navmesh_HasNavmesh()`, `Navmesh_IsPointOnNavmesh(float x, y, z)`
- **Pathfinding**: `Navmesh_FindPath(float sx, sy, sz, float ex, ey, ez)` — returns waypoint count (0 = no path), `Navmesh_PathExists(float sx, sy, sz, float ex, ey, ez)` — fast check, `Navmesh_GetPathWaypoint(int index)` — returns Vector3, `Navmesh_GetPathCost()` — total path cost
- **Behavior Tree**: `BT_Enable/Disable(uint64)`, `BT_SetBlackboardFloat/Int/Bool/String(uint64, string, value)`, `BT_GetBlackboardFloat/Int/Bool/String(uint64, string)`, `BT_ClearBlackboard(uint64)`

### Accessibility

**Colorblind Mode** (enum `ColorblindMode`: `CB_OFF`=0, `CB_PROTANOPIA`=1, `CB_DEUTERANOPIA`=2, `CB_TRITANOPIA`=3, `CB_PROTANOMALY`=4, `CB_DEUTERANOMALY`=5, `CB_TRITANOMALY`=6, `CB_ACHROMATOPSIA`=7)

- `Colorblind_SetMode(int)` — Set colorblind correction mode (0-7). Applied to post-processing shader.
- `Colorblind_GetMode()` — Get current colorblind mode.
- `Colorblind_SetStrength(float)` — Set correction strength (0.0-1.0).
- `Colorblind_GetStrength()` — Get correction strength.

**Visual Settings**

- `Accessibility_SetBrightness(float)` — Set additive brightness (-0.5 to 0.5).
- `Accessibility_GetBrightness()` — Get current brightness offset.
- `Accessibility_SetContrast(float)` — Set multiplicative contrast (0.5 to 2.0).
- `Accessibility_GetContrast()` — Get current contrast.
- `Accessibility_SetDyslexiaFont(bool)` — Enable/disable dyslexia-friendly text (font/spacing).
- `Accessibility_GetDyslexiaFont()` — Check if dyslexia-friendly text is on.

**Motion / Photosensitivity**

- `Accessibility_SetReducedMotion(bool)` — Enable/disable reduced motion.
- `Accessibility_GetReducedMotion()` — Check if reduced motion is on.
- `Accessibility_SetScreenShake(bool)` — Enable/disable screen shake.
- `Accessibility_GetScreenShake()` — Check if screen shake is enabled.
- `Accessibility_SetFlashingLights(bool)` — Enable/disable flashing lights (film grain, CRT, VHS).
- `Accessibility_GetFlashingLights()` — Check if flashing effects are enabled.

**Font / Cognitive**

- `Accessibility_SetFontScale(float)` — Set UI font scale (0.5-3.0).
- `Accessibility_GetFontScale()` — Get current font scale.

**Subtitles / Captions**

- `Subtitle_Show(string text, string speaker = "", float duration = 3.0)` — Show a subtitle.
- `Subtitle_ShowWithColor(string text, string speaker, float r, float g, float b, float duration = 3.0)` — Show subtitle with speaker color.
- `Subtitle_ShowCaption(string text, float duration = 2.5)` — Show a closed caption.
- `Subtitle_Clear()` — Clear all subtitles.
- `Subtitle_SetEnabled(bool)` — Enable/disable subtitles.
- `Subtitle_IsEnabled()` — Check if subtitles are enabled.
- `Subtitle_SetFontSize(float)` — Set subtitle font size (16-48).
- `Subtitle_GetFontSize()` — Get subtitle font size.

**Screen Reader / Announcer**

- `Announcer_Announce(string text)` — Queue an accessibility announcement. In browser exports, announcements are also spoken aloud via the Web Speech API.
- `Announcer_AnnounceHighPriority(string text)` — Queue a high-priority announcement.
- `Announcer_Clear()` — Clear all announcements.
- `Announcer_SetEnabled(bool)` — Enable/disable the announcer.
- `Announcer_IsEnabled()` — Check if announcer is enabled.

**Settings Persistence**

- `Accessibility_SaveSettings()` — Save all accessibility settings to `accessibility.json`.
