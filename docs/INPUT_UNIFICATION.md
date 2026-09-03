# Input, Menu, Touch and Accessibility Unification

Investigation date: 2026-09-03. Scope: every layer that decides what a button does or what is on screen to press it, across desktop Player, web Player and editor Play mode.

Goal: one source of truth for controls so that touch, gamepad, keyboard, menus and accessibility settings all derive from it automatically, and a game gets correct mobile parity without writing touch code.

## 1. What exists today

### 1.1 Six places that decide what a button does

| Layer | Where | Decides |
|---|---|---|
| InputActionMap | `Engine/src/Input/InputActionMap.cpp` | GameAction to key/mouse/gamepad bindings, hold/toggle mode, sensitivity. Persisted to `bindings.json` by both players. Never persisted by the editor. |
| Controller raw reads | `Engine/src/ECS/Systems/ControllerSystem.cpp` | Movement, jump, sprint, crouch, dash go through the map when one is attached. Doors (`E`, line 1130), swim rise (`Space`, 1222 and 1425) and mouse-look capture (1289, 1537, 2144) read hardcoded keys. |
| DialogueSystem | `Engine/src/ECS/Systems/DialogueSystem.cpp:274-378` | Advance, choice up/down, confirm. All hardcoded Space/Enter/W/S/Up/Down/LMB. Ignores the map and gamepad entirely. |
| UISystem focus nav | `Engine/src/GUI/UISystem.cpp:2327+` | Tab/arrows/Enter/Space and ImGui gamepad keys, hardcoded to ImGuiKey. Not driven by the map. |
| Touch scheme | `Core/src/Platform/Input.cpp:919-969` | Which on-screen buttons exist, chosen by controller component type (`web_main.cpp:829-850`). GameAction ids duplicated as bare ints `A_FWD=0 .. A_ATTACK=9` because Core cannot see the enum. |
| Ad-hoc web buttons | `Player/src/web_main.cpp:1232-1271`, `922-928` | Pause button, SLO-MO button, gamepad Y. Hard-bound to Escape and `B`. Outside the scheme, outside the map, web only. |

### 1.2 Accessibility input settings are stored in up to four places each

| Setting | Copies | Which one the game actually reads |
|---|---|---|
| Mouse sensitivity | `RuntimeAccessibilitySettings`, `EditorSettings`, `InputActionMap` Look actions, `FirstPersonController::mouseSensitivity` per entity | Only the per-entity controller field (`ControllerSystem.cpp:1541`). The Controls menu slider writes the map, so it is a silent no-op in every exported game. |
| Sprint / crouch toggle | `RuntimeAccessibilitySettings`, `EditorSettings`, `InputActionMap` action mode | Only the map. Player and web load and save the accessibility copy but never apply it (`main.cpp:2971-3031`, `web_main.cpp:1647-1686`). |
| Invert Y | `RuntimeAccessibilitySettings`, per-controller `invertY`, `HeadTrackingConfig::invertY` | Runtime copy XOR controller. Editor hardcodes it false (`EditorLayer.cpp:5416`). No menu exposes it. |
| Dwell click | `RuntimeAccessibilitySettings`, `EditorSettings`, `UISystem`, `EyeTrackingConfig` | Three independent timers. |
| Switch access | `RuntimeAccessibilitySettings`, `UISystem`, `SwitchAccessConfig` | Two scanners with different flags and speeds can run at once. |

The editor's reverse sync (`EditorLayer.cpp:320-364`) drops eleven fields, so toggling switch access in the in-editor pause menu is reverted on the next settings edit.

BuildPipeline writes a hardcoded 15-key `accessibility.json` (`BuildPipeline.cpp:705-722`). Nothing configured in the editor ships.

### 1.3 The touch overlay

