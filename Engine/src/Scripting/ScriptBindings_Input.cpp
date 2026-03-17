#include "Enjin/Scripting/ScriptBindings.h"
#include "Enjin/Platform/Input.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Logging/Log.h"
#include <angelscript.h>
#include <cassert>

using namespace Enjin;
using namespace Enjin::Math;

#define AS_CHECK(expr) \
    do { int _r = (expr); if (_r < 0) { ENJIN_LOG_ERROR(Script, "AS registration failed (code %d) at %s:%d", _r, __FILE__, __LINE__); } } while(0)

// ============================================================================
// Input wrapper functions
// ============================================================================

static bool Input_GetKey(int keyCode) {
    return Input::IsKeyDown(static_cast<KeyCode>(keyCode));
}

static bool Input_GetKeyDown(int keyCode) {
    return Input::IsKeyPressed(static_cast<KeyCode>(keyCode));
}

static bool Input_GetKeyUp(int keyCode) {
    return Input::IsKeyReleased(static_cast<KeyCode>(keyCode));
}

static bool Input_GetMouseButton(int button) {
    return Input::IsMouseButtonDown(static_cast<MouseButton>(button));
}

static bool Input_GetMouseButtonDown(int button) {
    return Input::IsMouseButtonPressed(static_cast<MouseButton>(button));
}

static bool Input_GetMouseButtonUp(int button) {
    return Input::IsMouseButtonReleased(static_cast<MouseButton>(button));
}

static Vector2 Input_GetMousePosition() {
    return Input::GetMousePosition();
}

static Vector2 Input_GetMouseDelta() {
    return Input::GetMouseDelta();
}

static Vector2 Input_GetScrollDelta() {
    return Input::GetScrollDelta();
}

static bool Input_IsMouseCaptured() {
    return Input::IsMouseCaptured();
}

static void Input_SetMouseCaptured(bool captured) {
    Input::SetMouseCaptured(captured);
}

// Gamepad wrappers
static bool Input_IsGamepadConnected(int index) {
    return Input::IsGamepadConnected(index);
}

static bool Input_GetGamepadButton(int button, int index) {
    return Input::IsGamepadButtonDown(static_cast<GamepadButton>(button), index);
}

static bool Input_GetGamepadButtonDown(int button, int index) {
    return Input::IsGamepadButtonPressed(static_cast<GamepadButton>(button), index);
}

static float Input_GetGamepadAxis(int axis, int index) {
    return Input::GetGamepadAxis(static_cast<GamepadAxis>(axis), index);
}

static Vector2 Input_GetGamepadLeftStick(int index) {
    return Input::GetGamepadLeftStick(index);
}

static Vector2 Input_GetGamepadRightStick(int index) {
    return Input::GetGamepadRightStick(index);
}

static float Input_GetGamepadLeftTrigger(int index) {
    return Input::GetGamepadLeftTrigger(index);
}

static float Input_GetGamepadRightTrigger(int index) {
    return Input::GetGamepadRightTrigger(index);
}

// ============================================================================
// Registration
// ============================================================================

