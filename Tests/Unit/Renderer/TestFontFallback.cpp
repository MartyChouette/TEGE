// A font that will not load must not cost the words.
//
// An authored fontPath pointing at a file that is not there produced an empty
// bake: the SDF atlas failed to build, the entity dropped to the rasterized
// path, that failed for the same reason, and the entity rendered as a BLANK
// GREY QUAD. The text was gone and the only sign of why was one line in the
// log.
//
// This is the engine's own trap, not project data. Two separate project
// documents record it as "world text needs a real .ttf" and worked around it
// rather than reporting a bug, because a blank box does not read as a font
// error. The embedded body face was available the whole time -- an empty
// fontPath has always used it.
//
// Verified against a real capture: before this, an orthographic scene with a
// missing font drew a grey rectangle; after, it draws the authored words.
#include "EnjinTest.h"
#include "Enjin/Renderer/TextRasterizer.h"
#include "Enjin/ECS/Components/Text.h"

using namespace Enjin;

namespace {

ECS::TextComponent TextWithFont(const std::string& fontPath) {
    ECS::TextComponent tc;
    tc.text = "AUTHORED";
    tc.fontPath = fontPath;
    tc.fontSize = 32.0f;
    tc.textureWidth = 256;
    tc.textureHeight = 64;
    tc.bgOpacity = 0.0f;                       // transparent ground
    tc.textColor = Math::Vector3(1.0f, 1.0f, 1.0f);
    tc.wrapWidth = 0.0f;
    return tc;
}

// How many pixels the glyphs actually inked. A blank bake -- the bug -- leaves
// every pixel fully transparent, so this is zero.
usize InkedPixels(const std::vector<u8>& rgba) {
    usize inked = 0;
    for (usize i = 3; i < rgba.size(); i += 4) {
        if (rgba[i] > 8) ++inked;
    }
    return inked;
}

} // namespace

ENJIN_TEST(FontFallback, AMissingFontStillRendersTheWords) {
    // Arrange: the exact shape a generated scene produces -- a font path
    // authored against a file that was never shipped.
    Renderer::TextRasterizer rasterizer;
    const ECS::TextComponent tc = TextWithFont("assets/fonts/DefinitelyNotHere_98765.ttf");

    // Act
    const std::vector<u8> pixels = rasterizer.Rasterize(tc);

    // Assert: a real bake, with glyphs in it.
    ENJIN_ASSERT_TRUE(!pixels.empty());
    ENJIN_EXPECT_TRUE(InkedPixels(pixels) > 0);
}

ENJIN_TEST(FontFallback, TheFallbackIsTheSameFaceAnEmptyPathUses) {
    // Arrange: falling back to SOMETHING is not enough -- it has to be the
    // engine's normal body face, or a missing font would silently restyle the
    // game instead of silently blanking it.
    Renderer::TextRasterizer rasterizer;

    // Act
    const std::vector<u8> missing = rasterizer.Rasterize(TextWithFont("no_such_font_98765.ttf"));
    const std::vector<u8> defaulted = rasterizer.Rasterize(TextWithFont(""));

    // Assert: byte-identical bakes.
    ENJIN_ASSERT_TRUE(!missing.empty() && !defaulted.empty());
    ENJIN_EXPECT_TRUE(missing.size() == defaulted.size());
    ENJIN_EXPECT_TRUE(missing == defaulted);
}

ENJIN_TEST(FontFallback, AnEmptyFontPathWasAlwaysMeantToWork) {
    // Arrange: the baseline the fallback restores. If this ever broke, the
    // fallback above would be falling back onto nothing.
    Renderer::TextRasterizer rasterizer;

    // Act
    const std::vector<u8> pixels = rasterizer.Rasterize(TextWithFont(""));

    // Assert
    ENJIN_ASSERT_TRUE(!pixels.empty());
    ENJIN_EXPECT_TRUE(InkedPixels(pixels) > 0);
}

ENJIN_TEST(FontFallback, EmptyTextBakesNothingRatherThanFallingBack) {
    // Arrange: an entity with a font but no words is not a font failure, and
    // must not start drawing something.
    Renderer::TextRasterizer rasterizer;
    ECS::TextComponent tc = TextWithFont("");
    tc.text = "";

    // Act
    const std::vector<u8> pixels = rasterizer.Rasterize(tc);

    // Assert: nothing inked, whatever the buffer size.
    ENJIN_EXPECT_TRUE(InkedPixels(pixels) == 0);
}

ENJIN_TEST_MAIN()
