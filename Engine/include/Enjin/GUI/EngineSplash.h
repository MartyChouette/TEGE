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

// Extended form: the splash restyles itself per art-style preset and can carry
// the accessibility statement.
//
//   artStylePreset  0-6, same indices as the scene Art Style Preset dropdown
//                   (Realistic PBR, Blinn-Phong, Hand-Painted, Toon/Anime,
//                   Low-Poly Retro, Pixel Art, NPR Sketch). The splash palette
//                   and mood follow the game's chosen look.
//   a11yLine        small statement under the credit ("ships with colorblind
//                   modes, a screen reader, remappable input"); nullptr = omit
//   reducedMotion   honor the player's reduced-motion setting: decorative
//                   drift/orbit layers are skipped, fades remain
struct SplashOptions {
    u32 artStylePreset = 0;
    const char* a11yLine = nullptr;
    bool reducedMotion = false;
};

void DrawEngineSplash(f32 timeSeconds, f32 duration, f32 fadeStart, const char* creditLine,
                      ImFont* titleFont, const SplashOptions& opts);

} // namespace GUI
} // namespace Enjin
