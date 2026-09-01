#include "Enjin/Input/TouchActionBridge.h"
#include "Enjin/Input/InputAction.h"
#include "Enjin/Platform/Input.h"

namespace Enjin {
namespace InputSystem {

namespace {
    InputActionMap* s_TouchMap = nullptr;
}

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
