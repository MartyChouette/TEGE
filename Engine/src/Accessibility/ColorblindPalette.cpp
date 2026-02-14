#include "Enjin/Accessibility/ColorblindPalette.h"

namespace Enjin {
namespace Accessibility {

bool ColorblindPalette::s_Initialized = false;
std::array<UIColorPalette, 9> ColorblindPalette::s_Palettes;

void ColorblindPalette::InitializePalettes() {
    if (s_Initialized) return;
    s_Initialized = true;

    // ========================================================================
    // Normal vision (Off) — standard web-style palette
    // ========================================================================
    {
        auto& p = s_Palettes[0];
        p.primary   = { Math::Vector3(0.20f, 0.47f, 0.96f), PatternType::None,       "*" };
        p.secondary = { Math::Vector3(0.40f, 0.40f, 0.45f), PatternType::None,       "~" };
        p.success   = { Math::Vector3(0.18f, 0.75f, 0.30f), PatternType::None,       "+" };
        p.warning   = { Math::Vector3(0.95f, 0.75f, 0.10f), PatternType::Dots,       "!" };
        p.error     = { Math::Vector3(0.90f, 0.20f, 0.20f), PatternType::Stripes,    "X" };
        p.info      = { Math::Vector3(0.10f, 0.70f, 0.90f), PatternType::None,       "i" };
    }

    // ========================================================================
    // Protanopia (no red cones) — avoid red/green distinction
    // Use blue/orange/yellow axis
    // ========================================================================
    {
        auto& p = s_Palettes[1];
        p.primary   = { Math::Vector3(0.00f, 0.45f, 0.85f), PatternType::None,       "*" };
        p.secondary = { Math::Vector3(0.45f, 0.45f, 0.50f), PatternType::None,       "~" };
        p.success   = { Math::Vector3(0.00f, 0.60f, 0.80f), PatternType::None,       "+" };
        p.warning   = { Math::Vector3(0.95f, 0.80f, 0.00f), PatternType::Dots,       "!" };
        p.error     = { Math::Vector3(0.90f, 0.55f, 0.00f), PatternType::Stripes,    "X" };
        p.info      = { Math::Vector3(0.40f, 0.70f, 0.95f), PatternType::None,       "i" };
    }

    // ========================================================================
    // Deuteranopia (no green cones) — similar to protanopia
    // Use blue/orange axis
    // ========================================================================
    {
        auto& p = s_Palettes[2];
        p.primary   = { Math::Vector3(0.00f, 0.45f, 0.85f), PatternType::None,       "*" };
        p.secondary = { Math::Vector3(0.45f, 0.45f, 0.50f), PatternType::None,       "~" };
        p.success   = { Math::Vector3(0.00f, 0.55f, 0.75f), PatternType::None,       "+" };
        p.warning   = { Math::Vector3(0.95f, 0.75f, 0.00f), PatternType::Dots,       "!" };
        p.error     = { Math::Vector3(0.85f, 0.50f, 0.00f), PatternType::Stripes,    "X" };
        p.info      = { Math::Vector3(0.35f, 0.65f, 0.95f), PatternType::None,       "i" };
    }

    // ========================================================================
    // Tritanopia (no blue cones) — avoid blue/yellow distinction
    // Use red/cyan axis
    // ========================================================================
    {
        auto& p = s_Palettes[3];
        p.primary   = { Math::Vector3(0.80f, 0.20f, 0.40f), PatternType::None,       "*" };
        p.secondary = { Math::Vector3(0.45f, 0.45f, 0.45f), PatternType::None,       "~" };
        p.success   = { Math::Vector3(0.00f, 0.65f, 0.60f), PatternType::None,       "+" };
        p.warning   = { Math::Vector3(0.85f, 0.40f, 0.50f), PatternType::Dots,       "!" };
        p.error     = { Math::Vector3(0.90f, 0.15f, 0.20f), PatternType::Stripes,    "X" };
        p.info      = { Math::Vector3(0.00f, 0.55f, 0.55f), PatternType::None,       "i" };
    }

    // ========================================================================
    // Protanomaly (reduced red sensitivity) — shifted protanopia
    // ========================================================================
    {
        auto& p = s_Palettes[4];
        p.primary   = { Math::Vector3(0.10f, 0.45f, 0.90f), PatternType::None,       "*" };
        p.secondary = { Math::Vector3(0.42f, 0.42f, 0.48f), PatternType::None,       "~" };
        p.success   = { Math::Vector3(0.05f, 0.65f, 0.75f), PatternType::None,       "+" };
        p.warning   = { Math::Vector3(0.95f, 0.78f, 0.05f), PatternType::Dots,       "!" };
        p.error     = { Math::Vector3(0.88f, 0.50f, 0.05f), PatternType::Stripes,    "X" };
        p.info      = { Math::Vector3(0.38f, 0.68f, 0.92f), PatternType::None,       "i" };
    }

    // ========================================================================
    // Deuteranomaly (reduced green sensitivity) — most common CVD
    // ========================================================================
    {
        auto& p = s_Palettes[5];
        p.primary   = { Math::Vector3(0.05f, 0.45f, 0.88f), PatternType::None,       "*" };
        p.secondary = { Math::Vector3(0.43f, 0.43f, 0.48f), PatternType::None,       "~" };
        p.success   = { Math::Vector3(0.00f, 0.58f, 0.78f), PatternType::None,       "+" };
        p.warning   = { Math::Vector3(0.95f, 0.76f, 0.00f), PatternType::Dots,       "!" };
        p.error     = { Math::Vector3(0.87f, 0.52f, 0.00f), PatternType::Stripes,    "X" };
        p.info      = { Math::Vector3(0.36f, 0.66f, 0.94f), PatternType::None,       "i" };
    }

    // ========================================================================
    // Tritanomaly (reduced blue sensitivity)
    // ========================================================================
    {
        auto& p = s_Palettes[6];
        p.primary   = { Math::Vector3(0.70f, 0.25f, 0.50f), PatternType::None,       "*" };
        p.secondary = { Math::Vector3(0.44f, 0.44f, 0.46f), PatternType::None,       "~" };
        p.success   = { Math::Vector3(0.00f, 0.62f, 0.58f), PatternType::None,       "+" };
        p.warning   = { Math::Vector3(0.88f, 0.45f, 0.48f), PatternType::Dots,       "!" };
        p.error     = { Math::Vector3(0.88f, 0.18f, 0.22f), PatternType::Stripes,    "X" };
        p.info      = { Math::Vector3(0.00f, 0.52f, 0.52f), PatternType::None,       "i" };
    }

    // ========================================================================
    // Achromatopsia (complete color blindness — grayscale vision)
    // Rely entirely on luminance differences + patterns + icons
    // ========================================================================
    {
        auto& p = s_Palettes[7];
        p.primary   = { Math::Vector3(0.30f, 0.30f, 0.30f), PatternType::None,       "*" };
        p.secondary = { Math::Vector3(0.55f, 0.55f, 0.55f), PatternType::None,       "~" };
        p.success   = { Math::Vector3(0.75f, 0.75f, 0.75f), PatternType::Crosshatch, "+" };
        p.warning   = { Math::Vector3(0.60f, 0.60f, 0.60f), PatternType::Dots,       "!" };
        p.error     = { Math::Vector3(0.15f, 0.15f, 0.15f), PatternType::Stripes,    "X" };
        p.info      = { Math::Vector3(0.85f, 0.85f, 0.85f), PatternType::Chevron,    "i" };
    }

    // ========================================================================
    // Copy palette[0] (Normal) to palette[8] as a duplicate for enum safety
    // In case the enum ever extends, palette[8] = Normal fallback
    // ========================================================================
    s_Palettes[8] = s_Palettes[0];
}

const UIColorPalette& ColorblindPalette::GetPalette(ColorblindMode mode) {
    InitializePalettes();
    u32 idx = static_cast<u32>(mode);
    if (idx >= s_Palettes.size()) idx = 0;
    return s_Palettes[idx];
}

const char* ColorblindPalette::GetModeName(ColorblindMode mode) {
    switch (mode) {
        case ColorblindMode::Off:            return "Normal";
        case ColorblindMode::Protanopia:     return "Protanopia";
        case ColorblindMode::Deuteranopia:   return "Deuteranopia";
        case ColorblindMode::Tritanopia:     return "Tritanopia";
        case ColorblindMode::Protanomaly:    return "Protanomaly";
        case ColorblindMode::Deuteranomaly:  return "Deuteranomaly";
        case ColorblindMode::Tritanomaly:    return "Tritanomaly";
        case ColorblindMode::Achromatopsia:  return "Achromatopsia";
        default:                             return "Unknown";
    }
}

} // namespace Accessibility
} // namespace Enjin
