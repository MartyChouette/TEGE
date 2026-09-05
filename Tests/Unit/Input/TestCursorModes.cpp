// Who has the cursor, and what it looks like while they do.
//
// There was exactly ONE answer: GLFW_CURSOR_DISABLED -- hidden, locked, and
// reporting an unbounded virtual position. Right for an FPS in the game view;
// wrong for the editor's scene viewport, where the cursor vanishing while you
// orbit is disorienting and you want to see what you are pointing at.
//
// VisibleWrapped is the missing middle: visible, and warped to the opposite
// edge when it reaches one, so a long drag never runs out of screen. GLFW 3.4
// would give this natively as GLFW_CURSOR_CAPTURED; this build is on 3.3, so
// the wrap is done by hand.
//
// These run without a window, so they pin the STATE MACHINE -- which mode is
// active, and that the legacy boolean still maps onto it. The warp itself
// needs a real window and is exercised by using the editor.
#include "EnjinTest.h"
#include "Enjin/Platform/Input.h"

using namespace Enjin;

namespace {

// The mode is process-global, so every test puts it back.
struct ModeScope {
    ~ModeScope() { Input::SetMouseCaptureMode(Input::MouseCaptureMode::Free); }
};

} // namespace

ENJIN_TEST(CursorModes, NothingOwnsTheCursorByDefault) {
    // Arrange / Act / Assert: an editor sitting idle must not be holding it.
    ModeScope scope;
    Input::SetMouseCaptureMode(Input::MouseCaptureMode::Free);
    ENJIN_EXPECT_TRUE(Input::GetMouseCaptureMode() == Input::MouseCaptureMode::Free);
    ENJIN_EXPECT_TRUE(!Input::IsMouseCaptured());
}

ENJIN_TEST(CursorModes, HiddenIsTheOnlyModeThatCountsAsCaptured) {
    // Arrange: IsMouseCaptured gates a lot of behaviour -- whether the editor
    // hands input to the game, whether the fly-cam reads deltas. Only the
    // hidden+locked mode means "the cursor is gone"; a visible wrapped cursor
    // is still the user's.
    ModeScope scope;

    // Act / Assert
    Input::SetMouseCaptureMode(Input::MouseCaptureMode::Hidden);
    ENJIN_EXPECT_TRUE(Input::IsMouseCaptured());

    Input::SetMouseCaptureMode(Input::MouseCaptureMode::VisibleWrapped);
    ENJIN_EXPECT_TRUE(!Input::IsMouseCaptured());

    Input::SetMouseCaptureMode(Input::MouseCaptureMode::Free);
    ENJIN_EXPECT_TRUE(!Input::IsMouseCaptured());
}

ENJIN_TEST(CursorModes, TheOldBooleanStillMapsOntoTheModes) {
    // Arrange: dozens of call sites still say SetMouseCaptured(true/false),
    // and every one of them has to keep working. If the boolean stopped
    // agreeing with the mode, half the engine would think the cursor was free
    // while the other half hid it.
    ModeScope scope;

    // Act / Assert
    Input::SetMouseCaptured(true);
    ENJIN_EXPECT_TRUE(Input::IsMouseCaptured());
    ENJIN_EXPECT_TRUE(Input::GetMouseCaptureMode() == Input::MouseCaptureMode::Hidden);

    Input::SetMouseCaptured(false);
    ENJIN_EXPECT_TRUE(!Input::IsMouseCaptured());
    ENJIN_EXPECT_TRUE(Input::GetMouseCaptureMode() == Input::MouseCaptureMode::Free);
}

ENJIN_TEST(CursorModes, SwitchingModesIsIdempotent) {
    // Arrange: the fly-cam sets its mode every frame it is active. Re-applying
    // the same mode must not keep resetting the delta origin, or looking would
    // read zero every frame and the camera would never turn.
    ModeScope scope;
    Input::SetMouseCaptureMode(Input::MouseCaptureMode::VisibleWrapped);

    // Act
    Input::SetMouseCaptureMode(Input::MouseCaptureMode::VisibleWrapped);
    Input::SetMouseCaptureMode(Input::MouseCaptureMode::VisibleWrapped);

    // Assert
    ENJIN_EXPECT_TRUE(Input::GetMouseCaptureMode() == Input::MouseCaptureMode::VisibleWrapped);
}

ENJIN_TEST(CursorModes, EveryTransitionIsReachable) {
    // Arrange: the editor moves between all three -- free while editing,
    // wrapped while orbiting, hidden when the game view takes over -- and back.
    ModeScope scope;
    const Input::MouseCaptureMode order[] = {
        Input::MouseCaptureMode::Free,
        Input::MouseCaptureMode::VisibleWrapped,
        Input::MouseCaptureMode::Hidden,
        Input::MouseCaptureMode::VisibleWrapped,
        Input::MouseCaptureMode::Free,
        Input::MouseCaptureMode::Hidden,
        Input::MouseCaptureMode::Free,
    };

    // Act / Assert
    for (Input::MouseCaptureMode m : order) {
        Input::SetMouseCaptureMode(m);
        ENJIN_EXPECT_TRUE(Input::GetMouseCaptureMode() == m);
        ENJIN_EXPECT_TRUE(Input::IsMouseCaptured() == (m == Input::MouseCaptureMode::Hidden));
    }
}

ENJIN_TEST_MAIN()
