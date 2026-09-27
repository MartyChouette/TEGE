#include <algorithm>
#include "Enjin/Accessibility/TextFont.h"
#include "Enjin/Accessibility/AccessibilitySettings.h"
#include "Enjin/Renderer/PostProcessing.h"
#include "Enjin/Accessibility/SubtitleSystem.h"
#include "Enjin/Accessibility/Announcer.h"
#include "Enjin/GUI/UISystem.h"
#include "Enjin/GUI/TextSpacing.h"
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
    j["touchMode"] = touchMode;
    j["touchHand"] = touchHand;
    j["touchButtonSize"] = touchButtonSize;
    j["switchAccessEnabled"] = switchAccessEnabled;
    j["switchScanSpeed"] = switchScanSpeed;
    j["eyeTrackingEnabled"] = eyeTrackingEnabled;
    j["eyeDwellTime"] = eyeDwellTime;
    j["eyeSmoothing"] = eyeSmoothing;
    j["eyeDeadZone"] = eyeDeadZone;
    j["eyeShowGazeIndicator"] = eyeShowGazeIndicator;
    j["audioIndicatorsEnabled"] = audioIndicatorsEnabled;
    j["screenReaderEnabled"] = screenReaderEnabled;
    return j.dump(2);
}

bool RuntimeAccessibilitySettings::FromJson(const std::string& jsonStr) {
    if (jsonStr.empty()) return false;
    try {
        nlohmann::json j = nlohmann::json::parse(jsonStr);
        if (!j.is_object()) return false;
        // A key that is missing keeps its current value, so layers apply in
        // order: the project's defaults, then the player's file (IN-22). On a
        // fresh settings object that is the same as the defaults.
        auto mode = j.value("colorblindMode", static_cast<u32>(colorblindMode));
        colorblindMode = static_cast<ColorblindMode>(mode <= 8u ? mode : 0u);
        colorblindStrength = j.value("colorblindStrength", colorblindStrength);
        screenBrightness = j.value("screenBrightness", screenBrightness);
        screenContrast = j.value("screenContrast", screenContrast);
        reducedMotion = j.value("reducedMotion", reducedMotion);
        disableScreenShake = j.value("disableScreenShake", disableScreenShake);
        disableFOVEffects = j.value("disableFOVEffects", disableFOVEffects);
        disableFlashingLights = j.value("disableFlashingLights", disableFlashingLights);
        subtitlesEnabled = j.value("subtitlesEnabled", subtitlesEnabled);
        closedCaptionsEnabled = j.value("closedCaptionsEnabled", closedCaptionsEnabled);
        subtitleFontSize = j.value("subtitleFontSize", subtitleFontSize);
        subtitleBgOpacity = j.value("subtitleBgOpacity", subtitleBgOpacity);
        subtitleSpeakerNames = j.value("subtitleSpeakerNames", subtitleSpeakerNames);
        subtitleDirectionIndicators = j.value("subtitleDirectionIndicators", subtitleDirectionIndicators);
        fontScale = j.value("fontScale", fontScale);
        if (fontScale < 0.5f) fontScale = 0.5f;
        if (fontScale > 3.0f) fontScale = 3.0f;
        dyslexiaFriendly = j.value("dyslexiaFriendly", dyslexiaFriendly);
        letterSpacing = j.value("letterSpacing", letterSpacing);
        wordSpacing = j.value("wordSpacing", wordSpacing);
        lineSpacing = std::clamp(j.value("lineSpacing", lineSpacing), 1.0f, 3.0f);
        auto fam = j.value("fontFamily", static_cast<u32>(fontFamily));
        fontFamily = static_cast<FontFamily>(fam <= 2u ? fam : 0u);
        dwellClickEnabled = j.value("dwellClickEnabled", dwellClickEnabled);
        dwellClickTime = std::clamp(j.value("dwellClickTime", dwellClickTime), 0.3f, 3.0f);
        stickyDragEnabled = j.value("stickyDragEnabled", stickyDragEnabled);
        touchMode = std::min(j.value("touchMode", touchMode), 2u);
        touchHand = std::min(j.value("touchHand", touchHand), 2u);
        touchButtonSize = std::min(j.value("touchButtonSize", touchButtonSize), 4u);
        switchAccessEnabled = j.value("switchAccessEnabled", switchAccessEnabled);
        switchScanSpeed = std::clamp(j.value("switchScanSpeed", switchScanSpeed), 0.5f, 5.0f);
        eyeTrackingEnabled = j.value("eyeTrackingEnabled", eyeTrackingEnabled);
        eyeDwellTime = j.value("eyeDwellTime", eyeDwellTime);
        eyeSmoothing = j.value("eyeSmoothing", eyeSmoothing);
        eyeDeadZone = j.value("eyeDeadZone", eyeDeadZone);
        eyeShowGazeIndicator = j.value("eyeShowGazeIndicator", eyeShowGazeIndicator);
        audioIndicatorsEnabled = j.value("audioIndicatorsEnabled", audioIndicatorsEnabled);
        screenReaderEnabled = j.value("screenReaderEnabled", screenReaderEnabled);
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
    // The font FACE, not just the size. This is the call every runtime already
    // makes for text settings, so it is where the choice has to be pushed --
    // it used to reach only ImGui's atlas via FontLibrary, which is why the
    // toggle never changed a word the game drew.
    SetDyslexiaFontEnabled(settings.dyslexiaFriendly);
    // The spacing goes with it, for the same reason: FontLibrary was its only
    // reader and that only styles ImGui, which cannot space letters, so the
    // three options did nothing on any platform (WP-10)
    GUI::SetTextLetterSpacing(settings.letterSpacing);
    GUI::SetTextWordSpacing(settings.wordSpacing);
    GUI::SetTextLineSpacing(settings.lineSpacing);

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
