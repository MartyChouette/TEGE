#include "EnjinTest.h"
#include "Enjin/Input/InputAction.h"

#include <string>

using namespace Enjin;
using namespace Enjin::InputSystem;

// ===========================================================================
// Construction & Defaults
// ===========================================================================

ENJIN_TEST(Defaults, ActionCountIs18) {
    InputActionMap map;
    ENJIN_EXPECT_EQ(map.GetActionCount(), 18);
}

ENJIN_TEST(Defaults, AllActionNamesNonNull) {
    InputActionMap map;
    for (i32 i = 0; i < map.GetActionCount(); ++i) {
        const char* name = map.GetActionName(i);
        ENJIN_EXPECT_NOT_NULL(name);
        ENJIN_EXPECT_GT(strlen(name), (size_t)0);
    }
}

ENJIN_TEST(Defaults, AllBindingDisplayNamesNonNull) {
    InputActionMap map;
    for (i32 i = 0; i < map.GetActionCount(); ++i) {
        const char* name = map.GetBindingDisplayName(i);
        ENJIN_EXPECT_NOT_NULL(name);
    }
}

ENJIN_TEST(Defaults, MovementActionsAreHoldMode) {
    InputActionMap map;
    ENJIN_EXPECT_EQ((int)map.GetActionConfig(GameAction::MoveForward).mode, (int)ActionMode::Hold);
    ENJIN_EXPECT_EQ((int)map.GetActionConfig(GameAction::MoveBack).mode, (int)ActionMode::Hold);
    ENJIN_EXPECT_EQ((int)map.GetActionConfig(GameAction::MoveLeft).mode, (int)ActionMode::Hold);
    ENJIN_EXPECT_EQ((int)map.GetActionConfig(GameAction::MoveRight).mode, (int)ActionMode::Hold);
}

ENJIN_TEST(Defaults, JumpIsPressMode) {
    InputActionMap map;
    ENJIN_EXPECT_EQ((int)map.GetActionConfig(GameAction::Jump).mode, (int)ActionMode::Press);
}

ENJIN_TEST(Defaults, SprintIsHoldByDefault) {
    InputActionMap map;
    ENJIN_EXPECT_EQ((int)map.GetActionConfig(GameAction::Sprint).mode, (int)ActionMode::Hold);
    ENJIN_EXPECT_FALSE(map.IsSprintToggle());
}

ENJIN_TEST(Defaults, DefaultSensitivityIsOne) {
    InputActionMap map;
    for (i32 i = 0; i < map.GetActionCount(); ++i) {
        ENJIN_EXPECT_FLOAT_EQ(map.GetActionConfig((GameAction)i).sensitivity, 1.0f);
    }
}

ENJIN_TEST(Defaults, AllActionsHaveBindings) {
    InputActionMap map;
    for (i32 i = 0; i < map.GetActionCount(); ++i) {
        auto& config = map.GetActionConfig((GameAction)i);
        ENJIN_EXPECT_GT(config.bindings.size(), (size_t)0);
    }
}

ENJIN_TEST(Defaults, ActionCategories) {
    InputActionMap map;
    // Movement actions = category 0
    ENJIN_EXPECT_EQ(map.GetActionCategory(0), 0); // MoveForward
    // Look actions = category 2
    ENJIN_EXPECT_EQ(map.GetActionCategory((i32)GameAction::LookUp), 2);
}

// ===========================================================================
// Configuration Mutation
// ===========================================================================

ENJIN_TEST(Config, SetSensitivity) {
    InputActionMap map;
    map.SetSensitivity(GameAction::Attack, 2.5f);
    ENJIN_EXPECT_FLOAT_EQ(map.GetActionConfig(GameAction::Attack).sensitivity, 2.5f);
}

ENJIN_TEST(Config, SetActionMode) {
    InputActionMap map;
    map.SetActionMode(GameAction::Sprint, ActionMode::Toggle);
    ENJIN_EXPECT_EQ((int)map.GetActionConfig(GameAction::Sprint).mode, (int)ActionMode::Toggle);
    ENJIN_EXPECT_TRUE(map.IsSprintToggle());
}

ENJIN_TEST(Config, SprintToggleSwitch) {
    InputActionMap map;
    ENJIN_EXPECT_FALSE(map.IsSprintToggle());
    map.SetSprintToggle(true);
    ENJIN_EXPECT_TRUE(map.IsSprintToggle());
    map.SetSprintToggle(false);
    ENJIN_EXPECT_FALSE(map.IsSprintToggle());
}

ENJIN_TEST(Config, CrouchToggleSwitch) {
    InputActionMap map;
    ENJIN_EXPECT_FALSE(map.IsCrouchToggle());
    map.SetCrouchToggle(true);
    ENJIN_EXPECT_TRUE(map.IsCrouchToggle());
}

ENJIN_TEST(Config, MouseSensitivity) {
    InputActionMap map;
    map.SetMouseSensitivity(2.0f);
    ENJIN_EXPECT_FLOAT_EQ(map.GetMouseSensitivity(), 2.0f);
}

ENJIN_TEST(Config, ResetToDefaults) {
    InputActionMap map;
    map.SetSensitivity(GameAction::Attack, 5.0f);
    map.SetSprintToggle(true);
    map.ResetToDefaults();
    ENJIN_EXPECT_FLOAT_EQ(map.GetActionConfig(GameAction::Attack).sensitivity, 1.0f);
    ENJIN_EXPECT_FALSE(map.IsSprintToggle());
}

// ===========================================================================
// Presets
// ===========================================================================

