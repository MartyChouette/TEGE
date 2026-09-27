#include "Enjin/Input/InputAction.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/Platform/Input.h"
#include <imgui.h>
#include <nlohmann/json.hpp>
#include <cmath>
#include <algorithm>

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
    constexpr u32 HOLD = 0, TOGGLE = 1, PRESS = 2;
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
        { "Crouch",            AC::Movement, K(KeyCode::LeftControl), K(KeyCode::C), N,           P(GamepadButton::B),          N,                       N,       true,  0.5f, TOGGLE, TH::NotShown, "",     "crouch" },
        { "Dash",              AC::Movement, K(KeyCode::LeftAlt), N,               N,             P(GamepadButton::RightBumper), N,                      N,       true,  0.5f, PRESS, TH::NotShown, "",     "dash" },
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

    // Every project action past the legacy eight shares this row
    const ActionInfo kProjectActionInfo =
        { "Custom", AC::Custom, N, N, N, N, N, N, true, 0.5f, PRESS, TH::Button, "", "" };
}

namespace {
    // One per enumerator, in enum order. The script enum is registered from
    // this, so a new action is one row here and one in kActionInfo.
    const char* const kActionIdent[] = {
        "MoveForward", "MoveBack", "MoveLeft", "MoveRight", "Jump", "Sprint", "Crouch", "Dash",
        "Interact", "Attack", "Block", "Pause", "LookUp", "LookDown", "LookLeft", "LookRight",
        "CameraZoomIn", "CameraZoomOut", "UIConfirm", "UICancel", "UINavUp", "UINavDown",
        "UINavLeft", "UINavRight", "DialogueAdvance",
        "Custom0", "Custom1", "Custom2", "Custom3", "Custom4", "Custom5", "Custom6", "Custom7",
    };
    static_assert(sizeof(kActionIdent) / sizeof(kActionIdent[0]) == static_cast<size_t>(GameAction::Count),
                  "kActionIdent must have one name per GameAction, in enum order");
}

const char* GetActionCategoryName(ActionCategory category) {
    switch (category) {
        case ActionCategory::Movement: return "Movement";
        case ActionCategory::Actions:  return "Actions";
        case ActionCategory::Camera:   return "Camera";
        case ActionCategory::UI:       return "Menus";
        case ActionCategory::Custom:   return "Game";
        default:                       return "Other";
    }
}

const char* GetActionModeName(ActionMode mode) {
    switch (mode) {
        case ActionMode::Hold:    return "Hold";
        case ActionMode::Toggle:  return "Toggle";
        case ActionMode::Press:   return "Press";
        case ActionMode::Release: return "Release";
        default:                  return "?";
    }
}

const char* GetActionIdentifier(GameAction action) {
    const u32 i = static_cast<u32>(action);
    return i < static_cast<u32>(GameAction::Count) ? kActionIdent[i] : "";
}

const ActionInfo& GetActionInfo(GameAction action) {
    const u32 i = static_cast<u32>(action);
    if (i < static_cast<u32>(GameAction::Count)) return kActionInfo[i];
    return kProjectActionInfo;
}

InputActionMap::InputActionMap() {
    EnsureActionCount(static_cast<u32>(GameAction::Count));
    LoadDefaults();
}

void InputActionMap::EnsureActionCount(u32 count) {
    if (count > kFirstProjectAction + kMaxProjectActions) count = kFirstProjectAction + kMaxProjectActions;
    if (m_Actions.size() >= count) return;
    const u32 old = static_cast<u32>(m_Actions.size());
    m_Actions.resize(count);
    for (u32 i = old; i < count; ++i) {
        m_Actions[i] = ActionConfig{};
        m_Actions[i].action = static_cast<GameAction>(i);
        m_Actions[i].mode = ActionMode::Press;
    }
    m_ToggleState.resize(count, 0);
    m_ActionDown.resize(count, 0);
    m_ActionPressed.resize(count, 0);
    m_ActionReleased.resize(count, 0);
    m_ActionHeld.resize(count, 0);
    m_ActionValue.resize(count, 0.0f);
    m_TouchDownPrev.resize(count, 0);
    m_LastUsed.resize(count, 0);
    m_ProjectNames.resize(count - kFirstProjectAction);
}

void InputActionMap::SetProjectDefaults(std::function<void(InputActionMap&)> layer) {
    m_ProjectDefaults = std::move(layer);
    RefreshDefaultConfigs();
}

void InputActionMap::RefreshDefaultConfigs() {
    // Build into m_Actions (the layers write there) and swap the live
    // bindings back. Toggle state is per-frame state, not a binding, so it is
    // kept as it was.
    std::vector<ActionConfig> live = m_Actions;
    std::vector<u8> toggles = m_ToggleState;
    LoadTableDefaults();
    if (m_ProjectDefaults) m_ProjectDefaults(*this);
    ApplyPresetLayer();
    m_DefaultConfigs = m_Actions;
    // A project layer can name new slots and grow the map; keep what grew
    if (live.size() < m_Actions.size()) {
        for (usize i = live.size(); i < m_Actions.size(); ++i) live.push_back(m_Actions[i]);
    }
    m_Actions = std::move(live);
    m_ToggleState = std::move(toggles);
    m_ToggleState.resize(m_Actions.size(), 0);
}

const ActionConfig* InputActionMap::GetDefaultConfig(u32 index) const {
    return index < m_DefaultConfigs.size() ? &m_DefaultConfigs[index] : nullptr;
}

void InputActionMap::SetPreset(BindingPreset preset) {
    m_Preset = preset;
    LoadDefaults();
}

