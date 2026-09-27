#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Input.h"
#include "Enjin/Math/Vector.h"
#include <functional>
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
    // Project actions start here. The eight Custom slots are the first eight of
    // them, kept so saved ordinals and scripts that name GameAction::Custom0
    // still resolve; a project can have as many named actions as it wants past
    // them (IN-0). Unnamed ones stay hidden from menus and touch.
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

constexpr u32 kCustomActionCount = 8;    // the legacy slots with enum names
// Engine actions are 0 .. kFirstProjectAction-1; project actions are every id
// from kFirstProjectAction up to InputActionMap::GetActionCount(). Use the
// map's count, never GameAction::Count, as the upper bound of a valid id.
constexpr u32 kFirstProjectAction = static_cast<u32>(GameAction::Custom0);
constexpr u32 kMaxProjectActions = 256;  // a ceiling on a mistake, not a budget

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

// Table lookup. Engine actions get their row; any project action gets the
// shared project row (category Custom, a touch button, Press).
ENJIN_API const ActionInfo& GetActionInfo(GameAction action);
// The C++ enumerator's name ("MoveForward", "Custom3"), for the generated
// script enum; empty for project actions past the legacy slots.
ENJIN_API const char* GetActionIdentifier(GameAction action);

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
// An accessibility preset: a layer between the game's defaults and the
// player's own changes. It is stored by name rather than baked into the
// bindings, so it can be removed, and a game update that changes a default
// still reaches a player using one (IN-23).
enum class BindingPreset : u8 {
    None = 0,
    LeftHand,      // every gameplay action on the left of the keyboard, no mouse
    RightHand,     // every gameplay action on the right of the keyboard and the mouse
    GamepadOnly,   // gameplay reads the pad only; menus, touch and switches still work
};
ENJIN_API const char* GetBindingPresetName(BindingPreset preset);   // "LeftHand", for the save file
ENJIN_API BindingPreset ParseBindingPreset(const std::string& name);  // None for anything unknown
// Which half of the keyboard a key sits on. Space is a thumb key and counts as
// both. Modifiers go by their side. Used by the one-handed presets and their test.
ENJIN_API bool IsLeftHandKey(i32 keyCode);
ENJIN_API bool IsRightHandKey(i32 keyCode);

