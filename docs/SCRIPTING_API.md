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

- **OnMouseEnter() / OnMouseExit() / OnClick()**: TegeBehavior lifecycle hooks for cursor interaction. The engine raycasts under the mouse once per frame (same ray as `Physics_RaycastScreen`, only when some script defines one of these) — Enter/Exit fire on the hover edge, OnClick on left-press while hovered. The entity needs a collider to be hit; nothing fires while the mouse is captured (FPS look) or on the web player (no screen-pick there yet). `Physics_RaycastScreen` remains the manual alternative.
- **A script does not tick on the frame it starts**: `OnUpdate`, `OnFixedUpdate` and `OnLateUpdate` begin on the frame AFTER `OnStart`. The frame's `dt` covers the whole frame including the time before the script existed, and the fixed-step accumulator can hold several ticks of that same pre-existence time (up to six, since a frame's delta is clamped to 0.1s), so a script integrating `position += velocity * dt` would jump on its first frame alive. Anything that has to happen the instant an entity spawns belongs in `OnStart` itself.
- **Execution budget**: a single call into a script is capped at one million executed statements, after which it is aborted and that script is disabled (the log names the method). The cap is per CALL, so it bounds one runaway method and not a frame: a hundred scripts each running just under it is a hundred million statements in one frame. The frame total is reported instead — the editor profiler shows "Script statements", and one warning is logged when a frame crosses the reporting budget.
- **OnFixedUpdate(float fixedDt)**: TegeBehavior lifecycle hook that runs once per physics tick. On fixed-timestep projects (Settings > Project > Fixed Physics Timestep) it lands exactly in step with the physics simulation - use it for forces and tick-locked gameplay. On classic projects it runs from a 60Hz accumulator. OnUpdate(dt) stays frame-paced either way.
- **Bullet time**: `Controller_SetIgnoreTimeScale(uint64 entity, bool)` / `bool Controller_GetIgnoreTimeScale(uint64)` — the flagged entity's character controller runs at wall-clock rate while `Time_SetScale` slows the world (movement, jumps, gravity all stay normal; no compensation needed). Pair with `Animator_SetSpeed(player, 1/scale)` if the player is animated. Also an inspector checkbox on every controller.
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

## Input — Custom actions

> You do not need scripts for this. Name your actions in **Project Settings > Input & Touch**, then wire them up in a scene with an **Action Trigger** component (pick an action, pick what it does). The calls below are the optional scripted path.

`GameAction::Custom0..Custom7` are game-defined slots. Name one at boot and bind it; from then on the controls menu, the bottom-left controls hint and touch treat it like a built-in action. Unnamed slots stay hidden.

- `InputAction_SetName(int action, const string &in name)` — e.g. `InputAction_SetName(GameAction::Custom0, "SLO-MO")`
- `InputAction_Rebind(int action, int keyCode)`, `InputAction_AddGamepadBinding(int action, int button)`, `InputAction_AddMouseBinding(int action, int button)`, `InputAction_ClearBindings(int action)`
- Read it like any action: `InputAction_IsPressed(GameAction::Custom0)`
- Menu/dialogue actions also exist: `UIConfirm`, `UICancel`, `UINavUp/Down/Left/Right`, `DialogueAdvance`

## Input — Mobile Touch Overlay

On-screen touch controls: a floating move stick, an optional look-drag region, and up to 6 anchored buttons. The layout is derived from the scene's controller type (one button per action that controller consumes, labelled with the LIVE binding), and the same layout is drawn on web, on desktop with `EnjinPlayer --touch` (mouse acts as one touch), and in the editor via View > Simulate Touch Controls. The bottom-left controls hint is built from the same list and hides while the touch overlay is on. These calls let a game author its own layout.

- `Touch_UsePreset(int)` — 0 Platformer2D, 1 TopDown2D, 2 TopDown3D, 3 FirstPerson, 4 ThirdPerson, 5 Generic
- `Touch_ClearButtons()` — remove all buttons (stick/look unchanged)
- `Touch_AddActionButton(const string &in label, int action, float col, float row, float radiusFrac)` — preferred: the button presses the action's CURRENT binding and shows its label (a named Custom action shows its name). col/row place it on a grid growing up-left from the bottom-right of the safe area; radiusFrac is relative to safe-area height (~0.065-0.09, out-of-range falls back to 0.075)
- `Touch_AddButton(const string &in label, int keyCode, float col, float row, float radiusFrac)` — raw key variant; keyCode is the key held while pressed, negative = mouse button (-1 = left click / fire). Not rebindable, so prefer the action variant
- `Touch_SetStickActions(bool enabled, int leftAction, int rightAction, int fwdAction, int backAction)` — bind the stick to actions (default MoveLeft/Right/Forward/Back)
- `Touch_SetStick(bool enabled, int leftKey, int rightKey, int upKey, int downKey)` — raw-key variant, or hide the stick
- `Touch_SetLookRegion(bool)` — enable/disable the right-side camera drag region

The auto-preset is applied before scripts tick and reapplies only when the controller type changes (e.g. across scenes), so author additions in `OnStart`.

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

### Where the audio is

- `float Audio_GetTime(uint64)` — seconds into the clip this entity is playing.
- `float Audio_GetLength(uint64)` — how long that clip is, in seconds.
- `bool Audio_Seek(uint64, float seconds)` — jump to a position. Returns whether it happened.

Both getters return **-1**, never 0, when they cannot answer: nothing playing,
no clip, or a backend that does not know. A real `0.0` means "at the very
start", and a caller has to be able to tell those apart.

This is what timed subtitles need. Without it a caption track has to
dead-reckon from the frame `Audio_Play` fired and hard-code the clip length,
and any drift or engine-side loop restart desyncs it permanently with nothing
able to notice:

<!-- sample-context: uint64 radio = Scene_FindEntity("Radio"); -->
```angelscript
// Show the cue whose window contains the current playback position. The track is
// four parallel arrays in a .enjdata asset -- see "Reading a list" below, and
// "Importing a subtitle file" for where it comes from.
float t = Audio_GetTime(radio);
if (t >= 0.0f) {
    int cues = DataAsset_GetArrayLength("wgrb_night", "cue_t");
    for (int i = 0; i < cues; i++) {
        float start = DataAsset_GetFloatAt("wgrb_night", "cue_t", i);
        float end   = DataAsset_GetFloatAt("wgrb_night", "cue_end", i);
        if (t >= start && t < end) {
            Subtitle_Show(DataAsset_GetStringAt("wgrb_night", "cue_line", i),
                          DataAsset_GetStringAt("wgrb_night", "cue_who", i));
            break;
        }
    }
}
```

Read `cue_end`, do not infer the end from the next cue's start. The two agree only
when the dialogue is continuous, and every pause in a conversation is a gap --
inferring holds the previous caption on screen right through the silence.

### Importing a subtitle file

Drop a `.srt` on the editor window, or use **Tools > Scripting & Logic > Import
Captions (.srt)**. It writes `assets/data/<name>.enjdata` as a `CaptionTrack`:

| Field | Type | Holds |
|-------|------|-------|
| `cue_t` | FloatArray | start of each cue, seconds |
| `cue_end` | FloatArray | end of each cue, seconds |
| `cue_who` | StringArray | speaker, `""` where the file does not name one |
| `cue_line` | StringArray | the caption text, newlines kept |

All four are always written and always the same length, so one
`DataAsset_GetArrayLength` walks the whole track.

The speaker is read from the two notations that are unambiguous: WebVTT's
`<v Speaker>` and a leading `[Speaker]`. A bare `NAME: text` is left alone, because
`WGRB: Night desk.` and `Look: over there.` are the same shape and splitting on the
colon would rewrite some captions and not others. The importer counts the lines
that look like a prefix and says so in the Console; **Settings > System > Workflow
> Split caption speaker prefixes** turns the split on for a file where you know it
is right.

A malformed block is reported with its source line number and skipped, and the
notification says "imported N of M blocks" so a track that came out short cannot
look like a clean import.

### Randomised playback

An Audio Source can carry alternate clips and pitch/volume ranges (inspector:
**Randomization**). Each play picks between the assigned clip and its alternates
and jitters pitch and volume inside the authored ranges, so a footstep does not
sound like the same recording every time. Nothing to call — it applies to every
play of that source. Leave the ranges at 1.0 for no variation.

**On web**, no sound plays until the player's first click, tap or key press: a
browser refuses to start an AudioContext before a real user gesture. The engine
holds play-on-awake sources until then so they start from the beginning rather
than partway through. Design for it — a title track begins on the first input,
not on load.

## Sprites (2D)

- `Sprite_SetTexture(uint64, string)`, `string Sprite_GetTexture(uint64)`
- `Sprite_SetSize(uint64, float w, float h)` — world units, not pixels
- `float Sprite_GetWidth(uint64)`, `float Sprite_GetHeight(uint64)`
- `Sprite_SetColor(uint64, float r, float g, float b, float a)`, `Sprite_SetAlpha(uint64, float)`
- `Sprite_SetFlipX/SetFlipY(uint64, bool)`
- `Sprite_SetSortOrder(uint64, int)` — order within the sorting layer
- `Sprite_SetVisible(uint64, bool)`

Size is the sprite's own width and height; the entity's transform **scale**
multiplies it, so either lever works and they compose. Reading the size back is
usually the point — a health bar is a fraction of the authored width:

```angelscript
float full = Sprite_GetWidth(bar);          // once, at start
...
Sprite_SetSize(bar, full * (hp / maxHp), Sprite_GetHeight(bar));
```

## Data Assets

Read-only records loaded from `.enjdata` files in the project, described by
`.enjschema`. Useful for anything a designer should be able to change without
touching script: item tables, enemy stats, caption tracks, localised strings.

- `bool DataAsset_Load(const string &in asset)` — is this record present?
- `string DataAsset_GetString(const string &in asset, const string &in field)`
- `float DataAsset_GetFloat(...)`, `int DataAsset_GetInt(...)`, `bool DataAsset_GetBool(...)`
- `Vector3 DataAsset_GetVector3(...)`, `Vector4 DataAsset_GetVector4(...)`

Lists:

- `int DataAsset_GetArrayLength(const string &in asset, const string &in field)`
- `string DataAsset_GetStringAt(const string &in asset, const string &in field, int index)`
- `float DataAsset_GetFloatAt(const string &in asset, const string &in field, int index)`

Every getter takes the asset name first and the field name second. Field types
are `String`, `Float`, `Int`, `Bool`, `Vector3`, `Vector4`, `StringArray` and
`FloatArray`, and all eight are reachable from script.

```angelscript
if (DataAsset_Load("sword_iron")) {
    float damage = DataAsset_GetFloat("sword_iron", "damage");
    string name  = DataAsset_GetString("sword_iron", "displayName");
}
```

### Reading a list

`GetArrayLength` answers `0` for a missing asset, a missing field, and a field
that is not a list. All three mean "nothing to iterate", so the obvious loop is
correct without a guard:

```angelscript
int cues = DataAsset_GetArrayLength("wgrb_night", "cue_t");
for (int i = 0; i < cues; i++) {
    float at    = DataAsset_GetFloatAt("wgrb_night", "cue_t", i);
    string who  = DataAsset_GetStringAt("wgrb_night", "cue_who", i);
    string line = DataAsset_GetStringAt("wgrb_night", "cue_line", i);
}
```

Reading past the end, or reading a float list as strings, returns the fallback
(`""` / `0.0`) **and logs a warning once** per asset and field. That matters
because an authored empty caption is also `""` — without the warning the two are
indistinguishable, which is the same reason `Audio_GetTime` answers `-1` rather
than `0.0` when it cannot say where a sound is.

Authoring note: a bare JSON array of numbers is always a **float list**, never a
`Vector3` or `Vector4` guessed from its length. A caption track with exactly four
cues would otherwise silently become a vector and read back as empty. Vectors use
the tagged form, which is what the editor writes:

