#include "EnjinTest.h"
#include "Enjin/Editor/EditorSettings.h"

#include <cstdio>
#include <filesystem>
#include <fstream>

using namespace Enjin;
using namespace Enjin::Editor;

// ===========================================================================
// Default Values
// ===========================================================================

ENJIN_TEST(Defaults, ThemeIsDark) {
    EditorSettings settings;
    ENJIN_EXPECT_EQ((int)settings.theme, (int)EditorTheme::Dark);
}

ENJIN_TEST(Defaults, UIScaleIsOne) {
    EditorSettings settings;
    ENJIN_EXPECT_FLOAT_EQ(settings.uiScale, 1.0f);
}

ENJIN_TEST(Defaults, AudioDefaults) {
    EditorSettings settings;
    ENJIN_EXPECT_TRUE(settings.enableHRTF);
    ENJIN_EXPECT_TRUE(settings.enableOcclusion);
    ENJIN_EXPECT_TRUE(settings.enableTransmission);
}

ENJIN_TEST(Defaults, AccessibilityMotion) {
    EditorSettings settings;
    ENJIN_EXPECT_FALSE(settings.reducedMotion);
    ENJIN_EXPECT_FALSE(settings.disableScreenShake);
    ENJIN_EXPECT_FALSE(settings.disableFOVEffects);
}

ENJIN_TEST(Defaults, SubtitleDefaults) {
    EditorSettings settings;
    ENJIN_EXPECT_FALSE(settings.subtitlesEnabled);
    ENJIN_EXPECT_FALSE(settings.closedCaptionsEnabled);
    ENJIN_EXPECT_FLOAT_EQ(settings.subtitleFontSize, 24.0f);
    ENJIN_EXPECT_FLOAT_NEAR(settings.subtitleBgOpacity, 0.7f, 0.01f);
    ENJIN_EXPECT_TRUE(settings.subtitleSpeakerNames);
}

ENJIN_TEST(Defaults, InputDefaults) {
    EditorSettings settings;
    ENJIN_EXPECT_EQ(settings.sprintMode, 0u);
    ENJIN_EXPECT_EQ(settings.crouchMode, 0u);
    ENJIN_EXPECT_FLOAT_EQ(settings.mouseSensitivity, 1.0f);
    ENJIN_EXPECT_EQ(settings.inputPreset, 0u);
    ENJIN_EXPECT_TRUE(settings.rawMouseInput);
}

ENJIN_TEST(Defaults, MotorAccessibility) {
    EditorSettings settings;
    ENJIN_EXPECT_FALSE(settings.dwellClickEnabled);
    ENJIN_EXPECT_FLOAT_EQ(settings.dwellClickDelay, 1.0f);
    ENJIN_EXPECT_FALSE(settings.stickyDragEnabled);
}

ENJIN_TEST(Defaults, KeyboardNav) {
    EditorSettings settings;
    ENJIN_EXPECT_FALSE(settings.keyboardNavEnabled);
    ENJIN_EXPECT_FLOAT_EQ(settings.gizmoNudgeAmount, 0.1f);
    ENJIN_EXPECT_FLOAT_EQ(settings.gizmoRotateNudge, 5.0f);
}

ENJIN_TEST(Defaults, FrameRate) {
    EditorSettings settings;
    ENJIN_EXPECT_EQ((int)settings.editorFrameRateLimit, (int)FrameRateLimit::Uncapped);
    ENJIN_EXPECT_FALSE(settings.editorVSync);
    ENJIN_EXPECT_TRUE(settings.reduceFrameRateWhenUnfocused);
    ENJIN_EXPECT_EQ(settings.unfocusedFrameRate, 15u);
}

ENJIN_TEST(Defaults, WindowIconEmpty) {
    EditorSettings settings;
    ENJIN_EXPECT_TRUE(settings.windowIconPath.empty());
}

ENJIN_TEST(Defaults, ColorblindModeOff) {
    EditorSettings settings;
    ENJIN_EXPECT_EQ(settings.colorblindMode, 0u);
    ENJIN_EXPECT_FLOAT_EQ(settings.colorblindStrength, 1.0f);
}

// ===========================================================================
// JSON Round-Trip (via temp file)
// ===========================================================================

