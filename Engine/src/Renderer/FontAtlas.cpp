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

ECS::MeshComponent FontAtlas::BuildTextMesh(const ECS::TextComponent& tc) const {
    ECS::MeshComponent mesh;
    if (!m_Built || tc.text.empty()) return mesh;

    // Layout happens in "authored pixels" (glyph metrics scaled from kBasePx to
    // tc.fontSize) so wrapWidth keeps the same pixel semantic as the rasterizer
    // path. worldHeight then maps one line height to world units - fontSize
    // cancels out of the final size and only decides how much text fits a line.
    const f32 glyphScale = (tc.fontSize > 0.0f ? tc.fontSize : kBasePx) / kBasePx;
    const f32 lineHeightPx = m_LineHeight * glyphScale;
    if (lineHeightPx <= 0.0f) return mesh;
    const f32 world = (tc.worldHeight > 0.0f ? tc.worldHeight : 0.5f) / lineHeightPx;
    const f32 wrapPx = tc.wrapWidth > 0.0f ? tc.wrapWidth : 0.0f;

    // Pass 1: break the text into lines (explicit \n + word wrap at wrapPx).
    struct Line { std::string s; f32 width; };
    std::vector<Line> lines;
    {
        std::string cur;
        f32 curW = 0.0f;
        auto measure = [&](const std::string& s) {
            f32 w = 0.0f;
            for (usize i = 0; i < s.size(); ++i) {
                const Glyph* g = Find(static_cast<u32>(static_cast<unsigned char>(s[i])));
                if (!g) continue;
                w += g->xadvance * glyphScale;
                if (i + 1 < s.size())
                    w += Kern(static_cast<unsigned char>(s[i]), static_cast<unsigned char>(s[i + 1])) * glyphScale;
            }
            return w;
        };
        auto flush = [&]() { lines.push_back({cur, curW}); cur.clear(); curW = 0.0f; };
        std::string word;
        for (usize i = 0; i <= tc.text.size(); ++i) {
            char c = i < tc.text.size() ? tc.text[i] : '\n';
            if (c == ' ' || c == '\n') {
                if (!word.empty()) {
                    f32 wordW = measure(word);
                    f32 spaceW = cur.empty() ? 0.0f : measure(" ");
                    if (wrapPx > 0.0f && !cur.empty() && curW + spaceW + wordW > wrapPx) flush();
                    if (!cur.empty()) { cur += ' '; curW += measure(" "); }
                    cur += word; curW += wordW;
                    word.clear();
                }
                if (c == '\n' && i < tc.text.size()) flush();
            } else {
                word += c;
            }
        }
        if (!cur.empty() || lines.empty()) flush();
    }

    // Alignment box: wrap width when set, else the widest line.
    f32 maxW = wrapPx;
    if (maxW <= 0.0f)
        for (const Line& l : lines) maxW = std::max(maxW, l.width);

    // Pass 2: emit one quad per visible glyph. Bitmap space (x right, y down,
    // origin = block top-left) maps to world as (x*world, -y*world, 0).
    mesh.vertices.reserve(tc.text.size() * 4);
    mesh.indices.reserve(tc.text.size() * 6);
    const Math::Vector3 normal(0.0f, 0.0f, 1.0f);
    const Math::Vector4 color(tc.textColor.x, tc.textColor.y, tc.textColor.z, 1.0f);

    f32 baselineY = m_Ascent * glyphScale;
    for (const Line& line : lines) {
        f32 penX = 0.0f;
        if (tc.horizontalAlign == ECS::TextAlign::Center)      penX = (maxW - line.width) * 0.5f;
        else if (tc.horizontalAlign == ECS::TextAlign::Right)  penX = maxW - line.width;
        for (usize i = 0; i < line.s.size(); ++i) {
            u32 cp = static_cast<u32>(static_cast<unsigned char>(line.s[i]));
            const Glyph* g = Find(cp);
            if (!g) continue;
            if (g->w > 0.0f && g->h > 0.0f) {
                f32 x0 = penX + g->xoff * glyphScale;
                f32 y0 = baselineY + g->yoff * glyphScale;   // top (yoff < 0 above baseline)
                f32 x1 = x0 + g->w * glyphScale;
                f32 y1 = y0 + g->h * glyphScale;             // bottom
                u32 base = static_cast<u32>(mesh.vertices.size());
                // Match CreateSpriteQuad winding: bottom-left, bottom-right,
                // top-right, top-left, indices {0,1,2, 0,2,3}, CCW from +Z.
                // Atlas v grows downward, so bitmap bottom (y1) samples v1.
                ECS::MeshComponent::Vertex v;
                v.normal = normal; v.color = color;
                v.position = Math::Vector3(x0 * world, -y1 * world, 0.0f); v.uv = Math::Vector2(g->u0, g->v1);
                mesh.vertices.push_back(v);
                v.position = Math::Vector3(x1 * world, -y1 * world, 0.0f); v.uv = Math::Vector2(g->u1, g->v1);
                mesh.vertices.push_back(v);
                v.position = Math::Vector3(x1 * world, -y0 * world, 0.0f); v.uv = Math::Vector2(g->u1, g->v0);
                mesh.vertices.push_back(v);
                v.position = Math::Vector3(x0 * world, -y0 * world, 0.0f); v.uv = Math::Vector2(g->u0, g->v0);
                mesh.vertices.push_back(v);
                mesh.indices.push_back(base + 0); mesh.indices.push_back(base + 1); mesh.indices.push_back(base + 2);
                mesh.indices.push_back(base + 0); mesh.indices.push_back(base + 2); mesh.indices.push_back(base + 3);
            }
            penX += g->xadvance * glyphScale;
            if (i + 1 < line.s.size())
                penX += Kern(cp, static_cast<unsigned char>(line.s[i + 1])) * glyphScale;
        }
        baselineY += lineHeightPx;
    }
    return mesh;
}

f32 FontAtlas::Kern(u32 a, u32 b) const {
    if (!m_Built || !m_FontInfo) return 0.0f;
    const stbtt_fontinfo& font = *static_cast<const stbtt_fontinfo*>(m_FontInfo);
    return stbtt_GetCodepointKernAdvance(&font, static_cast<int>(a), static_cast<int>(b)) * m_Scale;
}

} // namespace Renderer
} // namespace Enjin
