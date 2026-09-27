#include "EnjinTest.h"
#include "Enjin/Input/InputAction.h"
#include "Enjin/Input/InputProjectSettings.h"

using namespace Enjin;
using namespace Enjin::InputSystem;

// ===========================================================================
// GameAction Enum
// ===========================================================================

ENJIN_TEST(GameActionEnum, Values) {
    ENJIN_EXPECT_EQ((int)GameAction::MoveForward, 0);
    ENJIN_EXPECT_EQ((int)GameAction::MoveBack, 1);
    ENJIN_EXPECT_EQ((int)GameAction::MoveLeft, 2);
    ENJIN_EXPECT_EQ((int)GameAction::MoveRight, 3);
    ENJIN_EXPECT_EQ((int)GameAction::Jump, 4);
    ENJIN_EXPECT_EQ((int)GameAction::Sprint, 5);
    ENJIN_EXPECT_EQ((int)GameAction::Crouch, 6);
    ENJIN_EXPECT_EQ((int)GameAction::Dash, 7);
    ENJIN_EXPECT_EQ((int)GameAction::Interact, 8);
    ENJIN_EXPECT_EQ((int)GameAction::Attack, 9);
    ENJIN_EXPECT_EQ((int)GameAction::Block, 10);
    ENJIN_EXPECT_EQ((int)GameAction::Pause, 11);
}

ENJIN_TEST(GameActionEnum, LookAndCamera) {
    ENJIN_EXPECT_EQ((int)GameAction::LookUp, 12);
    ENJIN_EXPECT_EQ((int)GameAction::LookDown, 13);
    ENJIN_EXPECT_EQ((int)GameAction::LookLeft, 14);
    ENJIN_EXPECT_EQ((int)GameAction::LookRight, 15);
    ENJIN_EXPECT_EQ((int)GameAction::CameraZoomIn, 16);
    ENJIN_EXPECT_EQ((int)GameAction::CameraZoomOut, 17);
}

ENJIN_TEST(GameActionEnum, Count) {
    ENJIN_EXPECT_EQ((int)GameAction::Count, 33);
}

// ===========================================================================
// BindingType Enum
// ===========================================================================

ENJIN_TEST(BindingTypeEnum, Values) {
    ENJIN_EXPECT_EQ((int)BindingType::Key, 0);
    ENJIN_EXPECT_EQ((int)BindingType::MouseButton, 1);
    ENJIN_EXPECT_EQ((int)BindingType::GamepadButton, 2);
    ENJIN_EXPECT_EQ((int)BindingType::GamepadAxis, 3);
}

// ===========================================================================
// ActionMode Enum
// ===========================================================================

ENJIN_TEST(ActionModeEnum, Values) {
    ENJIN_EXPECT_EQ((int)ActionMode::Hold, 0);
    ENJIN_EXPECT_EQ((int)ActionMode::Toggle, 1);
    ENJIN_EXPECT_EQ((int)ActionMode::Press, 2);
    ENJIN_EXPECT_EQ((int)ActionMode::Release, 3);
}

// ===========================================================================
// InputBinding Defaults
// ===========================================================================

ENJIN_TEST(InputBinding, Defaults) {
    InputBinding binding;
    ENJIN_EXPECT_EQ((int)binding.type, (int)BindingType::Key);
    ENJIN_EXPECT_EQ(binding.code, 0);
    ENJIN_EXPECT_FLOAT_EQ(binding.axisThreshold, 0.5f);
    ENJIN_EXPECT_TRUE(binding.axisPositive);
}

// ===========================================================================
// ActionConfig Defaults
// ===========================================================================

ENJIN_TEST(ActionConfig, Defaults) {
    ActionConfig cfg;
    ENJIN_EXPECT_EQ((int)cfg.action, (int)GameAction::MoveForward);
    ENJIN_EXPECT_EQ((int)cfg.mode, (int)ActionMode::Hold);
    ENJIN_EXPECT_EQ(cfg.bindings.size(), (size_t)0);
    ENJIN_EXPECT_FLOAT_EQ(cfg.sensitivity, 1.0f);
    ENJIN_EXPECT_FALSE(cfg.invertAxis);
}

// ===========================================================================
// InputActionMap
// ===========================================================================

ENJIN_TEST(InputActionMap, ActionCount) {
    InputActionMap map;
    ENJIN_EXPECT_EQ(map.GetActionCount(), (i32)GameAction::Count);
}

ENJIN_TEST(InputActionMap, ActionNames) {
    InputActionMap map;
    // Verify first few action names are non-null
    ENJIN_ASSERT_NOT_NULL(map.GetActionName(0));
    ENJIN_ASSERT_NOT_NULL(map.GetActionName(1));
    ENJIN_ASSERT_NOT_NULL(map.GetActionName(2));
}

ENJIN_TEST(InputActionMap, BindingDisplayNames) {
    InputActionMap map;
    // Should not crash for valid indices
    const char* name = map.GetBindingDisplayName(0);
    ENJIN_ASSERT_NOT_NULL(name);
}