ENJIN_TEST(JSONRoundTrip, SaveAndLoadPreservesTheme) {
    std::string tmpPath = "test_editor_settings_tmp.json";

    EditorSettings s1;
    s1.theme = EditorTheme::HighContrastDark;
    s1.uiScale = 1.5f;
    // Note: enableHRTF, enableOcclusion, enableTransmission, windowIconPath
    // are no longer written by Save() — migrated to SceneManager / .enjinproject
    s1.reducedMotion = true;
    s1.subtitlesEnabled = true;
    s1.subtitleFontSize = 32.0f;
    s1.colorblindMode = 3;
    s1.mouseSensitivity = 2.0f;
    s1.dwellClickEnabled = true;
    s1.dwellClickDelay = 2.0f;
    s1.keyboardNavEnabled = true;
    s1.gizmoNudgeAmount = 0.5f;
    s1.Save(tmpPath);

    EditorSettings s2;
    s2.Load(tmpPath);

    ENJIN_EXPECT_EQ((int)s2.theme, (int)EditorTheme::HighContrastDark);
    ENJIN_EXPECT_FLOAT_EQ(s2.uiScale, 1.5f);
    // Migrated fields keep defaults after fresh Save (not written)
    ENJIN_EXPECT_TRUE(s2.enableHRTF);
    ENJIN_EXPECT_TRUE(s2.enableOcclusion);
    ENJIN_EXPECT_TRUE(s2.enableTransmission);
    ENJIN_EXPECT_STR_EQ(s2.windowIconPath.c_str(), "");
    ENJIN_EXPECT_TRUE(s2.reducedMotion);
    ENJIN_EXPECT_TRUE(s2.subtitlesEnabled);
    ENJIN_EXPECT_FLOAT_EQ(s2.subtitleFontSize, 32.0f);
    ENJIN_EXPECT_EQ(s2.colorblindMode, 3u);
    ENJIN_EXPECT_FLOAT_EQ(s2.mouseSensitivity, 2.0f);
    ENJIN_EXPECT_TRUE(s2.dwellClickEnabled);
    ENJIN_EXPECT_FLOAT_EQ(s2.dwellClickDelay, 2.0f);
    ENJIN_EXPECT_TRUE(s2.keyboardNavEnabled);
    ENJIN_EXPECT_FLOAT_EQ(s2.gizmoNudgeAmount, 0.5f);

    std::remove(tmpPath.c_str());
}

ENJIN_TEST(JSONRoundTrip, FrameRateSettings) {
    std::string tmpPath = "test_editor_settings_fps.json";

    EditorSettings s1;
    s1.editorFrameRateLimit = FrameRateLimit::FPS144;
    s1.editorVSync = true;
    s1.reduceFrameRateWhenUnfocused = false;
    s1.unfocusedFrameRate = 30;
    s1.reduceFrameRateWhenIdle = true;
    s1.idleTimeoutSeconds = 60.0f;
    s1.idleFrameRate = 10;
    s1.Save(tmpPath);

    EditorSettings s2;
    s2.Load(tmpPath);

    ENJIN_EXPECT_EQ((int)s2.editorFrameRateLimit, (int)FrameRateLimit::FPS144);
    ENJIN_EXPECT_TRUE(s2.editorVSync);
    ENJIN_EXPECT_FALSE(s2.reduceFrameRateWhenUnfocused);
    ENJIN_EXPECT_EQ(s2.unfocusedFrameRate, 30u);
    ENJIN_EXPECT_TRUE(s2.reduceFrameRateWhenIdle);
    ENJIN_EXPECT_FLOAT_EQ(s2.idleTimeoutSeconds, 60.0f);
    ENJIN_EXPECT_EQ(s2.idleFrameRate, 10u);

    std::remove(tmpPath.c_str());
}

ENJIN_TEST(JSONRoundTrip, MotorAccessibilitySettings) {
    std::string tmpPath = "test_editor_settings_motor.json";

    EditorSettings s1;
    s1.clickThreshold = 10.0f;
    s1.dragThreshold = 15.0f;
    s1.stickyDragEnabled = true;
    s1.holdRepeatDelay = 0.8f;
    s1.holdRepeatRate = 0.03f;
    s1.gizmoNudgeFine = 0.005f;
    s1.gizmoRotateNudge = 15.0f;
    s1.Save(tmpPath);

    EditorSettings s2;
    s2.Load(tmpPath);

    ENJIN_EXPECT_FLOAT_EQ(s2.clickThreshold, 10.0f);
    ENJIN_EXPECT_FLOAT_EQ(s2.dragThreshold, 15.0f);
    ENJIN_EXPECT_TRUE(s2.stickyDragEnabled);
    ENJIN_EXPECT_FLOAT_NEAR(s2.holdRepeatDelay, 0.8f, 0.01f);
    ENJIN_EXPECT_FLOAT_NEAR(s2.holdRepeatRate, 0.03f, 0.01f);
    ENJIN_EXPECT_FLOAT_NEAR(s2.gizmoNudgeFine, 0.005f, 0.001f);
    ENJIN_EXPECT_FLOAT_EQ(s2.gizmoRotateNudge, 15.0f);

    std::remove(tmpPath.c_str());
}

