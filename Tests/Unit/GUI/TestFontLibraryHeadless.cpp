#include "EnjinTest.h"
#include "Enjin/Accessibility/FontLibrary.h"

using namespace Enjin;
using namespace Enjin::Accessibility;

// ============================================================================
// FONT LIBRARY WITHOUT AN IMGUI CONTEXT
//
// FontLibrary reached straight for ImGui::GetIO() and ImGui::GetStyle(), both
// of which dereference the global context with no null check of their own. Any
// runtime that has no ImGui layer therefore crashed on an accessibility setting
// rather than ignoring one: found by the dedicated server (adr-0007 Track B
// step 2), which applies the player's accessibility settings at boot like every
// other runtime and took an access violation inside SetFont.
//
// The rule these pin down: a font choice without an atlas to apply it to is a
// no-op, never a crash, and the CHOICE is still recorded so that a context
// appearing later starts from the right family.
//
// These run with no ImGui context by construction -- a unit test never creates
// one -- so the test fixture IS the condition under test.
// ============================================================================

ENJIN_TEST(FontLibraryHeadless, SetFontDoesNotCrashWithoutAContext) {
    // Arrange
    FontLibrary fonts;

    // Act: every family, because the original crash was inside one switch arm
    // and the others reach the same io.Fonts.
    fonts.SetFont(FontFamily::Default);
    fonts.SetFont(FontFamily::Monospace);
    fonts.SetFont(FontFamily::OpenDyslexic);

    // Assert: reaching this line at all is most of the point -- the original
    // defect was an access violation inside the calls above. The family check
    // proves each call ran to completion rather than being optimised away.
    ENJIN_EXPECT_TRUE(fonts.GetCurrentFamily() == FontFamily::OpenDyslexic);
}

ENJIN_TEST(FontLibraryHeadless, TheChoiceIsStillRecorded) {
    FontLibrary fonts;

    fonts.SetFont(FontFamily::OpenDyslexic);

    // Not applied, but not forgotten either. A runtime that gains a context
    // later -- or a settings round trip that reads this back -- must see what
    // the player actually chose, not a default standing in for it.
    ENJIN_EXPECT_TRUE(fonts.GetCurrentFamily() == FontFamily::OpenDyslexic);
}

ENJIN_TEST(FontLibraryHeadless, ApplySpacingReportsItDidNothing) {
    FontLibrary fonts;
    FontLibraryConfig cfg;
    cfg.letterSpacing = 2.0f;
    cfg.lineSpacing = 1.5f;
    fonts.SetConfig(cfg);

    // Spacing that WOULD be applied given a context: the early "nothing to do"
    // return must not be what saves us here, or the test proves nothing.
    ENJIN_EXPECT_FALSE(fonts.ApplySpacing());
}

ENJIN_TEST(FontLibraryHeadless, SpacingConfigStillClamps) {
    FontLibrary fonts;
    FontLibraryConfig cfg;
    cfg.letterSpacing = 999.0f;
    cfg.lineSpacing = 0.1f;
    fonts.SetConfig(cfg);

    // Config handling is pure and owes nothing to ImGui, so it keeps working
    // headless. Pinned so a future "just guard the whole class" does not take
    // the clamp out with the drawing.
    ENJIN_EXPECT_TRUE(fonts.GetConfig().letterSpacing <= 10.0f);
    ENJIN_EXPECT_TRUE(fonts.GetConfig().lineSpacing >= 1.0f);
}

ENJIN_TEST_MAIN()