ENJIN_TEST(InputActionMap, GetActionConfig) {
    InputActionMap map;
    const ActionConfig& cfg = map.GetActionConfig(GameAction::Jump);
    ENJIN_EXPECT_EQ((int)cfg.action, (int)GameAction::Jump);
}

ENJIN_TEST(InputActionMap, SetSensitivity) {
    InputActionMap map;
    map.SetSensitivity(GameAction::Attack, 2.0f);
    const ActionConfig& cfg = map.GetActionConfig(GameAction::Attack);
    ENJIN_EXPECT_FLOAT_EQ(cfg.sensitivity, 2.0f);
}

ENJIN_TEST(InputActionMap, SetActionMode) {
    InputActionMap map;
    map.SetActionMode(GameAction::Sprint, ActionMode::Toggle);
    const ActionConfig& cfg = map.GetActionConfig(GameAction::Sprint);
    ENJIN_EXPECT_EQ((int)cfg.mode, (int)ActionMode::Toggle);
}

ENJIN_TEST(InputActionMap, SprintToggle) {
    InputActionMap map;
    map.SetSprintToggle(true);
    ENJIN_EXPECT_TRUE(map.IsSprintToggle());
    map.SetSprintToggle(false);
    ENJIN_EXPECT_FALSE(map.IsSprintToggle());
}

ENJIN_TEST(InputActionMap, CrouchToggle) {
    InputActionMap map;
    map.SetCrouchToggle(true);
    ENJIN_EXPECT_TRUE(map.IsCrouchToggle());
    map.SetCrouchToggle(false);
    ENJIN_EXPECT_FALSE(map.IsCrouchToggle());
}

ENJIN_TEST(InputActionMap, MouseSensitivity) {
    InputActionMap map;
    map.SetMouseSensitivity(2.5f);
    ENJIN_EXPECT_FLOAT_EQ(map.GetMouseSensitivity(), 2.5f);
}

ENJIN_TEST(InputActionMap, ToJsonNotEmpty) {
    InputActionMap map;
    std::string json = map.ToJson();
    ENJIN_EXPECT_FALSE(json.empty());
}

ENJIN_TEST(InputActionMap, FromJsonRoundTrip) {
    InputActionMap map;
    map.SetMouseSensitivity(3.0f);
    map.SetSprintToggle(true);
    std::string json = map.ToJson();

    InputActionMap map2;
    bool ok = map2.FromJson(json);
    ENJIN_EXPECT_TRUE(ok);
    ENJIN_EXPECT_FLOAT_EQ(map2.GetMouseSensitivity(), 3.0f);
    ENJIN_EXPECT_TRUE(map2.IsSprintToggle());
}

// IN-10: Reset and every one-handed preset called LoadDefaults, which rebuilt
// the map from the engine table alone. The project's custom actions came out
// unbound, and the player's sensitivity, invert-Y and toggle choices were
// reset, and the runtimes saved that. Defaults are the table PLUS the
// project's layer, and preferences survive.
static bool HasKey(const InputActionMap& map, GameAction a, KeyCode k) {
    for (const auto& b : map.GetActionConfig(a).bindings) {
        if (b.type == BindingType::Key && b.code == static_cast<i32>(k)) return true;
    }
    return false;
}

ENJIN_TEST(InputActionMap, ResetKeepsProjectActionsAndPlayerPreferences) {
    // Arrange: a project custom action, the player's preferences, and one
    // engine action rebound away from its default.
    InputActionMap map;
    InputProjectSettings project;
    CustomActionDef dash;
    dash.slot = 0;
    dash.name = "Dash";
    dash.key = static_cast<i32>(KeyCode::X);
    project.customActions.push_back(dash);
    project.ApplyTo(map);
    map.SetMouseSensitivity(2.5f);
    map.SetInvertY(true);
    map.SetSprintToggle(true);
    map.SetCrouchToggle(true);
    map.RebindAction(static_cast<i32>(GameAction::Jump), static_cast<i32>(KeyCode::J));

    // Act / Assert: Reset.
    map.ResetToDefaults();
    ENJIN_EXPECT_TRUE(HasKey(map, GameAction::Custom0, KeyCode::X));
    ENJIN_EXPECT_TRUE(map.IsActionListed(static_cast<i32>(GameAction::Custom0)));
    ENJIN_EXPECT_FALSE(HasKey(map, GameAction::Jump, KeyCode::J));   // the rebind is undone
    ENJIN_EXPECT_FLOAT_NEAR(map.GetMouseSensitivity(), 2.5f, 1e-6f);
    ENJIN_EXPECT_TRUE(map.GetInvertY());
    ENJIN_EXPECT_TRUE(map.IsSprintToggle());
    ENJIN_EXPECT_TRUE(map.IsCrouchToggle());

    // And a preset, which starts from the same defaults.
    map.ApplyLeftHandOnly();
    ENJIN_EXPECT_TRUE(HasKey(map, GameAction::Custom0, KeyCode::X));
    ENJIN_EXPECT_FLOAT_NEAR(map.GetMouseSensitivity(), 2.5f, 1e-6f);
    ENJIN_EXPECT_TRUE(map.GetInvertY());
}

