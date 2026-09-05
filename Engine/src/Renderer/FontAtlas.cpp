#include "Enjin/Renderer/FontAtlas.h"
#include "Enjin/Renderer/TextEncoding.h"
#include "Enjin/Logging/Log.h"

#include <stb_truetype.h>

#include <algorithm>
#include <cstring>
#include <vector>

namespace Enjin {
namespace Renderer {

FontAtlas::~FontAtlas() {
    delete static_cast<stbtt_fontinfo*>(m_FontInfo);
}

bool FontAtlas::Build(const u8* fontFileData, usize fontFileSize,
                      const std::vector<u32>& extraCodepoints) {
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
    // Then whatever this project's strings actually contain. bake() already
    // skips a codepoint the font has no glyph for, and m_Glyphs is a map, so a
    // duplicate of something baked above costs nothing.
    for (u32 cp : extraCodepoints) {
        if (cp < 32u) continue;                       // control characters
        if (cp <= 126u || (cp >= 160u && cp <= 255u)) continue;   // already baked
        bake(cp);
    }

    m_Built = !m_Glyphs.empty();
    if (m_Built) {
        ENJIN_LOG_INFO(Renderer, "FontAtlas: baked %zu glyphs (%ux%u SDF)",
                       m_Glyphs.size(), kAtlasSize, kAtlasSize);
    }
    return m_Built;
}

// The one layout pass. Produces a Placed for EVERY codepoint in the string,
// spaces included, so a caller can address a character by its source index:
// runs colour by it, revealCount cuts by it, MeasureTo finds a caret by it.
//
// Layout happens in "authored pixels" (glyph metrics scaled from kBasePx to
// tc.fontSize) so wrapWidth keeps the same pixel semantic as the rasterizer
// path. worldHeight then maps one line height to world units - fontSize
// cancels out of the final size and only decides how much text fits a line.
bool FontAtlas::LayoutText(const ECS::TextComponent& tc, std::vector<Placed>& out,
                           f32& world, f32& lineTop) const {
    out.clear();
    if (!m_Built || tc.text.empty()) return false;

    const f32 glyphScale = (tc.fontSize > 0.0f ? tc.fontSize : kBasePx) / kBasePx;
    const f32 lineHeightPx = m_LineHeight * glyphScale;
    if (lineHeightPx <= 0.0f) return false;
    world = (tc.worldHeight > 0.0f ? tc.worldHeight : 0.5f) / lineHeightPx;
    lineTop = m_Ascent * glyphScale;
    const f32 wrapPx = tc.wrapWidth > 0.0f ? tc.wrapWidth : 0.0f;

    // Layout runs over CODEPOINTS, never bytes. Indexing UTF-8 a byte at a
    // time turned every character above U+007F into two or more garbage
    // glyphs, so accented text rendered as mojibake even though Latin-1 is
    // baked into the atlas by codepoint and the glyphs were there all along.
    const std::vector<u32> text = DecodeUTF8All(tc.text);

    struct Cell { u32 cp; usize src; };

    // Width of a run of cells, including the kern against whatever precedes it
    // (prevCp = 0 for "nothing precedes"). A cell with no glyph takes no space,
    // matching emission, which skips it.
    auto runWidth = [&](const std::vector<Cell>& cells, u32 prevCp) {
        f32 w = 0.0f;
        u32 prev = prevCp;
        for (const Cell& c : cells) {
            const Glyph* g = Find(c.cp);
            if (!g) continue;
            if (prev != 0) w += Kern(prev, c.cp) * glyphScale;
            w += g->xadvance * glyphScale;
            prev = c.cp;
        }
        return w;
    };

    // Pass 1: break into lines (explicit newline + word wrap at wrapPx).
    //
    // SPACING IS AUTHORED DATA. This pass used to rebuild each line word by
    // word joined with one space, which silently ate leading and trailing
    // spaces and collapsed every run of them, so a caller could not align
    // columns, indent a line, or leave a gap - and nothing said why. Runs of
    // spaces are carried through verbatim now. Only a WRAP drops the spaces at
    // the break, because a line should not open with the gap that broke it.
    struct Line { std::vector<Cell> cells; f32 width; };
    std::vector<Line> lines;
    {
        std::vector<Cell> cur, gap, word;
        f32 curW = 0.0f;
        auto flush = [&]() { lines.push_back({cur, curW}); cur.clear(); curW = 0.0f; };
        auto take = [&](std::vector<Cell>& run) {
            if (run.empty()) return;
            curW += runWidth(run, cur.empty() ? 0u : cur.back().cp);
            cur.insert(cur.end(), run.begin(), run.end());
            run.clear();
        };
        auto commitWord = [&]() {
            if (word.empty()) return;
            const f32 gapW  = runWidth(gap, cur.empty() ? 0u : cur.back().cp);
            const f32 wordW = runWidth(word, gap.empty() ? (cur.empty() ? 0u : cur.back().cp)
                                                         : gap.back().cp);
            if (wrapPx > 0.0f && !cur.empty() && curW + gapW + wordW > wrapPx) {
                flush();
                gap.clear();          // the break consumes the spaces
            }
            take(gap);
            take(word);
        };

        for (usize i = 0; i <= text.size(); ++i) {
            const u32 c = (i < text.size()) ? text[i] : 10u;   // sentinel newline
            if (c == 32u) {
                commitWord();
                gap.push_back({c, i});
            } else if (c == 10u) {
                commitWord();
                take(gap);            // trailing spaces belong to the line they end
                if (i < text.size()) flush();
            } else {
                word.push_back({c, i});
            }
        }
        if (!cur.empty() || lines.empty()) flush();
    }

    // Alignment box: wrap width when set, else the widest line.
    f32 maxW = wrapPx;
    if (maxW <= 0.0f)
        for (const Line& l : lines) maxW = std::max(maxW, l.width);

    // Pass 2: walk the pen and record where every character landed.
    out.reserve(text.size() + lines.size());
    f32 baselineY = lineTop;
    for (const Line& line : lines) {
        f32 penX = 0.0f;
        if (tc.horizontalAlign == ECS::TextAlign::Center)      penX = (maxW - line.width) * 0.5f;
        else if (tc.horizontalAlign == ECS::TextAlign::Right)  penX = maxW - line.width;
        for (usize i = 0; i < line.cells.size(); ++i) {
            const Cell& c = line.cells[i];
            out.push_back({c.cp, c.src, penX, baselineY});
            const Glyph* g = Find(c.cp);
            if (!g) continue;
            penX += g->xadvance * glyphScale;
            if (i + 1 < line.cells.size()) penX += Kern(c.cp, line.cells[i + 1].cp) * glyphScale;
        }
        // A sentinel past the last character of the line, so a caret at the end
        // of a line has somewhere to sit.
        out.push_back({0u, kNoSource, penX, baselineY});
        baselineY += lineHeightPx;
    }
    return true;
}

ECS::MeshComponent FontAtlas::BuildTextMesh(const ECS::TextComponent& tc) const {
    ECS::MeshComponent mesh;
    std::vector<Placed> placed;
    f32 world = 0.0f, lineTop = 0.0f;
    if (!LayoutText(tc, placed, world, lineTop)) return mesh;

    const f32 glyphScale = (tc.fontSize > 0.0f ? tc.fontSize : kBasePx) / kBasePx;
    const usize cpCount = DecodeUTF8All(tc.text).size();

    // Per-codepoint colour: textColor everywhere, then each run painted over in
    // order so a later run wins the overlap. Resolved once, not searched per
    // glyph. No runs (the common case) allocates nothing.
    const Math::Vector4 baseColor(tc.textColor.x, tc.textColor.y, tc.textColor.z, 1.0f);
    std::vector<Math::Vector4> colorAt;
    if (!tc.runs.empty()) {
        colorAt.assign(cpCount, baseColor);
        for (const ECS::TextRun& r : tc.runs) {
            if (r.start < 0) continue;
            const usize from = static_cast<usize>(r.start);
            if (from >= cpCount) continue;
            const usize to = (r.length <= 0)
                ? cpCount
                : std::min(cpCount, from + static_cast<usize>(r.length));
            const Math::Vector4 c(r.color.x, r.color.y, r.color.z, 1.0f);
            for (usize i = from; i < to; ++i) colorAt[i] = c;
        }
    }

    // Draw only the first revealCount codepoints. Layout above already ran over
    // the whole string, so nothing reflows as characters appear.
    const bool limited = tc.revealCount >= 0;
    const usize revealTo = limited ? static_cast<usize>(tc.revealCount) : cpCount;

    mesh.vertices.reserve(placed.size() * 4);
    mesh.indices.reserve(placed.size() * 6);
    const Math::Vector3 normal(0.0f, 0.0f, 1.0f);

    for (const Placed& p : placed) {
        if (p.src == kNoSource) continue;                   // end-of-line sentinel
        if (limited && p.src >= revealTo) continue;
        const Glyph* g = Find(p.cp);
        if (!g || g->w <= 0.0f || g->h <= 0.0f) continue;   // spaces land here

        // Bitmap space (x right, y down, origin = block top-left) maps to world
        // as (x*world, -y*world, 0).
        const f32 x0 = p.penX + g->xoff * glyphScale;
        const f32 y0 = p.baselineY + g->yoff * glyphScale;  // top (yoff < 0 above baseline)
        const f32 x1 = x0 + g->w * glyphScale;
        const f32 y1 = y0 + g->h * glyphScale;              // bottom
        const u32 vbase = static_cast<u32>(mesh.vertices.size());
        // Match CreateSpriteQuad winding: bottom-left, bottom-right, top-right,
        // top-left, indices {0,1,2, 0,2,3}, CCW from +Z. Atlas v grows downward,
        // so bitmap bottom (y1) samples v1.
        ECS::MeshComponent::Vertex v;
        v.normal = normal;
        v.color = colorAt.empty() ? baseColor : colorAt[p.src];
        v.position = Math::Vector3(x0 * world, -y1 * world, 0.0f); v.uv = Math::Vector2(g->u0, g->v1);
        mesh.vertices.push_back(v);
        v.position = Math::Vector3(x1 * world, -y1 * world, 0.0f); v.uv = Math::Vector2(g->u1, g->v1);
        mesh.vertices.push_back(v);
        v.position = Math::Vector3(x1 * world, -y0 * world, 0.0f); v.uv = Math::Vector2(g->u1, g->v0);
        mesh.vertices.push_back(v);
        v.position = Math::Vector3(x0 * world, -y0 * world, 0.0f); v.uv = Math::Vector2(g->u0, g->v0);
        mesh.vertices.push_back(v);
        mesh.indices.push_back(vbase + 0); mesh.indices.push_back(vbase + 1); mesh.indices.push_back(vbase + 2);
        mesh.indices.push_back(vbase + 0); mesh.indices.push_back(vbase + 2); mesh.indices.push_back(vbase + 3);
    }
    return mesh;
}

Math::Vector3 FontAtlas::MeasureTo(const ECS::TextComponent& tc, i32 codepointIndex) const {
    std::vector<Placed> placed;
    f32 world = 0.0f, lineTop = 0.0f;
    if (!LayoutText(tc, placed, world, lineTop) || placed.empty())
        return Math::Vector3(0.0f, 0.0f, 0.0f);

    const usize want = codepointIndex < 0 ? 0u : static_cast<usize>(codepointIndex);
    const Placed* hit = nullptr;
    for (const Placed& p : placed) {
        if (p.src != kNoSource && p.src >= want) { hit = &p; break; }
    }
    // Past the last character: the pen at the end of the last line, which is
    // the final sentinel.
    if (!hit) hit = &placed.back();

    // Answered at the character's TOP-LEFT, the anchor the block itself uses.
    return Math::Vector3(hit->penX * world, -(hit->baselineY - lineTop) * world, 0.0f);
}

f32 FontAtlas::Kern(u32 a, u32 b) const {
    if (!m_Built || !m_FontInfo) return 0.0f;
    const stbtt_fontinfo& font = *static_cast<const stbtt_fontinfo*>(m_FontInfo);
    return stbtt_GetCodepointKernAdvance(&font, static_cast<int>(a), static_cast<int>(b)) * m_Scale;
}

} // namespace Renderer
} // namespace Enjin
