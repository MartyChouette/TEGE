# Engine facts this folder is allowed to assert

Everything in `product/` is written for people who will try it. A sentence here
that is wrong becomes a person stuck looking for a menu that does not exist. So
the rule for this folder is narrow:

> **No claim about the engine goes into a manual, a diagram or a prototype
> unless it is verified in this file, with the source it was read from.**

Verified 2026-09-17 by reading the sources named. Re-verify before a release.
This engine moves fast, and a stale manual is worse than no manual.

---

## Creative Mode: the first-timer's entry point

`Engine/include/Enjin/Editor/CreativeMode.h`

The design verb, quoted from the header, is what the whole first-timer journey is
built to honour:

> someone should be able to block out a playable level and press play without
> ever opening a settings window.

Thirteen tools in `enum class BuildTool`, grouped on the rail in three bands:

| Band | Tools |
|---|---|
| Structure | Wall, Floor, Stairs, Path |
| Volume | Brush, Water, Terrain, Cave, Plants |
| Object | Ladder, Prop, Reduce |
| Its own group | Edit, the only one that changes what is already there instead of making something |

Two details worth carrying into the manual because they are unusual:

- **The geometry decision is pure.** What a gesture makes lives in `BuildBrushes`:
  a drag plus some numbers in, a brush list out. No viewport, no ImGui, no GPU.
  That is why the build tools can be tested at all.
- **Most tools are press-drag-release. `Path` is not**, and the header flags it as
  the first tool whose gesture differs. Anything written about "the gesture" has
  to say which gesture.

## Menus and pause menus

`Engine/include/Enjin/GUI/GameMenus.h`

`enum class MenuScreen`: MainMenu, PauseMenu, Options, Graphics, Audio, Controls,
HowToPlay, GameOver, LoadGame.

Save slots arrive through an injected `SaveSlotProvider`. The header carries its
own scar, and the manual should repeat the lesson rather than the incident: **an
empty slot list must render as "no saves", never as a blank panel**, because a
blank panel cannot be told apart from a screen that failed to draw.

## Startup flow

`Engine/src/Scene/SceneManager.cpp:50, 128, 327`

`startupFlow` in the `.enjinproject` is an ordered list, and it is deliberately
not seeded. An empty flow means go straight to the game. Nobody is forced through
a title screen they did not ask for.

## Shipping to a browser

`Engine/src/Editor/EditorLayerMenuBar.cpp:330`, `Engine/include/Enjin/Build/BuildReport.h`

HTML5 export is not a separate command. It is **Build Game, then Platform: Web**.
`BuildTargetPlatform` is `{ Desktop, Web }`; `PackagingMode` is
`{ Packed, PackedOpen, LooseFiles }`.

This matters for the golden rule: shipping to a browser is reachable from the
editor's own menus, with no command line.

## Touch controls

From `CLAUDE.md` (Input / Touch), cross-checked against the presets in
`TouchActionBridge`:

- Presets are lists of consumed actions per controller type, and they drive both
  the on-screen buttons and the bottom-left controls hint, so a scene only shows
  controls it actually has.
- The scheme is rebuilt on a **fingerprint**: controller preset, plus every
  `ActionTriggerComponent` button in the scene, plus project overrides. Dropping a
  component into a scene makes its touch button appear immediately.
- `EnjinPlayer --touch` and **View, then Simulate Touch Controls** turn the mouse
  into one finger, on every platform.

## Alternative input

`Engine/include/Enjin/Accessibility/AlternativeInput.h`

`enum class AlternativeInputType`: SwitchAccess, EyeTracking, SipAndPuff,
HeadTracking, VoiceControl. Each has a real config struct:

- **SwitchAccess**: scanning mode, scan speed 0.5 to 5.0 seconds per element,
  start delay, 1 to 4 switches, reverse on wrap, and per-switch key mappings.
- **EyeTracking**: dwell time 0.3 to 3.0 seconds, smoothing, a dead zone in
  pixels, and a visible gaze indicator.
- **SipAndPuff**: soft and hard thresholds in both directions, so sip, hard sip,
  puff and hard puff are four distinct signals.

A `ScanTarget` carries a label, a group for hierarchical scanning, screen bounds
and an `onActivate` callback.

## The quick-access dial

`Engine/src/Editor/EditorLayerGamepad.cpp:129-163`, `EditorLayer.h:1274-1285`

Four radial menus, each on its own button, hold to open and release to select:

| Button | Dial | Items |
|---|---|---|
| RB | Tools | Translate, Rotate, Scale, Toggle Space, Focus Selection, Toggle Grid |
| LB | File | Save, Undo, Redo, Duplicate, Delete, Command Palette |
| Start | Play | Play/Toggle, Pause, Stop |
| Y | Create | Empty, Cube, Light, Camera, Sprite |

Aimed with the right stick. Below 0.3 magnitude nothing is selected.

## Fixing a texture without leaving the engine

`Engine/include/Enjin/Editor/PixelEditor.h`

A real raster editor inside the editor: layers with visibility and opacity, named
palettes, retro resolution presets, and the tools Pencil, Eraser, Fill, Line,
Rect, Ellipse, Eyedropper, Select and PolygonEdit.

