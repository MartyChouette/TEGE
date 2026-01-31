#include "Enjin/Accessibility/AccessibilitySettings.h"
#include "Enjin/Renderer/PostProcessing.h"

namespace Enjin {
namespace Accessibility {

void RuntimeAccessibilitySettings::ApplyToPostProcessing(Renderer::PostProcessSettings& ppSettings) const {
    ppSettings.colorblindMode = static_cast<u32>(colorblindMode);
    ppSettings.colorblindStrength = colorblindStrength;
    ppSettings.brightness += screenBrightness;
    ppSettings.contrast *= screenContrast;

    // Disable flashy effects if photosensitive
    if (disableFlashingLights) {
        ppSettings.filmGrainEnabled = 0;
        ppSettings.crtEnabled = 0;
        ppSettings.vhsEnabled = 0;
    }
}

} // namespace Accessibility
} // namespace Enjin
