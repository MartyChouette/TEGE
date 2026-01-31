#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include <string>

namespace Enjin {
namespace Editor {

// Editor theme options
enum class EditorTheme : u32 {
    Dark = 0,
    Light,
    HighContrastDark,
    HighContrastLight
};

// Persistent editor settings (saved to disk as JSON)
struct EditorSettings {
    // Visual
    EditorTheme theme = EditorTheme::Dark;
    f32 uiScale = 1.0f;          // 0.75 - 2.0

    // Accessibility: Visual
    u32 colorblindMode = 0;       // 0=off, 1-7 = colorblind types
    f32 colorblindStrength = 1.0f;
    f32 screenBrightness = 0.0f;  // Additive brightness (-0.5 to 0.5)
    f32 screenContrast = 1.0f;    // Multiplicative contrast (0.5 to 2.0)

    // Accessibility: Motion
    bool reducedMotion = false;
    bool disableScreenShake = false;
    bool disableFOVEffects = false;

    // Accessibility: Subtitles / Cognitive
    bool subtitlesEnabled = false;
    bool closedCaptionsEnabled = false;
    f32 subtitleFontSize = 24.0f;  // 16-48
    f32 subtitleBgOpacity = 0.7f;
    bool subtitleSpeakerNames = true;
    bool simplifiedEditor = false;

    // Accessibility: Input
    u32 sprintMode = 0;      // 0=Hold, 1=Toggle
    u32 crouchMode = 0;      // 0=Hold, 1=Toggle
    f32 mouseSensitivity = 1.0f;
    u32 inputPreset = 0;     // 0=Default, 1=LeftHand, 2=RightHand, 3=GamepadOnly

    // Save/Load
    bool Save(const std::string& path = "") const;
    bool Load(const std::string& path = "");

    // Default save path
    static std::string GetDefaultPath();
};

} // namespace Editor
} // namespace Enjin
