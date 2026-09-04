#include "Enjin/Input/InputAction.h"
#include "Enjin/Logging/Log.h"
#include <imgui.h>
#include <nlohmann/json.hpp>
#include <cmath>

using json = nlohmann::json;

namespace Enjin {
namespace InputSystem {

// ---------------------------------------------------------------------------
// The action table. One row per GameAction, in enum order: display name,
// menu category, DEFAULT bindings, trigger mode, and how touch/hints show it.
// LoadDefaults, GetActionName, GetActionCategory, the touch presets and the
// controls hint all read this, so adding an action is adding a row here.
// ---------------------------------------------------------------------------
namespace {
    constexpr i32 N = -1;
    constexpr i32 K(KeyCode k)        { return static_cast<i32>(k); }
    constexpr i32 M(MouseButton b)    { return static_cast<i32>(b); }
    constexpr i32 P(GamepadButton b)  { return static_cast<i32>(b); }
    constexpr i32 AX(GamepadAxis a)   { return static_cast<i32>(a); }
    constexpr u32 HOLD = 0, PRESS = 2;
    using AC = ActionCategory;
    using TH = TouchHint;

    const ActionInfo kActionInfo[] = {
        //  name               category      key1               key2               mouse          pad                pad2                axis                 axis+   thr   mode   touch         label   verb
        { "Move Forward",      AC::Movement, K(KeyCode::W),     K(KeyCode::Up),    N,             N,                 N,                  AX(GamepadAxis::LeftY),  false, 0.5f, HOLD,  TH::Stick,    "",     "move" },
        { "Move Back",         AC::Movement, K(KeyCode::S),     K(KeyCode::Down),  N,             N,                 N,                  AX(GamepadAxis::LeftY),  true,  0.5f, HOLD,  TH::Stick,    "",     "move" },
        { "Move Left",         AC::Movement, K(KeyCode::A),     K(KeyCode::Left),  N,             N,                 N,                  AX(GamepadAxis::LeftX),  false, 0.5f, HOLD,  TH::Stick,    "",     "move" },
        { "Move Right",        AC::Movement, K(KeyCode::D),     K(KeyCode::Right), N,             N,                 N,                  AX(GamepadAxis::LeftX),  true,  0.5f, HOLD,  TH::Stick,    "",     "move" },
        { "Jump",              AC::Movement, K(KeyCode::Space), N,                 N,             P(GamepadButton::A),          N,                       N,       true,  0.5f, PRESS, TH::Button,   "JMP",  "jump" },
        { "Sprint",            AC::Movement, K(KeyCode::LeftShift), K(KeyCode::RightShift), N,    P(GamepadButton::LeftStick),  P(GamepadButton::LeftBumper), N,  true,  0.5f, HOLD,  TH::Button,   "RUN",  "sprint" },
        { "Crouch",            AC::Movement, K(KeyCode::LeftControl), K(KeyCode::C), N,           P(GamepadButton::B),          N,                       N,       true,  0.5f, PRESS, TH::NotShown, "",     "crouch" },
        { "Dash",              AC::Movement, K(KeyCode::LeftShift), N,             N,             P(GamepadButton::RightBumper), N,                      N,       true,  0.5f, PRESS, TH::NotShown, "",     "dash" },
        { "Interact",          AC::Actions,  K(KeyCode::E),     N,                 N,             P(GamepadButton::X),          N,                       N,       true,  0.5f, PRESS, TH::Button,   "USE",  "interact" },
        { "Attack",            AC::Actions,  N,                 N,                 M(MouseButton::Left),  N,                    N,   AX(GamepadAxis::RightTrigger), true, 0.3f, PRESS, TH::Button,   "FIRE", "attack" },
        { "Block",             AC::Actions,  N,                 N,                 M(MouseButton::Right), N,                    N,   AX(GamepadAxis::LeftTrigger),  true, 0.3f, HOLD,  TH::NotShown, "",     "block" },
        { "Pause",             AC::Actions,  K(KeyCode::Escape), N,                N,             P(GamepadButton::Start),      N,                       N,       true,  0.5f, PRESS, TH::NotShown, "",     "pause" },
        { "Look Up",           AC::Camera,   N,                 N,                 N,             N,                 N,                  AX(GamepadAxis::RightY), false, 0.5f, HOLD,  TH::Look,     "",     "look" },
        { "Look Down",         AC::Camera,   N,                 N,                 N,             N,                 N,                  AX(GamepadAxis::RightY), true,  0.5f, HOLD,  TH::Look,     "",     "look" },
        { "Look Left",         AC::Camera,   N,                 N,                 N,             N,                 N,                  AX(GamepadAxis::RightX), false, 0.5f, HOLD,  TH::Look,     "",     "look" },
        { "Look Right",        AC::Camera,   N,                 N,                 N,             N,                 N,                  AX(GamepadAxis::RightX), true,  0.5f, HOLD,  TH::Look,     "",     "look" },
        { "Camera Zoom In",    AC::Camera,   N,                 N,                 N,             P(GamepadButton::DPadUp),     N,                       N,       true,  0.5f, PRESS, TH::NotShown, "",     "zoom in" },
        { "Camera Zoom Out",   AC::Camera,   N,                 N,                 N,             P(GamepadButton::DPadDown),   N,                       N,       true,  0.5f, PRESS, TH::NotShown, "",     "zoom out" },
        { "Confirm",           AC::UI,       K(KeyCode::Enter), K(KeyCode::Space), N,             P(GamepadButton::A),          N,                       N,       true,  0.5f, PRESS, TH::NotShown, "",     "confirm" },
        { "Cancel",            AC::UI,       K(KeyCode::Escape), K(KeyCode::Backspace), N,        P(GamepadButton::B),          N,                       N,       true,  0.5f, PRESS, TH::NotShown, "",     "cancel" },
        { "Menu Up",           AC::UI,       K(KeyCode::Up),    K(KeyCode::W),     N,             P(GamepadButton::DPadUp),     N,                       N,       true,  0.5f, PRESS, TH::NotShown, "",     "up" },
        { "Menu Down",         AC::UI,       K(KeyCode::Down),  K(KeyCode::S),     N,             P(GamepadButton::DPadDown),   N,                       N,       true,  0.5f, PRESS, TH::NotShown, "",     "down" },
        { "Menu Left",         AC::UI,       K(KeyCode::Left),  K(KeyCode::A),     N,             P(GamepadButton::DPadLeft),   N,                       N,       true,  0.5f, PRESS, TH::NotShown, "",     "left" },
        { "Menu Right",        AC::UI,       K(KeyCode::Right), K(KeyCode::D),     N,             P(GamepadButton::DPadRight),  N,                       N,       true,  0.5f, PRESS, TH::NotShown, "",     "right" },
        { "Advance Dialogue",  AC::UI,       K(KeyCode::Space), K(KeyCode::Enter), M(MouseButton::Left), P(GamepadButton::A),   N,                       N,       true,  0.5f, PRESS, TH::NotShown, "",     "advance" },
        { "Custom 1",          AC::Custom,   N, N, N, N, N, N, true, 0.5f, PRESS, TH::Button, "", "" },
        { "Custom 2",          AC::Custom,   N, N, N, N, N, N, true, 0.5f, PRESS, TH::Button, "", "" },
        { "Custom 3",          AC::Custom,   N, N, N, N, N, N, true, 0.5f, PRESS, TH::Button, "", "" },
        { "Custom 4",          AC::Custom,   N, N, N, N, N, N, true, 0.5f, PRESS, TH::Button, "", "" },
        { "Custom 5",          AC::Custom,   N, N, N, N, N, N, true, 0.5f, PRESS, TH::Button, "", "" },
        { "Custom 6",          AC::Custom,   N, N, N, N, N, N, true, 0.5f, PRESS, TH::Button, "", "" },
        { "Custom 7",          AC::Custom,   N, N, N, N, N, N, true, 0.5f, PRESS, TH::Button, "", "" },
        { "Custom 8",          AC::Custom,   N, N, N, N, N, N, true, 0.5f, PRESS, TH::Button, "", "" },
    };
    static_assert(sizeof(kActionInfo) / sizeof(kActionInfo[0]) == static_cast<size_t>(GameAction::Count),
                  "kActionInfo must have exactly one row per GameAction, in enum order");