namespace Enjin {
namespace Scripting {

void RegisterInputBindings(asIScriptEngine* engine) {
    // ---- Key enum ----
    AS_CHECK(engine->RegisterEnum("Key"));

    // Letters
    AS_CHECK(engine->RegisterEnumValue("Key", "A", static_cast<int>(KeyCode::A)));
    AS_CHECK(engine->RegisterEnumValue("Key", "B", static_cast<int>(KeyCode::B)));
    AS_CHECK(engine->RegisterEnumValue("Key", "C", static_cast<int>(KeyCode::C)));
    AS_CHECK(engine->RegisterEnumValue("Key", "D", static_cast<int>(KeyCode::D)));
    AS_CHECK(engine->RegisterEnumValue("Key", "E", static_cast<int>(KeyCode::E)));
    AS_CHECK(engine->RegisterEnumValue("Key", "F", static_cast<int>(KeyCode::F)));
    AS_CHECK(engine->RegisterEnumValue("Key", "G", static_cast<int>(KeyCode::G)));
    AS_CHECK(engine->RegisterEnumValue("Key", "H", static_cast<int>(KeyCode::H)));
    AS_CHECK(engine->RegisterEnumValue("Key", "I", static_cast<int>(KeyCode::I)));
    AS_CHECK(engine->RegisterEnumValue("Key", "J", static_cast<int>(KeyCode::J)));
    AS_CHECK(engine->RegisterEnumValue("Key", "K", static_cast<int>(KeyCode::K)));
    AS_CHECK(engine->RegisterEnumValue("Key", "L", static_cast<int>(KeyCode::L)));
    AS_CHECK(engine->RegisterEnumValue("Key", "M", static_cast<int>(KeyCode::M)));
    AS_CHECK(engine->RegisterEnumValue("Key", "N", static_cast<int>(KeyCode::N)));
    AS_CHECK(engine->RegisterEnumValue("Key", "O", static_cast<int>(KeyCode::O)));
    AS_CHECK(engine->RegisterEnumValue("Key", "P", static_cast<int>(KeyCode::P)));
    AS_CHECK(engine->RegisterEnumValue("Key", "Q", static_cast<int>(KeyCode::Q)));
    AS_CHECK(engine->RegisterEnumValue("Key", "R", static_cast<int>(KeyCode::R)));
    AS_CHECK(engine->RegisterEnumValue("Key", "S", static_cast<int>(KeyCode::S)));
    AS_CHECK(engine->RegisterEnumValue("Key", "T", static_cast<int>(KeyCode::T)));
    AS_CHECK(engine->RegisterEnumValue("Key", "U", static_cast<int>(KeyCode::U)));
    AS_CHECK(engine->RegisterEnumValue("Key", "V", static_cast<int>(KeyCode::V)));
    AS_CHECK(engine->RegisterEnumValue("Key", "W", static_cast<int>(KeyCode::W)));
    AS_CHECK(engine->RegisterEnumValue("Key", "X", static_cast<int>(KeyCode::X)));
    AS_CHECK(engine->RegisterEnumValue("Key", "Y", static_cast<int>(KeyCode::Y)));
    AS_CHECK(engine->RegisterEnumValue("Key", "Z", static_cast<int>(KeyCode::Z)));

    // Numbers
    AS_CHECK(engine->RegisterEnumValue("Key", "Num0", static_cast<int>(KeyCode::Num0)));
    AS_CHECK(engine->RegisterEnumValue("Key", "Num1", static_cast<int>(KeyCode::Num1)));
    AS_CHECK(engine->RegisterEnumValue("Key", "Num2", static_cast<int>(KeyCode::Num2)));
    AS_CHECK(engine->RegisterEnumValue("Key", "Num3", static_cast<int>(KeyCode::Num3)));
    AS_CHECK(engine->RegisterEnumValue("Key", "Num4", static_cast<int>(KeyCode::Num4)));
    AS_CHECK(engine->RegisterEnumValue("Key", "Num5", static_cast<int>(KeyCode::Num5)));
    AS_CHECK(engine->RegisterEnumValue("Key", "Num6", static_cast<int>(KeyCode::Num6)));
    AS_CHECK(engine->RegisterEnumValue("Key", "Num7", static_cast<int>(KeyCode::Num7)));
    AS_CHECK(engine->RegisterEnumValue("Key", "Num8", static_cast<int>(KeyCode::Num8)));
    AS_CHECK(engine->RegisterEnumValue("Key", "Num9", static_cast<int>(KeyCode::Num9)));

    // Function keys
    AS_CHECK(engine->RegisterEnumValue("Key", "F1",  static_cast<int>(KeyCode::F1)));
    AS_CHECK(engine->RegisterEnumValue("Key", "F2",  static_cast<int>(KeyCode::F2)));
    AS_CHECK(engine->RegisterEnumValue("Key", "F3",  static_cast<int>(KeyCode::F3)));
    AS_CHECK(engine->RegisterEnumValue("Key", "F4",  static_cast<int>(KeyCode::F4)));
    AS_CHECK(engine->RegisterEnumValue("Key", "F5",  static_cast<int>(KeyCode::F5)));
    AS_CHECK(engine->RegisterEnumValue("Key", "F6",  static_cast<int>(KeyCode::F6)));
    AS_CHECK(engine->RegisterEnumValue("Key", "F7",  static_cast<int>(KeyCode::F7)));
    AS_CHECK(engine->RegisterEnumValue("Key", "F8",  static_cast<int>(KeyCode::F8)));
    AS_CHECK(engine->RegisterEnumValue("Key", "F9",  static_cast<int>(KeyCode::F9)));
    AS_CHECK(engine->RegisterEnumValue("Key", "F10", static_cast<int>(KeyCode::F10)));
    AS_CHECK(engine->RegisterEnumValue("Key", "F11", static_cast<int>(KeyCode::F11)));
    AS_CHECK(engine->RegisterEnumValue("Key", "F12", static_cast<int>(KeyCode::F12)));

    // Special keys
    AS_CHECK(engine->RegisterEnumValue("Key", "Space",     static_cast<int>(KeyCode::Space)));
    AS_CHECK(engine->RegisterEnumValue("Key", "Escape",    static_cast<int>(KeyCode::Escape)));
    AS_CHECK(engine->RegisterEnumValue("Key", "Enter",     static_cast<int>(KeyCode::Enter)));
    AS_CHECK(engine->RegisterEnumValue("Key", "Tab",       static_cast<int>(KeyCode::Tab)));
    AS_CHECK(engine->RegisterEnumValue("Key", "Backspace", static_cast<int>(KeyCode::Backspace)));
    AS_CHECK(engine->RegisterEnumValue("Key", "Insert",    static_cast<int>(KeyCode::Insert)));
    AS_CHECK(engine->RegisterEnumValue("Key", "Delete",    static_cast<int>(KeyCode::Delete)));

    // Arrow keys
    AS_CHECK(engine->RegisterEnumValue("Key", "Right", static_cast<int>(KeyCode::Right)));
    AS_CHECK(engine->RegisterEnumValue("Key", "Left",  static_cast<int>(KeyCode::Left)));
    AS_CHECK(engine->RegisterEnumValue("Key", "Down",  static_cast<int>(KeyCode::Down)));
    AS_CHECK(engine->RegisterEnumValue("Key", "Up",    static_cast<int>(KeyCode::Up)));

    // Modifiers
    AS_CHECK(engine->RegisterEnumValue("Key", "LeftShift",   static_cast<int>(KeyCode::LeftShift)));
    AS_CHECK(engine->RegisterEnumValue("Key", "LeftControl", static_cast<int>(KeyCode::LeftControl)));
    AS_CHECK(engine->RegisterEnumValue("Key", "LeftAlt",     static_cast<int>(KeyCode::LeftAlt)));
    AS_CHECK(engine->RegisterEnumValue("Key", "RightShift",  static_cast<int>(KeyCode::RightShift)));
    AS_CHECK(engine->RegisterEnumValue("Key", "RightControl",static_cast<int>(KeyCode::RightControl)));
    AS_CHECK(engine->RegisterEnumValue("Key", "RightAlt",    static_cast<int>(KeyCode::RightAlt)));

    // ---- MouseButton enum ----
    AS_CHECK(engine->RegisterEnum("MouseBtn"));
    AS_CHECK(engine->RegisterEnumValue("MouseBtn", "Left",   static_cast<int>(MouseButton::Left)));
    AS_CHECK(engine->RegisterEnumValue("MouseBtn", "Right",  static_cast<int>(MouseButton::Right)));
    AS_CHECK(engine->RegisterEnumValue("MouseBtn", "Middle", static_cast<int>(MouseButton::Middle)));

    // ---- GamepadButton enum ----
    AS_CHECK(engine->RegisterEnum("GamepadBtn"));
    AS_CHECK(engine->RegisterEnumValue("GamepadBtn", "A",          static_cast<int>(GamepadButton::A)));
    AS_CHECK(engine->RegisterEnumValue("GamepadBtn", "B",          static_cast<int>(GamepadButton::B)));
    AS_CHECK(engine->RegisterEnumValue("GamepadBtn", "X",          static_cast<int>(GamepadButton::X)));
    AS_CHECK(engine->RegisterEnumValue("GamepadBtn", "Y",          static_cast<int>(GamepadButton::Y)));
    AS_CHECK(engine->RegisterEnumValue("GamepadBtn", "LeftBumper", static_cast<int>(GamepadButton::LeftBumper)));
    AS_CHECK(engine->RegisterEnumValue("GamepadBtn", "RightBumper",static_cast<int>(GamepadButton::RightBumper)));
    AS_CHECK(engine->RegisterEnumValue("GamepadBtn", "Back",       static_cast<int>(GamepadButton::Back)));
    AS_CHECK(engine->RegisterEnumValue("GamepadBtn", "Start",      static_cast<int>(GamepadButton::Start)));
    AS_CHECK(engine->RegisterEnumValue("GamepadBtn", "DPadUp",     static_cast<int>(GamepadButton::DPadUp)));
    AS_CHECK(engine->RegisterEnumValue("GamepadBtn", "DPadRight",  static_cast<int>(GamepadButton::DPadRight)));
    AS_CHECK(engine->RegisterEnumValue("GamepadBtn", "DPadDown",   static_cast<int>(GamepadButton::DPadDown)));
    AS_CHECK(engine->RegisterEnumValue("GamepadBtn", "DPadLeft",   static_cast<int>(GamepadButton::DPadLeft)));

    // ---- GamepadAxis enum ----
    AS_CHECK(engine->RegisterEnum("GamepadAx"));
    AS_CHECK(engine->RegisterEnumValue("GamepadAx", "LeftX",       static_cast<int>(GamepadAxis::LeftX)));
    AS_CHECK(engine->RegisterEnumValue("GamepadAx", "LeftY",       static_cast<int>(GamepadAxis::LeftY)));
    AS_CHECK(engine->RegisterEnumValue("GamepadAx", "RightX",      static_cast<int>(GamepadAxis::RightX)));
    AS_CHECK(engine->RegisterEnumValue("GamepadAx", "RightY",      static_cast<int>(GamepadAxis::RightY)));
    AS_CHECK(engine->RegisterEnumValue("GamepadAx", "LeftTrigger", static_cast<int>(GamepadAxis::LeftTrigger)));
    AS_CHECK(engine->RegisterEnumValue("GamepadAx", "RightTrigger",static_cast<int>(GamepadAxis::RightTrigger)));

    // ---- Keyboard functions ----
    AS_CHECK(engine->RegisterGlobalFunction("bool Input_GetKey(int)",
        asFUNCTION(Input_GetKey), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool Input_GetKeyDown(int)",
        asFUNCTION(Input_GetKeyDown), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool Input_GetKeyUp(int)",
        asFUNCTION(Input_GetKeyUp), asCALL_CDECL));

    // ---- Mouse functions ----
    AS_CHECK(engine->RegisterGlobalFunction("bool Input_GetMouseButton(int)",
        asFUNCTION(Input_GetMouseButton), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool Input_GetMouseButtonDown(int)",
        asFUNCTION(Input_GetMouseButtonDown), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool Input_GetMouseButtonUp(int)",
        asFUNCTION(Input_GetMouseButtonUp), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("Vector2 Input_GetMousePosition()",
        asFUNCTION(Input_GetMousePosition), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("Vector2 Input_GetMouseDelta()",
        asFUNCTION(Input_GetMouseDelta), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("Vector2 Input_GetScrollDelta()",
        asFUNCTION(Input_GetScrollDelta), asCALL_CDECL));

    // ---- Mouse capture ----
    AS_CHECK(engine->RegisterGlobalFunction("bool Input_IsMouseCaptured()",
        asFUNCTION(Input_IsMouseCaptured), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Input_SetMouseCaptured(bool)",
        asFUNCTION(Input_SetMouseCaptured), asCALL_CDECL));

    // ---- Gamepad functions ----
    AS_CHECK(engine->RegisterGlobalFunction("bool Input_IsGamepadConnected(int)",
        asFUNCTION(Input_IsGamepadConnected), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool Input_GetGamepadButton(int, int)",
        asFUNCTION(Input_GetGamepadButton), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool Input_GetGamepadButtonDown(int, int)",
        asFUNCTION(Input_GetGamepadButtonDown), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Input_GetGamepadAxis(int, int)",
        asFUNCTION(Input_GetGamepadAxis), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("Vector2 Input_GetGamepadLeftStick(int)",
        asFUNCTION(Input_GetGamepadLeftStick), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("Vector2 Input_GetGamepadRightStick(int)",
        asFUNCTION(Input_GetGamepadRightStick), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Input_GetGamepadLeftTrigger(int)",
        asFUNCTION(Input_GetGamepadLeftTrigger), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Input_GetGamepadRightTrigger(int)",
        asFUNCTION(Input_GetGamepadRightTrigger), asCALL_CDECL));
}

} // namespace Scripting
} // namespace Enjin
