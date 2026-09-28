#pragma once

#include "Enjin/GUI/UIFontRegistry.h"
#include <imgui.h>

namespace Enjin {
namespace GUI {

// Draws the enclosed game text in the dyslexia-friendly face while the
// accessibility option is on, at whatever size it was going to use. Subtitles,
// the screen-reader bar, interaction prompts and the controls hint all draw
// with ImGui's current font, and none of them changed face with the option
// (decided 2026-09-27: the option switches UI text, subtitles and prompts in
// every runtime). UI canvases take the face through UISystem::ResolveFace.
class ScopedGameTextFont {
public:
    ScopedGameTextFont() {
        if (ImFont* face = UIFontRegistry::Get().DyslexiaOverride()) {
            ImGui::PushFont(face, 0.0f);   // 0 = keep the current size
            m_Pushed = true;
        }
    }
    ~ScopedGameTextFont() {
        if (m_Pushed) ImGui::PopFont();
    }
    ScopedGameTextFont(const ScopedGameTextFont&) = delete;
    ScopedGameTextFont& operator=(const ScopedGameTextFont&) = delete;

private:
    bool m_Pushed = false;
};

} // namespace GUI
} // namespace Enjin
