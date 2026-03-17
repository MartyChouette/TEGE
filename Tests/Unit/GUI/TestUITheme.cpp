#include "EnjinTest.h"
#include "Enjin/GUI/UITheme.h"

using namespace Enjin;
using namespace Enjin::GUI;

// ===========================================================================
// UIThemePreset Enum
// ===========================================================================

ENJIN_TEST(UIThemePresetEnum, Values) {
    ENJIN_EXPECT_EQ((int)UIThemePreset::Dark, 0);
    ENJIN_EXPECT_EQ((int)UIThemePreset::Light, 1);
    ENJIN_EXPECT_EQ((int)UIThemePreset::RetroGreen, 2);
    ENJIN_EXPECT_EQ((int)UIThemePreset::Fantasy, 3);
    ENJIN_EXPECT_EQ((int)UIThemePreset::HighContrastDark, 4);
    ENJIN_EXPECT_EQ((int)UIThemePreset::HighContrastLight, 5);
    ENJIN_EXPECT_EQ((int)UIThemePreset::ColorblindSafe, 6);
    ENJIN_EXPECT_EQ((int)UIThemePreset::Count, 7);
}

// ===========================================================================
// UITheme Defaults (Dark theme)
// ===========================================================================

ENJIN_TEST(UIThemeDefaults, Name) {
    UITheme theme;
    ENJIN_EXPECT_STR_EQ(theme.name, "Dark");
}

ENJIN_TEST(UIThemeDefaults, PrimaryColor) {
    UITheme theme;
    ENJIN_EXPECT_FLOAT_NEAR(theme.primary.x, 0.26f, 0.01f);
    ENJIN_EXPECT_FLOAT_NEAR(theme.primary.y, 0.59f, 0.01f);
    ENJIN_EXPECT_FLOAT_NEAR(theme.primary.z, 0.98f, 0.01f);
}

ENJIN_TEST(UIThemeDefaults, BackgroundColor) {
    UITheme theme;
    ENJIN_EXPECT_FLOAT_NEAR(theme.background.x, 0.08f, 0.01f);
    ENJIN_EXPECT_FLOAT_NEAR(theme.background.y, 0.08f, 0.01f);
    ENJIN_EXPECT_FLOAT_NEAR(theme.background.z, 0.10f, 0.01f);
}

ENJIN_TEST(UIThemeDefaults, ErrorColor) {
    UITheme theme;
    ENJIN_EXPECT_FLOAT_NEAR(theme.error.x, 0.90f, 0.01f);
    ENJIN_EXPECT_FLOAT_NEAR(theme.error.y, 0.25f, 0.01f);
    ENJIN_EXPECT_FLOAT_NEAR(theme.error.z, 0.25f, 0.01f);
}

ENJIN_TEST(UIThemeDefaults, TextColors) {
    UITheme theme;
    ENJIN_EXPECT_FLOAT_NEAR(theme.textPrimary.x, 0.95f, 0.01f);
    ENJIN_EXPECT_FLOAT_NEAR(theme.textDisabled.x, 0.35f, 0.01f);
}

// ===========================================================================
// UITheme Typography & Spacing
// ===========================================================================

ENJIN_TEST(UIThemeTypography, FontSizes) {
    UITheme theme;
    ENJIN_EXPECT_FLOAT_EQ(theme.fontSizeBody, 16.0f);
    ENJIN_EXPECT_FLOAT_EQ(theme.fontSizeHeading, 24.0f);
    ENJIN_EXPECT_FLOAT_EQ(theme.fontSizeSmall, 12.0f);
}

ENJIN_TEST(UIThemeTypography, Spacing) {
    UITheme theme;
    ENJIN_EXPECT_FLOAT_EQ(theme.spacing, 8.0f);
}

ENJIN_TEST(UIThemeTypography, BorderDefaults) {
    UITheme theme;
    ENJIN_EXPECT_FLOAT_EQ(theme.borderRadius, 4.0f);
    ENJIN_EXPECT_FLOAT_EQ(theme.borderWidth, 1.0f);
}

ENJIN_TEST(UIThemeTypography, BgAlpha) {
    UITheme theme;
    ENJIN_EXPECT_FLOAT_NEAR(theme.bgAlpha, 0.92f, 0.01f);
}

ENJIN_TEST(UIThemeTypography, FocusBorderWidth) {
    UITheme theme;
    ENJIN_EXPECT_FLOAT_NEAR(theme.focusBorderWidth, 2.5f, 0.01f);
}

// ===========================================================================
// UITheme Accessibility Features
// ===========================================================================

ENJIN_TEST(UIThemeAccessibility, DyslexicFontOff) {
    UITheme theme;
    ENJIN_EXPECT_FALSE(theme.useDyslexicFont);
}

ENJIN_TEST(UIThemeAccessibility, SpacingDefaults) {
    UITheme theme;
    ENJIN_EXPECT_FLOAT_EQ(theme.letterSpacing, 0.0f);
    ENJIN_EXPECT_FLOAT_EQ(theme.wordSpacing, 0.0f);
    ENJIN_EXPECT_FLOAT_EQ(theme.lineSpacingMultiplier, 1.0f);
}

// ===========================================================================
// UITheme Factory Presets
// ===========================================================================

ENJIN_TEST(UIThemePresets, DarkPreset) {
    UITheme dark = UITheme::Dark();
    ENJIN_EXPECT_STR_EQ(dark.name, "Dark");
}

ENJIN_TEST(UIThemePresets, LightPreset) {
    UITheme light = UITheme::Light();
    ENJIN_EXPECT_STR_EQ(light.name, "Light");
}

ENJIN_TEST(UIThemePresets, RetroGreenPreset) {
    UITheme retro = UITheme::RetroGreen();
    ENJIN_EXPECT_STR_EQ(retro.name, "RetroGreen");
}

ENJIN_TEST(UIThemePresets, FantasyPreset) {
    UITheme fantasy = UITheme::Fantasy();
    ENJIN_EXPECT_STR_EQ(fantasy.name, "Fantasy");
}

ENJIN_TEST(UIThemePresets, HighContrastDark) {
    UITheme hc = UITheme::HighContrastDark();
    ENJIN_EXPECT_STR_EQ(hc.name, "HighContrastDark");
}

ENJIN_TEST(UIThemePresets, ColorblindSafe) {
    UITheme cb = UITheme::ColorblindSafe();
    ENJIN_EXPECT_STR_EQ(cb.name, "ColorblindSafe");
}

ENJIN_TEST(UIThemePresets, FromPresetDark) {
    UITheme theme = UITheme::FromPreset(UIThemePreset::Dark);
    ENJIN_EXPECT_STR_EQ(theme.name, "Dark");
}

ENJIN_TEST_MAIN()
