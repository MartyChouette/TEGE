# AngelScript API Reference

Complete reference for all functions callable from AngelScript via `TegeBehavior` scripts. ~250 functions across all categories.

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
- **EntityHandle** class: `IsValid()`, `GetID()`, `GetPosition/SetPosition()`, `GetRotation/SetRotation()`, `GetScale/SetScale()`, `GetName()`, `HasTag(string)`
- **TransformProxy**: `position`, `rotation`, `scale`, `forward`, `right`, `up` (read-only)

## Scene Management

- `Scene_FindEntity(string name)`, `Scene_FindEntityByTag(string tag)`
- `Scene_DestroyEntity(uint64)`, `Scene_Instantiate()`, `Scene_InstantiateNamed(string)`, `Scene_InstantiateAt(Vector3)`
- `Scene_IsValid(uint64)`, `Scene_GetEntityCount()`
- `Scene_GetEntityName/SetEntityName(uint64, string)`
- `Scene_AddTag/RemoveTag/HasTag(uint64, string)`
- `Scene_LoadScene(string)`, `Scene_GetCurrentScene()`

## Time

`Time_GetDeltaTime()`, `Time_GetFixedDeltaTime()`, `Time_GetTime()`, `Time_GetTimeScale()`, `Time_SetTimeScale(float)`, `Time_GetFrameCount()`

## Debug

`Debug_Log(string)`, `Debug_LogWarning(string)`, `Debug_LogError(string)`

## Input — Keyboard

`Input_GetKey(int)`, `Input_GetKeyDown(int)`, `Input_GetKeyUp(int)` — Key enum: A-Z, Num0-9, F1-F12, Space, Escape, Enter, Tab, Backspace, arrows, Shift, Control, Alt, etc.

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

## Audio

`Audio_Play(uint64)`, `Audio_PlayAtPosition(string, Vector3)`, `Audio_Stop(uint64)`, `Audio_StopAll()`
`Audio_SetVolume/SetPitch(uint64, float)`, `Audio_IsPlaying(uint64)`
`Audio_SetMasterVolume/GetMasterVolume(float)`

## Component Access

- **Health**: `Health_Get/GetMax/SetCurrent(uint64)`, `Health_Damage(uint64, float)`
- **Material**: `Material_SetBaseColor/GetBaseColor(uint64, Vector3)`, `Material_SetMetallic/SetRoughness(uint64, float)`
- **Light**: `Light_SetColor/SetIntensity(uint64, ...)`
- **Camera**: `Camera_SetFOV/GetFOV(uint64, float)`
- **AudioSource**: `AudioSource_Play/Stop/SetClip/SetVolume(uint64, ...)`
- **Animator**: `Animator_Play(uint64, string)`, `Animator_SetSpeed(uint64, float)`
- **Controller**: `Controller_SetMoveSpeed/GetVelocity(uint64, ...)` — works with all 5 controller types
- **Camera2D**: `Camera2D_Shake(uint64, float intensity, float duration)`, `Camera2D_GetZoom/SetZoom(uint64, float)`, `Camera2D_AddTarget/RemoveTarget(uint64 camera, uint64 target)`, `Camera2D_ClearTargets(uint64)`, `Camera2D_SetDeadZone(uint64, float w, float h)`, `Camera2D_SetLookAhead(uint64, float distance, float smoothing)`, `Camera2D_SetFollowTarget/GetFollowTarget(uint64, uint64)`
- **Existence checks**: `HasComponent_Health/Light/Camera/Material/AudioSource/Rigidbody/BoxCollider/Animator(uint64)`
- **State Machine**: `SM_AddState(uint64, string)`, `SM_AddTransition(uint64, from, to)`, `SM_SetState/GetCurrentState/GetPreviousState(uint64)`, `SM_GetStateTime(uint64)`, `SM_SendTrigger(uint64, string)`, `SM_SetBool/GetBool(uint64, string, bool)`, `SM_SetFloat/GetFloat(uint64, string, float)`, `SM_SetInt/GetInt(uint64, string, int)`, `SM_HasState(uint64, string)`, `SM_SetOnEnter/SetOnUpdate/SetOnExit(uint64, stateName, funcName)`, `SM_GetOnEnter/GetOnUpdate/GetOnExit(uint64, stateName)`

## Coroutines

`StartCoroutine(string)`, `YieldSeconds(float)`, `YieldFrames(uint)`, `YieldEndOfFrame()`

## Event System

- **EventData** class: `SetFloat/GetFloat`, `SetInt/GetInt`, `SetString/GetString`, `SetEntity/GetEntity`
- `Events_Listen(string, EventCallback@)` — returns listener ID
- `Events_Send(string, EventData@)`, `Events_Broadcast(EventData@)`

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
- `UI_SetTextColor(uint64, int, float r, g, b)` — Text color.
- `UI_IsHovered(uint64, int)`, `UI_IsPressed(uint64, int)` — Interaction state queries.

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
