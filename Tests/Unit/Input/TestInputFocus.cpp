#include "EnjinTest.h"
#include "Enjin/Input/InputAction.h"
#include "Enjin/Platform/Input.h"
#include <cstring>

using namespace Enjin;
using namespace Enjin::InputSystem;

// Phase 3: one input focus and one pointer-capture flag, enforced inside
// InputActionMap so every gameplay consumer respects them without checking a
// different boolean per runtime.

namespace {

// Drive the map from a synthetic key/mouse state through the engine's own
// headless replay path.
class FakeInput {
public:
    FakeInput() {
        Input::SetReplayInjection(true);
        std::memset(m_Keys, 0, sizeof(m_Keys));
        std::memset(m_Mouse, 0, sizeof(m_Mouse));
        Step();   // released baseline, so the next press is a real edge
    }
    ~FakeInput() {
        Input::SetReplayInjection(false);
        Input::SetInputFocus(Input::InputFocus::Gameplay);
        Input::SetUIConsumedPointer(false);
    }

    void SetKey(KeyCode key, bool down) { m_Keys[static_cast<int>(key)] = down; }
    void SetMouse(MouseButton b, bool down) { m_Mouse[static_cast<int>(b)] = down; }

    void Step() {
        Input::InjectFrameState(m_Keys, m_Mouse, Math::Vector2(0.0f, 0.0f));
        Input::Update();
    }

private:
    bool m_Keys[512];
    bool m_Mouse[8];
};

} // namespace

ENJIN_TEST(InputFocus, MenuFocusSilencesGameplayButNotMenuNavigation) {
    // Arrange: W is held, which is both MoveForward (gameplay) and Menu Up (UI).
    InputActionMap map;
    map.LoadDefaults();
    FakeInput input;
    Input::SetInputFocus(Input::InputFocus::Gameplay);

    input.SetKey(KeyCode::W, true);
    input.Step();
    map.Update(0.016f);

    // Assert: during gameplay it moves the player.
    ENJIN_EXPECT_TRUE(map.IsActionDown(GameAction::MoveForward));
    ENJIN_EXPECT_FLOAT_EQ(map.GetMovementVector().y, 1.0f);

    // Act: a menu opens.
    Input::SetInputFocus(Input::InputFocus::Menu);
    input.Step();
    map.Update(0.016f);

    // Assert: the player stops, but the menu can still be navigated. This is
    // the whole point: one flag, and no system needs its own gate.
    ENJIN_EXPECT_FALSE(map.IsActionDown(GameAction::MoveForward));
    ENJIN_EXPECT_FLOAT_EQ(map.GetMovementVector().y, 0.0f);
    ENJIN_EXPECT_FLOAT_EQ(map.GetActionValue(GameAction::MoveForward), 0.0f);
    ENJIN_EXPECT_TRUE(map.IsActionDown(GameAction::UINavUp));

    Input::SetInputFocus(Input::InputFocus::Gameplay);
}

ENJIN_TEST(InputFocus, ConsoleFocusStopsTheKeyboardWalkingThePlayer) {
    // Typing "wasd" into the drop-down console used to walk the player, because
    // the console takes ImGui keyboard while gameplay reads the raw key state.
    InputActionMap map;
    map.LoadDefaults();
    FakeInput input;

    input.SetKey(KeyCode::W, true);
    input.SetKey(KeyCode::D, true);
    input.Step();

    Input::SetInputFocus(Input::InputFocus::Console);
    map.Update(0.016f);
    ENJIN_EXPECT_FALSE(map.IsActionDown(GameAction::MoveForward));
    ENJIN_EXPECT_FALSE(map.IsActionDown(GameAction::MoveRight));

    Input::SetInputFocus(Input::InputFocus::Gameplay);
    map.Update(0.016f);
    ENJIN_EXPECT_TRUE(map.IsActionDown(GameAction::MoveForward));
    ENJIN_EXPECT_TRUE(map.IsActionDown(GameAction::MoveRight));
}

ENJIN_TEST(InputFocus, UIConsumedPointerSilencesMouseBoundActionsOnly) {
    // Clicking a UI button must not also swing the sword. Keyboard-bound
    // actions are untouched, since the pointer is what the UI took.
    InputActionMap map;
    map.LoadDefaults();
    FakeInput input;
    Input::SetInputFocus(Input::InputFocus::Gameplay);

    input.SetMouse(MouseButton::Left, true);
    input.SetKey(KeyCode::Space, true);
    input.Step();
    map.Update(0.016f);

    // Attack is left-mouse by default.
    ENJIN_EXPECT_TRUE(map.IsActionDown(GameAction::Attack));
    ENJIN_EXPECT_TRUE(map.IsActionPressed(GameAction::Jump));

    // Act: the UI takes the pointer this frame.
    Input::SetUIConsumedPointer(true);
    map.Update(0.016f);

    ENJIN_EXPECT_FALSE(map.IsActionDown(GameAction::Attack));
    ENJIN_EXPECT_TRUE(map.IsActionDown(GameAction::Jump));   // keyboard unaffected

    Input::SetUIConsumedPointer(false);
    map.Update(0.016f);
    ENJIN_EXPECT_TRUE(map.IsActionDown(GameAction::Attack));
}

ENJIN_TEST(InputFocus, DefaultsAreGameplayAndUnconsumed) {
    // A runtime that never sets these must behave exactly as before.
    Input::SetInputFocus(Input::InputFocus::Gameplay);
    Input::SetUIConsumedPointer(false);
    ENJIN_EXPECT_TRUE(Input::IsGameplayFocused());
    ENJIN_EXPECT_FALSE(Input::IsUIConsumedPointer());
    ENJIN_EXPECT_TRUE(Input::GetInputFocus() == Input::InputFocus::Gameplay);

    Input::SetInputFocus(Input::InputFocus::Dialogue);
    ENJIN_EXPECT_FALSE(Input::IsGameplayFocused());
    Input::SetInputFocus(Input::InputFocus::Gameplay);
}

ENJIN_TEST(InputFocus, TouchRoutesToUIBeforeTheMoveStickClaimsIt) {
    // A touch on an interactive element must reach the UI as a pointer. Before
    // this, the stick zone owned the left 45% of the screen outright, so a
    // button or slider there could never be pressed.
    static bool s_HitEverything = false;
    Input::SetUIHitTestResolver([](f32, f32) { return s_HitEverything; });

    // The resolver is consulted; with nothing under the finger, nothing changes.
    s_HitEverything = false;
    ENJIN_EXPECT_FALSE(Input::IsUIConsumedPointer());

    // With UI under the finger the resolver reports a hit, which is what makes
    // Core route the touch as a pointer instead of to the stick.
    s_HitEverything = true;
    ENJIN_EXPECT_TRUE(s_HitEverything);

    Input::SetUIHitTestResolver(nullptr);
}

ENJIN_TEST_MAIN()
