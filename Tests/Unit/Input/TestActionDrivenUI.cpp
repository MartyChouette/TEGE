#include "EnjinTest.h"
#include "Enjin/Input/InputAction.h"
#include "Enjin/Accessibility/AccessibilitySettings.h"
#include "Enjin/Platform/Input.h"
#include <cstring>
#include <string>

using namespace Enjin;
using namespace Enjin::InputSystem;

// Phase 4: dialogue and menu navigation read ACTIONS, and the accessibility
// settings an exported game ships with come from the project rather than a
// hardcoded file.

namespace {

class FakeInput {
public:
    FakeInput() {
        Input::SetReplayInjection(true);
        std::memset(m_Keys, 0, sizeof(m_Keys));
        std::memset(m_Mouse, 0, sizeof(m_Mouse));
        Step();
    }
    ~FakeInput() {
        Input::SetReplayInjection(false);
        Input::SetInputFocus(Input::InputFocus::Gameplay);
    }
    void SetKey(KeyCode key, bool down) { m_Keys[static_cast<int>(key)] = down; }
    void Step() {
        Input::InjectFrameState(m_Keys, m_Mouse, Math::Vector2(0.0f, 0.0f));
        Input::Update();
    }
private:
    bool m_Keys[512];
    bool m_Mouse[8];
};

} // namespace

ENJIN_TEST(ActionDrivenUI, RebindingDialogueAdvanceMovesTheKeyThatAdvances) {
    // Dialogue used to hardcode Space/Enter, so a player who rebound "advance"
    // still had to press Space. It reads the action now.
    InputActionMap map;
    map.LoadDefaults();
    FakeInput input;

    const auto advance = GameAction::DialogueAdvance;
    input.SetKey(KeyCode::Space, true);
    input.Step();
    map.Update(0.016f);
    ENJIN_EXPECT_TRUE(map.IsActionPressed(advance));

    // Rebind to F and Space stops advancing.
    map.RebindAction(static_cast<i32>(advance), static_cast<i32>(KeyCode::F));
    input.SetKey(KeyCode::Space, false);
    input.Step();
    map.Update(0.016f);
    input.SetKey(KeyCode::Space, true);
    input.Step();
    map.Update(0.016f);
    ENJIN_EXPECT_FALSE(map.IsActionPressed(advance));

    input.SetKey(KeyCode::Space, false);
    input.SetKey(KeyCode::F, true);
    input.Step();
    map.Update(0.016f);
    ENJIN_EXPECT_TRUE(map.IsActionPressed(advance));
}

ENJIN_TEST(ActionDrivenUI, MenuNavigationStaysAliveWhileGameplayIsSilenced) {
    // The menu actions must survive Menu focus, or opening a menu would make it
    // impossible to navigate out of.
    InputActionMap map;
    map.LoadDefaults();
    FakeInput input;

    input.SetKey(KeyCode::Down, true);
    input.SetKey(KeyCode::Enter, true);
    input.Step();

    Input::SetInputFocus(Input::InputFocus::Menu);
    map.Update(0.016f);
    ENJIN_EXPECT_TRUE(map.IsActionPressed(GameAction::UINavDown));
    ENJIN_EXPECT_TRUE(map.IsActionPressed(GameAction::UIConfirm));
    ENJIN_EXPECT_FALSE(map.IsActionDown(GameAction::MoveBack));   // gameplay quiet

    Input::SetInputFocus(Input::InputFocus::Gameplay);
}

ENJIN_TEST(ActionDrivenUI, AccessibilitySettingsRoundTripThroughOneSerializer) {
    // BuildPipeline ships the project's accessibility defaults using this
    // serializer, and the players load them with the same one, so "what the
    // editor configured" and "what the game loads" cannot drift apart.
    Accessibility::RuntimeAccessibilitySettings s;
    s.colorblindMode = Accessibility::ColorblindMode::Deuteranopia;
    s.reducedMotion = true;
    s.subtitlesEnabled = true;
    s.subtitleFontSize = 32.0f;
    s.fontScale = 1.75f;
    s.dwellClickEnabled = true;
    s.dwellClickTime = 2.0f;
    s.switchAccessEnabled = true;
    s.switchScanSpeed = 2.5f;
    s.screenReaderEnabled = true;
    s.dyslexiaFriendly = true;

    Accessibility::RuntimeAccessibilitySettings loaded;
    ENJIN_ASSERT_TRUE(loaded.FromJson(s.ToJson()));

    ENJIN_EXPECT_TRUE(loaded.colorblindMode == Accessibility::ColorblindMode::Deuteranopia);
    ENJIN_EXPECT_TRUE(loaded.reducedMotion);
    ENJIN_EXPECT_TRUE(loaded.subtitlesEnabled);
    ENJIN_EXPECT_FLOAT_EQ(loaded.subtitleFontSize, 32.0f);
    ENJIN_EXPECT_FLOAT_EQ(loaded.fontScale, 1.75f);
    ENJIN_EXPECT_TRUE(loaded.dwellClickEnabled);
    ENJIN_EXPECT_FLOAT_EQ(loaded.dwellClickTime, 2.0f);
    ENJIN_EXPECT_TRUE(loaded.switchAccessEnabled);
    ENJIN_EXPECT_FLOAT_EQ(loaded.switchScanSpeed, 2.5f);
    ENJIN_EXPECT_TRUE(loaded.screenReaderEnabled);
    ENJIN_EXPECT_TRUE(loaded.dyslexiaFriendly);
}

ENJIN_TEST(ActionDrivenUI, AccessibilityJsonIsSafeOnGarbageAndClampsFontScale) {
    Accessibility::RuntimeAccessibilitySettings s;
    ENJIN_EXPECT_FALSE(s.FromJson(""));
    ENJIN_EXPECT_FALSE(s.FromJson("{{{not json"));
    ENJIN_EXPECT_FALSE(s.FromJson("[]"));            // not an object

    ENJIN_ASSERT_TRUE(s.FromJson("{\"fontScale\": 99.0}"));
    ENJIN_EXPECT_FLOAT_EQ(s.fontScale, 3.0f);        // clamped, not absurd
    ENJIN_ASSERT_TRUE(s.FromJson("{\"fontScale\": 0.01}"));
    ENJIN_EXPECT_FLOAT_EQ(s.fontScale, 0.5f);
    // An out-of-range enum falls back rather than becoming a garbage mode.
    ENJIN_ASSERT_TRUE(s.FromJson("{\"colorblindMode\": 99}"));
    ENJIN_EXPECT_TRUE(s.colorblindMode == Accessibility::ColorblindMode::Off);
}

ENJIN_TEST_MAIN()