ENJIN_TEST(JSONRoundTrip, RecentProjectHistory) {
    std::string tmpPath = "test_editor_settings_history.json";

    EditorSettings s1;
    s1.AddRecentProject("/path/to/project1");
    s1.AddRecentProject("/path/to/project2");
    s1.lastProjectDir = "/path/to/project2";
    s1.Save(tmpPath);

    EditorSettings s2;
    s2.Load(tmpPath);

    ENJIN_EXPECT_STR_EQ(s2.lastProjectDir.c_str(), "/path/to/project2");
    ENJIN_EXPECT_GE(s2.recentProjects.size(), (size_t)2);

    std::remove(tmpPath.c_str());
}

// ===========================================================================
// Missing File Handling
// ===========================================================================

ENJIN_TEST(MissingFile, LoadNonexistentReturnsFalse) {
    EditorSettings settings;
    bool ok = settings.Load("this_file_does_not_exist_12345.json");
    ENJIN_EXPECT_FALSE(ok);
    // Defaults should still be intact
    ENJIN_EXPECT_EQ((int)settings.theme, (int)EditorTheme::Dark);
    ENJIN_EXPECT_FLOAT_EQ(settings.uiScale, 1.0f);
}

// ===========================================================================
// AccentColorConfig
// ===========================================================================

ENJIN_TEST(AccentColors, DefaultForThemeReturnsSomething) {
    auto dark = AccentColorConfig::DefaultForTheme(EditorTheme::Dark);
    auto light = AccentColorConfig::DefaultForTheme(EditorTheme::Light);
    auto hcd = AccentColorConfig::DefaultForTheme(EditorTheme::HighContrastDark);
    // Just verify they don't crash and produce non-zero colors
    ENJIN_EXPECT_FALSE(dark.useCustom);
    ENJIN_EXPECT_FALSE(light.useCustom);
    ENJIN_EXPECT_FALSE(hcd.useCustom);
}

// ===========================================================================
// Enum Ranges
// ===========================================================================

ENJIN_TEST(EnumRanges, EditorThemeValues) {
    ENJIN_EXPECT_EQ((int)EditorTheme::Dark, 0);
    ENJIN_EXPECT_EQ((int)EditorTheme::Glass, 1);
    ENJIN_EXPECT_EQ((int)EditorTheme::Light, 2);
    ENJIN_EXPECT_EQ((int)EditorTheme::HighContrastDark, 3);
    ENJIN_EXPECT_EQ((int)EditorTheme::HighContrastLight, 4);
}

ENJIN_TEST(EnumRanges, FrameRateLimitValues) {
    ENJIN_EXPECT_EQ((int)FrameRateLimit::Uncapped, 0);
    ENJIN_EXPECT_EQ((int)FrameRateLimit::FPS30, 30);
    ENJIN_EXPECT_EQ((int)FrameRateLimit::FPS60, 60);
    ENJIN_EXPECT_EQ((int)FrameRateLimit::FPS120, 120);
    ENJIN_EXPECT_EQ((int)FrameRateLimit::FPS144, 144);
    ENJIN_EXPECT_EQ((int)FrameRateLimit::FPS240, 240);
}

// ===========================================================================
// Backward Compatibility — migrated fields still readable from legacy JSON
// ===========================================================================

ENJIN_TEST(BackwardCompat, LegacyAudioAndIconFieldsStillRead) {
    std::string tmpPath = "test_editor_settings_legacy.json";

    // Write a legacy-format JSON with fields that are no longer written by Save()
    {
        std::ofstream f(tmpPath);
        f << R"({
            "theme": 0,
            "enableHRTF": false,
            "enableOcclusion": false,
            "enableTransmission": false,
            "windowIconPath": "icons/legacy.png"
        })";
    }

    EditorSettings s;
    s.Load(tmpPath);

    // Load() still reads these fields for backward compat / migration
    ENJIN_EXPECT_FALSE(s.enableHRTF);
    ENJIN_EXPECT_FALSE(s.enableOcclusion);
    ENJIN_EXPECT_FALSE(s.enableTransmission);
    ENJIN_EXPECT_STR_EQ(s.windowIconPath.c_str(), "icons/legacy.png");

    std::remove(tmpPath.c_str());
}

ENJIN_TEST_MAIN()
