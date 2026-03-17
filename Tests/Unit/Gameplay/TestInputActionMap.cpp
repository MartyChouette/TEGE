#include "EnjinTest.h"
#include "Enjin/Input/InputAction.h"

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
    ENJIN_EXPECT_EQ((int)GameAction::Count, 18);
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

ENJIN_TEST_MAIN()
