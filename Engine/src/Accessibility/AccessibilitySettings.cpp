#include "Enjin/Accessibility/AccessibilitySettings.h"
#include "Enjin/Renderer/PostProcessing.h"
#include "Enjin/Accessibility/SubtitleSystem.h"
#include "Enjin/Accessibility/Announcer.h"
#include "Enjin/GUI/UISystem.h"

namespace Enjin {
namespace Accessibility {

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