void InputActionMap::LoadDefaults() {
    // Preferences outlive a reset. They share ActionConfig with the bindings
    // (sensitivity and invert on the Look actions, hold-or-toggle as the
    // Sprint/Crouch mode), so they are read before the table overwrites them.
    const bool keep = m_DefaultsLoaded;
    const f32 sensitivity = GetMouseSensitivity();
    const bool invertY = GetInvertY();
    const bool sprintToggle = IsSprintToggle();
    const bool crouchToggle = IsCrouchToggle();

    LoadTableDefaults();
    if (m_ProjectDefaults) m_ProjectDefaults(*this);
    ApplyPresetLayer();
    m_DefaultConfigs = m_Actions;

    if (keep) {
        SetMouseSensitivity(sensitivity);
        SetInvertY(invertY);
        SetSprintToggle(sprintToggle);
        SetCrouchToggle(crouchToggle);
    }
    m_DefaultsLoaded = true;
}

void InputActionMap::LoadTableDefaults() {
    const u32 count = static_cast<u32>(m_Actions.size());
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
    // Project actions have no engine defaults; the project layer binds them.
    for (u32 i = 0; i < count; ++i) {
        const ActionInfo& info = GetActionInfo(static_cast<GameAction>(i));
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
    EnsureActionCount(static_cast<u32>(action) + 1);
    if (!IsValidAction(static_cast<i32>(action))) return;
    m_Actions[static_cast<u32>(action)].bindings.push_back(binding);
}

void InputActionMap::ClearBindings(GameAction action) {
    if (!IsValidAction(static_cast<i32>(action))) return;
    m_Actions[static_cast<u32>(action)].bindings.clear();
}

void InputActionMap::SetCustomActionName(GameAction action, const std::string& name) {
    const u32 i = static_cast<u32>(action);
    if (i < kFirstProjectAction) return;
    SetProjectAction(static_cast<i32>(i - kFirstProjectAction), name);
}

i32 InputActionMap::SetProjectAction(i32 slot, const std::string& name) {
    if (slot < 0 || slot >= static_cast<i32>(kMaxProjectActions)) return -1;
    const u32 id = kFirstProjectAction + static_cast<u32>(slot);
    EnsureActionCount(id + 1);
    m_ProjectNames[static_cast<usize>(slot)] = name;
    return static_cast<i32>(id);
}

void InputActionMap::ClearProjectActionNames() {
    for (auto& n : m_ProjectNames) n.clear();
    for (u32 i = kFirstProjectAction; i < m_Actions.size(); ++i) m_Actions[i].bindings.clear();
}

i32 InputActionMap::DefineProjectAction(const std::string& name) {
    if (name.empty()) return -1;
    for (usize s = 0; s < m_ProjectNames.size(); ++s) {
        if (m_ProjectNames[s] == name) return static_cast<i32>(kFirstProjectAction + s);
    }
    for (usize s = 0; s < m_ProjectNames.size(); ++s) {
        if (m_ProjectNames[s].empty()) return SetProjectAction(static_cast<i32>(s), name);
    }
    return SetProjectAction(static_cast<i32>(m_ProjectNames.size()), name);
}

i32 InputActionMap::FindAction(const std::string& name) const {
    if (name.empty()) return -1;
    for (i32 a = 0; a < GetActionCount(); ++a) {
        if (!IsActionListed(a)) continue;
        if (name == GetActionName(a)) return a;
    }
    return -1;
}

bool InputActionMap::IsActionListed(i32 index) const {
    if (!IsValidAction(index)) return false;
    const u32 i = static_cast<u32>(index);
    if (i < kFirstProjectAction) return true;
    return !m_ProjectNames[i - kFirstProjectAction].empty();
}

void InputActionMap::Update(f32 dt) {
    (void)dt;
    const u32 count = static_cast<u32>(m_Actions.size());
    // The usage clock runs in gameplay only, so a paused game keeps its list
    if (Input::IsGameplayFocused()) ++m_UsageClock;

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
        // A touch control holding this action, whatever its bindings are
        const bool touchDown = Input::IsTouchActionDown(static_cast<int>(i));
        if (touchDown) anyDown = true;
        if (touchDown && !m_TouchDownPrev[i]) anyPressed = true;
        if (!touchDown && m_TouchDownPrev[i]) anyReleased = true;
        m_TouchDownPrev[i] = touchDown ? 1 : 0;
        m_ActionHeld[i] = anyDown ? 1 : 0;

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
    MarkUsed(action);
    if (!IsValidAction(static_cast<i32>(action)) || !ActionPassesFocus(action)) return false;
    return m_ActionDown[static_cast<u32>(action)] != 0;
}

bool InputActionMap::IsActionPressed(GameAction action) const {
    MarkUsed(action);
    if (!IsValidAction(static_cast<i32>(action)) || !ActionPassesFocus(action)) return false;
    return m_ActionPressed[static_cast<u32>(action)] != 0;
}

void InputActionMap::MarkUsed(GameAction action) const {
    const u32 i = static_cast<u32>(action);
    if (i < m_LastUsed.size()) m_LastUsed[i] = m_UsageClock;
}

bool InputActionMap::IsActionUsed(i32 index) const {
    if (!IsValidAction(index)) return false;
    const u32 last = m_LastUsed[static_cast<u32>(index)];
    return last != 0 && m_UsageClock - last <= kUsageWindow;
}

bool InputActionMap::AnyGameplayActionUsed() const {
    for (i32 i = 0; i < GetActionCount(); ++i) {
        if (GetActionCategory(i) != static_cast<i32>(ActionCategory::UI) && IsActionUsed(i)) return true;
    }
    return false;
}

void InputActionMap::ClearActionUsage() {
    std::fill(m_LastUsed.begin(), m_LastUsed.end(), 0u);
}

// Not stamped: it is the one read that happens in every menu of every game
// (Start closes what it opened), which says nothing about what the game uses
bool InputActionMap::IsActionPressedAnyFocus(GameAction action) const {
    if (!IsValidAction(static_cast<i32>(action))) return false;
    return m_ActionPressed[static_cast<u32>(action)] != 0;
}

bool InputActionMap::IsActionHeld(GameAction action) const {
    MarkUsed(action);
    if (!IsValidAction(static_cast<i32>(action)) || !ActionPassesFocus(action)) return false;
    return m_ActionHeld[static_cast<u32>(action)] != 0;
}

bool InputActionMap::IsActionReleased(GameAction action) const {
    MarkUsed(action);
    if (!IsValidAction(static_cast<i32>(action)) || !ActionPassesFocus(action)) return false;
    return m_ActionReleased[static_cast<u32>(action)] != 0;
}

f32 InputActionMap::GetActionValue(GameAction action) const {
    MarkUsed(action);
    if (!IsValidAction(static_cast<i32>(action)) || !ActionPassesFocus(action)) return 0.0f;
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
    EnsureActionCount(static_cast<u32>(action) + 1);
    if (!IsValidAction(static_cast<i32>(action))) return;
    auto& cfg = m_Actions[static_cast<u32>(action)];
    if (bindingIndex < cfg.bindings.size()) {
        cfg.bindings[bindingIndex] = binding;
    } else {
        cfg.bindings.push_back(binding);
    }
}

void InputActionMap::SetActionMode(GameAction action, ActionMode mode) {
    EnsureActionCount(static_cast<u32>(action) + 1);
    if (!IsValidAction(static_cast<i32>(action))) return;
    m_Actions[static_cast<u32>(action)].mode = mode;
    // Reset toggle state when changing modes
    m_ToggleState[static_cast<u32>(action)] = false;
}

void InputActionMap::SetSensitivity(GameAction action, f32 sensitivity) {
    if (!IsValidAction(static_cast<i32>(action))) return;
    m_Actions[static_cast<u32>(action)].sensitivity = sensitivity;
}

const ActionConfig& InputActionMap::GetActionConfig(GameAction action) const {
    static const ActionConfig kNone{};
    if (!IsValidAction(static_cast<i32>(action))) return kNone;
    return m_Actions[static_cast<u32>(action)];
}

ActionConfig& InputActionMap::GetActionConfig(GameAction action) {
    EnsureActionCount(static_cast<u32>(action) + 1);
    if (!IsValidAction(static_cast<i32>(action))) {
        static ActionConfig scratch{};
        scratch = ActionConfig{};
        return scratch;
    }
    return m_Actions[static_cast<u32>(action)];
}

namespace {
    bool IsKeyIn(i32 code, std::initializer_list<KeyCode> keys) {
        for (KeyCode k : keys) if (static_cast<i32>(k) == code) return true;
        return false;
    }

    void SetKeys(ActionConfig& cfg, std::initializer_list<KeyCode> keys, bool keepMouse) {
        std::vector<InputBinding> kept;
        for (const auto& b : cfg.bindings) {
            if (b.type == BindingType::GamepadButton || b.type == BindingType::GamepadAxis) kept.push_back(b);
            else if (keepMouse && b.type == BindingType::MouseButton) kept.push_back(b);
        }
        std::vector<InputBinding> out;
        for (KeyCode k : keys) {
            InputBinding b;
            b.type = BindingType::Key;
            b.code = static_cast<i32>(k);
            out.push_back(b);
        }
        out.insert(out.end(), kept.begin(), kept.end());
        cfg.bindings = std::move(out);
    }

    struct PresetRow { GameAction action; std::initializer_list<KeyCode> keys; };
}

const char* GetBindingPresetName(BindingPreset preset) {
    switch (preset) {
        case BindingPreset::LeftHand:    return "LeftHand";
        case BindingPreset::RightHand:   return "RightHand";
        case BindingPreset::GamepadOnly: return "GamepadOnly";
        default:                         return "None";
    }
}

BindingPreset ParseBindingPreset(const std::string& name) {
    if (name == "LeftHand")    return BindingPreset::LeftHand;
    if (name == "RightHand")   return BindingPreset::RightHand;
    if (name == "GamepadOnly") return BindingPreset::GamepadOnly;
    return BindingPreset::None;
}

bool IsLeftHandKey(i32 code) {
    if (code == static_cast<i32>(KeyCode::Space)) return true;
    return IsKeyIn(code, {
        KeyCode::Escape, KeyCode::GraveAccent, KeyCode::Num1, KeyCode::Num2, KeyCode::Num3, KeyCode::Num4, KeyCode::Num5,
        KeyCode::Tab, KeyCode::Q, KeyCode::W, KeyCode::E, KeyCode::R, KeyCode::T,
        KeyCode::CapsLock, KeyCode::A, KeyCode::S, KeyCode::D, KeyCode::F, KeyCode::G,
        KeyCode::LeftShift, KeyCode::Z, KeyCode::X, KeyCode::C, KeyCode::V, KeyCode::B,
        KeyCode::LeftControl, KeyCode::LeftAlt, KeyCode::LeftSuper,
        KeyCode::F1, KeyCode::F2, KeyCode::F3, KeyCode::F4, KeyCode::F5 });
}

bool IsRightHandKey(i32 code) {
    if (code == static_cast<i32>(KeyCode::Space)) return true;
    return IsKeyIn(code, {
        KeyCode::Num6, KeyCode::Num7, KeyCode::Num8, KeyCode::Num9, KeyCode::Num0, KeyCode::Minus, KeyCode::Equal, KeyCode::Backspace,
        KeyCode::Y, KeyCode::U, KeyCode::I, KeyCode::O, KeyCode::P, KeyCode::LeftBracket, KeyCode::RightBracket, KeyCode::Backslash,
        KeyCode::H, KeyCode::J, KeyCode::K, KeyCode::L, KeyCode::Semicolon, KeyCode::Apostrophe, KeyCode::Enter,
        KeyCode::N, KeyCode::M, KeyCode::Comma, KeyCode::Period, KeyCode::Slash, KeyCode::RightShift,
        KeyCode::RightAlt, KeyCode::RightControl, KeyCode::RightSuper,
        KeyCode::Up, KeyCode::Down, KeyCode::Left, KeyCode::Right,
        KeyCode::Insert, KeyCode::Delete, KeyCode::Home, KeyCode::End, KeyCode::PageUp, KeyCode::PageDown,
        KeyCode::KP0, KeyCode::KP1, KeyCode::KP2, KeyCode::KP3, KeyCode::KP4, KeyCode::KP5, KeyCode::KP6,
        KeyCode::KP7, KeyCode::KP8, KeyCode::KP9, KeyCode::KPDecimal, KeyCode::KPDivide, KeyCode::KPMultiply,
        KeyCode::KPSubtract, KeyCode::KPAdd, KeyCode::KPEnter, KeyCode::KPEqual,
        KeyCode::F6, KeyCode::F7, KeyCode::F8, KeyCode::F9, KeyCode::F10, KeyCode::F11, KeyCode::F12 });
}

void InputActionMap::ApplyPresetLayer() {
    using GA = GameAction;
    using K = KeyCode;
    const u32 count = static_cast<u32>(m_Actions.size());

    if (m_Preset == BindingPreset::GamepadOnly) {
        // Gameplay reads the pad alone. Menu and dialogue actions keep their
        // keys: switch-access devices and key-emulating hardware send Enter
        // and Space, and without Confirm nobody could leave the menu that
        // turned this on. Touch no longer needs a key (Input::IsTouchActionDown).
        for (u32 i = 0; i < count; ++i) {
            if (GetActionCategory(static_cast<i32>(i)) == static_cast<i32>(ActionCategory::UI)) continue;
            auto& cfg = m_Actions[i];
            std::vector<InputBinding> pad;
            for (const auto& b : cfg.bindings) {
                if (b.type == BindingType::GamepadButton || b.type == BindingType::GamepadAxis) pad.push_back(b);
            }
            cfg.bindings = std::move(pad);
        }
        return;
    }
    if (m_Preset != BindingPreset::LeftHand && m_Preset != BindingPreset::RightHand) return;

    // Every engine action gets a full row, so nothing is left on the other
    // hand. Pad bindings stay: the preset is about which hand is on the keyboard.
    const bool left = m_Preset == BindingPreset::LeftHand;
    const PresetRow leftRows[] = {
        { GA::MoveForward, {K::W} }, { GA::MoveBack, {K::S} }, { GA::MoveLeft, {K::A} }, { GA::MoveRight, {K::D} },
        { GA::Jump, {K::Space} }, { GA::Sprint, {K::LeftShift} }, { GA::Crouch, {K::LeftControl, K::C} },
        { GA::Dash, {K::LeftAlt} }, { GA::Interact, {K::G} }, { GA::Attack, {K::Z} }, { GA::Block, {K::X} },
        { GA::Pause, {K::Escape} },
        { GA::LookUp, {K::R} }, { GA::LookDown, {K::F} }, { GA::LookLeft, {K::Q} }, { GA::LookRight, {K::E} },
        { GA::CameraZoomIn, {K::T} }, { GA::CameraZoomOut, {K::B} },
        { GA::UIConfirm, {K::Space} }, { GA::UICancel, {K::Escape} },
        { GA::UINavUp, {K::W} }, { GA::UINavDown, {K::S} }, { GA::UINavLeft, {K::A} }, { GA::UINavRight, {K::D} },
        { GA::DialogueAdvance, {K::Space} },
    };
    const PresetRow rightRows[] = {
        { GA::MoveForward, {K::Up, K::KP8} }, { GA::MoveBack, {K::Down, K::KP2} },
        { GA::MoveLeft, {K::Left, K::KP4} }, { GA::MoveRight, {K::Right, K::KP6} },
        { GA::Jump, {K::KP0, K::RightControl} }, { GA::Sprint, {K::RightShift} }, { GA::Crouch, {K::KP1} },
        { GA::Dash, {K::KP3} }, { GA::Interact, {K::KP7} }, { GA::Attack, {K::KP9} }, { GA::Block, {K::KPAdd} },
        { GA::Pause, {K::P} },
        { GA::LookUp, {K::Home} }, { GA::LookDown, {K::End} }, { GA::LookLeft, {K::Delete} }, { GA::LookRight, {K::PageDown} },
        { GA::CameraZoomIn, {K::PageUp} }, { GA::CameraZoomOut, {K::Insert} },
        { GA::UIConfirm, {K::Enter, K::KPEnter} }, { GA::UICancel, {K::Backspace} },
        { GA::UINavUp, {K::Up} }, { GA::UINavDown, {K::Down} }, { GA::UINavLeft, {K::Left} }, { GA::UINavRight, {K::Right} },
        { GA::DialogueAdvance, {K::Enter, K::KPEnter} },
    };
    // Left Hand is mouse-free; Right Hand keeps the mouse, which is on that side
    for (const PresetRow& row : left ? leftRows : rightRows) {
        const u32 i = static_cast<u32>(row.action);
        if (i < count) SetKeys(m_Actions[i], row.keys, !left);
    }

    // Project actions: drop keys from the other side, and give any action left
    // with no key the next free key on this side
    const std::initializer_list<K> leftPool  = { K::Num1, K::Num2, K::Num3, K::Num4, K::Num5, K::V, K::Tab,
                                                 K::GraveAccent, K::CapsLock, K::F1, K::F2, K::F3, K::F4, K::F5 };
    const std::initializer_list<K> rightPool = { K::KP5, K::KPDecimal, K::KPDivide, K::KPMultiply, K::KPSubtract,
                                                 K::Num6, K::Num7, K::Num8, K::Num9, K::Num0, K::Y, K::U, K::I, K::O,
                                                 K::H, K::J, K::K, K::L, K::N, K::M };
    auto used = [&](i32 code) {
        for (const auto& cfg : m_Actions) {
            if (GetActionCategory(static_cast<i32>(cfg.action)) == static_cast<i32>(ActionCategory::UI)) continue;
            for (const auto& b : cfg.bindings) if (b.type == BindingType::Key && b.code == code) return true;
        }
        return false;
    };
    for (u32 i = kFirstProjectAction; i < count; ++i) {
        if (!IsActionListed(static_cast<i32>(i))) continue;
        auto& cfg = m_Actions[i];
        std::vector<InputBinding> kept;
        bool hasKey = false;
        for (const auto& b : cfg.bindings) {
            if (b.type == BindingType::Key) {
                if (!(left ? IsLeftHandKey(b.code) : IsRightHandKey(b.code))) continue;
                hasKey = true;
            } else if (b.type == BindingType::MouseButton && left) {
                continue;
            }
            kept.push_back(b);
        }
        cfg.bindings = std::move(kept);
        if (hasKey) continue;
        for (K k : left ? leftPool : rightPool) {
            if (used(static_cast<i32>(k))) continue;
            InputBinding b;
            b.type = BindingType::Key;
            b.code = static_cast<i32>(k);
            cfg.bindings.insert(cfg.bindings.begin(), b);
            break;
        }
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
    // Hold means hold. It used to set Press, and first person crouch toggled
    // on every press either way, so the menu's "Hold" was a toggle (IN-4)
    SetActionMode(GameAction::Crouch, toggle ? ActionMode::Toggle : ActionMode::Hold);
}

bool InputActionMap::IsBindingCodeValid(BindingType type, i32 code) {
    switch (type) {
        case BindingType::Key:
            // KeyCode is GLFW-style and spans Space(32)..Menu(348). Anything
            // outside that can never match a real press.
            return code >= static_cast<i32>(KeyCode::Space) &&
                   code <= static_cast<i32>(KeyCode::Menu);
        case BindingType::MouseButton:
            return code >= 0 && code < 8;
        case BindingType::GamepadButton:
        case BindingType::GamepadAxis:
            return code >= 0 && code < 64;
    }
    return false;
}

u32 InputActionMap::DropInvalidBindings() {
    // Repairs bindings that can never fire.
    //
    // PollNextKeyPress used to scan the ImGuiKey range and return the ImGuiKey
    // itself. That range INCLUDES ImGuiKey_MouseLeft, so clicking while a
    // rebind prompt was open stored a Key binding with a code near 656 -- a
    // value no key press can equal. Those bindings were then written to
    // bindings.json / browser storage and restored on every boot, so the action
    // stayed permanently dead and re-binding it looked like it did nothing.
    //
    // The poll is fixed, but the saved data is not, and a player has no way to
    // know their file is poisoned. Anything out of range is dropped on load; an
    // action left with nothing gets its defaults back rather than staying unbound.
    u32 repaired = 0;
    const u32 count = static_cast<u32>(m_Actions.size());
    for (u32 i = 0; i < count; ++i) {
        auto& cfg = m_Actions[i];
        const usize before = cfg.bindings.size();
        cfg.bindings.erase(
            std::remove_if(cfg.bindings.begin(), cfg.bindings.end(),
                           [](const InputBinding& b) {
                               return !IsBindingCodeValid(b.type, b.code);
                           }),
            cfg.bindings.end());
        if (cfg.bindings.size() == before) continue;
        repaired += static_cast<u32>(before - cfg.bindings.size());

        // Back to what the game, its project layer and the preset give it
        if (cfg.bindings.empty()) {
            if (const ActionConfig* def = GetDefaultConfig(i)) cfg.bindings = def->bindings;
        }
    }
    if (repaired > 0) {
        ENJIN_LOG_WARN(Core, "Dropped %u unusable input binding(s) from saved data "
                       "(codes outside the valid range); affected actions restored to defaults",
                       repaired);
    }
    return repaired;
}

i32 InputActionMap::PollNextKeyPress() const {
    // Scans the ENGINE key range and returns an engine KeyCode.
    //
    // This used to walk ImGuiKey_NamedKey_BEGIN..END and return the ImGuiKey
    // itself, with a comment claiming it was "a direct mapping" to a GLFW code.
    // It is not: KeyCode is GLFW-style (Space = 32, A = 65, Escape = 256) while
    // ImGuiKey named keys start at 512. Every rebind therefore stored a code no
    // real key press could ever equal, so rebinding silently did nothing --
    // for every key, not just the exotic ones.
    //
    // Reading engine input rather than ImGui also means this works with no ImGui
    // frame open, which is what the web player's UICanvas controls screen needs.
    for (i32 k = static_cast<i32>(KeyCode::Space); k <= static_cast<i32>(KeyCode::Menu); ++k) {
        if (Input::IsKeyPressed(static_cast<KeyCode>(k))) return k;
    }
    return -1;
}

i32 InputActionMap::PollNextMouseButton() const {
    for (i32 b = static_cast<i32>(MouseButton::Left);
         b <= static_cast<i32>(MouseButton::Button6); ++b) {
        if (Input::IsMouseButtonPressed(static_cast<MouseButton>(b))) return b;
    }
    return -1;
}

void InputActionMap::RebindAction(i32 actionIndex, i32 keyCode) {
    RebindAction(actionIndex, BindingType::Key, keyCode);
}

void InputActionMap::RebindAction(i32 actionIndex, BindingType type, i32 code) {
    if (!IsValidAction(actionIndex)) return;
    // A pad button goes to the pad side. An axis needs a direction, so it
    // comes through RebindGamepad with a whole binding.
    if (type == BindingType::GamepadButton) {
        InputBinding b;
        b.type = type;
        b.code = code;
        RebindGamepad(actionIndex, b);
        return;
    }
    if (type != BindingType::Key && type != BindingType::MouseButton) return;
    // A code that cannot fire is worse than no rebind: the screen would show it
    // and the action would be silently dead.
    if (!IsBindingCodeValid(type, code)) {
        ENJIN_LOG_WARN(Core, "Refused rebind: code %d is not valid for that input kind", code);
        return;
    }
    auto& cfg = m_Actions[actionIndex];

    // Rebinding REPLACES: after binding Jump to the left mouse button, Space
    // must stop jumping. An earlier version replaced only the binding of the
    // same kind, reasoning that a mouse binding should not cost you the key --
    // but that is not what "rebind" means to the person doing it, and it left
    // the old key quietly working while the controls screen claimed otherwise.
    //
    // Gamepad and axis bindings are deliberately kept: they are a separate
    // column in the action table and a separate control on the screen, so a
    // keyboard rebind must not silently unbind the pad.
    std::vector<InputBinding> kept;
    kept.reserve(cfg.bindings.size() + 1);
    for (const auto& b : cfg.bindings) {
        if (b.type == BindingType::GamepadButton || b.type == BindingType::GamepadAxis) {
            kept.push_back(b);
        }
    }
    InputBinding nb;
    nb.type = type;
    nb.code = code;
    kept.insert(kept.begin(), nb);
    cfg.bindings.swap(kept);
}

void InputActionMap::RebindGamepad(i32 actionIndex, const InputBinding& binding) {
    if (!IsValidAction(actionIndex)) return;
    if (binding.type != BindingType::GamepadButton && binding.type != BindingType::GamepadAxis) return;
    if (!IsBindingCodeValid(binding.type, binding.code)) {
        ENJIN_LOG_WARN(Core, "Refused pad rebind: code %d is not valid for that input kind", binding.code);
        return;
    }
    auto& cfg = m_Actions[actionIndex];
    std::vector<InputBinding> kept;
    for (const auto& b : cfg.bindings) {
        if (b.type == BindingType::Key || b.type == BindingType::MouseButton) kept.push_back(b);
    }
    kept.push_back(binding);
    cfg.bindings.swap(kept);
}

bool InputActionMap::PollNextGamepadInput(InputBinding& out) const {
    for (i32 gp = 0; gp < 4; ++gp) {
        if (!Input::IsGamepadConnected(gp)) continue;
        for (i32 b = 0; b <= static_cast<i32>(GamepadButton::DPadLeft); ++b) {
            if (!Input::IsGamepadButtonPressed(static_cast<GamepadButton>(b), gp)) continue;
            out = InputBinding{};
            out.type = BindingType::GamepadButton;
            out.code = b;
            return true;
        }
        // Triggers rest at -1: past half pulled counts
        for (i32 a = 4; a <= 5; ++a) {
            if (Input::GetGamepadAxis(static_cast<GamepadAxis>(a), gp) <= 0.5f) continue;
            out = InputBinding{};
            out.type = BindingType::GamepadAxis;
            out.code = a;
            out.axisPositive = true;
            out.axisThreshold = 0.3f;
            return true;
        }
        for (i32 a = 0; a <= 3; ++a) {
            const f32 v = Input::GetGamepadAxis(static_cast<GamepadAxis>(a), gp);
            if (std::fabs(v) <= 0.7f) continue;
            out = InputBinding{};
            out.type = BindingType::GamepadAxis;
            out.code = a;
            out.axisPositive = v > 0.0f;
            out.axisThreshold = 0.5f;
            return true;
        }
    }
    return false;
}

namespace {
    bool SameInput(const InputBinding& a, const InputBinding& b) {
        if (a.type != b.type || a.code != b.code) return false;
        return a.type != BindingType::GamepadAxis || a.axisPositive == b.axisPositive;
    }
    bool IsMenuCategory(i32 category) { return category == static_cast<i32>(ActionCategory::UI); }
}

std::vector<i32> InputActionMap::FindConflicts(i32 actionIndex, const InputBinding& binding) const {
    std::vector<i32> out;
    if (!IsValidAction(actionIndex)) return out;
    const bool menu = IsMenuCategory(GetActionCategory(actionIndex));
    for (i32 i = 0; i < GetActionCount(); ++i) {
        if (i == actionIndex || !IsActionListed(i)) continue;
        if (IsMenuCategory(GetActionCategory(i)) != menu) continue;
        for (const auto& b : m_Actions[static_cast<u32>(i)].bindings) {
            if (SameInput(b, binding)) { out.push_back(i); break; }
        }
    }
    return out;
}

std::vector<i32> InputActionMap::FindConflicts(i32 actionIndex) const {
    std::vector<i32> out;
    if (!IsValidAction(actionIndex)) return out;
    for (const auto& b : m_Actions[static_cast<u32>(actionIndex)].bindings) {
        for (i32 other : FindConflicts(actionIndex, b)) {
            if (std::find(out.begin(), out.end(), other) == out.end()) out.push_back(other);
        }
    }
    return out;
}

i32 InputActionMap::GetActionCount() const {
    return static_cast<i32>(m_Actions.size());
}

const char* InputActionMap::GetActionName(i32 index) const {
    if (!IsValidAction(index)) return "";
    const u32 i = static_cast<u32>(index);
    if (i >= kFirstProjectAction) {
        const std::string& name = m_ProjectNames[i - kFirstProjectAction];
        if (!name.empty()) return name.c_str();
        if (i < static_cast<u32>(GameAction::Count)) return kActionInfo[i].name;   // "Custom 3"
        return "";
    }
    return kActionInfo[i].name;
}

const char* GetKeyDisplayName(i32 code) {
    if (code >= 65 && code <= 90) { static char buf[2]; buf[0] = (char)code; buf[1] = 0; return buf; }
    if (code >= 48 && code <= 57) { static char buf[2]; buf[0] = (char)code; buf[1] = 0; return buf; }
    if (code >= 290 && code <= 301) { static char buf[4]; snprintf(buf, sizeof(buf), "F%d", code - 289); return buf; }
    if (code >= 320 && code <= 329) { static char buf[4]; snprintf(buf, sizeof(buf), "KP%d", code - 320); return buf; }
    switch (code) {
        case 32:  return "Space";
        case 39:  return "'";
        case 44:  return ",";
        case 45:  return "-";
        case 46:  return ".";
        case 47:  return "/";
        case 59:  return ";";
        case 61:  return "=";
        case 91:  return "[";
        case 92:  return "\\";
        case 93:  return "]";
        case 96:  return "`";
        case 256: return "Escape";
        case 257: return "Enter";
        case 258: return "Tab";
        case 259: return "Backspace";
        case 260: return "Insert";
        case 261: return "Delete";
        case 262: return "Right";
        case 263: return "Left";
        case 264: return "Down";
        case 265: return "Up";
        case 266: return "Page Up";
        case 267: return "Page Down";
        case 268: return "Home";
        case 269: return "End";
        case 280: return "Caps Lock";
        case 330: return "KP.";
        case 331: return "KP/";
        case 332: return "KP*";
        case 333: return "KP-";
        case 334: return "KP+";
        case 335: return "KP Enter";
        case 336: return "KP=";
        case 340: return "L.Shift";
        case 341: return "L.Ctrl";
        case 342: return "L.Alt";
        case 343: return "L.Super";
        case 344: return "R.Shift";
        case 345: return "R.Ctrl";
        case 346: return "R.Alt";
        case 347: return "R.Super";
        default: break;
    }
    static char fallback[16];
    snprintf(fallback, sizeof(fallback), "Key %d", code);
    return fallback;
}

const char* GetMouseButtonDisplayName(i32 code) {
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

const char* GetGamepadButtonDisplayName(i32 code, Input::GamepadFamily family) {
    // Buttons are POSITIONS in the standard mapping (0 = south, 1 = east,
    // 2 = west, 3 = north). Each family names the same position differently.
    static const char* const kXbox[] = {
        "A", "B", "X", "Y", "LB", "RB", "Back", "Start", "Guide", "LS", "RS",
        "D-Up", "D-Right", "D-Down", "D-Left" };
    static const char* const kPlayStation[] = {
        "Cross", "Circle", "Square", "Triangle", "L1", "R1", "Share", "Options", "PS", "L3", "R3",
        "D-Up", "D-Right", "D-Down", "D-Left" };
    static const char* const kNintendo[] = {
        "B", "A", "Y", "X", "L", "R", "Minus", "Plus", "Home", "LS", "RS",
        "D-Up", "D-Right", "D-Down", "D-Left" };
    if (code < 0 || code > 14) return "?";
    switch (family) {
        case Input::GamepadFamily::PlayStation: return kPlayStation[code];
        case Input::GamepadFamily::Nintendo:    return kNintendo[code];
        default:                                return kXbox[code];
    }
}

const char* GetGamepadAxisDisplayName(i32 code, bool positive, Input::GamepadFamily family) {
    switch (code) {
        case 0: return positive ? "LS Right" : "LS Left";
        case 1: return positive ? "LS Down" : "LS Up";
        case 2: return positive ? "RS Right" : "RS Left";
        case 3: return positive ? "RS Down" : "RS Up";
        case 4: return family == Input::GamepadFamily::PlayStation ? "L2"
                     : family == Input::GamepadFamily::Nintendo ? "ZL" : "LT";
        case 5: return family == Input::GamepadFamily::PlayStation ? "R2"
                     : family == Input::GamepadFamily::Nintendo ? "ZR" : "RT";
        default: break;
    }
    return "?";
}

Input::GamepadFamily GetActiveGamepadFamily() {
    return Input::GetGamepadFamily(Input::GetLastGamepadIndex());
}

namespace {
    const char* KeyCodeToName(i32 code) { return GetKeyDisplayName(code); }
    const char* MouseButtonToName(i32 code) { return GetMouseButtonDisplayName(code); }
    const char* PadBindingName(const InputBinding& b) {
        const Input::GamepadFamily f = GetActiveGamepadFamily();
        return b.type == BindingType::GamepadButton ? GetGamepadButtonDisplayName(b.code, f)
                                                     : GetGamepadAxisDisplayName(b.code, b.axisPositive, f);
    }
}

std::string InputActionMap::ResolvePromptText(const std::string& text) const {
    // Nothing to do for authored text that names no action.
    if (text.find('{') == std::string::npos) return text;

    std::string out;
    out.reserve(text.size() + 16);

    usize i = 0;
    while (i < text.size()) {
        const usize open = text.find('{', i);
        if (open == std::string::npos) { out.append(text, i, std::string::npos); break; }
        const usize close = text.find('}', open + 1);
        if (close == std::string::npos) { out.append(text, i, std::string::npos); break; }

        out.append(text, i, open - i);
        const std::string token = text.substr(open + 1, close - open - 1);

        // Match on the action's NAME, so a renamed custom action still resolves
        // and a game can write {Fire} as readily as {Interact}.
        bool resolved = false;
        for (i32 a = 0; a < GetActionCount(); ++a) {
            const char* name = GetActionName(a);
            if (!name || token != name) continue;
            const char* binding = GetBindingDisplayName(a);
            if (binding && *binding) {
                out += binding;
                resolved = true;
            }
            break;
        }
        // An unbound action, or an unknown token, keeps its braces. A player
        // seeing "Press {Interact}" has an unbound action to fix; a player
        // seeing "Press  " has nothing to go on.
        if (!resolved) out.append(text, open, close - open + 1);

        i = close + 1;
    }
    return out;
}

const char* InputActionMap::GetKeyboardBindingDisplayName(i32 index) const {
    if (!IsValidAction(index)) return "";
    for (const auto& b : m_Actions[index].bindings) {
        if (b.type == BindingType::Key) return KeyCodeToName(b.code);
        if (b.type == BindingType::MouseButton) return MouseButtonToName(b.code);
    }
    return "";
}

const char* InputActionMap::GetBindingDisplayName(i32 index) const {
    if (!IsValidAction(index)) return "";
    // On a pad, name the pad's button: a player holding a controller was told
    // to press "E" (IN-5)
    if (Input::GetLastDevice() == Input::InputDevice::Gamepad) {
        const char* pad = GetGamepadBindingDisplayName(index);
        if (pad && *pad) return pad;
    }
    const char* key = GetKeyboardBindingDisplayName(index);
    if (key && *key) return key;
    // Fallback to gamepad if no keyboard/mouse binding
    const char* pad = GetGamepadBindingDisplayName(index);
    return (pad && *pad) ? pad : "None";
}

const char* InputActionMap::GetGamepadBindingDisplayName(i32 index) const {
    if (!IsValidAction(index)) return "";
    for (const auto& b : m_Actions[index].bindings) {
        if (b.type == BindingType::GamepadButton || b.type == BindingType::GamepadAxis) return PadBindingName(b);
    }
    return "";
}

i32 InputActionMap::GetActionCategory(i32 index) const {
    if (!IsValidAction(index)) return static_cast<i32>(ActionCategory::UI);
    return static_cast<i32>(GetActionInfo(static_cast<GameAction>(index)).category);
}

namespace {
    bool SameBindings(const std::vector<InputBinding>& a, const std::vector<InputBinding>& b) {
        if (a.size() != b.size()) return false;
        for (usize i = 0; i < a.size(); ++i) {
            if (a[i].type != b[i].type || a[i].code != b[i].code ||
                a[i].axisPositive != b[i].axisPositive ||
                std::fabs(a[i].axisThreshold - b[i].axisThreshold) > 1e-6f) return false;
        }
        return true;
    }

    json BindingToJson(const InputBinding& b) {
        json bj;
        bj["type"] = static_cast<u32>(b.type);
        bj["code"] = b.code;
        bj["axisThreshold"] = b.axisThreshold;
        bj["axisPositive"] = b.axisPositive;
        return bj;
    }
}

bool InputActionMap::IsActionChanged(i32 index) const {
    if (!IsValidAction(index)) return false;
    const auto& cfg = m_Actions[static_cast<u32>(index)];
    const ActionConfig* def = GetDefaultConfig(static_cast<u32>(index));
    if (!def) return !cfg.bindings.empty();
    return cfg.mode != def->mode || cfg.invertAxis != def->invertAxis ||
           std::fabs(cfg.sensitivity - def->sensitivity) > 1e-6f ||
           !SameBindings(cfg.bindings, def->bindings);
}

std::string InputActionMap::ToJson() const {
    json j;
    j["version"] = kBindingsVersion;
    j["preset"] = GetBindingPresetName(m_Preset);
    json actions = json::array();
    const u32 count = static_cast<u32>(m_Actions.size());
    for (u32 i = 0; i < count; ++i) {
        if (!IsActionChanged(static_cast<i32>(i))) continue;
        // An unnamed project slot is nothing the player could have changed
        if (i >= kFirstProjectAction && m_ProjectNames[i - kFirstProjectAction].empty()) continue;
        const auto& cfg = m_Actions[i];
        json actionJson;
        actionJson["action"] = i;
        // Project actions carry their name, so a save still finds them if the
        // project reorders its slots
        if (i >= kFirstProjectAction) actionJson["name"] = m_ProjectNames[i - kFirstProjectAction];
        actionJson["mode"] = static_cast<u32>(cfg.mode);
        actionJson["sensitivity"] = cfg.sensitivity;
        actionJson["invertAxis"] = cfg.invertAxis;
        json bindingsJson = json::array();
        for (const auto& b : cfg.bindings) bindingsJson.push_back(BindingToJson(b));
        actionJson["bindings"] = bindingsJson;
        actions.push_back(actionJson);
    }
    j["actions"] = actions;
    return j.dump(2);
}

bool InputActionMap::FromJson(const std::string& jsonStr) {
    try {
        json j = json::parse(jsonStr);
        const json* actions = nullptr;
        const bool legacy = j.is_array();
        if (legacy) {
            actions = &j;   // version 1: every action, no preset
        } else if (j.is_object()) {
            const u32 version = j.value("version", 0u);
            if (version > kBindingsVersion) {
                ENJIN_LOG_WARN(Core, "Input bindings were saved by a newer version (%u); reading what this one understands", version);
            }
            // The preset first: the player's changes are relative to it
            m_Preset = (j.contains("preset") && j["preset"].is_string())
                           ? ParseBindingPreset(j["preset"].get<std::string>()) : BindingPreset::None;
            LoadDefaults();
            if (j.contains("actions") && j["actions"].is_array()) actions = &j["actions"];
        } else {
            return false;
        }
        if (!actions) { DropInvalidBindings(); return true; }

        for (const auto& actionJson : *actions) {
            if (!actionJson.is_object() || !actionJson.contains("action") ||
                !actionJson["action"].is_number_unsigned()) continue;   // skip, do not abort
            u32 idx = actionJson["action"].get<u32>();
            if (idx >= kFirstProjectAction) {
                // A project action goes to wherever its name lives now, and
                // one with no name was never the player's: in an old file it is
                // an unnamed slot, and loading it would overwrite whatever
                // default the game has since given that slot (IN-11)
                if (!actionJson.contains("name") || !actionJson["name"].is_string()) continue;
                const i32 byName = FindAction(actionJson["name"].get<std::string>());
                if (byName < static_cast<i32>(kFirstProjectAction)) continue;
                idx = static_cast<u32>(byName);
            }
            if (idx >= static_cast<u32>(m_Actions.size())) continue;

            auto& cfg = m_Actions[idx];
            cfg.mode = static_cast<ActionMode>(actionJson.value("mode", 0u));
            cfg.sensitivity = actionJson.value("sensitivity", 1.0f);
            cfg.invertAxis = actionJson.value("invertAxis", false);

            cfg.bindings.clear();
            if (actionJson.contains("bindings") && actionJson["bindings"].is_array()) {
                for (const auto& bj : actionJson["bindings"]) {
                    if (!bj.is_object()) continue;
                    InputBinding b;
                    b.type = static_cast<BindingType>(bj.value("type", 0u));
                    b.code = bj.value("code", 0);
                    b.axisThreshold = bj.value("axisThreshold", 0.5f);
                    b.axisPositive = bj.value("axisPositive", true);
                    cfg.bindings.push_back(b);
                }
            }
        }
        DropInvalidBindings();
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
