#include "EnjinTest.h"
#include "Enjin/Input/InputAction.h"
#include "Enjin/Input/TouchActionBridge.h"
#include "Enjin/Platform/Input.h"
#include "Enjin/Scripting/ScriptEngine.h"
#include "Enjin/Scripting/ScriptBindings.h"
#include <angelscript.h>
#include <cstring>
#include <utility>
#include <vector>

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

ENJIN_TEST(TouchActionBinding, GamepadOnlyActionHasNoKeyAndNoLabel) {
    // CameraZoomIn defaults to a gamepad button only. The key resolver reports
    // "no binding" (touch falls back to the button's static key), so the label
    // resolver must ALSO decline rather than show the gamepad glyph, or the
    // button lies about what it presses.
    InputActionMap map;
    map.LoadDefaults();
    SetTouchActionMap(&map);
    const int ZOOM = static_cast<int>(GameAction::CameraZoomIn);
    ENJIN_EXPECT_EQ(TouchActionKey(ZOOM), Input::kTouchNoBinding);
    ENJIN_EXPECT_TRUE(TouchActionLabel(ZOOM) == nullptr);

    // Give it a key and both resolvers agree again.
    map.RebindAction(ZOOM, 90);   // 'Z'
    ENJIN_EXPECT_EQ(TouchActionKey(ZOOM), 90);
    ENJIN_EXPECT_TRUE(TouchActionLabel(ZOOM) != nullptr);
    SetTouchActionMap(nullptr);
}

ENJIN_TEST(TouchActionBinding, PresetButtonsCarryActions) {
    // Presets are built in Engine from the action table and install on every
    // platform (desktop/editor simulate the overlay), so this runs in CI.
    ApplyTouchPreset(TouchPreset::ThirdPerson);
    const Input::TouchScheme& s = Input::GetTouchScheme();
    bool sawJump = false, sawInteract = false, sawSprint = false;
    for (int i = 0; i < s.buttonCount; ++i) {
        if (s.buttons[i].action == static_cast<int>(GameAction::Jump))     sawJump = true;
        if (s.buttons[i].action == static_cast<int>(GameAction::Interact)) sawInteract = true;
        if (s.buttons[i].action == static_cast<int>(GameAction::Sprint))   sawSprint = true;
    }
    ENJIN_EXPECT_TRUE(sawJump);
    ENJIN_EXPECT_TRUE(sawInteract);
    ENJIN_EXPECT_TRUE(sawSprint);
    ENJIN_EXPECT_TRUE(s.moveStick);
    ENJIN_EXPECT_TRUE(s.lookRegion);
    // Stick maps to movement actions, with the default keys as fallback.
    ENJIN_EXPECT_EQ(s.stickActions[2], static_cast<int>(GameAction::MoveForward));
    ENJIN_EXPECT_EQ(s.stickKeys[2], static_cast<int>(KeyCode::W));
    ENJIN_EXPECT_EQ(GetActiveTouchPreset() == TouchPreset::ThirdPerson, true);
}

ENJIN_TEST(TouchActionBinding, PresetsOnlyShowConsumedControls) {
    // A 2D platformer has no look region and only left/right on the stick:
    // dragging up must not press W (which some games bind to jump).
    ApplyTouchPreset(TouchPreset::Platformer2D);
    const Input::TouchScheme& p = Input::GetTouchScheme();
    ENJIN_EXPECT_FALSE(p.lookRegion);
    ENJIN_EXPECT_EQ(p.buttonCount, 1);
    ENJIN_EXPECT_EQ(p.buttons[0].action, static_cast<int>(GameAction::Jump));
    ENJIN_EXPECT_EQ(p.stickActions[0], static_cast<int>(GameAction::MoveLeft));
    ENJIN_EXPECT_EQ(p.stickActions[2], -1);
    ENJIN_EXPECT_EQ(p.stickKeys[2], -1);

    // First person: fire button falls back to the LEFT MOUSE button (Core's
    // negative encoding) when no map is wired.
    ApplyTouchPreset(TouchPreset::FirstPerson);
    const Input::TouchScheme& f = Input::GetTouchScheme();
    bool sawFire = false;
    for (int i = 0; i < f.buttonCount; ++i)
        if (f.buttons[i].action == static_cast<int>(GameAction::Attack)) {
            sawFire = true;
            ENJIN_EXPECT_EQ(f.buttons[i].keyCode, -1);
        }
    ENJIN_EXPECT_TRUE(sawFire);
    ENJIN_EXPECT_TRUE(f.buttonCount <= Input::kMaxTouchButtons);
}

