#include "Enjin/Input/TouchActionBridge.h"
#include "Enjin/Input/InputAction.h"
#include "Enjin/Platform/Input.h"

namespace Enjin {
namespace InputSystem {

namespace {
    InputActionMap* s_TouchMap = nullptr;
}

// Core cannot see GameAction, so Input::SetTouchControllerPreset (Core
// Input.cpp) spells the action ids it needs as bare ints. Pin them here so an
// enum insertion breaks the build instead of silently mis-binding every preset.
static_assert(static_cast<int>(GameAction::MoveForward) == 0, "Core touch preset assumes MoveForward == 0");
static_assert(static_cast<int>(GameAction::MoveBack)    == 1, "Core touch preset assumes MoveBack == 1");
static_assert(static_cast<int>(GameAction::MoveLeft)    == 2, "Core touch preset assumes MoveLeft == 2");
static_assert(static_cast<int>(GameAction::MoveRight)   == 3, "Core touch preset assumes MoveRight == 3");
static_assert(static_cast<int>(GameAction::Jump)        == 4, "Core touch preset assumes Jump == 4");
static_assert(static_cast<int>(GameAction::Sprint)      == 5, "Core touch preset assumes Sprint == 5");
static_assert(static_cast<int>(GameAction::Interact)    == 8, "Core touch preset assumes Interact == 8");
static_assert(static_cast<int>(GameAction::Attack)      == 9, "Core touch preset assumes Attack == 9");

int TouchActionKey(int action) {
    if (!s_TouchMap || action < 0 || action >= static_cast<int>(GameAction::Count))
        return Input::kTouchNoBinding;
    const ActionConfig& cfg = s_TouchMap->GetActionConfig(static_cast<GameAction>(action));
    for (const auto& b : cfg.bindings) {
        if (b.type == BindingType::Key) return b.code;
        // Core encodes a mouse button as a negative key: button i -> -(i)-1.
        if (b.type == BindingType::MouseButton) return -(b.code) - 1;
    }
    // No key/mouse binding (e.g. gamepad-only): let the touch button fall back
    // to its static key code rather than press nothing.
    return Input::kTouchNoBinding;
}

const char* TouchActionLabel(int action) {
    if (!s_TouchMap || action < 0 || action >= static_cast<int>(GameAction::Count))
        return nullptr;
    // Keep the label honest: when the key resolver has nothing (gamepad-only
    // action), the button presses its static fallback key, so don't show the
    // gamepad glyph GetBindingDisplayName would fall through to.
    if (TouchActionKey(action) == Input::kTouchNoBinding) return nullptr;
    return s_TouchMap->GetBindingDisplayName(action);
}

void SetTouchActionMap(InputActionMap* map) {
    s_TouchMap = map;
    // Function pointers match Input::ActionKeyResolver / ActionLabelResolver.
    Input::SetActionKeyResolver(map ? &TouchActionKey : nullptr);
    Input::SetActionLabelResolver(map ? &TouchActionLabel : nullptr);
}

} // namespace InputSystem
} // namespace Enjin
