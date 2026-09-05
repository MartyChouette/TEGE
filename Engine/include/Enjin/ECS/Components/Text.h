#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include <string>
#include <vector>

namespace Enjin {
namespace ECS {

// Horizontal text alignment
enum class TextAlign : i32 {
    Left = 0,
    Center = 1,
    Right = 2
};

// One coloured stretch of a TextComponent's string. A game that colours part of
// a line (a charged word in a dictated letter, a matched prefix, a spelling
// error) used to need ONE ENTITY PER COLOUR, because a TextComponent carried a
// single textColor. Every such layout then had to do its own font arithmetic to
// place the pieces, and a piece changing length moved everything after it.
//
// start/length count CODEPOINTS, not bytes, so they mean the same thing for
// accented text. length <= 0 runs to the end of the string. Runs are applied in
// order and may overlap; the last one covering a codepoint wins.
struct ENJIN_API TextRun {
    i32 start = 0;
    i32 length = 0;
    Math::Vector3 color = Math::Vector3(1.0f, 1.0f, 1.0f);
};

// Text component - rasterizes text to a texture that can be applied to any mesh surface
// Useful for books, signs, newspapers, UI panels in the 3D world
struct ENJIN_API TextComponent {
    std::string text;                   // Multi-line text content
    std::string fontPath;               // Path to TTF/OTF font file
    f32 fontSize = 32.0f;              // Font size in pixels
    f32 wrapWidth = 512.0f;            // Word wrap width in pixels
    u32 textureWidth = 512;            // Output texture width
    u32 textureHeight = 512;           // Output texture height
    Math::Vector3 textColor = Math::Vector3(1.0f, 1.0f, 1.0f);   // Text color (white)
    Math::Vector3 bgColor = Math::Vector3(0.0f, 0.0f, 0.0f);     // Background color (black)
    f32 bgOpacity = 1.0f;             // Background alpha (0 = transparent, 1 = opaque)
    TextAlign horizontalAlign = TextAlign::Left;
    f32 paddingX = 16.0f;             // Horizontal padding in pixels
    f32 paddingY = 16.0f;             // Vertical padding in pixels
    bool dirty = true;                 // Internal flag - triggers re-rasterization when true

    // SDF glyph-mesh path (unified display P1). Applies ONLY to bare text
    // entities (no authored mesh): instead of rasterizing to a texture on an
    // auto-quad, the text becomes a mesh of glyph quads sampling the shared
    // SDF font atlas - crisp at any scale, and a text change is a tiny mesh
    // rebuild with ZERO texture churn. Entities with their own mesh keep the
    // texture path (text painted on a surface: books, signs).
    bool sdfText = true;
    // World height of ONE text line for the glyph mesh (the transform's scale
    // multiplies this). Explicit so the pixel->world mapping has no magic.
    f32 worldHeight = 0.5f;
    // SDF path shading: false = flat/unlit (always readable), true = full PBR
    // so scene light falls on the words (candlelit pages). Texture-path text is
    // always lit by its material, as before.
    bool lit = false;

    // Coloured stretches of `text`. Empty (the default) = the whole string
    // takes textColor, exactly as before. SDF path only.
    std::vector<TextRun> runs;

    // Draw only the first N codepoints; -1 (the default) draws everything.
    // Layout always runs over the WHOLE string, so revealing does not reflow -
    // a word that will wrap is already on its final line before it appears.
    // This is the typing primitive: one entity, one string, revealCount++.
    i32 revealCount = -1;

    TextComponent() = default;
    TextComponent(const std::string& content) : text(content) {}
};

} // namespace ECS
} // namespace Enjin
