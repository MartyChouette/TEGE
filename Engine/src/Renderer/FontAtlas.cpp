#include "Enjin/Renderer/FontAtlas.h"
#include "Enjin/Logging/Log.h"

#include <stb_truetype.h>

#include <algorithm>
#include <cstring>

namespace Enjin {
namespace Renderer {

FontAtlas::~FontAtlas() {
    delete static_cast<stbtt_fontinfo*>(m_FontInfo);
}

bool FontAtlas::Build(const u8* fontFileData, usize fontFileSize) {
    m_Built = false;
    m_Glyphs.clear();
    if (!fontFileData || fontFileSize == 0) return false;

    m_FontData.assign(fontFileData, fontFileData + fontFileSize);

    // stb_truetype is NOT robust against malformed data - a negative offset
    // fed to InitFont reads out of bounds (caught by TestFontAtlas).
    int fontOffset = stbtt_GetFontOffsetForIndex(m_FontData.data(), 0);
    if (fontOffset < 0 || static_cast<usize>(fontOffset) >= fontFileSize) {
        ENJIN_LOG_ERROR(Renderer, "FontAtlas: not a font file");
        return false;
    }
    if (!m_FontInfo) m_FontInfo = new stbtt_fontinfo();
    stbtt_fontinfo& font = *static_cast<stbtt_fontinfo*>(m_FontInfo);
    if (!stbtt_InitFont(&font, m_FontData.data(), fontOffset)) {
        ENJIN_LOG_ERROR(Renderer, "FontAtlas: font parse failed");
        return false;
    }

    m_Scale = stbtt_ScaleForPixelHeight(&font, kBasePx);
    int ascent = 0, descent = 0, lineGap = 0;
    stbtt_GetFontVMetrics(&font, &ascent, &descent, &lineGap);
    m_Ascent = ascent * m_Scale;
    m_Descent = descent * m_Scale;
    m_LineHeight = (ascent - descent + lineGap) * m_Scale;

    // RGBA atlas: RGB=255 (tintable through the normal base-color path),
    // A=0 background.
    m_Pixels.assign(static_cast<usize>(kAtlasSize) * kAtlasSize * 4, 0);
    for (usize i = 0; i < static_cast<usize>(kAtlasSize) * kAtlasSize; ++i) {
        m_Pixels[i * 4 + 0] = 255;
        m_Pixels[i * 4 + 1] = 255;
        m_Pixels[i * 4 + 2] = 255;
    }

    // The SDF encoding: value 180 on the outline, falling off ~22.5/px, so
    // the field spans the full padding ring.
    constexpr unsigned char kOnEdge = 180;
    const f32 distScale = static_cast<f32>(kOnEdge) / static_cast<f32>(kPadding);

    // Shelf packer.
    u32 penX = 1, penY = 1, rowH = 0;

    auto bake = [&](u32 cp) {
        if (stbtt_FindGlyphIndex(&font, static_cast<int>(cp)) == 0 && cp != ' ') return;
        int w = 0, h = 0, xoff = 0, yoff = 0;
        unsigned char* sdf = stbtt_GetCodepointSDF(&font, m_Scale, static_cast<int>(cp),
                                                   kPadding, kOnEdge, distScale,
                                                   &w, &h, &xoff, &yoff);
        int advance = 0, lsb = 0;
        stbtt_GetCodepointHMetrics(&font, static_cast<int>(cp), &advance, &lsb);

        Glyph g;
        g.xadvance = advance * m_Scale;

        if (sdf && w > 0 && h > 0) {
            if (penX + static_cast<u32>(w) + 1 > kAtlasSize) {   // new shelf
                penX = 1;
                penY += rowH + 1;
                rowH = 0;
            }
            if (penY + static_cast<u32>(h) + 1 > kAtlasSize) {
                ENJIN_LOG_WARN(Renderer, "FontAtlas: atlas full at codepoint %u", cp);
                stbtt_FreeSDF(sdf, nullptr);
                return;
            }
            for (int y = 0; y < h; ++y) {
                u8* dst = &m_Pixels[((penY + y) * kAtlasSize + penX) * 4];
                for (int x = 0; x < w; ++x) dst[x * 4 + 3] = sdf[y * w + x];
            }
            g.u0 = static_cast<f32>(penX) / kAtlasSize;
            g.v0 = static_cast<f32>(penY) / kAtlasSize;
            g.u1 = static_cast<f32>(penX + w) / kAtlasSize;
            g.v1 = static_cast<f32>(penY + h) / kAtlasSize;
            g.xoff = static_cast<f32>(xoff);
            g.yoff = static_cast<f32>(yoff);
            g.w = static_cast<f32>(w);
            g.h = static_cast<f32>(h);
            penX += static_cast<u32>(w) + 1;
            rowH = std::max(rowH, static_cast<u32>(h));
        }
        if (sdf) stbtt_FreeSDF(sdf, nullptr);
        m_Glyphs[cp] = g;
    };

    for (u32 cp = 32; cp <= 126; ++cp) bake(cp);
    for (u32 cp = 160; cp <= 255; ++cp) bake(cp);

    m_Built = !m_Glyphs.empty();
    if (m_Built) {
        ENJIN_LOG_INFO(Renderer, "FontAtlas: baked %zu glyphs (%ux%u SDF)",
                       m_Glyphs.size(), kAtlasSize, kAtlasSize);
    }
    return m_Built;
}

f32 FontAtlas::Kern(u32 a, u32 b) const {
    if (!m_Built || !m_FontInfo) return 0.0f;
    const stbtt_fontinfo& font = *static_cast<const stbtt_fontinfo*>(m_FontInfo);
    return stbtt_GetCodepointKernAdvance(&font, static_cast<int>(a), static_cast<int>(b)) * m_Scale;
}

} // namespace Renderer
} // namespace Enjin