    bool IsCustomAction(u32 i) {
        return i >= static_cast<u32>(GameAction::Custom0) &&
               i <  static_cast<u32>(GameAction::Custom0) + kCustomActionCount;
    }
}

const ActionInfo& GetActionInfo(GameAction action) {
    u32 i = static_cast<u32>(action);
    if (i >= static_cast<u32>(GameAction::Count)) i = 0;
    return kActionInfo[i];
}

InputActionMap::InputActionMap() {
    LoadDefaults();
}

void InputActionMap::LoadDefaults() {
    const u32 count = static_cast<u32>(GameAction::Count);
    for (u32 i = 0; i < count; ++i) {
        m_Actions[i] = ActionConfig{};
        m_Actions[i].action = static_cast<GameAction>(i);
        m_Actions[i].bindings.clear();
        m_ToggleState[i] = false;
    }

    auto addKey = [](ActionConfig& cfg, KeyCode key) {
        InputBinding b;
        b.type = BindingType::Key;
        b.code = static_cast<i32>(key);
        cfg.bindings.push_back(b);
    };

    auto addGamepadBtn = [](ActionConfig& cfg, GamepadButton btn) {
        InputBinding b;
        b.type = BindingType::GamepadButton;
        b.code = static_cast<i32>(btn);
        cfg.bindings.push_back(b);
    };

    auto addGamepadAxis = [](ActionConfig& cfg, GamepadAxis axis, bool positive, f32 threshold = 0.5f) {
        InputBinding b;
        b.type = BindingType::GamepadAxis;
        b.code = static_cast<i32>(axis);
        b.axisPositive = positive;
        b.axisThreshold = threshold;
        cfg.bindings.push_back(b);
    };

    auto addMouse = [](ActionConfig& cfg, MouseButton mb) {
        InputBinding b;
        b.type = BindingType::MouseButton;
        b.code = static_cast<i32>(mb);
        cfg.bindings.push_back(b);
    };

    // Every default comes from the action table (keyboard first so
    // GetBindingDisplayName / touch labels prefer the key, then mouse, then pad).
    for (u32 i = 0; i < count; ++i) {
        const ActionInfo& info = kActionInfo[i];
        auto& cfg = m_Actions[i];
        cfg.mode = static_cast<ActionMode>(info.mode);
        if (info.key1  >= 0) addKey(cfg, static_cast<KeyCode>(info.key1));
        if (info.key2  >= 0) addKey(cfg, static_cast<KeyCode>(info.key2));
        if (info.mouse >= 0) addMouse(cfg, static_cast<MouseButton>(info.mouse));
        if (info.pad   >= 0) addGamepadBtn(cfg, static_cast<GamepadButton>(info.pad));
        if (info.pad2  >= 0) addGamepadBtn(cfg, static_cast<GamepadButton>(info.pad2));
        if (info.axis  >= 0) addGamepadAxis(cfg, static_cast<GamepadAxis>(info.axis), info.axisPositive, info.axisThreshold);
    }
}

void InputActionMap::AddBinding(GameAction action, const InputBinding& binding) {
    m_Actions[static_cast<u32>(action)].bindings.push_back(binding);
}

void InputActionMap::ClearBindings(GameAction action) {
    m_Actions[static_cast<u32>(action)].bindings.clear();
}

void InputActionMap::SetCustomActionName(GameAction action, const std::string& name) {
    u32 i = static_cast<u32>(action);
    if (!IsCustomAction(i)) return;
    m_CustomNames[i - static_cast<u32>(GameAction::Custom0)] = name;
}

bool InputActionMap::IsActionListed(i32 index) const {
    if (index < 0 || index >= static_cast<i32>(GameAction::Count)) return false;
    u32 i = static_cast<u32>(index);
    if (!IsCustomAction(i)) return true;
    return !m_CustomNames[i - static_cast<u32>(GameAction::Custom0)].empty();
}

void InputActionMap::Update(f32 dt) {
    (void)dt;
    const u32 count = static_cast<u32>(GameAction::Count);

    for (u32 i = 0; i < count; ++i) {
        const auto& cfg = m_Actions[i];
        bool anyDown = false;
        bool anyPressed = false;
        bool anyReleased = false;

        for (const auto& binding : cfg.bindings) {
            if (IsBindingActive(binding)) anyDown = true;
            if (IsBindingPressed(binding)) anyPressed = true;
            if (IsBindingReleased(binding)) anyReleased = true;
        }

        switch (cfg.mode) {
            case ActionMode::Hold:
                m_ActionDown[i] = anyDown;
                m_ActionPressed[i] = anyPressed;
                m_ActionReleased[i] = anyReleased;
                m_ActionValue[i] = anyDown ? 1.0f : 0.0f;
                break;

            case ActionMode::Toggle:
                if (anyPressed) {
                    m_ToggleState[i] = !m_ToggleState[i];
                }
                m_ActionDown[i] = m_ToggleState[i];
                m_ActionPressed[i] = anyPressed && m_ToggleState[i];
                m_ActionReleased[i] = anyPressed && !m_ToggleState[i];
                m_ActionValue[i] = m_ToggleState[i] ? 1.0f : 0.0f;
                break;

            case ActionMode::Press:
                m_ActionDown[i] = anyDown;
                m_ActionPressed[i] = anyPressed;
                m_ActionReleased[i] = anyReleased;
                m_ActionValue[i] = anyPressed ? 1.0f : 0.0f;
                break;

            case ActionMode::Release:
                m_ActionDown[i] = anyDown;
                m_ActionPressed[i] = anyPressed;
                m_ActionReleased[i] = anyReleased;
                m_ActionValue[i] = anyReleased ? 1.0f : 0.0f;
                break;
        }
    }
}

namespace {
    // A menu, a dialogue or the console owns input while it is up. Rather than
    // every gameplay system checking a different flag, gameplay actions simply
    // read as inactive unless focus is on gameplay. UI actions always pass, so
    // the thing that took focus can still be navigated and dismissed.
    bool ActionPassesFocus(GameAction action) {
        if (Input::IsGameplayFocused()) return true;
        return GetActionInfo(action).category == ActionCategory::UI;
    }
}

bool InputActionMap::IsActionDown(GameAction action) const {
    if (!ActionPassesFocus(action)) return false;
    return m_ActionDown[static_cast<u32>(action)];
}

bool InputActionMap::IsActionPressed(GameAction action) const {
    if (!ActionPassesFocus(action)) return false;
    return m_ActionPressed[static_cast<u32>(action)];
}

bool InputActionMap::IsActionReleased(GameAction action) const {
    if (!ActionPassesFocus(action)) return false;
    return m_ActionReleased[static_cast<u32>(action)];
}

f32 InputActionMap::GetActionValue(GameAction action) const {
    if (!ActionPassesFocus(action)) return 0.0f;
    return m_ActionValue[static_cast<u32>(action)];
}

Math::Vector2 InputActionMap::GetMovementVector() const {
    f32 x = 0.0f, y = 0.0f;
    if (IsActionDown(GameAction::MoveForward)) y += 1.0f;
    if (IsActionDown(GameAction::MoveBack))    y -= 1.0f;
    if (IsActionDown(GameAction::MoveLeft))    x -= 1.0f;
    if (IsActionDown(GameAction::MoveRight))   x += 1.0f;

    Math::Vector2 v(x, y);
    f32 len = v.Length();
    if (len > 1.0f) {
        v = v * (1.0f / len);
    }
    return v;
}

void InputActionMap::SetBinding(GameAction action, u32 bindingIndex, const InputBinding& binding) {
    auto& cfg = m_Actions[static_cast<u32>(action)];
    if (bindingIndex < cfg.bindings.size()) {
        cfg.bindings[bindingIndex] = binding;
    } else {
        cfg.bindings.push_back(binding);
    }
}

void InputActionMap::SetActionMode(GameAction action, ActionMode mode) {
    m_Actions[static_cast<u32>(action)].mode = mode;
    // Reset toggle state when changing modes
    m_ToggleState[static_cast<u32>(action)] = false;
}

void InputActionMap::SetSensitivity(GameAction action, f32 sensitivity) {
    m_Actions[static_cast<u32>(action)].sensitivity = sensitivity;
}

const ActionConfig& InputActionMap::GetActionConfig(GameAction action) const {
    return m_Actions[static_cast<u32>(action)];
}

ActionConfig& InputActionMap::GetActionConfig(GameAction action) {
    return m_Actions[static_cast<u32>(action)];
}

void InputActionMap::ApplyLeftHandOnly() {
    LoadDefaults();
    // Remap movement to WASD (already there) + jump to Space (already there)
    // Remap actions normally on right side to left-hand keys
    auto& interact = m_Actions[static_cast<u32>(GameAction::Interact)];
    interact.bindings.clear();
    InputBinding b; b.type = BindingType::Key;
    b.code = static_cast<i32>(KeyCode::F); interact.bindings.push_back(b);
    b.code = static_cast<i32>(KeyCode::R); interact.bindings.push_back(b);
}

void InputActionMap::ApplyRightHandOnly() {
    LoadDefaults();
    // Remap movement to arrow keys and numpad
    auto& fwd = m_Actions[static_cast<u32>(GameAction::MoveForward)];
    fwd.bindings.clear();
    InputBinding b; b.type = BindingType::Key;
    b.code = static_cast<i32>(KeyCode::Up); fwd.bindings.push_back(b);
    b.code = static_cast<i32>(KeyCode::KP8); fwd.bindings.push_back(b);

    auto& back = m_Actions[static_cast<u32>(GameAction::MoveBack)];
    back.bindings.clear();
    b.code = static_cast<i32>(KeyCode::Down); back.bindings.push_back(b);
    b.code = static_cast<i32>(KeyCode::KP2); back.bindings.push_back(b);

    auto& left = m_Actions[static_cast<u32>(GameAction::MoveLeft)];
    left.bindings.clear();
    b.code = static_cast<i32>(KeyCode::Left); left.bindings.push_back(b);
    b.code = static_cast<i32>(KeyCode::KP4); left.bindings.push_back(b);

    auto& right = m_Actions[static_cast<u32>(GameAction::MoveRight)];
    right.bindings.clear();
    b.code = static_cast<i32>(KeyCode::Right); right.bindings.push_back(b);
    b.code = static_cast<i32>(KeyCode::KP6); right.bindings.push_back(b);

    auto& jump = m_Actions[static_cast<u32>(GameAction::Jump)];
    jump.bindings.clear();
    b.code = static_cast<i32>(KeyCode::KP0); jump.bindings.push_back(b);
    b.code = static_cast<i32>(KeyCode::RightControl); jump.bindings.push_back(b);

    auto& sprint = m_Actions[static_cast<u32>(GameAction::Sprint)];
    sprint.bindings.clear();
    b.code = static_cast<i32>(KeyCode::RightShift); sprint.bindings.push_back(b);
}

void InputActionMap::ApplyGamepadOnly() {
    // Keep only gamepad bindings from defaults
    LoadDefaults();
    const u32 count = static_cast<u32>(GameAction::Count);
    for (u32 i = 0; i < count; ++i) {
        auto& cfg = m_Actions[i];
        std::vector<InputBinding> gamepadOnly;
        for (const auto& b : cfg.bindings) {
            if (b.type == BindingType::GamepadButton || b.type == BindingType::GamepadAxis) {
                gamepadOnly.push_back(b);
            }
        }
        cfg.bindings = gamepadOnly;
    }
}

bool InputActionMap::IsBindingActive(const InputBinding& binding) const {
    switch (binding.type) {
        case BindingType::Key:
            return ::Enjin::Input::IsKeyDown(static_cast<KeyCode>(binding.code));
        case BindingType::MouseButton:
            // A click the UI took is not also a click in the world.
            if (::Enjin::Input::IsUIConsumedPointer()) return false;
            return ::Enjin::Input::IsMouseButtonDown(static_cast<MouseButton>(binding.code));
        case BindingType::GamepadButton:
            for (i32 gp = 0; gp < 4; ++gp) {
                if (::Enjin::Input::IsGamepadConnected(gp) &&
                    ::Enjin::Input::IsGamepadButtonDown(static_cast<GamepadButton>(binding.code), gp)) {
                    return true;
                }
            }
            return false;
        case BindingType::GamepadAxis: {
            for (i32 gp = 0; gp < 4; ++gp) {
                if (!::Enjin::Input::IsGamepadConnected(gp)) continue;
                f32 val = ::Enjin::Input::GetGamepadAxis(static_cast<GamepadAxis>(binding.code), gp);
                if (binding.axisPositive && val > binding.axisThreshold) return true;
                if (!binding.axisPositive && val < -binding.axisThreshold) return true;
            }
            return false;
        }
    }
    return false;
}

bool InputActionMap::IsBindingPressed(const InputBinding& binding) const {
    switch (binding.type) {
        case BindingType::Key:
            return ::Enjin::Input::IsKeyPressed(static_cast<KeyCode>(binding.code));
        case BindingType::MouseButton:
            if (::Enjin::Input::IsUIConsumedPointer()) return false;
            return ::Enjin::Input::IsMouseButtonPressed(static_cast<MouseButton>(binding.code));
        case BindingType::GamepadButton:
            for (i32 gp = 0; gp < 4; ++gp) {
                if (::Enjin::Input::IsGamepadConnected(gp) &&
                    ::Enjin::Input::IsGamepadButtonPressed(static_cast<GamepadButton>(binding.code), gp)) {
                    return true;
                }
            }
            return false;
        case BindingType::GamepadAxis:
            // Axis doesn't have "pressed" - treat threshold crossing as always-down
            return false;
    }
    return false;
}

bool InputActionMap::IsBindingReleased(const InputBinding& binding) const {
    switch (binding.type) {
        case BindingType::Key:
            return ::Enjin::Input::IsKeyReleased(static_cast<KeyCode>(binding.code));
        case BindingType::MouseButton:
            if (::Enjin::Input::IsUIConsumedPointer()) return false;
            return ::Enjin::Input::IsMouseButtonReleased(static_cast<MouseButton>(binding.code));
        case BindingType::GamepadButton:
            // No "released" API for gamepad in the current Input system
            return false;
        case BindingType::GamepadAxis:
            return false;
    }
    return false;
}

f32 InputActionMap::GetMouseSensitivity() const {
    return m_Actions[static_cast<u32>(GameAction::LookUp)].sensitivity;
}

void InputActionMap::SetMouseSensitivity(f32 sens) {
    m_Actions[static_cast<u32>(GameAction::LookUp)].sensitivity = sens;
    m_Actions[static_cast<u32>(GameAction::LookDown)].sensitivity = sens;
    m_Actions[static_cast<u32>(GameAction::LookLeft)].sensitivity = sens;
    m_Actions[static_cast<u32>(GameAction::LookRight)].sensitivity = sens;
}

bool InputActionMap::GetInvertY() const {
    return m_Actions[static_cast<u32>(GameAction::LookUp)].invertAxis;
}

void InputActionMap::SetInvertY(bool invert) {
    m_Actions[static_cast<u32>(GameAction::LookUp)].invertAxis = invert;
    m_Actions[static_cast<u32>(GameAction::LookDown)].invertAxis = invert;
}

bool InputActionMap::IsSprintToggle() const {
    return m_Actions[static_cast<u32>(GameAction::Sprint)].mode == ActionMode::Toggle;
}

void InputActionMap::SetSprintToggle(bool toggle) {
    SetActionMode(GameAction::Sprint, toggle ? ActionMode::Toggle : ActionMode::Hold);
}

bool InputActionMap::IsCrouchToggle() const {
    return m_Actions[static_cast<u32>(GameAction::Crouch)].mode == ActionMode::Toggle;
}

void InputActionMap::SetCrouchToggle(bool toggle) {
    SetActionMode(GameAction::Crouch, toggle ? ActionMode::Toggle : ActionMode::Press);
}

i32 InputActionMap::PollNextKeyPress() const {
    for (int k = ImGuiKey_NamedKey_BEGIN; k < ImGuiKey_NamedKey_END; ++k) {
        if (ImGui::IsKeyPressed(static_cast<ImGuiKey>(k))) {
            // Map ImGuiKey back to GLFW key code where possible
            // For common keys this is a direct mapping
            return k;
        }
    }
    return -1;
}

void InputActionMap::RebindAction(i32 actionIndex, i32 keyCode) {
    if (actionIndex < 0 || actionIndex >= static_cast<i32>(GameAction::Count)) return;
    auto& cfg = m_Actions[actionIndex];
    // Replace the first keyboard binding, or add one
    for (auto& b : cfg.bindings) {
        if (b.type == BindingType::Key) {
            b.code = keyCode;
            return;
        }
    }
    InputBinding b;
    b.type = BindingType::Key;
    b.code = keyCode;
    cfg.bindings.insert(cfg.bindings.begin(), b);
}

i32 InputActionMap::GetActionCount() const {
    return static_cast<i32>(GameAction::Count);
}

const char* InputActionMap::GetActionName(i32 index) const {
    if (index < 0 || index >= static_cast<i32>(GameAction::Count)) return "";
    u32 i = static_cast<u32>(index);
    if (IsCustomAction(i)) {
        const std::string& custom = m_CustomNames[i - static_cast<u32>(GameAction::Custom0)];
        if (!custom.empty()) return custom.c_str();
    }
    return kActionInfo[i].name;
}

static const char* KeyCodeToName(i32 code) {
    // Common GLFW key codes
    if (code >= 65 && code <= 90) { static char buf[2]; buf[0] = (char)code; buf[1] = 0; return buf; }
    if (code >= 48 && code <= 57) { static char buf[2]; buf[0] = (char)code; buf[1] = 0; return buf; }
    switch (code) {
        case 32:  return "Space";
        case 256: return "Escape";
        case 257: return "Enter";
        case 258: return "Tab";
        case 259: return "Backspace";
        case 261: return "Delete";
        case 262: return "Right";
        case 263: return "Left";
        case 264: return "Down";
        case 265: return "Up";
        case 340: return "L.Shift";
        case 341: return "L.Ctrl";
        case 342: return "L.Alt";
        case 344: return "R.Shift";
        case 345: return "R.Ctrl";
        case 346: return "R.Alt";
        case 290: return "F1";  case 291: return "F2";  case 292: return "F3";
        case 293: return "F4";  case 294: return "F5";  case 295: return "F6";
        case 296: return "F7";  case 297: return "F8";  case 298: return "F9";
        case 299: return "F10"; case 300: return "F11"; case 301: return "F12";
        case 320: return "KP0"; case 321: return "KP1"; case 322: return "KP2";
        case 323: return "KP3"; case 324: return "KP4"; case 325: return "KP5";
        case 326: return "KP6"; case 327: return "KP7"; case 328: return "KP8";
        case 329: return "KP9";
        default: break;
    }
    static char fallback[16];
    snprintf(fallback, sizeof(fallback), "Key %d", code);
    return fallback;
}

static const char* MouseButtonToName(i32 code) {
    switch (code) {
        case 0: return "LMB";
        case 1: return "RMB";
        case 2: return "MMB";
        default: break;
    }
    static char buf[16];
    snprintf(buf, sizeof(buf), "Mouse %d", code);
    return buf;
}

static const char* GamepadButtonToName(i32 code) {
    switch (code) {
        case 0: return "A"; case 1: return "B"; case 2: return "X"; case 3: return "Y";
        case 4: return "LB"; case 5: return "RB"; case 6: return "Back"; case 7: return "Start";
        case 8: return "Guide"; case 9: return "LS"; case 10: return "RS";
        case 11: return "D-Up"; case 12: return "D-Right"; case 13: return "D-Down"; case 14: return "D-Left";
        default: break;
    }
    return "?";
}

static const char* GamepadAxisToName(i32 code, bool positive) {
    switch (code) {
        case 0: return positive ? "LS Right" : "LS Left";
        case 1: return positive ? "LS Down" : "LS Up";
        case 2: return positive ? "RS Right" : "RS Left";
        case 3: return positive ? "RS Down" : "RS Up";
        case 4: return "LT";
        case 5: return "RT";
        default: break;
    }
    return "?";
}

const char* InputActionMap::GetBindingDisplayName(i32 index) const {
    if (index < 0 || index >= static_cast<i32>(GameAction::Count)) return "";
    const auto& cfg = m_Actions[index];
    for (const auto& b : cfg.bindings) {
        if (b.type == BindingType::Key) return KeyCodeToName(b.code);
        if (b.type == BindingType::MouseButton) return MouseButtonToName(b.code);
    }
    // Fallback to gamepad if no keyboard/mouse binding
    for (const auto& b : cfg.bindings) {
        if (b.type == BindingType::GamepadButton) return GamepadButtonToName(b.code);
        if (b.type == BindingType::GamepadAxis) return GamepadAxisToName(b.code, b.axisPositive);
    }
    return "None";
}

const char* InputActionMap::GetGamepadBindingDisplayName(i32 index) const {
    if (index < 0 || index >= static_cast<i32>(GameAction::Count)) return "";
    const auto& cfg = m_Actions[index];
    for (const auto& b : cfg.bindings) {
        if (b.type == BindingType::GamepadButton) return GamepadButtonToName(b.code);
        if (b.type == BindingType::GamepadAxis) return GamepadAxisToName(b.code, b.axisPositive);
    }
    return "";
}

i32 InputActionMap::GetActionCategory(i32 index) const {
    if (index < 0 || index >= static_cast<i32>(GameAction::Count)) return static_cast<i32>(ActionCategory::UI);
    return static_cast<i32>(kActionInfo[index].category);
}

std::string InputActionMap::ToJson() const {
    json j = json::array();
    const u32 count = static_cast<u32>(GameAction::Count);
    for (u32 i = 0; i < count; ++i) {
        const auto& cfg = m_Actions[i];
        json actionJson;
        actionJson["action"] = i;
        actionJson["mode"] = static_cast<u32>(cfg.mode);
        actionJson["sensitivity"] = cfg.sensitivity;
        actionJson["invertAxis"] = cfg.invertAxis;
        json bindingsJson = json::array();
        for (const auto& b : cfg.bindings) {
            json bj;
            bj["type"] = static_cast<u32>(b.type);
            bj["code"] = b.code;
            bj["axisThreshold"] = b.axisThreshold;
            bj["axisPositive"] = b.axisPositive;
            bindingsJson.push_back(bj);
        }
        actionJson["bindings"] = bindingsJson;
        j.push_back(actionJson);
    }
    return j.dump(2);
}

bool InputActionMap::FromJson(const std::string& jsonStr) {
    try {
        json j = json::parse(jsonStr);
        if (!j.is_array()) return false;

        for (const auto& actionJson : j) {
            u32 idx = actionJson["action"].get<u32>();
            if (idx >= static_cast<u32>(GameAction::Count)) continue;

            auto& cfg = m_Actions[idx];
            cfg.mode = static_cast<ActionMode>(actionJson.value("mode", 0u));
            cfg.sensitivity = actionJson.value("sensitivity", 1.0f);
            cfg.invertAxis = actionJson.value("invertAxis", false);

            cfg.bindings.clear();
            if (actionJson.contains("bindings")) {
                for (const auto& bj : actionJson["bindings"]) {
                    InputBinding b;
                    b.type = static_cast<BindingType>(bj.value("type", 0u));
                    b.code = bj.value("code", 0);
                    b.axisThreshold = bj.value("axisThreshold", 0.5f);
                    b.axisPositive = bj.value("axisPositive", true);
                    cfg.bindings.push_back(b);
                }
            }
        }
        return true;
    } catch (const std::exception& e) {
        ENJIN_LOG_ERROR(Core, "Failed to load input action map: %s", e.what());
        return false;
    } catch (...) {
        ENJIN_LOG_ERROR(Core, "Failed to load input action map: unknown error");
        return false;
    }
}

} // namespace InputSystem
} // namespace Enjin
