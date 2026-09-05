#include "EnjinTest.h"
#include <vector>
#include "Enjin/Renderer/TextCodepointSet.h"
#include "Enjin/GUI/EmbeddedFonts.h"
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

// ---------------------------------------------------------------------------
// Coloured runs, reveal, and measurement.
//
// A game that colours part of a line used to need one entity per colour, and
// then its own font arithmetic to place them. These cover the three pieces that
// replace that: runs colour by codepoint, revealCount cuts without reflowing,
// and MeasureTo answers where a character sits.
// ---------------------------------------------------------------------------

namespace {

// The colour of the first vertex of glyph n (four vertices per quad).
Enjin::Math::Vector4 GlyphColor(const Enjin::ECS::MeshComponent& m, usize n) {
    return m.vertices[n * 4].color;
}

bool SameColor(const Enjin::Math::Vector4& a, f32 r, f32 g, f32 b) {
    return std::fabs(a.x - r) < 0.001f && std::fabs(a.y - g) < 0.001f && std::fabs(a.z - b) < 0.001f;
}

} // namespace

ENJIN_TEST(FontAtlas, ARunColoursOnlyItsOwnCharacters) {
    // Arrange: "abcd" with the middle two red. One entity, one string.
    FontAtlas& atlas = SharedAtlas();
    ENJIN_ASSERT_TRUE(atlas.IsBuilt());
    Enjin::ECS::TextComponent tc = MakeText("abcd");
    tc.textColor = Enjin::Math::Vector3(1.0f, 1.0f, 1.0f);
    Enjin::ECS::TextRun run;
    run.start = 1; run.length = 2; run.color = Enjin::Math::Vector3(1.0f, 0.0f, 0.0f);
    tc.runs.push_back(run);

    // Act
    const Enjin::ECS::MeshComponent m = atlas.BuildTextMesh(tc);

    // Assert
    ENJIN_ASSERT_TRUE(GlyphCount(m) == 4);
    ENJIN_EXPECT_TRUE(SameColor(GlyphColor(m, 0), 1.0f, 1.0f, 1.0f));
    ENJIN_EXPECT_TRUE(SameColor(GlyphColor(m, 1), 1.0f, 0.0f, 0.0f));
    ENJIN_EXPECT_TRUE(SameColor(GlyphColor(m, 2), 1.0f, 0.0f, 0.0f));
    ENJIN_EXPECT_TRUE(SameColor(GlyphColor(m, 3), 1.0f, 1.0f, 1.0f));
}

ENJIN_TEST(FontAtlas, RunsCountCharactersNotBytes) {
    // Arrange: an accented lead character is two BYTES, so a byte-indexed run
    // would colour the wrong half of the word.
    FontAtlas& atlas = SharedAtlas();
    ENJIN_ASSERT_TRUE(atlas.IsBuilt());
    Enjin::ECS::TextComponent tc = MakeText(EAcute() + "ab");
    tc.textColor = Enjin::Math::Vector3(1.0f, 1.0f, 1.0f);
    Enjin::ECS::TextRun run;
    run.start = 1; run.length = 1; run.color = Enjin::Math::Vector3(0.0f, 1.0f, 0.0f);
    tc.runs.push_back(run);

    // Act
    const Enjin::ECS::MeshComponent m = atlas.BuildTextMesh(tc);

    // Assert: the SECOND character is green, not the second byte.
    ENJIN_ASSERT_TRUE(GlyphCount(m) == 3);
    ENJIN_EXPECT_TRUE(SameColor(GlyphColor(m, 0), 1.0f, 1.0f, 1.0f));
    ENJIN_EXPECT_TRUE(SameColor(GlyphColor(m, 1), 0.0f, 1.0f, 0.0f));
    ENJIN_EXPECT_TRUE(SameColor(GlyphColor(m, 2), 1.0f, 1.0f, 1.0f));
}

