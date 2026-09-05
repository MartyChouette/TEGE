#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/ECS/Components/Mesh.h"
#include "Enjin/ECS/Components/Text.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace Enjin {
namespace Renderer {

// P1 of the unified display system (docs/DESIGN_UNIFIED_DISPLAY.md): a
// signed-distance-field glyph atlas baked once per font with stb_truetype's
// stbtt_GetCodepointSDF - zero new dependencies. Every SDF text consumer
// draws glyph quads sampling this atlas: crisp at any scale, and a text
// CHANGE is just new vertex data (no texture re-rasterize - the typewriter
// stops churning textures entirely).
//
// Atlas pixels are RGBA: RGB = 255 (so baseColor tinting works through the
// normal texture path) and A = the distance field (so even a shader WITHOUT
// the SDF flag renders soft-edged but correct text - graceful degradation).
class ENJIN_API FontAtlas {
public:
    struct Glyph {
        // Atlas UV rect
        f32 u0 = 0, v0 = 0, u1 = 0, v1 = 0;
        // Quad placement in pixels at kBasePx, relative to the baseline pen
        f32 xoff = 0, yoff = 0;      // top-left offset from the pen
        f32 w = 0, h = 0;            // quad size (includes SDF padding)
        f32 xadvance = 0;            // pen advance
    };

    // Baking parameters. 48px covers UI-to-billboard scales well; padding 8
    // gives the field enough range for outlines/glow later.
    static constexpr f32 kBasePx = 48.0f;
    static constexpr i32 kPadding = 8;
    static constexpr u32 kAtlasSize = 1024;

    // fontFileData = a complete TTF/OTF in memory (the caller resolves paths /
    // the embedded default, same as TextRasterizer). Returns false on
    // font-parse failure.
    //
    // Always bakes ASCII 32..126 and Latin-1 160..255, then `extraCodepoints`
    // on top.
    //
    // The extras exist because baking every codepoint a font HAS does not fit.
    // At kBasePx with kPadding a glyph occupies roughly 64x64 in a 1024 atlas,
    // so about 250 fit at all -- ASCII plus Latin-1 is already 191, and adding
    // the whole Cyrillic block would overflow and drop glyphs on the floor.
    // What a project actually USES is far smaller than what its font can
    // express: a Russian game needs 66 letters, not the 255-codepoint block.
    // So the caller passes the codepoints its strings contain.
    bool Build(const u8* fontFileData, usize fontFileSize,
               const std::vector<u32>& extraCodepoints = std::vector<u32>());

    bool IsBuilt() const { return m_Built; }
    const Glyph* Find(u32 codepoint) const {
        auto it = m_Glyphs.find(codepoint);
        return it != m_Glyphs.end() ? &it->second : nullptr;
    }
    // Kerning between two codepoints, pixels at kBasePx.
    f32 Kern(u32 a, u32 b) const;

    // Vertical metrics, pixels at kBasePx.
    f32 Ascent() const { return m_Ascent; }
    f32 Descent() const { return m_Descent; }   // negative (below baseline)
    f32 LineHeight() const { return m_LineHeight; }

    const std::vector<u8>& Pixels() const { return m_Pixels; }   // RGBA kAtlasSize^2
    u32 Width() const { return kAtlasSize; }
    u32 Height() const { return kAtlasSize; }

    // Build a glyph-quad mesh for a TextComponent (the SDF text path). Layout
    // honours \n, wrapWidth word-wrap (pixels at fontSize, same semantic as the
    // rasterizer), horizontalAlign, and kerning. worldHeight maps ONE line to
    // world units; the block's TOP-LEFT sits at the local origin, +x right,
    // lines descending -y, quads facing +Z. Vertex color carries textColor so
    // the shared white atlas tints per-entity with one material.
    ECS::MeshComponent BuildTextMesh(const ECS::TextComponent& tc) const;

    // Where a character sits, in the same local space BuildTextMesh emits into
    // (block top-left at the origin, +x right, lines descending -y). Pass the
    // codepoint index; index >= the string length answers with the pen at the
    // end of the last line, which is where a caret belongs after the last
    // character. Without this every caller re-derives font metrics by hand -
    // Ink Ribbon carried `charW = worldHeight * 1229/2320` as a magic number in
    // two languages to place one caret.
    Math::Vector3 MeasureTo(const ECS::TextComponent& tc, i32 codepointIndex) const;

    ~FontAtlas();

private:
    // One laid-out character: which codepoint, where it came from in the source
    // string (so runs and revealCount address it), and where the pen was.
    static constexpr usize kNoSource = static_cast<usize>(-1);   // end-of-line sentinel
    struct Placed {
        u32 cp = 0;
        usize src = 0;
        f32 penX = 0.0f;
        f32 baselineY = 0.0f;
    };
    // The single layout pass every consumer reads. `world` converts layout
    // pixels to world units; `lineTop` is the ascent offset that puts a line's
    // TOP at its baseline row, for callers that want the block-top anchor.
    bool LayoutText(const ECS::TextComponent& tc, std::vector<Placed>& out,
                    f32& world, f32& lineTop) const;

    bool m_Built = false;
    std::unordered_map<u32, Glyph> m_Glyphs;
    std::vector<u8> m_Pixels;
    std::vector<u8> m_FontData;     // kept alive for the kerning font info
    void* m_FontInfo = nullptr;     // stbtt_fontinfo, opaque here (impl header)
    f32 m_Ascent = 0, m_Descent = 0, m_LineHeight = 0;
    f32 m_Scale = 0;                // stbtt scale for kBasePx
};

} // namespace Renderer
} // namespace Enjin
