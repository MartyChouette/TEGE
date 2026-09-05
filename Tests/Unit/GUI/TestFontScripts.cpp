// Which glyphs the font atlas bakes, per locale.
//
// Every font was added to the atlas with no GlyphRanges, so ImGui used its
// default: Basic Latin and Latin-1, about 190 glyphs. That threw away most of
// what the embedded face already held -- Roboto-Medium carries the full Latin
// Extended-A block, 75 Greek glyphs and 255 Cyrillic ones -- so Polish, Czech,
// Turkish, Greek and Russian rendered as blank boxes from a font that had the
// glyphs sitting in it. The same atlas draws game UI, subtitles and the
// announcer in all three runtimes, so this was never editor-only.
//
// European coverage is now unconditional. The CJK-scale scripts are gated on
// the locale, because each is thousands of glyphs and a much larger atlas
// texture, and web pays that in memory. This covers the gating rule; the ImGui
// plumbing around it needs a live context and is not what has rules.
#include "EnjinTest.h"
#include "Enjin/GUI/FontScripts.h"

using namespace Enjin;
using namespace Enjin::GUI;

ENJIN_TEST(FontScripts, APlainEuropeanLocaleAsksForNoExtraScript) {
    // Arrange / Act / Assert: the common case, and the one that must not start
    // paying for a CJK atlas.
    ENJIN_EXPECT_TRUE(ScriptForLocale("en") == AtlasScript::EuropeanOnly);
    ENJIN_EXPECT_TRUE(ScriptForLocale("fr") == AtlasScript::EuropeanOnly);
    ENJIN_EXPECT_TRUE(ScriptForLocale("pl") == AtlasScript::EuropeanOnly);
    ENJIN_EXPECT_TRUE(ScriptForLocale("ru") == AtlasScript::EuropeanOnly);
    ENJIN_EXPECT_TRUE(ScriptForLocale("el") == AtlasScript::EuropeanOnly);
}

ENJIN_TEST(FontScripts, EachLargeScriptIsRecognisedFromItsLanguageSubtag) {
    // Arrange / Act / Assert
    ENJIN_EXPECT_TRUE(ScriptForLocale("ja") == AtlasScript::Japanese);
    ENJIN_EXPECT_TRUE(ScriptForLocale("zh") == AtlasScript::ChineseFull);
    ENJIN_EXPECT_TRUE(ScriptForLocale("ko") == AtlasScript::Korean);
    ENJIN_EXPECT_TRUE(ScriptForLocale("th") == AtlasScript::Thai);
    ENJIN_EXPECT_TRUE(ScriptForLocale("vi") == AtlasScript::Vietnamese);
}

ENJIN_TEST(FontScripts, RegionAndScriptSuffixesDoNotHideTheLanguage) {
    // Arrange: BCP 47 writes ja-JP, POSIX writes ja_JP, and both turn up in
    // project files. A prefix match that missed the separator would bake a
    // Latin-only atlas for a Japanese game and render every word as boxes.
    // Act / Assert
    ENJIN_EXPECT_TRUE(ScriptForLocale("ja-JP") == AtlasScript::Japanese);
    ENJIN_EXPECT_TRUE(ScriptForLocale("ja_JP") == AtlasScript::Japanese);
    ENJIN_EXPECT_TRUE(ScriptForLocale("zh-Hans-CN") == AtlasScript::ChineseFull);
    ENJIN_EXPECT_TRUE(ScriptForLocale("ko_KR.UTF-8") == AtlasScript::Korean);
}

ENJIN_TEST(FontScripts, MatchingIsCaseInsensitive) {
    // Arrange: locale codes come from hand-edited project files and from
    // players, not only from code.
    // Act / Assert
    ENJIN_EXPECT_TRUE(ScriptForLocale("JA") == AtlasScript::Japanese);
    ENJIN_EXPECT_TRUE(ScriptForLocale("Zh-HANS") == AtlasScript::ChineseFull);
}

ENJIN_TEST(FontScripts, ALanguageThatMerelyStartsWithThoseLettersIsNotMistaken) {
    // Arrange: a bare "starts with" test would read Javanese as Japanese and
    // Zhuang as Chinese, and quietly bake a 20k-glyph atlas for a Latin game.
    // Act / Assert
    ENJIN_EXPECT_TRUE(ScriptForLocale("jav") == AtlasScript::EuropeanOnly);
    ENJIN_EXPECT_TRUE(ScriptForLocale("zha") == AtlasScript::EuropeanOnly);
    ENJIN_EXPECT_TRUE(ScriptForLocale("kor") == AtlasScript::EuropeanOnly);
}

ENJIN_TEST(FontScripts, AnEmptyOrUnknownLocaleFallsBackToEuropean) {
    // Arrange: an absent localization block leaves the locale at its default,
    // and the fallback has to be what every existing project already gets.
    // Act / Assert
    ENJIN_EXPECT_TRUE(ScriptForLocale("") == AtlasScript::EuropeanOnly);
    ENJIN_EXPECT_TRUE(ScriptForLocale("xx-YY") == AtlasScript::EuropeanOnly);
    ENJIN_EXPECT_TRUE(ScriptForLocale("-") == AtlasScript::EuropeanOnly);
}

ENJIN_TEST_MAIN()
