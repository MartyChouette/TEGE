#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include <string>
#include <vector>

namespace Enjin {
namespace Editor {

// Editor theme options
enum class EditorTheme : u32 {
    Dark = 0,
    Light,
    HighContrastDark,
    HighContrastLight
};

// Frame rate limit options (shared between editor and game settings)
enum class FrameRateLimit : u32 {
    Uncapped = 0,
    FPS30 = 30,
    FPS60 = 60,
    FPS120 = 120,
    FPS144 = 144,
    FPS240 = 240
};

// Helper to get target FPS value from enum
inline f32 GetTargetFPS(FrameRateLimit limit) {
    return limit == FrameRateLimit::Uncapped ? 0.0f : static_cast<f32>(static_cast<u32>(limit));
}

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

    // Play Mode
    bool autoFocusMode = false;  // Auto-enter focus mode when pressing Play
    bool lockCursorOnPlay = true; // Lock/capture cursor when entering play mode

    // Performance / Frame Rate
    FrameRateLimit editorFrameRateLimit = FrameRateLimit::Uncapped;
    bool editorVSync = false;
    bool reduceFrameRateWhenUnfocused = true;
    u32 unfocusedFrameRate = 15;
    bool reduceFrameRateWhenIdle = false;
    f32 idleTimeoutSeconds = 30.0f;
    u32 idleFrameRate = 30;

    // Accessibility: Input
    u32 sprintMode = 0;      // 0=Hold, 1=Toggle
    u32 crouchMode = 0;      // 0=Hold, 1=Toggle
    f32 mouseSensitivity = 1.0f;
    u32 inputPreset = 0;     // 0=Default, 1=LeftHand, 2=RightHand, 3=GamepadOnly
    bool rawMouseInput = true;
    f32 mouseSmoothing = 0.0f; // 0.0 = none, 1.0 = heavy

    // External IDE
    u32 externalIDE = 0;          // 0=Auto, 1=VS Code, 2=Visual Studio, 3=Rider, 4=Custom
    std::string customIDEPath;     // Only used when externalIDE == 4

    // Recent projects (most recent first, max 8)
    std::vector<std::string> recentProjects;
    static constexpr int MAX_RECENT_PROJECTS = 8;

    void AddRecentProject(const std::string& path);

    // Recently-used components (most recent first, max 5)
    std::vector<std::string> recentComponents;
    static constexpr int MAX_RECENT_COMPONENTS = 5;

    void AddRecentComponent(const std::string& name);

    // Recently-used visual script nodes (most recent first, max 5)
    std::vector<std::string> recentVisualScriptNodes;
    static constexpr int MAX_RECENT_VS_NODES = 5;

    void AddRecentVisualScriptNode(const std::string& nodeTypeId);

    // Save/Load
    bool Save(const std::string& path = "") const;
    bool Load(const std::string& path = "");

    // Default save path
    static std::string GetDefaultPath();
};

} // namespace Editor
} // namespace Enjin
