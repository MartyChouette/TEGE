#include "Enjin/Scripting/ScriptBindings.h"
#include "Enjin/Scripting/ASCallConv.h"
#include "Enjin/Platform/Input.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Logging/Log.h"
#include <imgui.h>
#include <angelscript.h>
#include <cassert>

using namespace Enjin;
using namespace Enjin::Math;

#define AS_CHECK(expr) \
    do { int _r = (expr); if (_r < 0) { ENJIN_LOG_ERROR(Script, "AS registration failed (code %d) at %s:%d", _r, __FILE__, __LINE__); } } while(0)

// ============================================================================
// Text input (character-level, OS-processed with shift/layout/dead-key awareness)
// ============================================================================

// Returns all characters typed this frame as a UTF-8 string.
// Reads ImGui's character queue which is populated by GLFW's char callback.
// Supports full Latin range (accented characters, international keyboards).
static std::string Input_GetTextInput() {
    std::string result;
    ImGuiIO& io = ImGui::GetIO();
    for (int i = 0; i < io.InputQueueCharacters.Size; ++i) {
        ImWchar ch = io.InputQueueCharacters[i];
        if (ch < 32) continue; // skip control characters
        // Encode as UTF-8
        if (ch < 0x80) {
            result += static_cast<char>(ch);
        } else if (ch < 0x800) {
            result += static_cast<char>(0xC0 | (ch >> 6));
            result += static_cast<char>(0x80 | (ch & 0x3F));
        } else {
            result += static_cast<char>(0xE0 | (ch >> 12));
            result += static_cast<char>(0x80 | ((ch >> 6) & 0x3F));
            result += static_cast<char>(0x80 | (ch & 0x3F));
        }
    }
    return result;
}

// Returns the number of characters typed this frame.
static int Input_GetTextInputCount() {
    ImGuiIO& io = ImGui::GetIO();
    int count = 0;
    for (int i = 0; i < io.InputQueueCharacters.Size; ++i) {
        if (io.InputQueueCharacters[i] >= 32) ++count;
    }
    return count;
}

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
// Mobile touch overlay scheme (web) — all no-ops on desktop, so scripts can
// call these unconditionally. Presets: 0 Platformer2D, 1 TopDown2D,
// 2 TopDown3D, 3 FirstPerson, 4 ThirdPerson, 5 Generic.
// ============================================================================

static void Touch_UsePreset(int preset) {
    if (preset < 0 || preset > static_cast<int>(Input::TouchPreset::Generic)) return;
    Input::SetTouchControllerPreset(static_cast<Input::TouchPreset>(preset));
}

static void Touch_ClearButtons() {
    Input::TouchScheme s = Input::GetTouchScheme();
    s.buttonCount = 0;
    Input::SetTouchScheme(s);
}

// keyCode: GLFW key held while pressed; negative = mouse button (-1 = left
// click, i.e. fire). col/row place the button on a grid growing up-left from
// the bottom-right of the safe area; radiusFrac is relative to screen height.
static void Touch_AddButton(const std::string& label, int keyCode,
                            float col, float row, float radiusFrac) {
    Input::TouchScheme s = Input::GetTouchScheme();
    if (s.buttonCount >= Input::kMaxTouchButtons) return;
    Input::TouchButtonDef& b = s.buttons[s.buttonCount++];
    b.keyCode = keyCode;
    b.colFromRight = col;
    b.rowFromBottom = row;
    b.radiusFrac = (radiusFrac > 0.01f && radiusFrac < 0.3f) ? radiusFrac : 0.075f;
    for (int i = 0; i < 8; ++i) b.label[i] = '\0';
    for (int i = 0; i < 7 && i < static_cast<int>(label.size()); ++i) b.label[i] = label[i];
    Input::SetTouchScheme(s);
}

// Add a button bound to a GameAction, so it presses the action's CURRENT
// binding and shows its label (rebinding the action updates the button). Pass
// the action id from the InputAction enum; keyCode fallback stays 0.
static void Touch_AddActionButton(const std::string& label, int action,
                                  float col, float row, float radiusFrac) {
    Input::TouchScheme s = Input::GetTouchScheme();
    if (s.buttonCount >= Input::kMaxTouchButtons) return;
    Input::TouchButtonDef& b = s.buttons[s.buttonCount++];
    b.action = action;
    b.colFromRight = col;
    b.rowFromBottom = row;
    b.radiusFrac = (radiusFrac > 0.01f && radiusFrac < 0.3f) ? radiusFrac : 0.075f;
    for (int i = 0; i < 8; ++i) b.label[i] = '\0';
    for (int i = 0; i < 7 && i < static_cast<int>(label.size()); ++i) b.label[i] = label[i];
    Input::SetTouchScheme(s);
}

static void Touch_SetStick(bool enabled, int leftKey, int rightKey, int upKey, int downKey) {
    Input::TouchScheme s = Input::GetTouchScheme();
    s.moveStick = enabled;
    s.stickKeys[0] = leftKey; s.stickKeys[1] = rightKey;
    s.stickKeys[2] = upKey;   s.stickKeys[3] = downKey;
    Input::SetTouchScheme(s);
}

// Bind the move stick to the four movement GameActions, so it reflects their
// current bindings (defaults to MoveLeft/Right/Forward/Back if not customized).
static void Touch_SetStickActions(bool enabled, int leftAction, int rightAction,
                                  int fwdAction, int backAction) {
    Input::TouchScheme s = Input::GetTouchScheme();
    s.moveStick = enabled;
    s.stickActions[0] = leftAction; s.stickActions[1] = rightAction;
    s.stickActions[2] = fwdAction;  s.stickActions[3] = backAction;
    Input::SetTouchScheme(s);
}

