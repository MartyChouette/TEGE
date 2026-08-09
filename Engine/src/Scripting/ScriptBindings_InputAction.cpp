#include "Enjin/Scripting/ScriptBindings.h"
#include "Enjin/Scripting/ASCallConv.h"
#include "Enjin/Input/InputAction.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Platform/Platform.h"
#include "Enjin/Logging/Log.h"
#include <angelscript.h>
#include <cassert>
#include <string>

using namespace Enjin;

#define AS_CHECK(expr) \
    do { int _r = (expr); if (_r < 0) { ENJIN_LOG_ERROR(Script, "AS registration failed (code %d) at %s:%d", _r, __FILE__, __LINE__); } } while(0)

static InputSystem::InputActionMap* s_BindingsInputActionMap = nullptr;

namespace Enjin {
namespace Scripting {

void SetBindingsInputActionMap(InputSystem::InputActionMap* map) {
    s_BindingsInputActionMap = map;
}

// --- Wrapper functions ---

static bool Input_IsActionDown(i32 action) {
    if (!s_BindingsInputActionMap || action < 0 || action >= static_cast<i32>(InputSystem::GameAction::Count)) return false;
    return s_BindingsInputActionMap->IsActionDown(static_cast<InputSystem::GameAction>(action));
}

static bool Input_IsActionPressed(i32 action) {
    if (!s_BindingsInputActionMap || action < 0 || action >= static_cast<i32>(InputSystem::GameAction::Count)) return false;
    return s_BindingsInputActionMap->IsActionPressed(static_cast<InputSystem::GameAction>(action));
}

static bool Input_IsActionReleased(i32 action) {
    if (!s_BindingsInputActionMap || action < 0 || action >= static_cast<i32>(InputSystem::GameAction::Count)) return false;
    return s_BindingsInputActionMap->IsActionReleased(static_cast<InputSystem::GameAction>(action));
}

static f32 Input_GetActionValue(i32 action) {
    if (!s_BindingsInputActionMap || action < 0 || action >= static_cast<i32>(InputSystem::GameAction::Count)) return 0.0f;
    return s_BindingsInputActionMap->GetActionValue(static_cast<InputSystem::GameAction>(action));
}

static Math::Vector2 Input_GetMovementVector() {
    if (!s_BindingsInputActionMap) return Math::Vector2(0, 0);
    return s_BindingsInputActionMap->GetMovementVector();
}

static void Input_SetSensitivity(i32 action, f32 sensitivity) {
    if (!s_BindingsInputActionMap || action < 0 || action >= static_cast<i32>(InputSystem::GameAction::Count)) return;
    s_BindingsInputActionMap->SetSensitivity(static_cast<InputSystem::GameAction>(action), sensitivity);
}

static f32 Input_GetMouseSensitivity() {
    if (!s_BindingsInputActionMap) return 1.0f;
    return s_BindingsInputActionMap->GetMouseSensitivity();
}

static void Input_SetMouseSensitivity(f32 sens) {
    if (!s_BindingsInputActionMap) return;
    s_BindingsInputActionMap->SetMouseSensitivity(sens);
}

static bool Input_IsSprintToggle() {
    if (!s_BindingsInputActionMap) return false;
    return s_BindingsInputActionMap->IsSprintToggle();
}

static void Input_SetSprintToggle(bool toggle) {
    if (!s_BindingsInputActionMap) return;
    s_BindingsInputActionMap->SetSprintToggle(toggle);
}

static bool Input_IsCrouchToggle() {
    if (!s_BindingsInputActionMap) return false;
    return s_BindingsInputActionMap->IsCrouchToggle();
}

static void Input_SetCrouchToggle(bool toggle) {
    if (!s_BindingsInputActionMap) return;
    s_BindingsInputActionMap->SetCrouchToggle(toggle);
}

static void Input_RebindAction(i32 actionIndex, i32 keyCode) {
    if (!s_BindingsInputActionMap || actionIndex < 0 || actionIndex >= static_cast<i32>(InputSystem::GameAction::Count)) return;
    s_BindingsInputActionMap->RebindAction(actionIndex, keyCode);
}

static i32 Input_PollNextKeyPress() {
    if (!s_BindingsInputActionMap) return -1;
    return s_BindingsInputActionMap->PollNextKeyPress();
}

static i32 Input_GetActionCount() {
    if (!s_BindingsInputActionMap) return 0;
    return s_BindingsInputActionMap->GetActionCount();
}

static std::string Input_GetActionName(i32 index) {
    if (!s_BindingsInputActionMap || index < 0 || index >= s_BindingsInputActionMap->GetActionCount()) return "";
    const char* name = s_BindingsInputActionMap->GetActionName(index);
    return name ? std::string(name) : std::string();
}

static std::string Input_GetBindingDisplayName(i32 index) {
    if (!s_BindingsInputActionMap || index < 0 || index >= s_BindingsInputActionMap->GetActionCount()) return "";
    const char* name = s_BindingsInputActionMap->GetBindingDisplayName(index);
    return name ? std::string(name) : std::string();
}

static void Input_ApplyLeftHandOnly() {
    if (!s_BindingsInputActionMap) return;
    s_BindingsInputActionMap->ApplyLeftHandOnly();
}

static void Input_ApplyRightHandOnly() {
    if (!s_BindingsInputActionMap) return;
    s_BindingsInputActionMap->ApplyRightHandOnly();
}

static void Input_ApplyGamepadOnly() {
    if (!s_BindingsInputActionMap) return;
    s_BindingsInputActionMap->ApplyGamepadOnly();
}

static void Input_ResetToDefaults() {
    if (!s_BindingsInputActionMap) return;
    s_BindingsInputActionMap->ResetToDefaults();
}

void RegisterInputActionBindings(asIScriptEngine* engine) {
    // GameAction enum constants
    AS_CHECK(engine->RegisterEnum("GameAction"));
    AS_CHECK(engine->RegisterEnumValue("GameAction", "MoveForward", 0));
    AS_CHECK(engine->RegisterEnumValue("GameAction", "MoveBack", 1));
    AS_CHECK(engine->RegisterEnumValue("GameAction", "MoveLeft", 2));
    AS_CHECK(engine->RegisterEnumValue("GameAction", "MoveRight", 3));
    AS_CHECK(engine->RegisterEnumValue("GameAction", "Jump", 4));
    AS_CHECK(engine->RegisterEnumValue("GameAction", "Sprint", 5));
    AS_CHECK(engine->RegisterEnumValue("GameAction", "Crouch", 6));
    AS_CHECK(engine->RegisterEnumValue("GameAction", "Dash", 7));
    AS_CHECK(engine->RegisterEnumValue("GameAction", "Interact", 8));
    AS_CHECK(engine->RegisterEnumValue("GameAction", "Attack", 9));
    AS_CHECK(engine->RegisterEnumValue("GameAction", "Block", 10));
    AS_CHECK(engine->RegisterEnumValue("GameAction", "Pause", 11));
    AS_CHECK(engine->RegisterEnumValue("GameAction", "LookUp", 12));
    AS_CHECK(engine->RegisterEnumValue("GameAction", "LookDown", 13));
    AS_CHECK(engine->RegisterEnumValue("GameAction", "LookLeft", 14));
    AS_CHECK(engine->RegisterEnumValue("GameAction", "LookRight", 15));
    AS_CHECK(engine->RegisterEnumValue("GameAction", "CameraZoomIn", 16));
    AS_CHECK(engine->RegisterEnumValue("GameAction", "CameraZoomOut", 17));

    // Action query functions
    AS_CHECK(engine->RegisterGlobalFunction("bool InputAction_IsDown(int action)", ENJIN_AS_FN(Input_IsActionDown), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool InputAction_IsPressed(int action)", ENJIN_AS_FN(Input_IsActionPressed), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool InputAction_IsReleased(int action)", ENJIN_AS_FN(Input_IsActionReleased), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float InputAction_GetValue(int action)", ENJIN_AS_FN(Input_GetActionValue), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("Vector2 InputAction_GetMovement()", ENJIN_AS_FN(Input_GetMovementVector), ENJIN_AS_CALL_CDECL));

    // Sensitivity
    AS_CHECK(engine->RegisterGlobalFunction("void InputAction_SetSensitivity(int action, float sensitivity)", ENJIN_AS_FN(Input_SetSensitivity), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float InputAction_GetMouseSensitivity()", ENJIN_AS_FN(Input_GetMouseSensitivity), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void InputAction_SetMouseSensitivity(float sens)", ENJIN_AS_FN(Input_SetMouseSensitivity), ENJIN_AS_CALL_CDECL));

    // Toggle settings
    AS_CHECK(engine->RegisterGlobalFunction("bool InputAction_IsSprintToggle()", ENJIN_AS_FN(Input_IsSprintToggle), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void InputAction_SetSprintToggle(bool toggle)", ENJIN_AS_FN(Input_SetSprintToggle), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool InputAction_IsCrouchToggle()", ENJIN_AS_FN(Input_IsCrouchToggle), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void InputAction_SetCrouchToggle(bool toggle)", ENJIN_AS_FN(Input_SetCrouchToggle), ENJIN_AS_CALL_CDECL));

    // Rebinding
    AS_CHECK(engine->RegisterGlobalFunction("void InputAction_Rebind(int actionIndex, int keyCode)", ENJIN_AS_FN(Input_RebindAction), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("int InputAction_PollNextKey()", ENJIN_AS_FN(Input_PollNextKeyPress), ENJIN_AS_CALL_CDECL));

    // Display helpers
    AS_CHECK(engine->RegisterGlobalFunction("int InputAction_GetCount()", ENJIN_AS_FN(Input_GetActionCount), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("string InputAction_GetName(int index)", ENJIN_AS_FN(Input_GetActionName), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("string InputAction_GetBindingName(int index)", ENJIN_AS_FN(Input_GetBindingDisplayName), ENJIN_AS_CALL_CDECL));

    // Presets
    AS_CHECK(engine->RegisterGlobalFunction("void InputAction_ApplyLeftHandOnly()", ENJIN_AS_FN(Input_ApplyLeftHandOnly), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void InputAction_ApplyRightHandOnly()", ENJIN_AS_FN(Input_ApplyRightHandOnly), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void InputAction_ApplyGamepadOnly()", ENJIN_AS_FN(Input_ApplyGamepadOnly), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void InputAction_ResetDefaults()", ENJIN_AS_FN(Input_ResetToDefaults), ENJIN_AS_CALL_CDECL));
}

} // namespace Scripting
} // namespace Enjin
