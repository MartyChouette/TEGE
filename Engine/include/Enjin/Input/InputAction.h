#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Input.h"
#include "Enjin/Math/Vector.h"
#include <vector>
#include <string>

namespace Enjin {
namespace InputSystem {

// Semantic game actions
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
    Count
};

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

    // Load default bindings (mirrors current WASD+Space+Shift+gamepad)
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

    // Sprint/Crouch toggle convenience
    bool IsSprintToggle() const;
    void SetSprintToggle(bool toggle);
    bool IsCrouchToggle() const;
    void SetCrouchToggle(bool toggle);

    // Rebinding helpers (used by menu UI)
    i32 PollNextKeyPress() const;
    void RebindAction(i32 actionIndex, i32 keyCode);

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
