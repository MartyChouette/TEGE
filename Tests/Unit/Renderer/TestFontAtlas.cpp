#include "EnjinTest.h"
#include "Enjin/Renderer/FontAtlas.h"
#include "Enjin/Accessibility/OpenDyslexicFont.h"

using namespace Enjin;
using namespace Enjin::Renderer;

// The SDF glyph atlas is P1 of the unified display system - every SDF text
// consumer draws from it, so its invariants are foundational.

ENJIN_TEST(FontAtlas, BakesEmbeddedFont) {
    FontAtlas atlas;
    ENJIN_ASSERT_TRUE(atlas.Build(Accessibility::s_OpenDyslexicFontData,
                                  Accessibility::s_OpenDyslexicFontDataSize));
    ENJIN_EXPECT_TRUE(atlas.IsBuilt());

    // Vertical metrics are sane: ascent up, descent down, a line fits both.
    ENJIN_EXPECT_TRUE(atlas.Ascent() > 0.0f);
    ENJIN_EXPECT_TRUE(atlas.Descent() < 0.0f);
    ENJIN_EXPECT_TRUE(atlas.LineHeight() >= atlas.Ascent() - atlas.Descent() - 0.01f);

    // Full printable ASCII baked.
    for (u32 cp = 33; cp <= 126; ++cp) {
        const FontAtlas::Glyph* g = atlas.Find(cp);
        ENJIN_ASSERT_TRUE(g != nullptr);
        ENJIN_EXPECT_TRUE(g->w > 0.0f && g->h > 0.0f);
        ENJIN_EXPECT_TRUE(g->u1 > g->u0 && g->v1 > g->v0);
        ENJIN_EXPECT_TRUE(g->xadvance > 0.0f);
    }

    // Space advances but has no quad.
    const FontAtlas::Glyph* space = atlas.Find(' ');
    ENJIN_ASSERT_TRUE(space != nullptr);
    ENJIN_EXPECT_TRUE(space->xadvance > 0.0f);

    // The atlas actually contains field data: some alpha above the on-edge
    // value (inside glyphs) and plenty of empty background.
    const auto& px = atlas.Pixels();
    ENJIN_ASSERT_EQ(px.size(), static_cast<usize>(atlas.Width()) * atlas.Height() * 4);
    usize inside = 0, background = 0;
    for (usize i = 3; i < px.size(); i += 4) {
        if (px[i] >= 180) ++inside;
        if (px[i] == 0) ++background;
    }
    ENJIN_EXPECT_TRUE(inside > 1000);
    ENJIN_EXPECT_TRUE(background > px.size() / 8);

    // RGB stays white everywhere (tint-through-baseColor contract).
    ENJIN_EXPECT_EQ(px[0], 255u);
    ENJIN_EXPECT_EQ(px[1], 255u);
    ENJIN_EXPECT_EQ(px[2], 255u);
}

ENJIN_TEST(FontAtlas, RejectsGarbage) {
    FontAtlas atlas;
    const u8 junk[16] = { 1, 2, 3, 4 };
    ENJIN_EXPECT_FALSE(atlas.Build(junk, sizeof(junk)));
    ENJIN_EXPECT_FALSE(atlas.Build(nullptr, 0));
    ENJIN_EXPECT_TRUE(atlas.Find('A') == nullptr);
}

ENJIN_TEST_MAIN()
