#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Accessibility/AccessibilitySettings.h"

#include <string>

namespace Enjin {
namespace Accessibility {

// FontFamily enum is defined in AccessibilitySettings.h

// Configuration for font rendering adjustments
struct FontLibraryConfig {
    FontFamily selectedFamily = FontFamily::Default;
    f32 letterSpacing = 0.0f;   // Extra pixels between characters (0-10)
    f32 wordSpacing = 0.0f;     // Extra pixels between words (0-20)
    f32 lineSpacing = 1.0f;     // Line height multiplier (1.0-3.0)
};

// Font library for accessibility font management.
// Manages font family selection and text spacing adjustments
// to improve readability for users with dyslexia or visual impairments.
class ENJIN_API FontLibrary {
public:
    FontLibrary() = default;
    ~FontLibrary() = default;

    // Set the active font family.
    // For OpenDyslexic: this loads the embedded font data into ImGui.
    // For Default/Monospace: uses the existing ImGui fonts.
    void SetFont(FontFamily family);

    // Get the currently active font family
    FontFamily GetCurrentFamily() const { return m_Config.selectedFamily; }

    // Configuration access
    const FontLibraryConfig& GetConfig() const { return m_Config; }
    void SetConfig(const FontLibraryConfig& config);

    // Apply text spacing adjustments.
    // Call this before rendering text to apply letter/word/line spacing.
    // Returns true if spacing was applied (caller should reset after rendering).
    bool ApplySpacing();

    // Reset spacing to default (call after rendering if ApplySpacing returned true)
    void ResetSpacing();

    // Check if the OpenDyslexic font is available (compiled in)
    static bool IsOpenDyslexicAvailable();

    // Get a human-readable name for a font family
    static const char* GetFamilyName(FontFamily family);

private:
    FontLibraryConfig m_Config;
    bool m_FontLoaded = false;
};

} // namespace Accessibility
} // namespace Enjin
