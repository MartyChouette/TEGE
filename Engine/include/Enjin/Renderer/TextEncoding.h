#pragma once
// UTF-8 decoding for text layout.
//
// Both of the engine's text paths -- FontAtlas (SDF glyph meshes) and
// TextRasterizer (bitmap text painted on a surface) -- used to walk a
// std::string one BYTE at a time and hand each byte to the font as a
// codepoint. Every character above U+007F therefore became two or more
// garbage glyphs. The bitmap path was worse still: it passed a signed `char`,
// so those bytes arrived as NEGATIVE codepoints.
//
// Latin-1 is baked into the atlas by codepoint, so the correct glyph was
// present the whole time and only the lookup was wrong. Nothing can be
// localised until layout counts characters.
#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"

#include <string>
#include <vector>

namespace Enjin {
namespace Renderer {

// Decodes one codepoint at `i`, advancing `i` past it.
//
// A malformed or truncated sequence consumes at least one byte and yields
// U+FFFD, so a caller looping to the end of the string always terminates.
inline u32 DecodeUTF8(const std::string& s, usize& i) {
    const usize n = s.size();
    if (i >= n) return 0;
    const auto byte = [&](usize k) { return static_cast<u32>(static_cast<unsigned char>(s[k])); };
    const u32 c0 = byte(i);

    if (c0 < 0x80u) { ++i; return c0; }

    u32 need = 0, cp = 0;
    if ((c0 & 0xE0u) == 0xC0u)      { need = 1; cp = c0 & 0x1Fu; }
    else if ((c0 & 0xF0u) == 0xE0u) { need = 2; cp = c0 & 0x0Fu; }
    else if ((c0 & 0xF8u) == 0xF0u) { need = 3; cp = c0 & 0x07u; }
    else { ++i; return 0xFFFDu; }              // stray continuation or invalid lead

    for (u32 k = 1; k <= need; ++k) {
        if (i + k >= n) { i = n; return 0xFFFDu; }       // truncated at end of string
        const u32 cc = byte(i + k);
        if ((cc & 0xC0u) != 0x80u) { i += k; return 0xFFFDu; }   // resync on the bad byte
        cp = (cp << 6) | (cc & 0x3Fu);
    }
    i += need + 1;
    return cp;
}

// The whole string as codepoints, so layout never has to think about bytes.
inline std::vector<u32> DecodeUTF8All(const std::string& s) {
    std::vector<u32> out;
    out.reserve(s.size());
    usize i = 0;
    while (i < s.size()) out.push_back(DecodeUTF8(s, i));
    return out;
}

} // namespace Renderer
} // namespace Enjin