```json
{
  "cue_t":  [0.5, 4.0, 8.0, 11.5],
  "colour": { "type": "Vector4", "value": [0.2, 0.4, 0.6, 1.0] }
}
```

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
- **DynamicDifficulty**: `Difficulty_GetScore(uint64)`, `Difficulty_GetMultiplier(uint64, string)` — "enemyDamage"/"enemyHealth"/"aiAggression"/"resourceDrops"/"checkpoint", `Difficulty_SetBaseDifficulty/GetBaseDifficulty(uint64, uint)`, `Difficulty_RecordDeath(uint64)`, `Difficulty_RecordShot(uint64)`, `Difficulty_RecordHit(uint64)`, `Difficulty_RecordCheckpointHealth(uint64, float)`, `Difficulty_SetEnabled(uint64, bool)`, `Difficulty_SetPlayerEntity(uint64, uint64)` — the entity whose HealthComponent the health metric reads, `Difficulty_SetResourceRatio(uint64, float)`, `Difficulty_Reset(uint64)`
- **State Machine**: `SM_AddState(uint64, string)`, `SM_AddTransition(uint64, from, to)`, `SM_SetState/GetCurrentState/GetPreviousState(uint64)`, `SM_GetStateTime(uint64)`, `SM_SendTrigger(uint64, string)`, `SM_SetBool/GetBool(uint64, string, bool)`, `SM_SetFloat/GetFloat(uint64, string, float)`, `SM_SetInt/GetInt(uint64, string, int)`, `SM_HasState(uint64, string)`, `SM_SetOnEnter/SetOnUpdate/SetOnExit(uint64, stateName, funcName)`, `SM_GetOnEnter/GetOnUpdate/GetOnExit(uint64, stateName)`


## Generated Geometry and Textures

Six CPU generators that build geometry or pixels at runtime. Each is authored as
a component (Add Component > Generated Geometry) and driven by
`GeneratedGeometrySystem`, which runs in the editor, the desktop player and the
web build. The functions below retune a generator that is already on the entity;
changing a value the generator hashes makes it rebuild on the next tick.

Every function is a no-op (or returns a default) when the entity does not carry
the matching component.

### Metaballs

Blobs share a scalar field per `group`; one Metaball Surface entity per group
receives the marching-cubes isosurface into its MeshComponent.

`Metaball_SetRadius(uint64, float)`, `Metaball_SetStrength(uint64, float)` —
negative strength carves instead of adds, `Metaball_SetGroup(uint64, int)`,
`Metaball_SetColor(uint64, float r, float g, float b)`

`MetaballSurface_SetGroup(uint64, int)`,
`MetaballSurface_SetGridResolution(uint64, int)` — clamped 16-64, cost is cubic,
`MetaballSurface_SetGridSize(uint64, float)`

### Cellular automata

`CA_SetRunning(uint64, bool)`, `CA_Reset(uint64)`, `CA_SetRule(uint64, int)` —
0 Game of Life, 1 HighLife, 2 Day and Night, 3 Seeds, 4 Brian's Brain,
5 Rule 110, 6 Diamoeba, 7 Custom,
`CA_SetStampPattern(uint64, string)` — "", "glider", "pulsar", "gospergun"
(resets the grid, since a stamp only applies to a fresh one),
`CA_GetGeneration(uint64)`, `CA_GetLiveCells(uint64)`

### 4D projection

`P4D_SetPolytope(uint64, int)` — 0 tesseract, 1 5-cell, 2 16-cell, 3 24-cell,
4 120-cell, `P4D_SetRotation(uint64, float xy, float xz, float xw, float yz,
float yw, float zw)` — radians/second; the W planes are the ones with no 3D
equivalent, `P4D_SetScale(uint64, float)`, `P4D_SetAnimate(uint64, bool)`

### Fourier contours

`Fourier_SetContour(uint64, int)` — 0 circle, 1 square, 2 star, 3 heart,
4 custom, `Fourier_SetTerms(uint64, int)`, `Fourier_SetExtrude(uint64, float)` —
0 is a flat triangulated contour, `Fourier_GetActiveTerms(uint64)`

### Reaction-diffusion and Physarum (textures)

These two bake RGBA8 into `ProceduralTextureComponent`, which the renderer
uploads and binds as the entity's base colour. They **bake once** per
configuration rather than animating: the only texture upload path in the engine
ends in a graphics-queue idle, so driving it per frame loses the device.
`settleSteps` decides how developed the pattern is when it lands, and every one
of those steps runs in a single frame.

`RD_SetPreset(uint64, int)` — 0 mitosis spots, 1 coral, 2 fingerprints,
3 leopard, 4 labyrinth, 5 worm holes, 6 bubble packing, 7 spirals, 8 custom,
`RD_SetSettleSteps(uint64, uint)`, `RD_Rebake(uint64)`, `RD_GetStepCount(uint64)`

`Physarum_SetPreset(uint64, int)` — 0 classic slime, 1 branching network,
2 dense web, 3 tendrils, 4 pulsating, 5 custom,
`Physarum_SetAgentCount(uint64, uint)` — capped at 500000, every agent moves on
the CPU each step, `Physarum_SetSettleSteps(uint64, uint)`,
`Physarum_Rebake(uint64)`, `Physarum_GetStepCount(uint64)`

Known limits: the texture upload is Vulkan-only, so on web these two components
are inert. Reaction-diffusion currently floods to a uniform field with every
shipped preset and renders as one flat colour; Physarum works.

### Freezing a generator

`ProceduralMesh_SetRegenerate(uint64, bool)`,
`ProceduralTexture_SetRegenerate(uint64, bool)` — false keeps the current
geometry or image and stops the owning system rebuilding it.

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
- `Weather_SetWind(float dirX, float dirY, float dirZ, float strength)` — the WEATHER system's wind, which slants precipitation. It does NOT drive foliage, vegetation or particles; for those use `Wind_SetDirection` / `Wind_SetStrength`, which write the global wind field.
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
- `Particle_SetSize(uint64, float startSize, float endSize)` — Billboard width over lifetime, in world units, multiplied by the emitter entity's scale.
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
- `UI_SetFontSize(uint64, int, float)` — Font size in pixels for a Label/Button element. Pass a negative value to restore the theme default.
- `UI_GetFontSize(uint64, int)` — Current per-element font size override (-1 = theme default).
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
- `Streaming_SetMemoryBudgetMB(int)` — Cap the estimated resident memory of loaded chunks (0 = unlimited). Over budget, the least-recently-near chunk outside its load distance is unloaded; in-range chunks are never evicted.
- `Streaming_GetMemoryBudgetMB()` — Current budget in MB (0 = unlimited).
- `Streaming_GetResidentMB()` — Estimated resident MB of all loaded chunks.
- `Streaming_GetBudgetEvictions()` — Number of chunks the budget has evicted so far.

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

<!-- BEGIN GENERATED BINDING INDEX -- tools/gen_scripting_api.py -->

## Every registered binding

1322 global functions, grouped by where they are registered. These lines are
GENERATED from the registration strings themselves, so a signature here is the
one the engine accepts -- if it disagrees with the prose above, the prose is
wrong. Regenerate with `python tools/gen_scripting_api.py` after adding a
binding.

Signatures only. Where a function needs explaining rather than listing, it is
written up by hand in the sections above; this index exists so that nothing is
merely absent.

### AI and navigation  (34)

- `Vector3 Navmesh_GetPathWaypoint(int)`
- `bool BT_GetBlackboardBool(uint64, const string&in)`
- `bool BT_IsEnabled(uint64)`
- `bool Navmesh_HasNavmesh()`
- `bool Navmesh_IsPointOnNavmesh(float, float, float)`
- `bool Navmesh_PathExists(float, float, float, float, float, float)`
- `float AI_GetAttackRange(uint64)`
- `float AI_GetDetectionRange(uint64)`
- `float AI_GetMoveSpeed(uint64)`
- `float BT_GetBlackboardFloat(uint64, const string&in)`
- `float Navmesh_GetPathCost()`
- `int AI_GetState(uint64)`
- `int BT_GetBlackboardInt(uint64, const string&in)`
- `int Navmesh_FindPath(float, float, float, float, float, float)`
- `string BT_GetBlackboardString(uint64, const string&in)`
- `uint64 AI_GetTarget(uint64)`
- `void AI_SetAttackRange(uint64, float)`
- `void AI_SetChaseSpeed(uint64, float)`
- `void AI_SetDetectionRange(uint64, float)`
- `void AI_SetFieldOfView(uint64, float)`
- `void AI_SetFleeSpeed(uint64, float)`
- `void AI_SetMoveSpeed(uint64, float)`
- `void AI_SetState(uint64, int)`
- `void AI_SetTarget(uint64, uint64)`
- `void AI_SetTargetPosition(uint64, float, float, float)`
- `void AI_SetUseNavmesh(uint64, bool)`
- `void BT_ClearBlackboard(uint64)`
- `void BT_Disable(uint64)`
- `void BT_Enable(uint64)`
- `void BT_Reset(uint64)`
- `void BT_SetBlackboardBool(uint64, const string&in, bool)`
- `void BT_SetBlackboardFloat(uint64, const string&in, float)`
- `void BT_SetBlackboardInt(uint64, const string&in, int)`
- `void BT_SetBlackboardString(uint64, const string&in, const string&in)`

### Accessibility  (42)

