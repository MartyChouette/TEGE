#include "Enjin/Accessibility/AccessibilitySettings.h"
#include "Enjin/Renderer/PostProcessing.h"
#include "Enjin/Accessibility/SubtitleSystem.h"
#include "Enjin/Accessibility/Announcer.h"
#include "Enjin/GUI/UISystem.h"
#include "Enjin/Logging/Log.h"
#include <nlohmann/json.hpp>

namespace Enjin {
namespace Accessibility {

std::string RuntimeAccessibilitySettings::ToJson() const {
    nlohmann::json j;
    j["colorblindMode"] = static_cast<u32>(colorblindMode);
    j["colorblindStrength"] = colorblindStrength;
    j["screenBrightness"] = screenBrightness;
    j["screenContrast"] = screenContrast;
    j["reducedMotion"] = reducedMotion;
    j["disableScreenShake"] = disableScreenShake;
    j["disableFOVEffects"] = disableFOVEffects;
    j["disableFlashingLights"] = disableFlashingLights;
    j["subtitlesEnabled"] = subtitlesEnabled;
    j["closedCaptionsEnabled"] = closedCaptionsEnabled;
    j["subtitleFontSize"] = subtitleFontSize;
    j["subtitleBgOpacity"] = subtitleBgOpacity;
    j["subtitleSpeakerNames"] = subtitleSpeakerNames;
    j["subtitleDirectionIndicators"] = subtitleDirectionIndicators;
    j["fontScale"] = fontScale;
    j["dyslexiaFriendly"] = dyslexiaFriendly;
    j["letterSpacing"] = letterSpacing;
    j["wordSpacing"] = wordSpacing;
    j["lineSpacing"] = lineSpacing;
    j["fontFamily"] = static_cast<u32>(fontFamily);
    j["dwellClickEnabled"] = dwellClickEnabled;
    j["dwellClickTime"] = dwellClickTime;
    j["stickyDragEnabled"] = stickyDragEnabled;
    j["switchAccessEnabled"] = switchAccessEnabled;
    j["switchScanSpeed"] = switchScanSpeed;
    j["audioIndicatorsEnabled"] = audioIndicatorsEnabled;
    j["screenReaderEnabled"] = screenReaderEnabled;
    return j.dump(2);
}

bool RuntimeAccessibilitySettings::FromJson(const std::string& jsonStr) {
    if (jsonStr.empty()) return false;
    try {
        nlohmann::json j = nlohmann::json::parse(jsonStr);
        if (!j.is_object()) return false;
        auto mode = j.value("colorblindMode", 0u);
        colorblindMode = static_cast<ColorblindMode>(mode <= 8u ? mode : 0u);
        colorblindStrength = j.value("colorblindStrength", 1.0f);
        screenBrightness = j.value("screenBrightness", 0.0f);
        screenContrast = j.value("screenContrast", 1.0f);
        reducedMotion = j.value("reducedMotion", false);
        disableScreenShake = j.value("disableScreenShake", false);
        disableFOVEffects = j.value("disableFOVEffects", false);
        disableFlashingLights = j.value("disableFlashingLights", false);
        subtitlesEnabled = j.value("subtitlesEnabled", false);
        closedCaptionsEnabled = j.value("closedCaptionsEnabled", false);
        subtitleFontSize = j.value("subtitleFontSize", 24.0f);
        subtitleBgOpacity = j.value("subtitleBgOpacity", 0.7f);
        subtitleSpeakerNames = j.value("subtitleSpeakerNames", true);
        subtitleDirectionIndicators = j.value("subtitleDirectionIndicators", false);
        fontScale = j.value("fontScale", 1.0f);
        if (fontScale < 0.5f) fontScale = 0.5f;
        if (fontScale > 3.0f) fontScale = 3.0f;
        dyslexiaFriendly = j.value("dyslexiaFriendly", false);
        letterSpacing = j.value("letterSpacing", 0.0f);
        wordSpacing = j.value("wordSpacing", 0.0f);
        lineSpacing = j.value("lineSpacing", 1.0f);
        auto fam = j.value("fontFamily", 0u);
        fontFamily = static_cast<FontFamily>(fam <= 2u ? fam : 0u);
        dwellClickEnabled = j.value("dwellClickEnabled", false);
        dwellClickTime = j.value("dwellClickTime", 1.0f);
        stickyDragEnabled = j.value("stickyDragEnabled", false);
        switchAccessEnabled = j.value("switchAccessEnabled", false);
        switchScanSpeed = j.value("switchScanSpeed", 1.5f);
        audioIndicatorsEnabled = j.value("audioIndicatorsEnabled", false);
        screenReaderEnabled = j.value("screenReaderEnabled", false);
        return true;
    } catch (const std::exception& e) {
        ENJIN_LOG_ERROR(Core, "Failed to parse accessibility settings: %s", e.what());
        return false;
    } catch (...) {
        return false;
    }
}

void ApplyTextScale(const RuntimeAccessibilitySettings& settings,
                    GUI::UISystem* ui,
                    SubtitleSystem* subtitles,
                    AccessibilityAnnouncer* announcer) {
    if (ui) ui->SetFontScale(settings.fontScale);
    if (subtitles) {
        // The subtitle size setting stays the player's own; the global scale
        // multiplies it at draw time.
        subtitles->GetConfig().fontSize = settings.subtitleFontSize;
        subtitles->GetConfig().fontScale = settings.fontScale;
    }
    if (announcer) announcer->fontScale = settings.fontScale;
}

void RuntimeAccessibilitySettings::ApplyToPostProcessing(Renderer::PostProcessSettings& ppSettings) const {
    ppSettings.colorblindMode = static_cast<u32>(colorblindMode);
    ppSettings.colorblindStrength = colorblindStrength;
    // This runs EVERY FRAME in play mode and the player — it must be
    // idempotent. The old += / *= compounded each frame the moment a user set
    // a non-neutral value (screen blowout within seconds). Neutral values
    // leave the scene's own grading untouched; non-neutral user settings
    // override it absolutely.
    if (screenBrightness != 0.0f) ppSettings.brightness = screenBrightness;
    if (screenContrast != 1.0f)  ppSettings.contrast = screenContrast;

    // Disable flashy effects if photosensitive
    if (disableFlashingLights) {
        ppSettings.filmGrainEnabled = 0;
        ppSettings.crtEnabled = 0;
        ppSettings.vhsEnabled = 0;
    }
}

} // namespace Accessibility
} // namespace Enjin
