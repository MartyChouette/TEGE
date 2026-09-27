#pragma once
// Letter, word and line spacing for the text the game draws.
//
// Letter, Word and Line Spacing were accessibility options on every menu and
// did nothing. Their only consumer was FontLibrary, which edits the ImGui
// style of the editor and cannot space letters at all (ImGui has no per-glyph
// spacing). The game's canvases and subtitles draw through
// ImDrawList::AddText, so they need their own layout when spacing is on. With
// it off these are exactly AddText and CalcTextSizeA, so nothing changes for
// anyone who has not asked.
#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"

struct ImDrawList;
struct ImFont;
struct ImVec2;

namespace Enjin {
namespace GUI {

// Extra pixels after every glyph, extra pixels on every space, and a line
// height multiplier (1 = the font's own). Set by Accessibility::ApplyTextScale.
ENJIN_API void SetTextLetterSpacing(f32 pixels);
ENJIN_API f32 GetTextLetterSpacing();
ENJIN_API void SetTextWordSpacing(f32 pixels);
ENJIN_API void SetTextLineSpacing(f32 multiplier);

// CalcTextSizeA / AddText with the spacing applied. Word wrap follows
// ImGui's rule (break at spaces, a word longer than the line breaks anywhere).
ENJIN_API ImVec2 CalcTextSizeSpaced(ImFont* font, f32 size, const char* text, f32 wrapWidth = 0.0f);
ENJIN_API void AddTextSpaced(ImDrawList* dl, ImFont* font, f32 size, const ImVec2& pos, u32 color,
                             const char* text, f32 wrapWidth = 0.0f);

} // namespace GUI
} // namespace Enjin