ENJIN_TEST(FontAtlas, ALaterRunWinsTheOverlap) {
    FontAtlas& atlas = SharedAtlas();
    ENJIN_ASSERT_TRUE(atlas.IsBuilt());
    Enjin::ECS::TextComponent tc = MakeText("abc");
    Enjin::ECS::TextRun wide;  wide.start = 0; wide.length = 3;
    wide.color = Enjin::Math::Vector3(1.0f, 0.0f, 0.0f);
    Enjin::ECS::TextRun narrow; narrow.start = 2; narrow.length = 1;
    narrow.color = Enjin::Math::Vector3(0.0f, 0.0f, 1.0f);
    tc.runs.push_back(wide);
    tc.runs.push_back(narrow);

    const Enjin::ECS::MeshComponent m = atlas.BuildTextMesh(tc);

    ENJIN_ASSERT_TRUE(GlyphCount(m) == 3);
    ENJIN_EXPECT_TRUE(SameColor(GlyphColor(m, 0), 1.0f, 0.0f, 0.0f));
    ENJIN_EXPECT_TRUE(SameColor(GlyphColor(m, 2), 0.0f, 0.0f, 1.0f));
}

ENJIN_TEST(FontAtlas, ARunPastTheEndIsIgnoredNotACrash) {
    FontAtlas& atlas = SharedAtlas();
    ENJIN_ASSERT_TRUE(atlas.IsBuilt());
    Enjin::ECS::TextComponent tc = MakeText("ab");
    Enjin::ECS::TextRun a; a.start = 99; a.length = 5;
    Enjin::ECS::TextRun b; b.start = -3; b.length = 2;
    Enjin::ECS::TextRun c; c.start = 1;  c.length = 999;   // clamps to the end
    c.color = Enjin::Math::Vector3(1.0f, 0.0f, 0.0f);
    tc.runs.push_back(a); tc.runs.push_back(b); tc.runs.push_back(c);

    const Enjin::ECS::MeshComponent m = atlas.BuildTextMesh(tc);

    ENJIN_ASSERT_TRUE(GlyphCount(m) == 2);
    ENJIN_EXPECT_TRUE(SameColor(GlyphColor(m, 1), 1.0f, 0.0f, 0.0f));
}

ENJIN_TEST(FontAtlas, RevealDrawsAPrefixAndMinusOneDrawsEverything) {
    FontAtlas& atlas = SharedAtlas();
    ENJIN_ASSERT_TRUE(atlas.IsBuilt());
    Enjin::ECS::TextComponent tc = MakeText("abcde");

    tc.revealCount = 0;
    ENJIN_EXPECT_TRUE(GlyphCount(atlas.BuildTextMesh(tc)) == 0);
    tc.revealCount = 3;
    ENJIN_EXPECT_TRUE(GlyphCount(atlas.BuildTextMesh(tc)) == 3);
    tc.revealCount = 99;                    // past the end is not an error
    ENJIN_EXPECT_TRUE(GlyphCount(atlas.BuildTextMesh(tc)) == 5);
    tc.revealCount = -1;
    ENJIN_EXPECT_TRUE(GlyphCount(atlas.BuildTextMesh(tc)) == 5);
}

ENJIN_TEST(FontAtlas, RevealingDoesNotReflowTheTextAroundIt) {
    // Arrange: a string that wraps. Typing it out one character at a time must
    // not move the characters already on the page - layout runs over the whole
    // string, and reveal only decides what is drawn.
    FontAtlas& atlas = SharedAtlas();
    ENJIN_ASSERT_TRUE(atlas.IsBuilt());
    Enjin::ECS::TextComponent tc = MakeText("aaaa bbbb cccc");
    tc.wrapWidth = 120.0f;                  // forces a break

    // Act
    const Enjin::ECS::MeshComponent whole = atlas.BuildTextMesh(tc);
    tc.revealCount = 6;
    const Enjin::ECS::MeshComponent partial = atlas.BuildTextMesh(tc);

    // Assert: every revealed glyph sits exactly where it sits in the full text.
    ENJIN_ASSERT_TRUE(GlyphCount(partial) > 0);
    ENJIN_ASSERT_TRUE(GlyphCount(whole) >= GlyphCount(partial));
    for (usize i = 0; i < partial.vertices.size(); ++i) {
        ENJIN_EXPECT_TRUE(std::fabs(partial.vertices[i].position.x - whole.vertices[i].position.x) < 0.0001f);
        ENJIN_EXPECT_TRUE(std::fabs(partial.vertices[i].position.y - whole.vertices[i].position.y) < 0.0001f);
    }
}

