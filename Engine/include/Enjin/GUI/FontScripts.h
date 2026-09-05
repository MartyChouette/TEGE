#pragma once
// Which writing system a locale needs baked into the font atlas.
//
// The atlas was built with no GlyphRanges at all, so ImGui used its default --
// Basic Latin and Latin-1, about 190 glyphs. That threw away most of what the
// embedded face already contains (Roboto-Medium carries the whole Latin
// Extended-A block, 75 Greek glyphs and 255 Cyrillic ones), and it is the same
// atlas every runtime draws game UI, subtitles and the announcer from, so the
// gap was never editor-only.
//
// European coverage is now unconditional: it costs a few hundred glyphs and
// makes Polish, Czech, Turkish, Greek and Russian work from a font that always
// had them. The scripts below are different -- each is thousands of glyphs and
// a much larger atlas texture, which matters most on web -- so an atlas asks
// for one only when the active locale actually needs it.
//
// No embedded face contains these; a project wanting them ships a font. The
// atlas still has to request the codepoints, or the supplied font gets clipped
// back to Latin exactly as before.
#include "Enjin/Platform/Platform.h"

#include <string>

namespace Enjin {
namespace GUI {

enum class AtlasScript {
    EuropeanOnly,   // Latin + Latin Extended + Greek + Cyrillic: always baked
    Japanese,
    ChineseFull,
    Korean,
    Thai,
    Vietnamese
};

// Maps a locale code to the extra script its atlas needs.
//
// Matches on the language subtag, so "ja", "ja-JP" and "ja_JP" agree, and is
// case-insensitive because locale codes arrive from project files and from
// players. An unknown or empty code means EuropeanOnly, which is the safe
// answer: it is what every existing project already gets.
ENJIN_API AtlasScript ScriptForLocale(const std::string& localeCode);

} // namespace GUI
} // namespace Enjin
