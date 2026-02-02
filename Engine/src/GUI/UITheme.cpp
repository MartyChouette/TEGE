#include "Enjin/GUI/UITheme.h"

namespace Enjin::GUI {

UITheme UITheme::Dark() {
    UITheme t;
    t.name = "Dark";
    // All defaults in the header are the Dark theme values
    return t;
}

UITheme UITheme::Light() {
    UITheme t;
    t.name = "Light";

    t.primary    = Math::Vector3(0.20f, 0.47f, 0.85f);
    t.secondary  = Math::Vector3(0.50f, 0.50f, 0.55f);
    t.background = Math::Vector3(0.93f, 0.93f, 0.95f);
    t.surface    = Math::Vector3(1.00f, 1.00f, 1.00f);
    t.error      = Math::Vector3(0.85f, 0.20f, 0.20f);

    t.textPrimary   = Math::Vector3(0.10f, 0.10f, 0.12f);
    t.textSecondary = Math::Vector3(0.40f, 0.40f, 0.45f);
    t.textDisabled  = Math::Vector3(0.65f, 0.65f, 0.70f);

    t.buttonDefault  = Math::Vector3(0.20f, 0.47f, 0.85f);
    t.buttonHovered  = Math::Vector3(0.30f, 0.57f, 0.95f);
    t.buttonPressed  = Math::Vector3(0.15f, 0.37f, 0.75f);
    t.buttonDisabled = Math::Vector3(0.75f, 0.75f, 0.80f);

    t.inputBg      = Math::Vector3(0.96f, 0.96f, 0.98f);
    t.inputBorder  = Math::Vector3(0.75f, 0.75f, 0.80f);
    t.inputFocused = Math::Vector3(0.20f, 0.47f, 0.85f);

    t.sliderTrack = Math::Vector3(0.80f, 0.80f, 0.83f);
    t.sliderFill  = Math::Vector3(0.20f, 0.47f, 0.85f);
    t.sliderThumb = Math::Vector3(1.00f, 1.00f, 1.00f);

    t.checkboxBg      = Math::Vector3(0.90f, 0.90f, 0.93f);
    t.checkboxChecked  = Math::Vector3(0.20f, 0.47f, 0.85f);
    t.toggleOffBg      = Math::Vector3(0.75f, 0.75f, 0.78f);
    t.toggleOnBg       = Math::Vector3(0.20f, 0.47f, 0.85f);
    t.toggleKnob       = Math::Vector3(1.00f, 1.00f, 1.00f);

    t.bgAlpha = 0.95f;
    return t;
}

UITheme UITheme::RetroGreen() {
    UITheme t;
    t.name = "RetroGreen";

    t.primary    = Math::Vector3(0.20f, 0.80f, 0.20f);
    t.secondary  = Math::Vector3(0.15f, 0.50f, 0.15f);
    t.background = Math::Vector3(0.02f, 0.06f, 0.02f);
    t.surface    = Math::Vector3(0.04f, 0.12f, 0.04f);
    t.error      = Math::Vector3(0.80f, 0.20f, 0.10f);

    t.textPrimary   = Math::Vector3(0.20f, 0.90f, 0.20f);
    t.textSecondary = Math::Vector3(0.15f, 0.60f, 0.15f);
    t.textDisabled  = Math::Vector3(0.10f, 0.35f, 0.10f);

    t.buttonDefault  = Math::Vector3(0.10f, 0.40f, 0.10f);
    t.buttonHovered  = Math::Vector3(0.15f, 0.55f, 0.15f);
    t.buttonPressed  = Math::Vector3(0.08f, 0.30f, 0.08f);
    t.buttonDisabled = Math::Vector3(0.06f, 0.15f, 0.06f);

    t.inputBg      = Math::Vector3(0.03f, 0.08f, 0.03f);
    t.inputBorder  = Math::Vector3(0.15f, 0.50f, 0.15f);
    t.inputFocused = Math::Vector3(0.20f, 0.80f, 0.20f);

    t.sliderTrack = Math::Vector3(0.05f, 0.15f, 0.05f);
    t.sliderFill  = Math::Vector3(0.20f, 0.80f, 0.20f);
    t.sliderThumb = Math::Vector3(0.30f, 1.00f, 0.30f);

    t.checkboxBg      = Math::Vector3(0.03f, 0.10f, 0.03f);
    t.checkboxChecked  = Math::Vector3(0.20f, 0.80f, 0.20f);
    t.toggleOffBg      = Math::Vector3(0.08f, 0.25f, 0.08f);
    t.toggleOnBg       = Math::Vector3(0.20f, 0.80f, 0.20f);
    t.toggleKnob       = Math::Vector3(0.30f, 1.00f, 0.30f);

    t.borderRadius = 0.0f;
    t.borderWidth  = 2.0f;
    t.bgAlpha = 0.90f;
    return t;
}

UITheme UITheme::Fantasy() {
    UITheme t;
    t.name = "Fantasy";

    t.primary    = Math::Vector3(0.72f, 0.55f, 0.25f);
    t.secondary  = Math::Vector3(0.50f, 0.35f, 0.18f);
    t.background = Math::Vector3(0.12f, 0.08f, 0.05f);
    t.surface    = Math::Vector3(0.18f, 0.13f, 0.08f);
    t.error      = Math::Vector3(0.80f, 0.15f, 0.10f);

    t.textPrimary   = Math::Vector3(0.95f, 0.90f, 0.80f);
    t.textSecondary = Math::Vector3(0.70f, 0.60f, 0.45f);
    t.textDisabled  = Math::Vector3(0.45f, 0.38f, 0.28f);

    t.buttonDefault  = Math::Vector3(0.50f, 0.35f, 0.15f);
    t.buttonHovered  = Math::Vector3(0.65f, 0.48f, 0.22f);
    t.buttonPressed  = Math::Vector3(0.40f, 0.28f, 0.12f);
    t.buttonDisabled = Math::Vector3(0.25f, 0.18f, 0.10f);

    t.inputBg      = Math::Vector3(0.10f, 0.07f, 0.04f);
    t.inputBorder  = Math::Vector3(0.50f, 0.38f, 0.20f);
    t.inputFocused = Math::Vector3(0.72f, 0.55f, 0.25f);

    t.sliderTrack = Math::Vector3(0.15f, 0.10f, 0.06f);
    t.sliderFill  = Math::Vector3(0.72f, 0.55f, 0.25f);
    t.sliderThumb = Math::Vector3(0.95f, 0.85f, 0.60f);

    t.checkboxBg      = Math::Vector3(0.12f, 0.08f, 0.05f);
    t.checkboxChecked  = Math::Vector3(0.72f, 0.55f, 0.25f);
    t.toggleOffBg      = Math::Vector3(0.30f, 0.22f, 0.12f);
    t.toggleOnBg       = Math::Vector3(0.72f, 0.55f, 0.25f);
    t.toggleKnob       = Math::Vector3(0.95f, 0.85f, 0.60f);

    t.borderRadius = 6.0f;
    t.borderWidth  = 2.0f;
    t.bgAlpha = 0.94f;
    return t;
}

UITheme UITheme::FromPreset(UIThemePreset preset) {
    switch (preset) {
        case UIThemePreset::Light:      return Light();
        case UIThemePreset::RetroGreen: return RetroGreen();
        case UIThemePreset::Fantasy:    return Fantasy();
        default:                        return Dark();
    }
}

} // namespace Enjin::GUI
