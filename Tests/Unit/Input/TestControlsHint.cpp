// The engine's controls hint has to be switchable off.
//
// DrawControlsHint paints a line in the bottom-left of every runtime, built
// from the active touch PRESET -- so it describes the controls a preset
// implies, not the ones a particular game uses. A game that draws its own
// control legend got both, stacked on top of each other. Worse, the engine's
// line can be wrong for that game: a sailing project steering on A/D rudder
// and W/S sheet was told "W/A/S/D move - Hold RMB look - L.Shift sprint".
//
// It stays ON by default, because every project that never asks has always had
// it and turning it off silently would be its own bug.
#include "EnjinTest.h"
#include "Enjin/Input/TouchActionBridge.h"
#include "Enjin/Input/InputProjectSettings.h"
#include "Enjin/Input/InputAction.h"

using namespace Enjin;
using namespace Enjin::InputSystem;

namespace {

// The switch is process-global, so every test puts it back.
struct HintScope {
    ~HintScope() { SetControlsHintEnabled(true); }
};

} // namespace

ENJIN_TEST(ControlsHint, ItIsOnUntilSomethingTurnsItOff) {
    // Arrange / Act / Assert: the default every existing project relies on.
    HintScope scope;
    SetControlsHintEnabled(true);
    ENJIN_EXPECT_TRUE(IsControlsHintEnabled());
}

ENJIN_TEST(ControlsHint, AGameCanTurnItOffAndBackOn) {
    // Arrange: a game drawing its own legend turns this off, and a pause menu
    // or a scene change may want it back.
    HintScope scope;

    // Act / Assert
    SetControlsHintEnabled(false);
    ENJIN_EXPECT_TRUE(!IsControlsHintEnabled());
    SetControlsHintEnabled(true);
    ENJIN_EXPECT_TRUE(IsControlsHintEnabled());
}

ENJIN_TEST(ControlsHint, ProjectSettingsDefaultToShowingIt) {
    // Arrange: an untouched project must be byte-identical to before this
    // existed, which also means IsEmpty stays true so nothing is written.
    InputProjectSettings s;

    // Act / Assert
    ENJIN_EXPECT_TRUE(s.showControlsHint);
    ENJIN_EXPECT_TRUE(s.IsEmpty());
}

ENJIN_TEST(ControlsHint, AProjectThatHidesItIsNoLongerEmpty) {
    // Arrange: IsEmpty decides whether the block is written to the project
    // file at all. If hiding the hint left the settings "empty", the choice
    // would be dropped on the next save and silently come back.
    InputProjectSettings s;
    s.showControlsHint = false;

    // Act / Assert
    ENJIN_EXPECT_TRUE(!s.IsEmpty());
}

ENJIN_TEST(ControlsHint, TheChoiceSurvivesASaveAndLoad) {
    // Arrange: this is the trap CLAUDE.md names -- a key written but not read
    // back is erased by the next save, with no warning anywhere.
    InputProjectSettings saved;
    saved.showControlsHint = false;

    // Act
    InputProjectSettings loaded;
    ENJIN_ASSERT_TRUE(loaded.FromJson(saved.ToJson()));

    // Assert
    ENJIN_EXPECT_TRUE(!loaded.showControlsHint);
}

ENJIN_TEST(ControlsHint, AProjectWithNoOpinionLoadsAsShown) {
    // Arrange: every scene authored before this field existed.
    InputProjectSettings loaded;
    loaded.showControlsHint = false;          // start wrong, so a miss is visible

    // Act
    ENJIN_ASSERT_TRUE(loaded.FromJson("{}"));

    // Assert
    ENJIN_EXPECT_TRUE(loaded.showControlsHint);
}

ENJIN_TEST(ControlsHint, ApplyToPushesTheChoiceToTheRuntime) {
    // Arrange: ApplyTo is the one call every runtime makes with the project
    // block. If the flag were pushed anywhere else, one of the three runtimes
    // would miss it -- which is how these gaps happen.
    HintScope scope;
    SetControlsHintEnabled(true);
    InputProjectSettings s;
    s.showControlsHint = false;
    InputActionMap map;

    // Act
    s.ApplyTo(map);

    // Assert
    ENJIN_EXPECT_TRUE(!IsControlsHintEnabled());
}

ENJIN_TEST_MAIN()
