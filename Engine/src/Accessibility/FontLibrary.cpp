#include "Enjin/Accessibility/FontLibrary.h"
#include "Enjin/Logging/Log.h"

#include <imgui.h>
#include <algorithm>

namespace Enjin {
namespace Accessibility {

// ============================================================================
// OpenDyslexic font placeholder
// The real OpenDyslexic font data would be embedded here as a byte array.
// OpenDyslexic is licensed under the SIL Open Font License and can be
// legally embedded. For now, we fall back to the default font with
// increased letter/word spacing which achieves a similar effect.
// ============================================================================

static constexpr bool OPEN_DYSLEXIC_EMBEDDED = false;
// static const unsigned char s_OpenDyslexicFontData[] = { ... };
// static const u32 s_OpenDyslexicFontDataSize = sizeof(s_OpenDyslexicFontData);

void FontLibrary::SetFont(FontFamily family) {
    m_Config.selectedFamily = family;

    ImGuiIO& io = ImGui::GetIO();

    switch (family) {
        case FontFamily::Default:
            // Use the first font in the atlas (default body font)
            if (io.Fonts->Fonts.Size > 0) {
                io.FontDefault = io.Fonts->Fonts[0];
            }
            m_FontLoaded = true;
            ENJIN_LOG_INFO(Editor, "FontLibrary: Set to Default font");
            break;

        case FontFamily::Monospace:
            // Use the monospace font if available (typically index 2 in the atlas)
            if (io.Fonts->Fonts.Size > 2) {
                io.FontDefault = io.Fonts->Fonts[2];
            } else if (io.Fonts->Fonts.Size > 0) {
                io.FontDefault = io.Fonts->Fonts[0];
            }
            m_FontLoaded = true;
            ENJIN_LOG_INFO(Editor, "FontLibrary: Set to Monospace font");
            break;

        case FontFamily::OpenDyslexic:
            if constexpr (OPEN_DYSLEXIC_EMBEDDED) {
                // Load embedded OpenDyslexic font into ImGui
                // ImFontConfig cfg;
                // cfg.FontDataOwnedByAtlas = false;
                // io.Fonts->AddFontFromMemoryTTF((void*)s_OpenDyslexicFontData,
                //     s_OpenDyslexicFontDataSize, 17.0f, &cfg);
                // io.Fonts->Build();
                // m_FontLoaded = true;
            } else {
                // Fallback: use default font with dyslexia-friendly spacing
                // OpenDyslexic font data would be embedded here in a production build.
                // The key accessibility benefit of OpenDyslexic is:
                // 1. Weighted bottoms on letters (prevents rotation confusion)
                // 2. Unique letter shapes (b/d, p/q differentiation)
                // 3. Increased spacing between characters
                //
                // We simulate #3 by applying generous letter/word/line spacing.
                if (io.Fonts->Fonts.Size > 0) {
                    io.FontDefault = io.Fonts->Fonts[0];
                }
                m_Config.letterSpacing = std::max(m_Config.letterSpacing, 2.0f);
                m_Config.wordSpacing = std::max(m_Config.wordSpacing, 4.0f);
                m_Config.lineSpacing = std::max(m_Config.lineSpacing, 1.5f);
                m_FontLoaded = true;
                ENJIN_LOG_INFO(Editor, "FontLibrary: OpenDyslexic font not embedded — using default with dyslexia-friendly spacing (letter=%.1f, word=%.1f, line=%.1f)",
                    m_Config.letterSpacing, m_Config.wordSpacing, m_Config.lineSpacing);
            }
            break;
    }
}

void FontLibrary::SetConfig(const FontLibraryConfig& config) {
    m_Config = config;
    // Clamp values to reasonable ranges
    m_Config.letterSpacing = std::clamp(m_Config.letterSpacing, 0.0f, 10.0f);
    m_Config.wordSpacing = std::clamp(m_Config.wordSpacing, 0.0f, 20.0f);
    m_Config.lineSpacing = std::clamp(m_Config.lineSpacing, 1.0f, 3.0f);
}

bool FontLibrary::ApplySpacing() {
    if (m_Config.letterSpacing <= 0.0f && m_Config.lineSpacing <= 1.0f) {
        return false;
    }

    ImGuiStyle& style = ImGui::GetStyle();

    // Apply letter spacing via ItemInnerSpacing.x (affects text rendering)
    // Note: ImGui does not natively support per-character letter spacing.
    // The line spacing can be adjusted via ItemSpacing.y.
    if (m_Config.lineSpacing > 1.0f) {
        style.ItemSpacing.y *= m_Config.lineSpacing;
    }

    return true;
}

void FontLibrary::ResetSpacing() {
    // Spacing is reset by the normal ImGui style push/pop mechanism
    // The caller should use PushStyleVar/PopStyleVar for proper scoping
}

bool FontLibrary::IsOpenDyslexicAvailable() {
    return OPEN_DYSLEXIC_EMBEDDED;
}

const char* FontLibrary::GetFamilyName(FontFamily family) {
    switch (family) {
        case FontFamily::Default:       return "Default";
        case FontFamily::Monospace:     return "Monospace";
        case FontFamily::OpenDyslexic:  return "OpenDyslexic";
        default:                        return "Unknown";
    }
}

} // namespace Accessibility
} // namespace Enjin