ENJIN_TEST(Presets, LeftHandOnly) {
    InputActionMap map;
    map.ApplyLeftHandOnly();
    // Should still have 18 actions
    ENJIN_EXPECT_EQ(map.GetActionCount(), 18);
    // Movement should still have bindings
    ENJIN_EXPECT_GT(map.GetActionConfig(GameAction::MoveForward).bindings.size(), (size_t)0);
}

ENJIN_TEST(Presets, RightHandOnly) {
    InputActionMap map;
    map.ApplyRightHandOnly();
    ENJIN_EXPECT_EQ(map.GetActionCount(), 18);
    ENJIN_EXPECT_GT(map.GetActionConfig(GameAction::MoveForward).bindings.size(), (size_t)0);
}

ENJIN_TEST(Presets, GamepadOnly) {
    InputActionMap map;
    map.ApplyGamepadOnly();
    ENJIN_EXPECT_EQ(map.GetActionCount(), 18);
    // All bindings should be gamepad types
    for (i32 i = 0; i < map.GetActionCount(); ++i) {
        auto& config = map.GetActionConfig((GameAction)i);
        for (auto& binding : config.bindings) {
            bool isGamepad = (binding.type == BindingType::GamepadButton ||
                              binding.type == BindingType::GamepadAxis);
            ENJIN_EXPECT_TRUE(isGamepad);
        }
    }
}

// ===========================================================================
// JSON Serialization
// ===========================================================================

ENJIN_TEST(JSON, RoundTripPreservesActions) {
    InputActionMap map;
    map.SetSensitivity(GameAction::Attack, 3.0f);
    map.SetSprintToggle(true);
    map.SetMouseSensitivity(1.5f);

    std::string json = map.ToJson();
    ENJIN_EXPECT_GT(json.size(), (size_t)0);

    InputActionMap map2;
    ENJIN_EXPECT_TRUE(map2.FromJson(json));
    ENJIN_EXPECT_FLOAT_EQ(map2.GetActionConfig(GameAction::Attack).sensitivity, 3.0f);
    ENJIN_EXPECT_TRUE(map2.IsSprintToggle());
}

ENJIN_TEST(JSON, EmptyJsonReturnsFalse) {
    InputActionMap map;
    ENJIN_EXPECT_FALSE(map.FromJson(""));
}

ENJIN_TEST(JSON, InvalidJsonReturnsFalse) {
    InputActionMap map;
    ENJIN_EXPECT_FALSE(map.FromJson("{{{not valid json"));
}

ENJIN_TEST(JSON, OutputIsValidJSON) {
    InputActionMap map;
    std::string json = map.ToJson();
    // Should start with [ (array of actions)
    ENJIN_EXPECT_GT(json.size(), (size_t)2);
    ENJIN_EXPECT_EQ(json[0], '[');
    ENJIN_EXPECT_EQ(json[json.size() - 1], ']');
}

ENJIN_TEST(JSON, PresetSurvivesRoundTrip) {
    InputActionMap map;
    map.ApplyGamepadOnly();
    std::string json = map.ToJson();

    InputActionMap map2;
    ENJIN_EXPECT_TRUE(map2.FromJson(json));

    // Verify gamepad-only survived
    for (i32 i = 0; i < map2.GetActionCount(); ++i) {
        auto& config = map2.GetActionConfig((GameAction)i);
        for (auto& binding : config.bindings) {
            bool isGamepad = (binding.type == BindingType::GamepadButton ||
                              binding.type == BindingType::GamepadAxis);
            ENJIN_EXPECT_TRUE(isGamepad);
        }
    }
}

// ===========================================================================
// Remapping
// ===========================================================================

ENJIN_TEST(Remap, SetBindingChangesKey) {
    InputActionMap map;
    InputBinding newBinding;
    newBinding.type = BindingType::Key;
    newBinding.code = 90; // Z key
    map.SetBinding(GameAction::Jump, 0, newBinding);

    auto& config = map.GetActionConfig(GameAction::Jump);
    ENJIN_EXPECT_EQ((int)config.bindings[0].type, (int)BindingType::Key);
    ENJIN_EXPECT_EQ(config.bindings[0].code, 90);
}

ENJIN_TEST(Remap, RebindActionChangesFirstKey) {
    InputActionMap map;
    map.RebindAction((i32)GameAction::Jump, 88); // X key
    auto& config = map.GetActionConfig(GameAction::Jump);
    ENJIN_EXPECT_EQ(config.bindings[0].code, 88);
}

ENJIN_TEST(Remap, RemapSurvivesJsonRoundTrip) {
    InputActionMap map;
    map.RebindAction((i32)GameAction::Interact, 70); // F key
    std::string json = map.ToJson();

    InputActionMap map2;
    ENJIN_EXPECT_TRUE(map2.FromJson(json));
    auto& config = map2.GetActionConfig(GameAction::Interact);
    ENJIN_EXPECT_EQ(config.bindings[0].code, 70);
}

// ===========================================================================
// GameAction enum
// ===========================================================================

ENJIN_TEST(GameActionEnum, CountIs18) {
    ENJIN_EXPECT_EQ((int)GameAction::Count, 18);
}

ENJIN_TEST(GameActionEnum, ValuesAreSequential) {
    ENJIN_EXPECT_EQ((int)GameAction::MoveForward, 0);
    ENJIN_EXPECT_EQ((int)GameAction::MoveBack, 1);
    ENJIN_EXPECT_EQ((int)GameAction::MoveLeft, 2);
    ENJIN_EXPECT_EQ((int)GameAction::MoveRight, 3);
    ENJIN_EXPECT_EQ((int)GameAction::Jump, 4);
    ENJIN_EXPECT_EQ((int)GameAction::Sprint, 5);
    ENJIN_EXPECT_EQ((int)GameAction::Pause, 11);
}

ENJIN_TEST_MAIN()
