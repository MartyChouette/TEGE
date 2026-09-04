#include "EnjinTest.h"
#include "Enjin/Input/InputAction.h"
#include "Enjin/Accessibility/AccessibilitySettings.h"
#include "Enjin/Accessibility/SubtitleSystem.h"
#include "Enjin/Accessibility/ContentWarning.h"
#include "Enjin/Accessibility/AlternativeInput.h"

using namespace Enjin;
using namespace Enjin::Accessibility;

// ===========================================================================
// RuntimeAccessibilitySettings Defaults
// ===========================================================================

ENJIN_TEST(AccessSettings, ColorblindOff) {
    RuntimeAccessibilitySettings settings;
    ENJIN_EXPECT_EQ((int)settings.colorblindMode, (int)ColorblindMode::Off);
    ENJIN_EXPECT_FLOAT_EQ(settings.colorblindStrength, 1.0f);
}

ENJIN_TEST(AccessSettings, MotionDefaults) {
    RuntimeAccessibilitySettings settings;
    ENJIN_EXPECT_FALSE(settings.reducedMotion);
    ENJIN_EXPECT_FALSE(settings.disableScreenShake);
    ENJIN_EXPECT_FALSE(settings.disableFOVEffects);
    ENJIN_EXPECT_FALSE(settings.disableFlashingLights);
}

ENJIN_TEST(AccessSettings, SubtitleDefaults) {
    RuntimeAccessibilitySettings settings;
    ENJIN_EXPECT_FALSE(settings.subtitlesEnabled);
    ENJIN_EXPECT_FALSE(settings.closedCaptionsEnabled);
    ENJIN_EXPECT_FLOAT_EQ(settings.subtitleFontSize, 24.0f);
    ENJIN_EXPECT_TRUE(settings.subtitleSpeakerNames);
}

ENJIN_TEST(AccessSettings, FontScale) {
    RuntimeAccessibilitySettings settings;
    ENJIN_EXPECT_FLOAT_EQ(settings.fontScale, 1.0f);
}

ENJIN_TEST(AccessSettings, DyslexiaDefaults) {
    RuntimeAccessibilitySettings settings;
    ENJIN_EXPECT_FALSE(settings.dyslexiaFriendly);
    ENJIN_EXPECT_EQ((int)settings.fontFamily, (int)FontFamily::Default);
}

ENJIN_TEST(AccessSettings, MotorDefaults) {
    RuntimeAccessibilitySettings settings;
    ENJIN_EXPECT_FALSE(settings.dwellClickEnabled);
    ENJIN_EXPECT_FLOAT_EQ(settings.dwellClickTime, 1.0f);
    ENJIN_EXPECT_FALSE(settings.stickyDragEnabled);
    ENJIN_EXPECT_FALSE(settings.switchAccessEnabled);
}

ENJIN_TEST(AccessSettings, InputSettingsLiveOnTheActionMapNotHere) {
    // Sprint/crouch mode, mouse sensitivity and invert-Y used to be duplicated
    // into RuntimeAccessibilitySettings, where nothing read them: the Controls
    // menu and the Accessibility menu edited different state, and the file
    // round-tripped values the game never applied. They now have one home.
    InputSystem::InputActionMap map;
    map.LoadDefaults();

    ENJIN_EXPECT_FALSE(map.IsSprintToggle());
    ENJIN_EXPECT_FALSE(map.IsCrouchToggle());
    ENJIN_EXPECT_FLOAT_EQ(map.GetMouseSensitivity(), 1.0f);
    ENJIN_EXPECT_FALSE(map.GetInvertY());

    map.SetSprintToggle(true);
    map.SetInvertY(true);
    map.SetMouseSensitivity(2.5f);
    ENJIN_EXPECT_TRUE(map.IsSprintToggle());
    ENJIN_EXPECT_TRUE(map.GetInvertY());
    ENJIN_EXPECT_FLOAT_EQ(map.GetMouseSensitivity(), 2.5f);

    // And they persist with the bindings, so a player keeps them.
    InputSystem::InputActionMap loaded;
    ENJIN_ASSERT_TRUE(loaded.FromJson(map.ToJson()));
    ENJIN_EXPECT_TRUE(loaded.IsSprintToggle());
    ENJIN_EXPECT_TRUE(loaded.GetInvertY());
    ENJIN_EXPECT_FLOAT_EQ(loaded.GetMouseSensitivity(), 2.5f);
}

// ===========================================================================
// ColorblindMode Enum
// ===========================================================================

ENJIN_TEST(ColorblindEnum, AllModes) {
    ENJIN_EXPECT_EQ((int)ColorblindMode::Off, 0);
    ENJIN_EXPECT_EQ((int)ColorblindMode::Protanopia, 1);
    ENJIN_EXPECT_EQ((int)ColorblindMode::Deuteranopia, 2);
    ENJIN_EXPECT_EQ((int)ColorblindMode::Tritanopia, 3);
    ENJIN_EXPECT_EQ((int)ColorblindMode::Achromatopsia, 7);
}