ENJIN_TEST(FontAtlas, AuthoredSpacingSurvivesLayout) {
    // Arrange: leading, trailing and repeated spaces are DATA. The old layout
    // rebuilt each line word by word joined with one space, so a caller could
    // not indent a line or align a column and nothing said why.
    FontAtlas& atlas = SharedAtlas();
    ENJIN_ASSERT_TRUE(atlas.IsBuilt());

    // Act: the same two words, one pair separated by one space and one by five.
    const Enjin::ECS::MeshComponent tight = atlas.BuildTextMesh(MakeText("ab cd"));
    const Enjin::ECS::MeshComponent wide  = atlas.BuildTextMesh(MakeText("ab     cd"));
    const Enjin::ECS::MeshComponent lead  = atlas.BuildTextMesh(MakeText("    ab"));
    const Enjin::ECS::MeshComponent bare  = atlas.BuildTextMesh(MakeText("ab"));

    // Assert: spaces draw nothing, so the glyph count never changes...
    ENJIN_ASSERT_TRUE(GlyphCount(tight) == 4);
    ENJIN_ASSERT_TRUE(GlyphCount(wide) == 4);
    ENJIN_ASSERT_TRUE(GlyphCount(lead) == 2);
    // ...but they take room: the wider gap pushes "cd" further right.
    ENJIN_EXPECT_TRUE(wide.vertices[2 * 4].position.x > tight.vertices[2 * 4].position.x + 0.001f);
    // ...and a leading run indents the first character.
    ENJIN_EXPECT_TRUE(lead.vertices[0].position.x > bare.vertices[0].position.x + 0.001f);
}

ENJIN_TEST(FontAtlas, AWrapStillEatsTheSpacesAtTheBreak) {
    // A line should not open with the gap that broke it: the spaces at a wrap
    // point are consumed, so the second line starts flush left.
    FontAtlas& atlas = SharedAtlas();
    ENJIN_ASSERT_TRUE(atlas.IsBuilt());
    Enjin::ECS::TextComponent tc = MakeText("aaaa bbbb");
    tc.wrapWidth = 100.0f;

    const Enjin::ECS::MeshComponent m = atlas.BuildTextMesh(tc);

    ENJIN_ASSERT_TRUE(GlyphCount(m) == 8);
    // Compare PENS, not quad edges: a quad is padded outward from its pen by
    // the SDF margin and by the glyph's own bearing, so two different letters
    // starting two lines do not share a quad x even when both start flush left.
    const Enjin::Math::Vector3 lineOne = atlas.MeasureTo(tc, 0);   // 'a'
    const Enjin::Math::Vector3 lineTwo = atlas.MeasureTo(tc, 5);   // 'b', after the space
    ENJIN_EXPECT_TRUE(std::fabs(lineTwo.x - lineOne.x) < 0.0001f);
    // And they really are on different lines.
    ENJIN_EXPECT_TRUE(lineTwo.y < lineOne.y - 0.0001f);
}

ENJIN_TEST(FontAtlas, MeasureToWalksAcrossAndDownWithTheText) {
    FontAtlas& atlas = SharedAtlas();
    ENJIN_ASSERT_TRUE(atlas.IsBuilt());
    Enjin::ECS::TextComponent tc = MakeText("abc\ndef");

    // Act
    const Enjin::Math::Vector3 first = atlas.MeasureTo(tc, 0);
    const Enjin::Math::Vector3 third = atlas.MeasureTo(tc, 2);
    const Enjin::Math::Vector3 nextLine = atlas.MeasureTo(tc, 4);   // 'd', after the newline
    const Enjin::Math::Vector3 past = atlas.MeasureTo(tc, 999);

    // Assert: the block's first character sits at the local origin, later
    // characters move right, and a later line moves down.
    ENJIN_EXPECT_TRUE(std::fabs(first.x) < 0.0001f);
    ENJIN_EXPECT_TRUE(std::fabs(first.y) < 0.0001f);
    ENJIN_EXPECT_TRUE(third.x > first.x + 0.001f);
    ENJIN_EXPECT_TRUE(nextLine.y < first.y - 0.001f);
    ENJIN_EXPECT_TRUE(std::fabs(nextLine.x) < 0.0001f);
    // Past the end answers the pen after the last character, not the origin.
    ENJIN_EXPECT_TRUE(past.x > 0.001f);
}