// The one set of input names (IN-2). The controls hint, prompts, touch, the
// controls menus and the editor's pickers all read these, so a key is called
// the same thing everywhere. Pad names follow the pad's family: the south
// button is A on Xbox, Cross on PlayStation and B on Switch.
ENJIN_API const char* GetKeyDisplayName(i32 keyCode);
ENJIN_API const char* GetMouseButtonDisplayName(i32 button);
ENJIN_API const char* GetGamepadButtonDisplayName(i32 button, Input::GamepadFamily family);
ENJIN_API const char* GetGamepadAxisDisplayName(i32 axis, bool positive, Input::GamepadFamily family);
// The family of the pad the player used last
ENJIN_API Input::GamepadFamily GetActiveGamepadFamily();

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

    // Load default bindings: the ActionInfo table, then the project's own
    // layer (SetProjectDefaults) on top. The player's preferences -- mouse
    // sensitivity, invert-Y, sprint and crouch hold-or-toggle -- are kept: a
    // reset or a one-handed preset is about which input does what, not how
    // looking feels.
    void LoadDefaults();

    // The project's defaults on top of the engine table, re-applied by every
    // LoadDefaults: custom action names and bindings from Project Settings.
    // InputProjectSettings::ApplyTo installs it. Without it, Reset and every
    // one-handed preset unbound the game's own custom actions, and the runtimes
    // saved that, so they were gone for good (IN-10).
    void SetProjectDefaults(std::function<void(InputActionMap&)> layer);

    // Per-frame update: manages toggle state, reads hardware input
    void Update(f32 dt);

    // Query action state
    bool IsActionDown(GameAction action) const;
    bool IsActionPressed(GameAction action) const;
    bool IsActionReleased(GameAction action) const;
    // Pressed this frame whatever the input focus. For the one case focus
    // cannot answer: the Pause binding that opened a menu (Start on a pad) has
    // to close it again, while the menu holds focus and gameplay actions,
    // Pause among them, read inactive.
    bool IsActionPressedAnyFocus(GameAction action) const;
    f32 GetActionValue(GameAction action) const;

    // Convenience: returns normalized 2D movement vector from Forward/Back/Left/Right
    Math::Vector2 GetMovementVector() const;

    // Remapping
    void SetBinding(GameAction action, u32 bindingIndex, const InputBinding& binding);
    void AddBinding(GameAction action, const InputBinding& binding);
    void ClearBindings(GameAction action);
    void SetActionMode(GameAction action, ActionMode mode);
    void SetSensitivity(GameAction action, f32 sensitivity);

    // Accessibility presets. Choosing one rebuilds the defaults with the preset
    // on top and drops the player's own changes, as the buttons always did;
    // choosing None removes it. The preset is saved by name in bindings.json.
    void SetPreset(BindingPreset preset);
    BindingPreset GetPreset() const { return m_Preset; }
    // What a preset button does: on, or off again if it is already on
    void TogglePreset(BindingPreset preset) { SetPreset(m_Preset == preset ? BindingPreset::None : preset); }
    void ApplyLeftHandOnly()  { SetPreset(BindingPreset::LeftHand); }
    void ApplyRightHandOnly() { SetPreset(BindingPreset::RightHand); }
    void ApplyGamepadOnly()   { SetPreset(BindingPreset::GamepadOnly); }
    // Everything back to the game's defaults: no preset, no player changes
    void ResetToDefaults() { SetPreset(BindingPreset::None); }
    // Whether an action differs from what the defaults and the preset give it:
    // the player's layer. Only these are written to bindings.json.
    bool IsActionChanged(i32 index) const;

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
    void RebindAction(i32 actionIndex, i32 keyCode);   // keyboard, kept for scripts
    // Rebind to a key OR a mouse button. Replaces the existing binding of the
    // same kind, so binding a mouse button does not drop the keyboard one.
    void RebindAction(i32 actionIndex, BindingType type, i32 code);

    // Engine key code pressed this frame, or -1. Returns a KeyCode, not an
    // ImGuiKey -- see the implementation for why that distinction cost every
    // rebind its effect.
    i32 PollNextMouseButton() const;

    // Pad rebinding (IN-17). Replaces the action's pad bindings and keeps its
    // keys and mouse buttons, the mirror of a keyboard rebind.
    void RebindGamepad(i32 actionIndex, const InputBinding& binding);
    // A pad button pressed this frame, or a trigger or stick pushed well past
    // half, on any connected pad. Axes have no edge: a screen capturing one
    // should wait for the pad to be at rest first.
    bool PollNextGamepadInput(InputBinding& out) const;

    // Other actions that share an input with this one in the same context
    // (IN-9): gameplay actions (movement, actions, camera, the project's own)
    // with each other, menu actions with menu actions. Space on Jump and on
    // Confirm is fine; Left Shift on Sprint and on Dash is not.
    std::vector<i32> FindConflicts(i32 actionIndex, const InputBinding& binding) const;
    std::vector<i32> FindConflicts(i32 actionIndex) const;   // over all its bindings

    // Whether a code can ever match a real input of that kind.
    static bool IsBindingCodeValid(BindingType type, i32 code);
    // Drop bindings that can never fire and restore defaults for any action left
    // with none. Run automatically by FromJson; returns how many were dropped.
    u32 DropInvalidBindings();

    // Project actions. A project names as many as it wants, each in a stable
    // SLOT (id = kFirstProjectAction + slot), so saved bindings keep pointing at
    // the same action when others are added or removed. Names survive
    // ResetToDefaults (they describe the game, not the player's bindings).
    // SetCustomActionName names any project id, growing the map to reach it.
    void SetCustomActionName(GameAction action, const std::string& name);
    // Name the project action in `slot`; returns its id, or -1 past the ceiling
    i32 SetProjectAction(i32 slot, const std::string& name);
    // The named project action, or a new one in the lowest free slot (scripts)
    i32 DefineProjectAction(const std::string& name);
    // Unname every project action. A project load starts here, so an action
    // deleted from Project Settings stops existing instead of staying named
    // and bound (IN-13). Not part of Reset: a script's Define survives that.
    void ClearProjectActionNames();
    // Any action by display name, engine or project; -1 when there is none
    i32 FindAction(const std::string& name) const;
    bool IsProjectAction(i32 index) const { return index >= static_cast<i32>(kFirstProjectAction) && index < GetActionCount(); }
    bool IsValidAction(i32 index) const { return index >= 0 && index < GetActionCount(); }
    // Whether menus / hints should list this action: every engine action, and
    // project actions that have a name.
    bool IsActionListed(i32 index) const;

    // Whether the game is reading this action (IN-37, IN-38). Every query
    // (IsActionDown, Pressed, Released, GetActionValue) stamps the action, so
    // a controller with dash turned on, a script polling "Grapple" and an
    // Action Trigger all count, and an action nothing reads does not. The
    // hint, How to Play and the touch buttons list what is used, not what a
    // preset guessed. "Recently" is counted in gameplay frames: the clock
    // stops while a menu has focus, so pausing does not make everything unused.
    bool IsActionUsed(i32 index) const;
    bool AnyGameplayActionUsed() const;   // any non-menu action
    void ClearActionUsage();              // a new scene, or a test
    static constexpr u32 kUsageWindow = 120;   // gameplay frames

    // Display helpers
    i32 GetActionCount() const;
    const char* GetActionName(i32 index) const;
    // The binding a prompt should name: the pad's button when the player is on
    // a pad (IN-5), the key or mouse button otherwise
    const char* GetBindingDisplayName(i32 index) const;
    const char* GetKeyboardBindingDisplayName(i32 index) const;   // key or mouse, whatever the device
    const char* GetGamepadBindingDisplayName(i32 index) const;
    i32 GetActionCategory(i32 index) const;

    // Substitute live bindings into authored prompt text.
    //
    // A prompt that names a key starts lying the moment someone rebinds, and
    // four component defaults shipped saying "Press E" while the engine knew
    // the real binding all along. Authored text uses a token -- "Press
    // {Interact} to open" -- and this replaces it with whatever Interact is
    // bound to right now.
    //
    // Unknown tokens are left alone rather than blanked: a prompt reading
    // "Press {Frobnicate}" is a visible authoring mistake, where an empty
    // string is an invisible one. Legacy text with no token is returned
    // untouched, so old scenes keep working and simply stay stale.
    std::string ResolvePromptText(const std::string& text) const;

    // Persistence. bindings.json holds a version, the preset, and ONLY the
    // actions the player changed (IN-11). Anything the player never touched
    // follows the game's defaults, so an action added or rebound by a game
    // update reaches existing players. The old format -- a bare array with
    // every action -- still loads.
    static constexpr u32 kBindingsVersion = 2;
    std::string ToJson() const;
    bool FromJson(const std::string& jsonStr);