ENJIN_TEST(TouchActionBinding, CustomActionLabelIsItsName) {
    InputActionMap map;
    map.LoadDefaults();
    SetTouchActionMap(&map);
    const int c0 = static_cast<int>(GameAction::Custom0);
    // Unnamed + unbound: nothing to press, nothing to show.
    ENJIN_EXPECT_EQ(TouchActionKey(c0), Input::kTouchNoBinding);
    ENJIN_EXPECT_TRUE(TouchActionLabel(c0) == nullptr);
    // Named and bound: the touch button presses the key but shows the NAME.
    map.SetCustomActionName(GameAction::Custom0, "SLO-MO");
    map.RebindAction(c0, static_cast<int>(KeyCode::B));
    ENJIN_EXPECT_EQ(TouchActionKey(c0), static_cast<int>(KeyCode::B));
    ENJIN_ASSERT_TRUE(TouchActionLabel(c0) != nullptr);
    ENJIN_EXPECT_TRUE(std::strcmp(TouchActionLabel(c0), "SLO-MO") == 0);
    SetTouchActionMap(nullptr);
}

// IN-43: a script adding buttons with col 0, row 0, radius 0 -- trusting the
// engine to lay them out, as Potions' comment says -- got every button on the
// same spot. Radius 0 now means the next slot of the engine's own cluster.
ENJIN_TEST(TouchActionBinding, AutoSlotsAreDistinctAndRunOut) {
    // Arrange / Act: every auto slot.
    std::vector<std::pair<f32, f32>> seen;
    for (int i = 0; i < static_cast<int>(Input::kMaxTouchButtons); ++i) {
        f32 col = -1.0f, row = -1.0f, radius = 0.0f;
        ENJIN_ASSERT_TRUE(AutoTouchSlot(i, col, row, radius));
        // Assert: a real size, and no two slots share a spot.
        ENJIN_EXPECT_TRUE(radius > 0.01f && radius < 0.3f);
        for (const auto& p : seen) ENJIN_EXPECT_FALSE(p.first == col && p.second == row);
        seen.push_back({col, row});
    }
    f32 c, r, rad;
    ENJIN_EXPECT_FALSE(AutoTouchSlot(static_cast<int>(Input::kMaxTouchButtons), c, r, rad));
    ENJIN_EXPECT_FALSE(AutoTouchSlot(-1, c, r, rad));
}

ENJIN_TEST(TouchActionBinding, ScriptButtonsWithNoLayoutDoNotStack) {
    // Arrange: a script adding two buttons exactly as Potions does.
    Scripting::ScriptEngine engine;
    ENJIN_ASSERT_TRUE(engine.Initialize());
    Scripting::RegisterAllBindings(engine.GetASEngine());
    ENJIN_ASSERT_TRUE(engine.CompileScriptFromMemory("touch_auto",
        "void Add() {"
        "    Touch_ClearButtons();"
        "    Touch_AddButton(\"A\", 65, 0.0f, 0.0f, 0.0f);"
        "    Touch_AddButton(\"B\", 66, 0.0f, 0.0f, 0.0f);"
        "}"));
    asIScriptFunction* fn = engine.GetASEngine()->GetModule("touch_auto")->GetFunctionByName("Add");
    ENJIN_ASSERT_NOT_NULL(fn);

    // Act
    asIScriptContext* ctx = engine.AcquireContext();
    ctx->Prepare(fn);
    ENJIN_EXPECT_EQ(ctx->Execute(), static_cast<int>(asEXECUTION_FINISHED));
    engine.ReturnContext(ctx);

    // Assert: two buttons, in two places, each a real size.
    const Input::TouchScheme s = Input::GetTouchScheme();
    ENJIN_ASSERT_EQ(s.buttonCount, 2);
    const auto& a = s.buttons[0];
    const auto& b = s.buttons[1];
    ENJIN_EXPECT_FALSE(a.colFromRight == b.colFromRight && a.rowFromBottom == b.rowFromBottom);
    ENJIN_EXPECT_TRUE(a.radiusFrac > 0.01f && b.radiusFrac > 0.01f);

    Input::SetTouchScheme(Input::TouchScheme{});
    engine.Shutdown();
}

ENJIN_TEST_MAIN()