// IN-0: a project was held to eight numbered Custom slots. It now names as
// many actions as it needs, each in a stable slot, and a player's saved
// bindings find them by NAME, so reordering the project's list moves nothing.
static CustomActionDef MakeAction(i32 slot, const char* name, KeyCode key) {
    CustomActionDef d;
    d.slot = slot;
    d.name = name;
    d.key = static_cast<i32>(key);
    return d;
}

ENJIN_TEST(InputActionMap, ProjectActionsPastTheEightSlots) {
    // Arrange: twelve named actions, the last well past Custom7.
    InputActionMap map;
    InputProjectSettings project;
    for (i32 i = 0; i < 11; ++i) project.customActions.push_back(MakeAction(i, "Filler", KeyCode::F1));
    project.customActions.push_back(MakeAction(40, "Grapple", KeyCode::G));

    // Act
    project.ApplyTo(map);

    // Assert: the action exists, is listed, bound, and found by name.
    const i32 grapple = static_cast<i32>(kFirstProjectAction) + 40;
    ENJIN_ASSERT_TRUE(map.GetActionCount() > grapple);
    ENJIN_EXPECT_TRUE(map.IsProjectAction(grapple));
    ENJIN_EXPECT_TRUE(map.IsActionListed(grapple));
    ENJIN_EXPECT_EQ(map.FindAction("Grapple"), grapple);
    ENJIN_EXPECT_TRUE(HasKey(map, static_cast<GameAction>(grapple), KeyCode::G));
    ENJIN_EXPECT_EQ(std::string(map.GetActionName(grapple)), std::string("Grapple"));
    // An unnamed slot in between is not listed.
    ENJIN_EXPECT_FALSE(map.IsActionListed(static_cast<i32>(kFirstProjectAction) + 20));
    // Engine actions are found by display name too.
    ENJIN_EXPECT_EQ(map.FindAction(GetActionInfo(GameAction::Jump).name), static_cast<i32>(GameAction::Jump));
    ENJIN_EXPECT_EQ(map.FindAction("Nothing Called This"), -1);
}

ENJIN_TEST(InputActionMap, DefineProjectActionReusesOrTakesLowestFreeSlot) {
    InputActionMap map;
    map.SetProjectAction(0, "Taken");
    const i32 a = map.DefineProjectAction("Glide");
    ENJIN_EXPECT_EQ(a, static_cast<i32>(kFirstProjectAction) + 1);
    ENJIN_EXPECT_EQ(map.DefineProjectAction("Glide"), a);   // same name, same action
    ENJIN_EXPECT_EQ(map.DefineProjectAction(""), -1);
    ENJIN_EXPECT_EQ(map.SetProjectAction(static_cast<i32>(kMaxProjectActions), "Too Far"), -1);
}

ENJIN_TEST(InputActionMap, SavedBindingsFollowTheActionNameNotTheSlot) {
    // Arrange: the player rebinds Grapple while it lives in slot 12.
    InputActionMap before;
    before.SetProjectAction(12, "Grapple");
    const i32 oldId = static_cast<i32>(kFirstProjectAction) + 12;
    before.RebindAction(oldId, static_cast<i32>(KeyCode::Q));
    const std::string saved = before.ToJson();

    // Act: a later version of the game moved Grapple to slot 3.
    InputActionMap after;
    after.SetProjectAction(3, "Grapple");
    ENJIN_ASSERT_TRUE(after.FromJson(saved));

    // Assert: the rebind landed on Grapple, and slot 12 stayed empty.
    const i32 newId = static_cast<i32>(kFirstProjectAction) + 3;
    ENJIN_EXPECT_TRUE(HasKey(after, static_cast<GameAction>(newId), KeyCode::Q));
    ENJIN_EXPECT_FALSE(after.IsActionListed(oldId));
}

ENJIN_TEST(InputActionMap, ProjectReloadForgetsDeletedActions) {
    InputActionMap map;
    InputProjectSettings project;
    project.customActions.push_back(MakeAction(5, "Old", KeyCode::O));
    project.ApplyTo(map);
    ENJIN_EXPECT_NE(map.FindAction("Old"), -1);

    project.customActions.clear();
    project.ApplyTo(map);
    ENJIN_EXPECT_EQ(map.FindAction("Old"), -1);
    ENJIN_EXPECT_FALSE(map.IsActionListed(static_cast<i32>(kFirstProjectAction) + 5));
}

ENJIN_TEST(InputActionMap, ActionIdentifiersMatchTheEnum) {
    ENJIN_EXPECT_EQ(std::string(GetActionIdentifier(GameAction::MoveForward)), std::string("MoveForward"));
    ENJIN_EXPECT_EQ(std::string(GetActionIdentifier(GameAction::DialogueAdvance)), std::string("DialogueAdvance"));
    ENJIN_EXPECT_EQ(std::string(GetActionIdentifier(GameAction::Custom7)), std::string("Custom7"));
}

ENJIN_TEST_MAIN()
