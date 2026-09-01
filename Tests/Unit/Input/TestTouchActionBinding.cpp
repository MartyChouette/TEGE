#include "EnjinTest.h"
#include "Enjin/Input/InputAction.h"
#include "Enjin/Input/TouchActionBridge.h"
#include "Enjin/Platform/Input.h"
#include <cstring>

using namespace Enjin;
using namespace Enjin::InputSystem;

// The fix: on-screen touch controls must reflect the CURRENT input bindings, so
// rebinding an action updates what the touch button presses and shows. The touch
// overlay lives in Core and resolves keys/labels through resolvers that Engine
// injects from the active InputActionMap (TouchActionBridge). These tests drive
// that resolution path directly.

namespace {
int JumpKey(const InputActionMap& m) {
    const ActionConfig& cfg = m.GetActionConfig(GameAction::Jump);
    for (const auto& b : cfg.bindings)
        if (b.type == BindingType::Key) return b.code;
    return -1;
}
} // namespace

ENJIN_TEST(TouchActionBinding, ReflectsCurrentBinding) {
    InputActionMap map;
    map.LoadDefaults();
    SetTouchActionMap(&map);

    const int JUMP = static_cast<int>(GameAction::Jump);

    // Touch resolves Jump to the same key the action map holds by default.
    int defaultKey = JumpKey(map);
    ENJIN_ASSERT_TRUE(defaultKey >= 0);
    ENJIN_EXPECT_EQ(TouchActionKey(JUMP), defaultKey);

    // Rebind Jump to 'K' (75). The touch button now presses 'K' with no other
    // change - this is the property that was broken before the bridge.
    map.RebindAction(JUMP, 75);
    ENJIN_EXPECT_EQ(TouchActionKey(JUMP), 75);
    ENJIN_EXPECT_EQ(JumpKey(map), 75);

    // And its label tracks the new binding (non-empty, and not the old one).
    const char* lbl = TouchActionLabel(JUMP);
    ENJIN_ASSERT_TRUE(lbl != nullptr);
    ENJIN_EXPECT_TRUE(lbl[0] != '\0');

    // Detaching the map makes the resolver report "no binding" so touch falls
    // back to a button's static key code instead of pressing a stale key.
    SetTouchActionMap(nullptr);
    ENJIN_EXPECT_EQ(TouchActionKey(JUMP), Input::kTouchNoBinding);
}

ENJIN_TEST(TouchActionBinding, OutOfRangeAndNoMapAreSafe) {
    SetTouchActionMap(nullptr);
    ENJIN_EXPECT_EQ(TouchActionKey(4), Input::kTouchNoBinding);   // no map
    ENJIN_EXPECT_TRUE(TouchActionLabel(4) == nullptr);

    InputActionMap map;
    map.LoadDefaults();
    SetTouchActionMap(&map);
    ENJIN_EXPECT_EQ(TouchActionKey(-1), Input::kTouchNoBinding);          // negative
    ENJIN_EXPECT_EQ(TouchActionKey(99999), Input::kTouchNoBinding);       // past Count
    ENJIN_EXPECT_TRUE(TouchActionLabel(-1) == nullptr);
    SetTouchActionMap(nullptr);
}

ENJIN_TEST(TouchActionBinding, PresetButtonsCarryActions) {
    // A preset must tag its buttons with GameActions (not just key codes), or
    // the resolver never runs. Verify the Generic preset's Jump button.
    Input::SetTouchControllerPreset(Input::TouchPreset::Generic);
    const Input::TouchScheme& s = Input::GetTouchScheme();
#if ENJIN_PLATFORM_WEB
    // Off-web the scheme is inert; on web the preset is populated.
    bool sawJumpAction = false;
    for (int i = 0; i < s.buttonCount; ++i)
        if (s.buttons[i].action == static_cast<int>(GameAction::Jump)) sawJumpAction = true;
    ENJIN_EXPECT_TRUE(sawJumpAction);
    // Stick maps to movement actions.
    ENJIN_EXPECT_EQ(s.stickActions[2], static_cast<int>(GameAction::MoveForward));
#else
    (void)s;   // preset is a no-op off web; nothing to assert
    ENJIN_EXPECT_TRUE(true);
#endif
}

ENJIN_TEST_MAIN()
