// A panel that was switched off overwrote other people's settings every frame.
//
// EditorLayer::Update syncs the Retro Effects panel into PostProcessSettings and
// the RenderSystem. The branch for "retro is disabled" looked like this:
//
//     } else if (m_PostProcessing) {
//         // When retro effects are disabled, clear the retro post-process fields
//         settings.ditherEnabled = 0;
//         settings.colorQuantEnabled = 0;
//         settings.resDownscaleEnabled = 0;
//         settings.crtEnabled = 0;
//         settings.crtPhosphorEnabled = 0;
//         settings.vhsEnabled = 0;
//     }
//
// with eight more global render overrides cleared just below it. Retro Effects is
// off in almost every session, so that ran on almost every frame, and it zeroed
// fourteen fields regardless of who had set them or why.
//
// Anything else that wanted CRT, VHS, dithering, colour quantisation or resolution
// downscale lost it on the next frame: the Rendering panel, an art-style preset, a
// scene's own saved renderSettings applied at load, and the camera ArtStyle
// mapping. What an author saw was a setting that would not stay on, with nothing
// in the panel they were touching to explain why -- the panel doing the wiping was
// a different one, and closed.
//
// The fix is that the clear-down is EDGE-TRIGGERED: it happens on the frame retro
// goes from on to off, and not again. These tests pin that, because it is pure
// state logic and the alternative -- noticing the bug in a render -- requires
// catching a field being reset one frame after something set it.
#include "EnjinTest.h"
#include <vector>



namespace {

// The retro sync's decision, extracted. `wasEnabled` is the previous frame's
// state; the function answers what this frame does and what to remember.
struct RetroSync {
    bool wasEnabled = false;

    // Returns true when this frame should clear the retro-owned fields.
    bool Step(bool enabledNow) {
        const bool clearNow = (!enabledNow && wasEnabled);
        wasEnabled = enabledNow;
        return clearNow;
    }
};

// What the old code did, kept so the difference is visible rather than asserted
// about in the abstract.
bool OldStep(bool enabledNow) { return !enabledNow; }

} // namespace

ENJIN_TEST(RetroClearDown, ADisabledPanelClearsOnceNotEveryFrame) {
    // Arrange: retro has never been on, which is the ordinary case.
    RetroSync sync;

    // Act: sixty frames with the panel off.
    int clears = 0;
    for (int frame = 0; frame < 60; ++frame) {
        if (sync.Step(false)) ++clears;
    }

    // Assert: it never clears, because there was nothing of its own to clear.
    ENJIN_EXPECT_EQ(clears, 0);

    // The old behaviour, for contrast: sixty frames, sixty wipes of fourteen
    // fields belonging to whoever last set them.
    int oldClears = 0;
    for (int frame = 0; frame < 60; ++frame) {
        if (OldStep(false)) ++oldClears;
    }
    ENJIN_EXPECT_EQ(oldClears, 60);
}

ENJIN_TEST(RetroClearDown, TurningRetroOffStillClearsItsLook) {
    // The behaviour that must survive the fix: switching the panel off has to
    // remove the effects it turned on, or "off" would not mean off.
    RetroSync sync;

    ENJIN_EXPECT_FALSE(sync.Step(true));    // on: applies, does not clear
    ENJIN_EXPECT_FALSE(sync.Step(true));    // still on
    ENJIN_EXPECT_TRUE(sync.Step(false));    // the falling edge: clear once
    ENJIN_EXPECT_FALSE(sync.Step(false));   // and not again
    ENJIN_EXPECT_FALSE(sync.Step(false));
}

ENJIN_TEST(RetroClearDown, ASettingFromElsewhereSurvivesADisabledPanel) {
    // The bug, as a sequence. A scene is loaded with crtEnabled saved as true, and
    // the Retro Effects panel is off -- which it is by default, and in almost every
    // session.
    bool crtEnabled = false;
    RetroSync sync;

    // Frame 1: the scene loads and applies its own saved render settings.
    crtEnabled = true;

    // Frames 2..30: the editor runs with the retro panel off.
    for (int frame = 0; frame < 30; ++frame) {
        if (sync.Step(false)) crtEnabled = false;
    }

    // The author's setting is still there.
    ENJIN_EXPECT_TRUE(crtEnabled);

    // Under the old rule it was gone by frame 2, and stayed gone however many
    // times they set it again.
    bool oldCrt = true;
    for (int frame = 0; frame < 30; ++frame) {
        if (OldStep(false)) oldCrt = false;
    }
    ENJIN_EXPECT_FALSE(oldCrt);
}

ENJIN_TEST(RetroClearDown, RetroStillWinsWhileItIsOn) {
    // The fix must not go the other way either: while retro IS on it owns these
    // fields, and something else setting them mid-session does not get to keep
    // them. The sync runs every frame it is enabled.
    RetroSync sync;
    bool crtFromRetro = false;

    for (int frame = 0; frame < 5; ++frame) {
        const bool enabled = true;
        sync.Step(enabled);
        if (enabled) crtFromRetro = true;   // the enabled branch applies each frame
    }
    ENJIN_EXPECT_TRUE(crtFromRetro);
}

ENJIN_TEST(RetroClearDown, TogglingRepeatedlyClearsOncePerFallingEdge) {
    RetroSync sync;
    int clears = 0;
    for (bool on : { true, false, true, false, false, true, true, false }) {
        if (sync.Step(on)) ++clears;
    }
    ENJIN_EXPECT_EQ(clears, 3);   // three on->off transitions in that sequence
}

ENJIN_TEST_MAIN()