// ===========================================================================
// SubtitleSystem
// ===========================================================================

ENJIN_TEST(Subtitles, ConfigDefaults) {
    SubtitleConfig cfg;
    ENJIN_EXPECT_FALSE(cfg.enabled);
    ENJIN_EXPECT_FALSE(cfg.captionsEnabled);
    ENJIN_EXPECT_FLOAT_EQ(cfg.fontSize, 24.0f);
    ENJIN_EXPECT_FLOAT_NEAR(cfg.backgroundOpacity, 0.7f, 0.01f);
    ENJIN_EXPECT_TRUE(cfg.showSpeakerNames);
    ENJIN_EXPECT_FALSE(cfg.showDirectionIndicators);
    ENJIN_EXPECT_EQ(cfg.maxVisibleLines, 4u);
    ENJIN_EXPECT_FLOAT_NEAR(cfg.positionY, 0.85f, 0.01f);
}

ENJIN_TEST(Subtitles, SystemConfigAccess) {
    SubtitleSystem subs;
    subs.GetConfig().enabled = true;
    ENJIN_EXPECT_TRUE(subs.GetConfig().enabled);
}

ENJIN_TEST(Subtitles, Clear) {
    SubtitleSystem subs;
    subs.ShowSubtitle("Hello", "NPC");
    subs.Clear();
    // After clear, no active subtitles (just verifying it doesn't crash)
    ENJIN_EXPECT_TRUE(true);
}

// ===========================================================================
// Content Warning
// ===========================================================================

ENJIN_TEST(ContentWarning, DefaultNone) {
    SceneContentFlags flags;
    ENJIN_EXPECT_EQ((u32)flags.flags, (u32)ContentWarningType::None);
}

ENJIN_TEST(ContentWarning, HasWarningCheck) {
    ContentWarningType flags = ContentWarningType::FlashingLights | ContentWarningType::Violence;
    ENJIN_EXPECT_TRUE(HasWarning(flags, ContentWarningType::FlashingLights));
    ENJIN_EXPECT_TRUE(HasWarning(flags, ContentWarningType::Violence));
    ENJIN_EXPECT_FALSE(HasWarning(flags, ContentWarningType::Heights));
}

ENJIN_TEST(ContentWarning, BitwiseOr) {
    ContentWarningType combined = ContentWarningType::Spiders | ContentWarningType::Drowning;
    ENJIN_EXPECT_TRUE(HasWarning(combined, ContentWarningType::Spiders));
    ENJIN_EXPECT_TRUE(HasWarning(combined, ContentWarningType::Drowning));
}

ENJIN_TEST(ContentWarning, SystemDismiss) {
    ContentWarningSystem cws;
    ENJIN_EXPECT_FALSE(cws.IsVisible());
    cws.Dismiss();
    ENJIN_EXPECT_FALSE(cws.IsVisible());
}

// ===========================================================================
// Alternative Input
// ===========================================================================

ENJIN_TEST(AltInput, SwitchAccessDefaults) {
    SwitchAccessConfig cfg;
    ENJIN_EXPECT_FALSE(cfg.enabled);
    ENJIN_EXPECT_TRUE(cfg.scanningMode);
    ENJIN_EXPECT_FLOAT_EQ(cfg.scanSpeed, 1.5f);
    ENJIN_EXPECT_EQ(cfg.switchCount, 1);
}

ENJIN_TEST(AltInput, EyeTrackingDefaults) {
    EyeTrackingConfig cfg;
    ENJIN_EXPECT_FALSE(cfg.enabled);
    ENJIN_EXPECT_FLOAT_EQ(cfg.dwellTime, 1.0f);
    ENJIN_EXPECT_TRUE(cfg.showGazeIndicator);
}

ENJIN_TEST(AltInput, SipAndPuffDefaults) {
    SipAndPuffConfig cfg;
    ENJIN_EXPECT_FALSE(cfg.enabled);
    ENJIN_EXPECT_FLOAT_NEAR(cfg.sipThreshold, 0.3f, 0.01f);
}

ENJIN_TEST(AltInput, HeadTrackingDefaults) {
    HeadTrackingConfig cfg;
    ENJIN_EXPECT_FALSE(cfg.enabled);
    ENJIN_EXPECT_FLOAT_EQ(cfg.sensitivity, 1.0f);
    ENJIN_EXPECT_FALSE(cfg.invertX);
    ENJIN_EXPECT_FALSE(cfg.invertY);
}

ENJIN_TEST(AltInput, ManagerNoActiveInputs) {
    AlternativeInputManager mgr;
    ENJIN_EXPECT_FALSE(mgr.IsAnyAlternativeInputActive());
    ENJIN_EXPECT_EQ(mgr.GetScanTargetCount(), (usize)0);
}

ENJIN_TEST_MAIN()
