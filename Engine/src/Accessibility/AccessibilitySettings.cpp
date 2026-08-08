#include "Enjin/Accessibility/AccessibilitySettings.h"
#include "Enjin/Renderer/PostProcessing.h"

namespace Enjin {
namespace Accessibility {

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
