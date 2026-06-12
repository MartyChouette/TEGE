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
    Glass,              // Frosted glass with subtle prismatic borders
    Light,
    HighContrastDark,
    HighContrastLight,
    // Retro console themes
    SNES,
    PS2,
    Xbox,
    Dreamcast,
    SegaSaturn,
    GBA,
    DS
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

// Accent color (RGBA float)
struct AccentColor {
    f32 r, g, b, a;
    AccentColor() : r(0), g(0), b(0), a(1) {}
    AccentColor(f32 r_, f32 g_, f32 b_, f32 a_ = 1.0f) : r(r_), g(g_), b(b_), a(a_) {}
};

// Customizable accent color configuration for the editor UI
struct AccentColorConfig {
    AccentColor button       = {0.22f, 0.27f, 0.24f, 1.00f};
    AccentColor buttonHover  = {0.30f, 0.38f, 0.32f, 1.00f};
    AccentColor buttonActive = {0.35f, 0.45f, 0.38f, 1.00f};
    AccentColor checkMark    = {0.55f, 0.78f, 0.58f, 1.00f};
    AccentColor sliderGrab   = {0.50f, 0.70f, 0.52f, 1.00f};
    AccentColor sliderGrabActive = {0.60f, 0.82f, 0.63f, 1.00f};
    AccentColor resizeGrip   = {0.30f, 0.40f, 0.33f, 0.50f};
    AccentColor textSelected = {0.35f, 0.50f, 0.38f, 0.50f};
    AccentColor dragDropTarget = {0.55f, 0.78f, 0.58f, 0.90f};
    AccentColor tabActive    = {0.25f, 0.33f, 0.28f, 1.00f};
    AccentColor tabHovered   = {0.35f, 0.45f, 0.38f, 1.00f};
    bool useCustom = false;  // When false, theme defaults are used

    static AccentColorConfig DefaultDark();
    static AccentColorConfig DefaultLight();
    static AccentColorConfig DefaultHighContrastDark();
    static AccentColorConfig DefaultHighContrastLight();
    static AccentColorConfig DefaultSNES();
    static AccentColorConfig DefaultPS2();
    static AccentColorConfig DefaultXbox();
    static AccentColorConfig DefaultDreamcast();
    static AccentColorConfig DefaultSegaSaturn();
    static AccentColorConfig DefaultGBA();
    static AccentColorConfig DefaultDS();
    static AccentColorConfig DefaultForTheme(EditorTheme theme);
};

// Persistent editor settings (saved to disk as JSON)
struct EditorSettings {
    // Visual
    EditorTheme theme = EditorTheme::Dark;
    f32 uiScale = 1.0f;          // 0.75 - 2.0

    // Accent Colors
    AccentColorConfig accentColors;

    // Accessibility: Visual
    u32 colorblindMode = 0;       // 0=off, 1-7 = colorblind types
    f32 colorblindStrength = 1.0f;
    f32 screenBrightness = 0.0f;  // Additive brightness (-0.5 to 0.5)
    f32 screenContrast = 1.0f;    // Multiplicative contrast (0.5 to 2.0)

    // Accessibility: Motion
    bool reducedMotion = false;
    bool disableScreenShake = false;
    bool disableFOVEffects = false;
    bool disableFlashingLights = false;

    // Accessibility: Subtitles / Cognitive
    bool subtitlesEnabled = false;
    bool closedCaptionsEnabled = false;
    f32 subtitleFontSize = 24.0f;  // 16-48
    f32 subtitleBgOpacity = 0.7f;
    bool subtitleSpeakerNames = true;
    bool simplifiedEditor = false;
    bool dyslexiaFontEnabled = false;  // Increase letter/word/line spacing for readability
    f32 gameFontScale = 1.0f;          // Font scale for in-game UI (0.5-3.0)

    // Play Mode
    bool autoFocusMode = false;  // Auto-enter focus mode when pressing Play

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

    // Accessibility: Motor
    f32 clickThreshold = 5.0f;        // Pixels of movement before click becomes drag (1-20)
    f32 dragThreshold = 6.0f;         // Pixels before drag starts (1-30)
    bool dwellClickEnabled = false;   // Auto-click after hovering in place
    f32 dwellClickDelay = 1.0f;       // Seconds before dwell-click triggers (0.3-3.0)
    bool stickyDragEnabled = false;   // Click to start drag, click again to release
    f32 holdRepeatDelay = 0.5f;       // Initial hold-repeat delay in seconds (0.1-1.5)
    f32 holdRepeatRate = 0.05f;       // Hold-repeat rate in seconds (0.01-0.2)

    // Accessibility: Keyboard Navigation
    bool keyboardNavEnabled = false;  // Enable full keyboard-only editor navigation
    f32 gizmoNudgeAmount = 0.1f;      // World units per arrow-key nudge (0.01-10.0)
    f32 gizmoNudgeFine = 0.01f;       // Fine nudge (Ctrl+Arrow) units (0.001-1.0)
    f32 gizmoRotateNudge = 5.0f;      // Degrees per rotate nudge (1-45)

    // Audio
    bool enableHRTF = true;           // HRTF binaural audio (requires Steam Audio SDK)
    bool enableOcclusion = true;      // Audio occlusion by geometry
    bool enableTransmission = true;   // Frequency-dependent sound through walls

    // Surface Snap
    bool surfaceSnap = false;
    bool surfaceAlignNormal = true;

    // Window Icon
    std::string windowIconPath;    // Custom icon.png path (empty = default)

    // External IDE
    u32 externalIDE = 0;          // 0=Auto, 1=VS Code, 2=Visual Studio, 3=Rider, 4=Custom
    std::string customIDEPath;     // Only used when externalIDE == 4

    // Last directory used for project creation/open (persisted across sessions)
    std::string lastProjectDir;

    // Recent projects (most recent first, max 8)
    std::vector<std::string> recentProjects;
    static constexpr int MAX_RECENT_PROJECTS = 8;

    void AddRecentProject(const std::string& path);
    void RemoveRecentProject(const std::string& path);

    // Recently-used components (most recent first, max 5)
    std::vector<std::string> recentComponents;
    static constexpr int MAX_RECENT_COMPONENTS = 5;

    void AddRecentComponent(const std::string& name);

    // Recently-used visual script nodes (most recent first, max 5)
    std::vector<std::string> recentVisualScriptNodes;
    static constexpr int MAX_RECENT_VS_NODES = 5;

    void AddRecentVisualScriptNode(const std::string& nodeTypeId);

    // Layout persistence
    u32 visiblePanels = 0xFFFFFFFF;   // EditorPanel bitmask - all visible by default
    f32 leftPanelWidth = 0.18f;
    f32 rightPanelWidth = 0.25f;
    f32 bottomPanelHeight = 0.22f;
    u32 gizmoOperation = 0;           // 0=Translate, 1=Rotate, 2=Scale
    u32 gizmoSpace = 0;               // 0=Local, 1=World

    // Auto-save
    bool autoSaveEnabled = true;
    f32 autoSaveIntervalMinutes = 5.0f;

    // Discord bug report webhook
    std::string discordWebhookUrl;  // Empty = disabled; set in Settings > System

    // Discord feedback/survey webhook (falls back to discordWebhookUrl if empty)
    std::string discordFeedbackWebhookUrl;

    // Save/Load
    bool Save(const std::string& path = "") const;
    bool Load(const std::string& path = "");

    // Default save path
    static std::string GetDefaultPath();
};

} // namespace Editor
} // namespace Enjin