static void Touch_SetLookRegion(bool enabled) {
    Input::TouchScheme s = Input::GetTouchScheme();
    s.lookRegion = enabled;
    Input::SetTouchScheme(s);
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

    // Punctuation / symbol keys (US layout positions), so scripts that read the
    // keyboard per-key (typewriters, text entry) can reach them.
    AS_CHECK(engine->RegisterEnumValue("Key", "Apostrophe",   static_cast<int>(KeyCode::Apostrophe)));
    AS_CHECK(engine->RegisterEnumValue("Key", "Comma",        static_cast<int>(KeyCode::Comma)));
    AS_CHECK(engine->RegisterEnumValue("Key", "Minus",        static_cast<int>(KeyCode::Minus)));
    AS_CHECK(engine->RegisterEnumValue("Key", "Period",       static_cast<int>(KeyCode::Period)));
    AS_CHECK(engine->RegisterEnumValue("Key", "Slash",        static_cast<int>(KeyCode::Slash)));
    AS_CHECK(engine->RegisterEnumValue("Key", "Semicolon",    static_cast<int>(KeyCode::Semicolon)));
    AS_CHECK(engine->RegisterEnumValue("Key", "Equal",        static_cast<int>(KeyCode::Equal)));
    AS_CHECK(engine->RegisterEnumValue("Key", "LeftBracket",  static_cast<int>(KeyCode::LeftBracket)));
    AS_CHECK(engine->RegisterEnumValue("Key", "Backslash",    static_cast<int>(KeyCode::Backslash)));
    AS_CHECK(engine->RegisterEnumValue("Key", "RightBracket", static_cast<int>(KeyCode::RightBracket)));
    AS_CHECK(engine->RegisterEnumValue("Key", "GraveAccent",  static_cast<int>(KeyCode::GraveAccent)));

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
        ENJIN_AS_FN(Input_GetKey), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool Input_GetKeyDown(int)",
        ENJIN_AS_FN(Input_GetKeyDown), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool Input_GetKeyUp(int)",
        ENJIN_AS_FN(Input_GetKeyUp), ENJIN_AS_CALL_CDECL));

    // ---- Text input (character-level) ----
    AS_CHECK(engine->RegisterGlobalFunction("string Input_GetTextInput()",
        ENJIN_AS_FN(Input_GetTextInput), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("int Input_GetTextInputCount()",
        ENJIN_AS_FN(Input_GetTextInputCount), ENJIN_AS_CALL_CDECL));

    // ---- Mouse functions ----
    AS_CHECK(engine->RegisterGlobalFunction("bool Input_GetMouseButton(int)",
        ENJIN_AS_FN(Input_GetMouseButton), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool Input_GetMouseButtonDown(int)",
        ENJIN_AS_FN(Input_GetMouseButtonDown), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool Input_GetMouseButtonUp(int)",
        ENJIN_AS_FN(Input_GetMouseButtonUp), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("Vector2 Input_GetMousePosition()",
        ENJIN_AS_FN(Input_GetMousePosition), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("Vector2 Input_GetMouseDelta()",
        ENJIN_AS_FN(Input_GetMouseDelta), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("Vector2 Input_GetScrollDelta()",
        ENJIN_AS_FN(Input_GetScrollDelta), ENJIN_AS_CALL_CDECL));

    // ---- Mouse capture ----
    AS_CHECK(engine->RegisterGlobalFunction("bool Input_IsMouseCaptured()",
        ENJIN_AS_FN(Input_IsMouseCaptured), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Input_SetMouseCaptured(bool)",
        ENJIN_AS_FN(Input_SetMouseCaptured), ENJIN_AS_CALL_CDECL));

    // ---- Gamepad functions ----
    AS_CHECK(engine->RegisterGlobalFunction("bool Input_IsGamepadConnected(int)",
        ENJIN_AS_FN(Input_IsGamepadConnected), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool Input_GetGamepadButton(int, int)",
        ENJIN_AS_FN(Input_GetGamepadButton), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool Input_GetGamepadButtonDown(int, int)",
        ENJIN_AS_FN(Input_GetGamepadButtonDown), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Input_GetGamepadAxis(int, int)",
        ENJIN_AS_FN(Input_GetGamepadAxis), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("Vector2 Input_GetGamepadLeftStick(int)",
        ENJIN_AS_FN(Input_GetGamepadLeftStick), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("Vector2 Input_GetGamepadRightStick(int)",
        ENJIN_AS_FN(Input_GetGamepadRightStick), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Input_GetGamepadLeftTrigger(int)",
        ENJIN_AS_FN(Input_GetGamepadLeftTrigger), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Input_GetGamepadRightTrigger(int)",
        ENJIN_AS_FN(Input_GetGamepadRightTrigger), ENJIN_AS_CALL_CDECL));

    // Mobile touch overlay scheme (web; desktop no-ops)
    AS_CHECK(engine->RegisterGlobalFunction("void Touch_UsePreset(int)",
        ENJIN_AS_FN(Touch_UsePreset), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Touch_ClearButtons()",
        ENJIN_AS_FN(Touch_ClearButtons), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "void Touch_AddButton(const string &in, int, float, float, float)",
        ENJIN_AS_FN(Touch_AddButton), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "void Touch_AddActionButton(const string &in, int, float, float, float)",
        ENJIN_AS_FN(Touch_AddActionButton), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Touch_SetStick(bool, int, int, int, int)",
        ENJIN_AS_FN(Touch_SetStick), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Touch_SetStickActions(bool, int, int, int, int)",
        ENJIN_AS_FN(Touch_SetStickActions), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Touch_SetLookRegion(bool)",
        ENJIN_AS_FN(Touch_SetLookRegion), ENJIN_AS_CALL_CDECL));
}

} // namespace Scripting
} // namespace Enjin
