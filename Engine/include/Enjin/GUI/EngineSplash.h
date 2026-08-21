#pragma once

#include "Enjin/Platform/Types.h"

struct ImFont;

namespace Enjin {
namespace GUI {

// Draws the animated TEGE splash as a fullscreen ImGui overlay (must be
// called between ImGui frame begin/end). Used by the editor at startup and
// by the Player for the optional "Made with TEGE" intro card in built games.
//
//   timeSeconds  seconds since the splash started
//   duration     total lifetime; the overlay is fully faded at this point
//   fadeStart    when the fade-out begins (editor uses 3.0 / 4.0)
//   creditLine   small line under the title ("by marty64" in the editor,
//                "made with" in built games, nullptr to omit)
//   titleFont    font for the "TEGE" title (Playfair heading font); nullptr = default
void DrawEngineSplash(f32 timeSeconds, f32 duration, f32 fadeStart, const char* creditLine,
                      ImFont* titleFont = nullptr);

} // namespace GUI
} // namespace Enjin
