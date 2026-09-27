#include "Enjin/GUI/TextSpacing.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <cfloat>
#include <cstdint>
#include <cstring>
#include <vector>

namespace Enjin {
namespace GUI {

namespace {

f32 s_LetterSpacing = 0.0f;
f32 s_WordSpacing = 0.0f;
f32 s_LineSpacing = 1.0f;

bool SpacingOff() { return s_LetterSpacing <= 0.0f && s_WordSpacing <= 0.0f && s_LineSpacing == 1.0f; }

struct Glyph { ImWchar c; f32 x; f32 y; };

// Lays the text out glyph by glyph and returns its size. Lines break at '\n',
// and at the last space that fits when wrapWidth > 0.
ImVec2 Layout(ImFont* font, f32 size, const char* text, f32 wrapWidth, std::vector<Glyph>* out) {
    ImFontBaked* baked = font->GetFontBaked(size);
    const f32 lineH = size * s_LineSpacing;
    const char* end = text + strlen(text);

    // Decode once
    std::vector<ImWchar> chars;
    for (const char* p = text; p < end;) {
        unsigned int c = 0;
        const int n = ImTextCharFromUtf8(&c, p, end);
        if (n <= 0) break;
        p += n;
        chars.push_back(static_cast<ImWchar>(c));
    }
    auto advance = [&](ImWchar c) {
        return baked->GetCharAdvance(c) + s_LetterSpacing + (c == ' ' ? s_WordSpacing : 0.0f);
    };

    f32 maxW = 0.0f, y = 0.0f;
    usize i = 0;
    while (i <= chars.size()) {
        // One line: [i, lineEnd)
        usize lineEnd = i;
        f32 w = 0.0f;
        usize lastSpace = SIZE_MAX;
        while (lineEnd < chars.size() && chars[lineEnd] != '\n') {
            const f32 a = advance(chars[lineEnd]);
            // The gap after a glyph only counts once another glyph follows it
            if (wrapWidth > 0.0f && w + a - s_LetterSpacing > wrapWidth && lineEnd > i) {
                if (lastSpace != SIZE_MAX && lastSpace > i) lineEnd = lastSpace;
                break;
            }
            if (chars[lineEnd] == ' ') lastSpace = lineEnd;
            w += a;
            ++lineEnd;
        }
        f32 x = 0.0f;
        for (usize k = i; k < lineEnd; ++k) {
            if (out) out->push_back({chars[k], x, y});
            x += advance(chars[k]);
        }
        // Trailing spacing after the last glyph is not part of the width
        if (x > 0.0f) x -= s_LetterSpacing;
        maxW = x > maxW ? x : maxW;
        y += lineH;
        if (lineEnd >= chars.size()) break;
        // Skip the break character (a newline, or the space the wrap used)
        i = (chars[lineEnd] == '\n' || chars[lineEnd] == ' ') ? lineEnd + 1 : lineEnd;
    }
    // The last line is the font's height, not the spaced one: the spacing is
    // between lines, and a single line should not grow
    if (y > 0.0f) y -= lineH - size;
    return ImVec2(maxW, y);
}

} // namespace

void SetTextLetterSpacing(f32 pixels) { s_LetterSpacing = pixels > 0.0f ? pixels : 0.0f; }
f32 GetTextLetterSpacing() { return s_LetterSpacing; }
void SetTextWordSpacing(f32 pixels) { s_WordSpacing = pixels > 0.0f ? pixels : 0.0f; }
void SetTextLineSpacing(f32 multiplier) { s_LineSpacing = multiplier > 1.0f ? multiplier : 1.0f; }

ImVec2 CalcTextSizeSpaced(ImFont* font, f32 size, const char* text, f32 wrapWidth) {
    if (!font || !text) return ImVec2(0.0f, 0.0f);
    if (SpacingOff()) return font->CalcTextSizeA(size, FLT_MAX, wrapWidth, text);
    return Layout(font, size, text, wrapWidth, nullptr);
}

void AddTextSpaced(ImDrawList* dl, ImFont* font, f32 size, const ImVec2& pos, u32 color,
                   const char* text, f32 wrapWidth) {
    if (!dl || !font || !text) return;
    if (SpacingOff()) {
        dl->AddText(font, size, pos, color, text, nullptr, wrapWidth);
        return;
    }
    std::vector<Glyph> glyphs;
    Layout(font, size, text, wrapWidth, &glyphs);
    for (const Glyph& g : glyphs) {
        if (g.c == ' ' || g.c == '\t') continue;
        font->RenderChar(dl, size, ImVec2(pos.x + g.x, pos.y + g.y), color, g.c);
    }
}

} // namespace GUI
} // namespace Enjin