- Web only. `GetTouchOverlay` is `#if ENJIN_PLATFORM_WEB`, so the editor and desktop Player can never preview it.
- No project or scene data describes touch controls. Layout comes from a controller-type list in `web_main.cpp` that already drifts from a second controller list in the same file.
- `lookRegion` is written by presets and scripts but never read by the touch callback (`Input.cpp:387-393`). 2D presets still drag the camera.
- `Touch_AddButton` reuses a stale slot. After any preset, slot 0 still carries the Jump action, so a custom button presses Jump and shows Jump's label (`ScriptBindings_Input.cpp:159-160`).
- Key resolver and label resolver disagree for gamepad-only actions: label shows the gamepad glyph, button presses the static fallback key.
- Touch never reaches UI as a pointer drag. Only a short tap becomes a click (`Input.cpp:411-419`). Sliders and scroll areas are unusable by touch. Any canvas element under the stick zone or an action button is unclickable, because Core assigns roles by geometry before the game sees the touch.
- Safe-area insets are read once at init. Rotation and fullscreen (which the code itself triggers) never re-query.
- No touch accessibility: no button scale setting, no left-handed mirror, no haptics.

### 1.4 UI layers and input capture

Everything is ImGui-backed. UISystem draws to the foreground draw list, so it paints above every ImGui window and never sets `WantCaptureMouse`. Neither player reads `WantCaptureMouse`. `UISystem::ProcessInput` runs per canvas with no consumed flag, so overlapping canvases both receive the same click.

Gameplay suppression is one boolean per runtime:

- Desktop: `if (menuOpen || paused || !started) return;` (`main.cpp:983`). Console is not in that gate, so typing `wasd` into the console walks the player.
- Web: `if (paused || atMainMenu) return;` (`web_main.cpp:710`). The pause and SLO-MO ImGui buttons double-fire into gameplay and into any canvas beneath.
- Editor: `PlayMode::Pause()` disables systems.

The pause menu exists four times: UITemplates canvas (desktop, web), a bespoke draw-list menu in the docked editor game view (`EditorLayerRendering.cpp:918-951`), and GameMenus ImGui in editor focus mode. The editor user tests a different menu from the one that ships.

`fontScale` reaches only UISystem. Subtitles, announcer, GameMenus, dialogue overlay, console, splash and touch overlay are fixed-size.

Dead code with no runtime caller: `InventoryUI`, `ScreenTransition`, `MinimapRenderer`.

## 2. Target design

One rule: **GameAction is the spine. Everything else is a view of it.**

### 2.1 Action descriptor table (Engine)

Add a static table next to the enum, one row per GameAction:

```
struct ActionInfo {
    const char* name;        // "Jump"
    ActionCategory category; // Movement, Action, Camera, UI, Custom
    KeyCode defaultKeys[2];
    GamepadButton defaultPad;
    TouchHint touch;         // NotShown | Button | Stick | Look
    const char* touchLabel;  // "JMP"
};
```

Add these actions:

- `UIConfirm`, `UICancel`, `UINavUp`, `UINavDown`, `UINavLeft`, `UINavRight` (menus, dialogue, focus nav)
- `DialogueAdvance` (defaults to UIConfirm's bindings)
- `Custom0..Custom7`: game-named slots. Playground names Custom0 "SLO-MO", binds `B` and gamepad Y. This replaces the three ad-hoc bullet-time entry points with one row.

`LoadDefaults` becomes a loop over the table. `GetActionName`, `GetActionCategory`, display names and the touch label all read the table.

### 2.2 Touch derives from actions, not from controller types

Move preset construction out of Core into Engine (`TouchActionBridge`). Core keeps only geometry, hit-testing and drawing state. Core receives resolved slots: `{action id, key, label, radius, col, row}`. The bare int constants in `Input.cpp:935` are deleted, which removes the enum-drift risk.

Preset selection stays automatic but becomes a function of which actions the scene consumes. Each controller type declares the actions it uses (a small table in ControllerSystem). The touch scheme is "one button per consumed action whose `touch` hint is Button, plus stick if a Move action is consumed, plus look region if a Look action is consumed." `lookRegion` then means something and is actually honored.

`Pause` is always a scheme button on touch. That removes the `##pausebtn` window.

### 2.3 Touch controls are project data

Add a `touchControls` block to `.enjinproject`:

```
"touchControls": {
  "mode": "auto" | "custom",
  "buttons": [ { "action": "Jump", "col": 0, "row": 0, "size": 0.085 }, ... ],
  "stick": true,
  "look": "auto",
  "handedness": "right"
}
```

- Editor: Project Settings gets a Touch Controls tab with a live preview drawn over the game view.
- BuildPipeline carries the block verbatim into the manifest, like `startupFlow`.
- Web player loads it. Scripts can still override at runtime with the existing `Touch_*` bindings, which now take action names.

### 2.4 Touch overlay compiles on every platform

Remove the `#if ENJIN_PLATFORM_WEB` around scheme, geometry and overlay state. Only the browser event source stays web-only. Add a "Simulate touch" toggle in editor Play mode and a `--touch` flag on the desktop Player. The mouse acts as a single touch. This is what makes the layout testable without a phone and gives the editor real parity.

### 2.5 Touch is a pointer first, a key second

Reorder the touchstart role assignment:

1. Ask Engine "is this point over an interactive UI element?" through an injected callback (same pattern as the action resolvers). If yes, the touch is a pointer: down, move, up all reach UISystem. Sliders and drags work.
2. Otherwise scheme button.
3. Otherwise stick or look.

UISystem gets one `ProcessInput` per frame across all canvases, sorted by `sortOrder`, that stops at the first hit and reports `consumed`.

### 2.6 One input focus, one consumed flag

Add `Input::SetUIConsumedPointer(bool)` set once per frame from UISystem's result plus `io.WantCaptureMouse`. Controllers check it where they already check `IsMouseCaptured`. Add an `InputFocus` enum {Gameplay, Menu, Dialogue, Console} owned by one object per runtime. Gameplay reads are gated on Gameplay. This fixes the console walk bug, the web double-fire, and the overlapping-canvas double click in one mechanism.

### 2.7 Accessibility input settings live in InputActionMap only

Delete `sprintMode`, `crouchMode`, `mouseSensitivity`, `invertMouseY` from `RuntimeAccessibilitySettings` and `EditorSettings`. `InputActionMap` gains `invertY`, `touchButtonScale`, `touchLeftHanded`, `touchHoldToToggle`. Camera look reads `map.GetMouseSensitivity() * ctrl.mouseSensitivity`, where the per-entity value becomes a designer multiplier with default 1. Both menus (Controls tab, Accessibility tab) edit the same object. The editor persists its map like the players do. Dwell click and switch access collapse to the UISystem implementation, with `AlternativeInputManager` feeding it rather than running its own scan.

### 2.8 Menus and dialogue read actions

- DialogueSystem uses `DialogueAdvance`, `UINavUp/Down`, `UIConfirm`.
- UISystem focus nav is driven by the UI actions. Feed ImGui nav from the same actions (`io.AddKeyEvent(ImGuiKey_GamepadFaceDown, ...)` when `UIConfirm` is down) so GameMenus follows rebinds too.
- Remaining raw reads in ControllerSystem (doors, swim) go through `Interact` and `Jump`.
- Player splash/skip reads `UIConfirm` or `UICancel`.

### 2.9 One pause menu, one accessibility export

- The editor docked pause menu is replaced by the same UITemplates canvas the players spawn.
- BuildPipeline writes `accessibility.json` and `bindings.json` from project settings instead of literal defaults. Accessibility defaults become project-level (a new `accessibilityDefaults` block in `.enjinproject`), not editor preferences.
- `fontScale` is applied to subtitles, announcer, GameMenus and the touch overlay labels.

## 3. Phases

### Phase 0: bug fixes, no design change (DONE 2026-09-03)

- Honor `lookRegion` in `WebTouchCallback`.
- Reset the whole `TouchButtonDef` in `Touch_AddButton` / `Touch_AddActionButton` / `Touch_ClearButtons`.
- Make `TouchActionLabel` return nullptr when `TouchActionKey` returns `kTouchNoBinding`, so label and behavior agree.
- Player and web: apply sprint/crouch/sensitivity from `bindings.json` (they already load it); stop loading them from `accessibility.json`.
- Camera look multiplies by `map.GetMouseSensitivity()` so the existing slider works.
- Add console to the desktop gameplay gate.
- Re-query safe-area insets on resize and fullscreenchange.
- Stick radius uses safe-area height like buttons.
- `static_assert` or a desktop-compiled test tying the Core action ints to the enum until Phase 1 deletes them.

### Phase 1: action table and Custom actions (DONE 2026-09-03)

Shipped: `kActionInfo` table in InputActionMap.cpp, UI + DialogueAdvance + Custom0..7 actions, presets built in Engine (TouchActionBridge) from consumed-action lists, overlay compiled on all platforms (`EnjinPlayer --touch`, editor View > Simulate Touch Controls), the bottom-left controls hint drawn by the engine from the active preset and live bindings (was a static div in the web shell), Playground SLO-MO as Custom0. Dropped the TopDown2D "F" button (no action, could never be relabelled or rebound). Still ad-hoc: the web pause button, which waits on Phase 3 pointer routing.

Original scope:

Descriptor table, UI actions, Custom0..7, preset construction moved to Engine, Playground's SLO-MO becomes Custom0. Touch overlay compiles on all platforms with editor simulate toggle.

### Phase 2: project data + components (DONE 2026-09-03)

Reshaped after Marty's note that everything must be reachable from the editor by a user with components only, no scripts and no agent. Shipped:

- `InputProjectSettings` (`input` block in `.enjinproject`): custom action names + bindings, touch layout, and touch accessibility (left-handed mirror, button scale, camera-drag override). BuildPipeline carries it verbatim into the game manifest; desktop, web and editor Play all load it before the player's `bindings.json`.
- Project Settings > Input & Touch tab: name your own actions and bind them from pick-lists, set the touch accessibility defaults, and optionally hand-author the button layout.
- `ActionTriggerComponent` + `ActionTriggerSystem`: pick an action, pick an effect (slow time, show/hide an entity, send an event, show a subtitle). Runs in all three runtimes; `Reset()` on stop so bullet time cannot strand the editor clock.
- A trigger contributes its own touch button, and the scheme is rebuilt on a fingerprint of (preset + scene triggers + project settings), so dropping the component in makes the mobile button appear at once.
- Left-handed touch mirroring and button scale in Core, the first touch accessibility settings the engine has had.

Still script-only: nothing required. The `Touch_*` and `InputAction_*` bindings remain as the optional path.

Original scope:

`.enjinproject` block, editor tab with preview, BuildPipeline carry, web load. Script `Touch_*` accept action names.

### Phase 3: pointer routing and focus

UI-hit callback into Core, one consumed flag, `InputFocus`, single-pass UISystem input with sortOrder priority.

### Phase 4: accessibility collapse and menu unification

Fields deleted from accessibility structs, editor persists bindings, dwell/switch single implementation, DialogueSystem and UISystem on actions, editor pause menu replaced, BuildPipeline exports project defaults, fontScale everywhere.

## 4. What this buys

- A game author never writes touch code. Attach a controller, the right buttons appear, labelled with the live bindings, on every runtime.
- Rebinding a key updates keyboard, gamepad glyphs, touch labels, dialogue advance and menu confirm at once.
- A menu open anywhere stops gameplay input everywhere, by one mechanism.
- Accessibility settings have one home, and what the editor configures is what ships.
- The editor shows the same pause menu and the same touch layout as the exported game.