- `bool Accessibility_GetAudioIndicators()`
- `bool Accessibility_GetDwellClick()`
- `bool Accessibility_GetDyslexiaFont()`
- `bool Accessibility_GetFlashingLights()`
- `bool Accessibility_GetReducedMotion()`
- `bool Accessibility_GetScreenReader()`
- `bool Accessibility_GetScreenShake()`
- `bool Accessibility_GetStickyDrag()`
- `bool Accessibility_GetSwitchAccess()`
- `bool Announcer_IsEnabled()`
- `bool Subtitle_IsEnabled()`
- `float Accessibility_GetBrightness()`
- `float Accessibility_GetContrast()`
- `float Accessibility_GetFontScale()`
- `float Colorblind_GetStrength()`
- `float Subtitle_GetFontSize()`
- `int Colorblind_GetMode()`
- `void Accessibility_SaveSettings()`
- `void Accessibility_SetAudioIndicators(bool)`
- `void Accessibility_SetBrightness(float)`
- `void Accessibility_SetContrast(float)`
- `void Accessibility_SetDwellClick(bool, float = 0.0)`
- `void Accessibility_SetDyslexiaFont(bool)`
- `void Accessibility_SetFlashingLights(bool)`
- `void Accessibility_SetFontScale(float)`
- `void Accessibility_SetReducedMotion(bool)`
- `void Accessibility_SetScreenReader(bool)`
- `void Accessibility_SetScreenShake(bool)`
- `void Accessibility_SetStickyDrag(bool)`
- `void Accessibility_SetSwitchAccess(bool, float = 0.0)`
- `void Announcer_Announce(const string&in)`
- `void Announcer_AnnounceHighPriority(const string&in)`
- `void Announcer_Clear()`
- `void Announcer_SetEnabled(bool)`
- `void Colorblind_SetMode(int)`
- `void Colorblind_SetStrength(float)`
- `void Subtitle_Clear()`
- `void Subtitle_SetEnabled(bool)`
- `void Subtitle_SetFontSize(float)`
- `void Subtitle_Show(const string&in, const string&in = \`
- `void Subtitle_ShowCaption(const string&in, float = 2.5)`
- `void Subtitle_ShowWithColor(const string&in, const string&in, float, float, float, float = 3.0)`

### Audio  (15)

- `bool Audio_IsPlaying(uint64)`
- `bool Audio_Seek(uint64, float)`
- `float Audio_GetChannelVolume(uint8)`
- `float Audio_GetLength(uint64)`
- `float Audio_GetMasterVolume()`
- `float Audio_GetTime(uint64)`
- `void Audio_Play(uint64)`
- `void Audio_PlayAtPosition(const string &in, const Vector3 &in)`
- `void Audio_SetChannelVolume(uint8, float)`
- `void Audio_SetMasterVolume(float)`
- `void Audio_SetPitch(uint64, float)`
- `void Audio_SetVolume(uint64, float)`
- `void Audio_Stop(uint64)`
- `void Audio_StopAll()`
- `void Audio_StopChannel(uint8)`

### Audio graph  (4)

- `float AudioGraph_GetParameter(const string &in)`
- `void AudioGraph_SetParameter(const string &in, float)`
- `void AudioGraph_StopAll()`
- `void AudioGraph_TriggerEvent(const string &in)`

### Audio-reactive drivers  (17)

- `bool BeatClock_IsBeatThisFrame(uint64)`
- `bool BeatClock_IsDownbeatThisFrame(uint64)`
- `bool Sidechain_IsDucking(uint64)`
- `float AudioReactive_GetCurrentValue(uint64)`
- `float BeatClock_GetBPM(uint64)`
- `float Morph_GetWeight(uint64, const string &in)`
- `float RTPC_GetParameter(uint64, const string &in)`
- `int Conductor_GetState(uint64)`
- `int Morph_GetTargetCount(uint64)`
- `uint BeatClock_GetCurrentBar(uint64)`
- `uint BeatClock_GetCurrentBeat(uint64)`
- `void AudioReactive_SetEnabled(uint64, bool)`
- `void BeatClock_SetBPM(uint64, float)`
- `void Conductor_SetState(uint64, int)`
- `void Morph_SetWeight(uint64, const string &in, float)`
- `void RTPC_SetParameter(uint64, const string &in, float)`
- `void Sidechain_SetEnabled(uint64, bool)`

### Components  (350)

- `Vector3 BoxCollider_GetCenter(uint64)`
- `Vector3 BoxCollider_GetSize(uint64)`
- `Vector3 CapsuleCollider_GetCenter(uint64)`
- `Vector3 Controller_GetVelocity(uint64)`
- `Vector3 Conveyor_GetDirection(uint64)`
- `Vector3 Light_GetColor(uint64)`
- `Vector3 Material_GetBaseColor(uint64)`
- `Vector3 Material_GetSSSColor(uint64)`
- `Vector3 Rigidbody_GetAngularVelocity(uint64)`
- `Vector3 Rigidbody_GetVelocity(uint64)`
- `Vector3 SphereCollider_GetCenter(uint64)`
- `Vector3 Teleporter_GetDestination(uint64)`
- `Vector3 TriggerZone_GetBoxSize(uint64)`
- `Vector3 WaterVehicle_GetForward(uint64)`
- `Vector3 WaterVehicle_GetVelocity(uint64)`
- `bool AddComponent_AudioSource(uint64)`
- `bool AddComponent_BoxCollider(uint64)`
- `bool AddComponent_CapsuleCollider(uint64)`
- `bool AddComponent_Health(uint64)`
- `bool AddComponent_Interactable(uint64)`
- `bool AddComponent_Inventory(uint64)`
- `bool AddComponent_Light(uint64)`
- `bool AddComponent_Material(uint64)`
- `bool AddComponent_Notes(uint64)`
- `bool AddComponent_Rigidbody(uint64)`
- `bool AddComponent_SphereCollider(uint64)`
- `bool AddComponent_Sprite2D(uint64)`
- `bool AddComponent_Tag(uint64)`
- `bool AddComponent_Text(uint64)`
- `bool AddComponent_Timer(uint64)`
- `bool AddComponent_TriggerZone(uint64)`
- `bool Animator_IsPlaying(uint64)`
- `bool BoxCollider_IsTrigger(uint64)`
- `bool Camera_HasVCam(uint64)`
- `bool Camera_IsActive(uint64)`
- `bool Camera_IsEnabled(uint64)`
- `bool Camera_IsManualControl()`
- `bool Camera_IsVCamLive(uint64)`
- `bool CapsuleCollider_IsTrigger(uint64)`
- `bool Controller_GetIgnoreTimeScale(uint64)`
- `bool Conveyor_IsActive(uint64)`
- `bool GameOver_IsTriggered(uint64)`
- `bool GoalZone_IsSatisfied(uint64)`
- `bool HasComponent_Animator(uint64)`
- `bool HasComponent_AudioSource(uint64)`
- `bool HasComponent_BoxCollider(uint64)`
- `bool HasComponent_Camera(uint64)`
- `bool HasComponent_CapsuleCollider(uint64)`
- `bool HasComponent_Conveyor(uint64)`
- `bool HasComponent_Damage(uint64)`
- `bool HasComponent_GoalZone(uint64)`
- `bool HasComponent_Health(uint64)`
- `bool HasComponent_Interactable(uint64)`
- `bool HasComponent_Inventory(uint64)`
- `bool HasComponent_LOD(uint64)`
- `bool HasComponent_Layer(uint64)`
- `bool HasComponent_Light(uint64)`
- `bool HasComponent_Lock(uint64)`
- `bool HasComponent_Material(uint64)`
- `bool HasComponent_MovingPlatform(uint64)`
- `bool HasComponent_Notes(uint64)`
- `bool HasComponent_Pickup(uint64)`
- `bool HasComponent_Resource(uint64)`
- `bool HasComponent_Rigidbody(uint64)`
- `bool HasComponent_SphereCollider(uint64)`
- `bool HasComponent_Switch(uint64)`
- `bool HasComponent_Tag(uint64)`
- `bool HasComponent_Teleporter(uint64)`
- `bool HasComponent_Tilemap(uint64)`
- `bool HasComponent_Timer(uint64)`
- `bool HasComponent_TriggerZone(uint64)`
- `bool Health_IsDead(uint64)`
- `bool Health_IsInvulnerable(uint64)`
- `bool Interactable_HasBeenUsed(uint64)`
- `bool Interactable_IsEnabled(uint64)`
- `bool Inventory_AddItem(uint64, const string &in, int)`
- `bool Inventory_HasItem(uint64, const string &in)`
- `bool Inventory_HasKey(uint64, const string &in)`
- `bool Inventory_RemoveItem(uint64, const string &in, int)`
- `bool LOD_IsEnabled(uint64)`
- `bool Light_GetCastShadows(uint64)`
- `bool Lock_IsLocked(uint64)`
- `bool Lock_IsOpen(uint64)`
- `bool Material_GetPaletteIndexed(uint64)`
- `bool Material_GetStippleTransparency(uint64)`
- `bool MovingPlatform_IsMoving(uint64)`
- `bool Pickup_GetDestroyOnPickup(uint64)`
- `bool Resource_IsDepleted(uint64)`
- `bool Resource_TryConsume(uint64, float)`
- `bool Rigidbody_GetUseGravity(uint64)`
- `bool Rigidbody_IsGrounded(uint64)`
- `bool Rigidbody_IsKinematic(uint64)`
- `bool SphereCollider_IsTrigger(uint64)`
- `bool Switch_IsActive(uint64)`
- `bool Tag_Has(uint64, const string &in)`
- `bool Teleporter_GetPreserveVelocity(uint64)`
- `bool Timer_GetLoop(uint64)`
- `bool Timer_IsComplete(uint64)`
- `bool Timer_IsRunning(uint64)`
- `bool TriggerZone_GetTriggerOnce(uint64)`
- `bool Viewmodel_Get(uint64)`
- `bool WaterVehicle_Has(uint64)`
- `bool WaterVehicle_IsPlaning(uint64)`
- `float Animator_GetSpeed(uint64)`
- `float BoxCollider_GetBounciness(uint64)`
- `float BoxCollider_GetFriction(uint64)`
- `float Camera2D_GetZoom(uint64)`
- `float Camera_GetFOV(uint64)`
- `float Camera_GetOrthoSize(uint64)`
- `float CapsuleCollider_GetBounciness(uint64)`
- `float CapsuleCollider_GetFriction(uint64)`
- `float CapsuleCollider_GetHeight(uint64)`
- `float CapsuleCollider_GetRadius(uint64)`
- `float Controller_GetCameraYaw(uint64)`
- `float Conveyor_GetSpeed(uint64)`
- `float Damage_GetDamage(uint64)`
- `float Damage_GetInterval(uint64)`
- `float Damage_GetKnockback(uint64)`
- `float Health_Get(uint64)`
- `float Health_GetMax(uint64)`
- `float Health_GetPercent(uint64)`
- `float Health_GetShield(uint64)`
- `float Interactable_GetRange(uint64)`
- `float Light_GetIntensity(uint64)`
- `float Light_GetRange(uint64)`
- `float Material_GetIOR(uint64)`
- `float Material_GetOpacity(uint64)`
- `float Material_GetSSSIntensity(uint64)`
- `float Material_GetSSSRadius(uint64)`
- `float Material_GetThickness(uint64)`
- `float Material_GetTransmission(uint64)`
- `float MovingPlatform_GetSpeed(uint64)`
- `float MovingPlatform_GetWaitTime(uint64)`
- `float Pickup_GetRange(uint64)`
- `float Pickup_GetValue(uint64)`
- `float Resource_GetMax(uint64)`
- `float Resource_GetPercent(uint64)`
- `float Resource_GetValue(uint64)`
- `float Rigidbody_GetAngularDrag(uint64)`
- `float Rigidbody_GetDrag(uint64)`
- `float Rigidbody_GetGravityScale(uint64)`
- `float Rigidbody_GetMass(uint64)`
- `float SphereCollider_GetBounciness(uint64)`
- `float SphereCollider_GetFriction(uint64)`
- `float SphereCollider_GetRadius(uint64)`
- `float Teleporter_GetCooldown(uint64)`
- `float Timer_GetDuration(uint64)`
- `float Timer_GetElapsed(uint64)`
- `float Timer_GetProgress(uint64)`
- `float Timer_GetRemaining(uint64)`
- `float TriggerZone_GetSphereRadius(uint64)`
- `float WaterVehicle_GetHeading(uint64)`
- `float WaterVehicle_GetHeel(uint64)`
- `float WaterVehicle_GetHullSpeed(uint64)`
- `float WaterVehicle_GetLeeway(uint64)`
- `float WaterVehicle_GetRudder(uint64)`
- `float WaterVehicle_GetSpeedOverGround(uint64)`
- `float WaterVehicle_GetSpeedThroughWater(uint64)`
- `float WaterVehicle_GetThrottle(uint64)`
- `int Camera_GetPriority(uint64)`
- `int Camera_GetProjectionType(uint64)`
- `int Camera_GetVCamPriority(uint64)`
- `int GoalZone_GetGoalGroup(uint64)`
- `int Inventory_GetCoins(uint64)`
- `int Inventory_GetGems(uint64)`
- `int Inventory_GetItemCount(uint64, const string &in)`
- `int LOD_GetCurrentLOD(uint64)`
- `int LOD_GetLevelCount(uint64)`
- `int Light_GetType(uint64)`
- `int Material_GetAlphaMode(uint64)`
- `int Material_GetPaletteSlot(uint64)`
- `int MovingPlatform_GetWaypointCount(uint64)`
- `int Pickup_GetType(uint64)`
- `int Switch_GetLinkedCount(uint64)`
- `int Tag_GetCount(uint64)`
- `int Tilemap_GetHeight(uint64)`
- `int Tilemap_GetTile(uint64, int, int)`
- `int Tilemap_GetWidth(uint64)`
- `int TriggerZone_GetShape(uint64)`
- `string Animator_GetCurrentAnimation(uint64)`
- `string Camera_GetPresetName(int)`
- `string GoalZone_GetNextScene(uint64)`
- `string GoalZone_GetRequiredTag(uint64)`
- `string Interactable_GetPrompt(uint64)`
- `string Layer_GetName(uint64)`
- `string Lock_GetRequiredKey(uint64)`
- `string Notes_Get(uint64)`
- `string Pickup_GetCustomId(uint64)`
- `string Resource_GetName(uint64)`
- `string Switch_GetPrompt(uint64)`
- `string Tag_GetAt(uint64, int)`
- `uint BoxCollider_GetCategoryBits(uint64)`
- `uint BoxCollider_GetCollisionMask(uint64)`
- `uint Layer_GetLayer(uint64)`
- `uint SphereCollider_GetCategoryBits(uint64)`
- `uint SphereCollider_GetCollisionMask(uint64)`
- `uint64 Camera2D_GetFollowTarget(uint64)`
- `uint64 Camera_GetActive()`
- `uint64 Switch_GetLinkedEntity(uint64, int)`
- `void Animator_CrossFade(uint64, const string &in, float)`
- `void Animator_Pause(uint64)`
- `void Animator_Play(uint64, const string &in)`
- `void Animator_Resume(uint64)`
- `void Animator_SetSpeed(uint64, float)`
- `void Animator_Stop(uint64)`
- `void AudioSource_Play(uint64)`
- `void AudioSource_SetClip(uint64, const string &in)`
- `void AudioSource_SetVolume(uint64, float)`
- `void AudioSource_Stop(uint64)`
- `void BoxCollider_SetBounciness(uint64, float)`
- `void BoxCollider_SetCategoryBits(uint64, uint)`
- `void BoxCollider_SetCenter(uint64, float, float, float)`
- `void BoxCollider_SetCollisionMask(uint64, uint)`
- `void BoxCollider_SetFriction(uint64, float)`
- `void BoxCollider_SetSize(uint64, float, float, float)`
- `void BoxCollider_SetTrigger(uint64, bool)`
- `void Camera2D_AddTarget(uint64, uint64)`
- `void Camera2D_ClearTargets(uint64)`
- `void Camera2D_RemoveTarget(uint64, uint64)`
- `void Camera2D_SetDeadZone(uint64, float, float)`
- `void Camera2D_SetFollowTarget(uint64, uint64)`
- `void Camera2D_SetLookAhead(uint64, float, float)`
- `void Camera2D_SetZoom(uint64, float)`
- `void Camera2D_Shake(uint64, float, float)`
- `void Camera_ApplyPreset(uint64, int)`
- `void Camera_ApplyVCamShot(uint64, int)`
- `void Camera_MakeActive(uint64)`
- `void Camera_ReleaseManualControl()`
- `void Camera_SetEnabled(uint64, bool)`
- `void Camera_SetFOV(uint64, float)`
- `void Camera_SetNearFar(uint64, float, float)`
- `void Camera_SetOrthoSize(uint64, float)`
- `void Camera_SetPriority(uint64, int)`
- `void Camera_SetProjectionType(uint64, int)`
- `void Camera_SetVCamEnabled(uint64, bool)`
- `void Camera_SetVCamFOV(uint64, float)`
- `void Camera_SetVCamOffset(uint64, float, float, float)`
- `void Camera_SetVCamPriority(uint64, int)`
- `void Camera_TakeManualControl(uint64)`
- `void CapsuleCollider_SetBounciness(uint64, float)`
- `void CapsuleCollider_SetCenter(uint64, float, float, float)`
- `void CapsuleCollider_SetFriction(uint64, float)`
- `void CapsuleCollider_SetHeight(uint64, float)`
- `void CapsuleCollider_SetRadius(uint64, float)`
- `void CapsuleCollider_SetTrigger(uint64, bool)`
- `void Controller_SetCameraYaw(uint64, float)`
- `void Controller_SetEnabled(uint64, bool)`
- `void Controller_SetIgnoreTimeScale(uint64, bool)`
- `void Controller_SetMouseLook(uint64, bool)`
- `void Controller_SetMoveSpeed(uint64, float)`
- `void Controller_SetThirdPersonCamera(uint64, float, float, float)`
- `void Conveyor_SetActive(uint64, bool)`
- `void Conveyor_SetDirection(uint64, float, float, float)`
- `void Conveyor_SetSpeed(uint64, float)`
- `void Damage_SetDamage(uint64, float)`
- `void Damage_SetInterval(uint64, float)`
- `void Damage_SetKnockback(uint64, float)`
- `void GameOver_SetMessages(uint64, const string &in, const string &in)`
- `void GameOver_Trigger(uint64, bool)`
- `void Health_Damage(uint64, float)`
- `void Health_Heal(uint64, float)`
- `void Health_SetCurrent(uint64, float)`
- `void Health_SetInvulnerable(uint64, bool)`
- `void Health_SetMaxHealth(uint64, float)`
- `void Health_SetShield(uint64, float)`
- `void Interactable_SetEnabled(uint64, bool)`
- `void Interactable_SetPrompt(uint64, const string &in)`
- `void Interactable_SetRange(uint64, float)`
- `void Inventory_AddKey(uint64, const string &in)`
- `void Inventory_Clear(uint64)`
- `void Inventory_SetCoins(uint64, int)`
- `void Inventory_SetGems(uint64, int)`
- `void LOD_SetEnabled(uint64, bool)`
- `void Layer_SetLayer(uint64, uint)`
- `void Light_SetCastShadows(uint64, bool)`
- `void Light_SetColor(uint64, const Vector3 &in)`
- `void Light_SetIntensity(uint64, float)`
- `void Light_SetRange(uint64, float)`
- `void Light_SetSpotAngles(uint64, float, float)`
- `void Light_SetType(uint64, int)`
- `void Lock_SetLocked(uint64, bool)`
- `void Lock_SetOpen(uint64, bool)`
- `void Material_SetAlphaMode(uint64, int)`
- `void Material_SetBaseColor(uint64, const Vector3 &in)`
- `void Material_SetBaseColorTexture(uint64, const string &in)`
- `void Material_SetEmissiveTexture(uint64, const string &in)`
- `void Material_SetIOR(uint64, float)`
- `void Material_SetMetallic(uint64, float)`
- `void Material_SetNormalTexture(uint64, const string &in)`
- `void Material_SetOpacity(uint64, float)`
- `void Material_SetPaletteIndexed(uint64, bool)`
- `void Material_SetPaletteSlot(uint64, int)`
- `void Material_SetRoughness(uint64, float)`
- `void Material_SetSSSColor(uint64, const Vector3 &in)`
- `void Material_SetSSSIntensity(uint64, float)`
- `void Material_SetSSSRadius(uint64, float)`
- `void Material_SetStippleTransparency(uint64, bool)`
- `void Material_SetThickness(uint64, float)`
- `void Material_SetTransmission(uint64, float)`
- `void MovingPlatform_SetMoving(uint64, bool)`
- `void MovingPlatform_SetSpeed(uint64, float)`
- `void MovingPlatform_SetWaitTime(uint64, float)`
- `void Notes_Set(uint64, const string &in)`
- `void Pickup_SetCustomId(uint64, const string &in)`
- `void Pickup_SetDestroyOnPickup(uint64, bool)`
- `void Pickup_SetRange(uint64, float)`
- `void Pickup_SetType(uint64, int)`
- `void Pickup_SetValue(uint64, float)`
- `void Resource_SetMax(uint64, float)`
- `void Resource_SetValue(uint64, float)`
- `void Rigidbody_SetAngularDrag(uint64, float)`
- `void Rigidbody_SetAngularVelocity(uint64, float, float, float)`
- `void Rigidbody_SetDrag(uint64, float)`
- `void Rigidbody_SetGravityScale(uint64, float)`
- `void Rigidbody_SetKinematic(uint64, bool)`
- `void Rigidbody_SetMass(uint64, float)`
- `void Rigidbody_SetUseGravity(uint64, bool)`
- `void Rigidbody_SetVelocity(uint64, float, float, float)`
- `void SphereCollider_SetBounciness(uint64, float)`
- `void SphereCollider_SetCategoryBits(uint64, uint)`
- `void SphereCollider_SetCenter(uint64, float, float, float)`
- `void SphereCollider_SetCollisionMask(uint64, uint)`
- `void SphereCollider_SetFriction(uint64, float)`
- `void SphereCollider_SetRadius(uint64, float)`
- `void SphereCollider_SetTrigger(uint64, bool)`
- `void Switch_SetActive(uint64, bool)`
- `void Tag_Add(uint64, const string &in)`
- `void Tag_Remove(uint64, const string &in)`
- `void Teleporter_SetCooldown(uint64, float)`
- `void Teleporter_SetDestination(uint64, float, float, float)`
- `void Teleporter_SetPreserveVelocity(uint64, bool)`
- `void Tilemap_SetTile(uint64, int, int, int)`
- `void Timer_SetDuration(uint64, float)`
- `void Timer_SetElapsed(uint64, float)`
- `void Timer_SetLoop(uint64, bool)`
- `void Timer_SetRunning(uint64, bool)`
- `void TriggerZone_SetBoxSize(uint64, float, float, float)`
- `void TriggerZone_SetShape(uint64, int)`
- `void TriggerZone_SetSphereRadius(uint64, float)`
- `void TriggerZone_SetTriggerOnce(uint64, bool)`
- `void Viewmodel_Set(uint64, bool)`
- `void WaterVehicle_SetCurrent(uint64, float, float)`
- `void WaterVehicle_SetDrive(uint64, float, float, float)`
- `void WaterVehicle_SetHeading(uint64, float)`
- `void WaterVehicle_SetHullSpeed(uint64, float)`
- `void WaterVehicle_SetLateralGrip(uint64, float)`
- `void WaterVehicle_SetMaxThrust(uint64, float)`
- `void WaterVehicle_SetRighting(uint64, float)`
- `void WaterVehicle_SetRudder(uint64, float)`
- `void WaterVehicle_SetThrottle(uint64, float)`

### Core, math, debug and entity  (67)

- `Quaternion Quaternion_FromEuler(const Vector3 &in)`
- `Quaternion Quaternion_Identity()`
- `Quaternion Quaternion_Slerp(const Quaternion &in, const Quaternion &in, float)`
- `Vector3 DataAsset_GetVector3(const string &in, const string &in)`
- `Vector4 DataAsset_GetVector4(const string &in, const string &in)`
- `bool DataAsset_GetBool(const string &in, const string &in)`
- `bool DataAsset_Load(const string &in)`
- `float Abs(float)`
- `float Acos(float)`
- `float Asin(float)`
- `float Atan2(float, float)`
- `float Ceil(float)`
- `float Clamp(float, float, float)`
- `float Cos(float)`
- `float DataAsset_GetFloat(const string &in, const string &in)`
- `float DataAsset_GetFloatAt(const string &in, const string &in, int)`
- `float Degrees(float)`
- `float Events_CurrentFloat(const string &in)`
- `float Floor(float)`
- `float Lerp(float, float, float)`
- `float Max(float, float)`
- `float Min(float, float)`
- `float MoveTowards(float, float, float)`
- `float PI()`
- `float Pow(float, float)`
- `float Radians(float)`
- `float Random()`
- `float RandomRange(float, float)`
- `float Round(float)`
- `float Sign(float)`
- `float Sin(float)`
- `float Sqrt(float)`
- `float Tan(float)`
- `float Time_GetDeltaTime()`
- `float Time_GetFixedDeltaTime()`
- `float Time_GetScale()`
- `float Time_GetTime()`
- `float Time_GetTimeScale()`
- `float VisualScript_GetVariable(uint64, const string &in)`
- `int DataAsset_GetArrayLength(const string &in, const string &in)`
- `int DataAsset_GetInt(const string &in, const string &in)`
- `int DataAsset_ListAll()`
- `int DataAsset_ListBySchema(const string &in)`
- `int Events_CurrentInt(const string &in)`
- `int RandomInt(int, int)`
- `string DataAsset_GetListResult(int)`
- `string DataAsset_GetSchemaName(const string &in)`
- `string DataAsset_GetString(const string &in, const string &in)`
- `string DataAsset_GetStringAt(const string &in, const string &in, int)`
- `string Events_CurrentString(const string &in)`
- `uint Events_Listen(const string &in, EventCallback@)`
- `uint StartCoroutine(const string &in)`
- `uint Time_GetFrameCount()`
- `void Debug_Log(const string &in)`
- `void Debug_LogError(const string &in)`
- `void Debug_LogWarning(const string &in)`
- `void Events_Broadcast(EventData@)`
- `void Events_Send(const string &in, EventData@)`
- `void StopAllCoroutines()`
- `void StopCoroutine(uint)`
- `void Time_SetScale(float)`
- `void Time_SetTimeScale(float)`
- `void VisualScript_SendEvent(uint64, const string &in)`
- `void VisualScript_SetVariable(uint64, const string &in, float)`
- `void YieldEndOfFrame()`
- `void YieldFrames(uint)`
- `void YieldSeconds(float)`

### Dialogue  (10)

- `bool Dialogue_IsActive(uint64)`
- `string Dialogue_GetChoiceText(uint64, uint)`
- `string Dialogue_GetCurrentSpeaker(uint64)`
- `string Dialogue_GetCurrentText(uint64)`
- `string Dialogue_GetVariable(uint64, const string&in)`
- `uint Dialogue_GetChoiceCount(uint64)`
- `void Dialogue_Advance(uint64)`
- `void Dialogue_Choose(uint64, uint)`
- `void Dialogue_SetVariable(uint64, const string&in, const string&in)`
- `void Dialogue_Start(uint64)`

### Elemental effects  (29)

- `bool Elemental_HasEmitter(uint64)`
- `bool Elemental_HasSurface(uint64)`
- `bool Elemental_HasVolume(uint64)`
- `bool Elemental_IsEmitterActive(uint64)`
- `float Elemental_GetEmitterRate(uint64)`
- `float Elemental_GetFireIntensityAt(float, float, float, float)`
- `float Elemental_GetFlammability(uint64)`
- `float Elemental_GetMoistureAt(float, float, float, float)`
- `float Elemental_GetSurfaceChar(uint64)`
- `float Elemental_GetSurfaceFrost(uint64)`
- `float Elemental_GetSurfaceSnow(uint64)`
- `float Elemental_GetSurfaceWetness(uint64)`
- `int Elemental_GetActiveCount()`
- `int Elemental_GetFireCount()`
- `int Elemental_GetWaterCount()`
- `uint Elemental_SpawnEarth(float, float, float, float, float, float, float)`
- `uint Elemental_SpawnFire(float, float, float, float, float)`
- `uint Elemental_SpawnSnow(float, float, float, float)`
- `uint Elemental_SpawnSteam(float, float, float, float)`
- `uint Elemental_SpawnWater(float, float, float, float, float, float, float)`
- `void Elemental_SetEmitterActive(uint64, bool)`
- `void Elemental_SetEmitterElement(uint64, float, float, float, float)`
- `void Elemental_SetEmitterIntensity(uint64, float)`
- `void Elemental_SetEmitterRate(uint64, float)`
- `void Elemental_SetFlammability(uint64, float)`
- `void Elemental_SetVolumeKill(uint64, bool)`
- `void Elemental_SetVolumeTempBias(uint64, float)`
- `void Elemental_SpawnDebrisBurst(float, float, float, float, float, float, int)`
- `void Elemental_SpawnRainBurst(float, float, float, float, int)`

### Flash API shim  (58)

- `bool Flash_GetVisible(uint64)`
- `bool Flash_IsKeyDown(int)`
- `bool Flash_SO_Has(const string &in, const string &in)`
- `float Flash_GetAlpha(uint64)`
- `float Flash_GetFrameRate()`
- `float Flash_GetMouseX()`
- `float Flash_GetMouseY()`
- `float Flash_GetRotation(uint64)`
- `float Flash_GetScaleX(uint64)`
- `float Flash_GetScaleY(uint64)`
- `float Flash_GetStageHeight()`
- `float Flash_GetStageWidth()`
- `float Flash_GetX(uint64)`
- `float Flash_GetY(uint64)`
- `float Flash_MathAbs(float)`
- `float Flash_MathAtan2(float, float)`
- `float Flash_MathCeil(float)`
- `float Flash_MathCos(float)`
- `float Flash_MathFloor(float)`
- `float Flash_MathRandom()`
- `float Flash_MathRound(float)`
- `float Flash_MathSin(float)`
- `float Flash_MathSqrt(float)`
- `float Math_Abs(float)`
- `float Math_Atan2(float, float)`
- `float Math_Ceil(float)`
- `float Math_Cos(float)`
- `float Math_Floor(float)`
- `float Math_Random()`
- `float Math_Round(float)`
- `float Math_Sin(float)`
- `float Math_Sqrt(float)`
- `int Flash_GetCurrentFrame(uint64)`
- `int Flash_GetTotalFrames(uint64)`
- `string Flash_GetText(uint64)`
- `string Flash_SO_Get(const string &in, const string &in)`
- `uint Flash_SetInterval(float)`
- `uint Flash_SetTimeout(float)`
- `uint64 Flash_GetChildByName(uint64, const string &in)`
- `void Flash_ClearInterval(uint)`
- `void Flash_GotoAndPlay(uint64, int)`
- `void Flash_GotoAndStop(uint64, int)`
- `void Flash_Play(uint64)`
- `void Flash_PlaySound(const string &in)`
- `void Flash_SO_Clear(const string &in)`
- `void Flash_SO_Flush(const string &in)`
- `void Flash_SO_Set(const string &in, const string &in, const string &in)`
- `void Flash_SetAlpha(uint64, float)`
- `void Flash_SetRotation(uint64, float)`
- `void Flash_SetScaleX(uint64, float)`
- `void Flash_SetScaleY(uint64, float)`
- `void Flash_SetText(uint64, const string &in)`
- `void Flash_SetVisible(uint64, bool)`
- `void Flash_SetVolume(const string &in, float)`
- `void Flash_SetX(uint64, float)`
- `void Flash_SetY(uint64, float)`
- `void Flash_Stop(uint64)`
- `void Flash_StopSound(const string &in)`

### Flower  (24)

- `bool Flower_HasGrabbable(uint64)`
- `bool Flower_HasJelly(uint64)`
- `bool Flower_HasStem(uint64)`
- `bool Flower_HasTether(uint64)`
- `bool Flower_IsBroken(uint64)`
- `bool Flower_IsEvaluated(uint64)`
- `bool Flower_IsGrabbed(uint64)`
- `bool Flower_JustBroke(uint64)`
- `float Flower_GetGrabRadius(uint64)`
- `float Flower_GetGrabSpring(uint64)`
- `float Flower_GetMaxDistance(uint64)`
- `float Flower_GetScore(uint64)`
- `float Flower_GetTension(uint64)`
- `int Flower_GetPartsRemoved(uint64)`
- `void Flower_SetDamping(uint64, float)`
- `void Flower_SetGrabRadius(uint64, float)`
- `void Flower_SetGrabSpring(uint64, float)`
- `void Flower_SetGroundLevel(uint64, float)`
- `void Flower_SetJellyDamping(uint64, float)`
- `void Flower_SetJellyStiffness(uint64, float)`
- `void Flower_SetLiquidIntensity(uint64, float)`
- `void Flower_SetMaxDistance(uint64, float)`
- `void Flower_SetSapColor(uint64, float, float, float)`
- `void Flower_SetSpringK(uint64, float)`

### Gameplay components  (71)

- `bool Possessable_IsPossessed(uint64)`
- `bool Pushable_IsBeingPushed(uint64)`
- `bool Ragdoll_IsActive(uint64)`
- `bool ReflectionProbe_IsActive(uint64)`
- `bool SavePoint_IsUsed(uint64)`
- `float DamageResist_Get(uint64, const string &in)`
- `float Difficulty_GetMultiplier(uint64, const string &in)`
- `float Difficulty_GetScore(uint64)`
- `float LookAt_GetSpeed(uint64)`
- `float ReflectionProbe_GetIntensity(uint64)`
- `float SliderJoint_GetDisplacement(uint64)`
- `float SpringJoint_GetStress(uint64)`
- `float TempZone_GetTemperature(uint64)`
- `uint Difficulty_GetBaseDifficulty(uint64)`
- `void BallSocket_SetConeLimit(uint64, bool, float)`
- `void BallSocket_SetTwistLimit(uint64, bool, float, float)`
- `void Billboard_SetFaceCamera(uint64, bool)`
- `void Billboard_SetLockY(uint64, bool)`
- `void Billboard_SetRotationOffset(uint64, float)`
- `void DamageResist_Set(uint64, const string &in, float)`
- `void Difficulty_RecordCheckpointHealth(uint64, float)`
- `void Difficulty_RecordDeath(uint64)`
- `void Difficulty_RecordHit(uint64)`
- `void Difficulty_RecordShot(uint64)`
- `void Difficulty_Reset(uint64)`
- `void Difficulty_SetBaseDifficulty(uint64, uint)`
- `void Difficulty_SetEnabled(uint64, bool)`
- `void Difficulty_SetPlayerEntity(uint64, uint64)`
- `void Difficulty_SetResourceRatio(uint64, float)`
- `void FixedJoint_SetBreakable(uint64, bool, float)`
- `void Footstep_SetPitchVariance(uint64, float)`
- `void Footstep_SetRunInterval(uint64, float)`
- `void Footstep_SetVolume(uint64, float)`
- `void Footstep_SetWalkInterval(uint64, float)`
- `void Lens_SetAnamorphicSqueeze(uint64, float)`
- `void Lens_SetChromaticAberration(uint64, float)`
- `void Lens_SetDistortion(uint64, float)`
- `void Lens_SetEnabled(uint64, bool)`
- `void Lens_SetVignette(uint64, float, float)`
- `void LookAt_ClearTarget(uint64)`
- `void LookAt_SetConstraints(uint64, bool, bool, bool)`
- `void LookAt_SetInstant(uint64, bool)`
- `void LookAt_SetSpeed(uint64, float)`
- `void LookAt_SetTarget(uint64, uint64)`
- `void LookAt_SetTargetPosition(uint64, float, float, float)`
- `void Possessable_SetPlayerIndex(uint64, int)`
- `void Possessable_SetPrompt(uint64, const string &in)`
- `void Possessable_SetRange(uint64, float)`
- `void Pushable_SetAxes(uint64, bool, bool, bool)`
- `void Pushable_SetPushSpeed(uint64, float)`
- `void Ragdoll_SetActive(uint64, bool)`
- `void Ragdoll_SetBlendWeight(uint64, float)`
- `void Ragdoll_SetGravityScale(uint64, float)`
- `void ReflectionProbe_SetActive(uint64, bool)`
- `void ReflectionProbe_SetIntensity(uint64, float)`
- `void Reverb_SetActive(uint64, bool)`
- `void Reverb_SetDamping(uint64, float)`
- `void Reverb_SetDecayTime(uint64, float)`
- `void Reverb_SetRoomSize(uint64, float)`
- `void Reverb_SetWetDryMix(uint64, float)`
- `void SavePoint_SetMessage(uint64, const string &in)`
- `void SavePoint_SetRadius(uint64, float)`
- `void SavePoint_SetSaveOnEnter(uint64, bool)`
- `void SavePoint_SetSlot(uint64, int)`
- `void SliderJoint_SetLimits(uint64, bool, float, float)`
- `void SliderJoint_SetMotor(uint64, bool, float, float)`
- `void SpringJoint_SetDamping(uint64, float)`
- `void SpringJoint_SetRestLength(uint64, float)`
- `void SpringJoint_SetStiffness(uint64, float)`
- `void TempZone_SetPriority(uint64, int)`
- `void TempZone_SetTemperature(uint64, float)`

### Gameplay systems  (18)

- `bool Cinematic_IsPlaying()`
- `bool QuestFlow_HasVariable(uint64, const string &in)`
- `bool Quest_IsActive(const string &in)`
- `bool Quest_IsComplete(const string &in)`
- `string QuestFlow_GetVariable(uint64, const string &in)`
- `uint64 Pool_Acquire(const string &in)`
- `void Cinematic_Play(uint64)`
- `void Cinematic_Stop(uint64)`
- `void Destructible_ApplyDamage(uint64, float)`
- `void Destructible_ApplyDamageAt(uint64, float, float, float, float)`
- `void Destructible_Destroy(uint64, float, float, float, float)`
- `void Pool_Release(const string &in, uint64)`
- `void QuestFlow_ClearVariable(uint64, const string &in)`
- `void QuestFlow_SetInt(uint64, const string &in, int)`
- `void QuestFlow_SetVariable(uint64, const string &in, const string &in)`
- `void Quest_CompleteObjective(const string &in, int)`
- `void Quest_Fail(const string &in)`
- `void Quest_Start(const string &in)`

### HUD  (15)

- `bool HUD_IsVisible(uint64)`
- `float HUD_GetMaxValue(uint64)`
- `float HUD_GetValue(uint64)`
- `string HUD_GetText(uint64)`
- `void HUD_SetBindField(uint64, const string &in)`
- `void HUD_SetFillColor(uint64, float, float, float)`
- `void HUD_SetFontSize(uint64, float)`
- `void HUD_SetPosition(uint64, float, float)`
- `void HUD_SetSize(uint64, float, float)`
- `void HUD_SetSourceEntity(uint64, uint64)`
- `void HUD_SetText(uint64, const string &in)`
- `void HUD_SetTextColor(uint64, float, float, float)`
- `void HUD_SetValue(uint64, float, float)`
- `void HUD_SetVisible(uint64, bool)`
- `void HUD_SetWorldOffset(uint64, const Vector3 &in)`

### Input  (30)

- `Vector2 Input_GetGamepadLeftStick(int)`
- `Vector2 Input_GetGamepadRightStick(int)`
- `Vector2 Input_GetMouseDelta()`
- `Vector2 Input_GetMousePosition()`
- `Vector2 Input_GetScrollDelta()`
- `bool Input_GetGamepadButton(int, int)`
- `bool Input_GetGamepadButtonDown(int, int)`
- `bool Input_GetKey(int)`
- `bool Input_GetKeyDown(int)`
- `bool Input_GetKeyUp(int)`
- `bool Input_GetMouseButton(int)`
- `bool Input_GetMouseButtonDown(int)`
- `bool Input_GetMouseButtonUp(int)`
- `bool Input_IsGamepadConnected(int)`
- `bool Input_IsMouseCaptured()`
- `float Input_GetGamepadAxis(int, int)`
- `float Input_GetGamepadLeftTrigger(int)`
- `float Input_GetGamepadRightTrigger(int)`
- `float Input_GetPinchDelta()`
- `int Input_GetTextInputCount()`
- `int Input_GetTouchCount()`
- `string Input_GetTextInput()`
- `void Input_SetMouseCaptured(bool)`
- `void Touch_AddActionButton(const string &in, int, float, float, float)`
- `void Touch_AddButton(const string &in, int, float, float, float)`
- `void Touch_ClearButtons()`
- `void Touch_SetLookRegion(bool)`
- `void Touch_SetStick(bool, int, int, int, int)`
- `void Touch_SetStickActions(bool, int, int, int, int)`
- `void Touch_UsePreset(int)`

### Input actions and rebinding  (25)

- `Vector2 InputAction_GetMovement()`
- `bool InputAction_IsCrouchToggle()`
- `bool InputAction_IsDown(int action)`
- `bool InputAction_IsPressed(int action)`
- `bool InputAction_IsReleased(int action)`
- `bool InputAction_IsSprintToggle()`
- `float InputAction_GetMouseSensitivity()`
- `float InputAction_GetValue(int action)`
- `int InputAction_GetCount()`
- `int InputAction_PollNextKey()`
- `string InputAction_GetBindingName(int index)`
- `string InputAction_GetName(int index)`
- `void InputAction_AddGamepadBinding(int action, int button)`
- `void InputAction_AddMouseBinding(int action, int button)`
- `void InputAction_ApplyGamepadOnly()`
- `void InputAction_ApplyLeftHandOnly()`
- `void InputAction_ApplyRightHandOnly()`
- `void InputAction_ClearBindings(int action)`
- `void InputAction_Rebind(int actionIndex, int keyCode)`
- `void InputAction_ResetDefaults()`
- `void InputAction_SetCrouchToggle(bool toggle)`
- `void InputAction_SetMouseSensitivity(float sens)`
- `void InputAction_SetName(int action, const string &in name)`
- `void InputAction_SetSensitivity(int action, float sensitivity)`
- `void InputAction_SetSprintToggle(bool toggle)`

### Level streaming  (10)

- `bool Streaming_IsLoaded(const string &in)`
- `float Streaming_GetResidentMB()`
- `int Streaming_GetBudgetEvictions()`
- `int Streaming_GetLoadedCount()`
- `int Streaming_GetMemoryBudgetMB()`
- `int Streaming_GetState(const string &in)`
- `void Streaming_ForceLoad(const string &in)`
- `void Streaming_ForceUnload(const string &in)`
- `void Streaming_SetEnabled(bool)`
- `void Streaming_SetMemoryBudgetMB(int)`

### MIDI  (11)

- `bool MIDI_IsDeviceOpen()`
- `bool MIDI_IsNoteOff(uint8, uint8 = 0xFF)`
- `bool MIDI_IsNoteOn(uint8, uint8 = 0xFF)`
- `bool MIDI_OpenDevice(uint32)`
- `string MIDI_GetDeviceName(uint32)`
- `uint32 MIDI_GetDeviceCount()`
- `uint32 MIDI_GetEventCount()`
- `uint8 MIDI_GetCC(uint8, uint8 = 0xFF)`
- `uint8 MIDI_GetCCValue(uint8, uint8 = 0)`
- `uint8 MIDI_GetNoteVelocity(uint8, uint8 = 0xFF)`
- `void MIDI_CloseDevice()`

### Networking (LAN)  (20)

- `bool Net_GetLobbyPlayerReady(int)`
- `bool Net_HostGame(int, const string &in)`
- `bool Net_IsConnected()`
- `bool Net_IsHost()`
- `bool Net_JoinGame(const string &in, int, const string &in)`
- `float Net_GetPacketLoss()`
- `float Net_GetPing()`
- `int Net_GetLobbyPlayerCount()`
- `int Net_GetLocalPlayerId()`
- `int Net_GetPlayerCount()`
- `int Net_GetRole()`
- `int Net_RegisterEntity(uint64)`
- `string Net_GetLobbyPlayerName(int)`
- `void Net_CallRPC(const string &in, int, const string &in)`
- `void Net_CallRPCAll(const string &in, const string &in)`
- `void Net_Disconnect()`
- `void Net_RegisterRPCHandler(const string &in)`
- `void Net_RequestOwnership(int)`
- `void Net_SetReady(bool)`
- `void Net_UnregisterEntity(int)`

### Noise  (16)

- `float Noise_Billow2D(float, float, int, float, float, float, uint)`
- `float Noise_Billow3D(float, float, float, int, float, float, float, uint)`
- `float Noise_DomainWarp2D(float, float, float, float, uint)`
- `float Noise_DomainWarp3D(float, float, float, float, float, uint)`
- `float Noise_FBM2D(float, float, int, float, float, float, uint)`
- `float Noise_FBM3D(float, float, float, int, float, float, float, uint)`
- `float Noise_Perlin2D(float, float, uint)`
- `float Noise_Perlin3D(float, float, float, uint)`
- `float Noise_Ridged2D(float, float, int, float, float, float, uint)`
- `float Noise_Ridged3D(float, float, float, int, float, float, float, uint)`
- `float Noise_Simplex2D(float, float, uint)`
- `float Noise_Simplex3D(float, float, float, uint)`
- `float Noise_Value2D(float, float, uint)`
- `float Noise_Value3D(float, float, float, uint)`
- `float Noise_Worley2D(float, float, uint)`
- `float Noise_Worley3D(float, float, float, uint)`

### Particles  (15)

- `bool Particle_IsPlaying(uint64)`
- `float Particle_GetEmissionRate(uint64)`
- `void GPUParticle_Burst(uint64, int)`
- `void Particle_ApplyPreset(uint64, const string &in)`
- `void Particle_Burst(uint64, int)`
- `void Particle_Play(uint64)`
- `void Particle_SetAlpha(uint64, float, float)`
- `void Particle_SetColor(uint64, float, float, float, float, float, float)`
- `void Particle_SetEmissionRate(uint64, float)`
- `void Particle_SetGravity(uint64, float, float, float)`
- `void Particle_SetLifetime(uint64, float)`
- `void Particle_SetLoop(uint64, bool)`
- `void Particle_SetSize(uint64, float, float)`
- `void Particle_SetSpeed(uint64, float)`
- `void Particle_Stop(uint64)`

### Physics  (34)

- `Vector2 Camera_WorldToScreen(uint64 camera, const Vector3 &in worldPoint)`
- `Vector2 Input_GetScreenSize()`
- `Vector3 Camera_ScreenToWorld(uint64 camera, const Vector2 &in screen)`
- `Vector3 Camera_ScreenToWorldOnPlane(uint64 camera, const Vector2 &in screen, float planeZ)`
- `Vector3 Physics_GetVelocity(uint64)`
- `bool Controls_IsHintVisible()`
- `bool Physics_CheckBox(const Vector3 &in, const Vector3 &in)`
- `bool Physics_CheckBox(const Vector3 &in, const Vector3 &in, uint)`
- `bool Physics_CheckSphere(const Vector3 &in, float)`
- `bool Physics_CheckSphere(const Vector3 &in, float, uint)`
- `bool Physics_Raycast(const Vector3 &in, const Vector3 &in, float)`
- `bool Physics_Raycast(const Vector3 &in, const Vector3 &in, float, uint)`
- `bool Physics_RaycastHit(const Vector3 &in, const Vector3 &in, float, RaycastHit &out)`
- `bool Physics_RaycastHit(const Vector3 &in, const Vector3 &in, float, uint, RaycastHit &out)`
- `float DistanceJoint_GetCurrentStress(uint64)`
- `float HingeJoint_GetCurrentAngle(uint64)`
- `int Physics_OverlapBoxEntities(const Vector3 &in, const Vector3 &in)`
- `int Physics_OverlapBoxEntitiesMask(const Vector3 &in, const Vector3 &in, uint)`
- `int Physics_OverlapSphereEntities(const Vector3 &in, float)`
- `int Physics_OverlapSphereEntitiesMask(const Vector3 &in, float, uint)`
- `uint64 Physics_CreateDistanceJoint(uint64, uint64, float)`
- `uint64 Physics_CreateHingeJoint(uint64, uint64, float, float, float)`
- `uint64 Physics_GetOverlapResult(int)`
- `uint64 Physics_RaycastScreen(float, float)`
- `void Controls_SetHintVisible(bool)`
- `void DistanceJoint_SetRestDistance(uint64, float)`
- `void HingeJoint_SetLimits(uint64, float, float)`
- `void HingeJoint_SetMotor(uint64, float, float)`
- `void Physics_AddForce(uint64, const Vector3 &in)`
- `void Physics_AddImpulse(uint64, const Vector3 &in)`
- `void Physics_DestroyJoint(uint64)`
- `void Physics_SetGravityScale(uint64, float)`
- `void Physics_SetVelocity(uint64, const Vector3 &in)`
- `void Physics_Teleport(uint64, const Vector3 &in)`

### Plugins  (4)

- `bool Plugin_IsLoaded(const string &in)`
- `bool Plugin_Load(const string &in)`
- `string Plugin_GetVersion(const string &in)`
- `void Plugin_Unload(const string &in)`

### Prefabs  (4)

- `bool Prefab_IsPrefabInstance(uint64)`
- `uint64 Prefab_Instantiate(const string &in, float, float, float)`
- `uint64 Prefab_InstantiateEx(const string &in, float, float, float, float, float, float, float, float, float)`
- `void Prefab_Unpack(uint64)`

### Procedural generation  (56)

- `float ProceduralGen_GetHeight(int x, int y)`
- `int Fourier_GetActiveTerms(uint64)`
- `int ProceduralGen_GetCell(int x, int y)`
- `int ProceduralGen_GetGridHeight()`
- `int ProceduralGen_GetWidth()`
- `int ProceduralGen_PrefabAssemble(int maxRooms, uint seed)`
- `int RandomBag_Count(uint64 self)`
- `int RandomBag_DrawIndex(uint64 self)`
- `int RandomBag_Remaining(uint64 self)`
- `int Scatter_Clear(uint64 self)`
- `int Scatter_Count(uint64 self)`
- `int Scatter_Generate(uint64 self)`
- `int TerrainGen_Generate(uint64 self)`
- `int WFC_Generate(uint64 self)`
- `string ProceduralGen_Grammar(const string &in rules, const string &in startSymbol, uint seed)`
- `string ProceduralGen_LSystem(const string &in axiom, const string &in rules, uint iterations)`
- `string RandomBag_Draw(uint64 self)`
- `uint CA_GetGeneration(uint64)`
- `uint CA_GetLiveCells(uint64)`
- `uint Physarum_GetStepCount(uint64)`
- `uint RD_GetStepCount(uint64)`
- `void CA_Reset(uint64)`
- `void CA_SetRule(uint64, int)`
- `void CA_SetRunning(uint64, bool)`
- `void CA_SetStampPattern(uint64, const string &in)`
- `void Fourier_SetContour(uint64, int)`
- `void Fourier_SetExtrude(uint64, float)`
- `void Fourier_SetTerms(uint64, int)`
- `void MetaballSurface_SetGridResolution(uint64, int)`
- `void MetaballSurface_SetGridSize(uint64, float)`
- `void MetaballSurface_SetGroup(uint64, int)`
- `void Metaball_SetColor(uint64, float, float, float)`
- `void Metaball_SetGroup(uint64, int)`
- `void Metaball_SetRadius(uint64, float)`
- `void Metaball_SetStrength(uint64, float)`
- `void P4D_SetAnimate(uint64, bool)`
- `void P4D_SetPolytope(uint64, int)`
- `void P4D_SetRotation(uint64, float, float, float, float, float, float)`
- `void P4D_SetScale(uint64, float)`
- `void Physarum_Rebake(uint64)`
- `void Physarum_SetAgentCount(uint64, uint)`
- `void Physarum_SetPreset(uint64, int)`
- `void Physarum_SetSettleSteps(uint64, uint)`
- `void ProceduralGen_BSP(uint width, uint height, uint minRoomSize, uint maxRoomSize, uint seed)`
- `void ProceduralGen_CellularAutomata(uint width, uint height, uint fillPct, uint smoothPasses, uint seed)`
- `void ProceduralGen_DiamondSquare(uint size, float roughness, uint seed)`
- `void ProceduralGen_RandomWalker(uint width, uint height, uint steps, uint seed)`
- `void ProceduralGen_SpawnGrid(float cellSize, int wallValue, int floorValue)`
- `void ProceduralGen_Voronoi(uint width, uint height, uint numPoints, uint seed)`
- `void ProceduralGen_WFC(uint width, uint height, uint tileSetSize, uint seed)`
- `void ProceduralMesh_SetRegenerate(uint64, bool)`
- `void ProceduralTexture_SetRegenerate(uint64, bool)`
- `void RD_Rebake(uint64)`
- `void RD_SetPreset(uint64, int)`
- `void RD_SetSettleSteps(uint64, uint)`
- `void RandomBag_Reset(uint64 self)`

### Rendering  (107)

- `Vector3 PostProcess_GetColorFilter()`
- `Vector3 Render_GetAmbientColor()`
- `Vector3 Render_GetFogColor()`
- `bool PPVolume_IsActive(uint64)`
- `bool PPVolume_IsGlobal(uint64)`
- `bool PostProcess_IsBloomEnabled()`
- `bool PostProcess_IsCausticsEnabled()`
- `bool PostProcess_IsChromaticAberrationEnabled()`
- `bool PostProcess_IsContactShadowsEnabled()`
- `bool PostProcess_IsFXAAEnabled()`
- `bool PostProcess_IsFilmGrainEnabled()`
- `bool PostProcess_IsFogShaftsEnabled()`
- `bool PostProcess_IsGodRaysEnabled()`
- `bool PostProcess_IsSSAOEnabled()`
- `bool PostProcess_IsVignetteEnabled()`
- `bool RenderTarget_BindToEntity(uint64 handle, uint64 entity)`
- `bool Render_IsRainActive()`
- `bool Render_IsShadowsEnabled()`
- `bool Render_IsWireframeEnabled()`
- `float PPVolume_GetBlendRadius(uint64)`
- `float PPVolume_GetWeight(uint64)`
- `float PostProcess_GetBloomIntensity()`
- `float PostProcess_GetBloomThreshold()`
- `float PostProcess_GetBrightness()`
- `float PostProcess_GetCausticsIntensity()`
- `float PostProcess_GetCausticsWaterY()`
- `float PostProcess_GetChromaticAberrationIntensity()`
- `float PostProcess_GetContactShadowsIntensity()`
- `float PostProcess_GetContrast()`
- `float PostProcess_GetExposure()`
- `float PostProcess_GetFilmGrainIntensity()`
- `float PostProcess_GetFogShaftsIntensity()`
- `float PostProcess_GetFogShaftsMaxDistance()`
- `float PostProcess_GetGamma()`
- `float PostProcess_GetGodRaysIntensity()`
- `float PostProcess_GetSSAOIntensity()`
- `float PostProcess_GetSSAORadius()`
- `float PostProcess_GetSaturation()`
- `float PostProcess_GetVignetteIntensity()`
- `float PostProcess_GetVignetteSmoothness()`
- `float Render_GetAmbientIntensity()`
- `float Render_GetFogDensity()`
- `float Render_GetFogEnd()`
- `float Render_GetFogHeightFalloff()`
- `float Render_GetFogStart()`
- `float Render_GetShadowDistance()`
- `float Render_GetShadowStrength()`
- `float Render_GetSnowIntensity()`
- `float Render_GetWorldCurvature()`
- `int PPVolume_GetPriority(uint64)`
- `int PostProcess_GetGodRaysSamples()`
- `int PostProcess_GetToneMapping()`
- `uint64 RenderTarget_Create(int width, int height)`
- `void PPVolume_SetActive(uint64, bool)`
- `void PPVolume_SetBlendRadius(uint64, float)`
- `void PPVolume_SetGlobal(uint64, bool)`
- `void PPVolume_SetPriority(uint64, int)`
- `void PPVolume_SetWeight(uint64, float)`
- `void Particles_OneShot(const string &in preset, float x, float y, float z, int count)`
- `void PostProcess_SetBloomEnabled(bool)`
- `void PostProcess_SetBloomIntensity(float)`
- `void PostProcess_SetBloomThreshold(float)`
- `void PostProcess_SetBrightness(float)`
- `void PostProcess_SetCausticsEnabled(bool)`
- `void PostProcess_SetCausticsIntensity(float)`
- `void PostProcess_SetCausticsWaterY(float)`
- `void PostProcess_SetChromaticAberrationEnabled(bool)`
- `void PostProcess_SetChromaticAberrationIntensity(float)`
- `void PostProcess_SetColorFilter(const Vector3 &in)`
- `void PostProcess_SetContactShadowsEnabled(bool)`
- `void PostProcess_SetContactShadowsIntensity(float)`
- `void PostProcess_SetContrast(float)`
- `void PostProcess_SetExposure(float)`
- `void PostProcess_SetFXAAEnabled(bool)`
- `void PostProcess_SetFilmGrainEnabled(bool)`
- `void PostProcess_SetFilmGrainIntensity(float)`
- `void PostProcess_SetFogShaftsEnabled(bool)`
- `void PostProcess_SetFogShaftsIntensity(float)`
- `void PostProcess_SetFogShaftsMaxDistance(float)`
- `void PostProcess_SetGamma(float)`
- `void PostProcess_SetGodRaysEnabled(bool)`
- `void PostProcess_SetGodRaysIntensity(float)`
- `void PostProcess_SetGodRaysSamples(int)`
- `void PostProcess_SetSSAOEnabled(bool)`
- `void PostProcess_SetSSAOIntensity(float)`
- `void PostProcess_SetSSAORadius(float)`
- `void PostProcess_SetSaturation(float)`
- `void PostProcess_SetToneMapping(int)`
- `void PostProcess_SetVignetteEnabled(bool)`
- `void PostProcess_SetVignetteIntensity(float)`
- `void PostProcess_SetVignetteSmoothness(float)`
- `void RenderTarget_Destroy(uint64 handle)`
- `void RenderTarget_SetCamera(uint64 handle, uint64 cameraEntity)`
- `void Render_SetAmbientColor(const Vector3 &in)`
- `void Render_SetAmbientIntensity(float)`
- `void Render_SetFogColor(const Vector3 &in)`
- `void Render_SetFogDensity(float)`
- `void Render_SetFogEnd(float)`
- `void Render_SetFogHeightFalloff(float)`
- `void Render_SetFogStart(float)`
- `void Render_SetRainActive(bool)`
- `void Render_SetShadowDistance(float)`
- `void Render_SetShadowStrength(float)`
- `void Render_SetShadowsEnabled(bool)`
- `void Render_SetSnowIntensity(float)`
- `void Render_SetWireframeEnabled(bool)`
- `void Render_SetWorldCurvature(float)`

### Rewind and replay  (11)

- `bool Rewind_IsAnyRewinding()`
- `bool Rewind_IsEntityRewinding(uint64)`
- `bool Rewind_IsSceneRewinding()`
- `float Rewind_GetCurrentTime()`
- `float Rewind_GetRecordedDuration()`
- `void Rewind_SeekScene(float)`
- `void Rewind_SetEntityChannels(uint64, uint)`
- `void Rewind_StartEntity(uint64)`
- `void Rewind_StartScene()`
- `void Rewind_StopEntity(uint64)`
- `void Rewind_StopScene()`

### Save system  (28)

- `bool Meta_GetBool(const string &in, bool)`
- `bool SaveData_GetBool(uint64, const string &in, bool)`
- `bool SaveData_Has(uint64)`
- `bool SaveData_HasTag(uint64, const string &in)`
- `bool SaveGame_DeleteSlot(int)`
- `bool SaveGame_FromSlot(int)`
- `bool SaveGame_ToSlot(int)`
- `float Meta_GetFloat(const string &in, float)`
- `float SaveData_GetFloat(uint64, const string &in, float)`
- `int Meta_GetInt(const string &in, int)`
- `int SaveData_GetInt(uint64, const string &in, int)`
- `int SaveData_GetTier(uint64)`
- `string Meta_GetString(const string &in, const string &in)`
- `string SaveData_GetString(uint64, const string &in, const string &in)`
- `void AutoSave_Enable(bool)`
- `void AutoSave_SetInterval(float)`
- `void Meta_Save()`
- `void Meta_SetBool(const string &in, bool)`
- `void Meta_SetFloat(const string &in, float)`
- `void Meta_SetInt(const string &in, int)`
- `void Meta_SetString(const string &in, const string &in)`
- `void SaveData_AddTag(uint64, const string &in)`
- `void SaveData_Set(uint64, int)`
- `void SaveData_SetBool(uint64, const string &in, bool)`
- `void SaveData_SetFloat(uint64, const string &in, float)`
- `void SaveData_SetInt(uint64, const string &in, int)`
- `void SaveData_SetString(uint64, const string &in, const string &in)`
- `void SaveGame_Checkpoint()`

### Scene and save data  (34)

- `Vector3 Entity_GetForward(uint64)`
- `Vector3 Entity_GetPosition(uint64)`
- `Vector3 Entity_GetRight(uint64)`
- `Vector3 Entity_GetRotation(uint64)`
- `Vector3 Entity_GetScale(uint64)`
- `Vector3 Entity_GetUp(uint64)`
- `bool Entity_IsVisible(uint64)`
- `bool Scene_HasTag(uint64, const string &in)`
- `bool Scene_IsValid(uint64)`
- `int Entity_GetChildCount(uint64)`
- `string Entity_GetName(uint64)`
- `string Scene_GetCurrentScene()`
- `string Scene_GetEntityName(uint64)`
- `uint64 Entity_GetChild(uint64, int)`
- `uint64 Entity_GetParent(uint64)`
- `uint64 Scene_FindEntity(const string &in)`
- `uint64 Scene_FindEntityByTag(const string &in)`
- `uint64 Scene_GetEntityCount()`
- `uint64 Scene_Instantiate()`
- `uint64 Scene_InstantiateAt(const Vector3 &in)`
- `uint64 Scene_InstantiateNamed(const string &in)`
- `void Entity_RemoveParent(uint64)`
- `void Entity_SetParent(uint64, uint64)`
- `void Entity_SetPosition(uint64, const Vector3 &in)`
- `void Entity_SetRotation(uint64, const Vector3 &in)`
- `void Entity_SetScale(uint64, const Vector3 &in)`
- `void Entity_SetVisible(uint64, bool)`
- `void Flow_Advance()`
- `void Scene_AddTag(uint64, const string &in)`
- `void Scene_DestroyEntity(uint64)`
- `void Scene_LoadScene(const string &in)`
- `void Scene_RemoveTag(uint64, const string &in)`
- `void Scene_Restart()`
- `void Scene_SetEntityName(uint64, const string &in)`

### ScriptBindings_Physics2D.cpp  (20)

- `Vector2 Physics2D_GetGravity()`
- `Vector2 Physics2D_GetVelocity(uint64)`
- `bool Physics2D_OverlapBox(const Vector2 &in, const Vector2 &in)`
- `bool Physics2D_OverlapBoxMask(const Vector2 &in, const Vector2 &in, uint)`
- `bool Physics2D_OverlapCircle(const Vector2 &in, float)`
- `bool Physics2D_OverlapCircleMask(const Vector2 &in, float, uint)`
- `bool Physics2D_Raycast(const Vector2 &in, const Vector2 &in, float)`
- `bool Physics2D_RaycastMask(const Vector2 &in, const Vector2 &in, float, uint)`
- `int Physics2D_OverlapBoxEntities(const Vector2 &in, const Vector2 &in)`
- `int Physics2D_OverlapBoxEntitiesMask(const Vector2 &in, const Vector2 &in, uint)`
- `int Physics2D_OverlapCircleEntities(const Vector2 &in, float)`
- `int Physics2D_OverlapCircleEntitiesMask(const Vector2 &in, float, uint)`
- `uint64 Physics2D_GetOverlapResult(int)`
- `uint64 Physics2D_RaycastHit(const Vector2 &in, const Vector2 &in, float)`
- `uint64 Physics2D_RaycastHitMask(const Vector2 &in, const Vector2 &in, float, uint)`
- `void Physics2D_AddForce(uint64, const Vector2 &in)`
- `void Physics2D_AddImpulse(uint64, const Vector2 &in)`
- `void Physics2D_SetGravity(const Vector2 &in)`
- `void Physics2D_SetGravityScale(uint64, float)`
- `void Physics2D_SetVelocity(uint64, const Vector2 &in)`

### Sprites (2D)  (16)

- `bool SpriteAnim_IsPlaying(uint64)`
- `float Sprite_GetHeight(uint64)`
- `float Sprite_GetWidth(uint64)`
- `string Sprite_GetTexture(uint64)`
- `uint SpriteAnim_GetCurrentFrame(uint64)`
- `void SpriteAnim_Play(uint64, const string &in)`
- `void SpriteAnim_SetSpeed(uint64, float)`
- `void SpriteAnim_Stop(uint64)`
- `void Sprite_SetAlpha(uint64, float)`
- `void Sprite_SetColor(uint64, float, float, float, float)`
- `void Sprite_SetFlipX(uint64, bool)`
- `void Sprite_SetFlipY(uint64, bool)`
- `void Sprite_SetSize(uint64, float, float)`
- `void Sprite_SetSortOrder(uint64, int)`
- `void Sprite_SetTexture(uint64, const string &in)`
- `void Sprite_SetVisible(uint64, bool)`

### State machines  (20)

- `bool SM_GetBool(uint64, const string&in)`
- `bool SM_HasState(uint64, const string&in)`
- `float SM_GetFloat(uint64, const string&in)`
- `float SM_GetStateTime(uint64)`
- `int SM_GetInt(uint64, const string&in)`
- `string SM_GetCurrentState(uint64)`
- `string SM_GetOnEnter(uint64, const string&in)`
- `string SM_GetOnExit(uint64, const string&in)`
- `string SM_GetOnUpdate(uint64, const string&in)`
- `string SM_GetPreviousState(uint64)`
- `void SM_AddState(uint64, const string&in)`
- `void SM_AddTransition(uint64, const string&in, const string&in)`
- `void SM_SendTrigger(uint64, const string&in)`
- `void SM_SetBool(uint64, const string&in, bool)`
- `void SM_SetFloat(uint64, const string&in, float)`
- `void SM_SetInt(uint64, const string&in, int)`
- `void SM_SetOnEnter(uint64, const string&in, const string&in)`
- `void SM_SetOnExit(uint64, const string&in, const string&in)`
- `void SM_SetOnUpdate(uint64, const string&in, const string&in)`
- `void SM_SetState(uint64, const string&in)`

### Text and fonts  (13)

- `Vector3 Text_MeasureTo(uint64, int)`
- `int Text_Length(uint64)`
- `string Text_GetContent(uint64)`
- `void Text_ClearRuns(uint64)`
- `void Text_RevealTo(uint64, int)`
- `void Text_SetAlignment(uint64, int)`
- `void Text_SetBgColor(uint64, float, float, float)`
- `void Text_SetBgOpacity(uint64, float)`
- `void Text_SetColor(uint64, float, float, float)`
- `void Text_SetContent(uint64, const string &in)`
- `void Text_SetFontSize(uint64, float)`
- `void Text_SetRun(uint64, int, int, float, float, float)`
- `void Text_SetWrapWidth(uint64, float)`

### Tweening  (10)

- `float Tween_GetValue(uint64, uint)`
- `uint Tween_Color(uint64, const Vector3&in, float, int)`
- `uint Tween_Float(uint64, float, float, float, int)`
- `uint Tween_Opacity(uint64, float, float, int)`
- `uint Tween_Position(uint64, const Vector3&in, float, int)`
- `uint Tween_Rotation(uint64, const Vector3&in, float, int)`
- `uint Tween_Scale(uint64, const Vector3&in, float, int)`
- `void Tween_SetDelay(uint64, uint, float)`
- `void Tween_SetOnComplete(uint64, uint, const string&in)`
- `void Tween_StopAll(uint64)`

### UI and dialogue  (40)

- `bool Loc_HasString(const string &in)`
- `bool UI_IsCanvasVisible(uint64)`
- `bool UI_IsChecked(uint64, int)`
- `bool UI_IsElementVisible(uint64, int)`
- `bool UI_IsFocused(uint64, int)`
- `bool UI_IsHovered(uint64, int)`
- `bool UI_IsPressed(uint64, int)`
- `float UI_GetFontSize(uint64, int)`
- `float UI_GetProgress(uint64, int)`
- `float UI_GetSliderValue(uint64, int)`
- `int UI_AddElement(uint64, int, const string &in, int parentId = 0)`
- `int UI_FindElement(uint64, const string &in)`
- `int UI_GetFocusedElement(uint64)`
- `string Loc_Get(const string &in)`
- `string Loc_GetLocale()`
- `string Loc_GetWithFallback(const string &in, const string &in)`
- `string UI_GetText(uint64, int)`
- `void Loc_SetLocale(const string &in)`
- `void UI_ClearCharColors(uint64, int)`
- `void UI_ClearFocus(uint64)`
- `void UI_SetBgColor(uint64, int, float, float, float, float)`
- `void UI_SetCanvasSortOrder(uint64, int)`
- `void UI_SetCanvasVisible(uint64, bool)`
- `void UI_SetCharColor(uint64, int, int, float, float, float)`
- `void UI_SetCharColorRange(uint64, int, int, int, float, float, float)`
- `void UI_SetChecked(uint64, int, bool)`
- `void UI_SetElementAnchor(uint64, int, float, float, float, float)`
- `void UI_SetElementEnabled(uint64, int, bool)`
- `void UI_SetElementOffsets(uint64, int, float, float, float, float)`
- `void UI_SetElementVisible(uint64, int, bool)`
- `void UI_SetFocus(uint64, int)`
- `void UI_SetFocusable(uint64, int, bool)`
- `void UI_SetFontSize(uint64, int, float)`
- `void UI_SetImageAlpha(uint64, int, float)`
- `void UI_SetImagePath(uint64, int, const string &in)`
- `void UI_SetProgress(uint64, int, float)`
- `void UI_SetSliderValue(uint64, int, float)`
- `void UI_SetTabOrder(uint64, int, int)`
- `void UI_SetText(uint64, int, const string &in)`
- `void UI_SetTextColor(uint64, int, float, float, float)`

### Water  (18)

- `bool Water3D_Has(uint64)`
- `float Water3D_GetHeight(float, float)`
- `float Water3D_GetWaveHeight(uint64)`
- `float Water_GetHeight(uint64, float, float)`
- `void Water3D_SetDeepColor(uint64, const Vector3 &in)`
- `void Water3D_SetFoam(uint64, bool, float, float)`
- `void Water3D_SetGerstner(uint64, bool, float)`
- `void Water3D_SetOpacity(uint64, float)`
- `void Water3D_SetReflection(uint64, float, float)`
- `void Water3D_SetShallowColor(uint64, const Vector3 &in)`
- `void Water3D_SetStyle(uint64, int)`
- `void Water3D_SetWaveDirection(uint64, float, float)`
- `void Water3D_SetWaveFrequency(uint64, float)`
- `void Water3D_SetWaveHeight(uint64, float)`
- `void Water3D_SetWaveSpeed(uint64, float)`
- `void Water_Splash(uint64, float, float, float)`
- `void Water_SustainedPressure(uint64, float, float, float, float)`
- `void Water_Wake(uint64, float, float, float, float, float)`

### Weather  (26)

- `bool Weather_IsLightning()`
- `bool Weather_LightningJustFired()`
- `bool WorldTime_GetSeasonalWeather()`
- `bool WorldTime_IsNight()`
- `float Weather_GetFogDensity()`
- `float Weather_GetRainIntensity()`
- `float Weather_GetSnowIntensity()`
- `float WorldTime_GetTimeOfDay()`
- `int Weather_Get()`
- `int WorldTime_GetSeason()`
- `string WorldTime_GetSeasonName()`
- `void Weather_Set(int, float = 2.0)`
- `void Weather_SetFogColor(float, float, float)`
- `void Weather_SetFogDensity(float)`
- `void Weather_SetFogRange(float, float)`
- `void Weather_SetLightningInterval(float, float)`
- `void Weather_SetRainIntensity(float)`
- `void Weather_SetSnowIntensity(float)`
- `void Weather_SetWind(float, float, float, float)`
- `void Wind_SetDirection(float, float, float)`
- `void Wind_SetStrength(float)`
- `void WorldTime_AdvanceSeason()`
- `void WorldTime_SetSeason(int)`
- `void WorldTime_SetSeasonalWeather(bool)`
- `void WorldTime_SetSecondsPerHour(float)`
- `void WorldTime_SetTimeOfDay(float)`

<!-- END GENERATED BINDING INDEX -->
