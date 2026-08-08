#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include <string>

namespace Enjin {

namespace Renderer {
    struct PostProcessSettings;
}

namespace Accessibility {

// Colorblind mode enum matching shader values
enum class ColorblindMode : u32 {
    Off = 0,
    Protanopia = 1,
    Deuteranopia = 2,
    Tritanopia = 3,
    Protanomaly = 4,
    Deuteranomaly = 5,
    Tritanomaly = 6,
    Achromatopsia = 7,
    Achromatomaly = 8   // partial color weakness (mild achromatopsia)
};

// Font family for accessibility text rendering
enum class FontFamily : u8 {
    Default = 0,
    Monospace,
    OpenDyslexic
};

// Runtime accessibility settings that games can expose to players
struct RuntimeAccessibilitySettings {
    // Visual
    ColorblindMode colorblindMode = ColorblindMode::Off;
    f32 colorblindStrength = 1.0f;
    f32 screenBrightness = 0.0f;
    f32 screenContrast = 1.0f;

    // Motion
    bool reducedMotion = false;
    bool disableScreenShake = false;
    bool disableFOVEffects = false;
    bool disableFlashingLights = false;

    // Subtitles
    bool subtitlesEnabled = false;
    bool closedCaptionsEnabled = false;
    f32 subtitleFontSize = 24.0f;
    f32 subtitleBgOpacity = 0.7f;
    bool subtitleSpeakerNames = true;
    bool subtitleDirectionIndicators = false;

    // Font scaling (applied to UISystem runtime font sizes)
    f32 fontScale = 1.0f;  // 0.5 - 3.0

    // Dyslexia-friendly text adjustments
    bool dyslexiaFriendly = false;
    f32 letterSpacing = 0.0f;   // Extra pixels between characters
    f32 wordSpacing = 0.0f;     // Extra pixels between words
    f32 lineSpacing = 1.0f;     // Line height multiplier (1.0-3.0)
    FontFamily fontFamily = FontFamily::Default;

    // Motor accessibility
    bool dwellClickEnabled = false;  // Hover to auto-click
    f32 dwellClickTime = 1.0f;      // Seconds to hover before auto-click (0.3-3.0)
    bool stickyDragEnabled = false;  // Sliders lock once drag starts
    bool switchAccessEnabled = false; // One-button scanning mode
    f32 switchScanSpeed = 1.5f;      // Seconds per element (0.5-5.0)

    // Audio visual indicators
    bool audioIndicatorsEnabled = false;

    // Screen reader (announcer TTS + status bar) in exported games
    bool screenReaderEnabled = false;

    // Input
    u32 sprintMode = 0;   // 0=Hold, 1=Toggle
    u32 crouchMode = 0;   // 0=Hold, 1=Toggle
    f32 mouseSensitivity = 1.0f;
    bool invertMouseY = false;

    // Apply visual settings to a PostProcessSettings struct
    void ApplyToPostProcessing(Renderer::PostProcessSettings& ppSettings) const;
};

} // namespace Accessibility
} // namespace Enjin
