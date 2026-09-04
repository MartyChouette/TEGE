// The dyslexia-friendly font has to reach the text the GAME draws.
//
// Turning it on only ever wrote ImGui's io.FontDefault, so it restyled the
// editor's own furniture and left every word the game drew unchanged. The
// engine's two text bakers -- TextRasterizer (bitmap) and FontAtlas (SDF) --
// resolved their own bytes and knew nothing about the setting.
//
// They also had no body face of their own and used the embedded OpenDyslexic
// bytes as "the bundled default", so authored text was ALWAYS in the
// accessibility face and the toggle had nothing left to switch. Both halves
// are covered here.
#include "EnjinTest.h"
#include "Enjin/Accessibility/TextFont.h"
#include "Enjin/Accessibility/AccessibilitySettings.h"
#include "Enjin/Accessibility/OpenDyslexicFont.h"
#include "Enjin/GUI/EmbeddedFonts.h"

using namespace Enjin;
using namespace Enjin::Accessibility;

namespace {

// The choice is process-global, so every test puts it back.
struct FontChoiceReset {
    ~FontChoiceReset() { SetDyslexiaFontEnabled(false); }
};

} // namespace

ENJIN_TEST(TextFont, TheDefaultBodyFaceIsNotTheAccessibilityFace) {
    // Arrange: the bug that made the toggle a no-op. Text with no authored
    // font must bake in the normal body face, or there is nothing to switch
    // TO when a player turns the setting on.
    FontChoiceReset reset;
    SetDyslexiaFontEnabled(false);
    usize size = 0;

    // Act
    const u8* bytes = ResolveFontBytes("", size);

    // Assert
    ENJIN_ASSERT_TRUE(bytes != nullptr);
    ENJIN_EXPECT_TRUE(bytes != s_OpenDyslexicFontData);
    ENJIN_EXPECT_TRUE(bytes == GUI::RobotoMediumTTF);
    ENJIN_EXPECT_TRUE(size == GUI::RobotoMediumTTFSize);
}

ENJIN_TEST(TextFont, TurningItOnGivesEngineTextTheDyslexicFace) {
    // Arrange
    FontChoiceReset reset;
    SetDyslexiaFontEnabled(true);
    usize size = 0;

    // Act
    const u8* bytes = ResolveFontBytes("", size);

    // Assert
    ENJIN_ASSERT_TRUE(bytes != nullptr);
    ENJIN_EXPECT_TRUE(bytes == s_OpenDyslexicFontData);
    ENJIN_EXPECT_TRUE(size == s_OpenDyslexicFontDataSize);
}

ENJIN_TEST(TextFont, AnAuthoredFontPathIsLoadedFromDiskWhenTheSettingIsOff) {
    // Arrange: a game that picked its own font must keep it. A null return is
    // the resolver saying "read the file yourself".
    FontChoiceReset reset;
    SetDyslexiaFontEnabled(false);
    usize size = 0;

    // Act
    const u8* bytes = ResolveFontBytes("fonts/TitleFace.ttf", size);

    // Assert
    ENJIN_EXPECT_TRUE(bytes == nullptr);
    ENJIN_EXPECT_TRUE(size == 0);
}

ENJIN_TEST(TextFont, TheSettingOverridesTheAuthoredFontToo) {
    // Arrange: a player who needs this face needs it on the text the game
    // chose a font for as much as on the rest, so the setting wins over the
    // authored path rather than only filling in the default.
    FontChoiceReset reset;
    SetDyslexiaFontEnabled(true);
    usize size = 0;

    // Act
    const u8* bytes = ResolveFontBytes("fonts/TitleFace.ttf", size);

    // Assert
    ENJIN_EXPECT_TRUE(bytes == s_OpenDyslexicFontData);
}

ENJIN_TEST(TextFont, TheCacheKeyChangesWithTheSettingSoAToggleRebuilds) {
    // Arrange: both bakers cache by path. Without the choice in the key the
    // atlas baked before the toggle is handed straight back and the screen
    // never changes -- the toggle would look broken exactly as reported.
    FontChoiceReset reset;

    // Act
    SetDyslexiaFontEnabled(false);
    const std::string off = FontCacheKey("fonts/TitleFace.ttf");
    const std::string offDefault = FontCacheKey("");
    SetDyslexiaFontEnabled(true);
    const std::string on = FontCacheKey("fonts/TitleFace.ttf");
    const std::string onDefault = FontCacheKey("");

    // Assert
    ENJIN_EXPECT_TRUE(off != on);
    ENJIN_EXPECT_TRUE(offDefault != onDefault);
}

ENJIN_TEST(TextFont, ApplyTextScaleIsWhatPushesTheChoice) {
    // Arrange: every runtime already calls ApplyTextScale for text settings,
    // and it is documented as the one place that knows which systems draw
    // text. If the face is pushed anywhere else, a runtime will miss it.
    FontChoiceReset reset;
    SetDyslexiaFontEnabled(false);
    RuntimeAccessibilitySettings settings;
    settings.dyslexiaFriendly = true;

    // Act: no UI, subtitles or announcer -- this must not depend on them.
    ApplyTextScale(settings, nullptr, nullptr, nullptr);

    // Assert
    ENJIN_EXPECT_TRUE(IsDyslexiaFontEnabled());

    // And back off again, so the setting tracks rather than latches.
    settings.dyslexiaFriendly = false;
    ApplyTextScale(settings, nullptr, nullptr, nullptr);
    ENJIN_EXPECT_TRUE(!IsDyslexiaFontEnabled());
}

ENJIN_TEST_MAIN()
