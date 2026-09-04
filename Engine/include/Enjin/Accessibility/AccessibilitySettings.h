#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include <string>

namespace Enjin {

namespace Renderer {
    struct PostProcessSettings;
}

namespace GUI { class UISystem; }

namespace Accessibility {

class SubtitleSystem;
class AccessibilityAnnouncer;

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

    // Gaze / head pointing. AlternativeInputManager has always had the whole
    // path -- smoothing, dead zone, dwell-to-select, indicator -- but nothing
    // could switch it on because it had no entry here, so it never appeared in
    // any menu and never saved.
    //
    // Assistive head pointers and eye-gaze systems present to the OS as a
    // mouse, which is what the manager reads, so this works with the hardware
    // people own rather than requiring a specific SDK.
    bool eyeTrackingEnabled = false;
    f32 eyeDwellTime = 1.0f;         // Seconds of gaze before a click (0.3-3.0)
    f32 eyeSmoothing = 0.3f;         // 0 = raw and jittery, 1 = very laggy
    f32 eyeDeadZone = 5.0f;          // Pixels of movement ignored
    bool eyeShowGazeIndicator = true;

    // Audio visual indicators
    bool audioIndicatorsEnabled = false;

    // Screen reader (announcer TTS + status bar) in exported games
    bool screenReaderEnabled = false;

    // NOTE: sprint/crouch toggle, mouse sensitivity and invert-Y are NOT here.
    // They live on InputActionMap (persisted in bindings.json), which is what
    // ControllerSystem actually reads. Keeping copies here meant the Controls
    // menu and the Accessibility menu edited different state and the file
    // round-tripped values the game never applied.

    // Apply visual settings to a PostProcessSettings struct
    void ApplyToPostProcessing(Renderer::PostProcessSettings& ppSettings) const;

    // The ONE serializer for these settings. Used for the player's
    // accessibility.json and for the project-level defaults an exported game
    // ships with, so "what the editor configured" and "what the game loads"
    // cannot drift into different key sets.
    std::string ToJson() const;
    bool FromJson(const std::string& jsonStr);
};

// Push the text-scaling settings to everything that draws text. One call so a
// player's font scale reaches the UI, subtitles AND the screen-reader bar;
// before this it reached only the UI, and subtitles kept a separate size.
// Any argument may be null.
ENJIN_API void ApplyTextScale(const RuntimeAccessibilitySettings& settings,
                              GUI::UISystem* ui,
                              SubtitleSystem* subtitles,
                              AccessibilityAnnouncer* announcer);

} // namespace Accessibility
} // namespace Enjin
