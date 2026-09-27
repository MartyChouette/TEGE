#include "Enjin/Scripting/ScriptBindings.h"
#include "Enjin/Scripting/ASCallConv.h"
#include "Enjin/Input/InputAction.h"
#include "Enjin/Input/TouchActionBridge.h"
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
    // Keep the touch overlay's action resolvers pointed at the same active map,
    // so on-screen touch controls reflect the current bindings (and rebinds).
    InputSystem::SetTouchActionMap(map);
}

std::string ResolveBindingsPromptText(const std::string& text) {
    return s_BindingsInputActionMap ? s_BindingsInputActionMap->ResolvePromptText(text) : text;
}

// ---------------------------------------------------------------------------
// Public forwarders for VisualScript input-action nodes. Reuse the same map
// pointer the AngelScript API uses (set via SetBindingsInputActionMap at every
// call site) so node rebinding behaves identically to scripted rebinding.
// ---------------------------------------------------------------------------
bool VSInputActionIsDown(i32 action) {
    if (!s_BindingsInputActionMap || action < 0 || action >= s_BindingsInputActionMap->GetActionCount()) return false;
    return s_BindingsInputActionMap->IsActionDown(static_cast<InputSystem::GameAction>(action));
}
bool VSInputActionIsPressed(i32 action) {
    if (!s_BindingsInputActionMap || action < 0 || action >= s_BindingsInputActionMap->GetActionCount()) return false;
    return s_BindingsInputActionMap->IsActionPressed(static_cast<InputSystem::GameAction>(action));
}
f32 VSInputActionGetValue(i32 action) {
    if (!s_BindingsInputActionMap || action < 0 || action >= s_BindingsInputActionMap->GetActionCount()) return 0.0f;
    return s_BindingsInputActionMap->GetActionValue(static_cast<InputSystem::GameAction>(action));
}
i32 VSInputActionCount() {
    return s_BindingsInputActionMap ? s_BindingsInputActionMap->GetActionCount() : 0;
}
std::string VSInputActionName(i32 index) {
    if (!s_BindingsInputActionMap || index < 0 || index >= s_BindingsInputActionMap->GetActionCount()) return "";
    const char* name = s_BindingsInputActionMap->GetActionName(index);
    return name ? std::string(name) : std::string();
}
std::string VSInputBindingName(i32 index) {
    if (!s_BindingsInputActionMap || index < 0 || index >= s_BindingsInputActionMap->GetActionCount()) return "";
    const char* name = s_BindingsInputActionMap->GetBindingDisplayName(index);
    return name ? std::string(name) : std::string();
}
void VSInputRebind(i32 actionIndex, i32 keyCode) {
    if (!s_BindingsInputActionMap || actionIndex < 0 || actionIndex >= s_BindingsInputActionMap->GetActionCount()) return;
    s_BindingsInputActionMap->RebindAction(actionIndex, keyCode);
}
i32 VSInputPollKey() {
    return s_BindingsInputActionMap ? s_BindingsInputActionMap->PollNextKeyPress() : -1;
}
void VSInputResetBindings() {
    if (s_BindingsInputActionMap) s_BindingsInputActionMap->ResetToDefaults();
}

// --- Wrapper functions ---

static bool Input_IsActionDown(i32 action) {
    if (!s_BindingsInputActionMap || action < 0 || action >= s_BindingsInputActionMap->GetActionCount()) return false;
    return s_BindingsInputActionMap->IsActionDown(static_cast<InputSystem::GameAction>(action));
}

static bool Input_IsActionPressed(i32 action) {
    if (!s_BindingsInputActionMap || action < 0 || action >= s_BindingsInputActionMap->GetActionCount()) return false;
    return s_BindingsInputActionMap->IsActionPressed(static_cast<InputSystem::GameAction>(action));
}

static bool Input_IsActionReleased(i32 action) {
    if (!s_BindingsInputActionMap || action < 0 || action >= s_BindingsInputActionMap->GetActionCount()) return false;
    return s_BindingsInputActionMap->IsActionReleased(static_cast<InputSystem::GameAction>(action));
}

static f32 Input_GetActionValue(i32 action) {
    if (!s_BindingsInputActionMap || action < 0 || action >= s_BindingsInputActionMap->GetActionCount()) return 0.0f;
    return s_BindingsInputActionMap->GetActionValue(static_cast<InputSystem::GameAction>(action));
}

static Math::Vector2 Input_GetMovementVector() {
    if (!s_BindingsInputActionMap) return Math::Vector2(0, 0);
    return s_BindingsInputActionMap->GetMovementVector();
}

