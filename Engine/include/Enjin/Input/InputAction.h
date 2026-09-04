#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Input.h"
#include "Enjin/Math/Vector.h"
#include <vector>
#include <string>

namespace Enjin {
namespace InputSystem {

// Semantic game actions. This enum is the spine of the input system: bindings,
// touch buttons, menu navigation, dialogue advance and the controls hint all
// derive from it. Ordinals are persisted in bindings.json and exposed to
// scripts, so ONLY APPEND, never reorder.
enum class GameAction : u32 {
    MoveForward = 0,
    MoveBack,
    MoveLeft,
    MoveRight,
    Jump,
    Sprint,
    Crouch,
    Dash,
    Interact,
    Attack,
    Block,
    Pause,
    LookUp,
    LookDown,
    LookLeft,
    LookRight,
    CameraZoomIn,
    CameraZoomOut,
    // UI / menu navigation (menus, dialogue choices, canvas focus nav)
    UIConfirm,
    UICancel,
    UINavUp,
    UINavDown,
    UINavLeft,
    UINavRight,
    DialogueAdvance,
    // Game-defined slots. A game names them (InputActionMap::SetCustomActionName)
    // and binds them at boot; unnamed slots stay hidden from menus and touch.
    Custom0,
    Custom1,
    Custom2,
    Custom3,
    Custom4,
    Custom5,
    Custom6,
    Custom7,
    Count
};

constexpr u32 kCustomActionCount = 8;

// Menu grouping (GetActionCategory). Ordinals are what GameMenus indexes.
enum class ActionCategory : i32 {
    Movement = 0,
    Actions = 1,
    Camera = 2,
    UI = 3,
    Custom = 4,
    Count
};

// How the mobile touch overlay represents an action when a scene consumes it.
enum class TouchHint : u8 {
    NotShown = 0,   // no on-screen control (e.g. Pause, Block, Crouch)
    Button,         // an anchored action button
    Stick,          // part of the floating move stick
    Look            // the look-drag region
};

// Static description of one GameAction: display name, category, DEFAULT
// bindings (LoadDefaults reads these) and how touch/hints present it.
// -1 in any code field means "none".
struct ActionInfo {
    const char* name;         // "Jump"
    ActionCategory category;
    i32 key1;                 // KeyCode or -1
    i32 key2;                 // KeyCode or -1
    i32 mouse;                // MouseButton or -1
    i32 pad;                  // GamepadButton or -1
    i32 pad2;                 // GamepadButton or -1
    i32 axis;                 // GamepadAxis or -1
    bool axisPositive;
    f32 axisThreshold;
    u32 mode;                 // ActionMode ordinal
    TouchHint touch;
    const char* touchLabel;   // fallback label when no binding label is available
    const char* hintVerb;     // lowercase verb for the controls hint ("jump")
};

// Table lookup, valid for every GameAction below Count.
ENJIN_API const ActionInfo& GetActionInfo(GameAction action);

// Input binding type
enum class BindingType : u32 {
    Key,
    MouseButton,
    GamepadButton,
    GamepadAxis
};

// How an action triggers
enum class ActionMode : u32 {
    Hold = 0,    // Active while held
    Toggle,      // Press once to start, again to stop
    Press,       // Active only on frame pressed
    Release      // Active only on frame released
};

// A single input binding
struct InputBinding {
    BindingType type = BindingType::Key;
    i32 code = 0;           // KeyCode, MouseButton, GamepadButton, or GamepadAxis cast to i32
    f32 axisThreshold = 0.5f; // For axis bindings: value above which triggers
    bool axisPositive = true; // For axis: positive or negative direction
};

// Configuration for a single action
struct ActionConfig {
    GameAction action = GameAction::MoveForward;
    ActionMode mode = ActionMode::Hold;
    std::vector<InputBinding> bindings;
    f32 sensitivity = 1.0f;
    bool invertAxis = false;
};

// Input action map: abstraction between hardware input and gameplay
class ENJIN_API InputActionMap {
public:
    InputActionMap();
    ~InputActionMap() = default;

    // Load default bindings from the ActionInfo table
    void LoadDefaults();

    // Per-frame update: manages toggle state, reads hardware input
    void Update(f32 dt);

    // Query action state
    bool IsActionDown(GameAction action) const;
    bool IsActionPressed(GameAction action) const;
    bool IsActionReleased(GameAction action) const;
    f32 GetActionValue(GameAction action) const;

    // Convenience: returns normalized 2D movement vector from Forward/Back/Left/Right
    Math::Vector2 GetMovementVector() const;

    // Remapping
    void SetBinding(GameAction action, u32 bindingIndex, const InputBinding& binding);
    void AddBinding(GameAction action, const InputBinding& binding);
    void ClearBindings(GameAction action);
    void SetActionMode(GameAction action, ActionMode mode);
    void SetSensitivity(GameAction action, f32 sensitivity);

    // One-handed presets
    void ApplyLeftHandOnly();
    void ApplyRightHandOnly();
    void ApplyGamepadOnly();
    void ResetToDefaults() { LoadDefaults(); }

    // Access config
    const ActionConfig& GetActionConfig(GameAction action) const;
    ActionConfig& GetActionConfig(GameAction action);

    // Mouse sensitivity (applies to all Look actions)
    f32 GetMouseSensitivity() const;
    void SetMouseSensitivity(f32 sens);

    // Invert the vertical look axis. Lives here with sensitivity so both
    // persist in bindings.json and there is one home for "how looking feels".
    bool GetInvertY() const;
    void SetInvertY(bool invert);

    // Sprint/Crouch toggle convenience
    bool IsSprintToggle() const;
    void SetSprintToggle(bool toggle);
    bool IsCrouchToggle() const;
    void SetCrouchToggle(bool toggle);

    // Rebinding helpers (used by menu UI)
    i32 PollNextKeyPress() const;
    void RebindAction(i32 actionIndex, i32 keyCode);

    // Custom action slots: a game names Custom0..7 at boot. Names survive
    // ResetToDefaults (they describe the game, not the player's bindings).
    void SetCustomActionName(GameAction action, const std::string& name);
    // Whether menus / hints should list this action: everything except
    // unnamed Custom slots.
    bool IsActionListed(i32 index) const;

    // Display helpers
    i32 GetActionCount() const;
    const char* GetActionName(i32 index) const;
    const char* GetBindingDisplayName(i32 index) const;
    const char* GetGamepadBindingDisplayName(i32 index) const;
    i32 GetActionCategory(i32 index) const;

    // Persistence
    std::string ToJson() const;
    bool FromJson(const std::string& jsonStr);

private:
    bool IsBindingActive(const InputBinding& binding) const;
    bool IsBindingPressed(const InputBinding& binding) const;
    bool IsBindingReleased(const InputBinding& binding) const;

    ActionConfig m_Actions[static_cast<u32>(GameAction::Count)];
    std::string m_CustomNames[kCustomActionCount];

    // Toggle state tracking
    bool m_ToggleState[static_cast<u32>(GameAction::Count)] = {};

    // Per-action state (computed each frame)
    bool m_ActionDown[static_cast<u32>(GameAction::Count)] = {};
    bool m_ActionPressed[static_cast<u32>(GameAction::Count)] = {};
    bool m_ActionReleased[static_cast<u32>(GameAction::Count)] = {};
    f32  m_ActionValue[static_cast<u32>(GameAction::Count)] = {};
};

} // namespace InputSystem
} // namespace Enjin
