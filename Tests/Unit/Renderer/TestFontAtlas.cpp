#include "EnjinTest.h"
#include "Enjin/Renderer/FontAtlas.h"
#include "Enjin/Accessibility/OpenDyslexicFont.h"
#include "Enjin/ECS/Components/Text.h"
#include "Enjin/ECS/Components/Mesh.h"
#include <algorithm>
#include <cmath>

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


// ---------------------------------------------------------------------------
// UTF-8. Layout used to index tc.text a BYTE at a time and treat each byte as
// a codepoint, so every character above U+007F became two or more garbage
// glyphs -- accented text rendered as mojibake. Latin-1 is baked into the
// atlas BY CODEPOINT, so the right glyph was present the whole time and only
// the lookup was wrong. This also blocks localisation outright.
//
// Every string under test is built from explicit BYTE VALUES rather than a
// source literal: a literal has to survive this file's own encoding, and one
// that is silently re-encoded would leave the test passing against the wrong
// input.
// ---------------------------------------------------------------------------

namespace {

std::string U8(std::initializer_list<int> bytes) {
    std::string out;
    for (int b : bytes) out.push_back(static_cast<char>(static_cast<unsigned char>(b)));
    return out;
}

// U+00E9 LATIN SMALL LETTER E WITH ACUTE: two bytes, one character, one glyph.
const std::string& EAcute() { static const std::string s = U8({0xC3, 0xA9}); return s; }

Enjin::ECS::TextComponent MakeText(const std::string& body) {
    Enjin::ECS::TextComponent tc;
    tc.text = body;
    tc.sdfText = true;
    tc.wrapWidth = 0.0f;       // no wrapping: one line, so quads == visible glyphs
    tc.fontSize = 32.0f;
    tc.worldHeight = 0.5f;
    return tc;
}

// One quad is four vertices, so this is the number of VISIBLE glyphs laid out.
usize GlyphCount(const Enjin::ECS::MeshComponent& m) { return m.vertices.size() / 4; }

FontAtlas& SharedAtlas() {
    static FontAtlas atlas;
    static bool built = atlas.Build(Accessibility::s_OpenDyslexicFontData,
                                    Accessibility::s_OpenDyslexicFontDataSize);
    (void)built;
    return atlas;
}

} // namespace

ENJIN_TEST(FontAtlas, TheAtlasBakesLatin1ByCodepoint) {
    // Arrange / Act / Assert: the premise of everything below -- the accented
    // glyph really is in the atlas, so any failure to draw it is a LOOKUP bug.
    FontAtlas& atlas = SharedAtlas();
    ENJIN_ASSERT_TRUE(atlas.IsBuilt());
    const FontAtlas::Glyph* g = atlas.Find(0x00E9u);
    ENJIN_ASSERT_TRUE(g != nullptr);
    ENJIN_EXPECT_TRUE(g->w > 0.0f && g->h > 0.0f);
}

ENJIN_TEST(FontAtlas, AnAccentedWordLaysOutOneGlyphPerCharacter) {
    // Arrange: "cafe" with an acute is FIVE bytes but FOUR characters. Byte
    // indexing laid out five quads, the last two of them garbage.
    FontAtlas& atlas = SharedAtlas();
    ENJIN_ASSERT_TRUE(atlas.IsBuilt());
    const std::string accented = "caf" + EAcute();
    ENJIN_ASSERT_TRUE(accented.size() == 5);      // the premise: 5 bytes, 4 chars

    // Act
    const Enjin::ECS::MeshComponent plain = atlas.BuildTextMesh(MakeText("cafe"));
    const Enjin::ECS::MeshComponent acc   = atlas.BuildTextMesh(MakeText(accented));

    // Assert
    ENJIN_EXPECT_TRUE(GlyphCount(plain) == 4);
    ENJIN_EXPECT_TRUE(GlyphCount(acc) == 4);
}

ENJIN_TEST(FontAtlas, TheAccentedQuadSamplesTheRectBakedForThatCodepoint) {
    // Arrange: counting quads alone would also pass if layout silently DROPPED
    // both bytes. The quad has to sample the atlas rect baked for U+00E9.
    FontAtlas& atlas = SharedAtlas();
    ENJIN_ASSERT_TRUE(atlas.IsBuilt());
    const FontAtlas::Glyph* g = atlas.Find(0x00E9u);
    ENJIN_ASSERT_TRUE(g != nullptr);

    // Act: the accented character alone, so its quad is the only one.
    const Enjin::ECS::MeshComponent m = atlas.BuildTextMesh(MakeText(EAcute()));

    // Assert
    ENJIN_ASSERT_TRUE(GlyphCount(m) == 1);
    bool sawU0 = false, sawU1 = false;
    for (const auto& v : m.vertices) {
        if (std::fabs(v.uv.x - g->u0) < 1e-5f) sawU0 = true;
        if (std::fabs(v.uv.x - g->u1) < 1e-5f) sawU1 = true;
    }
    ENJIN_EXPECT_TRUE(sawU0 && sawU1);
}

ENJIN_TEST(FontAtlas, AsciiLayoutIsUnchangedByTheDecoder) {
    // Arrange: the decoder must be a no-op for plain ASCII, which is what
    // every scene authored so far contains.
    FontAtlas& atlas = SharedAtlas();
    ENJIN_ASSERT_TRUE(atlas.IsBuilt());

    // Act
    const Enjin::ECS::MeshComponent m = atlas.BuildTextMesh(MakeText("Hello World"));

    // Assert: eleven characters, the space baking no quad.
    ENJIN_EXPECT_TRUE(GlyphCount(m) == 10);
}

ENJIN_TEST(FontAtlas, MalformedUTF8TerminatesInsteadOfHanging) {
    // Arrange: a lone continuation byte, and a two-byte lead truncated at the
    // end of the string. A decoder that consumed ZERO bytes on bad input would
    // spin forever, so reaching the assertions at all is half the test.
    FontAtlas& atlas = SharedAtlas();
    ENJIN_ASSERT_TRUE(atlas.IsBuilt());
    const std::string lone      = U8({'a', 0x80, 'b'});
    const std::string truncated = U8({'a', 'b', 0xC3});

    // Act
    const Enjin::ECS::MeshComponent a = atlas.BuildTextMesh(MakeText(lone));
    const Enjin::ECS::MeshComponent b = atlas.BuildTextMesh(MakeText(truncated));

    // Assert: the well-formed characters still laid out either side of the bad byte.
    ENJIN_EXPECT_TRUE(GlyphCount(a) >= 2);
    ENJIN_EXPECT_TRUE(GlyphCount(b) == 2);
}

ENJIN_TEST(FontAtlas, WordWrapMeasuresCharactersNotBytes) {
    // Arrange: wrap width is measured from glyph advances. Byte indexing made
    // an accented word measure wider than it draws, so lines broke early.
    FontAtlas& atlas = SharedAtlas();
    ENJIN_ASSERT_TRUE(atlas.IsBuilt());
    const std::string e = EAcute();
    const std::string accented = e + e + e + e + " " + e + e + e + e;

    // Act
    const Enjin::ECS::MeshComponent a = atlas.BuildTextMesh(MakeText("aaaa aaaa"));
    const Enjin::ECS::MeshComponent b = atlas.BuildTextMesh(MakeText(accented));

    // Assert: eight visible glyphs each, whatever their byte lengths.
    ENJIN_EXPECT_TRUE(GlyphCount(a) == 8);
    ENJIN_EXPECT_TRUE(GlyphCount(b) == 8);
}

ENJIN_TEST_MAIN()