static void Input_SetSensitivity(i32 action, f32 sensitivity) {
    if (!s_BindingsInputActionMap || action < 0 || action >= s_BindingsInputActionMap->GetActionCount()) return;
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
    if (!s_BindingsInputActionMap || actionIndex < 0 || actionIndex >= s_BindingsInputActionMap->GetActionCount()) return;
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

// 0 none, 1 left hand, 2 right hand, 3 gamepad only
static void Input_SetPreset(i32 preset) {
    if (!s_BindingsInputActionMap || preset < 0 || preset > 3) return;
    s_BindingsInputActionMap->SetPreset(static_cast<InputSystem::BindingPreset>(preset));
}
static i32 Input_GetPreset() {
    return s_BindingsInputActionMap ? static_cast<i32>(s_BindingsInputActionMap->GetPreset()) : 0;
}

static void Input_ApplyGamepadOnly() {
    if (!s_BindingsInputActionMap) return;
    s_BindingsInputActionMap->ApplyGamepadOnly();
}

static void Input_ResetToDefaults() {
    if (!s_BindingsInputActionMap) return;
    s_BindingsInputActionMap->ResetToDefaults();
}

// Project actions by name (IN-0). Find returns any action, engine or project,
// by its display name; Define returns the project action with that name,
// creating it in the lowest free slot if there is none. The eight Custom slots
// are simply the first eight project actions.
static i32 Input_FindAction(const std::string& name) {
    return s_BindingsInputActionMap ? s_BindingsInputActionMap->FindAction(name) : -1;
}
static i32 Input_DefineAction(const std::string& name) {
    return s_BindingsInputActionMap ? s_BindingsInputActionMap->DefineProjectAction(name) : -1;
}

// Custom action slots (GameAction::Custom0..7): a game names one at boot, binds
// it, and from then on menus, the controls hint and touch (via
// Touch_AddActionButton) treat it like any built-in action.
static void Input_SetActionName(i32 action, const std::string& name) {
    if (!s_BindingsInputActionMap || action < 0 || action >= s_BindingsInputActionMap->GetActionCount()) return;
    s_BindingsInputActionMap->SetCustomActionName(static_cast<InputSystem::GameAction>(action), name);
}

static void Input_AddGamepadBinding(i32 action, i32 button) {
    if (!s_BindingsInputActionMap || action < 0 || action >= s_BindingsInputActionMap->GetActionCount()) return;
    if (button < 0 || button >= static_cast<i32>(GamepadButton::Count)) return;
    InputSystem::InputBinding b;
    b.type = InputSystem::BindingType::GamepadButton;
    b.code = button;
    s_BindingsInputActionMap->AddBinding(static_cast<InputSystem::GameAction>(action), b);
}

static void Input_AddMouseBinding(i32 action, i32 button) {
    if (!s_BindingsInputActionMap || action < 0 || action >= s_BindingsInputActionMap->GetActionCount()) return;
    if (button < 0 || button > 7) return;
    InputSystem::InputBinding b;
    b.type = InputSystem::BindingType::MouseButton;
    b.code = button;
    s_BindingsInputActionMap->AddBinding(static_cast<InputSystem::GameAction>(action), b);
}

static void Input_ClearBindings(i32 action) {
    if (!s_BindingsInputActionMap || action < 0 || action >= s_BindingsInputActionMap->GetActionCount()) return;
    s_BindingsInputActionMap->ClearBindings(static_cast<InputSystem::GameAction>(action));
}

void RegisterInputActionBindings(asIScriptEngine* engine) {
    // GameAction enum constants
    AS_CHECK(engine->RegisterEnum("GameAction"));
    // Generated from the action registry (IN-1): one value per engine action
    // and legacy Custom slot, named by the C++ enumerator, so the script enum
    // cannot drift from the table the way the hand-written list could
    for (u32 i = 0; i < static_cast<u32>(InputSystem::GameAction::Count); ++i) {
        const auto act = static_cast<InputSystem::GameAction>(i);
        AS_CHECK(engine->RegisterEnumValue("GameAction", InputSystem::GetActionIdentifier(act), static_cast<int>(i)));
    }

    // Project actions by name
    AS_CHECK(engine->RegisterGlobalFunction("int InputAction_Find(const string &in name)", ENJIN_AS_FN(Input_FindAction), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("int InputAction_Define(const string &in name)", ENJIN_AS_FN(Input_DefineAction), ENJIN_AS_CALL_CDECL));

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
    AS_CHECK(engine->RegisterGlobalFunction("void InputAction_SetPreset(int preset)", ENJIN_AS_FN(Input_SetPreset), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("int InputAction_GetPreset()", ENJIN_AS_FN(Input_GetPreset), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void InputAction_ResetDefaults()", ENJIN_AS_FN(Input_ResetToDefaults), ENJIN_AS_CALL_CDECL));

    // Custom actions + extra binding types
    AS_CHECK(engine->RegisterGlobalFunction("void InputAction_SetName(int action, const string &in name)", ENJIN_AS_FN(Input_SetActionName), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void InputAction_AddGamepadBinding(int action, int button)", ENJIN_AS_FN(Input_AddGamepadBinding), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void InputAction_AddMouseBinding(int action, int button)", ENJIN_AS_FN(Input_AddMouseBinding), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void InputAction_ClearBindings(int action)", ENJIN_AS_FN(Input_ClearBindings), ENJIN_AS_CALL_CDECL));
}

} // namespace Scripting
} // namespace Enjin
