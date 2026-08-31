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
    // the embedded default, same as TextRasterizer). Bakes ASCII 32..126 and
    // Latin-1 160..255. Returns false on font-parse failure.
    bool Build(const u8* fontFileData, usize fontFileSize);

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

    ~FontAtlas();

private:
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