private:
    void LoadTableDefaults();   // the ActionInfo table alone
    void ApplyPresetLayer();    // m_Preset on top of whatever m_Actions holds
    // Rebuild m_DefaultConfigs (table + project + preset) without touching the
    // live bindings. Run when the project layer changes.
    void RefreshDefaultConfigs();
    const ActionConfig* GetDefaultConfig(u32 index) const;
    void EnsureActionCount(u32 count);   // grow every per-action array to count
    bool IsBindingActive(const InputBinding& binding) const;
    bool IsBindingPressed(const InputBinding& binding) const;
    bool IsBindingReleased(const InputBinding& binding) const;

    // One entry per action id, engine actions first. Grown, never shrunk, as
    // project actions are named (the legacy eight slots always exist).
    std::vector<ActionConfig> m_Actions;
    std::vector<std::string> m_ProjectNames;   // indexed by slot
    std::function<void(InputActionMap&)> m_ProjectDefaults;
    bool m_DefaultsLoaded = false;   // the first load has no preferences to keep
    BindingPreset m_Preset = BindingPreset::None;
    // What the defaults and the preset give each action; the player layer is
    // whatever differs from this
    std::vector<ActionConfig> m_DefaultConfigs;
    std::vector<u8> m_TouchDownPrev;   // last frame's Input::IsTouchActionDown, for edges
    // Usage stamps: the gameplay-frame clock at each action's last query
    mutable std::vector<u32> m_LastUsed;
    u32 m_UsageClock = 1;
    void MarkUsed(GameAction action) const;

    // Toggle state tracking
    std::vector<u8> m_ToggleState;

    // Per-action state (computed each frame)
    std::vector<u8> m_ActionDown;
    std::vector<u8> m_ActionPressed;
    std::vector<u8> m_ActionReleased;
    std::vector<f32> m_ActionValue;
};

} // namespace InputSystem
} // namespace Enjin
