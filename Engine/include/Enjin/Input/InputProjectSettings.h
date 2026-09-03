#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include <string>
#include <vector>

namespace Enjin {
namespace InputSystem {

class InputActionMap;

// Project-authored input configuration. This is what the editor's
// Project Settings > Input & Touch tab writes into `.enjinproject`, what
// BuildPipeline copies verbatim into the game manifest, and what all three
// runtimes load at boot. Everything here is reachable from the editor UI: a
// game never needs a script to name an action, bind it, or place a touch button.
//
// Player rebinds (bindings.json) are applied AFTER this, so a project sets the
// defaults and the player still owns their own controls.

// One game-defined action occupying a GameAction::Custom0..7 slot.
struct CustomActionDef {
    i32 slot = 0;              // 0-7 -> GameAction::Custom0 + slot
    std::string name;          // "SLO-MO"; empty leaves the slot hidden
    i32 key = -1;              // KeyCode, or -1
    i32 mouse = -1;            // MouseButton, or -1
    i32 gamepad = -1;          // GamepadButton, or -1
    u32 mode = 2;              // ActionMode ordinal (2 = Press)
};

// One on-screen touch button in a hand-authored layout.
struct TouchButtonLayout {
    i32 action = -1;           // GameAction ordinal
    f32 col = 0.0f;            // grid column inward from the cluster edge
    f32 row = 0.0f;            // grid row upward from the bottom
    f32 size = 0.075f;         // radius as a fraction of safe-area height
};

// Whether a scheme's look-drag region is decided by the controller or forced.
enum class TouchLookMode : u32 { Auto = 0, AlwaysOn, AlwaysOff };

struct InputProjectSettings {
    // Custom actions (naming + default bindings)
    std::vector<CustomActionDef> customActions;

    // Touch layout. Auto (the default) derives buttons from the scene's
    // controller and its ActionTrigger components. Custom uses `touchButtons`
    // verbatim, for a game that wants its own arrangement.
    bool customTouchLayout = false;
    std::vector<TouchButtonLayout> touchButtons;
    bool touchStick = true;
    TouchLookMode touchLook = TouchLookMode::Auto;

    // Touch accessibility defaults (a player can still override at runtime).
    f32 touchButtonScale = 1.0f;   // 0.5 - 2.0
    bool touchLeftHanded = false;  // mirror: stick right, buttons bottom-left

    bool IsEmpty() const {
        return customActions.empty() && !customTouchLayout && touchStick &&
               touchLook == TouchLookMode::Auto && touchButtonScale == 1.0f &&
               !touchLeftHanded;
    }

    // Name and bind the custom actions on a map. Called at boot, before any
    // player bindings.json is loaded.
    void ApplyTo(InputActionMap& map) const;

    // Round-trip as the `input` object inside `.enjinproject` / the game manifest.
    std::string ToJson() const;
    bool FromJson(const std::string& jsonStr);
};

} // namespace InputSystem
} // namespace Enjin