ENJIN_TEST(FontAtlas, MeasureToLandsOnTheGlyphItNames) {
    // The caret and the glyph must agree, or a typing game draws the cursor in
    // the wrong place by exactly one character.
    FontAtlas& atlas = SharedAtlas();
    ENJIN_ASSERT_TRUE(atlas.IsBuilt());
    Enjin::ECS::TextComponent tc = MakeText("abcd");

    const Enjin::ECS::MeshComponent m = atlas.BuildTextMesh(tc);
    ENJIN_ASSERT_TRUE(GlyphCount(m) == 4);

    // Pens advance strictly left to right, one character at a time.
    const Enjin::Math::Vector3 pen1 = atlas.MeasureTo(tc, 1);
    const Enjin::Math::Vector3 pen2 = atlas.MeasureTo(tc, 2);
    const Enjin::Math::Vector3 pen3 = atlas.MeasureTo(tc, 3);
    ENJIN_EXPECT_TRUE(pen1.x < pen2.x);
    ENJIN_EXPECT_TRUE(pen2.x < pen3.x);

    // And the pen for character 2 falls inside the quad drawn for character 2.
    // The quad is padded outward from the pen (SDF margin + bearing), so it
    // brackets the pen rather than starting at it - that bracket is the
    // guarantee a caret needs.
    const f32 glyphLeft  = m.vertices[2 * 4 + 0].position.x;
    const f32 glyphRight = m.vertices[2 * 4 + 1].position.x;
    ENJIN_EXPECT_TRUE(glyphLeft <= pen2.x + 0.0001f);
    ENJIN_EXPECT_TRUE(pen2.x <= glyphRight + 0.0001f);
}

ENJIN_TEST(FontAtlas, TextWithNoRunsIsUntouched) {
    // The default path must not change: no runs, no reveal, same mesh as ever.
    FontAtlas& atlas = SharedAtlas();
    ENJIN_ASSERT_TRUE(atlas.IsBuilt());
    Enjin::ECS::TextComponent tc = MakeText("Hello World");
    tc.textColor = Enjin::Math::Vector3(0.25f, 0.5f, 0.75f);

    const Enjin::ECS::MeshComponent m = atlas.BuildTextMesh(tc);

    ENJIN_ASSERT_TRUE(GlyphCount(m) == 10);          // the space draws nothing
    for (usize i = 0; i < GlyphCount(m); ++i)
        ENJIN_EXPECT_TRUE(SameColor(GlyphColor(m, i), 0.25f, 0.5f, 0.75f));
}


// ---------------------------------------------------------------------------
// Demand-driven baking.
//
// The atlas cannot hold every codepoint a font has: at kBasePx with kPadding a
// glyph occupies roughly 64x64 in a 1024 atlas, so about 250 fit, and ASCII
// plus Latin-1 is already 191. Adding the whole Cyrillic block would overflow
// and silently drop glyphs -- so in-world text simply could not be localised.
//
// What a project USES is far smaller than what its font can express. A Russian
// game needs 66 letters, not a 255-codepoint block, so the caller passes the
// codepoints its strings actually contain.
// ---------------------------------------------------------------------------

ENJIN_TEST(FontAtlas, TheBaseRangeIsBakedWithNoExtras) {
    // Arrange: unchanged behaviour for every project that asks for nothing.
    FontAtlas atlas;
    ENJIN_ASSERT_TRUE(atlas.Build(Accessibility::s_OpenDyslexicFontData,
                                  Accessibility::s_OpenDyslexicFontDataSize));

    // Act / Assert: ASCII and Latin-1 present, Cyrillic absent.
    ENJIN_EXPECT_TRUE(atlas.Find('A') != nullptr);
    ENJIN_EXPECT_TRUE(atlas.Find(0x00E9u) != nullptr);      // e-acute
    ENJIN_EXPECT_TRUE(atlas.Find(0x041Fu) == nullptr);      // Cyrillic PE
}

