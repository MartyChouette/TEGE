#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/GUI/UIElement.h"

#include <string>

namespace Enjin::GUI {

enum class UIThemePreset : u8 {
    Dark = 0,
    Light,
    RetroGreen,
    Fantasy,
    Count
};

struct UITheme {
    std::string name = "Dark";

    // Palette
    Math::Vector3 primary    = Math::Vector3(0.26f, 0.59f, 0.98f);
    Math::Vector3 secondary  = Math::Vector3(0.45f, 0.45f, 0.50f);
    Math::Vector3 background = Math::Vector3(0.08f, 0.08f, 0.10f);
    Math::Vector3 surface    = Math::Vector3(0.14f, 0.14f, 0.17f);
    Math::Vector3 error      = Math::Vector3(0.90f, 0.25f, 0.25f);

    // Text
    Math::Vector3 textPrimary   = Math::Vector3(0.95f, 0.95f, 0.97f);
    Math::Vector3 textSecondary = Math::Vector3(0.60f, 0.60f, 0.65f);
    Math::Vector3 textDisabled  = Math::Vector3(0.35f, 0.35f, 0.40f);

    // Button
    Math::Vector3 buttonDefault = Math::Vector3(0.26f, 0.59f, 0.98f);
    Math::Vector3 buttonHovered = Math::Vector3(0.36f, 0.69f, 1.00f);
    Math::Vector3 buttonPressed = Math::Vector3(0.16f, 0.49f, 0.88f);
    Math::Vector3 buttonDisabled = Math::Vector3(0.25f, 0.25f, 0.30f);

    // Input fields
    Math::Vector3 inputBg        = Math::Vector3(0.10f, 0.10f, 0.13f);
    Math::Vector3 inputBorder    = Math::Vector3(0.30f, 0.30f, 0.35f);
    Math::Vector3 inputFocused   = Math::Vector3(0.26f, 0.59f, 0.98f);

    // Slider / ProgressBar
    Math::Vector3 sliderTrack    = Math::Vector3(0.20f, 0.20f, 0.25f);
    Math::Vector3 sliderFill     = Math::Vector3(0.26f, 0.59f, 0.98f);
    Math::Vector3 sliderThumb    = Math::Vector3(0.90f, 0.90f, 0.95f);

    // Checkbox / Toggle
    Math::Vector3 checkboxBg       = Math::Vector3(0.15f, 0.15f, 0.20f);
    Math::Vector3 checkboxChecked  = Math::Vector3(0.26f, 0.59f, 0.98f);
    Math::Vector3 toggleOffBg      = Math::Vector3(0.30f, 0.30f, 0.35f);
    Math::Vector3 toggleOnBg       = Math::Vector3(0.26f, 0.59f, 0.98f);
    Math::Vector3 toggleKnob       = Math::Vector3(0.95f, 0.95f, 0.97f);

    // Border / spacing
    f32 borderRadius = 4.0f;
    f32 borderWidth  = 1.0f;
    f32 fontSizeBody    = 16.0f;
    f32 fontSizeHeading = 24.0f;
    f32 fontSizeSmall   = 12.0f;
    f32 spacing = 8.0f;

    // Background alpha
    f32 bgAlpha = 0.92f;

    // Nine-slice defaults per widget type (empty = flat color)
    NineSliceConfig panelNineSlice;
    NineSliceConfig buttonNineSlice;

    // Factory presets
    static UITheme Dark();
    static UITheme Light();
    static UITheme RetroGreen();
    static UITheme Fantasy();
    static UITheme FromPreset(UIThemePreset preset);
};

} // namespace Enjin::GUI
