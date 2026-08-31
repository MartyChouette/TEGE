#include "EnjinTest.h"
#include "Enjin/Renderer/FontAtlas.h"
#include "Enjin/Accessibility/OpenDyslexicFont.h"
#include <algorithm>

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

ENJIN_TEST(FontAtlas, BuildsTextMesh) {
    FontAtlas atlas;
    ENJIN_ASSERT_TRUE(atlas.Build(Accessibility::s_OpenDyslexicFontData,
                                  Accessibility::s_OpenDyslexicFontDataSize));

    ECS::TextComponent tc;
    tc.text = "HELLO";
    tc.worldHeight = 0.5f;
    tc.wrapWidth = 0.0f;   // no wrap
    tc.textColor = Math::Vector3(1.0f, 0.0f, 0.0f);
    ECS::MeshComponent mesh = atlas.BuildTextMesh(tc);

    // One quad per visible glyph.
    ENJIN_ASSERT_EQ(mesh.vertices.size(), static_cast<usize>(5 * 4));
    ENJIN_ASSERT_EQ(mesh.indices.size(), static_cast<usize>(5 * 6));

    f32 minX = 1e9f, maxX = -1e9f, minY = 1e9f, maxY = -1e9f;
    for (const auto& v : mesh.vertices) {
        minX = std::min(minX, v.position.x); maxX = std::max(maxX, v.position.x);
        minY = std::min(minY, v.position.y); maxY = std::max(maxY, v.position.y);
        // +Z facing, textColor in vertex color.
        ENJIN_EXPECT_TRUE(v.normal.z > 0.99f);
        ENJIN_EXPECT_TRUE(v.color.x > 0.99f && v.color.y < 0.01f);
        ENJIN_EXPECT_EQ(v.position.z, 0.0f);
    }
    // Top-left anchored: block starts at x=0, extends right and DOWN (-y).
    // Quads carry the SDF padding ring (kPadding px each side - it renders
    // transparent after thresholding), so bounds may exceed the tight text
    // box by exactly that ring.
    const f32 glyphScale = tc.fontSize / FontAtlas::kBasePx;
    const f32 world = tc.worldHeight / (atlas.LineHeight() * glyphScale);
    const f32 padWorld = FontAtlas::kPadding * glyphScale * world + 0.01f;
    ENJIN_EXPECT_TRUE(minX >= -padWorld && maxX > minX);
    ENJIN_EXPECT_TRUE(maxY <= padWorld && minY < maxY);
    ENJIN_EXPECT_TRUE(maxY - minY <= 0.5f + 2.0f * padWorld);
    ENJIN_EXPECT_TRUE(maxY - minY > 0.15f);

    // A newline makes a second line strictly below the first.
    tc.text = "A\nB";
    ECS::MeshComponent two = atlas.BuildTextMesh(tc);
    ENJIN_ASSERT_EQ(two.vertices.size(), static_cast<usize>(2 * 4));
    f32 topA = std::max(two.vertices[2].position.y, two.vertices[3].position.y);
    f32 topB = std::max(two.vertices[6].position.y, two.vertices[7].position.y);
    ENJIN_EXPECT_TRUE(topB < topA);

    // Empty text -> empty mesh (renderer skips it safely).
    tc.text = "";
    ENJIN_EXPECT_TRUE(atlas.BuildTextMesh(tc).vertices.empty());

    // Word wrap: a narrow wrap width forces multiple lines (lower minY).
    tc.text = "aaa bbb ccc ddd";
    tc.wrapWidth = 40.0f;
    ECS::MeshComponent wrapped = atlas.BuildTextMesh(tc);
    tc.wrapWidth = 0.0f;
    ECS::MeshComponent flat = atlas.BuildTextMesh(tc);
    f32 wrapMinY = 1e9f, flatMinY = 1e9f;
    for (const auto& v : wrapped.vertices) wrapMinY = std::min(wrapMinY, v.position.y);
    for (const auto& v : flat.vertices) flatMinY = std::min(flatMinY, v.position.y);
    ENJIN_EXPECT_TRUE(wrapMinY < flatMinY - 0.1f);
}

ENJIN_TEST_MAIN()