ENJIN_TEST(FontAtlas, ARequestedCodepointOutsideTheBaseRangeIsBaked) {
    // Arrange: the whole point. Roboto carries 255 Cyrillic glyphs, and the
    // atlas has room for the handful a game shows.
    const std::vector<Enjin::u32> wanted = { 0x041Fu, 0x0440u, 0x0438u,
                                             0x0432u, 0x0435u, 0x0442u };  // Privet
    FontAtlas atlas;

    // Act
    ENJIN_ASSERT_TRUE(atlas.Build(Enjin::GUI::RobotoMediumTTF,
                                  Enjin::GUI::RobotoMediumTTFSize, wanted));

    // Assert: every requested letter has a real quad, not just an entry.
    for (Enjin::u32 cp : wanted) {
        const FontAtlas::Glyph* g = atlas.Find(cp);
        ENJIN_ASSERT_TRUE(g != nullptr);
        ENJIN_EXPECT_TRUE(g->w > 0.0f && g->h > 0.0f);
    }
    // And the base range still came along.
    ENJIN_EXPECT_TRUE(atlas.Find('A') != nullptr);
}

ENJIN_TEST(FontAtlas, ExtrasDoNotDisplaceTheBaseRange) {
    // Arrange: the shelf packer fills in bake order, so extras are packed after
    // ASCII. If they ever crowded it out, every existing scene would lose
    // letters to make room for a locale it is not using.
    const std::vector<Enjin::u32> wanted = { 0x0416u, 0x0417u, 0x0418u, 0x0419u };
    FontAtlas atlas;

    // Act
    ENJIN_ASSERT_TRUE(atlas.Build(Enjin::GUI::RobotoMediumTTF,
                                  Enjin::GUI::RobotoMediumTTFSize, wanted));

    // Assert: all of printable ASCII survived.
    for (Enjin::u32 cp = 33; cp <= 126; ++cp) {
        ENJIN_ASSERT_TRUE(atlas.Find(cp) != nullptr);
    }
}

ENJIN_TEST(FontAtlas, DuplicateAndAlreadyBakedRequestsAreHarmless) {
    // Arrange: the collector returns whatever the strings contained, so it can
    // repeat, and a caller should not have to filter.
    const std::vector<Enjin::u32> wanted = { 0x041Fu, 0x041Fu, 0x041Fu,
                                             'A', 'A', 0x00E9u, 7u, 0u };
    FontAtlas atlas;

    // Act
    ENJIN_ASSERT_TRUE(atlas.Build(Enjin::GUI::RobotoMediumTTF,
                                  Enjin::GUI::RobotoMediumTTFSize, wanted));

    // Assert
    ENJIN_EXPECT_TRUE(atlas.Find(0x041Fu) != nullptr);
    ENJIN_EXPECT_TRUE(atlas.Find('A') != nullptr);
    ENJIN_EXPECT_TRUE(atlas.Find(7u) == nullptr);      // control char, never baked
}

ENJIN_TEST(FontAtlas, ACodepointTheFontLacksIsSkippedNotFabricated) {
    // Arrange: a project asking for a script its font has no glyphs for must
    // get nothing for it, rather than a box or a wrong glyph.
    const std::vector<Enjin::u32> wanted = { 0x4E00u, 0x3042u };   // CJK, Hiragana
    FontAtlas atlas;

    // Act
    ENJIN_ASSERT_TRUE(atlas.Build(Enjin::GUI::RobotoMediumTTF,
                                  Enjin::GUI::RobotoMediumTTFSize, wanted));

    // Assert
    ENJIN_EXPECT_TRUE(atlas.Find(0x4E00u) == nullptr);
    ENJIN_EXPECT_TRUE(atlas.Find('A') != nullptr);     // and the build still succeeded
}

ENJIN_TEST(FontAtlas, TheSameSetHashesTheSameAndADifferentSetDoesNot) {
    // Arrange: the hash is the font-atlas cache key. Two character sets against
    // one font path are two different atlases -- without this the first baked
    // would be returned forever, and switching locale would look inert.
    const std::vector<Enjin::u32> a = { 0x041Fu, 0x0440u, 0x0438u };
    const std::vector<Enjin::u32> b = { 0x041Fu, 0x0440u, 0x0438u };
    const std::vector<Enjin::u32> c = { 0x041Fu, 0x0440u, 0x0439u };

    // Act / Assert
    ENJIN_EXPECT_TRUE(Renderer::HashCodepoints(a) == Renderer::HashCodepoints(b));
    ENJIN_EXPECT_TRUE(Renderer::HashCodepoints(a) != Renderer::HashCodepoints(c));
    ENJIN_EXPECT_TRUE(Renderer::HashCodepoints(a) !=
                      Renderer::HashCodepoints(std::vector<Enjin::u32>()));
}

ENJIN_TEST_MAIN()
