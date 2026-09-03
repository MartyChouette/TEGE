#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"

namespace Enjin {
namespace ECS { class World; }
namespace InputSystem {

class InputActionMap;

// Bridges the platform touch overlay (Core) to the action system (Engine).
// Core cannot depend on Engine, so Engine injects resolvers here: with a map
// wired, on-screen touch buttons and the move stick press each action's CURRENT
// binding and show its label, so rebinding controls is reflected on touch with
// no extra game code. Pass nullptr to detach (falls back to static key codes).
ENJIN_API void SetTouchActionMap(InputActionMap* map);

// Effective key code a touch control bound to this GameAction id would press:
// the action's current Key binding, a Core-encoded mouse button (-code-1), or
// Input::kTouchNoBinding when the action has no key/mouse binding. Exposed for
// tests and tooling.
ENJIN_API int TouchActionKey(int action);

// Short display label for the action's current binding (e.g. "W", "Space",
// "Mouse1"), or nullptr when unavailable. Exposed for tests and tooling.
ENJIN_API const char* TouchActionLabel(int action);

// ---- Layout presets ----------------------------------------------------------
// A preset is the set of GameActions a controller type consumes. It drives BOTH
// the touch scheme (one button per consumed action whose TouchHint is Button, a
// stick if movement is consumed, a look region if look is consumed) and the
// on-screen controls hint. Ordinals are exposed to scripts (Touch_UsePreset).
enum class TouchPreset : int {
    Platformer2D = 0,   // stick + jump, no look region
    TopDown2D,          // stick + interact, no look region
    TopDown3D,          // stick + look + jump + interact + sprint
    FirstPerson,        // stick + look + fire + jump + sprint + interact
    ThirdPerson,        // stick + look + jump + interact + sprint
    Generic,            // no controller: free camera (move + look + sprint)
    Count
};

// Actions a preset consumes, in hint order. `lookRegion` reports whether the
// preset drags the camera on the right side of the screen.
ENJIN_API int PresetActionCount(TouchPreset preset);
ENJIN_API int PresetAction(TouchPreset preset, int index);   // GameAction ordinal
ENJIN_API bool PresetHasLook(TouchPreset preset);

// Build and install the Core touch scheme for a preset from the ActionInfo
// table. Works on every platform (desktop/editor simulate the overlay).
ENJIN_API void ApplyTouchPreset(TouchPreset preset);
ENJIN_API TouchPreset GetActiveTouchPreset();

// Project-authored input settings (custom touch layout + touch accessibility
// defaults such as left-handed mirroring and button scale). Set once when the
// project/manifest loads; null restores engine defaults. Pointer is borrowed
// and must outlive the bridge's use of it.
struct InputProjectSettings;
ENJIN_API void SetTouchProjectSettings(const InputProjectSettings* settings);

// Preset for whatever player controller the world contains (Generic if none).
ENJIN_API TouchPreset TouchPresetForWorld(ECS::World* world);

// Build the scheme for a world: the controller preset's buttons PLUS a button
// for every ActionTriggerComponent in the scene that asks for one, then any
// project overrides. Rebuilt only when that set actually changes, so a game
// that adds its own buttons mid-scene is not clobbered, while dropping an
// ActionTrigger component into a scene makes its touch button appear at once.
// Call once per frame BEFORE scripts run. Returns true when it rebuilt.
// SetTouchActionMap(nullptr) resets tracking.
ENJIN_API bool ApplyTouchPresetForWorld(ECS::World* world);
ENJIN_API void ResetTouchPresetTracking();

// ---- Drawing (ImGui foreground draw list; call inside an ImGui frame) --------
// The on-screen stick + buttons. No-op when Input::GetTouchOverlay is inactive.
ENJIN_API void DrawTouchOverlay();

// Bottom-left one-line hint listing the active preset's actions with their
// LIVE bindings ("W/A/S/D move · Space jump · E interact"). Hidden while the
// touch overlay is active (its buttons already show the labels) and when no
// action map is wired. The rect is the game surface in ImGui coordinates.
ENJIN_API void DrawControlsHint(f32 x0, f32 y0, f32 w, f32 h);

} // namespace InputSystem
} // namespace Enjin