Neighbouring surfaces for the same "fix it here" story, confirmed present as
headers: `AnimationGraphEditor.h`, `VectorDrawingEditor.h`, `ShaderGraph.h`,
`SpriteSheetImporter.h`, `SpriteColliderGenerator.h`, `TerrainBrush.h`.

## Wind

`Engine/include/Enjin/Effects/Wind.h` declares `class ENJIN_API WindSystem`. Cloth and
vegetation read it, `WeatherZone` can override it, and a strength of 0 is a calm
indoor pocket.

---

# Gaps this folder found

Writing a manual is a use of the product, so it finds things. These are not
documentation problems. They are engine problems that documentation uncovered,
and they belong in `_docs_internal/BACKLOG.md` rather than being smoothed over in
prose.

### G1. There is no hover or glide on the character controller

`Engine/include/Enjin/ECS/Components/Controllers/CharacterController.h:90-97` has
`airControl` and `coyoteTime`. It has no hover, no glide, no float, no descent
clamp and no double jump.

There is also no scripted way back in. `ScriptBindings_Components.cpp` registers
nine `Controller_` bindings, and velocity is read-only among them:

```
Controller_GetVelocity          Controller_SetEnabled
Controller_GetCameraYaw         Controller_SetCameraYaw
Controller_GetIgnoreTimeScale   Controller_SetIgnoreTimeScale
                                Controller_SetMouseLook
                                Controller_SetMoveSpeed
                                Controller_SetThirdPersonCamera
```

No `Controller_SetVelocity`, no jump control, no per-entity gravity control. So a
hover cannot be authored in the inspector, and it cannot be scripted onto a
character controller either. The only route left is to stop using
`CharacterController` and drive a rigidbody yourself with `Physics_SetVelocity`
and `Physics_SetGravityScale`, which means giving up ground detection, coyote
time, slopes and step-up to buy one mechanic.

**The engine question**, the one the traps section of `CLAUDE.md` says always gets
skipped: should a descent clamp and a hover budget be fields on
`CharacterController`, sitting next to `coyoteTime`, which is exactly the same
kind of forgiveness parameter?

### G2. The quick-access dial is reachable by exactly one input device

The dial is the right affordance for someone driving the editor on a sip-and-puff
switch or a gaze tracker: few targets, large, always in the same place, no travel
across the screen.

It opens on `Input::IsGamepadButtonPressed`, aims on `GetGamepadRightStick`, and
`EditorLayer.cpp:5908` only draws it. There is no keyboard path, no pointer path,
and nothing routes `AlternativeInputManager` into it.

Worse than absent: **hold-aim-release is the hardest gesture shape there is for a
binary switch.** It needs a sustained hold and an analogue aim at the same time,
which is the one combination a single switch cannot produce. So the feature that
suits this person best is both unreachable and, in its current gesture, unsuited.

### G3. Two creative systems overlap, and the hidden one had the plants

Already known and already written down in `CLAUDE.md`, restated because the
first-timer journey walks straight into it. The rail is the one people find, and
**View, then Build Palette (Creative)** is off by default. Vegetation used to live
only in the hidden one. `Plants` is on the rail now, which closes the worst of it,
but two systems that make overlapping things is still one more than a first-timer
should ever have to notice.

### G4. Directional gravity zones do nothing to a character controller

This is the sharpest one, and the manual found it by looking for a
components-only way to build a hover pocket.

`GravityZoneComponent` (`Engine/include/Enjin/ECS/Components/GravityZone.h`) has
two modes. `Directional = 0` is the default, and it is the obvious one for a
floaty pocket over a gap: point gravity down, turn the strength to 2.0, drop the
box over the hole.

`JoltBackend::ApplyGravityZones` (`Engine/src/Physics/JoltBackend.cpp:982`) reads
the zone without filtering by mode, so it works correctly on rigidbodies.

`ControllerSystem` does not. `GravityZoneMode::Directional` never appears in
`Engine/src/ECS/Systems/ControllerSystem.cpp`. All three reads of the component
sit in `UpdateSurfaceAligned`, the planetary controller, and every one of them
skips the zone unless it is Point mode:

```cpp
if (!gz || !gz->isActive || gz->mode != GravityZoneMode::Point) continue;
```

So the default configuration of the component is the configuration that does
nothing, and it does nothing specifically for the controller every platformer
uses. The zone has an inspector, it serializes, it reports `isActive`, and the
player walks through it at normal gravity without a single warning anywhere.

Both failure shapes this repo has already named are present at once:

- **A setter storing a value nothing reads.** The silent stub audit swept for
  exactly this shape and this one survived, because the value IS read, just by a
  different system than the one the author was aiming at.
- **A believable wrong default.** `Directional` is the mode a person picks on
  purpose. It is not an absurd value that announces itself, it is the reasonable
  one, which is why nobody catches it.

Apply the two questions from the traps section of `CLAUDE.md`:

1. **Would a second project hit this?** Yes, immediately. Every platformer that
   reaches for a low-gravity zone hits it. This is an engine bug and there is no
   version of it that is a project's fault.
2. **Should the engine have prevented, defaulted or warned?** Yes. The cheap fix
   is a one-time warning when a Directional zone overlaps an entity with a
   character controller, which turns a silent afternoon into a log line. The real
   fix is for `ControllerSystem` to honour Directional zones.
