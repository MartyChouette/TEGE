#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Accessibility/AccessibilitySettings.h"

#include <string>
#include <array>

namespace Enjin {
namespace Accessibility {

// Pattern types that accompany colors for redundant encoding
enum class PatternType : u8 {
    None = 0,
    Stripes,
    Dots,
    Crosshatch,
    Chevron
};

// A single palette entry: color + pattern + icon for redundant visual encoding
struct PaletteEntry {
    Math::Vector3 color;
    PatternType pattern = PatternType::None;
    std::string icon;  // Short text icon (e.g., "!" for error, "i" for info)
};

// A complete UI palette with 6 semantic color roles
struct UIColorPalette {
    PaletteEntry primary;
    PaletteEntry secondary;
    PaletteEntry success;
    PaletteEntry warning;
    PaletteEntry error;
    PaletteEntry info;
};

// Provides colorblind-safe UI palettes for each colorblind mode.
// Each palette uses colors that are distinguishable under the given
// color vision deficiency, combined with patterns and icons for
// redundant encoding that does not rely on color alone.
class ENJIN_API ColorblindPalette {
public:
    // Get the palette for a given colorblind mode.
    // Normal (Off) returns the standard palette.
    static const UIColorPalette& GetPalette(ColorblindMode mode);

    // Get a human-readable name for a colorblind mode
    static const char* GetModeName(ColorblindMode mode);

    // Get the number of available palettes (9 total: Normal + 8 modes)
    static constexpr u32 GetPaletteCount() { return 9; }

private:
    static void InitializePalettes();
    static bool s_Initialized;
    static std::array<UIColorPalette, 9> s_Palettes;
};

} // namespace Accessibility
} // namespace Enjin
