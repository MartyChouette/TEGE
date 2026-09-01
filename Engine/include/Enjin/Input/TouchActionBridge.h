#pragma once

#include "Enjin/Platform/Platform.h"

namespace Enjin {
namespace InputSystem {

class InputActionMap;

// Bridges the platform touch overlay (Core) to the action-binding map (Engine).
// Core cannot depend on Engine, so Engine injects resolvers here: with a map
// wired, on-screen touch buttons and the move stick press each action's CURRENT
// binding and show its label, so rebinding controls is reflected on touch with
// no extra game code. Pass nullptr to detach (falls back to static key codes).
void SetTouchActionMap(InputActionMap* map);

// Effective key code a touch control bound to this GameAction id would press:
// the action's current Key binding, a Core-encoded mouse button (-code-1), or
// Input::kTouchNoBinding when the action has no key/mouse binding. Exposed for
// tests and tooling.
int TouchActionKey(int action);

// Short display label for the action's current binding (e.g. "W", "Space",
// "Mouse1"), or nullptr when unavailable. Exposed for tests and tooling.
const char* TouchActionLabel(int action);

} // namespace InputSystem
} // namespace Enjin
